# A2CHAT — summary

**A2CHAT** is a native ProDOS 8 chat client for the Apple IIe and IIc. It talks HTTP over Uthernet II to [Ollama](https://ollama.com) on your LAN, streams the model’s reply in 80-column text, and can write notes or BASIC listings back to a ProDOS disk.

No browser. No cloud account. The Apple is the terminal; your Mac or PC only runs the model.
Does not work with public AI models, as these need an encrypted connection. Only works with private AI solutions such as Ollama, running on your home network.

## What you need

**Apple**

- Enhanced IIe or IIc / IIc+ with 65C02 and 80-column firmware
- 128K RAM (aux memory holds the HTTP body and the streamed answer)
- Uthernet II (WIZnet W5100)
  - **IIe:** card in any slot (auto-scan tries slot 3 first). If an accelerator moved the card, set `SLOT=` in config.
  - **IIc / IIc+:** MegaFlash exposing Uthernet II in **slot 4**. A stock IIc without MegaFlash cannot network.
- ProDOS 8 on the boot volume
- **NOTE** This is fairly slow on a standard speed IIe or IIc at this time. An accelerator, zip chip or IIc+ will improve the experience.

**Ollama host (Mac/PC/Linux)**

- Ollama listening on the LAN, not only localhost
- A pulled model whose name matches `MODEL=` in `A2CHAT.CFG` (for example `llama3.2:3b`)
- The Apple and the Ollama machine on the same IPv4 subnet. A2CHAT has **no DNS** — `HOST=` must be a dotted address such as `192.168.0.111`.

**Initial welcome screen shows connection status**
<img width="2576" height="1920" alt="iScreen Shoter - GSSquared - 260911190443" src="https://github.com/user-attachments/assets/418b770b-71dc-437e-8197-befe45959529" />

**helps write applesoft program code and can write it to disk as an *untokenized* .bas file**
<img width="2576" height="1920" alt="iScreen Shoter - GSSquared - 260911191317" src="https://github.com/user-attachments/assets/09421dbe-3a1a-49f1-9e0e-6fe9113d55d0" />

**Some interesting conversations are possible**
<img width="2576" height="1920" alt="iScreen Shoter - GSSquared - 260911134131" src="https://github.com/user-attachments/assets/80e9ad8d-d057-4bd3-97fe-e9606f84bf2a" />

