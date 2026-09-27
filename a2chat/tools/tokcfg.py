#!/usr/bin/env python3
"""Linker config for A2CHAT.TOK.

Code is loaded at eth_inp and must end before ProDOS mliparam ($B9EC)
and cc65 errno ($B9E4). The keyword-table copy and the line stubs are
RODATA at $BA00, which is free until the BAS file is opened.
"""
import re
import subprocess
import sys

lbl_path, obj_path, cfg_path = sys.argv[1:4]
eth = None
for line in open(lbl_path):
    parts = line.split()
    if len(parts) >= 3 and parts[0] == "al":
        name = parts[2][1:] if parts[2].startswith(".") else parts[2]
        if name == "eth_inp":
            eth = int(parts[1], 16)
if eth is None:
    sys.exit("eth_inp missing from label file")

text = subprocess.check_output(["od65", "-S", obj_path], text=True)
sizes = {}
for name, val in re.findall(r"(CODE|RODATA|BSS):\s+(\d+)", text):
    sizes[name] = int(val)
code = sizes.get("CODE", 0)
ro = sizes.get("RODATA", 0)
bss = sizes.get("BSS", 0)
code_end = eth + code
bss_end = code_end + bss
if bss_end > 0xB9EC:
    sys.exit("tokenizer hits mliparam at $%04X (bss ends $%04X)" % (0xB9EC, bss_end))
if ro > 0xF0 or 0xBA00 + ro > 0xBB00:
    sys.exit("tokenizer rodata hits the keyword table ($%04X)" % (0xBA00 + ro))

with open(cfg_path, "w") as f:
    f.write(
        "MEMORY {\n"
        "    CMEM: start = $%04X, size = $%04X, file = \"build/tokcode.bin\";\n"
        "    BMEM: start = $%04X, size = $%04X, file = \"\";\n"
        "    RMEM: start = $BA00, size = $%04X, file = \"build/tokro.bin\";\n"
        "}\n"
        "SEGMENTS {\n"
        "    CODE:   load = CMEM, type = ro;\n"
        "    BSS:    load = BMEM, type = bss;\n"
        "    RODATA: load = RMEM, type = ro;\n"
        "}\n" % (eth, code, code_end, bss if bss else 1, ro if ro else 1)
    )
