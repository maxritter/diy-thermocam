# DIY-Thermocam V4 launch film

Remotion project for the launch film on the website and in the README: composition `DiyThermocamFilm`, 1920×1080, 30 fps, about 2:15, H.264 + AAC. Source in `src/film/`.

## Where the pictures and sounds come from

Every picture comes from the project itself; nothing is stock footage.

- **3D shots** (`blender/film_shots.py`): Blender renders from the enclosure's own scene (`hardware/enclosure/cad/build/scene/`, the same models, materials and studio lights as the still renders), animated frame by frame. The shots are: the pull-back from the screen, the orbit to the Lepton with the lens cap sliding open, the assembly onto the finished board, both cases turning, the boot push-in, and the camera opening into its layers at the end. The display shows frames from the firmware simulator. Frames have alpha, so the film draws its own stage behind them. `--project` writes where each part lands on screen per frame (`src/film/data/*.json`), so the labels follow the parts.
- **Firmware screens** (`sim/film.cpp`): the real Firmware 4.0 GUI, fonts, HUD, menus and colour maps, compiled on the host against `firmware/sim/`. Animated thermal scenes play at the sensor's 9 fps: coffee poured into a cold mug, a steaming mug, a board warming up, an overloaded breaker, a draughty window and underfloor heating. The simulator also writes the hottest and coldest positions per frame (`hud.json`), the 19 palettes (`palettes.json`) and one frame's 160 × 120 temperatures (`grid-mug.json`) for the pixel-grid shot.
- **Studio**: screenshots of DIY-Thermocam Studio in `public/film/`. The live image inside the window is the simulator's mug.
- **Voice**: Max's ElevenLabs voice clone (eleven_v4). Each line of `vo/film.json` is recorded four times. Gemini Neon listens to the takes and keeps the one with the clearest names and the calmest delivery (`scripts/vo-takes.py`). A line's optional `say` respells a name for the voice only (FLIR as "Fleer"). Word timings come from local faster-whisper. Scenes are laid out around the takes (`src/film/time.ts`), and every motion and sound is pinned to a spoken word with `word(line, text)`.
- **Score and sounds**: ElevenLabs Music, one section per act, matched to the scene lengths (`scripts/film-music.ts`). The physical sounds (pour, module clicks, screws, switch, cable, ticks) come from ElevenLabs Sound Effects (`scripts/film-sfx.py`, `public/sfx/film/`).

## Rebuild

```sh
cd video
bun install
bun run film:frames     # firmware frames into public/sim/ (needs clang and zlib)
bun run film:shots      # 3D shots into work/blender/ (resumable; about 1-2 hours on Apple silicon)
bun run film:points     # where the parts land on screen, for the labels
bun run film:takes      # narration: four takes per new line, Gemini Neon keeps the best (paid)
bun run film:vo         # word timings for the kept takes
bun run film:music      # score (paid; keeps an existing take unless --force)
bun run film:sfx        # sound effects (paid; keeps existing ones)
bun run check
bun run film:render     # out/diy-thermocam-v4-film.mp4 (mastered to -14 LUFS), .vtt, and out/web/ (website copy and poster)
```

Git keeps the sources only. Generated and paid media (`public/sim/`, `public/vo/`, `public/audio/`, `work/`, `out/`) stay out of the repository; the commands above rebuild them. A new recording sounds slightly different from the published one.

## Facts on screen

Specifications come from `README.md`: FLIR Lepton 3.5 with 160 × 120 pixels, −10 to +450 °C, Teensy 4.1, 3.2-inch touchscreen, 19 palettes, three themes, up to 96 measuring spots, 4 to 6 hours on a 2000 mAh LiPo, and about $293 in parts with the sensor ($121 without). V4 hardware has not been built yet: the film shows renders and the simulator, not footage.

## License

PolyForm Noncommercial 1.0.0 (see `LICENSE`).
