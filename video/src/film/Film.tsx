// DIY-Thermocam V4: the launch film. Scenes end to end on one light studio stage (dark while the
// camera's own screen fills the frame), Max's narration, the score and quiet, physical sounds.
import { Audio } from '@remotion/media'
import type { ReactNode } from 'react'
import { AbsoluteFill, interpolate, Sequence, staticFile, useCurrentFrame } from 'remotion'
import { Brand, INOUT, ramp, Stage } from './kit'
import {
  Assemble, Battery, Boot, Cases, End, endLands, Hook, Live, Menu, menuTimes, Open, paletteTimes, Palettes, Price,
  priceLands, Sensor, Studio, studioTimes, Uses, useTimes,
} from './scenes'
import { FPS, scene, SCENES, TOTAL, word } from './time'

const VIEWS: Record<string, () => ReactNode> = {
  hook: Hook, open: Open, sensor: Sensor, assemble: Assemble, cases: Cases, boot: Boot, live: Live, palettes: Palettes,
  menu: Menu, uses: Uses, battery: Battery, studio: Studio, price: Price, end: End,
}

// Scene joins that cut straight through (the picture continues), not through a short fade
const MATCHED_IN = new Set(['open', 'live'])
const MATCHED_OUT = new Set(['hook', 'boot'])

const OPEN = scene('open').start

/** 1 while the Lepton's pixel grid fills the frame (sensor scene, after the push through the lens). */
function onGrid(t: number) {
  const sn = scene('sensor')
  const grid = sn.start + word('sensor-2', 'pixels', 0, 'e') + 0.2
  return Math.min(ramp(t, grid - 0.15, grid + 0.3), 1 - ramp(t, sn.start + sn.length - 0.3, sn.start + sn.length + 0.1))
}

/** How dark the stage is at film time t: the hook, the open's first two seconds and the dark scenes. */
function darkness(t: number) {
  let d = t < OPEN + 3 ? 1 - ramp(t, OPEN + 2.0, OPEN + 2.6, INOUT) : 0
  d = Math.max(d, onGrid(t))
  for (const s of SCENES) {
    if (!s.dark) continue
    const a = s.start
    const b = s.start + s.length
    const next = SCENES[SCENES.indexOf(s) + 1]
    const prev = SCENES[SCENES.indexOf(s) - 1]
    const fadeIn = prev?.dark ? 1 : ramp(t, a - 0.25, a + 0.15, INOUT)
    const fadeOut = next?.dark ? 1 : 1 - ramp(t, b - 0.15, b + 0.25, INOUT)
    if (t >= a - 0.3 && t <= b + 0.3) d = Math.max(d, Math.min(fadeIn, fadeOut))
  }
  return d
}

// Sounds: [scene, seconds into it, file, volume, playback rate]
type Cue = [string, number, string, number, number?]
function cues(): Cue[] {
  const c: Cue[] = [['hook', 0.35, 'pour', 0.55]]
  const sensorPush = word('sensor-2', 'pixels', 0, 'e') + 0.2
  c.push(['sensor', (4.4 * sensorPush) / 11.2, 'cap', 0.45])
  c.push(['sensor', sensorPush - 0.35, 'soft-whoosh', 0.35])
  for (const w of ['Lepton', 'Teensy', 'battery', 'touchscreen'] as const) c.push(['assemble', word('stack-1', w) + 0.45, 'seat', 0.5])
  const screws = word('stack-1', 'shell', 0, 'e') + 0.35
  for (let i = 0; i < 4; i++) c.push(['assemble', screws + i * 0.22, 'screw', 0.32, 0.94 + (i % 2) * 0.08])
  c.push(['boot', 0.75, 'switch', 0.55], ['boot', 0.95, 'hum', 0.3])
  const pal = paletteTimes()
  pal.at.forEach((a, k) => c.push(['palettes', a, 'tick', 0.07, 0.85 + k * 0.03]))
  const m = menuTimes()
  c.push(['menu', m.range, 'tick', 0.3], ['menu', m.spots - 0.1, 'tick', 0.3], ['menu', m.photos - 0.35, 'tick', 0.22], ['menu', m.video + 0.15, 'shutter', 0.16])
  for (const a of useTimes()) c.push(['uses', a + 0.35, 'tick', 0.25, 0.8])
  const st = studioTimes()
  c.push(['studio', st.cable + 0.05, 'cable', 0.55], ['studio', st.regions + 0.7, 'tick', 0.25], ['studio', st.alarms + 0.8, 'tick', 0.25], ['studio', st.exp + 0.05, 'shutter', 0.3])
  c.push(['price', priceLands(), 'tick', 0.2, 0.8])
  c.push(['end', 0.9, 'seat', 0.25])
  return c
}
const CUES = cues()

export const FILM_FRAMES = Math.ceil(TOTAL * FPS)
export const MUSIC = 'audio/film-music.mp3'
export const HAS_MUSIC = true
export const MUSIC_START = OPEN - 1.4 // enters as the first line ends

export function Film() {
  const frame = useCurrentFrame()
  const t = frame / FPS
  const dark = darkness(t)
  const end = scene('end')
  return (
    <AbsoluteFill>
      <Stage dark={dark}>
        {SCENES.map((s) => {
          const View = VIEWS[s.id]
          const from = Math.round(s.start * FPS)
          const dur = Math.round(s.length * FPS)
          const local = t - s.start
          const fin = MATCHED_IN.has(s.id) ? 1 : ramp(local, 0, 0.35)
          const fout = MATCHED_OUT.has(s.id) ? 1 : 1 - ramp(local, s.length - 0.3, s.length, INOUT)
          return (
            <Sequence key={s.id} name={s.id} from={from} durationInFrames={dur} premountFor={FPS}>
              <AbsoluteFill style={{ opacity: Math.min(fin, fout) }}>
                <View />
              </AbsoluteFill>
            </Sequence>
          )
        })}
        <Brand dark={dark} show={ramp(t, OPEN + 3.4, OPEN + 4.2) * (1 - onGrid(t)) * (1 - ramp(t, end.start + endLands() - 0.4, end.start + endLands() + 0.4))} />
      </Stage>

      {/* Narration */}
      {SCENES.flatMap((s) =>
        s.lines.map((l) => (
          <Sequence key={l.id} name={`vo ${l.id}`} from={Math.round((s.start + l.at) * FPS)} durationInFrames={Math.ceil((l.duration + 0.3) * FPS)} premountFor={FPS}>
            <Audio src={staticFile(l.file)} />
          </Sequence>
        )),
      )}

      {/* Score: enters with the camera, quiet under the voice, up a little at the end */}
      {HAS_MUSIC ? (
        <Sequence name="music" from={Math.round(MUSIC_START * FPS)}>
          <Audio
            src={staticFile(MUSIC)}
            volume={(f) => {
              const s = f / FPS + MUSIC_START
              return interpolate(s, [MUSIC_START, MUSIC_START + 1.2, TOTAL - 5, TOTAL - 0.2], [0, 0.34, 0.34, 0], {
                extrapolateLeft: 'clamp',
                extrapolateRight: 'clamp',
              })
            }}
          />
        </Sequence>
      ) : null}

      {/* Turns between the light and the dark stage */}
      {SCENES.filter((s, i) => i > 1 && s.dark !== SCENES[i - 1].dark).map((s) => (
        <Sequence key={`whoosh-${s.id}`} name={`whoosh ${s.id}`} from={Math.max(0, Math.round((s.start - 0.3) * FPS))} durationInFrames={2 * FPS}>
          <Audio src={staticFile('sfx/soft-whoosh.mp3')} volume={0.17} />
        </Sequence>
      ))}
      <Sequence name="whoosh open" from={Math.round((OPEN + 1.8) * FPS)} durationInFrames={2 * FPS}>
        <Audio src={staticFile('sfx/soft-whoosh.mp3')} volume={0.32} />
      </Sequence>
      {CUES.map(([sceneId, at, file, vol, rate], i) => (
        <Sequence key={`cue-${i}`} name={`${file} ${sceneId}`} from={Math.max(0, Math.round((scene(sceneId).start + at) * FPS))} durationInFrames={Math.round((file === 'pour' ? 6 : 2) * FPS)}>
          <Audio src={staticFile(file === 'soft-whoosh' ? 'sfx/soft-whoosh.mp3' : `sfx/film/${file}.mp3`)} volume={vol} playbackRate={rate ?? 1} />
        </Sequence>
      ))}
      <Sequence name="mark end" from={Math.round((end.start + word('end-2', 'diy-thermocam.net', 0, 'e') + 0.15) * FPS)} durationInFrames={3 * FPS}>
        <Audio src={staticFile('sfx/mark-tone.mp3')} volume={0.4} />
      </Sequence>
    </AbsoluteFill>
  )
}
