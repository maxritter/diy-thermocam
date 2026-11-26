# Desktop App and Software

**DIY-Thermocam Desktop Studio** combines live viewing, radiometric analysis, video export, direct microSD access, and firmware updates in one open-source app for **macOS, Windows, and Linux**.

![DIY-Thermocam Desktop Studio Pro workbench showing live thermal stream, ROI measurements, 7x7 pixel loupe, histogram, and radiometry inspector](/images/studio/studio-live-pro.png)

## Supported systems

Download prebuilt installers from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases).

| Operating System | Architecture | Prerequisites |
| --- | --- | --- |
| **macOS 12+** | Apple Silicon (`arm64`) & Intel (`x64`) | None (USB serial & HalfKay HID work out of the box) |
| **Windows 10 / 11** | `x64` | None (WebView2 & USB CDC/HID drivers are built in) |
| **Linux** | `x86_64` & `ARM64` | Install [PJRC `00-teensy.rules`](https://www.pjrc.com/teensy/00-teensy.rules) into `/etc/udev/rules.d/` for non-root USB access and firmware updates |

## How Studio works

Built with [Tauri v2](https://github.com/tauri-apps/tauri), Rust (`thermocam-core`), React 19, and WebGL2, Desktop Studio talks to the camera over [USB Protocol v2](/docs/protocol-v2/). On real Teensy 4.1 + Lepton 3.5 hardware, it sustains **8.77 fps** live streaming, **0.26 ms** median command latency, and **9.20 MB/s** direct microSD reads. `.DAT` captures from every DIY-Thermocam camera open natively.

| Live view | Editing a saved capture | SD card browser |
| --- | --- | --- |
| ![Desktop Studio Standard Live View](/images/studio/studio-live-standard.png) | ![Desktop Studio offline .DAT radiometric editor](/images/studio/studio-editor.png) | ![Desktop Studio microSD browser at 9.2 MB/s](/images/studio/studio-sd-browser.png) |

### What it does

- **WebGL2 Live View:** Auto-detects connected cameras and renders all 19 firmware palettes plus `Inferno`, `Viridis`, `Magma`, `Plasma`, `Turbo`, and custom 256-pixel LUTs.
- **Standard and Pro modes:** Use **Standard** mode for clean live viewing, palette switches, and quick exports. Press `Shift+P` for **Pro** mode with draggable isotherms, live 256-bin histograms, multi-ROI measurements, and time-series logging.
- **Measurements and emissivity:** Place 1×1 or 3×3 spot meters, Box, Circle, and Polygon ROIs, Line Profiles, and 2-point ΔT comparisons. Apply global or per-ROI emissivity and reflected temperature compensation.
- **Time Plotter and TempSwitch alarms:** Plot multi-channel temperature curves over time and trigger automatic captures or recordings when a threshold crosses.
- **Built-in exports:** Export H.264 MP4 via bundled [OpenH264](https://github.com/cisco/openh264) (no FFmpeg install needed), animated GIF, PNG/JPEG with scale bars, temperature CSV, 16-bit Kelvin TIFF, lossless `.DAT` sequences, and HTML/Markdown reports.
- **Direct SD Card Browser (`9.20 MB/s`):** Browse, preview, download, and delete microSD captures over USB Protocol v2 without MTP drivers or camera reboots.
- **Built-in Firmware Updater:** Pure-Rust HalfKay HID flasher installs Teensy 4.1 `.hex` images from a local file with SHA-256 verification.
