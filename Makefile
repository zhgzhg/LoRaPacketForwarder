# Project: LoRaPktFwrd

# Compiler / tools
CXX       := g++
CC        := gcc
AR        := ar
RM        := rm -f

# Libraries / flags
LIBS      := -lwiringPi -lm -lpthread -lrt -lcrypt
CPPFLAGS  :=
CXXFLAGS  := -std=c++14 -Wall -DLINUX -DARDUINO=999 -DRADIOLIB_FIX_ERRATA_SX127X
CFLAGS    := -Wall
LDFLAGS   :=

# Build type (release|debug)
BUILD ?= release
ifeq ($(BUILD),debug)
  CXXFLAGS += -Og -g3 -DRADIOLIB_DEBUG
else
  CXXFLAGS += -O3
endif

# Include directories
INCDIRS = \
  RadioLib/src \
  RadioLib/src/modules \
  RadioLib/src/protocols \
  RadioLib/src/modules/RF69 \
  RadioLib/src/modules/SX127x \
  RadioLib/src/modules/SX126x \
  RadioLib/src/modules/LLCC68 \
  RadioLib/src/linux-workarounds \
  RadioLib/src/linux-workarounds/SPI \
  smtUdpPacketForwarder/base64 \
  smtUdpPacketForwarder/gpsTimestampUtils \
  .

CPPFLAGS += $(addprefix -I,$(INCDIRS))

# Version (preserve original intent but simplified)
# Produces forms like: v1.2.3, v1.2.3-<commit>, next-<commit>, <commit>-dirty, etc.
TAG_COMMIT := $(shell git rev-list --abbrev-commit --tags --max-count=1 2>/dev/null || true)
TAG        := $(shell git describe --abbrev=0 --tags $(TAG_COMMIT) 2>/dev/null || true)
COMMIT     := $(shell git rev-parse --short HEAD 2>/dev/null || true)
VERSION    := $(TAG:v%=%)

ifneq ($(COMMIT),$(TAG_COMMIT))
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
ifneq ($(shell git status --porcelain 2>/dev/null || true),)
  VERSION := $(VERSION)-dirty
endif

CPPFLAGS += -DGIT_VER=\"$(VERSION)\"

# Detect wiringPi SPI wirintPiSPISetupMode() extension
WPI_HEADER      := $(shell find /usr/include /usr/local/include -name wiringPiSPI.h 2>/dev/null | head -n1)
WIREPI_HAS_OPI  := $(shell grep -q -E  'int\s+wiringPiSPISetupMode\s*\(\s*int\s+\w+\s*,\s*int\s+\w+\s*,\s*int\s+\w+\s*,\s*int\s+\w+\s*\)\s*;' $(WPI_HEADER) 2>/dev/null && echo 1 || echo 0)
ifeq ($(WIREPI_HAS_OPI),1)
  CXXFLAGS += -DHAVE_WIRINGPI_SETUPMODE_OPI
endif

# Source discovery
SRC_CPP := \
  $(wildcard RadioLib/src/linux-workarounds/SPI/*.cpp) \
  $(wildcard RadioLib/src/linux-workarounds/*.cpp) \
  $(wildcard RadioLib/src/protocols/PhysicalLayer/*.cpp) \
  $(wildcard RadioLib/src/modules/RF69/*.cpp) \
  $(wildcard RadioLib/src/modules/SX127x/*.cpp) \
  $(wildcard RadioLib/src/modules/SX126x/*.cpp) \
  $(wildcard RadioLib/src/modules/LLCC68/*.cpp) \
  $(wildcard RadioLib/src/*.cpp) \
  $(wildcard smtUdpPacketForwarder/gpsTimestampUtils/*.cpp) \
  $(wildcard smtUdpPacketForwarder/*.cpp) \
  $(wildcard *.cpp)

SRC_C := \
  $(wildcard smtUdpPacketForwarder/base64/*.c)

SOURCES := $(SRC_CPP) $(SRC_C)

# Build directory
OBJDIR   := build
# Map sources to object paths (mirror directory tree under build/)
OBJECTS  := $(patsubst %.cpp,$(OBJDIR)/%.o,$(filter %.cpp,$(SOURCES))) \
            $(patsubst %.c,$(OBJDIR)/%.o,$(filter %.c,$(SOURCES)))

DEPS     := $(OBJECTS:.o=.d)

TARGET   := LoRaPktFwrd

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

# C++ compile
$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# C compile
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# Debug convenience target (same as: make BUILD=debug)
debug:
	@$(MAKE) BUILD=debug

# Cleaning
clean:
	$(RM) $(TARGET)
	$(RM) -r $(OBJDIR)

distclean: clean

# Install / uninstall
PREFIX ?= /usr
SYSCONFDIR ?= /etc
UNITDIR ?= /lib/systemd/system
PKT_DIR := $(SYSCONFDIR)/LoRaPacketForwarder

install: $(TARGET)
	mkdir -p $(PKT_DIR)
	[ -f config.json.template ] && cp -f config.json.template $(PKT_DIR) || true
	[ -f config.json ] && cp -f config.json $(PKT_DIR) || true
	install -m 0755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	[ -f LoRaPktFwrd.service ] && cp -f LoRaPktFwrd.service $(UNITDIR) && systemctl daemon-reload || true

uninstall:
	systemctl stop LoRaPktFwrd.service 2>/dev/null || true
	systemctl disable LoRaPktFwrd.service 2>/dev/null || true
	rm -f $(UNITDIR)/LoRaPktFwrd.service
	systemctl daemon-reload || true
	rm -rf $(PKT_DIR)
	rm -f $(PREFIX)/bin/$(TARGET)

getsvclogs:
	journalctl -n 100 -f -u LoRaPktFwrd.service

# Include auto-generated dependency files
-include $(DEPS)

.PHONY: all debug clean distclean install uninstall getsvclogs
