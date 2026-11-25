# Desktop App & Software Ecosystem

The 2026 **DIY-Thermocam Desktop App** unifies live viewing, radiometric analysis, video export, SD card browsing, and firmware updates into one cross-platform application for **macOS, Windows, and Linux**.

## DIY-Thermocam Desktop Studio (Tauri v2 + Rust)

Built with **Tauri v2**, **Rust** (`thermocam-core`), **React 19**, and **WebGL2**, DIY-Thermocam Studio communicates over [USB Protocol v2](/docs/protocol-v2/) at verified **8.77 fps** native Lepton push streaming, **0.26 ms** median command latency, and **9.20 MB/s** direct SD read speed, while reading every V1/V2/V3 `.DAT` capture natively.

Switch directly between **Standard** mode (clean live viewing, quick snapshots, palette/range adjustment, and one-click export) and **Pro** mode (full radiometric ROI analysis, isotherms, line profiles, multi-channel time-plotter, and TempSwitch threshold alarms).

### Key Capabilities

- **60 fps WebGL2 Live View (8.77 fps Native Sensor Push):** Auto-detects connected cameras, streams native-rate 16-bit radiometric frames at `0.26 ms` command latency, supports all 19 firmware colormaps plus perceptually uniform scientific palettes (`Inferno`, `Viridis`, `Magma`, `Plasma`) and custom 256-pixel LUT imports.
- **Standard | Pro Workspace Modes:** Start in **Standard** mode for uncluttered live capture and quick exports, or toggle **Pro** mode to open draggable colorbar isotherms, live 256-bin histograms, multi-ROI tools, and continuous time-series logging.
- **Precision Measurements & Emissivity:** Place 1×1 or 3×3 spot meters, Box, Circle, and Freehand Polygon ROIs, Line Profile charts, and 2-point ΔT comparisons. Apply global or **per-ROI emissivity and reflected temperature correction** for mixed scenes (e.g. bare aluminum next to matte PCB solder mask).
- **Time Plotter & TempSwitch Alarms:** Plot multi-channel temperature curves over time and trigger automatic `.DAT` captures, recordings, or alerts when a threshold is crossed.
- **Zero-FFmpeg Video & Radiometric Export:** Record and convert sequences directly to **H.264 MP4**, **animated GIF**, high-DPI PNG/JPEG with scale bars, temperature CSV, **16-bit Kelvin TIFF**, lossless `.DAT` sequences, and one-click HTML/PDF/Markdown inspection reports.
- **Direct SD Card Browser (`9.20 MB/s`):** Browse, preview, download, and delete captures on the camera's microSD card over USB Protocol v2 — no MTP drivers or camera reboots required.
- **Built-In Firmware Updater:** Pure-Rust HalfKay HID flasher updates Teensy 4.1 firmware from GitHub Releases or a local `.hex` file with SHA-256 verification.

## Legacy Windows & Python Tools (V3.0 Archive)

The original tools remain archived at git tag [`3.0.4`](https://github.com/maxritter/diy-thermocam/tree/3.0.4/software) for legacy Windows workflows and V1/V2/V3.0 `.DAT` files:

- **ThermoVision by Joe-C (Windows):** Third-party radiometric analysis software supporting DIY-Thermocam `.DAT` drag-and-drop editing and conversion. More details on [Joe-C's project page](http://joe-c.de/pages/projekte/thermovision.php) and in [tag `3.0.4`](https://github.com/maxritter/diy-thermocam/tree/3.0.4/software/thermal_analysis_software).
- **Thermal Live Viewer (Python):** Standalone cross-platform viewer for legacy v1 USB serial live streaming (archived at tag `3.0.4`).
- **Thermal Data Viewer (Windows):** Batch converter for legacy `.DAT` folders to JPG/BMP/PNG/AVI (archived at tag `3.0.4`).
- **Video Converter (Windows):** Merges saved frame sequences into video files (archived at tag `3.0.4`).
