# Changelog

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
