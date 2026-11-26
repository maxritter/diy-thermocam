#!/usr/bin/env node
/*
 * Master a render for the web: two-pass loudness normalisation to -14 LUFS
 * (YouTube's reference) with -1.5 dBTP headroom, AAC 192k, H.264 untouched,
 * moov atom at the front for progressive playback.
 *
 *   node scripts/master.mjs <in.mp4> <out.mp4>
 */
import { spawnSync } from 'node:child_process'

const [input, output] = process.argv.slice(2)
if (!input || !output) {
	console.error('usage: node scripts/master.mjs <in.mp4> <out.mp4>')
	process.exit(1)
}
const target = 'I=-14:TP=-1.5:LRA=11'

// Pass 1: measure. ffmpeg prints the loudnorm report on stderr.
const pass1 = spawnSync('ffmpeg', ['-hide_banner', '-i', input, '-af', `loudnorm=${target}:print_format=json`, '-f', 'null', '-'], { encoding: 'utf8' })
const report = pass1.stderr
const m = JSON.parse(report.slice(report.lastIndexOf('{'), report.lastIndexOf('}') + 1))

// Pass 2: apply linearly with the measured values.
const filter = `loudnorm=${target}:measured_I=${m.input_i}:measured_TP=${m.input_tp}:measured_LRA=${m.input_lra}:measured_thresh=${m.input_thresh}:offset=${m.target_offset}:linear=true`
const pass2 = spawnSync(
	'ffmpeg',
	['-hide_banner', '-loglevel', 'error', '-y', '-i', input, '-c:v', 'copy', '-af', filter, '-ar', '48000', '-c:a', 'aac', '-b:a', '192k', '-movflags', '+faststart', output],
	{ stdio: 'inherit' },
)
if (pass2.status !== 0) process.exit(pass2.status ?? 1)
console.log(`mastered ${output} (was ${m.input_i} LUFS, ${m.input_tp} dBTP)`)
