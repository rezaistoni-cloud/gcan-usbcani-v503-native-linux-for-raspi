CC ?= gcc
PKG_CONFIG ?= pkg-config
BUILD_DIR := build
SRC_DIR := src

CFLAGS ?= -Wall -Wextra -O2 -g
LIBUSB_CFLAGS := $(shell $(PKG_CONFIG) --cflags libusb-1.0 2>/dev/null)
LIBUSB_LIBS := $(shell $(PKG_CONFIG) --libs libusb-1.0 2>/dev/null)

ifeq ($(strip $(LIBUSB_LIBS)),)
LIBUSB_LIBS := -lusb-1.0
endif

.PHONY: all clean install

all: $(BUILD_DIR)/gcan-native-tool

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/gcan-native-tool: $(SRC_DIR)/gcan_native_tool.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LIBUSB_CFLAGS) $< -o $@ $(LIBUSB_LIBS)

clean:
	rm -rf $(BUILD_DIR)

install: all
	install -m 0755 $(BUILD_DIR)/gcan-native-tool /usr/local/bin/gcan-native-tool
