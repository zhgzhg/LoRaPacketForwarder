CC = g++
LIBS = -lwiringPi -lm -lpthread -lrt -lcrypt
CFLAGS = -std=c++14 -Wall -DLINUX -DARDUINO=999 -DRADIOLIB_FIX_ERRATA_SX127X -IRadioLib/src/ \
     -IRadioLib/src/modules/ -IRadioLib/src/protocols/ -IRadioLib/src/modules/RF69/ -IRadioLib/src/modules/RFM9x/ \
     -IRadioLib/src/modules/SX127x/ -IRadioLib/src/modules/SX126x/ -IRadioLib/src/modules/LLCC68/ \
     -IRadioLib/src/linux-workarounds/ -IRadioLib/src/linux-workarounds/SPI/

# Should be equivalent to your list of C files, if you don't build selectively
SRC = $(wildcard RadioLib/src/linux-workarounds/SPI/*.cpp) $(wildcard RadioLib/src/linux-workarounds/*.cpp) \
      $(wildcard RadioLib/src/protocols/PhysicalLayer/*.cpp) $(wildcard RadioLib/src/modules/RF69/*.cpp) \
      $(wildcard RadioLib/src/modules/RFM9x/*.cpp) $(wildcard RadioLib/src/modules/SX127x/*.cpp) \
      $(wildcard RadioLib/src/modules/SX126x/*.cpp) $(wildcard RadioLib/src/modules/LLCC68/*.cpp) \
      $(wildcard RadioLib/src/*.cpp) \
      $(wildcard smtUdpPacketForwarder/base64/*.c) $(wildcard smtUdpPacketForwarder/gpsTimestampUtils/*.cpp) \
      $(wildcard smtUdpPacketForwarder/*.cpp) $(wildcard *.cpp)

TAG_COMMIT := $(shell git rev-list --abbrev-commit --tags --max-count=1 || true)
TAG := $(shell git describe --abbrev=0 --tags ${TAG_COMMIT} 2>/dev/null || true)
COMMIT := $(shell git rev-parse --short HEAD || true)
VERSION := $(TAG:v%=%)
ifneq ($(COMMIT), $(TAG_COMMIT))
	ifneq ($(VERSION),)
		VERSION := $(VERSION)-
	endif
	VERSION := $(VERSION)next-$(COMMIT)
endif
ifeq ($(VERSION),)
	ifeq ($(COMMIT),)
		VERSION := unknown
	else
		VERSION := $(COMMIT)
	endif
endif
ifneq ($(shell git status --porcelain || true),)
	VERSION := $(VERSION)-dirty
endif


all: $(SRC)
	$(CC) -o LoRaPktFwrd $^ $(CFLAGS) -DGIT_VER=$(VERSION) -O3 $(LIBS)

debug: $(SRC)
	$(CC)  -o LoRaPktFwrd $^ -g3 -DRADIOLIB_DEBUG -DGIT_VER=$(VERSION) $(CFLAGS) $(LIBS)

clean:
	rm -f ./LoRaPktFwrd

install:
	mkdir -p /etc/LoRaPacketForwarder
	cp -f ./config.json.template /etc/LoRaPacketForwarder
	cp -f ./config.json /etc/LoRaPacketForwarder
	cp -f ./LoRaPktFwrd /usr/bin
	chmod a+x /usr/bin/LoRaPktFwrd
	cp -f LoRaPktFwrd.service /lib/systemd/system
	systemctl daemon-reload

uninstall:
	systemctl stop LoRaPktFwrd.service
	systemctl disable LoRaPktFwrd.service
	rm -f /lib/systemd/system/LoRaPktFwrd.service
	systemctl daemon-reload
	rm -fr /etc/LoRaPacketForwarder
	rm -f /usr/bin/LoRaPktFwrd

