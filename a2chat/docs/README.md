# A2CHAT

Native **ProDOS 8** chat client for an enhanced **Apple IIe** or **Apple IIc** with an **Uthernet II** (WIZnet W5100) or **CoGS** network card. It talks **HTTP directly** to [Ollama](https://ollama.com) on the LAN, streams replies in 80-column text, and can save notes or listings onto a ProDOS volume.

**Using it on the Apple:** [USERGUIDE.md](USERGUIDE.md) (hardware, disk install, config, commands, saving files, troubleshooting).

**CoGS card:** [COGS.md](COGS.md) (setup, differences from Uthernet II, streaming caveat).

**Performance and measured runs:** [PERFORMANCE.md](PERFORMANCE.md).

**Pitch / one-pagers:** [MARKETING.md](MARKETING.md).

**Planned work:** [TODO.md](TODO.md).

This file is the build and developer overview.

## Hardware

One binary drives either card. At startup A2CHAT looks for a CoGS card first (ID register `$C0n5 = $A2`, Disk II slots skipped); if it finds one it loads `A2CHAT.COG`, otherwise it loads `A2CHAT.UTH` and probes for a W5100. Both drivers are overlays loaded below `$BA00`, so both files must be on the disk.

**Uthernet II:**

- **IIe:** Uthernet II in any slot. Auto-scan tries **slot 3 first**, then 1–7. Accelerators often move the card off slot 3 — set `SLOT=` in config if probe is wrong.
- **IIc / IIc+:** requires **MegaFlash**, which exposes Uthernet II in **slot 4**. Auto-scan tries 4 first. A stock IIc without MegaFlash cannot run the network path.

**CoGS:**

- Any slot of a IIe or IIgs; the scan goes from slot 7 down. `SLOT=` overrides it.
- The card does DHCP, DNS and TCP, so `IP=` / `GATEWAY=` / `NETMASK=` are ignored and `HOST=` may be a hostname. Set addressing and the timezone on the card's own config screen.
- With no ProDOS or MegaFlash clock, the time comes from the card.
- Replies default to `STREAM=0` until the card's flow-control firmware fix is out; see [COGS.md](COGS.md).

**Both:** 65C02, 80-column firmware, ProDOS 8. 128K required for aux POST/answer staging.

## Build

Needs [cc65](https://cc65.github.io/) (`cl65` on your PATH; 2.19 or a later snapshot) and an [IP65](https://github.com/cc65/ip65) checkout. The Makefile looks for it next to this repo (`../ip65` from the repo root); point it elsewhere with:

```sh
make IP65=/path/to/ip65
```

The disk images also need Java and [AppleCommander](https://applecommander.github.io/) (`JAVA=`, `AC_JAR=` override the defaults).

```sh
cd a2chat
make
```

That builds IP65’s `ip65_tcp.lib` + `ip65_apple2_uther2.lib` if needed, then `a2chat.bin`, copies cc65 `loader.system` to `A2CHAT.SYSTEM`, and writes **`a2chat.po`** (140K) and **`a2chat.hdv`** (8MB). DHCP and a static address still use IP65 MACRAW. After the address is set, HTTP uses the W5100’s hardware TCP socket. The IP65 TCP engine is not linked. Timer, ICMP, and the Ethernet buffers are **A2CHAT objects** linked ahead of those libs; the IP65 tree is not patched.

Host-side parser tests (no Apple II required):

```sh
make host-test
```

`make disk` rebuilds only the 140K image. `make hd` rebuilds the 8MB image. Both volumes are `/A2CHAT/` with `PRODOS`, `A2CHAT.SYSTEM` (cc65 loader), `A2CHAT` (BIN at `$0803`), the network drivers `A2CHAT.UTH` and `A2CHAT.COG`, sample `A2CHAT.CFG`, `A2CHAT.TXT`, and `A2SOFT.TXT`. With no `BASIC.SYSTEM`, ProDOS starts `A2CHAT.SYSTEM` on boot. Restore your real `HOST` / `MODEL` / `PREFIX` (and your edited prompt files) after each image rebuild.

Manual AppleCommander layout (wget65-style):

- `A2CHAT.SYSTEM` — SYS loader
- `A2CHAT` — BIN (`-as` AppleSingle from `a2chat.bin`)
- `A2CHAT.UTH` — BIN (Uthernet II driver overlay)
- `A2CHAT.COG` — BIN (CoGS driver overlay)
- `A2CHAT.CFG` — TXT
- `A2CHAT.TXT` — TXT (short system prompt)
- `A2SOFT.TXT` — TXT (Applesoft reference)

## Ollama

Ollama must listen on the LAN, not only localhost:

```sh
export OLLAMA_HOST=0.0.0.0:11434
```

then restart Ollama. With an Uthernet II, `HOST=` in `A2CHAT.CFG` is the Mac/PC **dotted IPv4** (no DNS unless AppleWin’s virtual W5100 DNS offload is present). With a CoGS card it may also be a hostname. Pull a model, e.g. `ollama pull llama3.2:3b`.

## Olla (`PORT=40114`)

[Olla](https://thushan.github.io/olla/) proxies Ollama (default listen **40114**). A2CHAT stays on the **native Ollama** JSON it already streams, under Olla’s prefix:

| | Direct Ollama | Through Olla |
|--|--|--|
| Port | `11434` | `40114` |
| Chat | `POST /api/chat` | `POST /olla/ollama/api/chat` |
| Tags | `GET /api/tags` | `GET /olla/ollama/api/tags` |

Set `PORT=40114` (or `/config`) and keep `HOST=` as the machine running Olla. OpenAI-compatible `/olla/ollama/v1` is **not** used; that path expects a different request body than A2CHAT builds. Bind Olla on the LAN (`0.0.0.0:40114`), not only localhost.

## Config (`A2CHAT.CFG`)

| Key | Meaning |
|-----|---------|
| `HOST` | server IPv4; with CoGS, also a hostname (31 chars max) |
| `PORT` | `11434` Ollama, or `40114` Olla (`/olla/ollama/api/...`) |
| `MODEL` | model name |
| `SLOT` | `0` = auto (a CoGS card is looked for in slots 7–1 first; then for Uthernet II, IIc and IIgs try slot 4, IIe tries slot 3); or a slot number 1–7 |
| `IP` / `GATEWAY` / `NETMASK` | Uthernet II only: empty = DHCP (IP65); filled = static. CoGS ignores them |
| `PREFIX` | data directory for `A2CHAT.LOG` and default for `/cat` (e.g. `/A2.DESKTOP/A2CHAT`). Empty = folder the program was launched from. |
| `MAXHIST` | bytes of log spliced into each POST |
| `MAXREAD` / `MAXWRITE` | file I/O caps |
| `HISTCAP` | compact `A2CHAT.LOG` when larger |
| `STREAM` | 1 = show replies word by word, 0 = wait for the whole reply. Unset: 1 on Uthernet II, 0 on CoGS |

Chat POST is staged in 32K aux RAM, not a scratch BOD file. Disk is still slow for `/save` and `/read` of large files.

## Emulators

- **AppleWin** emulates Uthernet II (used by IP65 wget65).
- **Virtual ][** on macOS can attach Uthernet II.

Use a static `IP=` on the same subnet as the emulator NIC if DHCP is awkward.

## License notes

Link against your local IP65 tree rather than a random snapshot. `src/w5100.c` is the hardware TCP driver used after DHCP. `src/tcp_stub.s` replaces the IP65 TCP engine so the binary still fits.
