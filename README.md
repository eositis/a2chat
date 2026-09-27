# A2CHAT — summary

**A2CHAT** is a full featured native ProDOS 8 chat client for an Uthernet II equipped Apple IIe and IIc (Apple IIgs should also work). It talks HTTP over the net to an [Ollama](https://ollama.com) AI host on your LAN, streams the model’s reply in 80-column text, and can write notes or BASIC listings back to a ProDOS disk. It now also supports the [Olla](https://github.com/thushan/olla) AI proxy allowing proxied connections to public AI services.

This is a full AI client, able to read or write files to local disk. On connection to an AI host/model, provides a definitive context file for AppleSoft programming, ensuring resultant AppleSoft code generation to comply with AppleSoft standards.

## What you need

**Apple**

- Enhanced IIe or IIc / IIc+ with 65C02 and 80-column firmware. IIgs untested, but should work.
- 128K RAM
- Uthernet II (WIZnet W5100)
  - **IIe:** card in any slot (auto-scan tries slot 3 first). If an accelerator moved the card, set `SLOT=` in config.
  - **IIc / IIc+:** MegaFlash exposing Uthernet II in **slot 4**. A stock IIc without MegaFlash cannot network.
- ProDOS 8 on the boot volume
- Storage: Will work from floppy, performance will improve when using a SmartPort drive or equivalent. (MegaFlash, FujiNet, FloppyEMU, etc) A2chat stores the chat log on disk in txt format
- **NOTE** Works well on a 1MHz Apple machine. An accelerator, zip chip or IIc+ will improve the experience.
- File upload - de-tokenizes applesoft programs to text before sending to the AI
- File save - can save AI generated files as BAS, TXT or BIN. Automatically will identify if a BASIC file has been created. BASIC files are tokenized on save.

**Ollama host (Mac/PC/Linux) or Olla proxy**

- Ollama listening on the LAN, not only localhost
- **Alternate** Olla proxy on the LAN configured with connections to internal or external AI hosts
- A pulled model whose name matches `MODEL=` in `A2CHAT.CFG` (for example `llama3.2:3b`)
- The Apple and the Ollama machine on the same IPv4 subnet. A2CHAT has **no DNS** — `HOST=` must be a dotted address such as `192.168.0.111`.

**Sample chat conversation**
<img width="2240" height="1600" alt="a2caht-conversation" src="https://github.com/user-attachments/assets/bd0bf623-bda2-4187-85db-8e4b70a58dfc" />

**helps write applesoft program code and saves it to disk as a tokenized .bas file**
<img width="2240" height="1600" alt="a2chat-SaveBasic2" src="https://github.com/user-attachments/assets/a5590171-8a52-4946-94e9-d5d8c9cbb811" />

**loading code for analysis**
<img width="2240" height="1600" alt="a2chat-loadHangman" src="https://github.com/user-attachments/assets/684d4288-1fc6-4bc1-9b40-1d6a99226f1f" />

**About page**
<img width="2240" height="1600" alt="a2caht-about" src="https://github.com/user-attachments/assets/4320f594-6482-447b-81b6-6f56de61f022" />

