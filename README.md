LoRa Packet Forwarder for Linux
===============================

[![Build Status](https://travis-ci.com/zhgzhg/LoRaPacketForwarder.svg?branch=master)](https://travis-ci.com/zhgzhg/LoRaPacketForwarder)

Single channel LoRa UDP packet forwarder ideal for development or
testing purposes. Receives and transmits bidirectionally.
This project is ideal for DIY of one of the cheapest LoRa "gateways"
consisting of a single-board computer and a $4 LoRa module.

The goal of the project is to provide simple LoRa forwarder for:

* Linux - Orange Pi, Raspberry Pi, etc.
* Supports the Semtech UDP protocol v2 - uplink, downlink, and stats:
    * The Things Network - tested V2 and V3
    * ChirpStack - tested V3
* Supported LoRaWAN device classes:
    * class A , LoRa modulation - tested
* SPI communication based on wiringPi and modified LoRaLib/RadioLib for Linux (Orange Pi or Raspberry PI) with LoRa chips:
    * SX126x series
    * SX127x series
    * RFM9x series
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
* 3 or 4 GPIO pins for connecting to the LoRa module:
    * CS (a.k.a NSS)
    * DIO 0
    * DIO 1
    * REST - **optional**
* power pins - 2 pins for VCC (usually 3.3V) and 1 for GND

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
          |         REST |===== optional, if it isn't used leave floating or connect to VCC
          |          GND |
           --------------

```


## Clone the project
`git clone --recurse-submodules https://github.com/zhgzhg/LoRaPacketForwarder.git`


### Orange PI: compile & install

The following steps have been tested on Armbian v5.90. However it is recommended
to use its **latest** version.

* Compile wiringPi for Orange PI **//for the ZERO model use WiringOP-Zero library instead//** with `./build` command
    * Optionally specify the PLATFORM variable to change the board config (for e.g. for Orange Pi PC: `PLATFORM=orangepipc ./build`) or leave the build script to determine it automatically.
    *  On Armbian and Orange PI you might need to add the `spi-spidev` overlay. Additionally in __/boot/armbianEnv.txt__ you'll need to add parameter `param_spidev_spi_bus=1` or `param_spidev_spi_bus=0` depending on the board model. For e.g.:
        * Orange PI Zero - `param_spidev_spi_bus=1`
        * Orange PI PC - `param_spidev_spi_bus=0`
        * For more information check [https://docs.armbian.com/User-Guide_Allwinner_overlays/](https://docs.armbian.com/User-Guide_Allwinner_overlays/), [https://github.com/armbian/sunxi-DT-overlays/blob/master/sun8i-h3/README.sun8i-h3-overlays](https://github.com/armbian/sunxi-DT-overlays/blob/master/sun8i-h3/README.sun8i-h3-overlays)) , and consult with the board's specific docs
    * Execute `gpio readall` to see the board pinout scheme table
* Compile this project with `make`


### Raspberry PI and others: compile & install

* WiringPi is pre-installed on Raspbian - no actions needed
* Compile this project with `make`


## Running LoRa UDP Packet Forwarder

* Create config.json by copying config.json.template:
    * Edit the ic_model field to specify the LoRa chip model. Use one of the following supported values below:
        * For SX126x series: `SX1261`, `SX1262`, or `SX1268`
        * For SX127x series: `SX1272`, `SX1273`, `SX1276`, `SX1277`, `SX1278`, or `SX1279`
        * For RFM9x series: `RFM95`, `RFM96`, `RFM97`, or `RFM98`
    * Edit the pinout (execute `gpio readall` to check wiringPi pin numbers that need to be specified). Please
**note** that ***pin_rest*** is *optional*. If it isn't used you should set it to -1 and leave the transceiver's pin floating or connected to VCC;
    * Edit the remaining parameters accordingly.

* To execute the application:
    * `./LoRaPktFwrd`

* To execute the application and also specify network interface used for ID generation:
    * ./LoRaPktFwrd <interface_name>
        * The default one is eth0
        * Example: `./LoRaPktFwrd wlan0`

* To get the supported CLI options:
    * `./LoRaPktFwrd -h`


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


## Other Extras


### [TempMon](tools/TempMon)

A tiny temperature monitor program that can run in the background and modify GPIO pins in response.


## Limitations

Achieving a perfect downlink transmission timings appears to be difficult with the combination of
an ordinary single-board computer equipped with a plain LoRa transceiver. The reason for that comes
down to the imprecise hardware clock of the computer combined with the non-real time nature of Linux.
To compensate for it this project aims running with a very high (nearly real time) priority,
and increased CPU usage (roughly 20%) to partially make up for the irregular OS delays.


## This project is influenced and contains code from:

[https://github.com/jgromes/RadioLib](https://github.com/jgromes/RadioLib)

[https://github.com/adafruit/single_chan_pkt_fwd](https://github.com/adafruit/single_chan_pkt_fwd)

[https://github.com/orangepi-xunlong/wiringOP](https://github.com/orangepi-xunlong/wiringOP)

[https://github.com/xpertsavenue/WiringOP-Zero](https://github.com/xpertsavenue/WiringOP-Zero)

[https://github.com/Lora-net/packet_forwarder](https://github.com/Lora-net/packet_forwarder)

[https://www.gw-openscience.org/static/js/gpstimeutil.js](https://www.gw-openscience.org/static/js/gpstimeutil.js)

[https://github.com/Tencent/rapidjson](https://github.com/Tencent/rapidjson)
