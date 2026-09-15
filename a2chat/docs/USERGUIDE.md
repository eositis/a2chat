# A2CHAT user guide

A2CHAT is a ProDOS 8 chat client for an enhanced Apple IIe or Apple IIc. It talks HTTP to [Ollama](https://ollama.com) on your LAN, streams the reply in 80-column text, and can save notes or BASIC listings onto a ProDOS volume.

Version **1.1**. The help row shows the binary build on the far right (currently **B13**).

This guide is for using the program on the Apple. For building from source, see [README.md](README.md). For memory layout and measured timings, see [PERFORMANCE.md](PERFORMANCE.md).

## What you need

**Apple**

- Enhanced IIe or IIc / IIc+ with 65C02 and 80-column firmware
- 128K RAM (aux memory holds the HTTP body and the streamed answer)
- Uthernet II (WIZnet W5100)
  - **IIe:** card in any slot (auto-scan tries slot 3 first). If an accelerator moved the card, set `SLOT=` in config.
  - **IIc / IIc+:** MegaFlash exposing Uthernet II in **slot 4**. A stock IIc without MegaFlash cannot network.
- ProDOS 8 on the boot volume

**Ollama host (Mac/PC)**

- Ollama listening on the LAN, not only localhost
- A pulled model whose name matches `MODEL=` in `A2CHAT.CFG` (for example `llama3.2:3b`)
- The Apple and the Ollama machine on the same IPv4 subnet. A2CHAT has **no DNS** — `HOST=` must be a dotted address such as `192.168.0.111`.

On the Ollama machine:

```sh
export OLLAMA_HOST=0.0.0.0:11434
```

Then restart Ollama and pull a model, for example `ollama pull llama3.2:3b`.

## Install the disk

Copy `a2chat.po` (140K) or `a2chat.hdv` (8MB) onto a floppy, SmartPort image, or emulator drive. Both are volume `/A2CHAT/`.

| File | Role |
|------|------|
| `PRODOS` | Boot |
| `A2CHAT.SYSTEM` | cc65 loader (ProDOS starts this if there is no `BASIC.SYSTEM`) |
| `A2CHAT` | The program (BIN at `$0803`) |
| `A2CHAT.CFG` | Settings (text) |

**If you rebuilt with `make` / `make disk` / `make hd`:** the image always gets the *sample* `cfg/A2CHAT.CFG`. After copying the new image to the Apple, restore your real `HOST`, `MODEL`, `PREFIX`, and static `IP` if you use them.

Edit `A2CHAT.CFG` with any ProDOS text editor, or from inside A2CHAT with `/config` (host, port, model, slot only — PREFIX and IP are still edited in the file).

## First boot

1. Boot the volume. You should get 80-column A2CHAT, not Applesoft.
2. The top inverse bar shows host, port, model, slot, and `Conn` or `----`. After a reply it shows elapsed seconds, token count, and tokens/sec (for example `3s 16t 5/s`).
3. Startup prints the config path, the data directory, **Clock P8** (or `MFMS` / `JIFFY`), wall time, the Apple IP, and an Ollama probe.
4. **Probe OK** means HTTP reached Ollama. If it also says to pull the model, `MODEL=` does not match a tag on that server.
5. Type a question and press Return, or type a slash command from the help row.

If Ethernet fails, A2CHAT continues **offline**. `/cat`, `/config`, `/about`, `/quit`, and similar still work; chat will not.

## Screen

| Rows | What |
|------|------|
| 0 | Status: host, model, connection; after a reply, `Ns Nt N/s` |
| 1–20 | Chat (You / AI) |
| 21–22 | Prompt |
| 23 | Inverse help: commands on the left; **`HH:MM:SS` and `B13` on the far right** |

`P8` is the ProDOS clock, not a second build number. Only the **B** series is shown on the help row.

The first non-slash prompt, and `/new`, clear the chat pane. History stays in `A2CHAT.LOG` until you `/new`.

Open-Apple + `.` (or the IP65 abort key) can interrupt a transfer.

## Config (`A2CHAT.CFG`)

Lines are `KEY=value`. Unknown keys are ignored.

| Key | Meaning |
|-----|---------|
| `HOST` | Ollama IPv4 (required) |
| `PORT` | Usually `11434` |
| `MODEL` | Exact Ollama tag |
| `SLOT` | `0` = auto; `4` typical MegaFlash IIc; `3` typical IIe |
| `IP` / `GATEWAY` / `NETMASK` | Empty = DHCP. Fill all three for a static Apple address (recommended on some emulators). |
| `PREFIX` | Data folder for work files and default `/cat` (example `/A2.DESKTOP/A2CHAT`). Empty = the folder you launched from. |
| `MAXHIST` | Bytes of `A2CHAT.LOG` sent with each chat POST |
| `MAXREAD` / `MAXWRITE` | Size caps for file tools |
| `HISTCAP` | Compact the log when it grows past this |

`/config` saves host, port, model, and slot into `A2CHAT.CFG`. If you change slot or IP, quit and run A2CHAT again so Ethernet re-inits.

Use a `PREFIX` on a larger volume when you want notes and logs off the 140K program disk. Create that directory in ProDOS first.

## Slash commands

| Command | Also | Action |
|---------|------|--------|
| `/config` | `/c` | Edit host, port, model, slot; save CFG |
| `/ping` | `/p` | Re-probe Ollama |
| `/cat` | | List the PREFIX directory (or `.` if PREFIX is empty) |
| `/cat NAME` | | List or show a path under PREFIX (name is sanitized) |
| `/read PATH` | `/r` | Read a file and send it to the model as context |
| `/save PATH` | `/s` | Copy `A2CHAT.LOG` to that name under PREFIX |
| `/model NAME` | | Set and save the model; with no name, print the current model |
| `/new` | | Start a fresh chat (clears the log used as history) |
| `/about` | `/a` | Clear the chat pane; print version, author, date, and GitHub URL |
| `/quit` | `/q` | ProDOS QUIT (80-column off, ProDOS left mapped). Should not drop into the monitor. |
| `/help` | | Print the command list (any unknown `/` command does too) |

Paths you type are turned into **ProDOS leaf names** under `PREFIX`. You cannot overwrite `PRODOS`, `A2CHAT.SYSTEM`, `A2CHAT.CFG`, or `A2CHAT`.

## Chatting

Type a line that does not start with `/` and press Return. Status shows `POST /api/chat ...` then streams the reply.

Keep prompts reasonably short. The POST is staged in aux RAM (32K). A long `A2CHAT.LOG` (`MAXHIST`) plus a long prompt can fail or time out.

There is **no live function-calling** to the disk from the model. Extra tool JSON made the HTTP send miss TCP ACKs (`send body Timeout`). Chat is plain messages only.

## Saving a reply to disk

**Ask in the prompt** to save, write, or put it on disk, and mention `.md` or `.bas` if you care about type. After the stream, A2CHAT writes:

- `NOTE.MD` (TXT) if the prompt looked like a markdown/save/write/disk request
- `PROG.BAS` (BAS) if the prompt contained `.bas`

`.BAS` files written this way are **text listings** with ProDOS type BAS. They are not tokenized Applesoft `LOAD` files.

The model may also wrap a file as:

```text
<<A2W NAME TYP>>
...contents...
<<A2E>>
```

`TYP` is `TXT`, `BAS`, or `BIN`. `NAME` is sanitized to a legal ProDOS leaf. Small models often ignore this wrapper; the `NOTE.MD` / `PROG.BAS` fallback still runs when your prompt asked to save.

Text files **inside PREFIX** are written without a Y/N prompt. BIN/SYS files, and writes outside the workspace, still ask **Y / N / E** (edit path).

**ProDOS names** (A2CHAT enforces these):

- 1–15 characters
- Must start with A–Z
- Then A–Z, 0–9, and **at most one period**
- No spaces (spaces are stripped; `The Meaning of Life.md` becomes something like `THEMEANINGO.MD`)

If the sanitized name would be the PREFIX folder itself (for example `A2CHAT`), A2CHAT uses `NOTE.MD` instead of treating the directory as a file.

## Files A2CHAT creates

Under `PREFIX` (or the launch directory):

| File | Role |
|------|------|
| `A2CHAT.CFG` | Settings (also next to the program if PREFIX is empty) |
| `A2CHAT.LOG` | Timestamped text history (`>YOU` / `>AI` plus clock). Spliced into later POSTs; `/save` copies it. |
| `NOTE.MD` / `PROG.BAS` | Default saved replies |

There is no `A2CHAT.BOD` scratch file. Chat POST JSON lives in aux RAM.

Disk is slow. Short chats should finish in a few seconds of Apple-side work plus however long the model takes; `/read` or `/save` of a large file can take much longer.

## Performance and timing

POST body and the streamed answer use **32K of aux RAM** (`$4000–$BFFF`). After each reply the top bar shows elapsed seconds, Ollama `eval_count`, and tokens/sec, for example `11s 89t 8/s`.

Wall clock on the help row is ProDOS time when available (`P8` at boot). MegaFlash can supply millisecond elapsed time (`MFMS`). Clock-card IRQs are left masked.

Numbers from the B7–B13 bring-up (same LAN, `llama3.2:3b`) are in [PERFORMANCE.md](PERFORMANCE.md).

## Troubleshooting

| Symptom | What to try |
|---------|-------------|
| `Probe FAIL` | Ollama not on `0.0.0.0:11434`; firewall; Apple and host not on the same subnet; `HOST=` is a name instead of IPv4 |
| `----` in the status bar | Ethernet init failed — slot, DHCP, or Uthernet II not seen |
| `Pull the model or fix MODEL=` | `ollama list` on the host; set `MODEL=` to an exact tag |
| `send body Timeout` / `send aux Timeout` | Shorter prompt and `/new` to shrink history; do not ask the model to call tools |
| `cannot open` on `/cat` | PREFIX directory missing, or `make disk` restored an empty PREFIX — recreate the folder and set `PREFIX=` |
| Save does nothing | Prompt must contain `save`, `write`, `.md`, `.bas`, or `disk` (unless the model emitted `<<A2W>>`) |
| Illegal filename | Use letters/digits and one period; A2CHAT strips spaces |
| `/quit` into the monitor | Rebuild B12 or later (QUIT must not `BIT $C082`) |
| Chat works, CFG “wrong” after copy | `make disk` / `make hd` overwrote `A2CHAT.CFG` with the sample |
| Model greets instead of answering | `/new` and retry on B11+; the current prompt is sent in the JSON even if the log is odd |

## Emulators

AppleWin and Virtual ][ can emulate Uthernet II. Prefer a static `IP=` / `GATEWAY=` / `NETMASK=` on the same subnet as the virtual NIC if DHCP is unreliable.
