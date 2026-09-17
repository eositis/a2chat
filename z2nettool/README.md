# A2netTool

40-column ProDOS network utilities for the Apple II, built on the [IP65](https://github.com/cc65/ip65) TCP/IP stack.

## Requirements

- 64K Apple IIe (or II+ with language card). ProDOS 8 will not run in 48K.
- Ethernet card supported by IP65: Uthernet, Uthernet II, or LANceGS. The program scans slots 4, 3, 7, 2, 1, 5, then 6 (Disk II last). Slots with firmware ROM are still probed so a MegaFlash card can host storage (`$C0s0–$C0s3`) and the NIC (`$C0s4–$C0s7`) together — typically slot 4.
- Build host: [cc65](https://cc65.github.io/), Make, Java, [AppleCommander](https://applecommander.github.io/) `ac` jar.

The linked image uses the IP65 combo driver plus TCP stack. BSS ends near `$8822` with ProDOS HIMEM `$9600` and a 2K cc65 stack, so 64K is the minimum. Each tool keeps 32 lines of 40-column output.

## Build

Default IP65 tree: `/Users/eositis/Documents/github/ip65`

```sh
make
```

Produces `build/a2nettool.bin` and `build/a2nettool.dsk`. Boot the disk in ProDOS and run `A2NET.SYSTEM`.

```sh
make IP65_DIR=/path/to/ip65 AC=/path/to/AppleCommander-ac.jar
```

## Keys

| Key | Action |
|-----|--------|
| Open-Apple + P/T/N/W/C/M | Ping, traceroute, nslookup, whois, nc, nmap |
| Open-Apple + Q | Menu |
| Open-Apple + X, or 7 / X / Q on the menu | Quit to ProDOS |
| Ctrl-X | Quit to ProDOS |
| Ctrl-P/T/N/W/L/F/Q | Same tools on a II+ (L = nc, F = nmap) |
| 1-6 or letters | Select from the menu |
| Tab, up/down | Move between fields; down from last field focuses output |
| Up/down in output | Scroll that tool's history |
| Return | Run the current tool (appends to its output) |
| Esc | Abort a run, or return to the menu when idle |
| Left/right, Delete | Edit the focused field |

Host, port, count, timeout, and similar fields are shared when you switch tools. Each tool keeps its own output until you quit.

## Tools

BusyBox-inspired, not full clones. DNS is IPv4 A records only. nmap is a TCP connect probe (max 32 ports per run). nc is a client (no listen mode). traceroute sends ICMP echo with increasing TTL.
