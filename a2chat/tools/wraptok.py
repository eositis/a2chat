#!/usr/bin/env python3
"""Join the tokenizer bootstrap, code, and rodata into A2CHAT.TOK.

The first $03C0 bytes are loaded by the main program. The next four
bytes are the code length and the rodata length.
"""
import struct
import sys

boot_path, code_path, ro_path, out_path = sys.argv[1:5]
boot = open(boot_path, "rb").read()
code = open(code_path, "rb").read()
ro = open(ro_path, "rb").read()
if len(boot) > 0x3C0:
    sys.exit("bootstrap is %d bytes, limit 960" % len(boot))
boot = boot + bytes(0x3C0 - len(boot))
with open(out_path, "wb") as f:
    f.write(boot)
    f.write(struct.pack("<HH", len(code), len(ro)))
    f.write(code)
    f.write(ro)
