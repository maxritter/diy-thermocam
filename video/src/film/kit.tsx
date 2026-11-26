// Drawing and motion parts for the launch film: a quiet light studio (with dark interludes for the
// thermal image), thin titles with a small-caps line, the firmware's own screens and the 3D shots.
import type { CSSProperties, ReactNode } from 'react'
import { Easing, Img, interpolate, staticFile, useCurrentFrame } from 'remotion'
import simIndex from '../../public/sim/index.json'
import { FPS } from './time'

export const C = {
  light: '#ECEDEF',
  lightEdge: '#DFE1E5',
  dark: '#0A0B0D',
  ink: '#1D1F23',
  inkSoft: '#5B6068',
  muted: '#8B9097',
  paper: '#F7F7F8',
  amber: '#F36D21',
  amberHi: '#FFB25A',
  cold: '#5EB8FF',
  line: 'rgba(29,31,35,0.14)',
}

export const SANS = 'Geist, system-ui, sans-serif'
export const MONO = '"Geist Mono", ui-monospace, monospace'
export const PIXEL = '"Geist Pixel", "Geist Mono", monospace'

export const OUT = Easing.bezier(0.16, 1, 0.3, 1)
export const INOUT = Easing.bezier(0.65, 0, 0.35, 1)
export const POP = Easing.bezier(0.34, 1.4, 0.64, 1)
const clamp = { extrapolateLeft: 'clamp', extrapolateRight: 'clamp' } as const

/** Seconds since the enclosing <Sequence> started. */
export const useT = () => useCurrentFrame() / FPS

/** 0 → 1 between a and b (seconds). */
export const ramp = (t: number, a: number, b: number, easing = OUT) => interpolate(t, [a, b], [0, 1], { ...clamp, easing })

/** Fades in over `fin` from `a`, out over `fout` ending at `b`. */
export const window_ = (t: number, a: number, b: number, fin = 0.45, fout = 0.35) =>
  Math.min(ramp(t, a, a + fin), 1 - ramp(t, b - fout, b, INOUT))

export const lerp = (a: number, b: number, k: number) => a + (b - a) * k

// ---------------------------------------------------------------------------------------------
// Stage
// ---------------------------------------------------------------------------------------------

export function Stage({ dark, children }: { dark: number; children?: ReactNode }) {
  const light = `radial-gradient(ellipse 80% 70% at 50% 46%, #F5F6F7 0%, ${C.light} 55%, ${C.lightEdge} 100%)`
  return (
    <div style={{ position: 'absolute', inset: 0, background: light, overflow: 'hidden' }}>
      <div
        style={{
          position: 'absolute',
          inset: 0,
          opacity: dark,
          background: `radial-gradient(ellipse 60% 55% at 50% 48%, #17120F 0%, ${C.dark} 70%)`,
        }}
      />
      {children}
    </div>
  )
}

/** The mark and name, top left, on every scene (Lofree's "N°92"). */
export function Brand({ dark, show = 1 }: { dark: number; show?: number }) {
  const tone = dark > 0.5 ? '#C9CCD1' : C.ink
  return (
    <div style={{ position: 'absolute', left: 64, top: 54, display: 'flex', alignItems: 'center', gap: 14, opacity: show }}>
      <Img src={staticFile('film/logo-mark.svg')} style={{ width: 30, height: 30 }} />
      <div style={{ fontFamily: SANS, fontSize: 15, fontWeight: 600, letterSpacing: '0.24em', color: tone }}>DIY-THERMOCAM</div>
    </div>
  )
}

/** Thin centred title with a small-caps line under it; blurs in, rises a little, fades out. */
export function Title({ t, at, until, title, sub, dark = false, y = 96, size = 62 }: {
  t: number; at: number; until: number; title: string; sub?: string; dark?: boolean; y?: number; size?: number
}) {
  const k = ramp(t, at, at + 0.7)
  const o = window_(t, at, until, 0.6, 0.4)
  const subK = ramp(t, at + 0.25, at + 1.0)
  if (o <= 0) return null
  return (
    <div style={{ position: 'absolute', left: 0, right: 0, top: y, textAlign: 'center', opacity: o }}>
      <div
        style={{
          fontFamily: SANS,
          fontWeight: 300,
          fontSize: size,
          letterSpacing: '-0.015em',
          color: dark ? '#F3F3F4' : C.ink,
          translate: `0 ${lerp(14, 0, k)}px`,
          filter: k < 0.98 ? `blur(${lerp(8, 0, k)}px)` : undefined,
        }}
      >
        {title}
      </div>
      {sub ? (
        <div
          style={{
            marginTop: 14,
            fontFamily: SANS,
            fontWeight: 600,
            fontSize: 20,
            letterSpacing: '0.3em',
            color: dark ? '#A4A9B0' : '#5B6068',
            opacity: subK,
          }}
        >
          {sub}
        </div>
      ) : null}
    </div>
  )
}

/** Small caps label. */
export function Caps({ children, color = '#5B6068', size = 18, style }: { children: ReactNode; color?: string; size?: number; style?: CSSProperties }) {
  return <div style={{ fontFamily: SANS, fontWeight: 500, fontSize: size, letterSpacing: '0.28em', color, ...style }}>{children}</div>
}

// ---------------------------------------------------------------------------------------------
// The firmware's screens (320 × 240, drawn by the real Firmware 4.0 UI in the host simulator)
// ---------------------------------------------------------------------------------------------

const SENSOR_FPS = 9
const frames = simIndex as Record<string, number>

export const simSrc = (seq: string, i = 0) => staticFile(`sim/${seq}/${String(i).padStart(4, '0')}.png`)

/** Frame of a sequence at time t (s) at the sensor's ~9 fps. */
export const simAt = (seq: string, t: number, loop = true) => {
  const n = frames[seq]
  const i = Math.floor(Math.max(0, t) * SENSOR_FPS)
  return loop ? i % n : Math.min(i, n - 1)
}

/** A firmware screen at any size, pixels kept sharp. */
export function Screen({ seq, t = 0, frame, w, style, loop = true }: {
  seq: string; t?: number; frame?: number; w: number; style?: CSSProperties; loop?: boolean
}) {
  const i = frame ?? simAt(seq, t, loop)
  return (
    <Img
      src={simSrc(seq, i)}
      style={{ width: w, height: (w * 3) / 4, imageRendering: w >= 900 || Number.isInteger(w / 320) ? 'pixelated' : 'auto', display: 'block', ...style }}
    />
  )
}

/** A screen in a thin dark bezel with a soft shadow, like the display module itself. */
export function Device({ children, w, light = false, style }: { children: ReactNode; w: number; light?: boolean; style?: CSSProperties }) {
  const pad = Math.round(w * 0.028)
  return (
    <div
      style={{
        padding: pad,
        borderRadius: pad * 1.6,
        background: '#16181B',
        boxShadow: light
          ? '0 40px 80px -30px rgba(20,22,26,0.45), 0 12px 24px -12px rgba(20,22,26,0.3)'
          : '0 0 0 1px rgba(255,255,255,0.06), 0 40px 120px -20px rgba(243,109,33,0.18)',
        ...style,
      }}
    >
      <div style={{ borderRadius: pad * 0.6, overflow: 'hidden' }}>{children}</div>
    </div>
  )
}

// ---------------------------------------------------------------------------------------------
// 3D shots (Blender PNG sequences with alpha in public/shots/<name>/)
// ---------------------------------------------------------------------------------------------

export const SHOT_FRAMES: Record<string, number> = {
  open: 285, sensor: 336, assemble: 498, 'case-shell': 228, 'case-laser': 228, boot: 180, end: 300, explode: 300,
}

// Fades a shot's edges, so the shadow-catcher floor never shows the frame's border when a shot is scaled
const FEATHER: CSSProperties = {
  maskImage:
    'linear-gradient(to right, transparent 0%, #000 12%, #000 88%, transparent 100%), linear-gradient(to bottom, transparent 0%, #000 10%, #000 86%, transparent 100%)',
  maskComposite: 'intersect',
  WebkitMaskComposite: 'source-in',
}

/** The shot at its own time ts (s); holds the last frame past the end. `soft` feathers its edges. */
export function Shot({ name, ts, style, soft = false }: { name: string; ts: number; style?: CSSProperties; soft?: boolean }) {
  const n = SHOT_FRAMES[name]
  const i = Math.max(0, Math.min(n - 1, Math.round(ts * FPS)))
  return (
    <Img
      src={staticFile(`shots/${name}/${String(i).padStart(4, '0')}.png`)}
      style={{ position: 'absolute', left: 0, top: 0, width: '100%', height: '100%', ...(soft ? FEATHER : {}), ...style }}
    />
  )
}

// ---------------------------------------------------------------------------------------------
// Annotations
// ---------------------------------------------------------------------------------------------

/** A white pill with a dot and text. */
export function Pill({ children, tone = C.amber, dark = false, style }: { children: ReactNode; tone?: string; dark?: boolean; style?: CSSProperties }) {
  return (
    <div
      style={{
        display: 'inline-flex',
        alignItems: 'center',
        gap: 12,
        padding: '14px 26px',
        borderRadius: 999,
        background: dark ? 'rgba(28,30,34,0.86)' : 'rgba(255,255,255,0.92)',
        boxShadow: dark ? '0 0 0 1px rgba(255,255,255,0.08)' : '0 10px 30px -12px rgba(20,22,26,0.35)',
        fontFamily: SANS,
        fontSize: 28,
        fontWeight: 500,
        color: dark ? '#EDEEF0' : C.ink,
        whiteSpace: 'nowrap',
        ...style,
      }}
    >
      <span style={{ width: 12, height: 12, borderRadius: 6, background: tone }} />
      {children}
    </div>
  )
}

/** A ring that draws on a point, with a leader line to a label. Coordinates in px of the parent. */
export function Callout({ t, at, until, x, y, lx, ly, label, tone = C.amber, r = 26 }: {
  t: number; at: number; until: number; x: number; y: number; lx: number; ly: number; label: string; tone?: string; r?: number
}) {
  const o = window_(t, at, until, 0.35, 0.3)
  if (o <= 0) return null
  const k = ramp(t, at, at + 0.5, POP)
  const line = ramp(t, at + 0.15, at + 0.55)
  const left = lx < x
  const ex = lerp(x, lx, line)
  const ey = lerp(y, ly, line)
  return (
    <div style={{ position: 'absolute', inset: 0, opacity: o, pointerEvents: 'none' }}>
      <svg style={{ position: 'absolute', inset: 0, overflow: 'visible' }} width="100%" height="100%">
        <circle cx={x} cy={y} r={r * k} fill="none" stroke={tone} strokeWidth={3} />
        <circle cx={x} cy={y} r={r * k + 10 * (1 - k)} fill="none" stroke={tone} strokeOpacity={0.35 * (1 - k)} strokeWidth={2} />
        <line x1={x + (left ? -r : r) * 0.9} y1={y} x2={ex} y2={ey} stroke={tone} strokeWidth={2} />
      </svg>
      <div
        style={{
          position: 'absolute',
          top: ly - 30,
          ...(left ? { right: `calc(100% - ${lx - 14}px)` } : { left: lx + 14 }),
          opacity: ramp(t, at + 0.4, at + 0.8),
        }}
      >
        <Pill dark tone={tone}>{label}</Pill>
      </div>
    </div>
  )
}

/** A tap on a touchscreen: a dot that presses and a ring that spreads. */
export function Tap({ t, at, x, y }: { t: number; at: number; x: number; y: number }) {
  const d = t - at
  if (d < -0.2 || d > 0.9) return null
  const press = d < 0 ? ramp(t, at - 0.2, at) : 1 - ramp(t, at + 0.15, at + 0.5)
  const ring = ramp(t, at, at + 0.8)
  return (
    <svg style={{ position: 'absolute', inset: 0, overflow: 'visible', pointerEvents: 'none' }} width="100%" height="100%">
      <circle cx={x} cy={y} r={lerp(10, 70, ring)} fill="none" stroke="#FFFFFF" strokeOpacity={0.7 * (1 - ring)} strokeWidth={3} />
      <circle cx={x} cy={y} r={22 * (0.8 + 0.2 * press)} fill="rgba(255,255,255,0.55)" opacity={press} />
    </svg>
  )
}

/** A number that counts up to its value. */
export function Count({ t, at, dur = 1.2, to, from = 0, decimals = 0, format = false, style }: {
  t: number; at: number; dur?: number; to: number; from?: number; decimals?: number; format?: boolean; style?: CSSProperties
}) {
  const v = lerp(from, to, ramp(t, at, at + dur, Easing.bezier(0.2, 0.7, 0.2, 1)))
  const text = format ? Math.round(v).toLocaleString('en-US') : v.toFixed(decimals)
  return <span style={{ fontVariantNumeric: 'tabular-nums', ...style }}>{text}</span>
}

/** Piecewise-linear map through [from, to] knots (film time → shot time), clamped at both ends. */
export function warp(t: number, knots: [number, number][]) {
  if (t <= knots[0][0]) return knots[0][1]
  for (let i = 1; i < knots.length; i++) {
    const [a, x] = knots[i - 1]
    const [b, y] = knots[i]
    if (t <= b) return b > a ? x + ((t - a) / (b - a)) * (y - x) : y
  }
  return knots[knots.length - 1][1]
}
