# Changelog

## Unreleased

Changes since 1.3 (8 October 2026, build B69).

### Networking

- New CoGS network card build (`make COGS=1`); see [docs/COGS.md](docs/COGS.md). The card does DHCP and DNS, so `HOST=` may be a hostname. Tested on a real CoGS card.
- A W5100 connect to an address that never answers now gives up after about 30 seconds, as 1.3 intended. With 149 retries it took about 15 minutes per try, so startup hung for most of an hour when `HOST=` was unreachable. Ctrl-C still stops it at once.
- New `STREAM=` setting: 1 shows replies word by word, 0 waits for the whole reply. The default is 1 on Uthernet II and 0 on CoGS, whose current firmware drops data on long streamed replies.

### Screen

- UTF-8 and `\uXXXX` escapes in replies are shown as ASCII (curly quotes, dashes, ellipsis, accented letters); anything else becomes `?`.
- `/config` shows each current value and keeps it on Return, and has a prompt for `STREAM`. The title bar redraws after saving and after `/model NAME`.
- The CoGS build reads the time from the card when there is no other clock (`Clock COGS`), and sets the ProDOS date so saved files are dated.

### Build

- `IP65=` defaults to a checkout next to the repo (`../ip65` from the repo root), and make stops with a message when it is missing. `JAVA` falls back to the PATH, and `AC=` accepts a native AppleCommander. Disk-image temp files go in `build/`.
- `crt0.s` builds with cc65 2.18 (`sp`) and newer (`c_sp`).
- `make BUILD_TAG=c5` marks test builds (`B69c5` on the help row).

## 1.3 — 8 October 2026 (build B69)

Changes since 1.2 (27 September 2026, build B64).

### Networking

- DHCP and a static address still use IP65 MACRAW. Once the Apple has an address, the W5100 switches to one hardware TCP socket (8KB transmit, 8KB receive). HTTP no longer runs the software TCP state machine.
- The same image connects on MegaFlash Uthernet II emulation in an Apple IIc, in the gssquared emulator, and on a real Uthernet II in an Apple IIgs.
- The chip mode register is written directly, so indirect mode and auto-increment stay on. OPEN and CONNECT wait until the command register is idle. Each IP and port byte is stored on its own. The source port stays in the dynamic range, and the MAC read at startup is written back before the TCP socket opens.
- With `SLOT=0`, an Apple IIc or IIgs looks in slot 4 first. An Apple IIe still looks in slot 3 first.
- A connect still waits about 30 seconds and retries twice. A good reply prints `Connection established`.

### Screen

- Stream text is stored to the 80-column screen in one assembly pass for each run of letters.
- The JSON reader matches `"content"` and `"eval_count"` instead of walking every envelope byte in C. On a MegaFlash IIc the display keeps up with a 14B model running on an M1 MacBook Pro.
- The help-row clock stays `HH:MM`. It is rewritten when a chat line finishes, and only when that minute has changed.

`/about` shows version 1.3 and the build date. Model selection from the server’s tag list is still not in this release.

## 1.2 — 27 September 2026 (build B64)

Changes since `a2chat_v1.1` (15 September 2026).

### Networking

- IP65 in A2CHAT no longer waits 33 ms on the timer. The receive buffer is smaller, and the TCP window is 900 bytes with the bytes in the right order.
- Port `40114` talks to Olla. Chat and the startup probe use `/olla/ollama/api/chat` and `/olla/ollama/api/tags`. OpenAI `/olla/ollama/v1` is not used.
- If the first TCP connect does not succeed, A2CHAT keeps trying for about 30 seconds, then retries twice. A good HTTP reply prints `Connection established`.

### Screen

- The transcript scrolls. Rows 1–22 are one pane, and about 128 lines of scrollback stay in aux `$0800`. Open-Apple plus up or down pages that ring. Typing returns to the live line. `/new` clears the pane and the ring.
- The help-row clock is `HH:MM`. MegaFlash time is used when that card answers; otherwise the clock is ProDOS hour and minute.
- After a reply the status line is the token count only, for example `31 tokens`.
- `/about` shows version 1.2 and the build date. The startup screen no longer prints `123`.

### Disk and Applesoft

- `/save` of a `basic`, `bas`, or `applesoft` fence asks `Save as BASIC?`. Yes writes a tokenized ProDOS BAS file (type `$FC`, aux type `$0801`) using the Applesoft ROM. Other fences still offer TXT, BAS, BIN, or SYS.
- `/load` of a tokenized BAS file lists it to text with `A2CHAT.DTK` before the listing is sent to the model.
- `A2SOFT.TXT` is the Applesoft reference sent with `A2CHAT.TXT` on every chat.
- `/cat` and `/load` use the current ProDOS prefix. A name without a leading `/` is under `PREFIX`.

Model selection from the server’s tag list is not in this release.
