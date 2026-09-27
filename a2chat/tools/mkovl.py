#!/usr/bin/env python3
"""Write overlay equates and a linker config that runs at eth_inp."""
import sys

lbl_path, inc_path, cfg_path = sys.argv[1:4]
want = [
    "ptr1", "ptr2", "ptr3", "ptr4",
    "tmp1", "tmp2", "tmp3", "tmp4",
    "pushax", "COUT", "eth_outp", "eth_inp",
    "_fopen", "_fclose", "_fread", "_fgetc", "_fputc",
    "_p8_set_txt", "__filetype", "__auxtype",
    "_tok_src", "_tok_end", "wbmode", "fslot",
]
labs = {}
for line in open(lbl_path):
    parts = line.split()
    if len(parts) >= 3 and parts[0] == "al":
        name = parts[2][1:] if parts[2].startswith(".") else parts[2]
        labs[name] = int(parts[1], 16)

missing = [w for w in want if w not in labs]
if missing:
    sys.stderr.write("missing labels: %s\n" % ", ".join(missing))
    sys.exit(1)

with open(inc_path, "w") as f:
    f.write("; generated from a2chat.lbl — do not edit\n")
    for w in want:
        f.write("%s = $%04X\n" % (w, labs[w]))

org = labs["eth_inp"]
with open(cfg_path, "w") as f:
    f.write(
        "MEMORY {\n"
        "    OV: start = $%04X, size = $03C0, file = %%O;\n"
        "}\n"
        "SEGMENTS {\n"
        "    CODE:   load = OV, type = ro;\n"
        "    RODATA: load = OV, type = ro;\n"
        "    BSS:    load = OV, type = bss;\n"
        "}\n" % org
    )
