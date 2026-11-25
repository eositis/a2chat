# A2CHAT TODO

## Don't block startup on an unreachable server

Done: the startup check (`net_diag_ollama()` from `main.c`) makes one
connect try instead of three, prints `Server check: Ctrl-C skips`, and on a
connect failure points at `/config`. `/ping` and chats still try three
times; the first chat repeats API discovery if the startup check failed.

Still open: ship the sample `A2CHAT.CFG` with `HOST=` empty and prompt for
it on first run, instead of a guessed address.

## Model picker from `/api/tags`

`MODEL=` has to match an installed model exactly, tag included:
`gpt-oss` fails when only `gpt-oss:20b` is installed, because Ollama reads a
bare name as `name:latest`.

Ollama's `GET /api/tags` lists the installed models (`"name"` fields in a
`models` array), and OpenAI-style servers answer `GET /v1/models` (`"id"`
fields in a `data` array). A2CHAT already sends one of these in
`api_probe()` but only checks the HTTP status and discards the body.
Instead:

- Pull the `"name"` values out of the reply with the existing `jsonscan`
  code, keeping a short list (memory is tight, so cap the count and length).
- Extend the existing `/model` command. Today `/model NAME` sets and saves
  the name and bare `/model` prints it; make bare `/model` show a numbered
  list instead, set `g_cfg.model` from the choice, and save `A2CHAT.CFG`.
- When `MODEL=` is not in the list, say so at startup or on `/ping` and
  suggest the closest match (for example `gpt-oss` → `gpt-oss:20b`).

Memory is tight. In 1.4 (build 77) main-memory BSS ends at `$A189`, about
250 bytes below the network driver hole at `$A280`, so keep the model list
in aux memory or in the idle receive buffer rather than in main-memory BSS,
and check `a2chat.map` after the change. The driver hole has about
330 bytes free with `A2CHAT.UTH` loaded and 1.3 KB with `A2CHAT.COG`.

## Clock shows `JIFFY`

The startup line `Clock JIFFY --:--` means A2CHAT found no real-time clock
(`clock.c`). It checks two sources:

- MegaFlash, only on a IIc/IIc+ ROM.
- The ProDOS clock: MACHID bit 0 and the time bytes at `$BF92`/`$BF93`. It
  never calls `GET_TIME`, on purpose: clock-card interrupt handlers broke
  A2CHAT's language-card memory (see `p8clock.s`).

Otherwise it falls back to counting vertical blanks (`ip65_timer.s`, about
16 ms per frame), which can time things but can't tell the time of day, hence
`--:--`. To look into:

- Done for CoGS (works on a real card): `cogs_wall()` in
  `clock.c` reads the card's `LOCALTIME` when no socket is open and sets the
  ProDOS date and time. The timezone comes from the card's own settings.
- Whether the vertical-blank counter is right on every machine A2CHAT
  supports. `$C019` behaves differently on the IIc and IIgs than on the IIe,
  and every network timeout depends on this counter.
