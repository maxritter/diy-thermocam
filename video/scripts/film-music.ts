/*
 * Composes the launch film's score with the ElevenLabs Music API, one section per act, matched to the
 * scene lengths in src/film/time.ts. Paid: run with --dry-run first; an existing take is kept unless --force.
 *
 *   bun scripts/film-music.ts [--dry-run] [--force]
 */
import { execFileSync } from 'node:child_process'
import { existsSync, readFileSync, writeFileSync } from 'node:fs'
import { homedir } from 'node:os'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { SCENES, TOTAL } from '../src/film/time'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const out = join(root, 'public', 'audio', 'film-music.mp3')
const dryRun = process.argv.includes('--dry-run')
const force = process.argv.includes('--force')

// The score enters with the camera (Film.tsx MUSIC_START); the hook before it has only the pour.
const ACTS: { name: string; scenes: string[]; pos: string[]; neg?: string[] }[] = [
  { name: 'Reveal', scenes: ['open'], pos: ['warm soft analog pad', 'a simple felt piano motif enters', 'intimate and curious', 'no percussion'] },
  {
    name: 'Build',
    scenes: ['sensor', 'assemble', 'cases'],
    pos: ['felt piano motif continues', 'a soft muted pulse joins', 'tiny clicky percussion, very quiet', 'precise and tactile', 'leaves space for the voice'],
  },
  { name: 'Power on', scenes: ['boot'], pos: ['short gentle lift', 'one shimmering chord', 'a held breath'] },
  {
    name: 'Screen',
    scenes: ['live', 'palettes', 'menu', 'uses'],
    pos: ['lower and darker, softly filtered', 'warm round bass notes', 'slow glowing arpeggio', 'focused, nocturnal', 'leaves space for the voice'],
  },
  {
    name: 'Studio',
    scenes: ['battery', 'studio'],
    pos: ['the piano motif returns brighter', 'open airy pads', 'light steady pulse', 'optimistic', 'leaves space for the voice'],
  },
  {
    name: 'Close',
    scenes: ['price', 'end'],
    pos: ['warm harmonic resolution', 'the pulse thins out', 'the piano motif one last time', 'a final single piano note rings out softly for three seconds'],
    neg: ['abrupt ending', 'fade to silence before the end', 'drums'],
  },
]

const MUSIC_START = SCENES.find((s) => s.id === 'open')!.start - 1.4
// The model tends to end a few seconds early and pad with silence; a closing section past the
// film's end keeps the last chord ringing under the address (the film fades it out).
const RING_OUT = { section_name: 'Ring out', positive_local_styles: ['one sustained felt piano chord over a soft pad', 'slow natural decay', 'very quiet'], negative_local_styles: ['new melody', 'drums'], duration_ms: 6000, lines: [] as string[] }
const sections = ACTS.map((a, i) => {
  const secs = SCENES.filter((s) => a.scenes.includes(s.id)).reduce((n, s) => n + s.length, 0) + (i === 0 ? 1.4 : 0)
  return {
    section_name: a.name,
    positive_local_styles: a.pos,
    negative_local_styles: a.neg ?? [],
    duration_ms: Math.round(secs * 1000),
    lines: [] as string[],
  }
})
sections.push(RING_OUT)

const plan = {
  positive_global_styles: [
    'instrumental',
    'minimal modern ambient',
    'felt piano over warm analog pads',
    'premium hardware product film',
    'calm, warm, precise and quietly optimistic',
    'unobtrusive under a spoken voiceover',
    '76 BPM',
  ],
  negative_global_styles: ['vocals', 'singing', 'choir', 'heavy drums', 'snare', 'risers', 'EDM drops', 'rock guitars', 'epic trailer brass', 'dark horror', 'busy melodies'],
  sections,
}

console.log(sections.map((s) => `${s.section_name.padEnd(10)} ${(s.duration_ms / 1000).toFixed(1)} s`).join('\n'))
console.log(`total ${(sections.reduce((n, s) => n + s.duration_ms, 0) / 1000).toFixed(1)} s (film ${TOTAL.toFixed(1)} s, music from ${MUSIC_START.toFixed(1)} s)`)
if (dryRun) process.exit(0)
if (existsSync(out) && !force) {
  console.log(`${out} exists; pass --force to compose again (paid)`)
  process.exit(0)
}

const keyFile = join(homedir(), '.config', 'elevenlabs', 'api-key')
const apiKey = process.env.ELEVENLABS_API_KEY ?? (existsSync(keyFile) ? readFileSync(keyFile, 'utf8').trim() : undefined)
if (!apiKey) throw new Error('No ElevenLabs key found (ELEVENLABS_API_KEY or ~/.config/elevenlabs/api-key).')

writeFileSync(out.replace(/\.mp3$/, '.plan.json'), `${JSON.stringify(plan, null, 2)}\n`)
const res = await fetch('https://api.elevenlabs.io/v1/music?output_format=mp3_44100_192', {
  method: 'POST',
  headers: { 'xi-api-key': apiKey, 'Content-Type': 'application/json' },
  body: JSON.stringify({ composition_plan: plan, model_id: 'music_v1', respect_sections_durations: true }),
})
const receipt = {
  at: new Date().toISOString(),
  status: res.status,
  request_id: res.headers.get('request-id') ?? res.headers.get('x-request-id'),
  character_cost: res.headers.get('character-cost') ?? res.headers.get('x-character-count'),
  seconds: sections.reduce((n, s) => n + s.duration_ms, 0) / 1000,
}
writeFileSync(out.replace(/\.mp3$/, '.receipt.json'), `${JSON.stringify(receipt, null, 2)}\n`)
if (!res.ok) throw new Error(`ElevenLabs Music HTTP ${res.status}: ${(await res.text()).slice(0, 400)}`)
const raw = out.replace(/\.mp3$/, '.raw.mp3')
writeFileSync(raw, Buffer.from(await res.arrayBuffer()))
execFileSync('ffmpeg', ['-hide_banner', '-v', 'error', '-y', '-i', raw, '-af', 'loudnorm=I=-18:TP=-2:LRA=11', '-ar', '44100', '-b:a', '192k', out])
console.log('wrote', out, receipt)
