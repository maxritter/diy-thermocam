# app/AGENTS.md

Guide for AI coding agents working inside `app/` — the unified 2026 **DIY-Thermocam Studio** cross-platform desktop app (Tauri v2 + Rust + React 19 + Tailwind v4 + Base UI).

## Architecture

| Path | Responsibility |
|---|---|
| `crates/thermocam-core/` | Pure-Rust headless engine: USB Protocol v2 client & resync parser (verified against all 6 golden vectors in `docs/protocol-v2.md`), build-time single-source palette codegen (`build.rs` reads `firmware/3.0/src/general/colorschemes.cpp`, `globaldefines.h`, `thermal.cpp`), V1/V2/V3 `.DAT` + `.thermo.json` sidecar, Stefan-Boltzmann emissivity inversion & material presets, Spot/Box/Circle/Polygon/Line/ΔT ROIs, TempSwitch alarms, **Full HD / 1440p / 4K Lanczos-3 & Edge-Preserving Unsharp Mask upscaling**, native-resolution vector HUD compositor, **zero-FFmpeg H.264 MP4 (`openh264` + `mp4`)**, GIF, 16-bit Kelvin TIFF, CSV, HTML/Markdown inspection reports, and native Rust Teensy 4.1 HalfKay HID firmware updater (`hidapi` + `ihex` + `sha2` + 134-baud legacy fallback). |
| `src-tauri/` | Tauri v2 desktop shell (`diy-thermocam`): raw binary frame streaming over `Channel<InvokeResponseBody::Raw>`, `thermo://` URI scheme for SD thumbnails, native file dialogs via `rfd`, `withGlobalTauri: false`, explicit capabilities in `capabilities/default.json`. |
| `src/` | Desktop-first React 19 + Tailwind v4 + `@base-ui/react` + Geist Studio UI: WebGL2 60 fps thermal viewport (with automatic Canvas 2D fallback) + 7×7 zoomed pixel-grid loupe + draggable level/span colorbar + 256-bin histogram + all 8 Studio modes (Live View, Measurements & Time Plotter, TempSwitch Alarms, Export & Video, SD Card Browser, Radiometric Editor, Camera Control & Telemetry, In-App Firmware Updater). |

## Quick Commands

Always ensure Rust and Bun are on `PATH`:
```bash
export PATH="$HOME/.cargo/bin:$HOME/.bun/bin:$PATH"
```

- **Rust unit & integration tests:** `cd app && cargo test --workspace`
- **Rust lint (zero warnings):** `cd app && cargo clippy --workspace --all-targets -- -D warnings`
- **Rust Tauri binary build:** `cd app && cargo build -p diy-thermocam-app`
- **Frontend dependencies:** `cd app && bun install`
- **Frontend lint (Biome):** `cd app && bun run lint`
- **Frontend typecheck:** `cd app && bun run typecheck`
- **Frontend unit tests:** `cd app && bun test`
- **Frontend production build:** `cd app && bun run build`
- **Browser preview (Simulator mode):** `cd app && bun run dev` (or `bun run preview`)

## Invariants

- **Single-source palettes:** Never duplicate `firmware/3.0/src/general/colorschemes.cpp` LUT bytes manually in Rust; `crates/thermocam-core/build.rs` parses the firmware sources at compile time.
- **Measurement-safe upscaling:** Nearest, Bicubic, Lanczos-3, and Edge-Preserving Unsharp Mask run strictly on display buffers. Every ROI statistic, isotherm threshold, CSV cell, and 16-bit Kelvin TIFF pixel derives directly from the raw 16-bit sensor counts.
- **Zero external media binaries:** H.264 MP4 (`openh264` + `mp4`), GIF (`gif`), PNG/JPEG (`image`), and 16-bit TIFF (`tiff`) encode completely in-process with zero FFmpeg/ImageMagick runtime dependency.
- **Hardware safety:** Never open real serial ports (`/dev/cu.usbmodem*`, `/dev/ttyACM*`, `COM*`) or real HalfKay HID devices during automated tests; use `SimulatorTransport`, `MockTransport`, and `MockHalfKayDevice`.
