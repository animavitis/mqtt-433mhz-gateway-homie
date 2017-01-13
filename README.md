# 433Mhz <-> MQTT gateway with some extras  

[![Build Status](https://travis-ci.org/mhaack/mqtt-433mhz-gateway-homie.svg?branch=master)](https://travis-ci.org/mhaack/mqtt-433mhz-gateway-homie)

The mqtt-433mhz-gateway-homie project is a simple bidirectional gateway to transmit and receive 433Mhz RF signals connected to MQTT. The 
gateway is built with a cost-effective ESP8266 WiFi chip (I used a Wemos D1 mini, NodeMCU will do as well), simple 433Mhz RF modules and 
an additional BMP085 sensor.

It enables to:
- receive MQTT data from a topic and send the 433Mhz signal 
- receive 433Mhz signal from a traditional remote, optional map it to a channel and publish the data to a MQTT topic
- additional a simple temperature senor can record the room temperature of the room where the gateway is installed

The software is based on [Homie](https://github.com/marvinroger/homie-esp8266) to enable an easy integration with home automation systems like [OpenHab](http://www.openhab.org).

## Hardware

- ESP8266 (Wemos D1 mini, Nodemcu)
- RF Receiver 433Mhz
- RF Transmitter 433MHz
- BMP085 or BMP180 sensor breakout

I got the RF modules form https://www.sparkfun.com, others will do as well. Additional I got some [Wemos Protoboards](https://www.wemos.cc/product/protoboard.html), an USB power supply with every short cabele
and an [enclosure](https://www.amazon.de/gp/product/B00PZYMLJ4) to hold all together.

### Building the circuit

Wemos D1 mini | ...
------------- | ------------ 
5V            | X
GND           | GND
GND           | X
GND           | X

## Software

The following software libraries are used. When using PlatformIO all dependencies are resolved automatically.

- [Homie V2](https://github.com/marvinroger/homie-esp8266) (dev) including dependencies
- [RCSwitch](https://github.com/sui77/rc-switch)
- [Adafruit BMP085 Unified](https://github.com/adafruit/Adafruit_BMP085_Unified)
- [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor)
- Optinally PlatformIO environment for building the code

## Config

The following config parameters are available via MQTT message (see Homie documentation how to use):

Parameter           | Type        | Usage
------------------- | ----------- | -------
temperatureInterval | long        | temperature reading interval in seconds
temperatureOffset   | double      | temperature offset (-/+) to correct the sensor reading, for example if used in enclosure box
channels            | const char* | mapping of 433MHz signals to mqtt channels, useful if used with OpenHab

## Credits

This project is was inspired by 1 Technophile's [433toMQTTto433](https://1technophile.blogspot.de/2016/09/433tomqttto433-bidirectional-esp8266.html) solution.

