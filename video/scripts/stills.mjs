#!/usr/bin/env node
/*
 * Several stills of one composition from a single bundle.
 *
 *   node scripts/stills.mjs <CompositionId> <seconds,seconds,...> [--scale 0.5] [--out work/stills] [--prefix s]
 *
 * Seconds are film seconds (frame = seconds x fps). Writes <out>/<prefix>-<seconds>.png. Light by design:
 * one render at a time, concurrency 2.
 */
import { mkdirSync } from 'node:fs'
import { join, resolve } from 'node:path'
import { bundle } from '@remotion/bundler'
import { renderStill, selectComposition } from '@remotion/renderer'

const args = process.argv.slice(2)
const flag = (name, d) => (args.includes(name) ? args[args.indexOf(name) + 1] : d)
const [id, list] = args
const scale = Number(flag('--scale', '0.5'))
const out = resolve(flag('--out', 'work/stills'))
const prefix = flag('--prefix', 's')
mkdirSync(out, { recursive: true })
const serveUrl = await bundle({ entryPoint: resolve('src/index.ts'), onProgress: () => {} })
const comp = await selectComposition({ serveUrl, id, inputProps: {} })
for (const sec of list.split(',').map(Number)) {
	const frame = Math.min(comp.durationInFrames - 1, Math.round(sec * comp.fps))
	const output = join(out, `${prefix}-${String(sec).padStart(6, '0')}.png`)
	await renderStill({ composition: comp, serveUrl, frame, output, scale, imageFormat: 'png', chromiumOptions: { gl: 'angle' } })
	console.log('still', output)
}
