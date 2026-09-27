# Changelog

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
