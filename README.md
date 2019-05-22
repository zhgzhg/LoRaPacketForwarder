LoRaPacketForwarder for Linux
=============================

Single channel LoRa UDP packet forwarder based
on several libraries and existing projects.

Its goal is to provide simple LoRa forwarder for:

* Linux - OrangePi, RaspberryPi, etc.
* Very basic support for Semtech UDP protocol v2 - uplink and stats
    * The Things Network - tested
* SPI communication based on wiringPi (Orange Pi or Raspberry PI) with LoRa chips:
    * SX1278 - tested
    * Probably supports too:
        * SX127x
	* RFMxx
* Simple JSON configuration file

How to Use
==========

### Orange PI: compile & install

The following steps have been tested on Arbian v5.73.

* Compile wiringPi for Orange PI with ./build
    * Optionally specify PLATFORM before to change the board config. ( for e.g. for Orange Pi PC: PLATFORM=OrangePi_H3 ./build ).
    * execute gpio readall to see the board pinout scheme table
* Compile this project with make


### Raspberry PI and others (not tested):  compile & install


* WiringPi is preinstalled - no actions needed
* Compile this project with make

## Running

* Create config.json by copying config.json.template
    * edit the pinout (execute gipo readall to check wiringPi pin numbers that need to be specified)
    * edit the rest parameters accordingly

* To execute the application:
    * ./LoRaPktFwd

* To execute the application and also specify network interface used for ID generation
    * ./LoRaPktFwd <interface_name>
        * The default one is eth0
	* Example: ./LoRaPktFwd wlan0

