# 433Mhz & IR <-> MQTT gateway with DHT and alarm (home assistant MQTT alarm compatibile)  

It enables to:
- receive MQTT data from a topic and send the 433Mhz signal 
- receive MQTT data from a topic and send the IR signal 
- receive IR signal from a remote, optional map it to a channel and publish the data to a MQTT topic
- receive 433Mhz signal from a traditional remote, optional map it to a channel and publish the data to a MQTT topic
- additional DHT senor can record the room temperature & humidity of the room where the gateway is installed
- can work as alarm, it support MQTT Alarm Control Panel from Home assistant


The software is based on [Homie](https://github.com/marvinroger/homie-esp8266) to enable an easy integration with home automation systems like home assistant

## Hardware

- Nodemcu
- RF Receiver 433Mhz
- RF Transmitter 433MHz
- IR diode
- IR Reciever
- DHT sensor

### Building the circuit

todo

## Software

The following software libraries are used:

- [Homie V2]
- [RCSwitch]
- [DHT]
- [IRremoteESP8266]
- [Adafruit Unified Sensor]
- Optinally PlatformIO environment for building the code

## Config

The following config parameters are available via MQTT message (see Homie documentation how to use):

Parameter           | Type        | Usage
------------------- | ----------- | -------
channels            | const char* | mapping of 433MHz signals to mqtt channels, useful if used with OpenHab

All configs can be set during the init proccedure of the module or via MQTT messages (see Homie specification).

Sampe config:


TODO



## Credits

- 1 Technophile's [433toMQTTto433](https://1technophile.blogspot.de/2016/09/433tomqttto433-bidirectional-esp8266.html)
- Mhaack (https://github.com/mhaack/mqtt-433mhz-gateway-homie)

