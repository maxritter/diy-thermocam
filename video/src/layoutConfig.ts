import manifest from './vo/launch.json'
import { layout, type Chunk } from './timeline'

export const SCENE_OPTS = {
  lead: {
    intro: 0.35,
    hardware: 0.25,
    firmware: 0.25,
    studio: 0.25,
    outro: 0.2,
  },
  tail: {
    intro: 0.35,
    hardware: 0.45,
    firmware: 0.45,
    studio: 1.0,
    outro: 1.35,
  },
}

export const MUSIC_LEAD_IN = 4

export const LAUNCH_TIMELINE = layout(manifest as Chunk[], SCENE_OPTS)

export const SCENE_STARTS = LAUNCH_TIMELINE.order.map((id) => ({
  id,
  start: LAUNCH_TIMELINE.scenes[id].start / 30,
  length: LAUNCH_TIMELINE.scenes[id].frames / 30,
}))
