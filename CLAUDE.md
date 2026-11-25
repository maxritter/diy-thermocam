# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository

DIY-Thermocam V3: an open-source thermal imager built on a FLIR Lepton sensor and a Teensy 4.1. Only two folders hold source code:

- `firmware/3.0/`: the device firmware (C++, Arduino framework, built with arduino-cli).
- `software/thermal_live_viewer/3.0/`: a Python/pygame desktop app that streams live images from the device over USB serial.

The other folders hold binaries or design files that cannot be edited here: `software/*/` zips (third-party Windows tools), `pcb/3.0` (Eagle `.brd`/`.sch`, gerbers), `enclosure/3.0` (SketchUp). Firmware releases ship as `.hex` files on GitHub Releases, not in the repo.

There are no tests, linters or CI. To check a change, build it; the only full check is flashing a real device.

## Commands

Firmware is built with arduino-cli (run from `firmware/3.0`):

```sh
arduino-cli compile                 # first run downloads the pinned Teensy core + gcc
arduino-cli upload -p <port>        # device switched on; Teensy Loader opens and flashes
arduino-cli board list              # find the port
```

`sketch.yaml` is the single source of build settings: the FQBN (Teensy 4.1, `usb=serialmtp`, 450 MHz, `-O3`), the pinned `teensy:avr` core version, and a `dir:` entry for every library. The vendored copies in `lib/` are used instead of the core's bundled ones, so a new library needs both a folder in `lib/` and an entry in `sketch.yaml`. A library whose sources live in `src/` needs a `library.properties` file, or arduino-cli silently falls back to the core's copy (check the "Used library" table in the compile output). `include/` is listed as a library so the headers are on the include path. `3.0.ino` exists only because arduino-cli requires a sketch file named after the folder. Every compile and upload prints `discovery teensy:teensy-discovery not installed`; that line is harmless in profile mode.

To check a firmware change on a connected camera, talk to it over serial: send `0x64` (start), expect `0x64` back, send commands, and finish with `0xC8`, after which the camera returns to live mode.

Live viewer (run from `software/thermal_live_viewer/3.0`):

```sh
pip install -r requirements.txt   # use Python 3.12: pygame and opencv lack wheels for newer versions
python main.py                    # opens a pygame port picker, then LiveViewer(port).run()
```

The viewer is old Python 2 code. In Python 3, `/` returns floats, and numpy arrays reject float indices, so pixel and index arithmetic must use `//`.

Video recording calls the bundled `res/ffmpeg.exe`, so it only works on Windows.

## Firmware architecture

Every header is in `include/`, and each `src/**/x.cpp` has a matching `include/x.h`; files include each other by bare name (`#include <lepton.h>`). State is global: every variable is defined in `src/general/globalvariables.cpp` and declared `extern` in `include/globalvariables.h`. Pins, enum-like constants, EEPROM addresses and color-scheme numbers are all `#define`s in `include/globaldefines.h`.

Control flow (`src/main.cpp`): `setup()` runs once and never returns; `loop()` is empty.
`initHardware()` → `checkFWUpgrade()` → if neither display nor touch is detected, `serialInit()` (a headless serial-only loop that never returns) → `checkHardware()` → first-start wizard → `readEEPROM()` → `liveMode()`.

`liveMode()` in `src/thermal/thermal.cpp` is the main loop. Each pass checks for serial input, screen-off timeout, menu requests, image/video save, mass storage, then builds and draws the thermal frame. Interrupt handlers (`touchIRQ`, `buttonIRQ`) only set flags (`showMenu`, `imgSave`, `videoSave`, `longTouch`), and the loop acts on them. These flags are small state machines whose states are the `showMenu_*`, `imgSave_*` and `videoSave_*` defines.

Lepton frames are read asynchronously: `lepton_startFrame()` attaches `lepton_getFrameAsync` to the VSYNC pin, and the frame lands in `smallBuffer` (160×120 raw 16-bit). The Lepton sits on `SPI1`, while the display uses `SPI`. `display_writeScreen` sets `disableSPIIRQ`, and the VSYNC handler then skips frames until the screen write finishes. `createThermalImg()` (`src/thermal/create.cpp`) maps raw values through the active color map into `bigBuffer` (320×240 RGB565), which `display_writeScreen` pushes to the LCD. Both buffers are `malloc`ed in `initBuffer()`. A Lepton 2.5 (80×60) fills `smallBuffer` with each pixel doubled; `leptonVersion` selects the code path wherever the two sensors differ.

Folders under `src/`: `hardware/` holds drivers (Lepton over SPI/I2C, ILI9341 display, two touch controllers chosen at runtime by `touch_capacitive`, SD, battery, MTP mass storage, serial protocol). `gui/` holds the menus and screens. `thermal/` holds image creation, the live loop, saving and loading `.DAT`/`.BMP` files, and temperature conversion. `general/` holds globals and the 19 color maps.

### Gotchas when changing the firmware

- **Settings persist in EEPROM** at the fixed addresses in `globaldefines.h`. `readEEPROM()` (`src/hardware/hardware.cpp`) validates every byte and falls back to a default, because older firmware may have left other values there. A new setting needs a fresh address (never reuse one), validation in `readEEPROM()`, and a write wherever the menu changes it. `clearEEPROM()` wipes addresses 100–249.
- **Version bump**: change both `versionString` and `fwVersion` in `globalvariables.cpp`. A change in `fwVersion` makes `checkFWUpgrade()` show the "Firmware update completed" message, and the value is sent to the PC over serial.
- **Color schemes are duplicated**: `src/general/colorschemes.cpp` and the live viewer's `src/colorschemes.py` hold the same tables, indexed by the `colorScheme_*` numbers. Adding or reordering a scheme means updating `colorSchemeTotal`, `selectColorScheme()` in `thermal.cpp`, and the Python side.

## Serial protocol (firmware ↔ live viewer)

`include/connection.h` defines the byte commands, and `src/hardware/connection.cpp` implements them. The live viewer (`src/liveviewer.py`, 115200 baud) contains its own copy of the same constants and byte layouts, so any protocol change must be made in both places.

- The host sends `CMD_START` (0x64). In the live loop, `checkSerial()` replies with `CMD_START`, enters `serialOutput()`, and replies `CMD_END` (0xC8) when the session ends. A long touch on the device also ends the session.
- `CMD_GET_*` commands return fixed-size big-endian payloads: raw data is 4800 or 19200 `uint16` values depending on the Lepton version; the framebuffer is 76800 `uint16` values. Floats (spot temperature, calibration) go out as 4 bytes via `floatToBytes`. `CMD_SET_*` commands read their argument from the serial port and answer `CMD_INVALID` (0x00) when it is missing or out of range.
- `CMD_FRAME_*` commands switch what each frame request returns: raw, color, display or save.
