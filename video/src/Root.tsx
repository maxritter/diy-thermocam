import { Composition } from 'remotion'
import { LaunchFilm } from './films/LaunchFilm'
import { LAUNCH_TIMELINE } from './layoutConfig'
import { FPS, H, W } from './theme'

export function RemotionRoot() {
  return (
    <Composition
      id="DiyThermocamV4Launch"
      component={LaunchFilm}
      durationInFrames={LAUNCH_TIMELINE.total}
      fps={FPS}
      width={W}
      height={H}
    />
  )
}
