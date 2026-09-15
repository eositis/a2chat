#!/usr/bin/env python3
"""Create an empty 8MB ProDOS volume (16384 blocks)."""
import struct
import sys

BLOCKS = 16384
BITMAP_BLK = 6
BITMAP_N = (BLOCKS + 4095) // 4096
DIR_START = 2
DIR_N = 4


def mark_used(bitmap, blk):
    byte = blk >> 3
    bit = 7 - (blk & 7)
    bitmap[byte] &= ~(1 << bit)


def dir_block(prev_blk, next_blk):
    b = bytearray(512)
    b[0:2] = struct.pack("<H", prev_blk)
    b[2:4] = struct.pack("<H", next_blk)
    return b


def main():
    if len(sys.argv) != 4:
        sys.stderr.write("usage: mkprodos8.py boot140.po out.hdv VOLNAME\n")
        sys.exit(1)
    boot_path, out_path, volname = sys.argv[1], sys.argv[2], sys.argv[3]
    name = volname.encode("ascii").upper()[:15]
    boot = open(boot_path, "rb").read(1024)
    img = bytearray(BLOCKS * 512)
    img[0:len(boot)] = boot

    blk = dir_block(0, DIR_START + 1)
    vh = bytearray(39)
    vh[0] = 0xF0 | len(name)
    vh[1 : 1 + len(name)] = name
    vh[30] = 0xE3
    vh[31] = 0x27
    vh[32] = 0x0D
    vh[33:35] = struct.pack("<H", 0)
    vh[35:37] = struct.pack("<H", BITMAP_BLK)
    vh[37:39] = struct.pack("<H", BLOCKS)
    blk[4:43] = vh
    img[DIR_START * 512 : (DIR_START + 1) * 512] = blk
    for i in range(1, DIR_N):
        bno = DIR_START + i
        prev_b = bno - 1
        next_b = bno + 1 if i + 1 < DIR_N else 0
        img[bno * 512 : (bno + 1) * 512] = dir_block(prev_b, next_b)

    bitmap = bytearray(BITMAP_N * 512)
    for i in range(len(bitmap)):
        bitmap[i] = 0xFF
    used = DIR_START + DIR_N + BITMAP_N  # 0..9
    for b in range(used):
        mark_used(bitmap, b)
    img[BITMAP_BLK * 512 : (BITMAP_BLK + BITMAP_N) * 512] = bitmap

    open(out_path, "wb").write(img)


if __name__ == "__main__":
    main()
