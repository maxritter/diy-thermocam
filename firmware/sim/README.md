# Host Firmware UI Simulator

Compiles the real DIY-Thermocam Firmware 4.0 C++ UI, fonts, HUD, menus, and colormaps natively on macOS and Linux in under a second. Renders to a 320×240 RGB565 software framebuffer (`sim_display.cpp` + `sim_stubs.cpp`) and exports 2× (640×480) PNG screenshots without hardware.

## Build and Export

```sh
cd firmware/sim
make screenshots
```

Outputs in `out/`:
- `01-boot-splash.png` — Boot splash screen
- `02-live-hud.png` — Double-buffered live HUD with colorbar and spot readout
- `03-live-recording.png` — Live HUD during video recording (`REC` timer and FPS)
- `04-main-menu-dark.png` — Dark theme main menu
- `04b-main-menu-light.png` — Light theme main menu
- `05-palette-picker.png` — Palette picker with firmware LUT previews
- `06-font-sheet.png` — `8×12` and `16×16` monospace font sheet
