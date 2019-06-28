LoRa UDP Packet Forwarder for Linux
===================================

[![Build Status](https://travis-ci.com/zhgzhg/LoRaPacketForwarder.svg?branch=master)](https://travis-ci.com/zhgzhg/LoRaPacketForwarder)

Single channel LoRa UDP packet forwarder ideal for development or
testing purposes. Can only receive LoRa data and upload it to one 1 more
servers. 
This project is ideal for DIY of one of the cheapest LoRa "gateways"
consisting of a single-board computer and a $4 LoRa module.

The goal of the project is to provide simple LoRa forwarder for:

* Linux - OrangePi, RaspberryPi, etc.
* Very basic support for Semtech UDP protocol v2 - uplink and stats
    * The Things Network - tested
* SPI communication based on wiringPi and modified LoRaLib for Linux (Orange Pi or Raspberry PI) with LoRa chips:
    * SX1278 - tested
    * Probably supports whole chips series like:
        * SX127x 
        * RFM9x 
* Basic JSON configuration file

How to Use
==========

## Hardware Setup

Along with network connection on your single-board computer (WiFi,
Ethernet...) the following pins on your device have to be allocated:

* SPI module pins
    * MISO
    * MOSI
    * CLK
* 3 GPIO pins for connecting to LoRa's module:
    * CS (a.k.a NSS)
    * DIO 0
    * DIO 1
* power pins - 1 for VCC (usually 3.3V) and 1 for GND

Please refer to command `gpio readall` (also check the next sections) to
obtain more information for your particular board. Look into the WiringPi
numbers as well, because the configuration file expects that numbering
scheme.

For e.g.:

```

 +------+-----+----------+------+---+OrangePiH3+---+------+----------+-----+------+
 | GPIO | wPi |   Name   | Mode | V | Physical | V | Mode | Name     | wPi | GPIO |
 +------+-----+----------+------+---+----++----+---+------+----------+-----+------+
 |      |     |     3.3v |      |   |  1 || 2  |   |      | 5v       |     |      |
 |   12 |   0 |    SDA.0 | ALT2 | 0 |  3 || 4  |   |      | 5V       |     |      |
 |   11 |   1 |    SCL.0 | ALT2 | 0 |  5 || 6  |   |      | 0v       |     |      |
 |    6 |   2 |      PA6 |  OFF | 0 |  7 || 8  | 0 | OFF  | TxD3     | 3   | 13   |
 |      |     |       0v |      |   |  9 || 10 | 0 | OFF  | RxD3     | 4   | 14   |
 |    1 |   5 |     RxD2 |  OFF | 0 | 11 || 12 | 1 | OUT  | PD14     | 6   | 110  |
 |    0 |   7 |     TxD2 |  OFF | 1 | 13 || 14 |   |      | 0v       |     |      |
 |    3 |   8 |     CTS2 |  OFF | 0 | 15 || 16 | 0 | IN   | PC04     | 9   | 68   |
 |      |     |     3.3v |      |   | 17 || 18 | 1 | IN   | PC07     | 10  | 71   |
 |   64 |  11 |     MOSI | ALT3 | 0 | 19 || 20 |   |      | 0v       |     |      |
 |   65 |  12 |     MISO | ALT3 | 0 | 21 || 22 | 0 | OFF  | RTS2     | 13  | 2    |
 |   66 |  14 |     SCLK | ALT3 | 0 | 23 || 24 | 0 | ALT3 | CE0      | 15  | 67   |
 |      |     |       0v |      |   | 25 || 26 | 0 | OFF  | PA21     | 16  | 21   |
 |   19 |  17 |    SDA.1 | ALT3 | 0 | 27 || 28 | 0 | ALT3 | SCL.1    | 18  | 18   |
 |    7 |  19 |     PA07 |  OFF | 0 | 29 || 30 |   |      | 0v       |     |      |
 |    8 |  20 |     PA08 |  OFF | 0 | 31 || 32 | 0 | OFF  | RTS1     | 21  | 200  |
 |    9 |  22 |     PA09 |  OFF | 0 | 33 || 34 |   |      | 0v       |     |      |
 |   10 |  23 |     PA10 |  OFF | 0 | 35 || 36 | 0 | OFF  | CTS1     | 24  | 201  |
 |   20 |  25 |     PA20 |  OFF | 0 | 37 || 38 | 0 | OFF  | TxD1     | 26  | 198  |
 |      |     |       0v |      |   | 39 || 40 | 0 | OFF  | RxD1     | 27  | 199  |
 |    4 |  28 |     PA04 | ALT2 | 0 | 41 || 42 | 0 | ALT2 | PA05     | 29  | 5    |
 +------+-----+----------+------+---+----++----+---+------+----------+-----+------+
 | GPIO | wPi |   Name   | Mode | V | Physical | V | Mode | Name     | wPi | GPIO |
 +------+-----+----------+------+---+OrangePiH3+---+------+----------+-----+------+

   ___
   \_/     SX1278  module
    |      --------------
     \--- | ANT      GND |===== Pin #20 [OrangePiH3 Physical]
          | GND     DIO1 |===== Pin #18 [OrangePiH3 Physical] / [a.k.a WiringPi pin ## 10]
          |         DIO2 |
          |         DIO3 |
          |          VCC |===== Pin # 1 [OrangePiH3 Physical]
          |         MISO |===== Pin #21 [OrangePiH3 Physical] / [[a.k.a WiringPi pin ## 12]]
          |         MOSI |===== Pin #19 [OrangePiH3 Physical] / [[a.k.a WiringPi pin ## 11]]
          |         SLCK |===== Pin #23 [OrangePiH3 Physical] / [[a.k.a WiringPi pin ## 14]]
          |          NSS |===== Pin #12 [OrangePiH3 Physical] / [a.k.a WiringPi pin ## 6]
          |         DIO0 |===== Pin #16 [OrangePiH3 Physical] / [a.k.a WiringPi pin ## 9]
          |         REST |
          |          GND |
           --------------

```


## Clone the project
`git clone --recurse-submodules https://github.com/zhgzhg/LoRaPacketForwarder.git`


### Orange PI: compile & install

The following steps have been tested on Arbian v5.73.

* Compile wiringPi for Orange PI **for the ZERO model use WiringOP-Zero library instead** with `./build` command
    * Optionally specify the PLATFORM variable to change the board config **(not applicable for WiringOP-Zero)** . ( for e.g. for Orange Pi PC: `PLATFORM=OrangePi_H3 ./build` ).
    * On Armbian and Orange PI Zero you might need to add overlay spi-spidev overlay as well as parameter param_spidev_spi_bus=1 in /boot/armbianEnv.txt (for more info check [https://docs.armbian.com/Hardware_Allwinner_overlays/](https://docs.armbian.com/Hardware_Allwinner_overlays/) and [https://github.com/armbian/sunxi-DT-overlays/blob/master/sun8i-h3/README.sun8i-h3-overlays](https://github.com/armbian/sunxi-DT-overlays/blob/master/sun8i-h3/README.sun8i-h3-overlays)
    * execute `gpio readall` to see the board pinout scheme table
* Compile this project with `make`


### Raspberry PI and others (not tested):  compile & install

* WiringPi is pre-installed - no actions needed
* Compile this project with `make`


## Running LoRa UDP Packet Forwarder

* Create config.json by copying config.json.template
    * edit the pinout (execute `gpio readall` to check wiringPi pin numbers that need to be specified)
    * edit the rest parameters accordingly

* To execute the application:
    * `./LoRaPktFwrd`

* To execute the application and also specify network interface used for ID generation
    * ./LoRaPktFwrd <interface_name>
        * The default one is eth0
        * Example: `./LoRaPktFwrd wlan0`
        

## Dependencies

LoRa UDP Packet Forwarder relies on the following programs and libraries:

* g++ supporting C++14 standard
* make
* WiringPi


## How To Test

Provided is a simple LoRa trasmitter example Arduino project called
"Transmit" in the current directory. Hook your ESP8266/Arduino/whatever 
board via SPI to SX1278 module, compile it, and it will start
transmitting data. Using the default configuration inside
"config.json.template" in terms of RF specs, the forwarder app should
immediately pick data from the transmitter.


### This project is influenced and contains code from:

[https://github.com/jgromes/LoRaLib](https://github.com/jgromes/LoRaLib)

[https://github.com/adafruit/single_chan_pkt_fwd](https://github.com/adafruit/single_chan_pkt_fwd)

[https://github.com/orangepi-xunlong/wiringOP](https://github.com/orangepi-xunlong/wiringOP)

[https://github.com/xpertsavenue/WiringOP-Zero](https://github.com/xpertsavenue/WiringOP-Zero)

[https://github.com/Lora-net/packet_forwarder](https://github.com/Lora-net/packet_forwarder)

[https://www.gw-openscience.org/static/js/gpstimeutil.js](https://www.gw-openscience.org/static/js/gpstimeutil.js)

[https://github.com/Tencent/rapidjson](https://github.com/Tencent/rapidjson)
