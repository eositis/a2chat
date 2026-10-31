# A2CHAT on the CoGS card

A2CHAT normally talks to Ollama through an Uthernet II (W5100). Building
with `COGS=1` makes it use a CoGS co-processor/network card instead.

```
make COGS=1            # same targets and outputs as the Uthernet build
```

Without `COGS=1` the build is unchanged (byte-identical binary).

## What is different

| | Uthernet II | CoGS |
|---|---|---|
| DHCP / static IP | IP65 on the Apple, `IP=` `GATEWAY=` `NETMASK=` | done by the card; set it on the card's own config screen. `IP=` is ignored |
| DNS | none, `HOST=` must be a dotted IPv4 address | the card resolves names, so `HOST=` may be a hostname (31 chars max) |
| TCP | W5100 socket, driven register by register | card socket 0, via `CONNECT` / `SEND` / `CLOSE` frames |
| Slot detection | W5100 retransmit-register probe | CoGS ID register (`$C0n5 = $A2`), slots 7..1, Disk II slots skipped. `SLOT=` still overrides |

On startup A2CHAT waits up to about 20 seconds for the card to report a
network link (`NETINFO` link state 2), then shows the card's IP address.

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

- Not yet tested on real hardware. The protocol code was tested on a PC
  against a simulated card, and the 6502 register routines in a 65C02
  simulator.
- Run A2CHAT from a local disk rather than from a CoGS network disk. The
  card's ProDOS driver shares the same FIFO; A2CHAT does no disk I/O while
  a connection is open, but booting from a local volume avoids the
  question entirely.
- Plain HTTP only. The card can do TLS (`CONNECT` flags bit 0); it is not
  wired up here.
