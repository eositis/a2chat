#!/usr/bin/env python3
"""Copy a real ProDOS boot (blocks 0 and 1) onto a 140K image.

AppleCommander -pro140 fills block 0 with a splash screen
("APPLECOMMANDER CREATED THIS DISK" / "INSERT ANOTHER DISK").
A machine that boots that image never loads PRODOS, so the
volume looks blank or unformatted.

The source image may be DOS 3.3 sector order (.dsk) or ProDOS
block order (.po). Only the first two blocks are replaced.
"""
import sys

# DOS 3.3 logical sector -> ProDOS block-order sector, within a track.
DOS_TO_PO = [0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15]
BLOCKS = 280
BLOCK = 512


def dos_to_po(src):
    out = bytearray(BLOCKS * BLOCK)
    for block in range(BLOCKS):
        track = block // 8
        for half in (0, 1):
            sec = DOS_TO_PO[(block % 8) * 2 + half]
            src_off = (track * 16 + sec) * 256
            dst = block * BLOCK + half * 256
            out[dst : dst + 256] = src[src_off : src_off + 256]
    return out


def is_po(img):
    # Volume header at block 2: storage $F, entry length $27.
    if len(img) < 3 * BLOCK:
        return False
    hdr = img[2 * BLOCK + 4]
    entry_len = img[2 * BLOCK + 4 + 0x1F]
    return (hdr & 0xF0) == 0xF0 and entry_len == 0x27


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: prodosboot.py source.dsk dest.po\n")
        sys.exit(1)
    src_path, dst_path = sys.argv[1], sys.argv[2]
    src = open(src_path, "rb").read()
    if len(src) != BLOCKS * BLOCK:
        sys.stderr.write("source is not a 140K image\n")
        sys.exit(1)
    if not is_po(src):
        src = dos_to_po(src)
        if not is_po(src):
            sys.stderr.write("source has no ProDOS volume directory\n")
            sys.exit(1)
    boot = src[: 2 * BLOCK]
    plain = bytes(b & 0x7F for b in boot)
    if b"PRODOS" not in plain or b"UNABLE TO LOAD PRODOS" not in plain:
        sys.stderr.write("source boot block is not a ProDOS loader\n")
        sys.exit(1)
    with open(dst_path, "r+b") as f:
        dest = f.read(2 * BLOCK)
        if len(dest) < 2 * BLOCK:
            sys.stderr.write("dest is shorter than two blocks\n")
            sys.exit(1)
        f.seek(0)
        f.write(boot)


if __name__ == "__main__":
    main()
