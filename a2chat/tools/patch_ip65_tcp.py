#!/usr/bin/env python3
"""Copy IP65 tcp.s and set the advertised window to 900. Does not edit the IP65 tree."""
import sys

if len(sys.argv) != 3:
    sys.exit("usage: patch_ip65_tcp.py src.s dst.s")

src, dst = sys.argv[1], sys.argv[2]
with open(src, "r", encoding="utf-8") as f:
    text = f.read()
# Stock is ldax #$b405 so A=$05 X=$b4 → on-wire 0x05b4 (1460).
# 900 = 0x0384 needs A=$03 X=$84 → ldax #$8403, not #$0384 (that advertised 0x8403).
old = "ldax #$b405                   ; $05b4 (1460) in network byte order"
new = "ldax #$8403                   ; $0384 (900) in network byte order"
if old not in text:
    sys.exit("patch_ip65_tcp: window constant ldax #$b405 not found")
with open(dst, "w", encoding="utf-8") as f:
    f.write(text.replace(old, new, 1))
