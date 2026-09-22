# A2CHAT performance and measurement

Status as of **version 1.1 / build 25** (September 2026). B7–B13 bring-up numbers below were recorded on Virtual ][ (“A2 Desktop w net”), Apple IIe enhanced, 128K, 80-column, Uthernet II slot 4, Ollama `llama3.2:3b` at `192.168.0.111:11434`. Build 24 advertised TCP window bytes in the wrong order (`ldax #$0384` → on-wire 0x8403); the peer could send frames larger than `eth_inp` (1024), which the W5100 driver then skipped. That showed up as **`recv timeout`** after a successful connect/send (DHCP/`10.0.2.x` NAT was a red herring). Build 25 uses `ldax #$8403` (900).

The original plan (aux 32K, packet receive, 65C02 content loop, clock + tok/s) is **done**. IP65 work stays **in this tree**: do not edit the GitHub ip65 repo. What follows is layout, what shipped, and B7–B13 runs — not the planned 16K POST / `A2CHAT.BOD` / NSC-IRQ design.

## Constraints that must not regress

- Language card image at `$D400`, size `$0C00`. Do not put C or Ethernet buffers in LC. This machine cannot store at `$D400` (`LC bank2 map fail`); hist and aux copy live in **main**.
- Do not use `$D000` (ProDOS LC bank 1).
- Do not `BIT $C080` / `$C082` while ProDOS must stay mapped. `/quit` must not page ROM over LC. Local `timer_read` must not call Monitor WAIT or those switches (stock IP65 `a2_timer.s` did).
- No ProDOS `fread` during live TCP. POST JSON is staged in aux `$4000–$BFFF`.
- No Ollama tools JSON on the live POST (that caused `send body Timeout`).
- **NBUFS=1**: FILEIO `$BA00` is also `eth_outp`. Only one `fopen` at a time; files closed before `tcp_send`.
- RAMRD remaps `$0200–$BFFF`. Aux-read stub runs in zero page `$50` and **saves/restores** those bytes around the copy (otherwise `tcp_send` times out). Soft switches are **write-only: `STA`, not `BIT`**.
- Custom `crt0`: no BLTU2, no `initlib`, `setdos` sets `__dos_type`. `self_path()` is leaf names only.
- BSS must end below FILEIO (`$BA00`). Build 24 map: BSS `$A980–$B6E4` (`$0D65`), FILEIO `$BA00–$BDFF`, C stack `$BE00–$BF00` (~795 bytes BSS slack).
- Software TCP only. Do not call `w5100_config()`. TCP window **900** is stored like stock IP65 (`ldax #$8403` → 0x0384 on the wire); `eth_inp` is 1024.

## Memory as it is (build 24)

Linker (`a2chat.map`): CODE `$086A–$9971` (`$9108`), RODATA `$0C5F`, DATA `$0315`, BSS `$0D65`. Program file **41,677** bytes, load `$0803`.

```text
Main $0000–$BFFF
  $0803     STARTUP + CODE + RODATA + DATA + INIT
  $A980     BSS (IP65 + app), ONCE overlay at start of BSS
  $BA00     FILEIO 1K = ProDOS iobuf = eth_outp (TX)
  $BE00     cc65 C stack ($0100)
  $BF00     ProDOS HIMEM / global page

Language card
  $D000     ProDOS bank 1 (do not occupy)
  $D400     reserved in the cfg ($0C00); not used for C/eth on this box

Aux $0000–$BFFF
  $0400–$07FF   80-column text
  $0800–$2FFF   128-line scrollback (80-col rows)
  $4000–$BFFF   32K POST JSON + streamed answer  (A2CHAT_AUX_POST_MAX = $8000)
```

Bank switching is **RAMRD/RAMWRT** only. Do not use ALTZP / aux LC.

## IP65 local overrides (build 24)

Linked **before** `ip65_tcp.lib` / `ip65_apple2_uther2.lib` so stock modules are not pulled:

- `ip65_timer.s` — MegaFlash `CMD_GETTIMER_MS` on IIc+MF, else `$C019` VBL (~16 ms/edge). No 33 ms WAIT.
- `http65.c` — no `ack_window()`; POST bounce is 256 bytes (900-byte bounce did not fit BSS after the scrolling UI).
- `ip65_icmp.s` — drop ICMP (no echo-reply over FILEIO).
- `ip65_error.s` / `ip65_outbuf.s` — short strerror; DHCP scratch 300 bytes.
- Patched copy of IP65 `tcp.s` (window 900 as `ldax #$8403`) via `tools/patch_ip65_tcp.py`; `ethernet_a2chat.s` + `eth_buffer.s` `eth_inp` 1024.

`make host-test` still passes. DHCP / static IP / long POST / stream should be checked on Uthernet (emulator or hardware). Watch `Timeout` (timer) and truncated RX (window vs buffer).

## What the plan asked for vs what shipped

| Plan item | Outcome |
|-----------|---------|
| Aux POST/answer `$4000–$BFFF` (32K) | Done. `A2CHAT_AUX_POST_MAX 0x8000`. |
| Bulk aux copy; stop `A2CHAT.PAY` on chat POST | Done. No PAY file on the live stream. |
| Packet-oriented `on_tcp`, 16-byte JSON ring, batched UI flush | Done (`A2CHAT_WIN 16`, span emit, flush not per space). |
| 65C02 inner loop for `JS_MSG_CONTENT` | Done in `jsonscan.c` / feed_buf hot path (C with 65C02 target). |
| Clock + `eval_count` + tok/s | Done, but **not** NSC IRQ. See [Clock](#clock). |
| Stream hist into aux; drop `A2CHAT.BOD` | Done in B8–B13. JSON is built in aux (`json_escape_aux` / `hist_emit_json_aux`); `http_post_aux` sends headers from main and body from aux. |

## Clock

Clock-card IRQs (NSC) **SED** and smash zero page; they are unsafe with this LC map. `clock_init` keeps **SEI**. Wall time is:

1. **P8** — peek ProDOS `$BF92`/`$BF93` (hour/minute). Help row and `A2CHAT.LOG` stamps. Label `P8` (not a build number).
2. **MFMS** — MegaFlash Pico ms timer + time string if present (`mfclock.s`, IIc ID `$FBB3=6`).
3. **JIFFY** — local `timer_read()` (VBL ~16 ms, or MegaFlash ms). Stock IP65 WAIT is not used.

Elapsed for tok/s: MegaFlash `mf_get_ms()` when present, else jiffy delta. Reset at last successful POST `tcp_send` (`t_post`). First content byte is `t_first`. Bar after a reply looks like `3s 16t 5/s` (seconds, Ollama `eval_count`, tokens/sec). Help row shows `HH:MM:SS` and **B13** on the far right (P8 is not shown there).

`src/nsc.s` is in the tree and **not linked**.

## Chat I/O path (B13)

1. User line → UI. Current prompt is **injected into the JSON** in main RAM (hist parse is not the only copy of the question).
2. Previous turns from `A2CHAT.LOG` (text: `>YOU stamp` / `>AI stamp`, CR) via `hist_emit_json_aux`.
3. Aux holds the full POST. Headers `send_str` from main; body `send_aux` (16-byte bounce). Files closed; zp `$50` saved across `aux_read`.
4. Stream: IP65 callback → jsonscan → batched 80-col blit + aux answer buffer.
5. After TCP close: append YOU/AI to `A2CHAT.LOG` with timestamps; optional `NOTE.MD` / `PROG.BAS` save.

One ProDOS file at a time. Opening `A2CHAT.BOD` for the POST while the log was open (**NBUFS=1**) is what produced `body write failed: A2CHAT.BOD` on the second turn.

## Measured runs (Virtual ][, llama3.2:3b)

Times include model generation, not Apple-only CPU. Accelerator state is whatever Virtual ][ had for that session (not logged as a separate CLOCK= line on the bar).

| Build | What we saw | Timing / error |
|-------|-------------|----------------|
| B7 | First prompt HTTP worked; model **greeted** instead of doing the task. Help showed `B7` and `P8`. | `11s 89t 8/s` |
| B7 | Second prompt | `body write failed: A2CHAT.BOD` |
| B8 | POST from aux, no BOD | `send body Timeout` |
| B9 | JSON length on the bar | `POST json 202` then `send aux Timeout`. 202 bytes ≈ **system prompt only** (user text missing). |
| B10 | zp `$50` saved around `aux_read` | Chat **completed**: `3s 16t 5/s`. Model still ignored the question (hist not in POST). |
| B11 | Inject current user text; log parse without `fseek`; 7-bit chars | User: “looking good” (answers match the prompt). |
| B12 | `/quit` without `$C082` | ProDOS QUIT instead of monitor `ERR $0104`. |
| B13 | Debug status removed | Same path as B12; bar is `POST /api/chat ...`. |

**How to read tok/s:** `eval_count / max(1, elapsed_seconds)`. `11s 89t 8/s` and `3s 16t 5/s` are dominated by the 3B model, not 6502 JSON. Apple-side cost shows up when t/s stays low on a tiny reply or when send/recv errors appear before any tokens.

There is no separate “three identical baseline prompts with accelerator on/off” log. The table above is the complete runtime evidence from the B7–B13 bring-up.

## Disk images

`make` / `make disk` / `make hd` always copy **sample** `cfg/A2CHAT.CFG`. Restore `HOST`, `MODEL`, `PREFIX` (and static `IP` if used) after a rebuild.

| Image | Size | Typical use |
|-------|------|-------------|
| `a2chat.po` | 140K `/A2CHAT/` | Floppy / small emulator volume |
| `a2chat.hdv` | 8,388,608 bytes | 8MB ProDOS HD (`make hd`) |

## Out of scope (still)

- Moving LC back to `$D000` or storing C or Ethernet buffers in LC on this machine
- Re-enabling Ollama tools JSON on the live POST
- ALTZP / aux LC overlays
- Editing the IP65 git repo, or switching to W5100 on-chip TCP (`w5100_config()`)
- NSC IRQ as the elapsed-time clock
