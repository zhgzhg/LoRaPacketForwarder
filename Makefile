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

all: $(SRC)
	$(CC) -o LoRaPktFwrd $^ $(CFLAGS) -O3 $(LIBS)

debug: $(SRC)
	$(CC)  -o LoRaPktFwrd $^ -g3 -DRADIOLIB_DEBUG $(CFLAGS) $(LIBS)

clean:
	rm -f ./LoRaPktFwrd
