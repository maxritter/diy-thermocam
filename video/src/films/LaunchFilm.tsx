import { Audio } from '@remotion/media'
import { AbsoluteFill, interpolate, Sequence, staticFile } from 'remotion'
import { narrationCaptions } from '../captions'
import { StudioBackdrop } from '../components/Backdrop'
import { NarrationCaptions } from '../components/NarrationCaptions'
import { LAUNCH_TIMELINE, MUSIC_LEAD_IN } from '../layoutConfig'
import { FirmwareScene } from '../scenes/FirmwareScene'
import { HardwareScene } from '../scenes/HardwareScene'
import { IntroScene } from '../scenes/IntroScene'
import { OutroScene } from '../scenes/OutroScene'
import { StudioScene } from '../scenes/StudioScene'
import { C, FONT, FPS } from '../theme'

const SPEECH_WINDOWS = LAUNCH_TIMELINE.order.flatMap((id) =>
  LAUNCH_TIMELINE.scenes[id].chunks.map(
    (c) => [LAUNCH_TIMELINE.scenes[id].start + c.from, LAUNCH_TIMELINE.scenes[id].start + c.from + c.frames] as const,
  ),
)

/**
 * Clean ducking envelope: sits warm in intro/outro gaps (~0.24) and ducks smoothly
 * underneath Max's voice (~0.10) so narration stays crystal clear.
 */
function musicVolume(frame: number, totalFrames: number): number {
  let dist = Infinity
  for (const [a, b] of SPEECH_WINDOWS) {
    dist = Math.min(dist, frame < a ? a - frame : frame > b ? frame - b : 0)
  }
  const ducked = interpolate(dist, [0, 14], [0.1, 0.24], {
    extrapolateLeft: 'clamp',
    extrapolateRight: 'clamp',
  })
  const envelope = interpolate(frame, [0, 12, totalFrames - 54, totalFrames], [0, 1, 1, 0], {
    extrapolateLeft: 'clamp',
    extrapolateRight: 'clamp',
  })
  return ducked * envelope
}

export function LaunchFilm() {
  const S = (id: string) => LAUNCH_TIMELINE.scenes[id]
  const total = LAUNCH_TIMELINE.total

  return (
    <AbsoluteFill style={{ background: C.bg, fontFamily: FONT, color: C.text }}>
      <StudioBackdrop
        tones={[
          [0, C.ironOrange],
          [S('hardware').start / FPS, C.ironOrange],
          [S('firmware').start / FPS, C.ironMagenta],
          [S('studio').start / FPS, C.coolBlue],
          [S('outro').start / FPS, C.ironOrange],
        ]}
      />

      {/* Scene 1: Intro / Brand Reveal */}
      <Sequence from={S('intro').start} durationInFrames={S('intro').frames} premountFor={FPS} name="Intro">
        <IntroScene durationSec={S('intro').durationSec} />
        {S('intro').chunks.map((c) => (
          <Sequence key={c.id} from={c.from} durationInFrames={c.frames + 4} premountFor={FPS} name={`VO ${c.id}`}>
            <Audio src={staticFile(c.file)} volume={1} />
          </Sequence>
        ))}
        <NarrationCaptions captions={narrationCaptions(S('intro'))} />
      </Sequence>

      {/* Scene 2: Zero-Solder V4 Hardware & 3D/2D Enclosures */}
      <Sequence from={S('hardware').start} durationInFrames={S('hardware').frames} premountFor={FPS} name="Hardware">
        <HardwareScene durationSec={S('hardware').durationSec} />
        {S('hardware').chunks.map((c) => (
          <Sequence key={c.id} from={c.from} durationInFrames={c.frames + 4} premountFor={FPS} name={`VO ${c.id}`}>
            <Audio src={staticFile(c.file)} volume={1} />
          </Sequence>
        ))}
        <NarrationCaptions captions={narrationCaptions(S('hardware'))} />
      </Sequence>

      {/* Scene 3: Firmware 4.0.0 On-Device UI & Real V3 Performance */}
      <Sequence from={S('firmware').start} durationInFrames={S('firmware').frames} premountFor={FPS} name="Firmware">
        <FirmwareScene durationSec={S('firmware').durationSec} />
        {S('firmware').chunks.map((c) => (
          <Sequence key={c.id} from={c.from} durationInFrames={c.frames + 4} premountFor={FPS} name={`VO ${c.id}`}>
            <Audio src={staticFile(c.file)} volume={1} />
          </Sequence>
        ))}
        <NarrationCaptions captions={narrationCaptions(S('firmware'))} />
      </Sequence>

      {/* Scene 4: Unified Tauri v2 Desktop Studio & Real 1080p Thermal Capture */}
      <Sequence from={S('studio').start} durationInFrames={S('studio').frames} premountFor={FPS} name="Studio">
        <StudioScene durationSec={S('studio').durationSec} />
        {S('studio').chunks.map((c) => (
          <Sequence key={c.id} from={c.from} durationInFrames={c.frames + 4} premountFor={FPS} name={`VO ${c.id}`}>
            <Audio src={staticFile(c.file)} volume={1} />
          </Sequence>
        ))}
        <NarrationCaptions captions={narrationCaptions(S('studio'))} />
      </Sequence>

      {/* Scene 5: Outro */}
      <Sequence from={S('outro').start} durationInFrames={S('outro').frames} premountFor={FPS} name="Outro">
        <OutroScene durationSec={S('outro').durationSec} />
        {S('outro').chunks.map((c) => (
          <Sequence key={c.id} from={c.from} durationInFrames={c.frames + 4} premountFor={FPS} name={`VO ${c.id}`}>
            <Audio src={staticFile(c.file)} volume={1} />
          </Sequence>
        ))}
        <NarrationCaptions captions={narrationCaptions(S('outro'))} />
      </Sequence>

      {/* ElevenLabs Music ambient electronic underscore, trimmed past its lead-in fade and ducked beneath voice */}
      <Sequence from={0} durationInFrames={total} premountFor={FPS} name="Music">
        <Audio
          src={staticFile('audio/launch-music.mp3')}
          trimBefore={MUSIC_LEAD_IN * FPS}
          volume={(f) => musicVolume(f, total)}
        />
      </Sequence>
    </AbsoluteFill>
  )
}
