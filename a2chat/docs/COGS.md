# A2CHAT on the CoGS card

A2CHAT normally talks to Ollama through an Uthernet II (W5100). Building
with `COGS=1` makes it use a CoGS co-processor/network card instead.

```
make COGS=1            # same targets and outputs as the Uthernet build
```

Without `COGS=1` the build is unchanged (byte-identical binary). Both
variants write the same output names; switching between `make` and
`make COGS=1` relinks automatically, so check which one you last built
before copying a disk image.

## What is different

| | Uthernet II | CoGS |
|---|---|---|
| DHCP / static IP | IP65 on the Apple, `IP=` `GATEWAY=` `NETMASK=` | done by the card; set it on the card's own config screen. `IP=` is ignored |
| DNS | none, `HOST=` must be a dotted IPv4 address | the card resolves names, so `HOST=` may be a hostname (31 chars max) |
| TCP | W5100 socket, driven register by register | card socket 0, via `CONNECT` / `SEND` / `CLOSE` frames |
| Replies | streamed word by word (`STREAM=1`) | whole reply at once (`STREAM=0`); see below |
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

The CoGS build therefore defaults to `STREAM=0`: Ollama sends one JSON
object when the reply is finished, typically a few KB. Set `STREAM=1` in
`A2CHAT.CFG` once the card's firmware applies flow control as documented.
After each reply the CoGS build prints the bytes received and the peak FIFO
backlog, and warns if the FIFO filled.

## Files

- `src/cogs_io.s`: register primitives (probe, gated byte/block reads and
  writes, FIFO resync). All DATA accesses use `lda/sta $C080,x`.
- `src/cogs.c`: implements the `w5100_*` API from `w5100.h` on the CoGS
  frame protocol, plus `cogs_send()`, `cogs_netinfo()` and error codes.
- `src/http65.c`, `src/net.c`, `src/slot.c`, `src/ollama.c`: small
  `#ifdef A2CHAT_COGS` blocks for receive, send, connect, startup and
  hostname support.

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
