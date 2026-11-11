# A2CHAT — Apple IIe/IIc Ollama client (ProDOS 8 + Uthernet II)
# IP65 checkout (https://github.com/cc65/ip65). Defaults to a sibling of
# this repo; override with make IP65=/path/to/ip65.
IP65 ?= $(CURDIR)/../../ip65
override IP65 := $(abspath $(IP65))
CC65 = cl65
CFLAGS = -Or -t apple2enh -C cfg/apple2enh-a2chat.cfg -I$(IP65)/inc -Isrc -DSINGLE_SOCKET
IP65LIB = $(IP65)/ip65/ip65_tcp.lib
A2DRV = $(IP65)/drivers/ip65_apple2_uther2.lib
LOADER = $(shell cl65 --print-target-path)/apple2enh/util/loader.system
CC65LIB = $(shell cl65 --print-target-path)/../lib/apple2enh.lib
CUSTOMLIB = build/apple2enh-a2chat.lib
JAVA ?= $(firstword $(wildcard /opt/homebrew/opt/openjdk/bin/java) java)
AC_JAR ?= $(HOME)/Library/Application Support/AppleCommander/AppleCommander-ac-13.0.jar
# AppleCommander command line; e.g. make AC=applecommander for a native build.
AC ?= $(JAVA) -jar "$(AC_JAR)"
PRODOS_SRC ?= $(IP65)/build/prodos.dsk
DISK = a2chat.po

ifeq ($(filter clean host-test,$(MAKECMDGOALS)),)
ifeq ($(wildcard $(IP65)/inc/ip65.h),)
$(error IP65 not found at $(IP65). Clone https://github.com/cc65/ip65 there or run make IP65=/path/to/ip65)
endif
endif

# cc65 after 2.19 renamed the C stack pointer from sp to c_sp; crt0.s uses
# whichever name this cc65's zeropage.inc exports.
CC65_ASMINC = $(shell cl65 --print-target-path)/../asminc
CRT0FLAGS = $(if $(shell grep -s -w c_sp "$(CC65_ASMINC)/zeropage.inc"),-D CC65_C_SP)

# Both cards are overlays (A2CHAT.UTH, A2CHAT.COG) loaded into the hole
# below $BA00. COGS=1 is accepted and ignored so older commands still build.
# make BUILD_TAG=c5 appends a tag to the build number on the help row.
ifneq ($(BUILD_TAG),)
CFLAGS += '-DA2CHAT_BUILD_TAG="$(BUILD_TAG)"'
endif
VARIANT = nic$(BUILD_TAG)
NIC_HOLE = 0x1780
HD = a2chat.hdv

SRCS = \
	src/main.c \
	src/config.c \
	src/jsonscan.c \
	src/hist.c \
	src/slot.c \
	src/ui.c \
	src/prodos.c \
	src/net.c \
	src/http65.c \
	src/nic.c \
	src/ollama.c \
	src/clock.c

ASM = \
	src/iobuf_bss.s \
	src/auxmem.s \
	src/p8clock.s \
	src/mfclock.s \
	src/eth_buffer.s \
	src/chatmem.s \
	src/basload.s \
	src/close.s \
	src/nic_jump.s

IP65OVR = \
	build/ip65_timer.o \
	build/ip65_outbuf.o

UTH_OBJS = \
	build/uth_vec.o \
	build/nic_uth.o \
	build/w5100.o \
	build/ethernet_a2chat.o \
	build/tcp_stub.o \
	build/ip65_icmp.o \
	build/ip65_error.o

COG_OBJS = \
	build/cog_vec.o \
	build/nic_cog.o \
	build/cogs.o \
	build/cogs_io.o

CA65IP = ca65 -t apple2enh -I$(IP65)/inc

.PHONY: all ip65 host-test disk hd clean FORCE

all: ip65 a2chat.bin A2CHAT.SYSTEM disk hd

ip65:
	$(MAKE) -C $(IP65)/ip65 ip65_tcp.lib
	$(MAKE) -C $(IP65)/drivers ip65_apple2_uther2.lib

# Records the last network variant built, so switching COGS= relinks.
# The sleep makes a changed stamp newer than the last link even when make
# compares whole seconds (GNU make 3.81 on macOS) and the builds run back
# to back.
build/variant: FORCE
	@mkdir -p build
	@echo $(VARIANT) | cmp -s - $@ || { sleep 1; echo $(VARIANT) > $@; }

build/tcp_stub.o: src/tcp_stub.s
	mkdir -p build
	ca65 -t apple2enh -o $@ $<

build/tcp.o: $(IP65)/ip65/tcp.s tools/patch_ip65_tcp.py
	mkdir -p build/ip65-src
	ln -sfn "$(IP65)/inc" build/inc
	python3 tools/patch_ip65_tcp.py "$(IP65)/ip65/tcp.s" build/ip65-src/tcp.s
	ca65 -DTCP -t apple2enh -o $@ build/ip65-src/tcp.s

build/ip65_timer.o: src/ip65_timer.s
	mkdir -p build
	$(CA65IP) -o $@ $<

build/ip65_icmp.o: src/ip65_icmp.s
	mkdir -p build
	$(CA65IP) -o $@ $<

build/ip65_error.o: src/ip65_error.s
	mkdir -p build
	$(CA65IP) -o $@ $<

build/ip65_outbuf.o: src/ip65_outbuf.s
	mkdir -p build
	$(CA65IP) -o $@ $<

build/ethernet_a2chat.o: src/ethernet_a2chat.s
	mkdir -p build
	$(CA65IP) -o $@ $<

build/uth_vec.o: src/uth_vec.s
	mkdir -p build
	ca65 -t apple2enh -o $@ $<

build/cog_vec.o: src/cog_vec.s
	mkdir -p build
	ca65 -t apple2enh -o $@ $<

build/cogs_io.o: src/cogs_io.s
	mkdir -p build
	ca65 -t apple2enh -o $@ $<

build/nic_uth.o: src/nic_uth.c src/a2chat.h src/w5100.h src/nic.h
	mkdir -p build
	$(CC65) -c -o $@ $(CFLAGS) $<

build/nic_cog.o: src/nic_cog.c src/a2chat.h src/cogs.h src/w5100.h src/nic.h
	mkdir -p build
	$(CC65) -c -o $@ $(CFLAGS) $<

build/w5100.o: src/w5100.c src/w5100.h
	mkdir -p build
	$(CC65) -c -o $@ $(CFLAGS) $<

build/cogs.o: src/cogs.c src/cogs.h src/w5100.h src/a2chat.h
	mkdir -p build
	$(CC65) -c -o $@ $(CFLAGS) $<

a2chat.bin A2CHAT.DTK A2CHAT.TOK A2CHAT.UTH A2CHAT.COG: build/variant $(SRCS) src/a2chat.h src/nic.h src/cogs.h src/cogs.c src/cogs_io.s src/w5100.c $(ASM) $(IP65OVR) $(UTH_OBJS) $(COG_OBJS) cfg/apple2enh-a2chat.cfg $(CUSTOMLIB) src/basdetok.s src/bastok.s src/basboot.s tools/mkovl.py tools/mknic.py tools/tokcfg.py tools/bootcfg.py tools/wraptok.py
	$(CC65) -o a2chat.bin $(CFLAGS) --no-target-lib -m a2chat.map -Ln a2chat.lbl build/crt0.o $(IP65OVR) $(SRCS) $(ASM) $(IP65LIB) $(A2DRV) $(CUSTOMLIB)
	python3 tools/mknic.py a2chat.lbl $(NIC_HOLE) A2CHAT.UTH build/uth.map build/uth.cfg -- $(UTH_OBJS) $(IP65LIB) $(A2DRV) $(CC65LIB)
	python3 tools/mknic.py a2chat.lbl $(NIC_HOLE) A2CHAT.COG build/cog.map build/cog.cfg -- $(COG_OBJS) $(CC65LIB)
	mkdir -p build
	python3 tools/mkovl.py a2chat.lbl build/ovlsym.inc build/basovl.cfg
	ca65 -t apple2enh -I build -o build/basdetok.o src/basdetok.s
	ld65 -C build/basovl.cfg -o A2CHAT.DTK build/basdetok.o
	ca65 -t apple2enh -I build -o build/bastok.o src/bastok.s
	ca65 -t apple2enh -I build -o build/basboot.o src/basboot.s
	python3 tools/bootcfg.py a2chat.lbl build/bootovl.cfg
	ld65 -C build/bootovl.cfg build/basboot.o
	python3 tools/tokcfg.py a2chat.lbl build/bastok.o build/tokovl.cfg
	ld65 -C build/tokovl.cfg build/bastok.o
	python3 tools/wraptok.py build/tokboot.bin build/tokcode.bin build/tokro.bin A2CHAT.TOK

$(CUSTOMLIB): src/crt0.s
	mkdir -p build
	cp "$(CC65LIB)" $@
	ca65 -t apple2enh $(CRT0FLAGS) -o build/crt0.o src/crt0.s
	ar65 d $@ crt0.o close.o

A2CHAT.SYSTEM: a2chat.bin
	cp "$(LOADER)" A2CHAT.SYSTEM
	cp cfg/A2CHAT.CFG A2CHAT.CFG
	cp cfg/A2CHAT.TXT A2CHAT.TXT

host-test: tests/test_host
	./tests/test_host

tests/test_host: tests/test_host.c src/jsonscan.c src/config.c src/bas.c src/a2chat.h
	gcc -std=c99 -Wall -Wextra -DA2CHAT_HOST -Isrc -o $@ tests/test_host.c src/jsonscan.c src/config.c src/bas.c

disk: a2chat.po

hd: a2chat.hdv

a2chat.hdv: a2chat.bin A2CHAT.DTK A2CHAT.TOK A2CHAT.UTH A2CHAT.COG A2CHAT.SYSTEM cfg/A2CHAT.CFG cfg/A2CHAT.TXT cfg/A2SOFT.TXT tools/mkprodos8.py tools/prodosboot.py
	$(if $(filter command line environment,$(origin AC)),,@test -f "$(AC_JAR)" || (echo "Missing AppleCommander: $(AC_JAR)"; exit 1))
	@test -f "$(PRODOS_SRC)" || (echo "Missing ProDOS source image: $(PRODOS_SRC)"; exit 1)
	rm -f $@ build/a2chat-pro140.po
	$(AC) -g "$(PRODOS_SRC)" PRODOS build/a2chat-PRODOS.bin
	$(AC) -pro140 build/a2chat-pro140.po A2CHAT
	python3 tools/prodosboot.py "$(PRODOS_SRC)" build/a2chat-pro140.po
	python3 tools/mkprodos8.py build/a2chat-pro140.po $@ A2CHAT
	$(AC) -p $@ PRODOS SYS < build/a2chat-PRODOS.bin
	$(AC) -as $@ A2CHAT < a2chat.bin
	$(AC) -p $@ A2CHAT.SYSTEM SYS < A2CHAT.SYSTEM
	$(AC) -ptx $@ A2CHAT.CFG < cfg/A2CHAT.CFG
	$(AC) -ptx $@ A2CHAT.TXT < cfg/A2CHAT.TXT
	$(AC) -ptx $@ A2SOFT.TXT < cfg/A2SOFT.TXT
	$(AC) -p $@ A2CHAT.DTK BIN < A2CHAT.DTK
	$(AC) -p $@ A2CHAT.TOK BIN < A2CHAT.TOK
	$(AC) -p $@ A2CHAT.UTH BIN < A2CHAT.UTH
	$(AC) -p $@ A2CHAT.COG BIN < A2CHAT.COG
	$(AC) -ll $@
	ls -l $@

a2chat.po: a2chat.bin A2CHAT.DTK A2CHAT.TOK A2CHAT.UTH A2CHAT.COG A2CHAT.SYSTEM cfg/A2CHAT.CFG cfg/A2CHAT.TXT cfg/A2SOFT.TXT tools/prodosboot.py
	$(if $(filter command line environment,$(origin AC)),,@test -f "$(AC_JAR)" || (echo "Missing AppleCommander: $(AC_JAR)"; exit 1))
	@test -f "$(PRODOS_SRC)" || (echo "Missing ProDOS source image: $(PRODOS_SRC)"; exit 1)
	rm -f $@
	$(AC) -g "$(PRODOS_SRC)" PRODOS build/a2chat-PRODOS.bin
	$(AC) -pro140 $@ A2CHAT
	python3 tools/prodosboot.py "$(PRODOS_SRC)" $@
	$(AC) -p $@ PRODOS SYS < build/a2chat-PRODOS.bin
	$(AC) -as $@ A2CHAT < a2chat.bin
	$(AC) -p $@ A2CHAT.SYSTEM SYS < A2CHAT.SYSTEM
	$(AC) -ptx $@ A2CHAT.CFG < cfg/A2CHAT.CFG
	$(AC) -ptx $@ A2CHAT.TXT < cfg/A2CHAT.TXT
	$(AC) -ptx $@ A2SOFT.TXT < cfg/A2SOFT.TXT
	$(AC) -p $@ A2CHAT.DTK BIN < A2CHAT.DTK
	$(AC) -p $@ A2CHAT.TOK BIN < A2CHAT.TOK
	$(AC) -p $@ A2CHAT.UTH BIN < A2CHAT.UTH
	$(AC) -p $@ A2CHAT.COG BIN < A2CHAT.COG
	$(AC) -ll $@

clean:
	rm -f a2chat.bin a2chat.map a2chat.lbl A2CHAT.DTK A2CHAT.TOK A2CHAT.UTH A2CHAT.COG A2CHAT.SYSTEM A2CHAT.CFG A2CHAT.TXT tests/test_host a2chat.po a2chat.hdv
	rm -f src/*.o
	rm -rf build
