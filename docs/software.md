# Desktop App and Software

**DIY-Thermocam Desktop Studio** combines live viewing, radiometric analysis, video export, microSD access, and firmware updates in one open-source app for **macOS, Windows, and Linux**.

## DIY-Thermocam Desktop Studio (Tauri v2 + Rust)

Built with Tauri v2, Rust (`thermocam-core`), React 19, and WebGL2, Desktop Studio talks to the camera over [USB Protocol v2](/docs/protocol-v2/). On real Teensy 4.1 + Lepton 3.5 hardware, it sustains **8.77 fps** live streaming, **0.26 ms** median command latency, and **9.20 MB/s** direct microSD reads. It opens every V1, V2, V3, and V4 `.DAT` capture natively.

### Capabilities

- **WebGL2 Live View:** Auto-detects connected cameras and renders all 19 firmware palettes plus `Inferno`, `Viridis`, `Magma`, `Plasma`, and custom 256-pixel LUTs.
- **Standard and Pro modes:** Use **Standard** mode for clean live capture, palette switches, and quick exports. Switch to **Pro** mode for draggable isotherms, live 256-bin histograms, multi-ROI measurements, and time-series logging.
- **Measurements and emissivity:** Place 1×1 or 3×3 spot meters, Box, Circle, and Polygon ROIs, Line Profiles, and 2-point ΔT comparisons. Apply global or per-ROI emissivity and reflected temperature correction for mixed materials.
- **Time Plotter and TempSwitch alarms:** Plot multi-channel temperature curves over time and trigger automatic captures or recordings when a threshold crosses.
- **Built-in exports (no FFmpeg required):** Export H.264 MP4 (bundled OpenH264), animated GIF, PNG/JPEG with scale bars, temperature CSV, 16-bit Kelvin TIFF, lossless `.DAT` sequences, and HTML/PDF/Markdown reports.
- **Direct SD Card Browser (`9.20 MB/s`):** Browse, preview, download, and delete microSD captures over USB Protocol v2 with no MTP drivers or camera reboots.
- **Built-in Firmware Updater:** Pure-Rust HalfKay HID flasher installs Teensy 4.1 firmware from GitHub Releases or a local `.hex` file with SHA-256 verification.

## Legacy V3.0 Windows and Python Tools

The original tools remain archived at git tag [`3.0.4`](https://github.com/maxritter/diy-thermocam/tree/3.0.4/software). Every `.DAT` file stays compatible across legacy tools and Desktop Studio.

- **ThermoVision by Joe-C (Windows):** Third-party radiometric viewer supporting `.DAT` drag-and-drop editing ([project page](http://joe-c.de/pages/projekte/thermovision.php), [archive](https://github.com/maxritter/diy-thermocam/tree/3.0.4/software/thermal_analysis_software)).
- **Thermal Live Viewer (Python):** Standalone viewer for the legacy v1 USB serial protocol ([archive](https://github.com/maxritter/diy-thermocam/tree/3.0.4/software/thermal_live_viewer)).
- **Thermal Data Viewer (Windows):** Batch converter for `.DAT` folders to JPG, BMP, PNG, or AVI ([archive](https://github.com/maxritter/diy-thermocam/tree/3.0.4/software/thermal_data_viewer)).
- **Video Converter (Windows):** Merges saved frame sequences into video files ([archive](https://github.com/maxritter/diy-thermocam/tree/3.0.4/software/video_converter)).
