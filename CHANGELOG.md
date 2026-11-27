# Changelog

All notable changes to DIY-Thermocam are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [4.0.0] - 2026-11-25

Major release unifying firmware, desktop software, KiCad 10 hardware, parametric enclosures, single-source BOM, and documentation into one repository.

### Added
- **Unified repository:** Merged `firmware/`, `app/`, `hardware/` (`pcb/`, `enclosure/`, `bom/`), `docs/`, and `website/` with path-filtered CI, `.vscode/` tasks, and `.githooks/pre-commit`.
- **USB Protocol v2.0 (`docs/protocol-v2.md`):** Little-endian binary framing with CRC-8 and CRC-32 checks. Measured on a real camera at **8.77 fps** streaming, **0.26 ms** median command latency, and **9.20 MB/s** direct microSD reads without MTP.
- **Desktop Studio (`app/`):** Cross-platform Tauri v2, Rust (`thermocam-core`), React 19, and WebGL2 app for macOS, Windows, and Linux. Includes Standard and Pro modes, 19 firmware plus 4 scientific colormaps, ROIs with per-ROI emissivity, Line Profiles, isotherms, and Time Plotter alarms. Exports H.264 MP4, GIF, 16-bit Kelvin TIFF, and reports, with direct SD browsing and HalfKay firmware updates.
- **Firmware tests and simulator (`firmware/tests_host/`, `firmware/sim/`, `firmware/test/`):** Host C++17 unit tests, virtual Protocol v2 simulator, and physical Teensy 4.1 hardware regression suite.
- **KiCad 10 V4.0 PCB (`hardware/pcb/`):** Zero-solder PCBA with USB-C, ESD protection (`USBLC6-2SC6`), TI `BQ24074RGTR` charger, and TI `TPS61022RWUR` 5 V boost. Adds `AO3401A`/`AO3400A` reverse-polarity protection, 2 A PTC fuse, `XC6206P332MR-G` 3.3 V RTC LDO, SMD switches, USB pogo pins, touch jumpers, and 31 verified LCSC SKUs.
- **Parametric [build123d](https://github.com/gumyr/build123d) enclosures (`hardware/enclosure/`):** V4.0 support-free 3D-printed shell (`STEP`, `STL`, `3MF`) and 3 mm acrylic case (`SVG`, `DXF`). Clears both FLIR Lepton 3.5 (`71°`) and 3.1R (`119°`), with a dovetail lens cap, `505060` battery cradle, `1/4"-20` tripod pocket, automated geometry tests, and Blender renders.
- **Single-source BOM (`hardware/bom/`):** Machine-readable `bom.csv` and `validate_bom.py` aligned 1:1 with all 31 V4.0 LCSC SKUs.
- **Official website (`website/`):** Astro 5 + Starlight site for `www.diy-thermocam.net` with an interactive `/docs/partlist/` configurator, Pagefind search, and single-sourced repo docs.

### Changed
- Migrated firmware builds from PlatformIO to `arduino-cli` (`firmware/sketch.yaml`) pinned to Teensy core `1.62.0` (`0 errors, 0 warnings`).
- Upgraded on-device video playback to double-buffered rendering and restored interactive menus cleanly after USB disconnect.
- Updated manuals and hardware docs for Firmware 4.0 benchmarks, the V4.0 hardware, and `.DAT` compatibility.

### Fixed
- Resolved all Teensy 1.62.0 compiler warnings in firmware sources.
- Fixed USB serial live streaming stability and menu-return state handling (`#122`, `#123`).
- Fixed Python live viewer crash when displaying temperature points.

## [3.0.4] - 2023-05-06
- Firmware Version 3.0.4 ([GitHub Release](https://github.com/maxritter/diy-thermocam/releases/tag/3.0.4)).

## [3.0.3] - 2023-05-05
- Firmware Version 3.0.3 ([GitHub Release](https://github.com/maxritter/diy-thermocam/releases/tag/3.0.3)).

## [3.0.2] - 2021-06-21
- Firmware Version 3.0.2 ([GitHub Release](https://github.com/maxritter/diy-thermocam/releases/tag/3.0.2)).

## [3.0.1] - 2021-06-05
- Firmware Version 3.0.1 ([GitHub Release](https://github.com/maxritter/diy-thermocam/releases/tag/3.0.1)).

## [3.0.0] - 2021-06-04
- Initial DIY-Thermocam V3 (Teensy 4.1) firmware release ([GitHub Release](https://github.com/maxritter/diy-thermocam/releases/tag/3.0.0)).

## [2.4.9] - 2026-11-27 (V1 & V2 — Teensy 3.2 / 3.6)
- Latest maintenance release for DIY-Thermocam V1 and V2 ([GitHub Release](https://github.com/maxritter/diy-thermocam/releases/tag/2.4.9), source on branch [`v2.4.x`](https://github.com/maxritter/diy-thermocam/tree/v2.4.x/Firmware_V2)). V3 and V4 cameras (Teensy 4.1) use `4.0.0`.
- Fixed runaway calibration compensation in Fahrenheit/Celsius, EEPROM preset validation and auto-healing on upgrade, MLX90614 I2C retry freezes, Lepton 2.5/3.5 ID collision and bounded CCI I2C, and V1 USB Mass Storage boot jump (`extern "C" startup_late_hook`).
- Ported VoSPI CCITT CRC-16 verification, symmetric integer 3×3 filters, RGB565 colormap LUT, fixed-point V2 bilinear upscaling, and single-transaction V1 SPI refresh from Firmware 4.0.

## [2.4.8] - 2021-02-07
- Previous DIY-Thermocam V1/V2 (Teensy 3.2/3.6) firmware release ([GitHub Release](https://github.com/maxritter/diy-thermocam/releases/tag/2.4.8)).
