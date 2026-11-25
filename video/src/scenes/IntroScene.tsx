import { AbsoluteFill } from 'remotion'
import { IronbowMark } from '../components/Mark'
import { blurPx, OUT, ramp, SceneShell, snap, useSec, useSpringIn } from '../components/Kit'
import { C, FONT, MONO } from '../theme'

const PILLARS = ['KiCad 10 Zero-Solder PCB', 'build123d 3D & 2D CAD', 'Firmware 4.0.0', 'Tauri v2 Desktop Studio']

export function IntroScene({ durationSec }: { durationSec: number }) {
  const t = useSec()
  const markScale = useSpringIn(0.05, 16, 115)
  const ringReveal = ramp(t, 0.08, 1.15, OUT)
  const titleP = ramp(t, 0.35, 1.05, OUT)
  const subP = ramp(t, 1.85, 2.55, OUT)
  const pillarsP = ramp(t, 3.2, 4.0, OUT)

  return (
    <SceneShell len={durationSec} fadeIn={0.35} fadeOut={0.4}>
      <AbsoluteFill
        style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          paddingBottom: 90,
        }}
      >
        {/* Subtle Ironbow concentric halo behind the lens mark */}
        <div
          style={{
            position: 'relative',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            marginBottom: 40,
            scale: `${0.75 + 0.25 * markScale}`,
            opacity: Math.min(1, markScale * 1.4),
          }}
        >
          <div
            style={{
              position: 'absolute',
              width: 310,
              height: 310,
              borderRadius: '50%',
              background:
                'radial-gradient(circle, rgba(234,109,32,0.22) 0%, rgba(156,45,110,0.12) 45%, transparent 72%)',
              opacity: ringReveal,
            }}
          />
          <IronbowMark size={164} ringReveal={ringReveal} />
        </div>

        {/* Wordmark + V4 badge */}
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: 22,
            opacity: titleP,
            translate: `0px ${snap((1 - titleP) * 20)}px`,
            filter: blurPx((1 - titleP) * 3),
          }}
        >
          <span
            style={{
              fontFamily: FONT,
              fontSize: 88,
              fontWeight: 650,
              letterSpacing: -2.8,
              lineHeight: 1.02,
              color: C.text,
            }}
          >
            DIY-Thermocam
          </span>
          <span
            style={{
              fontFamily: MONO,
              fontSize: 40,
              fontWeight: 650,
              color: C.ironGold,
              background: 'rgba(234,109,32,0.16)',
              border: '1.5px solid rgba(234,109,32,0.55)',
              borderRadius: 14,
              padding: '8px 20px',
              letterSpacing: -0.5,
            }}
          >
            V4
          </span>
        </div>

        {/* Subtitle */}
        <div
          style={{
            marginTop: 26,
            fontFamily: FONT,
            fontSize: 38,
            fontWeight: 450,
            letterSpacing: -0.6,
            color: '#C7CFDE',
            opacity: subP,
            translate: `0px ${snap((1 - subP) * 16)}px`,
          }}
        >
          Open-source thermal imaging. Rebuilt from the ground up.
        </div>

        {/* Architecture pillars strip */}
        <div
          style={{
            marginTop: 52,
            display: 'flex',
            gap: 16,
            opacity: pillarsP,
            translate: `0px ${snap((1 - pillarsP) * 14)}px`,
          }}
        >
          {PILLARS.map((item) => (
            <div
              key={item}
              style={{
                fontFamily: MONO,
                fontSize: 19,
                fontWeight: 500,
                color: C.muted,
                background: 'rgba(18,22,32,0.85)',
                border: `1px solid ${C.border}`,
                borderRadius: 10,
                padding: '10px 18px',
              }}
            >
              {item}
            </div>
          ))}
        </div>
      </AbsoluteFill>
    </SceneShell>
  )
}
