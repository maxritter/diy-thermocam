import { AbsoluteFill, Img, interpolate, staticFile } from 'remotion'
import { INOUT, OUT, ramp, SceneHeader, SceneShell, snap, StudioCard, useSec } from '../components/Kit'
import { C, FONT, MONO } from '../theme'

const STUDIO_SHOTS = [
  {
    src: 'assets/studio/studio-standard-dark.png',
    label: 'Standard Mode — Clean Live Thermal Capture',
    tag: 'Dark Theme',
    from: 0,
    to: 4.1,
  },
  {
    src: 'assets/studio/studio-live-dark.png',
    label: 'Pro Workbench — Multi-ROI, Loupe & Per-ROI Emissivity',
    tag: 'Pro Mode',
    from: 4.1,
    to: 7.4,
  },
  {
    src: 'assets/studio/studio-editor-light.png',
    label: 'Radiometric .DAT Editor — 1080p / 4K & Zero-FFmpeg MP4',
    tag: 'Light Theme',
    from: 7.4,
    to: 11.2,
  },
] as const

const FEATURES = [
  { title: 'Standard | Pro Modes', detail: 'Switch between simple capture and full radiometric workbench', at: 2.5 },
  { title: 'Per-ROI Emissivity', detail: 'Independent material presets across spots, boxes & lines', at: 5.1 },
  { title: '1080p & 4K Lanczos-3', detail: 'Display-only upscaling preserving raw 16-bit .DAT data', at: 6.9 },
  { title: 'Zero-FFmpeg MP4 Export', detail: 'Built-in H.264 video & report export on macOS, Windows & Linux', at: 9.8 },
] as const

export function StudioScene({ durationSec }: { durationSec: number }) {
  const t = useSec()

  // Beat 1: Desktop Studio screenshots (0.0s -> 10.9s)
  const uiIn = ramp(t, 0.05, 0.65, OUT)
  const uiOut = ramp(t, 10.45, 11.0, INOUT)
  const uiOp = Math.min(uiIn, 1 - uiOut)

  // Beat 2: Real 1920x1080 thermal capture from Max's V3 (10.75s -> end)
  const capIn = ramp(t, 10.75, 11.4, OUT)
  const capZoom = interpolate(t, [10.75, durationSec], [1.0, 1.035], {
    extrapolateLeft: 'clamp',
    extrapolateRight: 'clamp',
  })

  const activeIdx = t < 4.1 ? 0 : t < 7.4 ? 1 : 2
  const activeShot = STUDIO_SHOTS[activeIdx]

  return (
    <SceneShell len={durationSec} fadeIn={0.35} fadeOut={0.4}>
      {/* Beat 1: Tauri v2 Desktop Studio */}
      {uiOp > 0.005 ? (
        <AbsoluteFill style={{ opacity: uiOp }}>
          <SceneHeader
            title="DIY-Thermocam Studio — Unified Desktop App"
            detail="Tauri v2 · Rust · React 19 · WebGL2"
            accent={C.coolBlue}
          />
          <AbsoluteFill
            style={{
              padding: '122px 72px 120px 72px',
              display: 'flex',
              flexDirection: 'row',
              alignItems: 'center',
              gap: 32,
              boxSizing: 'border-box',
            }}
          >
            {/* Left: Exact 16:10 Desktop Studio screenshot frame (1200x794 = 44px bar + 1200x750) */}
            <StudioCard
              label={activeShot.label}
              tag={activeShot.tag}
              accent="rgba(93,163,229,0.52)"
              style={{
                width: 1200,
                height: 794,
                flexShrink: 0,
                translate: `${snap((1 - uiIn) * -28)}px 0px`,
              }}
            >
              {STUDIO_SHOTS.map((shot, idx) => (
                <div
                  key={shot.src}
                  style={{
                    position: 'absolute',
                    inset: 0,
                    background: '#090B10',
                    opacity: idx === activeIdx ? 1 : 0,
                  }}
                >
                  <Img
                    src={staticFile(shot.src)}
                    style={{ width: '100%', height: '100%', objectFit: 'contain' }}
                  />
                </div>
              ))}
            </StudioCard>

            {/* Right: Synchronized Feature Rail */}
            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: 16 }}>
              {FEATURES.map((f, idx) => {
                const active = t >= f.at - 0.15 && (idx === FEATURES.length - 1 || t < FEATURES[idx + 1].at - 0.15)
                const shown = t >= f.at - 0.15
                return (
                  <div
                    key={f.title}
                    style={{
                      padding: '20px 22px',
                      borderRadius: 16,
                      background: active ? 'rgba(93,163,229,0.14)' : C.surface,
                      border: `1.5px solid ${active ? C.coolBlue : shown ? 'rgba(93,163,229,0.3)' : C.border}`,
                      opacity: active ? 1 : shown ? 0.84 : 0.45,
                      display: 'flex',
                      flexDirection: 'column',
                      gap: 7,
                    }}
                  >
                    <div style={{ fontFamily: MONO, fontSize: 21, fontWeight: 650, color: active ? C.text : '#D2D8E4' }}>
                      {f.title}
                    </div>
                    <div style={{ fontFamily: FONT, fontSize: 17, color: C.muted, lineHeight: 1.35 }}>
                      {f.detail}
                    </div>
                  </div>
                )
              })}
            </div>
          </AbsoluteFill>
        </AbsoluteFill>
      ) : null}

      {/* Beat 2: Full-frame Real 1920x1080 Thermal Capture from Max's V3 */}
      {capIn > 0.005 ? (
        <AbsoluteFill style={{ opacity: capIn, background: '#050608' }}>
          <div style={{ position: 'absolute', inset: 0, overflow: 'hidden' }}>
            <Img
              src={staticFile('assets/thermal/real-v3-1080p-capture.png')}
              style={{
                width: '100%',
                height: '100%',
                objectFit: 'contain',
                scale: `${capZoom}`,
              }}
            />
          </div>

          {/* Crisp non-intrusive provenance pill in top-left */}
          <div
            style={{
              position: 'absolute',
              left: 48,
              top: 40,
              display: 'flex',
              alignItems: 'center',
              gap: 14,
              padding: '12px 22px',
              borderRadius: 12,
              background: 'rgba(11,13,18,0.86)',
              border: '1px solid rgba(234,109,32,0.55)',
              boxShadow: '0 14px 40px rgba(0,0,0,0.65)',
            }}
          >
            <div style={{ width: 10, height: 10, borderRadius: 5, background: C.ironOrange }} />
            <span style={{ fontFamily: FONT, fontSize: 22, fontWeight: 600, color: C.text, letterSpacing: -0.3 }}>
              Real 1080p Radiometric Capture
            </span>
            <span style={{ fontFamily: MONO, fontSize: 18, color: C.ironGold }}>
              DIY-Thermocam V3 · Lanczos-3 Export
            </span>
          </div>
        </AbsoluteFill>
      ) : null}
    </SceneShell>
  )
}
