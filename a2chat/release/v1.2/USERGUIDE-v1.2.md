# A2CHAT user guide

A2CHAT is a ProDOS 8 chat client for an enhanced Apple IIe or Apple IIc. It talks HTTP to [Ollama](https://ollama.com) on your LAN, streams the reply in 80-column text, and can save notes or BASIC listings onto a ProDOS volume.

Version **1.2**. The help row shows the binary build on the far right (currently **B64**).

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

To go through [Olla](https://thushan.github.io/olla/) instead, run Olla on the LAN (`0.0.0.0:40114`) and set `PORT=40114`. A2CHAT then POSTs `/olla/ollama/api/chat` and GETs `/olla/ollama/api/tags` (Ollama JSON, not OpenAI `/olla/ollama/v1`).

## Install the disk

Copy `a2chat.po` (140K) or `a2chat.hdv` (8MB) onto a floppy, SmartPort image, or emulator drive. Both are volume `/A2CHAT/`.

| File | Role |
|------|------|
| `PRODOS` | Boot |
| `A2CHAT.SYSTEM` | cc65 loader (ProDOS starts this if there is no `BASIC.SYSTEM`) |
| `A2CHAT` | The program (BIN at `$0803`) |
| `A2CHAT.CFG` | Settings (text) |
| `A2CHAT.TXT` | Short system prompt (text, editable) |
| `A2SOFT.TXT` | Applesoft reference sent with every chat |
| `A2CHAT.DTK` | Lists a tokenized BAS file when you `/load` it |

**If you rebuilt with `make` / `make disk` / `make hd`:** the image always gets the *sample* `cfg/A2CHAT.CFG`. After copying the new image to the Apple, restore your real `HOST`, `MODEL`, `PREFIX`, and static `IP` if you use them.

Edit `A2CHAT.CFG` with any ProDOS text editor, or from inside A2CHAT with `/config` (host, port, model, slot only — PREFIX and IP are still edited in the file).

`A2CHAT.TXT` and `A2SOFT.TXT` are sent as the system prompt on every chat. `A2CHAT.TXT` is the short instruction. `A2SOFT.TXT` is the Applesoft reference (line numbers, no `ELSE` or `WHILE`, and a single ```basic fence around a program). Edit either file next to the program. Changes apply on the next message. If `A2CHAT.TXT` is missing, A2CHAT uses a short built-in default and still sends `A2SOFT.TXT` when that file is present.

## First boot

1. Boot the volume. You should get 80-column A2CHAT, not Applesoft.
2. The top inverse bar shows host, port, model, slot, and `Conn` or `----`. After a reply it shows the token count (for example `31 tokens`).
3. Startup prints the config path, the data directory, **Clock P8** (or `MFMS` / `JIFFY`), wall time, the Apple IP, and an Ollama probe.
4. **Connection established** means HTTP reached Ollama. If the first connect does not finish in about 30 seconds, A2CHAT retries twice.
5. Type a question and press Return, or type a slash command from the help row.

If Ethernet fails, A2CHAT continues **offline**. `/cat`, `/config`, `/about`, `/quit`, and similar still work; chat will not.

## Screen

| Rows | What |
|------|------|
| 0 | Status: host, model, connection; after a reply, the token count (`31 tokens`) |
| 1–22 | Scrolling transcript (`You` / `AI`); type on the `You` line |
| 23 | Inverse help: commands on the left; **`HH:MM` and `B64` on the far right** |

`P8` is the ProDOS clock, not a second build number. Only the **B** series is shown on the help row.

Type after the inverse `You` label. After Return, the next line is the `AI` reply. When the pane fills, text **scrolls up**; it does not wrap back to the top. About 128 lines (~6 screens) stay in aux RAM (`$0800`). Open-Apple + up/down (or the arrow keys) pages through that scrollback while idle; any typed character returns to the live prompt. `/new` clears the pane and the RAM ring. History in `A2CHAT.LOG` is unchanged.

Open-Apple + `.` (or the IP65 abort key) can interrupt a transfer.

## Config (`A2CHAT.CFG`)

Lines are `KEY=value`. Unknown keys are ignored.

| Key | Meaning |
|-----|---------|
| `HOST` | Ollama IPv4 (required) |
| `PORT` | `11434` for Ollama, or `40114` for Olla |
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
| `/cat` | | List PREFIX, or the current ProDOS directory if PREFIX is empty |
| `/cat PATH` | | List that directory. `/DEVTOOLS/A2CHAT` is a full path; `CAL2` is a name in the current directory |
| `/load NAME` | | Send `NAME` with the next query (prompts if the query is not on this line). A tokenized BAS file is listed to text first (`A2CHAT.LST`) and that listing is what the model sees. |
| `/load NAME query` | | Same, using `query` (up to 250 characters) |
| `/save NAME` | | Write the last reply to `NAME` under PREFIX. A ```basic fence asks `Save as BASIC?`. Y writes a tokenized BAS file. N, or any other fence, asks `1 TXT  2 BAS  3 BIN  4 SYS`. Do this before the next prompt. |
| `/model NAME` | | Set and save the model; with no name, print the current model |
| `/new` | | Start a fresh chat (clears the log used as history) |
| `/about` | `/a` | Clear the chat pane; print version, author, date, and GitHub URL |
| `/quit` | `/q` | ProDOS QUIT (80-column off, ProDOS left mapped). Should not drop into the monitor. |
| `/help` | | Print the command list (any unknown `/` command does too) |

A path that starts with `/` is a full ProDOS path (`/DEVTOOLS/A2CHAT/CAL2`). Any other name is under `PREFIX` when `PREFIX` is set, otherwise under the directory you launched from. You cannot overwrite `PRODOS`, `A2CHAT.SYSTEM`, `A2CHAT.CFG`, or `A2CHAT`.

## Chatting

Type a line that does not start with `/` and press Return. A prompt can be up to 250 characters. Status shows `POST /api/chat` (or `/olla/ollama/api/chat` on port 40114) then streams the reply.

The POST is staged in aux RAM (32K). **B21** copies the last `MAXHIST` bytes of `A2CHAT.LOG` into aux and **closes the file before TCP**, so follow-up turns should not need `/new`. `/new` still wipes the log if you want a blank conversation.

There is **no live function-calling** to the disk from the model. Extra tool JSON made the HTTP send miss TCP ACKs (`send body Timeout`). Chat is plain messages only.

## Files

`/load NAME` uploads that file with your query. The same attach works at the end of a prompt (`fix line 30 /load CALENDAR`). A full path works too (`/load /DEVTOOLS/A2CHAT/CAL2`). A tokenized BAS program is listed to text in `A2CHAT.LST` (the lister is `A2CHAT.DTK` on the program disk) and that listing is what the model sees. `/save` of a BASIC file tokenizes with the Applesoft ROM in the main program. A BAS file that is already a text listing is sent as-is. Use the ProDOS name (`CAL2`, not a partial word). `/cat` prints that name with its type.

`/save NAME` writes the last reply into PREFIX. If the reply's first fence is tagged `basic`, `bas`, or `applesoft`, A2CHAT asks `Save as BASIC?`. Y tokenizes the lines inside the fence and writes a ProDOS BAS file (type `$FC`, aux `$0801`) that `LOAD` can run. N shows the type list. A fence with any other tag, or no fence, goes straight to `1 TXT  2 BAS  3 BIN  4 SYS`. Choosing BAS from that list tokenizes the same way. TXT writes the fence text with return characters. The opening tag and any text before or after the fence are left out. A reply with no fence is saved whole. Run `/save` before the next prompt; the next send reuses the reply buffer.

**ProDOS names** (A2CHAT enforces these):

- 1–15 characters
- Must start with A–Z
- Then A–Z, 0–9, and **at most one period**
- No spaces (spaces are stripped; `The Meaning of Life.md` becomes something like `THEMEANINGO.MD`)
- A leading `/` keeps the volume and directory (`/DEVTOOLS/A2CHAT/CAL2`)

## Files A2CHAT creates

Under `PREFIX` (or the launch directory):

| File | Role |
|------|------|
| `A2CHAT.CFG` | Settings (also next to the program if PREFIX is empty) |
| `A2CHAT.TXT` | Short system prompt (next to the program) |
| `A2SOFT.TXT` | Applesoft reference, sent with `A2CHAT.TXT` |
| `A2CHAT.LOG` | Timestamped text history (`>YOU` / `>AI` plus clock), ProDOS **TXT** so editors can open it. Spliced into later POSTs. |
| `/save` file | Last reply, with the ProDOS type you pick |

There is no `A2CHAT.BOD` scratch file. Chat POST JSON lives in aux RAM.

Disk is slow. Short chats should finish in a few seconds of Apple-side work plus however long the model takes; `/read` or `/save` of a large file can take much longer.

## Performance and timing

POST body and the streamed answer use **32K of aux RAM** (`$4000–$BFFF`). After each reply the top bar shows Ollama `eval_count` as `31 tokens`.

Wall clock on the help row is MegaFlash time when that card answers (`MFMS`). Otherwise it is ProDOS hour and minute (`P8`), shown as `HH:MM`. Clock-card IRQs are left masked.

Numbers from the B7–B13 bring-up (same LAN, `llama3.2:3b`) are in [PERFORMANCE.md](PERFORMANCE.md).

## Troubleshooting

| Symptom | What to try |
|---------|-------------|
| `TCP connect failed` | Ollama not on `0.0.0.0:11434`, or Olla not on `:40114`; firewall; Apple and host not on the same subnet; `HOST=` is a name instead of IPv4. A2CHAT waits about 30 seconds and retries twice before this message. |
| `----` in the status bar | Ethernet init failed — slot, DHCP, or Uthernet II not seen |
| `send body Timeout` / `send aux Timeout` | Shorter prompt and `/new` to shrink history; do not ask the model to call tools |
| `cannot open` on `/cat` or `/load` | `/cat` with no name lists the current directory. Use the full path from that listing, for example `/load /DEVTOOLS/A2CHAT/CAL2` |
| `/save` says no reply | The next prompt already reused the reply buffer. Drop before sending again. |
| Illegal filename | Use letters/digits and one period; A2CHAT strips spaces |
| `/quit` into the monitor | Rebuild B12 or later (QUIT must not `BIT $C082`) |
| Chat works, CFG “wrong” after copy | `make disk` / `make hd` overwrote `A2CHAT.CFG` with the sample |
| Model greets instead of answering | `/new` and retry on B11+; the current prompt is sent in the JSON even if the log is odd |

## Emulators

AppleWin and Virtual ][ can emulate Uthernet II. Prefer a static `IP=` / `GATEWAY=` / `NETMASK=` on the same subnet as the virtual NIC if DHCP is unreliable.
