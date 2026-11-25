/*
 * Composes a calm minimal ambient electronic underscore matched to the exact scene
 * lengths of the DIY-Thermocam V4 Launch film via the ElevenLabs Music API.
 *
 *   bun scripts/music.ts [--dry-run] [--force]
 */
import { execFileSync, spawnSync } from 'node:child_process'
import { existsSync, readFileSync, writeFileSync } from 'node:fs'
import { homedir } from 'node:os'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { LAUNCH_TIMELINE, MUSIC_LEAD_IN, SCENE_STARTS } from '../src/layoutConfig'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const out = join(root, 'public', 'audio', 'launch-music.mp3')
const dryRun = process.argv.includes('--dry-run')
const force = process.argv.includes('--force')

const sceneStarts = SCENE_STARTS
const totalSec = LAUNCH_TIMELINE.total / 30

const STYLE: Record<string, { name: string; pos: string[]; neg?: string[] }> = {
  intro: {
    name: 'Intro',
    pos: ['warm analog synth pad', 'subtle minimal pulse', 'clean scientific precision', 'gentle opening lift'],
  },
  hardware: {
    name: 'Hardware',
    pos: ['steady minimal electronic pulse', 'soft muted synth plucks', 'calm engineering focus', 'leaves clear space for spoken voice'],
  },
  firmware: {
    name: 'Firmware',
    pos: ['crisp understated pulse', 'warm pads', 'subtle rhythmic clarity', 'leaves clear space for spoken voice'],
  },
  studio: {
    name: 'Studio',
    pos: ['spacious ambient electronic', 'warm pads and gentle bass', 'confident and clear', 'leaves clear space for spoken voice'],
  },
  outro: {
    name: 'Outro',
    pos: ['warm harmonic resolution', 'soft sustained chord', 'clean gentle fade'],
    neg: ['abrupt cut'],
  },
}

const sections: {
  section_name: string
  positive_local_styles: string[]
  negative_local_styles: string[]
  duration_ms: number
  lines: string[]
}[] = [
  {
    section_name: 'Lead-in',
    positive_local_styles: ['fades in smoothly'],
    negative_local_styles: [],
    duration_ms: MUSIC_LEAD_IN * 1000,
    lines: [],
  },
]

sceneStarts.forEach((s, i) => {
  const end = i + 1 < sceneStarts.length ? sceneStarts[i + 1].start : totalSec
  const durMs = Math.max(3000, Math.round((end - s.start) * 1000))
  const st = STYLE[s.id]
  sections.push({
    section_name: st.name,
    positive_local_styles: st.pos,
    negative_local_styles: st.neg ?? [],
    duration_ms: durMs,
    lines: [],
  })
})

const plan = {
  positive_global_styles: [
    'instrumental',
    'calm minimal ambient electronic',
    'warm analog pads',
    'soft understated pulse',
    'clean hardware product showcase underscore',
    'unobtrusive under a spoken voiceover',
    '98 BPM',
  ],
  negative_global_styles: [
    'vocals',
    'singing',
    'choir',
    'heavy drums',
    'snare',
    'risers',
    'EDM drops',
    'rock guitars',
    'epic trailer brass',
    'dark horror',
  ],
  sections,
}

console.log(sections.map((s) => `${s.section_name.padEnd(10)} ${(s.duration_ms / 1000).toFixed(1)} s`).join('\n'))
console.log(`total ${(sections.reduce((n, s) => n + s.duration_ms, 0) / 1000).toFixed(1)} s`)
if (dryRun) process.exit(0)
if (existsSync(out) && !force) {
  console.log(`${out} exists; pass --force to regenerate`)
  process.exit(0)
}

const keyFile = join(homedir(), '.config', 'elevenlabs', 'api-key')
const apiKey = process.env.ELEVENLABS_API_KEY ?? (existsSync(keyFile) ? readFileSync(keyFile, 'utf8').trim() : undefined)
if (!apiKey) throw new Error('No ElevenLabs key found (ELEVENLABS_API_KEY or ~/.config/elevenlabs/api-key).')

function detectGaps(file: string): number[] {
  const log = spawnSync('ffmpeg', ['-hide_banner', '-i', file, '-af', 'silencedetect=n=-45dB:d=2', '-f', 'null', '-'], {
    encoding: 'utf8',
  }).stderr
  return [...log.matchAll(/silence_start: ([\d.]+)/g)]
    .map((m) => Number(m[1]))
    .filter((s) => s > MUSIC_LEAD_IN - 1 && s < totalSec + MUSIC_LEAD_IN - 8)
}

writeFileSync(`${out.replace(/\.mp3$/, '')}.plan.json`, `${JSON.stringify(plan, null, 2)}\n`)
const res = await fetch('https://api.elevenlabs.io/v1/music?output_format=mp3_44100_192', {
  method: 'POST',
  headers: { 'xi-api-key': apiKey, 'Content-Type': 'application/json' },
  body: JSON.stringify({ composition_plan: plan, model_id: 'music_v1', respect_sections_durations: true }),
})
if (!res.ok) {
  const body = await res.text()
  throw new Error(`ElevenLabs Music HTTP ${res.status}: ${body.slice(0, 400)}`)
}
const raw = out.replace(/\.mp3$/, '.raw.mp3')
writeFileSync(raw, Buffer.from(await res.arrayBuffer()))
const gaps = detectGaps(raw)
if (gaps.length) {
  console.warn(`note: detected low-level gap at ${gaps.map((g) => `${g.toFixed(1)}s`).join(', ')}`)
}
execFileSync('ffmpeg', [
  '-hide_banner',
  '-v',
  'error',
  '-y',
  '-i',
  raw,
  '-af',
  'loudnorm=I=-18:TP=-2:LRA=11',
  '-ar',
  '44100',
  '-b:a',
  '192k',
  out,
])
console.log('wrote', out)
