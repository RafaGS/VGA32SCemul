# VGA32SCemul

---

Sega Computers SC-3000/SC-3000H Emulator for FabGL cards
---

This is an Sega Computer SC-3000/SC-3000H emulator for ESP32 systems compatible with FabGL/VGA32.

Installation and usage: [MINIBOTS](https://minibots.wordpress.com/)

## Origin

Based on the emulator [VGA32MSXemul](https://github.com/RafaGS/VGA32MSXemul)

### Reused from VGA32MSXemul without changes:

- `Z80.c/h` + `Codes*.h` — Fayzullin's Z80 core, machine-agnostic.
- `I8255.c/h` — matches the actual PPI of the SC-3000 exactly.
- FabGL bootloader (board detection, VGA16Controller, PS2Controller, SD card mounting) — adapted to the new `.ino`.

### Discarded:

- Floppy/FDIDisk/WD1793, NetPlay, Hunt (cheats), Patch/IPS, Record, SHA1 (detección de romsets MSX), YM2413/SCC (audio de MSX2), AY8910,
  V9938, Console/Menu/Touch*/`*Mux.h` (UI de MSX)

### Credits

- Mantained by RafaG
- Based on fMSX by Marat Fayzullin and FabGL by Fabrizio Di Vittorio.
- Based on ttgo-msxemul by ChanHyeok Ju
