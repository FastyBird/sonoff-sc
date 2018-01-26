# Sonoff SC

Custom firmware for the [Itead Sonoff SC](https://sonoff.itead.cc/en/products/residential/sonoff-sc) environmental unit - sensors part.

[![version](https://badge.fury.io/gh/FastyBird%2Fsonoff-sc.svg)](CHANGELOG.md)
[![travis](https://travis-ci.org/FastyBird/sonoff-sc.svg?branch=master)](https://travis-ci.org/FastyBird/sonoff-sc)
[![license](https://img.shields.io/github/license/FastyBird/sonoff-sc.svg)](LICENSE)

## Features

* Fully compatible with original EWeLink firmware
* Support for two types of temperature and humidity sensors, DHT11 & DHT22
* Optionally support **Microwave based** presence detector
* **Clap monitoring** sends info when clapping is detected

## Documentation

This firmware is only for the slave unit in the Sonoff SC device and could be only uploaded into ATMega328.

### Communication from ATMega328 to ESP8266 master:

```
  AT+UPDATE="humidity":42,"temperature":20,"light":7,"noise":3,"dusty":1,"dust":0,"illuminance":150[1B]
    response: AT+SEND=ok[1B] or AT+SEND=fail[1B]
  AT+STATUS?[1B]
    response: AT+STATUS=4[1B]
```

### Communication from ESP8266 master to ATMega328:

```
  AT+DEVCONFIG="uploadFreq":1800,"humiThreshold":2,"tempThreshold":1[1B]
  AT+NOTIFY="uploadFreq":1800,"humiThreshold":2,"tempThreshold":1[1B]
    response: AT+NOTIFY=ok[1B]
  AT+SEND=fail[1B]
  AT+SEND=ok[1B]
  AT+STATUS=4[1B]
  AT+STATUS[1B]
  AT+START[1B]
```

### Communication sequence:

#### Data exchange:

Communication have to be started from the master unit (ESP8266):

```
AT+START[1B]
```

Slave unit will respond with measured values:

```
AT+UPDATE="humidity":42,"temperature":20,"light":7,"noise":3,"dusty":1,"dust":0,"illuminance":150[1B]
```

Master have to answer to this response. If data are ok, then:

```
AT+SEND=ok[1B]
```

and if received data are not ok, then:

```
AT+SEND=fail[1B]
```

#### Connection check:

Slave unit, will in defined periods check the connection to the master. So it will ask master if is ready and receiving:

```
AT+STATUS?[1B]
```

Master have to answer with:

```
AT+STATUS=4[1B]
```

In case master do not respond to this check connection request, slave will stop transmit data.

### Display levels for measured values

This levels were measured during testing communication between this custom firmware and original EWeLink firmware.

#### Sound level

* **quiet** - for values from **0.00** to **3.00**
* **normal** - for values from **3.01** to **6.00**
* **noisy** - for values from **6.01** to **10.00**

#### Light intensity

* **bright** - for values from **0.00** to **4.00**
* **normal** - for values from **4.01** to **8.00**
* **dusky** - for values from **8.01** to **10.00**

#### Dust level or Air quality level

* **good** - for values from **0.00** to **4.00**
* **moderate** - for values from **4.01** to **7.00**
* **unhealthy** - for values from **7.01** to **10.00**

### Microwave sensor

 case microwave is present, ATMega328 will send extended data message:

```
  AT+UPDATE="humidity":42,"temperature":20,"light":7,"noise":3,"dusty":1,"dust":0,"illuminance":150,"movement":1[1B]
```

Value for **movement** parameter could be 0 or 1, when 0 mean no presence detected.

## License

Copyright (C) 2017-2018 by FastyBird Ltd.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
