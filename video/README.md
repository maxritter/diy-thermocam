# DIY-Thermocam V4 Launch Video

Remotion project (`1920×1080`, 30 fps, H.264 + AAC) for the DIY-Thermocam V4 launch video.

## Commands

```sh
cd video
bun install
bun run dev       # Interactive Remotion Studio preview
bun run check     # TypeScript check
bun run vo        # Synthesize and align narration (reuses cached takes)
bun run music     # Compose background underscore
bun run render    # Render out/diy-thermocam-v4-launch.mp4
```

## Source assets

- **Brand mark:** `website/public/images/brand/logo-mark.svg`
- **PCB renders:** `hardware/pcb/fab/diy-thermocam-v4.0-{top,bottom}.png`
- **Enclosures:** `hardware/enclosure/renders/`
- **Firmware 4.0.0 UI:** 640×480 simulator captures (`firmware/sim/out/`)
- **Desktop Studio and thermal capture:** Studio screenshots and `website/public/images/thermal/real-v3-1080p-capture.png`
