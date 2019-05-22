CC = g++
LIBS = -lwiringPi
CFLAGS = -std=c++14 -Wall -DLINUX -ILoRaLib/src/modules/ -ILoRaLib/src/ -ILoRaLib/src/linux-workarounds/SPI/

# Should be equivalent to your list of C files, if you don't build selectively
SRC = $(wildcard LoRaLib/src/linux-workarounds/SPI/*.cpp) $(wildcard LoRaLib/src/linux-workarounds/*.cpp) \
      $(wildcard LoRaLib/src/protocols/*.cpp) $(wildcard LoRaLib/src/modules/*.cpp) \
      $(wildcard smtUdpPacketForwarder/base64/*.c) \
      $(wildcard smtUdpPacketForwarder/gpsTimestampUtils/*.cpp) \
      $(wildcard smtUdpPacketForwarder/*.cpp) $(wildcard *.cpp)

all: $(SRC)
	$(CC) -o LoRaPktFwrd $^ $(CFLAGS) $(LIBS)

clean:
	rm -f ./LoRaPktFwd
