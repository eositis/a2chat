# A2CHAT — marketing summary

**A2CHAT** is a native ProDOS 8 chat client for the Apple IIe and IIc. It talks HTTP over Uthernet II to [Ollama](https://ollama.com) on your LAN, streams the model’s reply in 80-column text, and can write notes or BASIC listings back to a ProDOS disk.

No browser. No cloud account. The Apple is the terminal; your Mac or PC only runs the model.

---

## One-liner

Chat with a local LLM from an enhanced Apple IIe or IIc — 80 columns, ProDOS, Ethernet.

## Elevator pitch (≈40 words)

A2CHAT boots like any other ProDOS program. You type a prompt on the Apple. Ollama on the LAN answers, character by character, on the 80-column screen. Ask it to save, and A2CHAT writes a legal ProDOS file — markdown notes or a BASIC listing — under your data prefix.

## Short paragraph (listings, social, README blurb)

A2CHAT is a ProDOS 8 Ollama client for enhanced Apple IIe and IIc systems with Uthernet II. It streams LAN chat in 80-column text, keeps a compact history log, and can drop replies onto disk as `NOTE.MD` or `PROG.BAS`. Built with cc65 and IP65 software TCP. Your model stays on your network.

---

## Who it is for

- Apple II collectors who already have (or want) Ethernet on the machine
- Retro-computing demos: “the IIe is talking to a 3B model in the next room”
- People who run Ollama locally and like the idea of a *period* UI, not a terminal emulator pretending to be one
- IIc / IIc+ owners with MegaFlash (Uthernet II in slot 4)

It is not a general internet browser, not a ChatGPT website wrapper, and not a replacement for a modern IDE.

---

## What you can show in thirty seconds

1. Boot `/A2CHAT/` — inverse status bar, 80-column chrome, slash-command help row (time and build on the right).
2. `/ping` — Apple IP, gateway, **Probe OK**.
3. Type a question — tokens appear as they arrive.
4. Ask it to save as markdown — `Wrote /YOUR/PREFIX/NOTE.MD`.
5. `/cat` — the new file is on the volume. `/quit` returns to ProDOS, not the monitor.
6. `/about` — version, author, date, GitHub URL.

---

## Product points

| | |
|---|---|
| **Native** | ProDOS 8 SYS/BIN, 65C02, 80-column firmware. Not a cross-compiled Linux binary in an emulator-only world. |
| **Local AI** | HTTP to Ollama (`HOST=` dotted IPv4, typically port 11434). Model tags such as `llama3.2:3b`. |
| **Streaming UI** | 80×24 layout: status, 20-row chat, prompt, always-on command bar. |
| **Disk citizen** | `PREFIX` workspace, `/cat` `/read` `/save`, ProDOS 15-character names (no spaces), TXT and BAS writes. |
| **Honest hardware** | Uthernet II (W5100). IIe any slot; IIc needs MegaFlash. 128K required (aux RAM stages the POST). |
| **Yours to build** | cc65 + IP65. `make disk` produces a bootable 140K `.po`; `make hd` an 8MB `.hdv`. |

---

## Taglines (pick one)

- *The IIe still has something to say.*
- *Local models. Period hardware.*
- *Ollama, in inverse video.*
- *80 columns. One Return. A reply from the LAN.*
- *ProDOS 8 meets a 3-billion-parameter neighbor.*

---

## Requirements (fine print for a landing page)

Enhanced IIe or IIc/IIc+ · 80-column firmware · 128K RAM · Uthernet II · ProDOS 8 · Ollama on the same IPv4 subnet (`OLLAMA_HOST=0.0.0.0:11434`) · no DNS in v1.

---

## Positioning vs. “just SSH from the Mac”

The point is not throughput. A modern laptop will always chat faster. The point is that the **Apple is the computer in the conversation**: keyboard, screen, disk, and Ethernet card from that era, talking to a model you run yourself. A2CHAT is a demonstration that a 1 MHz 8-bit machine can still be a first-class client on a local AI LAN.

---

## Assets to pair with this copy

- Photo or capture: 80-column chrome (`A2CHAT  192.168.0.111:11434  llama3.2:3b  Slot4  [Conn]`)
- Boot volume listing: `PRODOS`, `A2CHAT.SYSTEM`, `A2CHAT`, `A2CHAT.CFG`
- One saved `NOTE.MD` in a catalog listing
- Hardware: IIe + Uthernet II, or IIc + MegaFlash

Operator details: [USERGUIDE.md](USERGUIDE.md). Build notes: [README.md](README.md). Measured runs: [PERFORMANCE.md](PERFORMANCE.md).
