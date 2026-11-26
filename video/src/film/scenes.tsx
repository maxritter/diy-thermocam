// The launch film's scenes, in order. Times are seconds from each scene's start; `word()` pins a
// motion to the narration (src/vo/film.json), so a re-recorded line keeps the picture in sync.
import { useLayoutEffect, useRef } from 'react'
import { Easing, Img, staticFile, useCurrentFrame } from 'remotion'
import palettes from '../../public/sim/palettes.json'
import grid from '../../public/sim/grid-mug.json'
import hudMug from '../../public/sim/live-mug/hud.json'
import hudBreaker from '../../public/sim/use-breaker/hud.json'
import hudWindow from '../../public/sim/use-window/hud.json'
import hudPcb from '../../public/sim/use-pcb/hud.json'
import assemblePts from './data/assemble-points.json'
import explodePts from './data/explode-points.json'
import {
  C, Callout, Caps, Count, Device, INOUT, lerp, MONO, Pill, POP, ramp, SANS, Screen, Shot, simAt, Tap, Title,
  useT, warp, window_,
} from './kit'
import { FPS, lineAt, scene, word } from './time'

const len = (id: string) => scene(id).length
type Pts = Record<string, [number, number]>[]

// ---------------------------------------------------------------------------------------------
// Hook: a cold mug, barely there; hot coffee pours in and the heat appears
// ---------------------------------------------------------------------------------------------

// Where the screen sits in the first frame of the "open" shot (measured from the render)
const OPEN_SCREEN = { left: 245, top: 4, w: 1424 }

export function Hook() {
  const t = useT()
  return (
    <div style={{ position: 'absolute', ...OPEN_SCREEN, opacity: ramp(t, 0.1, 0.8) }}>
      <Screen seq="pour" t={t} w={OPEN_SCREEN.w} loop={false} />
    </div>
  )
}

// ---------------------------------------------------------------------------------------------
// Open: the live image, then back and round to the camera
// ---------------------------------------------------------------------------------------------

export function Open() {
  const t = useT()
  const L = len('open')
  const u = Math.max(0, (t - 2.0) / (L - 2.0))
  const shotT = 9.5 * (1 - Math.pow(1 - u, 2.4)) // fast pull-back that settles, ending on the shot's last frame
  const titleAt = Math.max(2.8, word('open-1', 'DIY-Thermocam') + 1.0) // after the cut to the studio
  return (
    <>
      {t >= 2.0 ? <Shot name="open" ts={shotT} /> : null}
      {t < 2.0 ? (
        <div style={{ position: 'absolute', inset: 0, background: '#000' }}>
          <div style={{ position: 'absolute', ...OPEN_SCREEN }}>
            <Screen seq="live-mug" t={t} w={OPEN_SCREEN.w} />
          </div>
        </div>
      ) : null}
      <Title t={t} at={titleAt} until={L + 1} title="DIY-Thermocam V4" sub="THE THERMAL CAMERA YOU BUILD YOURSELF" />
    </>
  )
}

// ---------------------------------------------------------------------------------------------
// Sensor: round to the back, the lens cap opens, in to the Lepton, and through it into its pixels
// ---------------------------------------------------------------------------------------------

type Pal = { name: string; stops: string[] }
const PALS = palettes as Pal[]
const GLOW = PALS.find((p) => p.name === 'Glowbow')!.stops.map((h) => [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16)))
const G = grid as { w: number; h: number; t: number[]; min: number; max: number }

function glow(v: number) {
  const x = Math.max(0, Math.min(1, v)) * (GLOW.length - 1)
  const i = Math.min(GLOW.length - 2, Math.floor(x))
  const k = x - i
  return GLOW[i].map((c, j) => Math.round(c + (GLOW[i + 1][j] - c) * k))
}

/** The Lepton's 160 × 120 temperatures as cells; numbers show once the cells are large. */
function PixelGrid({ cell, cx, cy, oy = 540 }: { cell: number; cx: number; cy: number; oy?: number }) {
  const ref = useRef<HTMLCanvasElement>(null)
  const frame = useCurrentFrame()
  useLayoutEffect(() => {
    const ctx = ref.current?.getContext('2d')
    if (!ctx) return
    ctx.clearRect(0, 0, 1920, 1080)
    const x0 = 960 - cx * cell
    const y0 = oy - cy * cell
    const i0 = Math.max(0, Math.floor(-x0 / cell))
    const i1 = Math.min(G.w, Math.ceil((1920 - x0) / cell))
    const j0 = Math.max(0, Math.floor(-y0 / cell))
    const j1 = Math.min(G.h, Math.ceil((1080 - y0) / cell))
    const num = Math.max(0, Math.min(1, (cell - 58) / 30))
    const gap = cell > 24 ? Math.min(3, (cell - 24) / 14) : 0
    ctx.textAlign = 'center'
    ctx.textBaseline = 'middle'
    ctx.font = `500 ${Math.round(cell * 0.25)}px "Geist Mono", ui-monospace, monospace`
    for (let j = j0; j < j1; j++)
      for (let i = i0; i < i1; i++) {
        const v = G.t[j * G.w + i]
        const [r, g, b] = glow((v - G.min) / (G.max - G.min))
        const x = x0 + i * cell
        const y = y0 + j * cell
        ctx.fillStyle = `rgb(${r},${g},${b})`
        ctx.fillRect(x + gap / 2, y + gap / 2, cell - gap + 0.6, cell - gap + 0.6)
        if (num > 0) {
          const lum = 0.3 * r + 0.59 * g + 0.11 * b
          ctx.fillStyle = lum > 140 ? `rgba(20,14,10,${num})` : `rgba(255,255,255,${0.92 * num})`
          ctx.fillText(v.toFixed(1), x + cell / 2, y + cell / 2 + 1)
        }
      }
  }, [frame, cell, cx, cy, oy])
  return <canvas ref={ref} width={1920} height={1080} style={{ position: 'absolute', inset: 0 }} />
}

export function Sensor() {
  const t = useT()
  const L = len('sensor')
  const push = word('sensor-2', 'pixels', 0, 'e') + 0.2 // the shot reaches the lens; we go through it
  const shotT = Math.min(11.2, (t * 11.2) / push)
  const through = ramp(t, push - 0.55, push + 0.25, Easing.in(Easing.cubic))
  const gridIn = ramp(t, push - 0.15, push + 0.3)
  const pullA = word('sensor-2', 'temperature', 0, 'e') + 0.2
  const pullB = pullA + 1.9
  const pull = ramp(t, pullA, pullB, INOUT)
  const FULL = 7.2 // px per cell once the whole image is in view (1152 × 864)
  const cell = Math.exp(lerp(Math.log(lerp(132, 104, ramp(t, push, pullA, (x) => x))), Math.log(FULL), pull))
  const cx = lerp(59.5, 79.5, pull)
  const cy = lerp(63.5, 59.5, pull)
  const oy = lerp(540, 470, pull)
  const range = word('sensor-2', 'minus')
  return (
    <>
      {gridIn < 1 ? (
        <Shot name="sensor" ts={shotT} style={{ scale: String(lerp(1, 4.2, through)), transformOrigin: '50% 50%', filter: through > 0.02 ? `blur(${through * 6}px)` : undefined }} />
      ) : null}
      <Title t={t} at={word('sensor-1', 'FLIR') - 0.2} until={lineAt('sensor-2') - 0.1} title="FLIR Lepton" sub="LONG-WAVE INFRARED · RADIOMETRIC" />
      {gridIn > 0 ? (
        <div style={{ position: 'absolute', inset: 0, opacity: gridIn }}>
          <div style={{ position: 'absolute', inset: 0, background: C.dark }} />
          <PixelGrid cell={cell} cx={cx} cy={cy} oy={oy} />
          <div style={{ position: 'absolute', left: 0, right: 0, top: 932, display: 'flex', justifyContent: 'center', gap: 28 }}>
            <DarkCard t={t} at={pullA + 0.2} until={L + 1}>
              <Count t={t} at={pullA + 0.2} to={19200} dur={1.7} style={{}} format />
              <span style={{ fontSize: 30, color: '#9CA1A8', marginLeft: 16 }}>pixels, each a temperature</span>
            </DarkCard>
            <DarkCard t={t} at={range} until={L + 1}>
              {'−10 to 450 °C'}
            </DarkCard>
          </div>
        </div>
      ) : null}
    </>
  )
}

function DarkCard({ t, at, until, children }: { t: number; at: number; until: number; children: React.ReactNode }) {
  const o = window_(t, at, until, 0.45, 0.3)
  if (o <= 0) return null
  return (
    <div
      style={{
        opacity: o,
        translate: `0 ${lerp(14, 0, ramp(t, at, at + 0.5))}px`,
        padding: '14px 34px',
        borderRadius: 18,
        background: 'rgba(30,32,36,0.9)',
        boxShadow: '0 0 0 1px rgba(255,255,255,0.08)',
        fontFamily: SANS,
        fontWeight: 300,
        fontSize: 54,
        color: '#F3F3F4',
        display: 'flex',
        alignItems: 'baseline',
        whiteSpace: 'nowrap',
      }}
    >
      {children}
    </div>
  )
}

// ---------------------------------------------------------------------------------------------
// Labels that follow a part through a 3D shot (positions from blender/film_shots.py --project)
// ---------------------------------------------------------------------------------------------

function PartLabel({ pts, ts, part, text, side, o, dark = false }: {
  pts: Pts; ts: number; part: string; text: string; side: 'l' | 'r'; o: number; dark?: boolean
}) {
  const row = pts[Math.max(0, Math.min(pts.length - 1, Math.round(ts * FPS)))]
  const p = row?.[part]
  if (!p || o <= 0) return null
  const [x, y] = p
  const lx = side === 'l' ? x - 250 : x + 250
  const ink = dark ? '#EDEEF0' : C.ink
  return (
    <div style={{ position: 'absolute', inset: 0, opacity: o, pointerEvents: 'none' }}>
      <svg style={{ position: 'absolute', inset: 0, overflow: 'visible' }} width="100%" height="100%">
        <circle cx={x} cy={y} r={7} fill={C.amber} />
        <circle cx={x} cy={y} r={15} fill="none" stroke={C.amber} strokeOpacity={0.4} strokeWidth={2} />
        <line x1={x} y1={y} x2={lerp(x, lx, o)} y2={y} stroke={ink} strokeOpacity={0.55} strokeWidth={2} />
      </svg>
      <div style={{ position: 'absolute', top: y - 31, ...(side === 'l' ? { right: 1920 - lx + 10 } : { left: lx + 10 }) }}>
        <Pill dark={dark}>{text}</Pill>
      </div>
    </div>
  )
}

// ---------------------------------------------------------------------------------------------
// Assemble: the finished board, then everything stacks onto it
// ---------------------------------------------------------------------------------------------

/** Film time → assembly shot time: each part lands on its word (shot cues from film_shots.py ARRIVE). */
export function assembleKnots(): [number, number][] {
  const w = (s: string, edge: 's' | 'e' = 's') => word('stack-1', s, 0, edge)
  const close = w('shell', 'e') + 0.1
  return [
    [lineAt('stack-1') - 0.5, 6.0],
    [w('Lepton') - 0.15, 7.0],
    [w('Teensy') - 0.15, 8.0],
    [w('battery') - 0.15, 9.7],
    [w('touchscreen') - 0.15, 10.7],
    [w('Eight') - 0.1, 12.5],
    [close, 14.0],
    [close + 2.6, 16.6],
  ]
}

export function Assemble() {
  const t = useT()
  const stack = lineAt('stack-1')
  const ts = warp(t, assembleKnots())
  const items: [string, string, 'l' | 'r', number][] = [
    ['lepton_module', 'FLIR Lepton 3.5', 'l', word('stack-1', 'Lepton')],
    ['teensy', 'Teensy 4.1', 'r', word('stack-1', 'Teensy')],
    ['lipo', 'LiPo battery', 'l', word('stack-1', 'battery')],
    ['display', '3.2″ touchscreen', 'r', word('stack-1', 'touchscreen')],
    ['shell', 'Four screws each side', 'l', word('stack-1', 'shell', 0, 'e') + 0.2],
  ]
  return (
    <>
      <div style={{ position: 'absolute', inset: 0, opacity: ramp(t, stack - 0.6, stack + 0.1) }}>
        <Shot name="assemble" ts={ts} />
      </div>
      <BoardCard t={t} out={stack - 0.6} />
      <Title t={t} at={word('board-1', 'board') - 0.3} until={stack - 0.4} title="Factory-assembled board" sub="NO SOLDERING" y={60} />
      {items.map(([part, text, side, at], i) => {
        const close = word('stack-1', 'Eight') + 0.1
        const until = i < items.length - 1 ? Math.max(close, at + 1.6) : at + 2.6
        const o = window_(t, at + 0.35, until, 0.4, 0.3)
        return <PartLabel key={part} pts={assemblePts as unknown as Pts} ts={ts} part={part} text={text} side={side} o={o} />
      })}
    </>
  )
}

/** The populated underside of the V4 board (KiCad 3D view, hardware/pcb/fab/diy-thermocam-v4.0-bottom.png
 *  cropped to the board), its parts named as the voice says it is ready. */
function BoardCard({ t, out }: { t: number; out: number }) {
  const o = Math.min(ramp(t, 0, 0.6), 1 - ramp(t, out - 0.2, out + 0.4, INOUT))
  if (o <= 0) return null
  const W = 1000
  const IW = 1230 // the crop's size
  const IH = 932
  const k = W / IW
  const tilt = ramp(t, 0, out, (x) => x)
  const tags: [number, number, string, number, 'l' | 'r'][] = [
    [930, 850, 'USB-C', word('board-1', 'fully') - 0.1, 'l'],
    [1062, 200, '5 V boost', word('board-1', 'assembled'), 'r'],
    [327, 500, 'Teensy socket', word('board-1', 'No') - 0.1, 'r'],
    [653, 440, 'Lepton socket', word('board-1', 'soldering'), 'r'],
  ]
  return (
    <div style={{ position: 'absolute', left: (1920 - W) / 2, top: 230, width: W, height: IH * k, opacity: o, perspective: 2600 }}>
      <div
        style={{
          position: 'absolute',
          inset: 0,
          transform: `rotateX(${lerp(24, 12, tilt)}deg) rotateZ(${lerp(-5, -1, tilt)}deg) scale(${lerp(0.94, 1.0, tilt)})`,
          transformOrigin: '50% 60%',
        }}
      >
        <Img src={staticFile('film/v4.0-board-bottom.png')} style={{ width: W, height: IH * k, filter: 'drop-shadow(0 40px 40px rgba(20,22,26,0.35))' }} />
        {tags.map(([x, y, label, at, side]) => {
          const a = ramp(t, at, at + 0.4, POP)
          if (a <= 0) return null
          return (
            <div key={label} style={{ position: 'absolute', left: x * k, top: y * k, opacity: Math.min(1, a) }}>
              <div style={{ position: 'absolute', left: -9, top: -9, width: 18, height: 18, borderRadius: 9, background: C.amber, boxShadow: '0 0 0 6px rgba(243,109,33,0.25)', scale: String(a) }} />
              <div style={{ position: 'absolute', top: -30, ...(side === 'l' ? { right: 22 } : { left: 22 }) }}>
                <Pill>{label}</Pill>
              </div>
            </div>
          )
        })}
      </div>
    </div>
  )
}

// ---------------------------------------------------------------------------------------------
// Cases: the 3D-printed shell, then the laser-cut acrylic case beside it
// ---------------------------------------------------------------------------------------------

export function Cases() {
  const t = useT()
  const L = len('cases')
  const laser = word('case-1', 'cut')
  const both = word('case-1', 'Both')
  const slide = ramp(t, laser - 0.5, laser + 0.6, INOUT)
  const half = (name: string, x: number, o: number, title: string, sub: string) => (
    <div style={{ position: 'absolute', left: x, top: 0, width: 960, height: 1080, opacity: o }}>
      <Shot name={name} ts={t} soft style={{ width: 960, height: 1080, scale: '0.86', transformOrigin: '50% 36%' }} />
      <div style={{ position: 'absolute', left: 0, right: 0, bottom: 92, textAlign: 'center', translate: `0 ${lerp(16, 0, o)}px` }}>
        <div style={{ fontFamily: SANS, fontSize: 42, fontWeight: 400, color: C.ink }}>{title}</div>
        <Caps style={{ marginTop: 12 }}>{sub}</Caps>
      </div>
    </div>
  )
  return (
    <>
      {half('case-shell', lerp(480, 0, slide), ramp(t, 0, 0.5), '3D-printed shell', 'PA12 OR PETG')}
      {half('case-laser', lerp(1200, 960, slide), ramp(t, laser - 0.3, laser + 0.5), 'Laser-cut acrylic', '3 MM CAST ACRYLIC')}
      <Title t={t} at={both - 0.1} until={L + 1} title="One design, two shells" sub="THE SAME FILES, PRINTED OR CUT" y={70} />
    </>
  )
}

// ---------------------------------------------------------------------------------------------
// Boot: switched on, the splash, the live image, while we push in to the screen
// ---------------------------------------------------------------------------------------------

export function Boot() {
  const t = useT()
  return (
    <>
      <Shot name="boot" ts={t} />
    </>
  )
}

// ---------------------------------------------------------------------------------------------
// Live: the firmware's HUD, explained
// ---------------------------------------------------------------------------------------------

// Where the screen sits in the last frame of the "boot" shot (measured from the render)
export const BOOT_SCREEN = { left: 228, top: -11, w: 1467 }
const LIVE_SCREEN = { left: 440, top: 200, w: 1040 }
const BOOT_END = 6.0 // the live image already runs 3 s into the boot shot

const avg = (pts: number[][], i: number, k: number) => {
  let x = 0
  let y = 0
  let n = 0
  for (let j = Math.max(0, i - 1); j <= Math.min(pts.length - 1, i + 1); j++) {
    x += pts[j][k]
    y += pts[j][k + 1]
    n++
  }
  return [x / n, y / n]
}

export function Live() {
  const t = useT()
  const L = len('live')
  const g = ramp(t, 0, 1.1, INOUT)
  const box = {
    left: lerp(BOOT_SCREEN.left, LIVE_SCREEN.left, g),
    top: lerp(BOOT_SCREEN.top, LIVE_SCREEN.top, g),
    w: lerp(BOOT_SCREEN.w, LIVE_SCREEN.w, g),
  }
  const seqT = t + BOOT_END - 3.0
  const i = simAt('live-mug', seqT)
  const s = LIVE_SCREEN.w / 320
  const [hx, hy] = avg(hudMug, i, 0)
  const [cx, cy] = avg(hudMug, i, 2)
  const P = (x: number, y: number): [number, number] => [LIVE_SCREEN.left + x * s, LIVE_SCREEN.top + y * s]
  const hot = P(hx, hy)
  const cold = P(cx, cy)
  const spot = P(160, 120)
  const nine = word('live-1', 'nine')
  return (
    <>
      <div style={{ position: 'absolute', ...box }}>
        <Screen seq="live-mug" t={seqT} w={box.w} />
      </div>
      {t < 0.7 ? <Shot name="boot" ts={BOOT_END} style={{ opacity: 1 - ramp(t, 0, 0.65, INOUT) }} /> : null}
      <Callout t={t} at={word('live-1', 'hottest')} until={L} x={hot[0]} y={hot[1]} lx={330} ly={hot[1] - 30} label="Hottest" tone={C.amber} />
      <Callout t={t} at={word('live-1', 'coldest')} until={L} x={cold[0]} y={cold[1]} lx={330} ly={cold[1] - 40} label="Coldest" tone={C.cold} />
      <Callout t={t} at={word('live-1', 'center')} until={L} x={spot[0]} y={spot[1] + 6} lx={1600} ly={spot[1] + 40} label="Center" tone="#FFFFFF" r={34} />
      <Title t={t} at={0.9} until={L + 1} dark title="Live on the camera" sub="EVERY PIXEL A TEMPERATURE" y={56} size={52} />
      <div style={{ position: 'absolute', left: 0, right: 0, bottom: 44, display: 'flex', justifyContent: 'center', opacity: window_(t, nine - 0.2, L + 1) }}>
        <Pill dark tone="#3DDC84">Almost 9 frames per second</Pill>
      </div>
    </>
  )
}

// ---------------------------------------------------------------------------------------------
// Palettes and themes
// ---------------------------------------------------------------------------------------------

/** When each palette comes up (s from the scene start); also used for the ticks. */
export function paletteTimes() {
  const a = 0.35
  const themes = word('palette-1', 'Three')
  const per = (themes - 0.3 - a) / 19
  return { a, per, themes, at: Array.from({ length: 19 }, (_, k) => a + k * per) }
}

export function Palettes() {
  const t = useT()
  const L = len('palettes')
  const { a, per, themes } = paletteTimes()
  const k = Math.max(0, Math.min(18, Math.floor((t - a) / per)))
  const frame = k * 5 + (Math.floor(t * 9) % 5)
  const tri = ramp(t, themes - 0.35, themes + 0.45, INOUT)
  const cardW = lerp(1000, 560, tri)
  const row = 1 - ramp(t, themes - 0.4, themes)
  const themeSeq = ['menu-dark', 'menu-light', 'menu-contrast']
  const themeName = ['Dark', 'Light', 'High contrast']
  return (
    <>
      <Title t={t} at={word('palette-1', 'Nineteen') - 0.3} until={themes - 0.3} dark title="19 color palettes" sub="FROM THE FIRMWARE'S OWN TABLES" y={44} size={52} />
      <Title t={t} at={themes} until={L + 1} dark title="Three themes" sub="DARK · LIGHT · HIGH CONTRAST" y={150} size={52} />
      <div style={{ position: 'absolute', left: 0, right: 0, top: lerp(170, 380, tri), display: 'flex', justifyContent: 'center', gap: 44 }}>
        {[0, 1, 2].map((n) => {
          if (n > 0 && tri <= 0) return null
          const o = n === 0 ? 1 : ramp(t, themes + n * 0.18 - 0.2, themes + n * 0.18 + 0.4)
          const seq = tri > 0.5 ? themeSeq[n] : 'palettes'
          return (
            <div key={n} style={{ opacity: o, translate: `0 ${lerp(20, 0, o)}px`, textAlign: 'center' }}>
              <Screen seq={seq} frame={seq === 'palettes' ? frame : 0} w={cardW} style={{ borderRadius: 6 }} />
              <div style={{ marginTop: 24, fontFamily: SANS, fontSize: 32, color: '#E9EAEC', opacity: tri }}>{themeName[n]}</div>
            </div>
          )
        })}
      </div>
      <div style={{ position: 'absolute', left: 0, right: 0, top: 990, display: 'flex', justifyContent: 'center', gap: 10, opacity: row * ramp(t, a - 0.2, a + 0.3) }}>
        {PALS.map((p, j) => {
          const on = j === k
          return (
            <div key={p.name} style={{ position: 'relative' }}>
              <div
                style={{
                  width: 64,
                  height: on ? 24 : 14,
                  marginTop: on ? 0 : 10,
                  borderRadius: 4,
                  background: `linear-gradient(90deg, ${p.stops.join(', ')})`,
                  outline: on ? '2px solid #FFFFFF' : 'none',
                  outlineOffset: 3,
                  opacity: on ? 1 : 0.5,
                }}
              />
            </div>
          )
        })}
      </div>
      <div style={{ position: 'absolute', left: 0, right: 0, top: 1036, textAlign: 'center', opacity: row * ramp(t, a, a + 0.3), fontFamily: MONO, fontSize: 24, color: '#E9EAEC' }}>
        {PALS[k].name}
      </div>
    </>
  )
}

// ---------------------------------------------------------------------------------------------
// Menus: one tap away, screen pushing screen like on the camera
// ---------------------------------------------------------------------------------------------

const MENU_W = 1000
const MENU = { left: (1920 - MENU_W) / 2, top: 120 }

export function menuTimes() {
  return {
    range: word('menu-1', 'range'),
    spots: word('menu-1', 'measuring'),
    photos: word('menu-1', 'photos'),
    video: word('menu-1', 'video'),
    look: word('menu-1', 'look'),
    gallery: word('menu-1', 'gallery'),
  }
}

export function Menu() {
  const t = useT()
  const L = len('menu')
  const m = menuTimes()
  const screens: { seq: string; at: number; play?: boolean }[] = [
    { seq: 'menu-dark', at: 0 },
    { seq: 'range', at: m.range + 0.1 },
    { seq: 'points', at: m.spots + 0.1 },
    { seq: 'video-mode', at: m.photos - 0.35 },
    { seq: 'rec', at: m.video + 0.15, play: true },
    { seq: 'gallery', at: m.look - 0.1 },
    { seq: 'gallery-viewer', at: m.gallery + 0.25 },
  ]
  let pos = 0
  for (let i = 1; i < screens.length; i++) pos += ramp(t, screens[i].at, screens[i].at + 0.5, INOUT)
  const s = MENU_W / 320
  const tile = (x: number, y: number): [number, number] => [MENU.left + x * s, MENU.top + y * s]
  const [rx, ry] = tile(160, 70)
  const [px, py] = tile(58, 107)
  const enter = ramp(t, 0, 0.6)
  return (
    <>
      <div style={{ position: 'absolute', left: MENU.left - 28, top: MENU.top - 28, opacity: enter, scale: String(lerp(0.97, 1, enter)) }}>
        <Device w={MENU_W + 56}>
          <div style={{ position: 'relative', width: MENU_W, height: MENU_W * 0.75, overflow: 'hidden' }}>
            {screens.map((sc, i) => {
              const x = (i - pos) * MENU_W
              if (x <= -MENU_W || x >= MENU_W) return null
              return (
                <div key={sc.seq} style={{ position: 'absolute', left: x, top: 0 }}>
                  <Screen seq={sc.seq} t={sc.play ? t - sc.at : 0} w={MENU_W} />
                </div>
              )
            })}
          </div>
        </Device>
      </div>
      <Tap t={t} at={m.range - 0.15} x={rx} y={ry} />
      <Tap t={t} at={m.spots + 0.75} x={px} y={py} />
      <div style={{ position: 'absolute', left: 0, right: 0, top: MENU.top + MENU_W * 0.75 + 74, display: 'flex', justifyContent: 'center' }}>
        <MenuLabel t={t} at={m.range} until={m.spots - 0.2} text="Fixed or automatic range" />
        <MenuLabel t={t} at={m.spots - 0.1} until={m.photos - 0.2} text="Up to 96 measuring spots" />
        <MenuLabel t={t} at={m.photos} until={m.look - 0.2} text="Photos and video to microSD" />
        <MenuLabel t={t} at={m.look} until={L + 1} text="Gallery on the camera" />
      </div>
    </>
  )
}

function MenuLabel({ t, at, until, text }: { t: number; at: number; until: number; text: string }) {
  const o = window_(t, at, until, 0.4, 0.25)
  if (o <= 0) return null
  return (
    <div style={{ position: 'absolute', opacity: o, translate: `0 ${lerp(10, 0, ramp(t, at, at + 0.4))}px` }}>
      <Pill dark>{text}</Pill>
    </div>
  )
}

// ---------------------------------------------------------------------------------------------
// Uses: one full picture per beat, the marker locking on to what matters
// ---------------------------------------------------------------------------------------------

const USE_W = 1240
const USE = { left: (1920 - USE_W) / 2, top: 70 }

export function useTimes() {
  return [word('use-1', 'overloaded') - 0.3, word('use-1', 'The', 1) - 0.2, word('use-1', 'The', 2) - 0.2]
}

export function Uses() {
  const t = useT()
  const L = len('uses')
  const at = useTimes()
  const items: { seq: string; label: string; hud: number[][]; k: number; tone: string }[] = [
    { seq: 'use-breaker', label: 'Overloaded breaker', hud: hudBreaker, k: 0, tone: C.amber },
    { seq: 'use-window', label: 'Drafty window', hud: hudWindow, k: 2, tone: C.cold },
    { seq: 'use-pcb', label: 'The part that runs hot', hud: hudPcb, k: 0, tone: C.amber },
  ]
  const n = t < at[1] ? 0 : t < at[2] ? 1 : 2
  const it = items[n]
  const local = t - at[n]
  const seqT = local + 0.5
  const i = simAt(it.seq, seqT)
  const s = USE_W / 320
  const [hx, hy] = avg(it.hud, i, it.k)
  const x = USE.left + hx * s
  const y = USE.top + hy * s
  const push = lerp(1.0, 1.035, ramp(local, 0, 3, (v) => v))
  return (
    <>
      <div style={{ position: 'absolute', ...USE, opacity: n === 0 ? ramp(t, at[0] - 0.2, at[0] + 0.2) : 1, scale: String(push) }}>
        <Screen seq={it.seq} t={seqT} w={USE_W} />
      </div>
      <Callout t={local} at={0.35} until={(n < 2 ? at[n + 1] - at[n] : L - at[n]) + 1} x={x} y={y} lx={n === 2 ? x - 60 : x < 960 ? x + 230 : x - 230} ly={n === 2 ? y - 170 : y - 70} label={it.label} tone={it.tone} r={40} />
    </>
  )
}

// ---------------------------------------------------------------------------------------------
// Battery
// ---------------------------------------------------------------------------------------------

export function Battery() {
  const t = useT()
  const L = len('battery')
  const hours = word('battery-1', 'up')
  const usb = word('battery-1', 'charges')
  return (
    <>
      <Img
        src={staticFile('film/v4.0-part-lipo.png')}
        style={{
          position: 'absolute',
          left: 150,
          top: 230,
          width: 800,
          opacity: ramp(t, 0, 0.7),
          translate: `${lerp(-30, 0, ramp(t, 0, 1.2))}px 0`,
          rotate: `${lerp(-3, 0, ramp(t, 0, 1.2))}deg`,
        }}
      />
      <div style={{ position: 'absolute', left: 1060, top: 300, width: 760 }}>
        <Caps style={{ opacity: ramp(t, 0.2, 0.7) }}>1200 OR 2000 MAH LITHIUM-POLYMER</Caps>
        <div style={{ marginTop: 10, fontFamily: SANS, fontWeight: 300, fontSize: 170, lineHeight: 1.05, color: C.ink, letterSpacing: '-0.03em', opacity: ramp(t, hours - 0.2, hours + 0.3) }}>
          Up to 6 h
        </div>
        <div style={{ marginTop: 6, fontFamily: SANS, fontWeight: 400, fontSize: 36, color: C.inkSoft, opacity: ramp(t, hours + 0.2, hours + 0.6) }}>
          with the 2000 mAh cell
        </div>
        <div style={{ marginTop: 40, opacity: ramp(t, usb, usb + 0.4), translate: `0 ${lerp(10, 0, ramp(t, usb, usb + 0.4))}px` }}>
          <Pill tone="#3DDC84">Charges over USB-C</Pill>
        </div>
      </div>
      <Title t={t} at={0.2} until={L + 1} title="Battery" y={80} />
    </>
  )
}

// ---------------------------------------------------------------------------------------------
// Studio: one cable, the same image on the computer, then regions, alarms and export
// ---------------------------------------------------------------------------------------------

const SW = 2752
const SH = 2244
type Focus = [number, number, number] // centre x, y in screenshot pixels, zoom (1 = screenshot width fills the window)
const LIVE_VIEW = { x: 178, y: 580, w: 1671, h: 1253 } // the image area in studio-live.png

function focusAt(t: number, focus: Focus[], at: number[]): Focus {
  let f = focus[0]
  for (let i = 1; i < focus.length; i++) {
    const k = ramp(t, at[i], at[i] + 0.9, INOUT)
    f = [lerp(f[0], focus[i][0], k), lerp(f[1], focus[i][1], k), Math.exp(lerp(Math.log(f[2]), Math.log(focus[i][2]), k))]
  }
  return f
}

/** A Studio screenshot in a window; `over` draws on top in screenshot pixels (mapped for you). */
function StudioWin({ src, f, left, top, width, height, o = 1, over }: {
  src: string; f: Focus; left: number; top: number; width: number; height: number; o?: number
  over?: (map: (x: number, y: number) => [number, number], scale: number) => React.ReactNode
}) {
  const scale = (width / SW) * f[2]
  const tx = width / 2 - f[0] * scale
  const ty = height / 2 - f[1] * scale
  const map = (x: number, y: number): [number, number] => [tx + x * scale, ty + y * scale]
  return (
    <div
      style={{
        position: 'absolute',
        left,
        top,
        width,
        height,
        borderRadius: 16,
        overflow: 'hidden',
        background: '#0B0C0E',
        boxShadow: '0 50px 100px -40px rgba(20,22,26,0.55), 0 0 0 1px rgba(20,22,26,0.12)',
        opacity: o,
      }}
    >
      <Img src={staticFile(`film/${src}`)} style={{ position: 'absolute', left: 0, top: 0, width: SW, height: SH, transformOrigin: '0 0', translate: `${tx}px ${ty}px`, scale: String(scale) }} />
      {(() => {
        // The screenshots were taken with the demo source; name the camera instead
        const [x, y] = map(2413, 25)
        return (
          <div style={{ position: 'absolute', left: x, top: y, width: 186 * scale, height: 44 * scale, background: 'rgb(15,16,18)', display: 'flex', alignItems: 'center', fontFamily: SANS, fontWeight: 500, fontSize: 27 * scale, color: '#E8E9EB', whiteSpace: 'nowrap', overflow: 'hidden' }}>
            Thermocam V4
          </div>
        )
      })()}
      {over ? over(map, scale) : null}
    </div>
  )
}

export function studioTimes() {
  return {
    cable: word('studio-1', 'cable'),
    studio: word('studio-1', 'Studio'),
    same: word('studio-1', 'same'),
    draw: lineAt('studio-2'),
    regions: word('studio-2', 'regions'),
    alarms: word('studio-2', 'alarms'),
    exp: word('studio-2', 'export'),
    mac: word('studio-2', 'Mac'),
  }
}

export function Studio() {
  const t = useT()
  const L = len('studio')
  const m = studioTimes()
  const grow = ramp(t, m.draw - 0.7, m.draw + 0.3, INOUT)
  // Phase A: camera on the left, cable, Studio on the right showing the same image
  const devW = 440
  const dev = { left: lerp(150, -500, grow), top: 400 }
  const winA = { left: 760, top: 170, width: 1000, height: (1000 * SH) / SW }
  const winB = { left: 190, top: 150, width: 1540, height: 860 }
  const win = {
    left: lerp(winA.left, winB.left, grow),
    top: lerp(winA.top, winB.top, grow),
    width: lerp(winA.width, winB.width, grow),
    height: lerp(winA.height, winB.height, grow),
  }
  const showLive = t < m.draw + 0.3
  const showMeasure = t >= m.draw - 0.2
  const plug = ramp(t, m.cable - 0.5, m.cable + 0.05, INOUT)
  const live = ramp(t, m.same - 0.4, m.same + 0.2)
  // Cable from the camera's right edge to the window's left edge
  const ax = dev.left + devW + 40
  const ay = dev.top + devW * 0.75 * 0.75
  const bx = win.left - 6
  const by = win.top + win.height * 0.62
  const pulse = (t - m.cable) % 1.1
  const BOX = { x: 1127, y: 622, w: 198, h: 229 } // Box 1 in studio-measure.png
  const CHART = { x0: 160, x1: 1235, y60: 1998 } // the 60 ° line of "Over time"
  const EXPORT = { x: 1921, y: 146 } // the Export button
  const measureF = focusAt(t, [[1300, 1050, 1.0], [1226, 760, 1.75], [760, 1960, 1.55], [1700, 620, 1.12], [1376, 1050, 1.0]],
    [0, m.regions - 0.4, m.alarms - 0.45, m.exp - 0.5, m.mac])
  return (
    <>
      <Title t={t} at={m.studio - 0.5} until={m.draw - 0.6} title="DIY-Thermocam Studio" sub="MACOS · WINDOWS · LINUX" y={60} size={54} />
      {grow < 1 ? (
        <>
          <svg style={{ position: 'absolute', inset: 0, overflow: 'visible', opacity: (1 - grow) * ramp(t, m.cable - 0.6, m.cable - 0.3) }} width="100%" height="100%">
            <path d={`M ${ax} ${ay} C ${ax + 160} ${ay + 130}, ${bx - 180} ${by + 130}, ${bx} ${by}`} pathLength={1} strokeDasharray="1 1" strokeDashoffset={1 - plug} fill="none" stroke="#2A2D33" strokeWidth={7} strokeLinecap="round" />
          </svg>
          <CablePulse ax={ax} ay={ay} bx={bx} by={by} k={pulse / 1.1} on={t > m.cable && grow < 0.5} />
          <div style={{ position: 'absolute', ...dev, opacity: ramp(t, 0, 0.6) * (1 - grow) }}>
            <Device w={devW + 28} light>
              <Screen seq="live-mug" t={t + 2.0} w={devW} />
            </Device>
          </div>
        </>
      ) : null}
      <div style={{ opacity: ramp(t, 0.1, 0.8) }}>
        {showLive ? (
          <StudioWin
            src="studio-live.png"
            f={[SW / 2, SH / 2, 1]}
            {...win}
            over={(map, scale) => {
              const [x, y] = map(LIVE_VIEW.x, LIVE_VIEW.y)
              return (
                <div style={{ position: 'absolute', left: x, top: y, width: LIVE_VIEW.w * scale, height: LIVE_VIEW.h * scale, opacity: live }}>
                  <Screen seq="studio-mug" t={t + 2.0} w={LIVE_VIEW.w * scale} style={{ imageRendering: 'auto' }} />
                </div>
              )
            }}
          />
        ) : null}
        {showMeasure ? (
          <StudioWin
            src="studio-measure.png"
            f={measureF}
            {...win}
            o={ramp(t, m.draw - 0.2, m.draw + 0.2)}
            over={(map, scale) => {
              const draw = ramp(t, m.regions, m.regions + 0.7, INOUT)
              const [bx0, by0] = map(BOX.x, BOX.y)
              const alarm = ramp(t, m.alarms + 0.1, m.alarms + 0.8, INOUT)
              const [lx0, ly] = map(CHART.x0, CHART.y60)
              const [lx1] = map(CHART.x1, CHART.y60)
              const [ex, ey] = map(EXPORT.x, EXPORT.y)
              const file = ramp(t, m.exp + 0.35, m.exp + 0.85, POP)
              return (
                <>
                  <svg style={{ position: 'absolute', inset: 0, overflow: 'visible' }} width="100%" height="100%">
                    {t > m.regions - 0.1 ? (
                      <rect x={bx0} y={by0} width={BOX.w * scale * draw} height={BOX.h * scale * draw} fill="rgba(243,109,33,0.08)" stroke={C.amber} strokeWidth={4} opacity={window_(t, m.regions, m.alarms + 0.2, 0.2, 0.3)} />
                    ) : null}
                    {t > m.alarms ? (
                      <line x1={lx0} y1={ly} x2={lerp(lx0, lx1, alarm)} y2={ly} stroke={C.amber} strokeWidth={4} strokeDasharray="14 10" opacity={window_(t, m.alarms, m.exp - 0.1, 0.2, 0.3)} />
                    ) : null}
                  </svg>
                  {t > m.regions + 0.5 && t < m.alarms + 0.2 ? (
                    <div style={{ position: 'absolute', left: bx0 + BOX.w * scale + 18, top: by0 - 4, opacity: window_(t, m.regions + 0.5, m.alarms + 0.2, 0.3, 0.3) }}>
                      <Pill dark>
                        max <Count t={t} at={m.regions + 0.5} from={41} to={76.3} dur={0.9} decimals={1} /> °C
                      </Pill>
                    </div>
                  ) : null}
                  {t > m.alarms + 0.5 && t < m.exp ? (
                    <div style={{ position: 'absolute', left: lerp(lx0, lx1, 0.62), top: ly - 78, opacity: window_(t, m.alarms + 0.5, m.exp - 0.1, 0.3, 0.3) }}>
                      <Pill dark tone={C.amber}>Alarm above 60 °C</Pill>
                    </div>
                  ) : null}
                  {t > m.exp - 0.3 && t < m.mac + 0.6 ? (
                    <>
                      <div style={{ position: 'absolute', left: 0, top: 0 }}>
                        <Tap t={t} at={m.exp + 0.05} x={ex} y={ey} />
                      </div>
                      <div style={{ position: 'absolute', left: ex - 470, top: ey + 70 + lerp(-20, 0, file), opacity: file * (1 - ramp(t, m.mac, m.mac + 0.6)), display: 'flex', gap: 14 }}>
                        <Pill dark tone="#3DDC84">capture-14-32.mp4</Pill>
                        <Pill dark tone={C.cold}>.tiff</Pill>
                      </div>
                    </>
                  ) : null}
                </>
              )
            }}
          />
        ) : null}
      </div>
      <div style={{ position: 'absolute', left: 0, right: 0, top: 58, display: 'flex', justifyContent: 'center', gap: 16 }}>
        {[['Regions', m.regions], ['Alarms', m.alarms], ['Export', m.exp]].map(([s, a]) => (
          <Chip key={s as string} t={t} at={a as number} until={m.mac - 0.2} text={s as string} />
        ))}
      </div>
      <div style={{ position: 'absolute', left: 0, right: 0, top: 58, display: 'flex', justifyContent: 'center', gap: 16 }}>
        {[['macOS', m.mac], ['Windows', word('studio-2', 'Windows')], ['Linux', word('studio-2', 'Linux')]].map(([s, a]) => (
          <Chip key={s as string} t={t} at={a as number} until={L + 1} text={s as string} tone={C.ink} />
        ))}
      </div>
    </>
  )
}

function CablePulse({ ax, ay, bx, by, k, on }: { ax: number; ay: number; bx: number; by: number; k: number; on: boolean }) {
  if (!on) return null
  // Point on the same cubic as the cable
  const p0 = [ax, ay]
  const p1 = [ax + 160, ay + 130]
  const p2 = [bx - 180, by + 130]
  const p3 = [bx, by]
  const u = 1 - k
  const pt = [0, 1].map((i) => u * u * u * p0[i] + 3 * u * u * k * p1[i] + 3 * u * k * k * p2[i] + k * k * k * p3[i])
  return (
    <svg style={{ position: 'absolute', inset: 0, overflow: 'visible' }} width="100%" height="100%">
      <circle cx={pt[0]} cy={pt[1]} r={9} fill={C.amber} opacity={Math.sin(Math.PI * k)} />
      <circle cx={pt[0]} cy={pt[1]} r={18} fill={C.amber} opacity={0.25 * Math.sin(Math.PI * k)} />
    </svg>
  )
}

function Chip({ t, at, until, text, tone }: { t: number; at: number; until: number; text: string; tone?: string }) {
  const o = window_(t, at, until, 0.35, 0.25)
  if (o <= 0) return null
  return (
    <div style={{ opacity: o, translate: `0 ${lerp(12, 0, ramp(t, at, at + 0.4))}px` }}>
      <Pill tone={tone}>{text}</Pill>
    </div>
  )
}

// ---------------------------------------------------------------------------------------------
// Price
// ---------------------------------------------------------------------------------------------

export function priceLands() {
  return word('price-1', 'dollars', 0, 'e')
}

export function Price() {
  const t = useT()
  const k = ramp(t, 0, 1.2)
  const all = word('price-1', 'All')
  const land = priceLands()
  return (
    <>
      <Img
        src={staticFile('film/v4.0-3d-kit.jpg')}
        style={{
          position: 'absolute',
          left: 60,
          top: 150,
          width: 1060,
          opacity: k,
          scale: String(lerp(1.05, 1, k)),
          maskImage: 'radial-gradient(ellipse 50% 50% at 50% 50%, #000 58%, transparent 100%)',
          WebkitMaskImage: 'radial-gradient(ellipse 50% 50% at 50% 50%, #000 58%, transparent 100%)',
        }}
      />
      <div style={{ position: 'absolute', left: 1180, top: 360 }}>
        <Caps style={{ opacity: ramp(t, all, all + 0.5) }}>ALL PARTS, INCLUDING THE SENSOR</Caps>
        <div style={{ marginTop: 14, fontFamily: SANS, fontWeight: 300, fontSize: 168, letterSpacing: '-0.03em', color: C.ink, opacity: ramp(t, all + 0.4, all + 0.8) }}>
          $<Count t={t} at={all + 0.5} to={293} dur={land - all - 0.5} />
        </div>
        <div style={{ marginTop: 4, fontFamily: SANS, fontSize: 34, color: C.inkSoft, opacity: ramp(t, land + 0.4, land + 0.9) }}>
          About $121 without the sensor
        </div>
      </div>
    </>
  )
}

// ---------------------------------------------------------------------------------------------
// End: the camera opens into its layers one last time, closes, and the address lands
// ---------------------------------------------------------------------------------------------

function endKnots(): [number, number][] {
  const e1 = lineAt('end-1')
  const e2 = lineAt('end-2')
  return [
    [0, 0.3],
    [e1 - 0.3, 0.6],
    [e1 + 1.5, 2.8],
    [e2 - 0.5, 6.0],
    [e2 + 1.3, 8.0],
    [e2 + 4.5, 10.0],
  ]
}

export function endLands() {
  return word('end-2', 'diy-thermocam.net') - 0.2
}

export function End() {
  const t = useT()
  const ts = warp(t, endKnots())
  const e2 = lineAt('end-2')
  const site = endLands()
  const tags: [string, string, 'l' | 'r', number][] = [
    ['pcba', 'Board files', 'l', word('end-1', 'board')],
    ['front_shell', 'Shell model', 'r', word('end-1', 'shell')],
    ['teensy', 'Firmware source', 'r', word('end-1', 'firmware')],
  ]
  return (
    <>
      <Shot name="explode" ts={ts} />
      {tags.map(([part, text, side, at]) => (
        <PartLabel key={part} pts={explodePts as unknown as Pts} ts={ts} part={part} text={text} side={side} o={window_(t, at, e2 - 0.4, 0.4, 0.4)} />
      ))}
      {(() => {
        const at = word('end-1', 'Studio')
        const o = window_(t, at, e2 - 0.4, 0.4, 0.4)
        return o > 0 ? (
          <div style={{ position: 'absolute', left: 200, top: 130, width: 340, opacity: o, translate: `0 ${lerp(12, 0, ramp(t, at, at + 0.4))}px`, textAlign: 'center' }}>
            <Img src={staticFile('film/studio-live.png')} style={{ width: 340, borderRadius: 10, boxShadow: '0 30px 60px -28px rgba(20,22,26,0.55), 0 0 0 1px rgba(20,22,26,0.1)' }} />
            <div style={{ marginTop: 16, display: 'inline-block' }}>
              <Pill tone={C.cold}>Studio app</Pill>
            </div>
          </div>
        ) : null
      })()}
      <div style={{ position: 'absolute', left: 0, right: 0, bottom: 96, textAlign: 'center', opacity: ramp(t, site - 0.2, site + 0.5), translate: `0 ${lerp(14, 0, ramp(t, site - 0.2, site + 0.6))}px` }}>
        <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', gap: 24 }}>
          <Img src={staticFile('film/logo-mark.svg')} style={{ width: 62, height: 62, scale: String(lerp(0.6, 1, ramp(t, site - 0.2, site + 0.5, POP))) }} />
          <div style={{ fontFamily: SANS, fontWeight: 300, fontSize: 68, letterSpacing: '-0.015em', color: C.ink }}>diy-thermocam.net</div>
        </div>
        <Caps style={{ marginTop: 18, opacity: ramp(t, site + 0.8, site + 1.4) }}>FREE TO BUILD FOR NON-COMMERCIAL USE</Caps>
      </div>
    </>
  )
}

