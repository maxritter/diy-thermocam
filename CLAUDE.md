# CLAUDE.md

Guidance for Claude Code in `diy-thermocam`. Canonical repository instructions live in [AGENTS.md](AGENTS.md) and the per-area guides below (this repository uses plain Markdown, not RuleSync):

- **[AGENTS.md](AGENTS.md)** — Monorepo layout, cross-cutting invariants, hardware safety, and outward-facing approval gates.
- **[firmware/AGENTS.md](firmware/AGENTS.md)** — Teensy 4.1 `arduino-cli` commands, Teensy core 1.62.0 `sketch.yaml`, firmware architecture, EEPROM/SPI/protocol gotchas, and hardware regression tests.
- **[hardware/AGENTS.md](hardware/AGENTS.md)** — PCB (`hardware/pcb/`), enclosure (`hardware/enclosure/`), BOM (`hardware/bom/`), verification gates, and physical prototype gate.
- **[website/AGENTS.md](website/AGENTS.md)** — `www.diy-thermocam.net` (13 preserved routes, `/images/*` hotlinks, Umami analytics, Phase 5 Astro 5 + Starlight rebuild).

## Quick Commands

```sh
# Firmware compile (Teensy 4.1, core 1.62.0)
cd firmware && arduino-cli compile

# Hardware regression suite (against a flashed V3 camera)
cd firmware/test && pytest --baseline baseline-3.0.4.json

# BOM validation (when hardware/bom/validate_bom.py is present)
python3 hardware/bom/validate_bom.py
```
