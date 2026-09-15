# A2CHAT

Native **ProDOS 8** chat client for an enhanced **Apple IIe** or **Apple IIc** with **Uthernet II** (WIZnet W5100). It talks **HTTP directly** to [Ollama](https://ollama.com) on the LAN, streams replies in 80-column text, and can save notes or listings onto a ProDOS volume.

Current release: **version 1.1** (help row **B13**).

**Using it on the Apple:** [USERGUIDE.md](USERGUIDE.md) (hardware, disk install, config, commands, saving files, troubleshooting).

**Performance and measured runs:** [PERFORMANCE.md](PERFORMANCE.md).

**Pitch / one-pagers:** [MARKETING.md](MARKETING.md).

This file is the build and developer overview.

## Hardware

- **IIe:** Uthernet II in any slot. Auto-scan tries **slot 3 first**, then 1–7. Accelerators often move the card off slot 3 — set `SLOT=` in config if probe is wrong.
- **IIc / IIc+:** requires **MegaFlash**, which exposes Uthernet II in **slot 4**. Auto-scan tries 4 first. A stock IIc without MegaFlash cannot run the network path.
- 65C02, 80-column firmware, ProDOS 8. 128K required for aux POST/answer staging.

## Build

Needs [cc65](https://cc65.github.io/) (`cl65` on your PATH) and the local IP65 tree:

```text
IP65=/Users/eositis/Documents/GitHub/ip65
```

```sh
cd a2chat
make
```

That builds IP65’s `ip65_tcp.lib` + `ip65_apple2_uther2.lib` if needed, then `a2chat.bin`, copies cc65 `loader.system` to `A2CHAT.SYSTEM`, and writes **`a2chat.po`** (140K) and **`a2chat.hdv`** (8MB). HTTP uses the same software TCP path as telnet65 (`tcp_connect` / `ip65_process`), not wget65’s W5100 on-chip TCP.

Host-side parser tests (no Apple II required):

```sh
make host-test
```

`make disk` rebuilds only the 140K image. `make hd` rebuilds the 8MB image. Both volumes are `/A2CHAT/` with `PRODOS`, `A2CHAT.SYSTEM` (cc65 loader), `A2CHAT` (BIN at `$0803`), sample `A2CHAT.CFG`, and sample `A2CHAT.TXT`. With no `BASIC.SYSTEM`, ProDOS starts `A2CHAT.SYSTEM` on boot. Restore your real `HOST` / `MODEL` / `PREFIX` (and your edited `A2CHAT.TXT`) after each image rebuild.

Manual AppleCommander layout (wget65-style):

- `A2CHAT.SYSTEM` — SYS loader
- `A2CHAT` — BIN (`-as` AppleSingle from `a2chat.bin`)
- `A2CHAT.CFG` — TXT
- `A2CHAT.TXT` — TXT (system prompt)

## Ollama

Ollama must listen on the LAN, not only localhost:

```sh
export OLLAMA_HOST=0.0.0.0:11434
```

then restart Ollama. `HOST=` in `A2CHAT.CFG` is the Mac/PC **dotted IPv4** (v1 has no DNS unless AppleWin’s virtual W5100 DNS offload is present). Pull a model, e.g. `ollama pull llama3.2:3b`.

## Config (`A2CHAT.CFG`)

| Key | Meaning |
|-----|---------|
| `HOST` | Ollama IPv4 |
| `PORT` | default 11434 |
| `MODEL` | model name |
| `SLOT` | `0` = auto; `4` MegaFlash IIc; `3` typical IIe |
| `IP` / `GATEWAY` / `NETMASK` | empty = DHCP (IP65); filled = static |
| `PREFIX` | data directory for `A2CHAT.LOG` and default for `/cat` (e.g. `/A2.DESKTOP/A2CHAT`). Empty = folder the program was launched from. |
| `MAXHIST` | bytes of log spliced into each POST |
| `MAXREAD` / `MAXWRITE` | file I/O caps |
| `HISTCAP` | compact `A2CHAT.LOG` when larger |

Chat POST is staged in 32K aux RAM, not a scratch BOD file. Disk is still slow for `/save` and `/read` of large files.

## Emulators

- **AppleWin** emulates Uthernet II (used by IP65 wget65).
- **Virtual ][** on macOS can attach Uthernet II.

Use a static `IP=` on the same subnet as the emulator NIC if DHCP is awkward.

## License notes

Link against your local IP65 tree rather than a random snapshot. `src/w5100.c` / `w5100.h` are unused copies of Oliver Schmidt’s wget65 HAL (kept for reference).
