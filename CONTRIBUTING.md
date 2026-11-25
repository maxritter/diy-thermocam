# Contributing to DIY-Thermocam

Thank you for contributing to DIY-Thermocam! All contributions are licensed under the [GNU General Public License v3.0](LICENSE).

## Monorepo Layout

- `firmware/3.0/` — Teensy 4.1 C++ firmware (`arduino-cli`) and hardware regression suite (`firmware/3.0/test/`)
- `app/` — Cross-platform Tauri v2 desktop application (in progress)
- `hardware/` — PCB (`hardware/pcb/`), enclosure (`hardware/enclosure/`), and Bill of Materials (`hardware/bom/`)
- `docs/` — Technical documentation and specifications (single source of truth for GitHub and the website)
- `website/` — Official site (`https://www.diy-thermocam.net`)
- `software/` — Legacy desktop tools (maintained until the new desktop app reaches full parity)

## Local Setup & Verification

### Firmware
1. Install [Arduino CLI](https://arduino.github.io/arduino-cli/).
2. Compile from `firmware/3.0/`:
   ```sh
   arduino-cli compile
   ```
3. If you have a DIY-Thermocam V3 connected, flash and run the hardware regression suite:
   ```sh
   arduino-cli upload -p <port>
   cd test && pip install -r requirements.txt && pytest --baseline baseline-3.0.4.json
   ```

### Hardware & BOM
- Keep `hardware/bom/bom.csv` as the single source of truth for component specifications and supplier links.
- Run `python3 hardware/bom/validate_bom.py` (when present) before opening a PR.
- New PCB or enclosure revisions must pass DRC/ERC and clearance checks, and remain marked prototype-pending until physically tested.

### Documentation & Website
- Edit technical documentation in `docs/` rather than duplicating text across `website/` and READMEs.
- Preserve all existing `www.diy-thermocam.net` URLs and `/images/*` asset paths.

## Pull Request Guidelines

- Keep PRs focused on a single subsystem or well-scoped change.
- Run the relevant local build and test checks before submitting.
- Update `CHANGELOG.md` under `[Unreleased]` for user-visible changes.
- Reference related issues and describe how hardware-facing changes were verified.
