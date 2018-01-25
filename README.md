# SonoffSC

Custom firmware for the Sonoff SC slave unit based on ATMega328P.

[![version](https://badge.fury.io/gh/FastyBird%2Fsonoff-sc.svg)](CHANGELOG.md)
[![travis](https://travis-ci.org/FastyBird/sonoff-sc.svg?branch=master)](https://travis-ci.org/FastyBird/sonoff-sc)
[![license](https://img.shields.io/github/license/FastyBird/sonoff-sc.svg)](LICENSE)

## Features

* Fully compatible with original ESP8266 firmware communication protocol
* Support for new temperature & humidity sensor DHT22
* **Microwave** based presence detector
* **Clap monitoring** sends info when clapping is detected

## Documentation

### Communication from ATMega328 to ESP8266 master:

```
  AT+UPDATE="humidity":42,"temperature":20,"light":7,"illuminance":150,"noise":3,"dusty":1,"dust":0[1B]
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

### Sequence:

```
 SC master sends:       ATMEGA328P sends:
 AT+START[1B]
                        AT+UPDATE="humidity":42,"temperature":20,"light":7,"illuminance":150,"noise":3,"dusty":1,"dust":0[1B]
 AT+SEND=ok[1B]
                        AT+STATUS?[1B]
 AT+STATUS=4[1B]
```

### Microwave sensor

In case microwave is present, ATMega328 will send additional message:

```
  AT+UPDATE="movement":1[1B]
```

Value in this message could be 0 or 1, when 0 mean no presence detected.

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
