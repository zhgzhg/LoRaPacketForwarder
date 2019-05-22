LoRa UDP Packet Forwarder for Linux
===================================

Single channel LoRa UDP packet forwarder based on several libraries and
existing projects.

The goal of the project is to provide simple LoRa forwarder for:

* Linux - OrangePi, RaspberryPi, etc.
* Very basic support for Semtech UDP protocol v2 - uplink and stats
    * The Things Network - tested
* SPI communication based on wiringPi and modified LoRaLib for Linux (Orange Pi or Raspberry PI) with LoRa chips:
    * SX1278 - tested
    * Probably supports too:
        * SX127x
        * RFM9x
* Simple JSON configuration file

How to Use
==========

## Clone the project
`git clone --recurse-submodules https://github.com/zhgzhg/LoRaPacketForwarder.git`


### Orange PI: compile & install

The following steps have been tested on Arbian v5.73.

* Compile wiringPi for Orange PI with ./build
    * Optionally specify PLATFORM before to change the board config. ( for e.g. for Orange Pi PC: PLATFORM=OrangePi_H3 ./build ).
    * execute gpio readall to see the board pinout scheme table
* Compile this project with make


### Raspberry PI and others (not tested):  compile & install

* WiringPi is preinstalled - no actions needed
* Compile this project with make


## Running LoRa UDP Packet Forwarder

* Create config.json by copying config.json.template
    * edit the pinout (execute gipo readall to check wiringPi pin numbers that need to be specified)
    * edit the rest parameters accordingly

* To execute the application:
    * ./LoRaPktFwd

* To execute the application and also specify network interface used for ID generation
    * ./LoRaPktFwd <interface_name>
        * The default one is eth0
        * Example: ./LoRaPktFwd wlan0

### This project is influenced and contains code from:

[https://github.com/jgromes/LoRaLib](https://github.com/jgromes/LoRaLib)

[https://github.com/adafruit/single_chan_pkt_fwd](https://github.com/adafruit/single_chan_pkt_fwd)

[https://github.com/orangepi-xunlong/wiringOP](https://github.com/orangepi-xunlong/wiringOP)

[https://github.com/Lora-net/packet_forwarder](https://github.com/Lora-net/packet_forwarder)

[https://www.gw-openscience.org/static/js/gpstimeutil.js](https://www.gw-openscience.org/static/js/gpstimeutil.js)

[https://github.com/Tencent/rapidjson](https://github.com/Tencent/rapidjson)
