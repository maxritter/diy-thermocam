import { FPS } from './theme'

export type WordTiming = { w: string; s: number; e: number }

export type Chunk = {
  id: string
  scene: string
  gap: number | null
  file: string
  duration: number
  words: WordTiming[]
}

export type PlacedChunk = Chunk & { from: number; frames: number }

export type SceneTiming = {
  id: string
  start: number
  frames: number
  durationSec: number
  chunks: PlacedChunk[]
}

export const OVERLAP = 12

const norm = (s: string) => s.toLowerCase().replace(/[^a-z0-9'.]/g, '')

export function layout(
  manifest: Chunk[],
  opts: {
    lead?: Record<string, number>
    tail?: Record<string, number>
    gap?: number
  } = {},
) {
  const order: string[] = []
  for (const c of manifest) if (!order.includes(c.scene)) order.push(c.scene)
  const scenes: Record<string, SceneTiming> = {}
  let at = 0
  for (const id of order) {
    let t = opts.lead?.[id] ?? 0.45
    const chunks: PlacedChunk[] = []
    for (const c of manifest.filter((x) => x.scene === id)) {
      if (chunks.length) t += c.gap ?? opts.gap ?? 0.3
      chunks.push({ ...c, from: Math.round(t * FPS), frames: Math.ceil(c.duration * FPS) })
      t += c.duration
    }
    const durationSec = t + (opts.tail?.[id] ?? 0.7)
    const frames = Math.round(durationSec * FPS) + OVERLAP
    scenes[id] = { id, start: at, frames, durationSec: frames / FPS, chunks }
    at += frames - OVERLAP
  }
  const total = at + OVERLAP

  function wordSec(sceneId: string, needle: string, nth = 0): number {
    const hits: number[] = []
    const target = norm(needle)
    for (const c of scenes[sceneId].chunks) {
      for (const x of c.words) {
        if (norm(x.w) === target) hits.push(c.from / FPS + x.s)
      }
    }
    const hit = hits[nth]
    if (hit === undefined) {
      return (scenes[sceneId].chunks[0]?.from ?? 0) / FPS
    }
    return hit
  }

  return { order, scenes, total, wordSec }
}
