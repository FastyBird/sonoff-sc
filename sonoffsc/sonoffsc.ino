#include <DHT.h>
#include <MsTimer2.h>
#include <TimerOne.h>
#include <Ticker.h>

#define SERIAL_BAUDRATE         19200

#define FAN_PIN                 7         // LED header NC
#define FAN_OFF_DELAY           0

#define LDR_PIN                 A3
#define REF_RESISTANCE          10000     // Measure this for best results. It is R26
#define LUX_CALC_SCALAR         12518931
#define LUX_CALC_EXPONENT       -1.405
#define MAX_ADC_READING         1023
#define ADC_REF_VOLTAGE         5.0

#define SHARP_LED_PIN           9
#define SHARP_READ_PIN          A1
#define SHARP_SAMPLING_TIME     280
#define SHARP_DELTA_TIME        40

#define DHT_PIN                 6
#define DHT_TYPE                DHT11
//#define DHT_TYPE                DHT22
#define DHT_EXPIRES             60000

#define MICROPHONE_PIN          A2

#define MW_PIN                  13        // ISP header SCK

#define INTERUPT_PIN            10

#define NOISE_NUMREADINGS       10
#define ADC_NUMREADINGS         50
#define DHT_NUMREADINGS         5
#define VOLTAGE                 (5.0)

#define DUST_LEVEL_INDEX        0
#define DUST_VALUE_INDEX        1
#define LIGHT_LEVEL_INDEX       2
#define LIGHT_VALUE_INDEX       3
#define NOISE_INDEX             4

#define NULL_VALUE              -999

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

typedef struct _sensorDev
{
    uint32_t total;
    uint16_t average_value;
    int8_t value;
    int8_t last_value;
} sensorDev;

sensorDev _sensors_data[5];

bool _read_adc_flag = false;
bool _update_values_flag = false;
bool _network_status  = false;

uint16_t _upload_freq = 1800;
uint8_t _humi_threshold = 2;
uint8_t _temp_threshold = 1;

float _temperature = NULL_VALUE;
int _humidity = NULL_VALUE;
float _dust = NULL_VALUE;
int _illuminance = NULL_VALUE;
bool _movement = false;

DHT dht(DHT_PIN, DHT_TYPE);

Ticker fanTicker;

// -----------------------------------------------------------------------------
// DEVICE API
// -----------------------------------------------------------------------------

void initPins()
{
    // Setup physical pins on the ATMega328
    pinMode(LDR_PIN, INPUT);
    pinMode(DHT_PIN, INPUT);
    pinMode(SHARP_LED_PIN, OUTPUT);
    pinMode(SHARP_READ_PIN, INPUT);
    pinMode(MICROPHONE_PIN, INPUT_PULLUP);

    // Extra pins for modified device
    pinMode(FAN_PIN, OUTPUT);
    pinMode(MW_PIN, INPUT);

    // ???
    pinMode(INTERUPT_PIN, OUTPUT);
}

// -----------------------------------------------------------------------------

void initPwm()
{
    Timer1.initialize(10000);
    Timer1.pwm(10, 999);
    Timer1.pwm(9, 991);
}

// -----------------------------------------------------------------------------

void initData()
{
    _sensors_data[DUST_LEVEL_INDEX].total = 0;
    _sensors_data[DUST_LEVEL_INDEX].value = 0;

    _sensors_data[DUST_VALUE_INDEX].total = 0;
    _sensors_data[DUST_VALUE_INDEX].value = 0;

    _sensors_data[LIGHT_LEVEL_INDEX].total = 0;
    _sensors_data[LIGHT_LEVEL_INDEX].value = 0;

    _sensors_data[LIGHT_VALUE_INDEX].total = 0;
    _sensors_data[LIGHT_VALUE_INDEX].value = 0;

    _sensors_data[NOISE_INDEX].total = 0;
    _sensors_data[NOISE_INDEX].value = 0;
}

// -----------------------------------------------------------------------------

void handleInterupt()
{
    _read_adc_flag = true;
}

// -----------------------------------------------------------------------------

void handleTimer()
{
    static uint8_t couter = 0;

    (couter >= 200) ? (couter = 1) : (couter++);

    _update_values_flag = true;
}

// -----------------------------------------------------------------------------

void updateDeviceConfig(String *rec_string)
{
    String string_uploadfreq = "";
    String string_humithreshold = "";
    String string_tempthreshold = "";

    string_uploadfreq = _parseString(rec_string, "\"uploadFreq\":", ",");

    if (!string_uploadfreq.equals("")) {
        _upload_freq = string_uploadfreq.toInt();
    }

    string_humithreshold = _parseString(rec_string, "\"humiThreshold\":", ",");

    if (!string_humithreshold.equals("")) {
        _humi_threshold = string_humithreshold.toInt();
    }

    string_tempthreshold = _parseString(rec_string, "\"tempThreshold\":", ",");

    if (!string_tempthreshold.equals("")) {
        _temp_threshold = string_tempthreshold.toInt();
    }
}

// -----------------------------------------------------------------------------
// SERIAL COMMUNICATION
// -----------------------------------------------------------------------------

void readUart()
{
    static bool get_at_flag = false;
    static String rec_string = "";
    int16_t index1;

    while (Serial.available() > 0)
    {
        char a = (char) Serial.read();
        rec_string += a;

        // Search for end char
        if (a == 0x1B) {
             break;
        }
    }

    if (rec_string.indexOf(0x1B) !=-1) {
        if ((index1 = rec_string.indexOf("AT+DEVCONFIG=")) != -1) {
            _network_status = true;

            updateDeviceConfig(&rec_string);

        } else if ((index1 = rec_string.indexOf("AT+NOTIFY=")) != -1) {
            updateDeviceConfig(&rec_string);

            Serial.write("AT+NOTIFY=ok");
            Serial.write(0x1b);
            Serial.write("\n");

        } else if ((index1 = rec_string.indexOf("AT+SEND=fail")) != -1) {
            _network_status = false;

        } else if ((index1 = rec_string.indexOf("AT+STATUS=4")) != -1) {
            _network_status = true;

        } else if ((index1 = rec_string.indexOf("AT+STATUS")) != -1) {
              _network_status = false;

        } else if ((index1 = rec_string.indexOf("AT+START")) != -1) {
              _network_status = true;

        } else {
            // Do nothing
        }

        rec_string = "";

    } else if (rec_string.indexOf("AT") != -1) {
        Serial.flush();

    } else {
        // Do nothing
    }
}

// -----------------------------------------------------------------------------

void transmithData()
{
    static uint16_t couter = 0;
    static uint8_t diff_couter = 0;

    couter++;

    if (_network_status && couter == _upload_freq) {
        _syncData();
        _sendData();

        couter = 0;

    } else if (
      _network_status && (
        getTemperature() != NULL_VALUE ||
        getHumidity() != NULL_VALUE ||
        _sensors_data[DUST_LEVEL_INDEX].value != _sensors_data[DUST_LEVEL_INDEX].last_value ||
        _sensors_data[DUST_VALUE_INDEX].value != _sensors_data[DUST_VALUE_INDEX].last_value ||
        _sensors_data[LIGHT_LEVEL_INDEX].value != _sensors_data[LIGHT_LEVEL_INDEX].last_value ||
        _sensors_data[LIGHT_VALUE_INDEX].value != _sensors_data[LIGHT_VALUE_INDEX].last_value ||
        _sensors_data[NOISE_INDEX].value != _sensors_data[NOISE_INDEX].last_value
      )
    ) {
          diff_couter ++;

          if (diff_couter >= 3) {
              _syncData();
              _sendData();

              couter = 0;
              diff_couter = 0;
          }

    } else if (couter % 10 == 0) {
        checkNetwork();

    } else {
        diff_couter = 0;
    }
}

// -----------------------------------------------------------------------------

void checkNetwork()
{
    Serial.write("AT+STATUS?");
    Serial.write(0x1b);
    Serial.write("\n");
}

// -----------------------------------------------------------------------------
// SENSORS
// -----------------------------------------------------------------------------

void temperatureHumidityLoop() {
    loadTempAndHum();
}

// -----------------------------------------------------------------------------

void loadTempAndHum() {
    // Check at most once every minute
    static unsigned long _last_dht_reading = 0;

    if (millis() - _last_dht_reading < DHT_EXPIRES && _temperature != NULL_VALUE && _temperature != _humidity) {
        return;
    }

    // Retrieve data
    double h = dht.readHumidity();
    double t = dht.readTemperature();

    // Check values
    if (isnan(h) || isnan(t)) {
        Serial.println("it is error");

        return;
    }

    _temperature = t;
    _humidity = h;

    // Only set new expiration time if good reading
    _last_dht_reading = millis();
}

// -----------------------------------------------------------------------------

float getTemperature() {
    loadTempAndHum();

    return _temperature;
}

// -----------------------------------------------------------------------------

int getHumidity() {
    loadTempAndHum();

    return _humidity;
}

// -----------------------------------------------------------------------------

void sensorsLoop()
{
    static uint8_t _adc_read_index = 0;
    static int16_t _adc_readings[3][ADC_NUMREADINGS] = {0};

    static uint8_t _noise_index = 0;
    static int16_t _noise_readings[NOISE_NUMREADINGS] = {0};
    static int16_t _noise_max = -1;
    static int16_t _noise_min = 1025;

    // DUST & LIGHT SENSOR

    for (uint8_t i = 0; i <= 3; i++) {
        _sensors_data[i].total -= _adc_readings[i][_adc_read_index];

        if (i == DUST_LEVEL_INDEX) {
            _adc_readings[i][_adc_read_index] = getDust();

        } else if (i == DUST_VALUE_INDEX) {
            _adc_readings[i][_adc_read_index] = getDust(true);

        } else if (i == LIGHT_LEVEL_INDEX) {
            _adc_readings[i][_adc_read_index] = getLight();

        } else if (i == LIGHT_VALUE_INDEX) {

        }

        _sensors_data[i].total += _adc_readings[i][_adc_read_index];
        _sensors_data[i].average_value = _sensors_data[i].total / ADC_NUMREADINGS;
        // _sensors_data[i].voltage_value = _sensors_data[i].average_value * (VOLTAGE / 1023.0);
    }

    _sensors_data[LIGHT_LEVEL_INDEX].value = _sensors_data[LIGHT_LEVEL_INDEX].average_value > 999 ? 10 : (_sensors_data[LIGHT_LEVEL_INDEX].average_value / 100 + 1);
    _sensors_data[DUST_LEVEL_INDEX].value = _sensors_data[DUST_LEVEL_INDEX].average_value > 799 ? 10 : (_sensors_data[DUST_LEVEL_INDEX].average_value / 80 + 1);

    _adc_read_index = _adc_read_index + 1;

    // NOISE SENSOR

    int16_t nosie_value_temp = 0;
            nosie_value_temp = analogRead(MICROPHONE_PIN);

    _noise_max = max(nosie_value_temp, _noise_max);
    _noise_min = min(nosie_value_temp, _noise_min);

    if (_adc_read_index >= ADC_NUMREADINGS) {
        _sensors_data[NOISE_INDEX].total -= _noise_readings[_noise_index];

        _noise_readings[_noise_index] = (_noise_max - _noise_min);

        _sensors_data[NOISE_INDEX].total += _noise_readings[_noise_index];
        _sensors_data[NOISE_INDEX].average_value =  _sensors_data[NOISE_INDEX].total / NOISE_NUMREADINGS;
        _sensors_data[NOISE_INDEX].value = _sensors_data[NOISE_INDEX].average_value > 999 ? 10 : (_sensors_data[NOISE_INDEX].average_value / 100 + 1);
        // _sensors_data[NOISE_INDEX].voltage_value = _sensors_data[NOISE_INDEX].average_value * (VOLTAGE / 1023.0);

        _noise_index = _noise_index + 1;

        _noise_max = -1;
        _noise_min = 1025;

        _adc_read_index = 0;
    }

    if (_noise_index >= NOISE_NUMREADINGS) {
        _noise_index = 0;
    }
}

// -----------------------------------------------------------------------------

void moveLoop(bool force = false) {
    bool value = getMovement();

    if ((force || value != _movement) && _network_status) {
        Serial.write("AT+UPDATE=\"movement\":");
        Serial.print(String(value ? 1 : 0));

        Serial.write(0x1B);
        Serial.write("\n");
    }

    _movement = value;
}

// -----------------------------------------------------------------------------

float getDust(bool converted) {
    digitalWrite(SHARP_LED_PIN, LOW);
    delayMicroseconds(SHARP_SAMPLING_TIME);

    float reading = analogRead(SHARP_READ_PIN);

    delayMicroseconds(SHARP_DELTA_TIME);
    digitalWrite(SHARP_LED_PIN, HIGH);

    // 0.5V ==> 100ug/m3
    if (converted) {
        // mg/m3
        float dust = 170.0 * reading * (5.0 / 1024.0) - 100.0;

        if (dust < 0) {
            dust = 0;
        }

        return dust;
    }

    return reading;
}

// -----------------------------------------------------------------------------

void getDustDefer(uint8_t read_index) {
    if (fanoff > 0) {
        fanStatus(true);

        fanTicker.setInterval(fanoff);
        fanTicker.setCallback([](){
            fanTicker.stop();

            dust = getDust();
            storeDustValue(dust, DUST_LEVEL_INDEX, read_index);

            dust = getDust(true);
            storeDustValue(dust, DUST_VALUE_INDEX, read_index);

            fanStatus(false);
        });

        fanTicker.start();

    } else {
        dust = getDust();
        storeDustValue(dust, DUST_LEVEL_INDEX, read_index);

        dust = getDust(true);
        storeDustValue(dust, DUST_VALUE_INDEX, read_index);
    }
}

// -----------------------------------------------------------------------------

void storeDustValue(float value, uint8_t index, uint8_t read_index) {
    _sensors_data[index].total -= _adc_readings[index][read_index];

    _adc_readings[index][read_index] = value;

    _sensors_data[index].total += _adc_readings[index][read_index];
    _sensors_data[index].average_value = _sensors_data[index].total / ADC_NUMREADINGS;

    if (index == DUST_LEVEL_INDEX) {
        _sensors_data[DUST_LEVEL_INDEX].value = _sensors_data[DUST_LEVEL_INDEX].average_value > 799 ? 10 : (_sensors_data[DUST_LEVEL_INDEX].average_value / 80 + 1);
    }
}

// -----------------------------------------------------------------------------

float getDust() {
    return getDust(false);
}

// -----------------------------------------------------------------------------

int getLight() {
    return analogRead(LDR_PIN);
}

// -----------------------------------------------------------------------------

bool getMovement() {
    return digitalRead(MW_PIN) == HIGH;
}

// -----------------------------------------------------------------------------

float getIlluminance() {
    int ldr_raw_data;
    float resistor_voltage, ldr_voltage;
    float ldr_resistance;
    float ldr_lux;

    // Perform the analog to digital conversion
    ldr_raw_data = getLight();

    // RESISTOR VOLTAGE_CONVERSION
    // Convert the raw digital data back to the voltage that was measured on the analog pin
    resistor_voltage = (float) ldr_raw_data / MAX_ADC_READING * ADC_REF_VOLTAGE;

    // Voltage across the LDR is the 5V supply minus the ref resistor voltage
    ldr_voltage = ADC_REF_VOLTAGE - resistor_voltage;

    // LDR_RESISTANCE_CONVERSION
    // resistance that the LDR would have for that voltage
    ldr_resistance = ldr_voltage / resistor_voltage * REF_RESISTANCE;

    // LDR_LUX
    // Change the code below to the proper conversion from ldr_resistance to
    // ldr_lux
    ldr_lux = LUX_CALC_SCALAR * pow(ldr_resistance, LUX_CALC_EXPONENT);

    return ldr_lux;
}

// -----------------------------------------------------------------------------
// FAN
// -----------------------------------------------------------------------------

void fanStatus(bool status) {
    digitalWrite(FAN_PIN, status ? HIGH : LOW);
}

// -----------------------------------------------------------------------------

bool fanStatus() {
    return digitalRead(FAN_PIN) == HIGH;
}

// -----------------------------------------------------------------------------
// INTERNAL API
// -----------------------------------------------------------------------------

String _parseString(String *src, const char *start, const char *end)
{
    int16_t startInder;
    String dst = "";

    if (src == NULL || start == NULL) {
        return dst;
    }

    if ((startInder = src->indexOf(start)) != -1) {
        int16_t endIndex = startInder + strlen(start);

        if (src->indexOf(end, endIndex) != -1) {
            dst = src->substring(endIndex);

        } else {
            dst = src->substring(endIndex, src->indexOf(end, endIndex));
        }
    }

    return dst;
}

// -----------------------------------------------------------------------------

void _syncData()
{
    uint8_t i = 0;

    for(i = 0; i < 3; i++) {
        _sensors_data[i].last_value = _sensors_data[i].value;
    }
}

// -----------------------------------------------------------------------------

void _sendData()
{
    Serial.write("AT+UPDATE=\"humidity\":");
    Serial.print(String(getHumidity()));

    Serial.write(",\"temperature\":");
    Serial.print(String(getTemperature()));

    Serial.write(",\"light\":");
    Serial.print(String(_sensors_data[LIGHT_LEVEL_INDEX].value));

    Serial.write(",\"illuminance\":");
    Serial.print(String(_sensors_data[LIGHT_VALUE_INDEX].value));

    Serial.write(",\"noise\":");
    Serial.print(String(_sensors_data[NOISE_INDEX].value));

    Serial.write(",\"dusty\":");
    Serial.print(String(_sensors_data[DUST_LEVEL_INDEX].value));

    Serial.write(",\"dust\":");
    Serial.print(String(_sensors_data[DUST_VALUE_INDEX].value));

    Serial.write(",\"movement\":");
    Serial.print(String(getMovement() ? 1 : 0));

    Serial.write(0x1B);
    Serial.write("\n");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------

void setup() {
    // Setup Serial port
    Serial.begin(SERIAL_BAUDRATE);

    initPins();
    initPwm();
    initData();

    // Switch FAN off
    fanStatus(false);

    // Setup the DHT Thermometer/Humidity Sensor
    dht.begin();

    attachInterrupt(0, handleInterupt, RISING);

    MsTimer2::set(1000, handleTimer); // 500ms period
    MsTimer2::start();
}

// -----------------------------------------------------------------------------

void loop() {
    readUart();

    temperatureHumidityLoop();

    if (_read_adc_flag) {
        _read_adc_flag = false;

        sensorsLoop();
    }

    if (_update_values_flag) {
        _update_values_flag = false;

        transmithData();
    }

    fanTicker.update();

    moveLoop();
}
