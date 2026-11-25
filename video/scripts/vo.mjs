#!/usr/bin/env node
/*
 * Narration synthesis in recomposable per-scene chunks using Max Ritter's approved
 * ElevenLabs personal voice clone (eleven_v4) + local faster-whisper word alignment.
 *
 *   node scripts/vo.mjs launch [--dry-run]
 */
import { createHash } from 'node:crypto'
import { execFileSync, spawnSync } from 'node:child_process'
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from 'node:fs'
import { homedir } from 'node:os'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const video = process.argv[2] ?? 'launch'
const dryRun = process.argv.includes('--dry-run')

const script = JSON.parse(readFileSync(join(root, 'vo', `${video}.json`), 'utf8'))
const profile = JSON.parse(readFileSync(join(homedir(), '.config', 'personal-voice', 'profile.json'), 'utf8'))
const helperCandidates = [
  join(homedir(), '.gemini', 'config', 'skills', 'personal-voice', 'scripts', 'personal_voice.py'),
  join(homedir(), '.claude', 'skills', 'personal-voice', 'scripts', 'personal_voice.py'),
]
const helper = helperCandidates.find((p) => existsSync(p))
if (!helper) throw new Error('personal_voice.py helper not found')

const voice = `${profile.voice_id}|${profile.model_id}|${profile.stability}|${profile.similarity_boost}|${script.language}`

const audioDir = join(root, 'public', 'vo', video)
const receiptDir = join(root, 'vo', 'receipts', video)
const textDir = join(root, 'vo', 'text', video)
const wordsDir = join(root, 'vo', 'words')
for (const d of [audioDir, receiptDir, textDir, wordsDir, join(root, 'src', 'vo')]) mkdirSync(d, { recursive: true })

const chunks = script.chunks.map((c) => {
  const key = createHash('sha256').update(`${voice}\n${c.text.trim()}`).digest('hex').slice(0, 10)
  return { ...c, key, file: `vo/${video}/${c.id}.${key}.mp3` }
})

const missing = chunks.filter((c) => !existsSync(join(root, 'public', c.file)))
const chars = missing.reduce((n, c) => n + c.text.length, 0)
console.log(`${chunks.length} chunks, ${missing.length} to record (${chars} characters)`)
for (const c of missing) console.log(`  record ${c.id}: ${c.text}`)
if (dryRun) process.exit(0)

const sleep = (ms) => new Promise((r) => setTimeout(r, ms))
for (const c of missing) {
  const textFile = join(textDir, `${c.id}.${c.key}.txt`)
  writeFileSync(textFile, `${c.text.trim()}\n`)
  const out = join(root, 'public', c.file)
  const receipt = `${out}.receipt.json`
  for (let attempt = 1; ; attempt++) {
    const run = spawnSync(
      'python3',
      [helper, 'synthesize', '--text-file', textFile, '--output', out, '--language', script.language],
      { encoding: 'utf8' },
    )
    if (run.status === 0) break
    const refused = /HTTP 429/.test(run.stderr) && !existsSync(out)
    if (!refused || attempt === 5) {
      process.stderr.write(run.stderr)
      throw new Error(`recording ${c.id} failed; inspect ${receipt}`)
    }
    if (existsSync(receipt)) {
      mkdirSync(join(receiptDir, 'refused'), { recursive: true })
      renameSync(receipt, join(receiptDir, 'refused', `${c.id}.${c.key}.${attempt}.json`))
    }
    console.log(`  rate limited on ${c.id}; waiting ${attempt * 15} s`)
    await sleep(attempt * 15000)
  }
  if (existsSync(receipt)) renameSync(receipt, join(receiptDir, `${c.id}.${c.key}.receipt.json`))
  console.log('recorded', c.file)
  await sleep(1200)
}

const unaligned = chunks.filter((c) => !existsSync(join(wordsDir, `${c.key}.json`)))
if (unaligned.length) {
  const jobs = join(wordsDir, '.jobs.json')
  writeFileSync(jobs, JSON.stringify(unaligned.map((c) => ({ key: c.key, audio: join(root, 'public', c.file), text: c.text }))))
  execFileSync('uv', ['run', join(root, 'scripts', 'align-vo.py'), jobs, wordsDir], { stdio: ['ignore', 'inherit', 'inherit'] })
}

const manifest = chunks.map((c) => {
  const timing = JSON.parse(readFileSync(join(wordsDir, `${c.key}.json`), 'utf8'))
  return { id: c.id, scene: c.scene, gap: c.gap ?? null, file: c.file, duration: timing.duration, words: timing.words }
})
writeFileSync(join(root, 'src', 'vo', `${video}.json`), `${JSON.stringify(manifest, null, 2)}\n`)
const total = manifest.reduce((n, c) => n + c.duration, 0)
console.log(`wrote src/vo/${video}.json: ${manifest.length} chunks, ${total.toFixed(1)} s of narration`)
