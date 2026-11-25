import type { Caption } from '@remotion/captions'
import type { SceneTiming } from './timeline'
import { FPS } from './theme'

export type NarrationCaption = Caption & { chunkId: string }

export function narrationCaptions(scene: SceneTiming): NarrationCaption[] {
  const pages: NarrationCaption[] = []
  for (const chunk of scene.chunks) {
    const groups: (typeof chunk.words)[] = []
    let words: typeof chunk.words = []
    const flush = () => {
      if (!words.length) return
      groups.push(words)
      words = []
    }
    for (const word of chunk.words) {
      const text = [...words, word].map((w) => w.w).join(' ')
      if (words.length && (text.length > 74 || word.e - words[0].s > 4.6)) flush()
      words.push(word)
      if (/[.!?]$/.test(word.w)) flush()
    }
    flush()
    const isShort = (page: typeof words) => page.length < 3 || page[page.length - 1].e - page[0].s < 0.9
    for (let i = 1; i < groups.length; i++) {
      const curr = groups[i]
      if (!isShort(curr)) continue
      const prev = groups[i - 1]
      const joined = [...prev, ...curr]
      if (joined.map((w) => w.w).join(' ').length <= 78 && joined[joined.length - 1].e - joined[0].s <= 5.0) {
        groups.splice(i - 1, 2, joined)
        i--
      }
    }
    for (const grp of groups) {
      pages.push({
        chunkId: chunk.id,
        text: grp.map((w) => w.w).join(' '),
        startMs: (chunk.from / FPS + grp[0].s) * 1000,
        endMs: (chunk.from / FPS + grp[grp.length - 1].e + 0.12) * 1000,
        timestampMs: null,
        confidence: null,
      })
    }
  }
  return pages
}
