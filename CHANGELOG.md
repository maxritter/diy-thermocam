# Changelog

All notable changes to DIY-Thermocam are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [4.0.0] - 2026-11-25

Complete monorepo overhaul spanning firmware, USB protocol, cross-platform desktop software, native KiCad 10 hardware, parametric CAD enclosures, single-source BOM, and documentation website.

### Added
- **Monorepo Consolidation:** Unified `firmware/`, `app/`, `hardware/` (`pcb/`, `enclosure/`, `bom/`), `docs/`, and `website/` (`diy-thermocam-website` with full git history) into a single repository with path-filtered GitHub Actions CI/release workflows, `.vscode/` tasks/launch configs, and `.githooks/pre-commit`.
- **USB Protocol v2.0 (`docs/protocol-v2.md`):** Little-endian binary framing with CRC-8/CRC-32 verification, full-rate push streaming (**8.77 fps** verified on real V3 hardware), **0.26 ms** median command latency, hardware shutter triggers, live parameter control, and direct microSD file transfer (**9.20 MB/s**) without MTP drivers or camera reboots.
- **DIY-Thermocam Desktop Studio (`app/`):** Cross-platform **Tauri v2 + Rust (`thermocam-core`) + React 19 + WebGL2** desktop app for macOS, Windows, and Linux featuring `Standard | Pro` workspace modes, 60 fps WebGL2 viewport, 19 firmware + 4 scientific colormaps, Spot/Box/Circle/Polygon ROIs with per-ROI emissivity & reflected temperature correction, Line Profiles, isotherms, Time Plotter + TempSwitch alarms, zero-FFmpeg H.264 MP4 / GIF / 16-bit Kelvin TIFF / HTML-PDF report export, direct SD card browser, and pure-Rust HalfKay firmware updater.
- **Firmware Testing & Virtual Camera (`firmware/tests_host/`, `firmware/sim/`, `firmware/test/`):** Host-side C++ unit test suite, virtual Protocol v2 camera simulator, and physical Teensy 4.1 hardware regression suite (`pytest --baseline baseline-3.0.4.json`).
- **Native KiCad 10 PCB (`hardware/pcb/`, `PROTOTYPE-PENDING`):** Zero-solder turnkey V4.0 PCBA with board-edge USB-C + ESD (`USBLC6-2SC6`), TI `BQ24074RGTR` dynamic power-path charger (`4.4 V VSYS`), TI `TPS61022RWUR` 5 V boost + `FXL0630-1R0-M` inductor, `AO3401A`/`AO3400A` active reverse-polarity LiPo protection + 2 A PTC fuse, `XC6206P332MR-G` 1 µA 3.3 V RTC LDO (eliminating the CR1220 coin cell), onboard SMD controls (`SW1`/`SW2`/`SW3`), USB D+/D- spring pogo pins (`P1`/`P2`), 3-pad capacitive-touch jumpers (`JP1`/`JP2`), and 31 live-verified LCSC SKUs with calibrated JLCPCB/Seeed/PCBWay BOM & CPL exports (legacy V3.0 Eagle baseline preserved at git tag `3.0.4`).
- **Parametric `build123d` Enclosures (`hardware/enclosure/`):** Support-free 3D/CNC shells (`STEP`, `STL`, `3MF`) and improved 3 mm laser-cut acrylic (`SVG`, `DXF`) for both V3.0 and V4.0 (`PROTOTYPE-PENDING`), with flush top-open dovetail lens cap, widened `13.4→20.2 mm` cone clearing both **FLIR Lepton 3.5** (`71° DFOV`) and **Lepton 3.1R** (`95° HFOV / 119° DFOV`), guided `capture_button` plunger + captive `slide_switch_cap`, insulating `battery_cradle` + `battery_lid` for slim `≤5.0 mm` `505060` UL/IEC 62133 LiPo, blind-roofed `1/4"-20` hex-nut tripod pocket (`≤8.0 mm` stud), and 15/15 automated OCCT geometry/interference tests.
- **Single-Source BOM (`hardware/bom/`):** Machine-readable `bom.csv`, JSON Schema validator (`validate_bom.py`), and 1:1 parity with all 31 V4.0 LCSC SKUs.
- **Rebuilt Official Website (`website/`):** Astro 5 + Starlight site for `www.diy-thermocam.net` featuring the interactive modular `/docs/partlist/` BOM builder (`V4.0 Zero-Solder` vs `V3.0 Classic`, enclosure mode, Lepton/Display toggles, and V3-upgrade reuse mode), single-sourced repo docs, Pagefind search, and dark-studio V4.0 visual identity.

### Changed
- Migrated firmware builds from PlatformIO to `arduino-cli` (`firmware/sketch.yaml`) pinned to Teensy core `1.62.0` and its bundled libraries (`0 errors, 0 warnings`).
- Upgraded on-device video playback to zero-flicker rendering and cleanly restored interactive menus on USB disconnect/return.
- Updated all documentation (`docs/hardware.md`, `docs/building-instructions.md`, `docs/software.md`) to reflect verified 4.0.0 benchmarks, V4.0 `PROTOTYPE-PENDING` gate, and 100% `.DAT` backward compatibility.

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

## [2.4.8] - 2021-02-07
- Final DIY-Thermocam V1/V2 (Teensy 3.2/3.6) firmware release ([GitHub Release](https://github.com/maxritter/diy-thermocam/releases/tag/2.4.8)).
