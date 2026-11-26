import { Composition } from 'remotion'
import { Film, FILM_FRAMES } from './film/Film'
import { FPS } from './film/time'

export function RemotionRoot() {
  return <Composition id="DiyThermocamFilm" component={Film} durationInFrames={FILM_FRAMES} fps={FPS} width={1920} height={1080} />
}
