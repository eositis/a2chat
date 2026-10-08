# A2CHAT — Apple IIe/IIc Ollama client (ProDOS 8 + Uthernet II)
IP65 ?= /Users/eositis/Documents/GitHub/ip65
CC65 = cl65
CFLAGS = -Or -t apple2enh -C cfg/apple2enh-a2chat.cfg -I$(IP65)/inc -Isrc -DSINGLE_SOCKET
IP65LIB = $(IP65)/ip65/ip65_tcp.lib
A2DRV = $(IP65)/drivers/ip65_apple2_uther2.lib
LOADER = $(shell cl65 --print-target-path)/apple2enh/util/loader.system
CC65LIB = $(shell cl65 --print-target-path)/../lib/apple2enh.lib
CUSTOMLIB = build/apple2enh-a2chat.lib
JAVA ?= /opt/homebrew/opt/openjdk/bin/java
AC_JAR ?= $(HOME)/Library/Application Support/AppleCommander/AppleCommander-ac-13.0.jar
PRODOS_SRC ?= $(IP65)/build/prodos.dsk
DISK = a2chat.po
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
	src/w5100.c \
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
	src/close.s

IP65OVR = \
	build/tcp_stub.o \
	build/ip65_timer.o \
	build/ip65_icmp.o \
	build/ip65_error.o \
	build/ip65_outbuf.o \
	build/ethernet_a2chat.o

CA65IP = ca65 -t apple2enh -I$(IP65)/inc

.PHONY: all ip65 host-test disk hd clean

all: ip65 a2chat.bin A2CHAT.SYSTEM disk hd

ip65:
	$(MAKE) -C $(IP65)/ip65 ip65_tcp.lib
	$(MAKE) -C $(IP65)/drivers ip65_apple2_uther2.lib

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

a2chat.bin A2CHAT.DTK A2CHAT.TOK: $(SRCS) src/a2chat.h $(ASM) $(IP65OVR) cfg/apple2enh-a2chat.cfg $(CUSTOMLIB) src/basdetok.s src/bastok.s src/basboot.s tools/mkovl.py tools/tokcfg.py tools/bootcfg.py tools/wraptok.py
	$(CC65) -o $@ $(CFLAGS) --no-target-lib -m a2chat.map -Ln a2chat.lbl build/crt0.o $(IP65OVR) $(SRCS) $(ASM) $(IP65LIB) $(A2DRV) $(CUSTOMLIB)
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
	ca65 -t apple2enh -o build/crt0.o src/crt0.s
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

a2chat.hdv: a2chat.bin A2CHAT.DTK A2CHAT.TOK A2CHAT.SYSTEM cfg/A2CHAT.CFG cfg/A2CHAT.TXT cfg/A2SOFT.TXT tools/mkprodos8.py tools/prodosboot.py
	@test -f "$(AC_JAR)" || (echo "Missing AppleCommander: $(AC_JAR)"; exit 1)
	@test -f "$(PRODOS_SRC)" || (echo "Missing ProDOS source image: $(PRODOS_SRC)"; exit 1)
	rm -f $@ /tmp/a2chat-pro140.po
	$(JAVA) -jar "$(AC_JAR)" -g "$(PRODOS_SRC)" PRODOS /tmp/a2chat-PRODOS.bin
	$(JAVA) -jar "$(AC_JAR)" -pro140 /tmp/a2chat-pro140.po A2CHAT
	python3 tools/prodosboot.py "$(PRODOS_SRC)" /tmp/a2chat-pro140.po
	python3 tools/mkprodos8.py /tmp/a2chat-pro140.po $@ A2CHAT
	$(JAVA) -jar "$(AC_JAR)" -p $@ PRODOS SYS < /tmp/a2chat-PRODOS.bin
	$(JAVA) -jar "$(AC_JAR)" -as $@ A2CHAT < a2chat.bin
	$(JAVA) -jar "$(AC_JAR)" -p $@ A2CHAT.SYSTEM SYS < A2CHAT.SYSTEM
	$(JAVA) -jar "$(AC_JAR)" -ptx $@ A2CHAT.CFG < cfg/A2CHAT.CFG
	$(JAVA) -jar "$(AC_JAR)" -ptx $@ A2CHAT.TXT < cfg/A2CHAT.TXT
	$(JAVA) -jar "$(AC_JAR)" -ptx $@ A2SOFT.TXT < cfg/A2SOFT.TXT
	$(JAVA) -jar "$(AC_JAR)" -p $@ A2CHAT.DTK BIN < A2CHAT.DTK
	$(JAVA) -jar "$(AC_JAR)" -p $@ A2CHAT.TOK BIN < A2CHAT.TOK
	$(JAVA) -jar "$(AC_JAR)" -ll $@
	ls -l $@

a2chat.po: a2chat.bin A2CHAT.DTK A2CHAT.TOK A2CHAT.SYSTEM cfg/A2CHAT.CFG cfg/A2CHAT.TXT cfg/A2SOFT.TXT tools/prodosboot.py
	@test -f "$(AC_JAR)" || (echo "Missing AppleCommander: $(AC_JAR)"; exit 1)
	@test -f "$(PRODOS_SRC)" || (echo "Missing ProDOS source image: $(PRODOS_SRC)"; exit 1)
	rm -f $@
	$(JAVA) -jar "$(AC_JAR)" -g "$(PRODOS_SRC)" PRODOS /tmp/a2chat-PRODOS.bin
	$(JAVA) -jar "$(AC_JAR)" -pro140 $@ A2CHAT
	python3 tools/prodosboot.py "$(PRODOS_SRC)" $@
	$(JAVA) -jar "$(AC_JAR)" -p $@ PRODOS SYS < /tmp/a2chat-PRODOS.bin
	$(JAVA) -jar "$(AC_JAR)" -as $@ A2CHAT < a2chat.bin
	$(JAVA) -jar "$(AC_JAR)" -p $@ A2CHAT.SYSTEM SYS < A2CHAT.SYSTEM
	$(JAVA) -jar "$(AC_JAR)" -ptx $@ A2CHAT.CFG < cfg/A2CHAT.CFG
	$(JAVA) -jar "$(AC_JAR)" -ptx $@ A2CHAT.TXT < cfg/A2CHAT.TXT
	$(JAVA) -jar "$(AC_JAR)" -ptx $@ A2SOFT.TXT < cfg/A2SOFT.TXT
	$(JAVA) -jar "$(AC_JAR)" -p $@ A2CHAT.DTK BIN < A2CHAT.DTK
	$(JAVA) -jar "$(AC_JAR)" -p $@ A2CHAT.TOK BIN < A2CHAT.TOK
	$(JAVA) -jar "$(AC_JAR)" -ll $@

clean:
	rm -f a2chat.bin a2chat.map a2chat.lbl A2CHAT.DTK A2CHAT.TOK A2CHAT.SYSTEM A2CHAT.CFG A2CHAT.TXT tests/test_host a2chat.po a2chat.hdv
	rm -f src/*.o
	rm -rf build
