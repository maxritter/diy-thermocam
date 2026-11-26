/*
 * WebVTT subtitles for the launch film from the narration's word timings and the scene layout.
 * Cues break at sentence ends and after about 42 characters, using the script's own words.
 *
 *   bun scripts/film-captions.ts [out/diy-thermocam-v4-film.vtt]
 */
import { writeFileSync } from 'node:fs'
import { SCENES } from '../src/film/time'

const out = process.argv[2] ?? 'out/diy-thermocam-v4-film.vtt'
const stamp = (s: number) => {
  const ms = Math.round(s * 1000)
  const h = Math.floor(ms / 3600000)
  const m = Math.floor((ms % 3600000) / 60000)
  const sec = Math.floor((ms % 60000) / 1000)
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(sec).padStart(2, '0')}.${String(ms % 1000).padStart(3, '0')}`
}

const cues: { a: number; b: number; text: string }[] = []
for (const sc of SCENES)
  for (const line of sc.lines) {
    let cur: { a: number; b: number; words: string[] } | null = null
    for (const w of line.words) {
      const at = sc.start + line.at
      if (!cur) cur = { a: at + w.s, b: at + w.e, words: [] }
      cur.words.push(w.w)
      cur.b = at + w.e
      const text = cur.words.join(' ')
      if (/[.:]$/.test(w.w) || text.length > 42) {
        cues.push({ a: cur.a, b: cur.b, text })
        cur = null
      }
    }
    if (cur) cues.push({ a: cur.a, b: cur.b, text: cur.words.join(' ') })
  }

const end = (i: number) => Math.min(Math.max(cues[i].b + 0.25, cues[i].a + 1), cues[i + 1]?.a ?? Infinity)
const vtt = ['WEBVTT', '', ...cues.map((c, i) => `${i + 1}\n${stamp(c.a)} --> ${stamp(end(i))}\n${c.text}\n`)].join('\n')
writeFileSync(out, vtt)
console.log(`wrote ${out}: ${cues.length} cues`)
