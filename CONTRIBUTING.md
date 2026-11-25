# Contributing to DIY-Thermocam

Contributions are welcome. All contributions are licensed under the [GNU General Public License v3.0](LICENSE).

## Repository Layout

- `firmware/` — Teensy 4.1 C++17 firmware (`arduino-cli`), host tests, simulator, and hardware test suite
- `app/` — Cross-platform Tauri v2 desktop application
- `hardware/` — KiCad 10 PCB (`pcb/`), parametric enclosure (`enclosure/`), and Bill of Materials (`bom/`)
- `docs/` — Single-source technical documentation and specifications
- `website/` — Source for `https://www.diy-thermocam.net`

## Local Setup and Verification

### Firmware
1. Install [Arduino CLI](https://arduino.github.io/arduino-cli/).
2. Compile from `firmware/`:
   ```sh
   arduino-cli compile
   ```
3. Run host unit tests without hardware:
   ```sh
   make -C firmware/tests_host test
   ```
4. If a DIY-Thermocam is connected, flash and run the hardware regression suite:
   ```sh
   arduino-cli upload -p <port>
   cd test && pip install -r requirements.txt && pytest --baseline baseline-3.0.4.json
   ```

### Hardware and BOM
- Keep `hardware/bom/bom.csv` as the single source of truth for component specs and supplier links.
- Run `python3 hardware/bom/validate_bom.py` before opening a PR.
- New PCB or enclosure revisions must pass ERC/DRC and geometry checks. Mark them `PROTOTYPE-PENDING` until physically verified.

### Documentation and Website
- Edit technical documentation in `docs/`. The website and READMEs read directly from those files.
- Preserve all existing `www.diy-thermocam.net` URLs and `/images/*` asset paths.

## Pull Request Guidelines

- Keep PRs focused on a single subsystem or scoped change.
- Run the relevant local build and test checks before opening a PR.
- Update `CHANGELOG.md` for user-visible changes.
- Reference related issues and note how hardware-facing changes were verified.
