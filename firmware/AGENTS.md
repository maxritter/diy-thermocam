# Firmware Agent Guide

Applies to `firmware/3.0/` (Teensy 4.1 + FLIR Lepton 3.5 / 2.5 thermal imager).

## Commands

Run build/upload from `firmware/3.0/`:

```sh
arduino-cli compile                 # uses sketch.yaml (Teensy 4.1, 450 MHz, -O3, core 1.62.0)
arduino-cli board list              # locate serial port
arduino-cli upload -p <port>        # requires camera switched on; opens Teensy Loader
```

Hardware regression suite (run from `firmware/3.0/test/` against a flashed camera):

```sh
pip install -r requirements.txt
pytest --baseline baseline-3.0.4.json   # ~34 tests, ~50 s; auto-detects serial port
pytest -k colorbar                      # single test
pytest --manual -s                      # includes interactive touch/button tests
```

- `sketch.yaml` is the single source of build settings (`teensy:avr` `1.62.0`, `usb=serialmtp`, `speed=450`, `opt=o3std`). Core 1.62.0 bundles ADC, Bounce, EEPROM, Metro, SD, SdFat, SPI, Time, Wire, and `MTP_Teensy.h`. Only `include/` is listed as a local library dir.
- `3.0.ino` exists solely because `arduino-cli` requires a sketch file matching the folder name.
- `discovery teensy:teensy-discovery not installed` is expected and harmless in profile mode.

## Architecture

- Headers live in `include/`; each `src/**/x.cpp` has a matching `include/x.h` included by bare name (`#include <lepton.h>`).
- Global state is defined in `src/general/globalvariables.cpp` and declared `extern` in `include/globalvariables.h`. Pins, EEPROM addresses, and color-scheme indices are `#define`d in `include/globaldefines.h`.
- Control flow (`src/main.cpp`): `setup()` never returns (`loop()` is empty): `initHardware()` → `checkFWUpgrade()` → if neither display nor touch is found, `serialInit()` (headless serial loop) → `checkHardware()` → first-start wizard → `readEEPROM()` → `liveMode()`.
- `liveMode()` (`src/thermal/thermal.cpp`) polls serial, screen timeout, menus, image/video save, and mass storage, then renders thermal frames. `touchIRQ` / `buttonIRQ` set state-machine flags (`showMenu`, `imgSave`, `videoSave`, `longTouch`).
- Lepton uses `SPI1` with VSYNC interrupt filling `smallBuffer` (160×120 raw `uint16`; Lepton 2.5 doubles 80×60 pixels). Display uses `SPI` with `bigBuffer` (320×240 RGB565).
- Folders under `src/`: `hardware/` (drivers & serial protocol), `gui/` (touch menus), `thermal/` (rendering, live loop, `.DAT`/`.BMP`, temperature math), `general/` (globals & 19 colormaps). Pure hardware-independent logic for host tests belongs in `src/core/` (zero `<Arduino.h>`).

## Critical Gotchas

- **Keep 450 MHz default:** Below 460 MHz the Teensy 4.1 DCDC stays at 1150 mV (vs 1250 mV at 600 MHz), cutting heat inside the closed enclosure next to the Lepton sensor and LiPo.
- **EEPROM layout:** Fixed addresses in `globaldefines.h`; `clearEEPROM()` wipes 100–249. Never reuse an address. Any new setting needs validation/default fallback in `readEEPROM()` (`src/hardware/hardware.cpp`) and writes wherever changed.
- **Version bumps:** Update both `versionString` and `fwVersion` in `src/general/globalvariables.cpp` (`fwVersion` triggers `checkFWUpgrade()` and is reported over serial).
- **Color schemes:** `src/general/colorschemes.cpp` is the canonical source of the 19 firmware LUTs. Update `colorSchemeTotal`, `selectColorScheme()` (`src/thermal/thermal.cpp`), and downstream desktop consumers when changing them.
- **Legacy v1 serial gotcha (until v2 cutover):** `thermocam.py` opens sessions with `0x64` and closes with `0xC8`. Because v1 reads one byte per frame loop, one rejected payload desyncs subsequent answers (`0x00`) — always debug the first failure only.
- **Hardware tests:** Tests must restore every device setting they mutate.
