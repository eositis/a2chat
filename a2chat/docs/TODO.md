# A2CHAT TODO

## Don't block startup on an unreachable server

On launch, `main.c` calls `net_diag_ollama()`, which runs `api_probe()`
against `HOST=`/`PORT=`: a `GET /v1/models`, then `GET /api/tags` if that
is a 404 (or just one of them for a forced `API=`). Fresh disks ship with
`HOST=192.168.1.10`, so on most networks that probe has nothing to reach, and
the user waits through `connect_30()` before getting the prompt: three connect
tries, about 30 seconds each on the W5100, and on CoGS whatever the card's own
connect timeout is (with a 45-second guard in `cogs.c`). Changing `HOST=` is
only possible once that finishes. Ctrl-C cuts each try short, but nothing on
screen says so.

Ideas:

- Skip the startup probe entirely and let `/ping` (or the first chat) find out.
- Or probe once, with no retries, and say plainly that a key skips it.
- Ship the sample `A2CHAT.CFG` with `HOST=` empty and prompt for it on first
  run, instead of a guessed address.

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
