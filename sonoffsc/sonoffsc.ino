#include <DHT.h>
#include <MsTimer2.h>
#include <TimerOne.h>

#define SERIAL_BAUDRATE         19200

#define LDR_PIN                 A3
#define LDR_REF_RESISTANCE      9910     // Measure this for best results. It is R26
#define LUX_CALC_SCALAR         12500000
#define LUX_CALC_EXPONENT       -1.405

#define SHARP_LED_PIN           9
#define SHARP_READ_PIN          A1

#define DHT_PIN                 6
#define DHT_TYPE                DHT11
//#define DHT_TYPE                DHT22
#define DHT_EXPIRES             60000

#define MICROPHONE_PIN          A2

#define MW_PIN                  13        // ISP header SCK

#define INTERUPT_PIN            10

#define CLAP_ENABLED            0
#define CLAP_DEBOUNCE_DELAY     150
#define CLAP_TIMEOUT_DELAY      1000
#define CLAP_SENSIBILITY        80
#define CLAP_COUNT_TRIGGER      4
#define CLAP_BUFFER_SIZE        7
#define CLAP_TOLERANCE          1.50

#define DHT_NUMREADINGS         5
#define NOISE_NUMREADINGS       10
#define ADC_NUMREADINGS         50
#define LIGHT_NUMREADINGS       50
#define DUST_NUMREADINGS        50

#define ADC_REF_VOLTAGE         5.0
#define MAX_ADC_READING         1023

#define DUST_LEVEL_INDEX        0
#define DUST_VALUE_INDEX        1
#define LIGHT_LEVEL_INDEX       2
#define ILLUMINANCE_INDEX       3
#define NOISE_INDEX             4
#define TEMPERATURE_INDEX       5
#define HUMIDITY_INDEX          6

#define INDEX_SIZE              7

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

sensorDev _sensors_data[INDEX_SIZE];

bool _read_temp_humi_flag = false;
bool _read_adc_flag = false;
bool _update_values_flag = false;
bool _comm_status  = false;
bool _is_first_update = false;

uint16_t _upload_freq = 1800;
unsigned long _humi_threshold = 2;
unsigned long _temp_threshold = 1;

bool _movement = false;

bool _clap_enabled = CLAP_ENABLED;
int _clap_timings[CLAP_BUFFER_SIZE];
byte _clap_pointer = 0;

DHT dht(DHT_PIN, DHT_TYPE);

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
    pinMode(MW_PIN, INPUT);

    // Interupt trigger
    pinMode(INTERUPT_PIN, OUTPUT);
}

// -----------------------------------------------------------------------------

void initPwm()
{
    Timer1.initialize(10000);
    Timer1.pwm(INTERUPT_PIN, 999);
    Timer1.pwm(SHARP_LED_PIN, 991);
}

// -----------------------------------------------------------------------------

void initData()
{
    _sensors_data[DUST_LEVEL_INDEX].total = 0;
    _sensors_data[DUST_LEVEL_INDEX].value = 0;

    _sensors_data[DUST_VALUE_INDEX].total = 0;
    _sensors_data[DUST_VALUE_INDEX].average_value = 0;

    _sensors_data[LIGHT_LEVEL_INDEX].total = 0;
    _sensors_data[LIGHT_LEVEL_INDEX].value = 0;

    _sensors_data[ILLUMINANCE_INDEX].total = 0;
    _sensors_data[ILLUMINANCE_INDEX].average_value = 0;

    _sensors_data[NOISE_INDEX].total = 0;
    _sensors_data[NOISE_INDEX].value = 0;

    _sensors_data[TEMPERATURE_INDEX].total = 0;
    _sensors_data[TEMPERATURE_INDEX].average_value = 0;

    _sensors_data[HUMIDITY_INDEX].total = 0;
    _sensors_data[HUMIDITY_INDEX].average_value = 0;
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

    if (couter % 2 == 0) {
        _read_temp_humi_flag = true;
    }

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
        _humi_threshold = string_humithreshold.toFloat();
    }

    string_tempthreshold = _parseString(rec_string, "\"tempThreshold\":", ",");

    if (!string_tempthreshold.equals("")) {
        _temp_threshold = string_tempthreshold.toFloat();
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
            _comm_status = true;

            updateDeviceConfig(&rec_string);

        } else if ((index1 = rec_string.indexOf("AT+NOTIFY=")) != -1) {
            updateDeviceConfig(&rec_string);

            Serial.write("AT+NOTIFY=ok");
            Serial.write(0x1b);
            Serial.write("\n");

        } else if ((index1 = rec_string.indexOf("AT+SEND=fail")) != -1) {
            _comm_status = false;

        } else if ((index1 = rec_string.indexOf("AT+STATUS=4")) != -1) {
            _comm_status = true;

            if (_is_first_update) {
                _is_first_update = false;

                _sensors_data[TEMPERATURE_INDEX].last_value = 0;
                _sensors_data[HUMIDITY_INDEX].last_value = 0;
            }

        } else if ((index1 = rec_string.indexOf("AT+STATUS")) != -1) {
              _comm_status = false;
              _is_first_update = true;

        } else if ((index1 = rec_string.indexOf("AT+START")) != -1) {
              _comm_status = true;

              _sensors_data[TEMPERATURE_INDEX].last_value = 0;
              _sensors_data[HUMIDITY_INDEX].last_value = 0;

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

    if (_comm_status && couter == _upload_freq) {
        _syncData();
        _sendData();

        couter = 0;

    } else if (
      _comm_status && (
        abs(_sensors_data[TEMPERATURE_INDEX].average_value - _sensors_data[TEMPERATURE_INDEX].last_value) >= _temp_threshold ||
        abs(_sensors_data[HUMIDITY_INDEX].average_value - _sensors_data[HUMIDITY_INDEX].last_value) >= _humi_threshold ||
        _sensors_data[DUST_LEVEL_INDEX].value != _sensors_data[DUST_LEVEL_INDEX].last_value ||
        _sensors_data[DUST_VALUE_INDEX].average_value != _sensors_data[DUST_VALUE_INDEX].last_value ||
        _sensors_data[LIGHT_LEVEL_INDEX].value != _sensors_data[LIGHT_LEVEL_INDEX].last_value ||
        _sensors_data[ILLUMINANCE_INDEX].average_value != _sensors_data[ILLUMINANCE_INDEX].last_value ||
        _sensors_data[NOISE_INDEX].value != _sensors_data[NOISE_INDEX].last_value
      )
    ) {
          diff_couter++;

          if (diff_couter >= 3) {
              _syncData();
              _sendData();

              couter = 0;
              diff_couter = 0;
          }

    } else if (couter % 10 == 0) {
        checkMaster();

    } else {
        diff_couter = 0;
    }
}

// -----------------------------------------------------------------------------

void checkMaster()
{
    Serial.write("AT+STATUS?");
    Serial.write(0x1b);
    Serial.write("\n");
}

// -----------------------------------------------------------------------------
// SENSORS LOOPS
// -----------------------------------------------------------------------------

void temperatureHumidityLoop() {
    // Retrieve data
    double h = dht.readHumidity();
    double t = dht.readTemperature();

    // Check values
    if (isnan(h) || isnan(t)) {
        Serial.println("it is error");

        return;
    }

    static uint8_t _dht_read_index = 0;
    static int16_t _dht_readings[2][DHT_NUMREADINGS] = {0};

    _sensors_data[TEMPERATURE_INDEX].total -= _dht_readings[0][_dht_read_index];
    _sensors_data[HUMIDITY_INDEX].total -= _dht_readings[1][_dht_read_index];

    _dht_readings[0][_dht_read_index] = t;
    _dht_readings[1][_dht_read_index] = h;

    _sensors_data[TEMPERATURE_INDEX].total += _dht_readings[0][_dht_read_index];
    _sensors_data[HUMIDITY_INDEX].total += _dht_readings[1][_dht_read_index];

    _sensors_data[TEMPERATURE_INDEX].average_value = _sensors_data[TEMPERATURE_INDEX].total / DHT_NUMREADINGS;
    _sensors_data[HUMIDITY_INDEX].average_value = _sensors_data[HUMIDITY_INDEX].total / DHT_NUMREADINGS;

    _dht_read_index++;

    if (_dht_read_index >= DHT_NUMREADINGS) {
        _dht_read_index = 0;
    }
}

// -----------------------------------------------------------------------------

void dustLoop() {
    static uint8_t _dust_read_index = 0;

    _storeDustValue(readDust(), DUST_LEVEL_INDEX, _dust_read_index);
    _storeDustValue(readDust(true), DUST_VALUE_INDEX, _dust_read_index);

    _dust_read_index++;

    if (_dust_read_index >= DUST_NUMREADINGS) {
        _dust_read_index = 0;
    }
}

// -----------------------------------------------------------------------------

void lightLoop()
{
  static uint8_t _light_read_index = 0;

  _storeLightValue(readLight(), LIGHT_LEVEL_INDEX, _light_read_index);
  _storeLightValue(readIlluminance(), ILLUMINANCE_INDEX, _light_read_index);

  _light_read_index++;

  if (_light_read_index >= LIGHT_NUMREADINGS) {
      _light_read_index = 0;
  }
}

// -----------------------------------------------------------------------------

void noiseLoop()
{
    static uint8_t _adc_read_index = 0;
    static uint8_t _noise_read_index = 0;
    static int16_t _noise_readings[NOISE_NUMREADINGS] = {0};

    static int16_t _noise_max = -1;
    static int16_t _noise_min = 1025;

    static long sound_vol_avg = 0;
    static long sound_vol_max = 0;
    static long sound_vol_rms = 0;

    int16_t nosie_value_temp = 0;
            nosie_value_temp = analogRead(MICROPHONE_PIN);

    _noise_max = max(nosie_value_temp, _noise_max);
    _noise_min = min(nosie_value_temp, _noise_min);

    _adc_read_index++;

    unsigned int peak = map(_noise_max - _noise_min, 0, MAX_ADC_READING, 0, 100);

    if (_clap_enabled) {
        clapRecord(peak);
    }

    if (_adc_read_index >= ADC_NUMREADINGS) {
        _sensors_data[NOISE_INDEX].total -= _noise_readings[_noise_read_index];

        _noise_readings[_noise_read_index] = (_noise_max - _noise_min);

        _sensors_data[NOISE_INDEX].total += _noise_readings[_noise_read_index];
        _sensors_data[NOISE_INDEX].average_value =  _sensors_data[NOISE_INDEX].total / NOISE_NUMREADINGS;
        _sensors_data[NOISE_INDEX].value = _sensors_data[NOISE_INDEX].average_value > 999 ? 10 : (_sensors_data[NOISE_INDEX].average_value / 100 + 1);
        // _sensors_data[NOISE_INDEX].voltage_value = _sensors_data[NOISE_INDEX].average_value * (ADC_REF_VOLTAGE / MAX_ADC_READING);

        _noise_read_index++;

        _noise_max = -1;
        _noise_min = 1025;

        _adc_read_index = 0;
    }

    if (_noise_read_index >= NOISE_NUMREADINGS) {
        _noise_read_index = 0;
    }
}

void clapDecode() {
    // at least 2 claps
    if (_clap_pointer > 0) {
        byte code = 2;

        if (_clap_pointer > 1) {
            int length = _clap_timings[0] * CLAP_TOLERANCE;

            for(byte i=1; i<_clap_pointer; i++) {
                code <<= 1;
                if (_clap_timings[i] > length) {
                    code += 1;
                }
            }
        }

        Serial.write("AT+UPDATE=\"clap\":");
        Serial.print(String(code));

        Serial.write(0x1B);
        Serial.write("\n");
    }

    // reset
    _clap_pointer = 0;
}

void clapRecord(int value) {
    static bool reading = false;
    static unsigned long last_clap;
    static int counts = 0;
    unsigned long current = millis();
    unsigned long span = current - last_clap;

    if (value > CLAP_SENSIBILITY) {
        ++counts;

    } else {
        counts = 0;
    }

    if (counts == CLAP_COUNT_TRIGGER) {
        // Is it the first clap?
        if (!reading) {
            last_clap = current;
            reading = true;

        // or not
        } else {
            // timed out
            if (span > CLAP_TIMEOUT_DELAY) {
                clapDecode();

                // reset
                reading = false;

            } else if (span < CLAP_DEBOUNCE_DELAY) {
                // do nothing

            // new clap!
            } else if (_clap_pointer < CLAP_BUFFER_SIZE) {
                _clap_timings[_clap_pointer] = span;
                last_clap = current;
                _clap_pointer++;

            // buffer overrun
            } else {
                _clap_pointer = 0;
                reading = false;
            }
        }

    // check if we have to process it
    } else if (reading) {
        if (span > CLAP_TIMEOUT_DELAY) {
            clapDecode();

            // back to idle
            reading = false;
        }
    }
}

// -----------------------------------------------------------------------------

void moveLoop(bool force = false) {
    bool value = readMovement();

    if ((force || value != _movement) && _comm_status) {
        Serial.write("AT+UPDATE=\"movement\":");
        Serial.print(String(value ? 1 : 0));

        Serial.write(0x1B);
        Serial.write("\n");
    }

    _movement = value;
}

// -----------------------------------------------------------------------------
// SENSORS READINGS
// -----------------------------------------------------------------------------

float readDust(bool converted) {
    float reading = analogRead(SHARP_READ_PIN);

    // 0.5V ==> 100ug/m3
    if (converted) {
        // mg/m3
        float dust = (0.17 * reading * (ADC_REF_VOLTAGE / MAX_ADC_READING) - 0.1) * 1000;

        return dust < 0 ? 0 : dust;
    }

    return reading;
}

// -----------------------------------------------------------------------------

float readDust() {
    return readDust(false);
}

// -----------------------------------------------------------------------------

int readLight() {
    return analogRead(LDR_PIN);
}

// -----------------------------------------------------------------------------

float readIlluminance() {
    int ldr_raw_data;
    float ldr_voltage;
    float ldr_resistance;
    float ldr_lux;

    // Perform the analog to digital conversion
    ldr_raw_data = readLight();

    // LDR VOLTAGE CONVERSION
    // Convert the raw digital data back to the voltage that was measured on the analog pin
    ldr_voltage = (float) ldr_raw_data / MAX_ADC_READING * ADC_REF_VOLTAGE;

    // LDR RESISTANCE CONVERSION
    // resistance that the LDR would have for that voltage
    ldr_resistance = (ldr_voltage * LDR_REF_RESISTANCE) / (ADC_REF_VOLTAGE - ldr_voltage);

    // LDR LUX
    // Change the code below to the proper conversion from ldr_resistance to ldr_lux
    ldr_lux = LUX_CALC_SCALAR * pow(ldr_resistance, LUX_CALC_EXPONENT);

    return ldr_lux;
}

// -----------------------------------------------------------------------------

bool readMovement() {
    return digitalRead(MW_PIN) == HIGH;
}

// -----------------------------------------------------------------------------
// SENSORS INTERNAL API
// -----------------------------------------------------------------------------

void _storeDustValue(float value, uint8_t sensor, uint8_t read_index) {
    static int16_t _dust_readings[2][DUST_NUMREADINGS] = {0};

    int16_t data_index = 0;

    if (sensor == DUST_LEVEL_INDEX) {
      data_index = 1;
    }

    _sensors_data[sensor].total -= _dust_readings[data_index][read_index];

    _dust_readings[data_index][read_index] = value;

    _sensors_data[sensor].total += _dust_readings[data_index][read_index];
    _sensors_data[sensor].average_value = _sensors_data[sensor].total / DUST_NUMREADINGS;

    if (sensor == DUST_LEVEL_INDEX) {
        _sensors_data[sensor].value = _sensors_data[sensor].average_value > 799 ? 10 : (_sensors_data[sensor].average_value / 80 + 1);
    }
}

// -----------------------------------------------------------------------------

void _storeLightValue(float value, uint8_t sensor, uint8_t read_index) {
    static int16_t _light_readings[2][LIGHT_NUMREADINGS] = {0};

    int16_t data_index = 0;

    if (sensor == LIGHT_LEVEL_INDEX) {
      data_index = 1;
    }

    _sensors_data[sensor].total -= _light_readings[data_index][read_index];

    _light_readings[data_index][read_index] = value;

    _sensors_data[sensor].total += _light_readings[data_index][read_index];
    _sensors_data[sensor].average_value = _sensors_data[sensor].total / LIGHT_NUMREADINGS;
    // _sensors_data[sensor].voltage_value = _sensors_data[sensor].average_value * (ADC_REF_VOLTAGE / MAX_ADC_READING);

    if (sensor == LIGHT_LEVEL_INDEX) {
      _sensors_data[sensor].value = _sensors_data[sensor].average_value > 999 ? 10 : (_sensors_data[sensor].average_value / 100 + 1);
    }
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

    for (i = 0; i < INDEX_SIZE; i++) {
        if (i == TEMPERATURE_INDEX || i == HUMIDITY_INDEX || i == DUST_VALUE_INDEX || i == ILLUMINANCE_INDEX) {
            _sensors_data[i].last_value = _sensors_data[i].average_value;

        } else {
            _sensors_data[i].last_value = _sensors_data[i].value;
        }
    }
}

// -----------------------------------------------------------------------------

void _sendData()
{
    Serial.write("AT+UPDATE=\"humidity\":");
    Serial.print(String(_sensors_data[HUMIDITY_INDEX].average_value));

    Serial.write(",\"temperature\":");
    Serial.print(String(_sensors_data[TEMPERATURE_INDEX].average_value));

    Serial.write(",\"light\":");
    Serial.print(String(_sensors_data[LIGHT_LEVEL_INDEX].value));

    Serial.write(",\"illuminance\":");
    Serial.print(String(_sensors_data[ILLUMINANCE_INDEX].average_value));

    Serial.write(",\"noise\":");
    Serial.print(String(_sensors_data[NOISE_INDEX].value));

    Serial.write(",\"dusty\":");
    Serial.print(String(_sensors_data[DUST_LEVEL_INDEX].value));

    Serial.write(",\"dust\":");
    Serial.print(String(_sensors_data[DUST_VALUE_INDEX].average_value));

    Serial.write(",\"movement\":");
    Serial.print(String(readMovement() ? 1 : 0));

    Serial.write(0x1B);
    Serial.write("\n");
}

// -----------------------------------------------------------------------------
// PROGRAM CORE
// -----------------------------------------------------------------------------

void setup() {
    // Setup Serial port
    Serial.begin(SERIAL_BAUDRATE);

    initPins();
    initPwm();
    initData();

    // Setup the DHT Thermometer/Humidity Sensor
    dht.begin();

    attachInterrupt(0, handleInterupt, RISING);

    MsTimer2::set(1000, handleTimer); // 500ms period
    MsTimer2::start();
}

// -----------------------------------------------------------------------------

void loop() {
    readUart();

    if (_read_temp_humi_flag) {
        _read_temp_humi_flag = false;

        temperatureHumidityLoop();
    }

    if (_read_adc_flag) {
        _read_adc_flag = false;

        dustLoop();
        lightLoop();
        noiseLoop();
    }

    if (_update_values_flag) {
        _update_values_flag = false;

        transmithData();
    }

    moveLoop();
}
