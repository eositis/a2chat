# A2netTool — Apple II ProDOS network utilities on IP65

IP65_DIR ?= /Users/eositis/Documents/github/ip65
CC65_TARGET_PATH := $(shell cl65 --print-target-path)
JAVA ?= /opt/homebrew/opt/openjdk/bin/java
AC ?= $(HOME)/Library/Application Support/AppleCommander/AppleCommander-ac.jar
PRODOS_DSK ?= $(IP65_DIR)/build/prodos.dsk
LOADER = $(CC65_TARGET_PATH)/apple2/util/loader.system

IP65LIB = $(IP65_DIR)/ip65/ip65_tcp.lib
A2LIB   = $(IP65_DIR)/drivers/ip65_apple2.lib

SRC = \
	src/main.c \
	src/ui.c \
	src/keys.c \
	src/netutil.c \
	src/ping.c \
	src/traceroute.c \
	src/nslookup.c \
	src/whois.c \
	src/nc.c \
	src/nmap.c \
	src/icmp_ttl.s

CFLAGS = -Or -t apple2 -I $(IP65_DIR)/inc -I src --asm-include-dir $(IP65_DIR)/inc

BUILD = build
BIN = $(BUILD)/a2nettool.bin
MAP = $(BUILD)/a2nettool.map
DSK = $(BUILD)/a2nettool.dsk

.PHONY: all bin dsk libs clean

all: dsk

$(IP65LIB):
	$(MAKE) -C $(IP65_DIR)/ip65 ip65_tcp.lib

$(A2LIB):
	$(MAKE) -C $(IP65_DIR)/drivers ip65_apple2.lib

libs: $(IP65LIB) $(A2LIB)

$(BUILD):
	mkdir -p $(BUILD)

$(BIN): $(SRC) src/a2net.h $(IP65LIB) $(A2LIB) | $(BUILD)
	cl65 $(CFLAGS) -o $@ -m $(MAP) -vm $(SRC) $(IP65LIB) $(A2LIB)

bin: $(BIN)

dsk: $(BIN)
	cp $(PRODOS_DSK) $(DSK)
	$(JAVA) -jar "$(AC)" -d $(DSK) a2net 2>/dev/null || true
	$(JAVA) -jar "$(AC)" -d $(DSK) a2net.system 2>/dev/null || true
	$(JAVA) -jar "$(AC)" -as $(DSK) a2net < $(BIN)
	$(JAVA) -jar "$(AC)" -p $(DSK) a2net.system sys < $(LOADER)
	$(JAVA) -jar "$(AC)" -n $(DSK) A2NETTOOL

clean:
	rm -rf $(BUILD) src/*.o
