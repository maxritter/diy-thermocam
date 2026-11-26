/**
 * The browser preview's demo camera picture: the same eight real sample captures the app's
 * Rust demo camera plays (`thermocam_core::demo_scenes`), in turn, with a little sensor noise.
 * They load from `public/demo/`; until they arrive the generated scene stands in.
 */

import type { ThermalFrame } from "./types";

export const DEMO_SCENES = [
  "HOUSE",
  "SOLAR",
  "PCB",
  "BREAKER",
  "PRINTER",
  "PAN",
  "BEEHIVE",
  "FRIDGE",
] as const;

/** How long each capture shows: about ten seconds at the demo's 9 fps. */
export const FRAMES_PER_SCENE = 90;

const W = 160;
const H = 120;
const KELVIN = 273.15;

export interface DemoScene {
  name: string;
  /** °C, row by row, 160 × 120. */
  celsius: Float32Array;
}

/** A 160 × 120 `.DAT` (38,421 or 38,805 bytes) as °C, using the file's own calibration. */
export function parseDatCelsius(bytes: Uint8Array): Float32Array {
  if (bytes.length !== 38421 && bytes.length !== 38805) {
    throw new Error(`not a 160 × 120 .DAT (${bytes.length} bytes)`);
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const n = W * H;
  const meta = n * 2;
  let offset = view.getFloat32(meta + 13, true);
  let slope = view.getFloat32(meta + 17, true);
  if (!Number.isFinite(offset) || offset === 0) offset = -KELVIN;
  if (!Number.isFinite(slope) || slope <= 0) slope = 0.01;
  const out = new Float32Array(n);
  for (let i = 0; i < n; i++) out[i] = view.getUint16(i * 2, false) * slope + offset;
  return out;
}

/** Frame `seq` of the demo camera, from loaded scenes; `hold` keeps one scene on screen. */
export function sceneFrame(
  scenes: readonly DemoScene[],
  seq: number,
  hold: number | null = null,
): ThermalFrame {
  const index = hold ?? Math.floor(seq / FRAMES_PER_SCENE) % scenes.length;
  const scene = scenes[index].celsius;
  const pixels = new Uint16Array(W * H);
  let minRaw = 65535;
  let maxRaw = 0;
  let minI = 0;
  let maxI = 0;
  for (let i = 0; i < pixels.length; i++) {
    const x = i % W;
    const y = Math.floor(i / W);
    // About 50 mK of noise, the same hash as the Rust demo camera.
    const hash =
      ((Math.imul(x, 73856093) ^ Math.imul(y, 19349663) ^ Math.imul(seq, 83492791)) >>> 0) & 0xff;
    const c = scene[i] + (hash / 255 - 0.5) * 0.14;
    const raw = Math.min(65000, Math.max(1000, Math.round((c + KELVIN) / 0.01)));
    pixels[i] = raw;
    if (raw < minRaw) {
      minRaw = raw;
      minI = i;
    }
    if (raw > maxRaw) {
      maxRaw = raw;
      maxI = i;
    }
  }
  const center = pixels[(H / 2) * W + W / 2];
  return {
    frame_seq: seq,
    timestamp_us: seq * 113895,
    rtc_epoch: 1764066000 + Math.floor(seq / 9),
    dropped_frames: 0,
    width: W,
    height: H,
    min_raw: minRaw,
    max_raw: maxRaw,
    min_x: minI % W,
    min_y: Math.floor(minI / W),
    max_x: maxI % W,
    max_y: Math.floor(maxI / W),
    spot_temp_c: center * 0.01 - KELVIN,
    cal_offset: -KELVIN,
    cal_slope: 0.01,
    fpa_temp_c: 31.4,
    bat_pct: 87,
    usb_power: true,
    pixels,
  };
}

let loaded: DemoScene[] | null = null;
let loading: Promise<void> | null = null;

/** Starts loading the captures once; a failed load leaves the generated scene in place. */
export function loadDemoScenes(base = "/demo/"): Promise<void> {
  if (loading) return loading;
  loading = Promise.all(
    DEMO_SCENES.map(async (name) => {
      const res = await fetch(`${base}${name}.DAT`);
      if (!res.ok) throw new Error(`${name}.DAT: ${res.status}`);
      return { name, celsius: parseDatCelsius(new Uint8Array(await res.arrayBuffer())) };
    }),
  ).then(
    (scenes) => {
      loaded = scenes;
    },
    () => {},
  );
  return loading;
}

let held: number | null = null;

/**
 * Frame `seq` of the real captures, or null while they are still loading. `live` follows a
 * held scene (Live's picture); saved items are always drawn from their own `seq`.
 */
export function demoSceneFrame(seq: number, live = true): ThermalFrame | null {
  if (!loaded) {
    if (typeof fetch === "function" && typeof window !== "undefined") void loadDemoScenes();
    return null;
  }
  return sceneFrame(loaded, seq, live ? held : null);
}

// Development only (docs screenshots): `__studioDemoScene("BREAKER")` holds one capture on
// screen, `__studioDemoScene(null)` lets them play in turn again.
if (import.meta.env?.DEV && typeof window !== "undefined") {
  (window as unknown as Record<string, unknown>).__studioDemoScene = (name: string | null) => {
    const i = name === null ? -1 : DEMO_SCENES.indexOf(name as (typeof DEMO_SCENES)[number]);
    held = i >= 0 ? i : null;
  };
}
