# AGENTS.md

Repository guidance for AI coding agents working in `diy-thermocam`. Keep this file plain Markdown (this repository does not use RuleSync).

## Monorepo Map

| Path | Purpose | Local guide |
|---|---|---|
| `firmware/` | Teensy 4.1 C++17 firmware (`arduino-cli`, Teensy core 1.62.0), host unit tests (`tests_host/`), simulator (`sim/`), and hardware regression suite (`test/`) | [firmware/AGENTS.md](firmware/AGENTS.md) |
| `app/` | Cross-platform Tauri v2 + Rust + React 19 desktop app | [app/AGENTS.md](app/AGENTS.md) |
| `hardware/pcb/` | Native KiCad 10 V4.0 PCB sources, fabrication packages (`fab/`), and export scripts | [hardware/AGENTS.md](hardware/AGENTS.md) |
| `hardware/enclosure/` | Parametric `build123d` CAD (`cad/`), 2D/3D exports (`exports/`), and renders (`renders/`) | [hardware/AGENTS.md](hardware/AGENTS.md) |
| `hardware/bom/` | Single-source Bill of Materials (`bom.csv`, schema, validator) | [hardware/AGENTS.md](hardware/AGENTS.md) |
| `docs/` | Single-source project docs, protocol spec, overhaul plan | — |
| `website/` | `www.diy-thermocam.net` (Astro 5 + Starlight) | [website/AGENTS.md](website/AGENTS.md) |

## Quick Commands

- **Firmware build:** `cd firmware && arduino-cli compile`
- **Firmware flash (real camera):** `cd firmware && arduino-cli upload -p <port>`
- **Firmware hardware regression suite:** `cd firmware/test && pytest --baseline baseline-3.0.4.json`
- **Firmware host unit tests:** `make -C firmware/tests_host test`
- **BOM validation:** `python3 hardware/bom/validate_bom.py`
- **Website build:** `cd website && bun run build`

## Cross-Cutting Invariants

- **Single sources of truth:** Never duplicate `docs/`, `hardware/bom/bom.csv`, `docs/protocol-v2.md`, or firmware colormaps (`firmware/src/general/colorschemes.cpp`). Consumers generate or read directly from these files.
- **`.DAT` compatibility:** Radiometric `.DAT` file layout must remain 100% compatible with existing V1/V2/V3 captures and camera firmware.
- **Hardware safety:** Do not open the camera serial port (`/dev/cu.usbmodem*`, `/dev/ttyACM*`, `COM*`) or flash the connected camera unless your task explicitly involves hardware verification.
- **Outward-facing actions are gated:** Never push to GitHub, publish releases, trigger live Netlify cutovers, or archive/delete external repositories without Max's explicit approval.
- **Git hygiene:** Do not rewrite published history on `master`; keep commits scoped per subsystem and validate CI workflows before committing.
