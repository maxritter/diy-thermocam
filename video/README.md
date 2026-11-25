# DIY-Thermocam V4 Launch & 3D Assembly Showcase Video

Remotion source (`1920×1080 @ 30 fps`, H.264 + AAC) for the DIY-Thermocam V4 launch film.

## Commands

```sh
cd video
bun install
bun run dev       # Open interactive Remotion Studio preview
bun run check     # TypeScript check
bun run vo        # Synthesize / align narration chunks (reuses unchanged cached takes)
bun run music     # Compose scene-matched ElevenLabs Music underscore
bun run render    # Export out/diy-thermocam-v4-launch.mp4
```

## Real Source Assets (Zero Fake UI Mockups)

- **Brand mark:** Version 1 Ironbow Thermal Lens (`website/public/images/brand/logo-mark.svg`)
- **PCB renders:** KiCad 10 V4.0 top & bottom (`hardware/pcb/kicad-v4.0/fab/diy-thermocam-v4.0-{top,bottom}.png`)
- **Enclosures & assembly:** `build123d` 3D hero, 6-plate laser-cut preview, and 4-step screwdriver-only assembly renders (`hardware/enclosure/renders/`)
- **Firmware 4.0.0 UI:** 2× pixel-exact 640×480 simulator captures (`01-boot-splash`, `02-live-hud`, `04-main-menu-dark`, `04b-main-menu-light`, `05-palette-picker`)
- **Desktop Studio & Real Thermal Capture:** Tauri v2 Studio dark/light screenshots + real 1920×1080 V3 radiometric capture (`website/public/images/thermal/real-v3-1080p-capture.png`)

## Audio & Budget Accounting

- **Voiceover:** Max Ritter personal ElevenLabs voice clone (`eleven_v4`, stability `0.5`, similarity `0.75`), 5 chunks (59.0 s speech, 296 credits).
- **Underscore:** ElevenLabs Music (`music_v1`), scene-matched ambient electronic score (68.4 s including 4 s lead-in, 855 credits), loudness-normalized (`-18 LUFS`) and ducked smoothly beneath narration.
- **Total ElevenLabs credits:** 1,151 credits (tracked in `media-budget.json`, ~$0.35 USD equivalent under a $2.00 cap).
