CC = g++
LIBS = -lwiringPi -lm -lpthread -lrt -lcrypt
CFLAGS = -std=c++14 -Wall

# Should be equivalent to your list of C files, if you don't build selectively
SRC = $(wildcard *.cpp)

all: $(SRC)
	$(CC) -o TempMon $^ $(CFLAGS) -O3 $(LIBS)

debug: $(SRC)
	$(CC)  -o TempMon $^ $(CFLAGS) $(LIBS)

clean:
	rm -f ./TempMon

install:
	mkdir -p /etc/tempmon
	cp -f ./config.json.template /etc/tempmon
	cp -f ./config.json /etc/tempmon
	cp -f ./TempMon /usr/bin
	cp -f tempmon.service /etc/systemd/system
	systemctl daemon-reload
