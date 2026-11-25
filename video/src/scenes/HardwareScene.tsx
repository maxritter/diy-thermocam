import { AbsoluteFill, Img, staticFile } from 'remotion'
import { INOUT, OUT, ramp, SceneHeader, SceneShell, snap, StudioCard, useSec } from '../components/Kit'
import { C, FONT, MONO } from '../theme'

const ASSEMBLY_STEPS = [
  {
    num: '01',
    title: 'Plug Modules into V4 PCBA',
    detail: 'Teensy 4.1, FLIR Lepton 3.5 + Breakout V2, slim JST-PH LiPo',
    img: 'assets/pcb/diy-thermocam-v4.0-bottom.png',
    darkCardBg: true,
  },
  {
    num: '02',
    title: 'Seat Board in Back Shell',
    detail: '1/4"-20 tripod nut, slide lens cap, capture plunger & switch shuttle',
    img: 'assets/enclosure/v4-assembly-step-2-back-shell.png',
    darkCardBg: false,
  },
  {
    num: '03',
    title: 'Add LiPo Lid & 3.2" TFT',
    detail: '0.6 mm insulating roof lid + 2×20 header display stack',
    img: 'assets/enclosure/v4-assembly-step-3-display-stack.png',
    darkCardBg: false,
  },
  {
    num: '04',
    title: 'Close Front Shell & 8 Screws',
    detail: '4× M2.5×6 front + 4× M2.5×8 back into brass standoffs',
    img: 'assets/enclosure/v4-assembly-step-4-close-screws.png',
    darkCardBg: false,
  },
] as const

export function HardwareScene({ durationSec }: { durationSec: number }) {
  const t = useSec()

  // Beat 1: KiCad 10 V4.0 PCB top & bottom (0.0s -> 3.9s)
  const pcbIn = ramp(t, 0.05, 0.75, OUT)
  const pcbOut = ramp(t, 3.65, 4.1, INOUT)
  const pcbOpacity = Math.min(pcbIn, 1 - pcbOut)

  // Beat 2: 4-step screwdriver-only CAD assembly (3.85s -> 8.2s)
  const asmIn = ramp(t, 3.85, 4.3, OUT)
  const asmOut = ramp(t, 7.95, 8.4, INOUT)
  const asmOpacity = Math.min(asmIn, 1 - asmOut)
  const activeStep = t < 5.0 ? 0 : t < 6.0 ? 1 : t < 7.0 ? 2 : 3

  // Beat 3: 3D-printed/CNC & 2D laser-cut enclosures (8.15s -> end)
  const encIn = ramp(t, 8.15, 8.75, OUT)

  return (
    <SceneShell len={durationSec} fadeIn={0.35} fadeOut={0.4}>
      <SceneHeader
        title="Zero-Solder V4 Hardware & Enclosures"
        detail="KiCad 10 Factory PCBA · build123d Parametric CAD"
        accent={C.ironOrange}
      />

      {/* Beat 1: KiCad 10 V4.0 PCB Renders */}
      {pcbOpacity > 0.005 ? (
        <AbsoluteFill
          style={{
            opacity: pcbOpacity,
            padding: '126px 84px 128px 84px',
            display: 'flex',
            flexDirection: 'column',
            gap: 24,
          }}
        >
          <div style={{ display: 'flex', gap: 32, flex: 1, minHeight: 0 }}>
            <StudioCard
              label="V4.0 Mainboard — Top (Display Side)"
              tag="ER-TFTM032-3 · 2×20 Header"
              accent="rgba(234,109,32,0.45)"
              style={{
                flex: 1,
                translate: `${snap((1 - pcbIn) * -36)}px 0px`,
                transform: `perspective(2200px) rotateY(${(1 - pcbIn) * 7}deg)`,
              }}
            >
              <div style={{ position: 'absolute', inset: 0, background: '#171C28', display: 'flex', alignItems: 'center', justifyContent: 'center', padding: 14 }}>
                <Img
                  src={staticFile('assets/pcb/diy-thermocam-v4.0-top.png')}
                  style={{ width: '100%', height: '100%', objectFit: 'contain', borderRadius: 8 }}
                />
              </div>
            </StudioCard>

            <StudioCard
              label="V4.0 Mainboard — Bottom (Pre-Soldered Sockets)"
              tag="Zero Soldering · USB-C"
              accent="rgba(93,163,229,0.45)"
              style={{
                flex: 1,
                translate: `${snap((1 - pcbIn) * 36)}px 0px`,
                transform: `perspective(2200px) rotateY(${(1 - pcbIn) * -7}deg)`,
              }}
            >
              <div style={{ position: 'absolute', inset: 0, background: '#171C28', display: 'flex', alignItems: 'center', justifyContent: 'center', padding: 14 }}>
                <Img
                  src={staticFile('assets/pcb/diy-thermocam-v4.0-bottom.png')}
                  style={{ width: '100%', height: '100%', objectFit: 'contain', borderRadius: 8 }}
                />
              </div>
            </StudioCard>
          </div>

          <div style={{ display: 'flex', justifyContent: 'center', gap: 18 }}>
            {['Factory PCBA (JLCPCB / PCBWay / Seeed)', 'Plug-In Teensy 4.1 & FLIR Lepton', 'On-Board 5V Boost + LiPo Charger', 'Board-Edge USB-C'].map((chip) => (
              <div
                key={chip}
                style={{
                  fontFamily: MONO,
                  fontSize: 17,
                  color: C.text,
                  background: C.surface,
                  border: `1px solid ${C.border}`,
                  borderRadius: 9,
                  padding: '8px 16px',
                }}
              >
                {chip}
              </div>
            ))}
          </div>
        </AbsoluteFill>
      ) : null}

      {/* Beat 2: Animated 4-Step Screwdriver-Only Assembly */}
      {asmOpacity > 0.005 ? (
        <AbsoluteFill
          style={{
            opacity: asmOpacity,
            padding: '124px 80px 120px 80px',
            display: 'flex',
            flexDirection: 'row',
            alignItems: 'stretch',
            gap: 34,
            boxSizing: 'border-box',
          }}
        >
          {/* Left: Active CAD Assembly Step Render */}
          <StudioCard
            label={`Screwdriver-Only Assembly — Step ${activeStep + 1} of 4`}
            tag="Zero Soldering"
            accent="rgba(234,109,32,0.5)"
            lightBg={!ASSEMBLY_STEPS[activeStep].darkCardBg}
            style={{
              width: 1020,
              flexShrink: 0,
              translate: `0px ${snap((1 - asmIn) * 20)}px`,
            }}
          >
            {ASSEMBLY_STEPS.map((step, idx) => {
              const isCurrent = idx === activeStep
              return (
                <div
                  key={step.num}
                  style={{
                    position: 'absolute',
                    inset: 0,
                    background: step.darkCardBg ? '#171C28' : '#F5F7FA',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    padding: 14,
                    opacity: isCurrent ? 1 : 0,
                  }}
                >
                  <Img
                    src={staticFile(step.img)}
                    style={{ width: '100%', height: '100%', objectFit: 'contain', borderRadius: 6 }}
                  />
                </div>
              )
            })}
          </StudioCard>

          {/* Right: 4-Step Checklist Rail */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', justifyContent: 'center', gap: 16 }}>
            {ASSEMBLY_STEPS.map((step, idx) => {
              const active = idx === activeStep
              const done = idx < activeStep
              return (
                <div
                  key={step.num}
                  style={{
                    padding: '20px 22px',
                    borderRadius: 16,
                    background: active ? 'rgba(234,109,32,0.14)' : C.surface,
                    border: `1.5px solid ${active ? C.ironOrange : done ? 'rgba(93,163,229,0.35)' : C.border}`,
                    display: 'flex',
                    alignItems: 'flex-start',
                    gap: 18,
                    opacity: active ? 1 : done ? 0.82 : 0.48,
                  }}
                >
                  <div
                    style={{
                      width: 44,
                      height: 44,
                      borderRadius: 11,
                      background: active ? C.ironOrange : done ? 'rgba(93,163,229,0.18)' : C.surface2,
                      color: active ? '#0B0D12' : done ? C.coolBlue : C.muted,
                      fontFamily: MONO,
                      fontSize: 19,
                      fontWeight: 700,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      flexShrink: 0,
                    }}
                  >
                    {step.num}
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: 5 }}>
                    <div style={{ fontFamily: FONT, fontSize: 23, fontWeight: 650, color: C.text, letterSpacing: -0.4 }}>
                      {step.title}
                    </div>
                    <div style={{ fontFamily: FONT, fontSize: 17, color: C.muted, lineHeight: 1.35 }}>
                      {step.detail}
                    </div>
                  </div>
                </div>
              )
            })}
          </div>
        </AbsoluteFill>
      ) : null}

      {/* Beat 3: 3D-Printed / CNC & 2D Laser-Cut Enclosures */}
      {encIn > 0.005 ? (
        <AbsoluteFill
          style={{
            opacity: encIn,
            padding: '124px 80px 120px 80px',
            display: 'flex',
            flexDirection: 'row',
            alignItems: 'stretch',
            gap: 32,
            boxSizing: 'border-box',
          }}
        >
          <StudioCard
            label="3D-Printable / CNC Enclosure"
            tag="Support-Free FDM · Zero-Vignette 119° Cone"
            accent="rgba(234,109,32,0.5)"
            lightBg
            style={{
              flex: 1.04,
              translate: `${snap((1 - encIn) * -32)}px 0px`,
            }}
          >
            <div style={{ position: 'absolute', inset: 0, background: '#F5F7FA', padding: 10 }}>
              <Img
                src={staticFile('assets/enclosure/v4.0-3d-enclosure-hero.png')}
                style={{ width: '100%', height: '100%', objectFit: 'contain' }}
              />
            </div>
          </StudioCard>

          <StudioCard
            label="6-Plate Laser-Cut Acrylic Enclosure"
            tag="3 mm Acryl GS · SVG / DXF"
            accent="rgba(93,163,229,0.45)"
            lightBg
            style={{
              flex: 1,
              translate: `${snap((1 - encIn) * 32)}px 0px`,
            }}
          >
            <div style={{ position: 'absolute', inset: 0, background: '#F5F7FA', padding: 10 }}>
              <Img
                src={staticFile('assets/enclosure/v4.0-laser-enclosure-preview.png')}
                style={{ width: '100%', height: '100%', objectFit: 'contain' }}
              />
            </div>
          </StudioCard>
        </AbsoluteFill>
      ) : null}
    </SceneShell>
  )
}
