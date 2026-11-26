// The film's clock: scenes laid end to end around the recorded narration (src/vo/film.json).
// Everything is in seconds; components convert with FPS.
import vo from '../vo/film.json'

export const FPS = 30

type Word = { w: string; s: number; e: number }
type Chunk = { id: string; scene: string; file: string; duration: number; words: Word[] }
const chunks = vo as Chunk[]

// Per scene: seconds before the first line, after the last and between lines, and fixed lengths
// where a rendered 3D shot or the simulator sets the pace. Each line's scene comes from vo/film.json.
type Spec = { id: string; lead: number; tail: number; gap?: number; length?: number; dark?: boolean }

const SPECS: Spec[] = [
  { id: 'hook', lead: 1.2, tail: 0, length: 6.2, dark: true },
  { id: 'open', lead: 1.2, tail: 1.0 },
  { id: 'sensor', lead: 0.5, tail: 1.6, gap: 1.2 },
  { id: 'assemble', lead: 0.6, tail: 2.2, gap: 1.2 },
  { id: 'cases', lead: 0.5, tail: 1.4 },
  { id: 'boot', lead: 0.6, tail: 0, length: 6.0 },
  { id: 'live', lead: 0.3, tail: 1.3, dark: true },
  { id: 'palettes', lead: 2.8, tail: 1.6, dark: true },
  { id: 'menu', lead: 0.5, tail: 1.2, dark: true },
  { id: 'uses', lead: 0.4, tail: 1.4, dark: true },
  { id: 'battery', lead: 0.5, tail: 1.2 },
  { id: 'studio', lead: 0.7, tail: 1.4, gap: 1.0 },
  { id: 'price', lead: 0.7, tail: 2.0 },
  { id: 'end', lead: 0.8, tail: 3.4, gap: 1.4 },
]

export type Line = Chunk & { at: number } // at: seconds from the start of its scene
export type Scene = { id: string; start: number; length: number; dark: boolean; lines: Line[] }

export const SCENES: Scene[] = []
{
  let start = 0
  for (const spec of SPECS) {
    const own = chunks.filter((c) => c.scene === spec.id)
    let at = spec.lead
    const lines: Line[] = own.map((c) => {
      const placed = at
      at = placed + c.duration + (spec.gap ?? 0.9)
      return { ...c, at: placed }
    })
    const last = lines[lines.length - 1]
    const natural = last.at + last.duration + spec.tail
    const length = spec.length ?? natural
    SCENES.push({ id: spec.id, start, length, dark: !!spec.dark, lines })
    start += length
  }
}

export const TOTAL = SCENES.reduce((n, s) => n + s.length, 0)
export const scene = (id: string) => SCENES.find((s) => s.id === id)!

const clean = (s: string) => s.toLowerCase().replace(/[^a-z0-9.]/g, '')

/** Seconds from the scene's start at which `word` (n-th match, from 0) is spoken in line `lineId`. */
export function word(lineId: string, text: string, nth = 0, edge: 's' | 'e' = 's'): number {
  for (const sc of SCENES)
    for (const l of sc.lines)
      if (l.id === lineId) {
        const want = clean(text)
        const hits = l.words.filter((w) => clean(w.w).startsWith(want))
        if (hits.length <= nth) throw new Error(`"${text}" not found in ${lineId}`)
        return l.at + hits[nth][edge]
      }
  throw new Error(`no line ${lineId}`)
}

/** Seconds from the scene's start at which its line begins. */
export const lineAt = (lineId: string) => {
  for (const sc of SCENES) for (const l of sc.lines) if (l.id === lineId) return l.at
  throw new Error(`no line ${lineId}`)
}
