# A2CHAT — Apple IIe/IIc Ollama client (ProDOS 8 + Uthernet II)
IP65 ?= /Users/eositis/Documents/GitHub/ip65
CC65 = cl65
CFLAGS = -Or -t apple2enh -C cfg/apple2enh-a2chat.cfg -I$(IP65)/inc -Isrc
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
	src/ollama.c \
	src/clock.c

.PHONY: all ip65 host-test disk hd clean

all: ip65 a2chat.bin A2CHAT.SYSTEM disk hd

ip65:
	$(MAKE) -C $(IP65)/ip65 ip65_tcp.lib
	$(MAKE) -C $(IP65)/drivers ip65_apple2_uther2.lib

a2chat.bin: $(SRCS) src/a2chat.h src/iobuf_bss.s src/auxmem.s src/p8clock.s src/mfclock.s src/eth_buffer.s cfg/apple2enh-a2chat.cfg $(CUSTOMLIB)
	$(CC65) -o $@ $(CFLAGS) --no-target-lib -m a2chat.map build/crt0.o $(SRCS) src/iobuf_bss.s src/auxmem.s src/p8clock.s src/mfclock.s src/eth_buffer.s $(IP65LIB) $(A2DRV) $(CUSTOMLIB)

$(CUSTOMLIB): src/crt0.s
	mkdir -p build
	cp "$(CC65LIB)" $@
	ca65 -t apple2enh -o build/crt0.o src/crt0.s
	ar65 d $@ crt0.o

A2CHAT.SYSTEM: a2chat.bin
	cp "$(LOADER)" A2CHAT.SYSTEM
	cp cfg/A2CHAT.CFG A2CHAT.CFG

host-test: tests/test_host
	./tests/test_host

tests/test_host: tests/test_host.c src/jsonscan.c src/config.c src/bas.c src/a2chat.h
	gcc -std=c99 -Wall -Wextra -DA2CHAT_HOST -Isrc -o $@ tests/test_host.c src/jsonscan.c src/config.c src/bas.c

disk: a2chat.po

hd: a2chat.hdv

a2chat.hdv: a2chat.bin A2CHAT.SYSTEM cfg/A2CHAT.CFG tools/mkprodos8.py
	@test -f "$(AC_JAR)" || (echo "Missing AppleCommander: $(AC_JAR)"; exit 1)
	@test -f "$(PRODOS_SRC)" || (echo "Missing ProDOS source image: $(PRODOS_SRC)"; exit 1)
	rm -f $@ /tmp/a2chat-pro140.po
	$(JAVA) -jar "$(AC_JAR)" -g "$(PRODOS_SRC)" PRODOS /tmp/a2chat-PRODOS.bin
	$(JAVA) -jar "$(AC_JAR)" -pro140 /tmp/a2chat-pro140.po A2CHAT
	python3 tools/mkprodos8.py /tmp/a2chat-pro140.po $@ A2CHAT
	$(JAVA) -jar "$(AC_JAR)" -p $@ PRODOS SYS < /tmp/a2chat-PRODOS.bin
	$(JAVA) -jar "$(AC_JAR)" -as $@ A2CHAT < a2chat.bin
	$(JAVA) -jar "$(AC_JAR)" -p $@ A2CHAT.SYSTEM SYS < A2CHAT.SYSTEM
	$(JAVA) -jar "$(AC_JAR)" -ptx $@ A2CHAT.CFG < cfg/A2CHAT.CFG
	$(JAVA) -jar "$(AC_JAR)" -ll $@
	ls -l $@

a2chat.po: a2chat.bin A2CHAT.SYSTEM cfg/A2CHAT.CFG
	@test -f "$(AC_JAR)" || (echo "Missing AppleCommander: $(AC_JAR)"; exit 1)
	@test -f "$(PRODOS_SRC)" || (echo "Missing ProDOS source image: $(PRODOS_SRC)"; exit 1)
	rm -f $@
	$(JAVA) -jar "$(AC_JAR)" -g "$(PRODOS_SRC)" PRODOS /tmp/a2chat-PRODOS.bin
	$(JAVA) -jar "$(AC_JAR)" -pro140 $@ A2CHAT
	$(JAVA) -jar "$(AC_JAR)" -p $@ PRODOS SYS < /tmp/a2chat-PRODOS.bin
	$(JAVA) -jar "$(AC_JAR)" -as $@ A2CHAT < a2chat.bin
	$(JAVA) -jar "$(AC_JAR)" -p $@ A2CHAT.SYSTEM SYS < A2CHAT.SYSTEM
	$(JAVA) -jar "$(AC_JAR)" -ptx $@ A2CHAT.CFG < cfg/A2CHAT.CFG
	$(JAVA) -jar "$(AC_JAR)" -ll $@

clean:
	rm -f a2chat.bin a2chat.map A2CHAT.SYSTEM A2CHAT.CFG tests/test_host a2chat.po a2chat.hdv
	rm -f src/*.o
	rm -rf build
