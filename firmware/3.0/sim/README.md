# Native Host Firmware UI & Framebuffer Simulator

Compiles the **real DIY-Thermocam Firmware 4.0.0 C++ UI, fonts, HUD, menus, and colormaps** natively on macOS/Linux in `<1 s` against a pixel-exact 320×240 RGB565 software framebuffer HAL (`sim_display.cpp` + `sim_stubs.cpp`), exporting crisp 2× (640×480) PNG screenshots without flashing hardware.

## Build & Export All Screens

```sh
cd firmware/3.0/sim
make screenshots
```

Outputs in `out/`:
- `01-boot-splash.png` — Version 1 Ironbow Thermal Lens non-military splash
- `02-live-hud.png` — RAM-composited zero-flicker live HUD + colorbar + spot pill
- `03-live-recording.png` — Live HUD with `REC 00:16` & `142 FR | 8.7 FPS` pills
- `04-main-menu-dark.png` — 2026 Dark flat-card main menu (`#0B0D12` / `#161A23` / `#F36D21`)
- `04b-main-menu-light.png` — Daylight Light main menu (`#F8FAFC` / `#FFFFFF` / `#0F172A` / `#E05A14`)
- `05-palette-picker.png` — Visual palette picker with real firmware LUT gradient bars
- `06-font-sheet.png` — Full `8×12` & `16×16` monospace glyph verification sheet
