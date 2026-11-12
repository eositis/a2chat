# A2CHAT on the CoGS card

A2CHAT works with either an Uthernet II (W5100) or a CoGS
co-processor/network card, from the same disk. At startup it looks for a
CoGS card first; if it finds one it loads the CoGS driver `A2CHAT.COG`,
otherwise the Uthernet II driver `A2CHAT.UTH`. A plain `make` builds both
(`COGS=1` is still accepted and ignored).

## What is different

| | Uthernet II | CoGS |
|---|---|---|
| DHCP / static IP | IP65 on the Apple, `IP=` `GATEWAY=` `NETMASK=` | done by the card; set it on the card's own config screen. `IP=` is ignored |
| DNS | none, `HOST=` must be a dotted IPv4 address | the card resolves names, so `HOST=` may be a hostname (31 chars max) |
| TCP | W5100 socket, driven register by register | card socket 0, via `CONNECT` / `SEND` / `CLOSE` frames |
| Replies | streamed word by word (`STREAM=1`) | whole reply at once (`STREAM=0`); see below |
| Clock | ProDOS or MegaFlash clock, if any | the same, else the card's `LOCALTIME` (internet time plus the card's timezone offset); also sets the ProDOS date |
| Slot detection | W5100 retransmit-register probe | CoGS ID register (`$C0n5 = $A2`), slots 7..1, Disk II slots skipped. `SLOT=` still overrides |

On startup A2CHAT waits up to about 20 seconds for the card to report a
network link (`NETINFO` link state 2), then shows the card's IP address.

## Why replies are not streamed

The card is supposed to buffer up to 16 KB per socket and apply TCP flow
control when the Apple reads slowly (reference 4.5.2). In testing on
firmware current in October 2026, it instead dropped socket data whenever
its 16 KB card-to-host FIFO filled, with no ERROR event. A streamed Ollama
reply is about 140 bytes of JSON per word, plus any hidden reasoning (gpt-oss
streams it even with `"think":false`), and the Apple parses it at roughly
3 KB/s, so long replies filled the FIFO and arrived with whole chunks missing.

With a CoGS card, A2CHAT therefore defaults to `STREAM=0`: Ollama sends one JSON
object when the reply is finished, typically a few KB. Set `STREAM=1` in
`A2CHAT.CFG` once the card's firmware applies flow control as documented.
If a reply had card errors, resyncs or dropped data, or filled the FIFO,
A2CHAT prints a one-line summary after it and warns that the reply
may be damaged. A clean reply prints nothing extra.

## Files

- `src/cogs_io.s`: register primitives (probe, gated byte/block reads and
  writes, FIFO resync). All DATA accesses use `lda/sta $C080,x`.
- `src/cogs.c`: implements the `w5100_*` API from `w5100.h` on the CoGS
  frame protocol, plus `cogs_send()`, `cogs_netinfo()` and error codes.
- `src/nic_cog.c`, `src/cog_vec.s`: the CoGS side of the network driver
  interface in `src/nic.h` (startup, link wait, clock, report), built
  with `cogs.c` and `cogs_io.s` into the `A2CHAT.COG` overlay.
- `src/slot.c`: `slot_find_cogs()`, which `src/nic.c` runs at startup to
  choose between `A2CHAT.COG` and `A2CHAT.UTH`.

Received data is read straight out of the card's FIFO: for a `DATA`
event on our socket only the header and socket byte are consumed, and the
HTTP parser pops the payload in 64-byte windows. Sends go out as `SEND`
frames, one in flight at a time, each confirmed by `SEND_ACK`.

## Caveats

- Tested on a real CoGS card in October 2026 with `gpt-oss:20b` and
  `STREAM=0`: short and long replies arrive intact. Streaming loses data on
  long replies until the card's flow control is fixed (see above).
- Run A2CHAT from a local disk rather than from a CoGS network disk. The
  card's ProDOS driver shares the same FIFO; A2CHAT does no disk I/O while
  a connection is open, but booting from a local volume avoids the
  question entirely.
- Plain HTTP only. The card can do TLS (`CONNECT` flags bit 0); it is not
  wired up here.
