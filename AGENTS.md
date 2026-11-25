# AGENTS.md

Repository guidance for AI coding agents working in `diy-thermocam`. Keep this file plain Markdown (this repository does not use RuleSync).

## Monorepo Map

| Path | Purpose | Local guide |
|---|---|---|
| `firmware/3.0/` | Teensy 4.1 C++ firmware (`arduino-cli`, Teensy core 1.62.0) + hardware regression suite (`test/`) | [firmware/AGENTS.md](firmware/AGENTS.md) |
| `app/` | Cross-platform Tauri v2 + Rust + React 19 desktop app (Phase 4) | `app/AGENTS.md` (created in Phase 4) |
| `hardware/pcb/` | PCB sources (`eagle-v3.0/` legacy baseline; KiCad 9/10 port & V3.1/V4 in Phase 6) | [hardware/AGENTS.md](hardware/AGENTS.md) |
| `hardware/enclosure/` | Enclosure CAD (`legacy-v3.0/` SketchUp/SVG; `build123d` parametric CAD in Phase 6) | [hardware/AGENTS.md](hardware/AGENTS.md) |
| `hardware/bom/` | Single-source Bill of Materials (`bom.csv`, schema, validator) | [hardware/AGENTS.md](hardware/AGENTS.md) |
| `docs/` | Single-source project docs, protocol spec, overhaul plan | — |
| `website/` | `www.diy-thermocam.net` (imported Gatsby 5; Astro 5 + Starlight rebuild in Phase 5) | [website/AGENTS.md](website/AGENTS.md) |
| `software/` | Legacy Windows zips & Python live viewer (kept until `app/` hits parity) | — |

## Quick Commands

- **Firmware build:** `cd firmware/3.0 && arduino-cli compile`
- **Firmware flash (real camera):** `cd firmware/3.0 && arduino-cli upload -p <port>`
- **Firmware hardware regression suite:** `cd firmware/3.0/test && pytest --baseline baseline-3.0.4.json`
- **Firmware host unit tests (when present):** run target in `firmware/3.0/tests_host/`
- **BOM validation (when present):** `python3 hardware/bom/validate_bom.py`
- **Website build:** `cd website && yarn run build` (switches to `bun run build` in Phase 5)

## Cross-Cutting Invariants

- **Single sources of truth:** Never duplicate `docs/`, `hardware/bom/bom.csv`, `docs/protocol-v2.md`, or firmware colormaps (`firmware/3.0/src/general/colorschemes.cpp`). Consumers generate or read directly from these files.
- **`.DAT` compatibility:** Radiometric `.DAT` file layout must remain 100% compatible with existing V1/V2/V3 captures and camera firmware.
- **Hardware safety:** Do not open the camera serial port (`/dev/cu.usbmodem*`, `/dev/ttyACM*`, `COM*`) or flash the connected camera unless your task explicitly involves hardware verification.
- **Outward-facing actions are gated:** Never push to GitHub, publish releases, trigger live Netlify cutovers, or archive/delete external repositories without Max's explicit approval.
- **Git hygiene:** Do not rewrite published history on `master`; keep commits scoped per subsystem and validate CI workflows before committing.
