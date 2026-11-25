import type { CSSProperties, ReactNode } from 'react'
import { AbsoluteFill, Easing, interpolate, spring, useCurrentFrame, useVideoConfig } from 'remotion'
import { C, FONT, MONO } from '../theme'

const clamp = { extrapolateLeft: 'clamp', extrapolateRight: 'clamp' } as const
export const OUT = Easing.bezier(0.16, 1, 0.3, 1)
export const INOUT = Easing.bezier(0.65, 0, 0.35, 1)

export function useSec(): number {
  const frame = useCurrentFrame()
  const { fps } = useVideoConfig()
  return frame / fps
}

export const ramp = (t: number, a: number, b: number, easing = OUT) =>
  interpolate(t, [a, b], [0, 1], { ...clamp, easing })

export const snap = (px: number) => Math.round(px)
export const blurPx = (px: number) => (px < 0.5 ? undefined : `blur(${px.toFixed(1)}px)`)

export function useSpringIn(delaySec = 0, damping = 18, stiffness = 130) {
  const frame = useCurrentFrame()
  const { fps } = useVideoConfig()
  return spring({
    frame: Math.max(0, frame - Math.round(delaySec * fps)),
    fps,
    config: { damping, stiffness, mass: 0.9 },
  })
}

/** Clean cross-dissolve & subtle scale shell across scene boundaries (zero flicker). */
export function SceneShell({
  len,
  fadeIn = 0.4,
  fadeOut = 0.4,
  children,
}: {
  len: number
  fadeIn?: number
  fadeOut?: number
  children: ReactNode
}) {
  const t = useSec()
  const oIn = fadeIn > 0 ? ramp(t, 0, fadeIn, OUT) : 1
  const oOut = fadeOut > 0 ? 1 - ramp(t, len - fadeOut, len, INOUT) : 1
  const opacity = Math.min(oIn, oOut)
  return <AbsoluteFill style={{ opacity }}>{children}</AbsoluteFill>
}

/** Consistent top-left scene title header with subtle Ironbow accent bar (no marketing kickers/eyebrows). */
export function SceneHeader({
  title,
  detail,
  accent = C.ironOrange,
  delay = 0.08,
}: {
  title: string
  detail?: string
  accent?: string
  delay?: number
}) {
  const t = useSec()
  const p = ramp(t, delay, delay + 0.55, OUT)
  return (
    <div
      style={{
        position: 'absolute',
        left: 84,
        top: 52,
        right: 84,
        display: 'flex',
        alignItems: 'baseline',
        justifyContent: 'space-between',
        opacity: p,
        translate: `0px ${snap((1 - p) * 14)}px`,
      }}
    >
      <div style={{ display: 'flex', alignItems: 'center', gap: 16 }}>
        <div style={{ width: 6, height: 34, borderRadius: 3, background: accent }} />
        <div
          style={{
            fontFamily: FONT,
            fontSize: 38,
            fontWeight: 650,
            letterSpacing: -0.9,
            color: C.text,
            lineHeight: 1.1,
          }}
        >
          {title}
        </div>
      </div>
      {detail ? (
        <div
          style={{
            fontFamily: MONO,
            fontSize: 20,
            fontWeight: 500,
            color: C.muted,
            letterSpacing: -0.2,
          }}
        >
          {detail}
        </div>
      ) : null}
    </div>
  )
}

/** Elevated dark studio card frame around hardware renders or screenshots. */
export function StudioCard({
  label,
  tag,
  accent = C.borderStrong,
  lightBg = false,
  style,
  children,
}: {
  label?: string
  tag?: string
  accent?: string
  lightBg?: boolean
  style?: CSSProperties
  children: ReactNode
}) {
  return (
    <div
      style={{
        borderRadius: 18,
        background: lightBg ? '#F5F7FA' : 'linear-gradient(180deg, #141925 0%, #0E121B 100%)',
        border: `1px solid ${accent}`,
        boxShadow: '0 32px 90px rgba(0,0,0,0.68), 0 0 0 1px rgba(255,255,255,0.03)',
        overflow: 'hidden',
        display: 'flex',
        flexDirection: 'column',
        ...style,
      }}
    >
      {label ? (
        <div
          style={{
            height: 44,
            padding: '0 18px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            background: '#0F131D',
            borderBottom: `1px solid ${C.border}`,
            flexShrink: 0,
          }}
        >
          <span style={{ fontFamily: FONT, fontSize: 17, fontWeight: 600, color: C.text, letterSpacing: -0.2 }}>
            {label}
          </span>
          {tag ? (
            <span
              style={{
                fontFamily: MONO,
                fontSize: 14,
                fontWeight: 500,
                color: C.ironGold,
                background: 'rgba(234,109,32,0.14)',
                border: '1px solid rgba(234,109,32,0.35)',
                borderRadius: 6,
                padding: '2px 9px',
              }}
            >
              {tag}
            </span>
          ) : null}
        </div>
      ) : null}
      <div style={{ position: 'relative', flex: 1, minHeight: 0, overflow: 'hidden' }}>{children}</div>
    </div>
  )
}
