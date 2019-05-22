CC = g++
LIBS = -lwiringPi
CFLAGS = -g3 -std=c++14 -Wall -DLINUX -ILoRaLib/modules/ -I./ -ILoRaLib/linux-workarounds/SPI/

# Should be equivalent to your list of C files, if you don't build selectively
SRC = $(wildcard LoRaLib/linux-workarounds/SPI/*.cpp) $(wildcard LoRaLib/linux-workarounds/*.cpp) \
      $(wildcard LoRaLib/protocols/*.cpp) $(wildcard LoRaLib/modules/*.cpp) \
      $(wildcard smtUdpPacketForwarder/base64/*.c) \
      $(wildcard smtUdpPacketForwarder/gpsTimestampUtils/*.cpp) \
      $(wildcard smtUdpPacketForwarder/*.cpp) $(wildcard *.cpp)

all: $(SRC)
	$(CC) -o LoRaPktFwrd $^ $(CFLAGS) $(LIBS)

clean:
	rm -f ./LoRaPktFwd
