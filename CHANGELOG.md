# Changelog

All notable changes to DIY-Thermocam are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- Consolidated monorepo structure bringing `website/` (`diy-thermocam-website`, full git history), `hardware/` (`pcb/`, `enclosure/`, `bom/`), and `docs/` into a single repository.
- Hardware regression test suite in `firmware/3.0/test/` with Teensy 1.62.0 baseline verification.
- Agentic repository documentation (`AGENTS.md`, subsystem `AGENTS.md` guides, `CONTRIBUTING.md`, `CHANGELOG.md`) and path-filtered GitHub Actions CI & release workflows (`.github/workflows/`).

### Changed
- Migrated firmware builds from PlatformIO to `arduino-cli` (`firmware/3.0/sketch.yaml`) pinned to Teensy core `1.62.0` and its bundled libraries (including core `MTP_Teensy`).
- Merged USB serial fixes and live viewer stability improvements (`#122`, `#123`).

### Fixed
- Resolved Teensy 1.62.0 compiler warnings in firmware sources.
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
