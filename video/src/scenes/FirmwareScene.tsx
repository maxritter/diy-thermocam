import { AbsoluteFill, Img, staticFile } from 'remotion'
import { INOUT, OUT, ramp, SceneHeader, SceneShell, snap, useSec } from '../components/Kit'
import { C, FONT, MONO } from '../theme'

function TftDeviceFrame({
  src,
  label,
  width,
  highlight = false,
}: {
  src: string
  label: string
  width: number
  highlight?: boolean
}) {
  const imgH = Math.round((width - 24) * 0.75)
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 12 }}>
      <div
        style={{
          width,
          padding: 12,
          borderRadius: 20,
          background: 'linear-gradient(180deg, #1B2130 0%, #10141F 100%)',
          border: `1.5px solid ${highlight ? C.ironOrange : C.borderStrong}`,
          boxShadow: highlight
            ? '0 28px 80px rgba(0,0,0,0.72), 0 0 42px rgba(234,109,32,0.16)'
            : '0 24px 64px rgba(0,0,0,0.65)',
        }}
      >
        <div
          style={{
            width: width - 24,
            height: imgH,
            borderRadius: 10,
            overflow: 'hidden',
            background: '#050608',
            border: '1px solid #2A3245',
          }}
        >
          <Img
            src={staticFile(src)}
            style={{
              width: '100%',
              height: '100%',
              objectFit: 'cover',
              imageRendering: 'pixelated',
            }}
          />
        </div>
      </div>
      <div style={{ fontFamily: MONO, fontSize: 18, fontWeight: 500, color: highlight ? C.ironGold : C.muted }}>
        {label}
      </div>
    </div>
  )
}

const METRICS = [
  { value: '8.77 FPS', label: 'Live Radiometric Capture', note: 'Measured over USB & 3.2" TFT', accent: C.ironOrange, at: 8.2 },
  { value: '0.26 ms', label: 'USB Command Latency', note: 'Teensy 4.1 480 Mbps CDC', accent: C.coolBlue, at: 10.8 },
  { value: '9.2 MB/s', label: 'SD Card Transfer', note: '4-bit SDIO bulk export', accent: C.ironGold, at: 13.6 },
  { value: '0 Flicker', label: 'Tear-Free Refresh', note: 'DMA framebuffer updates', accent: '#6CE0A5', at: 17.2 },
] as const

export function FirmwareScene({ durationSec }: { durationSec: number }) {
  const t = useSec()

  // Beat 1: Boot Splash + Live HUD (0.0s -> 3.0s)
  const b1In = ramp(t, 0.05, 0.65, OUT)
  const b1Out = ramp(t, 2.7, 3.15, INOUT)
  const b1Op = Math.min(b1In, 1 - b1Out)

  // Beat 2: Dark Theme + Light Theme + Palette Picker (2.95s -> 7.1s)
  const b2In = ramp(t, 2.95, 3.5, OUT)
  const b2Out = ramp(t, 6.75, 7.2, INOUT)
  const b2Op = Math.min(b2In, 1 - b2Out)

  // Beat 3: Live HUD + Real Measured V3 Telemetry Grid (7.0s -> end)
  const b3In = ramp(t, 7.0, 7.6, OUT)

  return (
    <SceneShell len={durationSec} fadeIn={0.35} fadeOut={0.4}>
      <SceneHeader
        title="Firmware 4.0.0 — On-Device UI & Measured Hardware"
        detail="2× Pixel-Exact 320×240 Simulator & Real V3 Benchmarks"
        accent={C.ironMagenta}
      />

      {/* Beat 1: Boot Splash + Live Radiometric HUD */}
      {b1Op > 0.005 ? (
        <AbsoluteFill
          style={{
            opacity: b1Op,
            padding: '124px 84px 124px 84px',
            display: 'flex',
            flexDirection: 'row',
            alignItems: 'center',
            justifyContent: 'center',
            gap: 56,
            boxSizing: 'border-box',
          }}
        >
          <div style={{ translate: `${snap((1 - b1In) * -30)}px 0px` }}>
            <TftDeviceFrame src="assets/firmware/01-boot-splash.png" label="Boot Splash · Firmware 4.0.0" width={720} />
          </div>
          <div style={{ translate: `${snap((1 - b1In) * 30)}px 0px` }}>
            <TftDeviceFrame src="assets/firmware/02-live-hud.png" label="Live Radiometric HUD · Min / Center / Max" width={780} highlight />
          </div>
        </AbsoluteFill>
      ) : null}

      {/* Beat 2: Dark Theme, Light Theme & Scientific Palette Picker */}
      {b2Op > 0.005 ? (
        <AbsoluteFill
          style={{
            opacity: b2Op,
            padding: '124px 72px 124px 72px',
            display: 'flex',
            flexDirection: 'row',
            alignItems: 'center',
            justifyContent: 'center',
            gap: 32,
            boxSizing: 'border-box',
          }}
        >
          <div style={{ translate: `0px ${snap((1 - b2In) * 24)}px` }}>
            <TftDeviceFrame src="assets/firmware/04-main-menu-dark.png" label="Main Menu · Dark Theme" width={556} highlight />
          </div>
          <div style={{ translate: `0px ${snap((1 - b2In) * 36)}px` }}>
            <TftDeviceFrame src="assets/firmware/04b-main-menu-light.png" label="Main Menu · Light Theme" width={556} />
          </div>
          <div style={{ translate: `0px ${snap((1 - b2In) * 48)}px` }}>
            <TftDeviceFrame src="assets/firmware/05-palette-picker.png" label="Color Palette Picker" width={556} />
          </div>
        </AbsoluteFill>
      ) : null}

      {/* Beat 3: Real V3 Hardware Telemetry + Live HUD */}
      {b3Op(b3In) ? (
        <AbsoluteFill
          style={{
            opacity: b3In,
            padding: '124px 80px 124px 80px',
            display: 'flex',
            flexDirection: 'row',
            alignItems: 'center',
            gap: 44,
            boxSizing: 'border-box',
          }}
        >
          <div style={{ flexShrink: 0, translate: `${snap((1 - b3In) * -28)}px 0px` }}>
            <TftDeviceFrame src="assets/firmware/02-live-hud.png" label="Verified on Real DIY-Thermocam V3" width={800} highlight />
          </div>

          <div
            style={{
              flex: 1,
              display: 'grid',
              gridTemplateColumns: '1fr 1fr',
              gap: 22,
            }}
          >
            {METRICS.map((m) => {
              const cardP = ramp(t, m.at - 0.15, m.at + 0.45, OUT)
              const active = t >= m.at - 0.1
              return (
                <div
                  key={m.label}
                  style={{
                    padding: '30px 26px',
                    borderRadius: 18,
                    background: active ? 'linear-gradient(180deg, #171D2B 0%, #101520 100%)' : C.surface,
                    border: `1.5px solid ${active ? `${m.accent}99` : C.border}`,
                    boxShadow: active ? `0 20px 56px rgba(0,0,0,0.55), 0 0 36px ${m.accent}1A` : '0 16px 40px rgba(0,0,0,0.45)',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: 10,
                    opacity: 0.35 + 0.65 * cardP,
                    translate: `0px ${snap((1 - cardP) * 16)}px`,
                  }}
                >
                  <div
                    style={{
                      fontFamily: MONO,
                      fontSize: 52,
                      fontWeight: 700,
                      letterSpacing: -1.8,
                      color: m.accent,
                      lineHeight: 1.05,
                    }}
                  >
                    {m.value}
                  </div>
                  <div style={{ fontFamily: FONT, fontSize: 22, fontWeight: 650, color: C.text, letterSpacing: -0.3 }}>
                    {m.label}
                  </div>
                  <div style={{ fontFamily: MONO, fontSize: 15, color: C.muted }}>{m.note}</div>
                </div>
              )
            })}
          </div>
        </AbsoluteFill>
      ) : null}
    </SceneShell>
  )
}

function b3Op(v: number) {
  return v > 0.005
}
