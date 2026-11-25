import { AbsoluteFill } from 'remotion'
import { IronbowMark } from '../components/Mark'
import { blurPx, INOUT, OUT, ramp, snap, useSec } from '../components/Kit'
import { C, FONT, MONO } from '../theme'

export function OutroScene({ durationSec }: { durationSec: number }) {
  const t = useSec()
  const rise = (at: number, dist = 16) => {
    const p = ramp(t, at, at + 0.7, OUT)
    return {
      opacity: p,
      translate: `0px ${snap((1 - p) * dist)}px`,
      filter: blurPx((1 - p) * 2),
    }
  }
  const fadeOut = ramp(t, durationSec - 1.15, durationSec, INOUT)
  const fadeIn = ramp(t, 0, 0.4, OUT)

  return (
    <AbsoluteFill style={{ opacity: Math.min(fadeIn, 1 - fadeOut) }}>
      <AbsoluteFill
        style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          paddingBottom: 92,
        }}
      >
        {/* Brand mark + wordmark */}
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: 28,
            ...rise(0.05),
          }}
        >
          <IronbowMark size={108} />
          <span
            style={{
              fontFamily: FONT,
              fontSize: 80,
              fontWeight: 650,
              letterSpacing: -2.6,
              lineHeight: 1,
              color: C.text,
            }}
          >
            DIY-Thermocam V4
          </span>
        </div>

        {/* Plain factual tagline */}
        <div
          style={{
            marginTop: 28,
            fontFamily: FONT,
            fontSize: 34,
            fontWeight: 450,
            letterSpacing: -0.5,
            color: '#C7CFDE',
            ...rise(0.55),
          }}
        >
          Open-source thermal imaging hardware, firmware &amp; desktop Studio
        </div>

        {/* Primary URL + GitHub repository */}
        <div
          style={{
            marginTop: 50,
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            gap: 16,
            ...rise(1.15),
          }}
        >
          <div
            style={{
              fontFamily: MONO,
              fontSize: 40,
              fontWeight: 600,
              color: C.ironGold,
              letterSpacing: -0.3,
              padding: '12px 30px',
              borderRadius: 14,
              background: 'rgba(234,109,32,0.12)',
              border: '1px solid rgba(234,109,32,0.42)',
            }}
          >
            www.diy-thermocam.net
          </div>
          <div
            style={{
              fontFamily: MONO,
              fontSize: 27,
              fontWeight: 500,
              color: C.text,
              letterSpacing: -0.2,
            }}
          >
            github.com/maxritter/diy-thermocam
          </div>
        </div>

        {/* License & stack summary */}
        <div
          style={{
            marginTop: 34,
            fontFamily: MONO,
            fontSize: 20,
            color: C.muted,
            letterSpacing: 0.2,
            ...rise(1.8),
          }}
        >
          GPL-3.0 Open Source · KiCad 10 · build123d CAD · Teensy 4.1 · Tauri v2
        </div>
      </AbsoluteFill>
    </AbsoluteFill>
  )
}
