#!/usr/bin/env python3
"""Linker config for the first block of A2CHAT.TOK."""
import sys

lbl_path, cfg_path = sys.argv[1:3]
eth = None
for line in open(lbl_path):
    parts = line.split()
    if len(parts) >= 3 and parts[0] == "al":
        name = parts[2][1:] if parts[2].startswith(".") else parts[2]
        if name == "eth_inp":
            eth = int(parts[1], 16)
if eth is None:
    sys.exit("eth_inp missing from label file")

with open(cfg_path, "w") as f:
    f.write(
        "MEMORY {\n"
        "    BOOT: start = $%04X, size = $03C0, file = \"build/tokboot.bin\";\n"
        "    RES:  start = $0300, size = $00D0;\n"
        "}\n"
        "SEGMENTS {\n"
        "    CODE:     load = BOOT, type = ro;\n"
        "    RESIDENT: load = BOOT, run = RES, type = ro, define = yes;\n"
        "}\n" % eth
    )
