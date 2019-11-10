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
	chmod a+x /usr/bin/TempMon
	cp -f tempmon.service /lib/systemd/system
	systemctl daemon-reload

uninstall:
	systemctl stop tempmon.service
	systemctl disable tempmon.service
	rm -f /lib/systemd/system/tempmon.service
	systemctl daemon-reload
	rm -fr /etc/tempmon
	rm -f /usr/bin/TempMon
