import { AbsoluteFill } from 'remotion'
import { C } from '../theme'
import { INOUT, ramp, useSec } from './Kit'

function blendHex(a: string, b: string, p: number): string {
  const q = Math.min(1, Math.max(0, p))
  const parse = (c: string) =>
    c.startsWith('#')
      ? [1, 3, 5].map((i) => Number.parseInt(c.slice(i, i + 2), 16))
      : c.slice(4, -1).split(',').map(Number)
  const [x, y] = [parse(a), parse(b)]
  return `rgb(${x.map((v, i) => Math.round(v + (y[i] - v) * q)).join(',')})`
}

/**
 * Calm obsidian studio backdrop (#0B0D12) with a subtle dot grid and a single slow-shifting
 * Ironbow ambient glow that follows the current scene — zero distracting particle noise.
 */
export function StudioBackdrop({ tones }: { tones: [number, string][] }) {
  const t = useSec()
  let tone = tones[0][1]
  for (const [at, next] of tones.slice(1)) {
    tone = blendHex(tone, next, ramp(t, at - 0.9, at + 0.9, INOUT))
  }
  return (
    <AbsoluteFill style={{ background: C.bg, overflow: 'hidden' }}>
      <AbsoluteFill
        style={{
          background: `radial-gradient(ellipse 68% 52% at 50% 42%, ${tone}, transparent 72%)`,
          opacity: 0.14,
        }}
      />
      <AbsoluteFill
        style={{
          backgroundImage: 'radial-gradient(circle, rgba(242,244,248,0.085) 1.1px, transparent 1.7px)',
          backgroundSize: '42px 42px',
          backgroundPosition: '21px 21px',
          maskImage: 'radial-gradient(ellipse 78% 72% at 50% 46%, #000 28%, transparent 86%)',
          WebkitMaskImage: 'radial-gradient(ellipse 78% 72% at 50% 46%, #000 28%, transparent 86%)',
        }}
      />
      <AbsoluteFill
        style={{
          background: 'radial-gradient(ellipse 88% 82% at 50% 48%, transparent 56%, rgba(4,5,8,0.62) 100%)',
          pointerEvents: 'none',
        }}
      />
    </AbsoluteFill>
  )
}
