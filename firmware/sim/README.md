# Host Firmware UI Simulator

Compiles the real DIY-Thermocam Firmware 4.0 C++ UI, fonts, HUD, menus, gallery loader, and colormaps natively on macOS and Linux in under a second. Renders to a 320×240 RGB565 software framebuffer (`sim_display.cpp` + `sim_stubs.cpp`) and exports 2× (640×480) PNG screenshots without hardware.

## Build, test, and export

```sh
cd firmware/sim
make test         # Run 18 interactive UI, XPT2046 touch-glitch, and redraw-flicker regression tests
make screenshots  # Export 18 2x (640x480) PNG screenshots to out/
```

Outputs in `out/`:
- `01-boot-splash.png` — Boot splash screen
- `02-live-hud.png` — Live HUD (simulated PCB thermal inspection, Glowbow)
- `02b-live-building.png` — Live HUD (simulated window draft & radiator, Rainbow)
- `02c-live-breaker.png` — Live HUD (simulated electrical breaker hotspot, Ironblack)
- `02d-live-floor.png` — Live HUD (simulated underfloor heating loops, Lava)
- `03-live-recording.png` — Live video recording HUD (`REC` timer and FPS)
- `04-main-menu-dark.png`, `04b-main-menu-light.png`, `04e-main-menu-page2-dark.png` — Main Menu (Pages 1 & 2, Dark & Light)
- `04c-settings-dark.png`, `04d-theme-light.png`, `04f-overlays-grid-dark.png` — Settings, Theme Selector, Overlays 3×3 Grid
- `05-palette-picker.png` — Visual Palette Picker with live firmware LUT gradient bars
- `06-font-sheet.png` — `8×12` and `16×16` monospace font sheet
- `07-video-mode.png` — Video Recording chooser
- `08-gallery.png`, `08b-gallery-viewer.png` — Gallery chooser and image viewer HUD
- `09-status-screen.png` — System Status telemetry screen
