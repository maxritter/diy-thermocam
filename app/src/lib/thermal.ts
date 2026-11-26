import { DEFAULT_MATERIALS } from "./palettes";
import type {
  IsothermConfig,
  LineSample,
  MaterialPreset,
  RadiometryParams,
  Roi,
  RoiGeometry,
  RoiStats,
  TempUnit,
  ThermalFrame,
} from "./types";

const KELVIN_OFFSET = 273.15;

export const MATERIAL_PRESETS: MaterialPreset[] = DEFAULT_MATERIALS;

export function unitSymbol(unit: TempUnit): string {
  switch (unit) {
    case "celsius":
      return "°C";
    case "fahrenheit":
      return "°F";
    case "kelvin":
      return "K";
  }
}

export function fromCelsius(tempC: number, unit: TempUnit): number {
  switch (unit) {
    case "celsius":
      return tempC;
    case "fahrenheit":
      return (tempC * 9) / 5 + 32;
    case "kelvin":
      return tempC + KELVIN_OFFSET;
  }
}

export function formatTemp(tempC: number, unit: TempUnit, decimals = 1): string {
  return `${fromCelsius(tempC, unit).toFixed(decimals)} ${unitSymbol(unit)}`;
}

export function formatDeltaTemp(deltaC: number, unit: TempUnit, decimals = 1): string {
  const scaled = unit === "fahrenheit" ? (deltaC * 9) / 5 : deltaC;
  const sign = scaled >= 0 ? "+" : "";
  return `${sign}${scaled.toFixed(decimals)} ${unitSymbol(unit)}`;
}

export function rawToApparentC(raw: number, calOffset: number, calSlope: number): number {
  return raw * calSlope + calOffset;
}

export function compensateEmissivityC(
  apparentC: number,
  emissivity: number,
  reflectedTempC: number,
): number {
  const eps = Math.max(0.05, Math.min(1.0, emissivity));
  if (Math.abs(eps - 1.0) < 1e-6) return apparentC;
  const tAppK = Math.max(1.0, apparentC + KELVIN_OFFSET);
  const tReflK = Math.max(1.0, reflectedTempC + KELVIN_OFFSET);
  const radObj = (tAppK ** 4 - (1 - eps) * tReflK ** 4) / eps;
  if (radObj <= 1.0) return 1.0 - KELVIN_OFFSET;
  return radObj ** 0.25 - KELVIN_OFFSET;
}

export function rawToObjectC(raw: number, rad: RadiometryParams): number {
  const app = rawToApparentC(raw, rad.cal_offset, rad.cal_slope);
  return compensateEmissivityC(app, rad.emissivity, rad.reflected_temp_c);
}

/** Exact inverse of `rawToObjectC` so WebGL2 raw-temperature isotherm thresholds match Rust `imaging.rs` at any emissivity. */
export function objectCToRaw(tempC: number, rad: RadiometryParams): number {
  const eps = Math.max(0.05, Math.min(1.0, rad.emissivity));
  let apparentC = tempC;
  if (Math.abs(eps - 1.0) >= 1e-6) {
    const tObjK = Math.max(1.0, tempC + KELVIN_OFFSET);
    const tReflK = Math.max(1.0, rad.reflected_temp_c + KELVIN_OFFSET);
    const tAppK = (eps * tObjK ** 4 + (1 - eps) * tReflK ** 4) ** 0.25;
    apparentC = tAppK - KELVIN_OFFSET;
  }
  const slope = Math.abs(rad.cal_slope) > 1e-9 ? rad.cal_slope : 0.01;
  return (apparentC - rad.cal_offset) / slope;
}

export function matchesIsotherm(iso: IsothermConfig, tempC: number): boolean {
  switch (iso.mode) {
    case "off":
      return false;
    case "above":
      return tempC >= iso.high_c;
    case "below":
      return tempC <= iso.low_c;
    case "interval":
      return tempC >= iso.low_c && tempC <= iso.high_c;
  }
}

export function generateDemoFrame(frameSeq: number, width = 160, height = 120): ThermalFrame {
  const phase = frameSeq * 0.14;
  const pixels = new Uint16Array(width * height);
  let minRaw = 65535;
  let maxRaw = 0;
  let minX = 0;
  let minY = 0;
  let maxX = 0;
  let maxY = 0;

  for (let y = 0; y < height; y++) {
    const ny = y / height;
    for (let x = 0; x < width; x++) {
      const nx = x / width;
      let tempC = 23.4 + 1.4 * (1 - ny) + 0.4 * Math.sin(nx * 6.28);

      const dx1 = (nx - 0.38) * 1.33;
      const dy1 = ny - 0.48;
      tempC += (22.6 + 1.2 * Math.sin(phase)) * Math.exp(-(dx1 * dx1 + dy1 * dy1) / 0.011);

      const dx2 = (nx - 0.68) * 1.33;
      const dy2 = ny - 0.34;
      tempC += (17.2 + 0.9 * Math.cos(phase * 1.3)) * Math.exp(-(dx2 * dx2 + dy2 * dy2) / 0.0055);

      const tFrac = Math.max(0, Math.min(1, (0.68 - nx) / 0.3));
      const traceDist = Math.abs((ny - (0.34 + 0.14 * tFrac)) * 12.0);
      if (nx >= 0.38 && nx <= 0.68 && traceDist < 0.7) {
        tempC += 6.8 * (1 - traceDist / 0.7);
      }

      const dx3 = (nx - 0.72) * 1.33;
      const dy3 = ny - 0.72;
      tempC += 12.2 * Math.exp(-(dx3 * dx3 + dy3 * dy3) / 0.007);

      if (nx >= 0.08 && nx <= 0.21 && ny >= 0.2 && ny <= 0.8) {
        const fin = (Math.sin(ny * 40.0) * 0.5 + 0.5) * 1.0;
        tempC -= 3.2 - fin;
      }

      const hash =
        ((Math.imul(x, 73856093) ^ Math.imul(y, 19349663) ^ Math.imul(frameSeq, 83492791)) >>> 0) &
        0xff;
      tempC += (hash / 255 - 0.5) * 0.14;

      const raw = Math.max(1000, Math.min(65000, Math.round((tempC + KELVIN_OFFSET) / 0.01)));
      pixels[y * width + x] = raw;
      if (raw < minRaw) {
        minRaw = raw;
        minX = x;
        minY = y;
      }
      if (raw > maxRaw) {
        maxRaw = raw;
        maxX = x;
        maxY = y;
      }
    }
  }

  const spotRaw = pixels[Math.floor(height / 2) * width + Math.floor(width / 2)];
  return {
    frame_seq: frameSeq,
    timestamp_us: frameSeq * 113895,
    rtc_epoch: 1764066000 + Math.floor(frameSeq / 9),
    dropped_frames: 0,
    width,
    height,
    min_raw: minRaw,
    max_raw: maxRaw,
    min_x: minX,
    min_y: minY,
    max_x: maxX,
    max_y: maxY,
    spot_temp_c: spotRaw * 0.01 - KELVIN_OFFSET,
    cal_offset: -KELVIN_OFFSET,
    cal_slope: 0.01,
    fpa_temp_c: 31.4,
    bat_pct: 87,
    usb_power: true,
    pixels,
  };
}

export function decodeFrameRawBinary(buf: ArrayBuffer | Uint8Array): ThermalFrame {
  const byteLength = buf.byteLength;
  if (byteLength < 64) {
    throw new Error(`FrameRaw payload too short: ${byteLength} bytes (expected >= 64)`);
  }
  const view =
    buf instanceof ArrayBuffer
      ? new DataView(buf)
      : new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const frame_seq = view.getUint32(0, true);
  const timestamp_us = Number(view.getBigUint64(4, true));
  const rtc_epoch = view.getUint32(12, true);
  const dropped_frames = view.getUint32(16, true);
  const width = view.getUint16(20, true);
  const height = view.getUint16(22, true);
  // Offsets 24..27: pixel_format, gain_mode, ffc_state, flags
  const min_raw = view.getUint16(28, true);
  const max_raw = view.getUint16(30, true);
  const min_x = view.getUint16(32, true);
  const min_y = view.getUint16(34, true);
  const max_x = view.getUint16(36, true);
  const max_y = view.getUint16(38, true);
  const spot_temp_c = view.getFloat32(40, true);
  const cal_offset = view.getFloat32(44, true);
  const cal_slope = view.getFloat32(48, true);
  const fpa_temp_c = view.getFloat32(52, true);
  const bat_pct = view.getInt8(56);
  const usb_power = view.getUint8(57) !== 0;

  const count = width * height;
  const requiredBytes = 64 + count * 2;
  if (count === 0 || byteLength < requiredBytes) {
    throw new Error(
      `FrameRaw pixel buffer truncated: ${byteLength} bytes for ${width}x${height} (need ${requiredBytes})`,
    );
  }
  const pixels = new Uint16Array(count);
  for (let i = 0; i < count; i++) {
    pixels[i] = view.getUint16(64 + i * 2, true);
  }
  return {
    frame_seq,
    timestamp_us,
    rtc_epoch,
    dropped_frames,
    width,
    height,
    min_raw,
    max_raw,
    min_x,
    min_y,
    max_x,
    max_y,
    spot_temp_c,
    cal_offset,
    cal_slope,
    fpa_temp_c,
    bat_pct,
    usb_power,
    pixels,
  };
}

function pointInPolygon(px: number, py: number, poly: [number, number][]): boolean {
  let inside = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const [xi, yi] = poly[i];
    const [xj, yj] = poly[j];
    const intersect = yi > py !== yj > py && px < ((xj - xi) * (py - yi)) / (yj - yi || 1e-6) + xi;
    if (intersect) inside = !inside;
  }
  return inside;
}

export function collectRoiCoords(
  geom: RoiGeometry,
  width: number,
  height: number,
): [number, number][] {
  const maxX = width - 1;
  const maxY = height - 1;
  const clampX = (v: number) => Math.max(0, Math.min(maxX, Math.round(v)));
  const clampY = (v: number) => Math.max(0, Math.min(maxY, Math.round(v)));

  switch (geom.kind) {
    case "spot": {
      const cx = clampX(geom.x);
      const cy = clampY(geom.y);
      if (geom.kernel >= 3) {
        const pts: [number, number][] = [];
        for (let dy = -1; dy <= 1; dy++) {
          for (let dx = -1; dx <= 1; dx++) {
            pts.push([clampX(cx + dx), clampY(cy + dy)]);
          }
        }
        return pts;
      }
      return [[cx, cy]];
    }
    case "box": {
      const x0 = clampX(geom.x);
      const y0 = clampY(geom.y);
      const x1 = clampX(x0 + Math.max(1, geom.w) - 1);
      const y1 = clampY(y0 + Math.max(1, geom.h) - 1);
      const pts: [number, number][] = [];
      for (let py = y0; py <= y1; py++) {
        for (let px = x0; px <= x1; px++) {
          pts.push([px, py]);
        }
      }
      return pts;
    }
    case "circle": {
      const r2 = geom.radius * geom.radius;
      const x0 = clampX(Math.floor(geom.cx - geom.radius));
      const x1 = clampX(Math.ceil(geom.cx + geom.radius));
      const y0 = clampY(Math.floor(geom.cy - geom.radius));
      const y1 = clampY(Math.ceil(geom.cy + geom.radius));
      const pts: [number, number][] = [];
      for (let py = y0; py <= y1; py++) {
        const dy = py - geom.cy;
        for (let px = x0; px <= x1; px++) {
          const dx = px - geom.cx;
          if (dx * dx + dy * dy <= r2) pts.push([px, py]);
        }
      }
      return pts;
    }
    case "polygon": {
      if (geom.points.length < 3) return [];
      let minX = maxX;
      let maxPx = 0;
      let minY = maxY;
      let maxPy = 0;
      for (const [px, py] of geom.points) {
        minX = Math.min(minX, Math.floor(px));
        maxPx = Math.max(maxPx, Math.ceil(px));
        minY = Math.min(minY, Math.floor(py));
        maxPy = Math.max(maxPy, Math.ceil(py));
      }
      const pts: [number, number][] = [];
      for (let py = clampY(minY); py <= clampY(maxPy); py++) {
        for (let px = clampX(minX); px <= clampX(maxPx); px++) {
          if (pointInPolygon(px + 0.5, py + 0.5, geom.points)) pts.push([px, py]);
        }
      }
      return pts;
    }
    case "line": {
      const dx = geom.x1 - geom.x0;
      const dy = geom.y1 - geom.y0;
      const steps = Math.max(1, Math.ceil(Math.hypot(dx, dy)));
      const pts: [number, number][] = [];
      for (let i = 0; i <= steps; i++) {
        const t = i / steps;
        pts.push([clampX(geom.x0 + dx * t), clampY(geom.y0 + dy * t)]);
      }
      return pts;
    }
    case "delta":
      return [
        [clampX(geom.x0), clampY(geom.y0)],
        [clampX(geom.x1), clampY(geom.y1)],
      ];
  }
}

export function evaluateRoi(
  roi: Roi,
  pixels: Uint16Array,
  width: number,
  height: number,
  global: RadiometryParams,
): RoiStats {
  const rad: RadiometryParams = {
    ...global,
    emissivity: roi.emissivity_override ?? global.emissivity,
    reflected_temp_c: roi.reflected_temp_override_c ?? global.reflected_temp_c,
  };
  const coords = collectRoiCoords(roi.geometry, width, height);
  let minRaw = 65535;
  let maxRaw = 0;
  let minXy: [number, number] = [0, 0];
  let maxXy: [number, number] = [0, 0];
  let sumRaw = 0;
  const temps: number[] = [];

  for (const [x, y] of coords) {
    const raw = pixels[y * width + x];
    if (raw < minRaw) {
      minRaw = raw;
      minXy = [x, y];
    }
    if (raw > maxRaw) {
      maxRaw = raw;
      maxXy = [x, y];
    }
    sumRaw += raw;
    temps.push(rawToObjectC(raw, rad));
  }

  const n = coords.length;
  const meanRaw = n > 0 ? sumRaw / n : 0;
  const meanC = n > 0 ? temps.reduce((a, b) => a + b, 0) / n : 0;
  let variance = 0;
  if (n > 1) {
    for (const t of temps) variance += (t - meanC) ** 2;
    variance /= n;
  }
  const minC = n > 0 ? rawToObjectC(minRaw, rad) : 0;
  const maxC = n > 0 ? rawToObjectC(maxRaw, rad) : 0;
  let deltaC: number | null = null;
  if (roi.geometry.kind === "delta") {
    const p0 =
      pixels[Math.min(height - 1, roi.geometry.y0) * width + Math.min(width - 1, roi.geometry.x0)];
    const p1 =
      pixels[Math.min(height - 1, roi.geometry.y1) * width + Math.min(width - 1, roi.geometry.x1)];
    deltaC = rawToObjectC(p1, rad) - rawToObjectC(p0, rad);
  }

  return {
    id: roi.id,
    label: roi.label,
    pixel_count: n,
    min_raw: n > 0 ? minRaw : 0,
    max_raw: maxRaw,
    mean_raw: meanRaw,
    min_c: minC,
    max_c: maxC,
    mean_c: meanC,
    std_dev_c: Math.sqrt(variance),
    delta_c: deltaC,
    min_xy: minXy,
    max_xy: maxXy,
    effective_emissivity: rad.emissivity,
  };
}

export function roiContrastRange(
  roi: Roi,
  pixels: Uint16Array,
  width: number,
  height: number,
  padFraction = 0.06,
): [number, number] | null {
  const coords = collectRoiCoords(roi.geometry, width, height);
  if (coords.length === 0) return null;
  let lo = 65535;
  let hi = 0;
  for (const [x, y] of coords) {
    const v = pixels[y * width + x];
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  const pad = Math.round(Math.max(1, hi - lo) * padFraction);
  return [Math.max(0, lo - pad), Math.min(65535, Math.max(lo + 1, hi + pad))];
}

export function sampleLineProfile(
  pixels: Uint16Array,
  width: number,
  height: number,
  x0: number,
  y0: number,
  x1: number,
  y1: number,
  rad: RadiometryParams,
): LineSample[] {
  const dx = x1 - x0;
  const dy = y1 - y0;
  const dist = Math.hypot(dx, dy);
  const steps = Math.max(1, Math.ceil(dist));
  const out: LineSample[] = [];
  for (let i = 0; i <= steps; i++) {
    const t = i / steps;
    const x = Math.max(0, Math.min(width - 1, Math.round(x0 + dx * t)));
    const y = Math.max(0, Math.min(height - 1, Math.round(y0 + dy * t)));
    const raw = pixels[y * width + x];
    out.push({ dist_px: dist * t, x, y, raw, temp_c: rawToObjectC(raw, rad) });
  }
  return out;
}

export function computeHistogram256(
  pixels: Uint16Array,
  minRaw: number,
  maxRaw: number,
): Uint32Array {
  const bins = new Uint32Array(256);
  const span = Math.max(1, maxRaw - minRaw);
  for (let i = 0; i < pixels.length; i++) {
    const norm = Math.max(0, Math.min(1, (pixels[i] - minRaw) / span));
    bins[Math.min(255, Math.round(norm * 255))]++;
  }
  return bins;
}

export interface TemporalHeqState {
  smoothedCdf: Float32Array | null;
  alpha: number;
}

export interface ViewportScratch {
  raw16: Uint16Array;
  disp16: Uint16Array;
  bufA: Float32Array;
  bufB: Float32Array;
  heq: TemporalHeqState;
}

export function createViewportScratch(len: number): ViewportScratch {
  return {
    raw16: new Uint16Array(len),
    disp16: new Uint16Array(len),
    bufA: new Float32Array(len),
    bufB: new Float32Array(len),
    heq: { smoothedCdf: null, alpha: 0.25 },
  };
}

/**
 * Display-only clipped-CDF histogram equalization with optional temporal EMA smoothing
 * (`temporal.alpha = 0.25`) so live thermal video contrast never flickers.
 */
export function applyClippedHistogramEqFloat(
  norm: Float32Array,
  out: Float32Array,
  temporal?: TemporalHeqState,
): void {
  const n = norm.length;
  if (n === 0) return;
  const hist = new Uint32Array(256);
  for (let i = 0; i < n; i++) {
    const idx = Math.min(255, Math.max(0, Math.round(norm[i] * 255)));
    hist[idx]++;
  }
  const clipLimit = Math.max(4, Math.floor((n / 256) * 3));
  let excess = 0;
  for (let i = 0; i < 256; i++) {
    if (hist[i] > clipLimit) {
      excess += hist[i] - clipLimit;
      hist[i] = clipLimit;
    }
  }
  const redist = Math.floor(excess / 256);
  for (let i = 0; i < 256; i++) hist[i] += redist;

  const lut = new Float32Array(256);
  let running = 0;
  let cdfMin = -1;
  for (let i = 0; i < 256; i++) {
    running += hist[i];
    lut[i] = running;
    if (cdfMin < 0 && running > 0) cdfMin = running;
  }
  const denom = Math.max(1, running - Math.max(0, cdfMin));
  const minVal = Math.max(0, cdfMin);
  for (let i = 0; i < 256; i++) {
    lut[i] = Math.max(0, Math.min(1, (lut[i] - minVal) / denom));
  }
  if (temporal) {
    if (temporal.smoothedCdf) {
      const a = temporal.alpha;
      const inv = 1 - a;
      for (let i = 0; i < 256; i++) {
        temporal.smoothedCdf[i] = temporal.smoothedCdf[i] * inv + lut[i] * a;
        lut[i] = temporal.smoothedCdf[i];
      }
    } else {
      temporal.smoothedCdf = new Float32Array(lut);
    }
  }
  for (let i = 0; i < n; i++) {
    const lin = Math.max(0, Math.min(1, norm[i]));
    const idx = Math.min(255, Math.round(lin * 255));
    out[i] = Math.max(0, Math.min(1, lut[idx] * 0.75 + lin * 0.25));
  }
}

/**
 * Compute matching 16-bit `raw16` (linear raw normalized 0..65535 for exact isotherm) and
 * `disp16` (spatial filter + temporal HEQ + bilateral + local-range-clamped USM, 0..65535)
 * into reusable typed-array scratch buffers (zero 8-bit quantization, matches Rust `imaging.rs`).
 */
export function prepareDisplay16Bit(
  pixels: Uint16Array,
  comparePixels: Uint16Array | null,
  splitCol: number,
  w: number,
  h: number,
  minRaw: number,
  maxRaw: number,
  pipeline: import("./types").DisplayPipeline,
  scratch: ViewportScratch,
): void {
  const n = w * h;
  if (scratch.raw16.length !== n) {
    scratch.raw16 = new Uint16Array(n);
    scratch.disp16 = new Uint16Array(n);
    scratch.bufA = new Float32Array(n);
    scratch.bufB = new Float32Array(n);
  }
  const invSpan = 1 / Math.max(1, maxRaw - minRaw);
  const { raw16, disp16, bufA, bufB } = scratch;

  for (let y = 0; y < h; y++) {
    const rowOff = y * w;
    for (let x = 0; x < w; x++) {
      const idx = rowOff + x;
      const raw =
        comparePixels && x >= splitCol ? (comparePixels[idx] ?? pixels[idx]) : pixels[idx];
      const norm = Math.max(0, Math.min(1, (raw - minRaw) * invSpan));
      bufA[idx] = norm;
      raw16[idx] = Math.round(norm * 65535);
    }
  }

  let cur = bufA;
  let next = bufB;

  if (pipeline.filter !== "none") {
    next.set(cur);
    if (pipeline.filter === "box3x3" || pipeline.filter === "gaussian3x3") {
      const isGauss = pipeline.filter === "gaussian3x3";
      for (let y = 1; y < h - 1; y++) {
        for (let x = 1; x < w - 1; x++) {
          const i = y * w + x;
          if (isGauss) {
            next[i] =
              (cur[i - w - 1] +
                2 * cur[i - w] +
                cur[i - w + 1] +
                2 * cur[i - 1] +
                4 * cur[i] +
                2 * cur[i + 1] +
                cur[i + w - 1] +
                2 * cur[i + w] +
                cur[i + w + 1]) /
              16;
          } else {
            next[i] =
              (cur[i - w - 1] +
                cur[i - w] +
                cur[i - w + 1] +
                cur[i - 1] +
                cur[i] +
                cur[i + 1] +
                cur[i + w - 1] +
                cur[i + w] +
                cur[i + w + 1]) /
              9;
          }
        }
      }
    } else {
      const win = new Float32Array(9);
      for (let y = 1; y < h - 1; y++) {
        for (let x = 1; x < w - 1; x++) {
          const i = y * w + x;
          win[0] = cur[i - w - 1];
          win[1] = cur[i - w];
          win[2] = cur[i - w + 1];
          win[3] = cur[i - 1];
          win[4] = cur[i];
          win[5] = cur[i + 1];
          win[6] = cur[i + w - 1];
          win[7] = cur[i + w];
          win[8] = cur[i + w + 1];
          win.sort();
          next[i] = win[4];
        }
      }
    }
    [cur, next] = [next, cur];
  }

  if (pipeline.histogram_eq) {
    applyClippedHistogramEqFloat(cur, next, scratch.heq);
    [cur, next] = [next, cur];
  } else {
    scratch.heq.smoothedCdf = null;
  }

  if (pipeline.upscaler === "edge_preserve") {
    const inv2sr2 = 1 / (2 * 0.08 * 0.08);
    const sp = [0.606, 0.778, 0.606, 0.778, 1.0, 0.778, 0.606, 0.778, 0.606];
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const center = cur[y * w + x];
        let num = 0;
        let den = 0;
        let ki = 0;
        for (let dy = -1; dy <= 1; dy++) {
          const ny = Math.max(0, Math.min(h - 1, y + dy));
          for (let dx = -1; dx <= 1; dx++) {
            const nx = Math.max(0, Math.min(w - 1, x + dx));
            const v = cur[ny * w + nx];
            const diff = v - center;
            const wt = sp[ki++] * Math.exp(-(diff * diff) * inv2sr2);
            num += v * wt;
            den += wt;
          }
        }
        next[y * w + x] = den > 0 ? num / den : center;
      }
    }
    [cur, next] = [next, cur];
  }

  if (pipeline.sharpen_amount > 0.001 && pipeline.upscaler !== "nearest" && w >= 3 && h >= 3) {
    const amt = Math.max(0, Math.min(2, pipeline.sharpen_amount));
    const kw = [1 / 16, 2 / 16, 1 / 16, 2 / 16, 4 / 16, 2 / 16, 1 / 16, 2 / 16, 1 / 16];
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const orig = cur[y * w + x];
        let blur = 0;
        let lo = 1;
        let hi = 0;
        let ki = 0;
        for (let dy = -1; dy <= 1; dy++) {
          const ny = Math.max(0, Math.min(h - 1, y + dy));
          for (let dx = -1; dx <= 1; dx++) {
            const nx = Math.max(0, Math.min(w - 1, x + dx));
            const v = cur[ny * w + nx];
            blur += v * kw[ki++];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
          }
        }
        const sharp = orig + amt * (orig - blur);
        next[y * w + x] = Math.max(0, Math.min(1, Math.max(lo, Math.min(hi, sharp))));
      }
    }
    [cur, next] = [next, cur];
  }

  for (let i = 0; i < n; i++) {
    disp16[i] = Math.round(Math.max(0, Math.min(1, cur[i])) * 65535);
  }
}

/**
 * Display-only clipped-CDF histogram equalization (CLAHE-style plateau clip).
 * Enhances local contrast on subtle scenes without touching raw 16-bit radiometry.
 */
export function applyClippedHistogramEq(normBytes: Uint8Array): Uint8Array {
  const n = normBytes.length;
  if (n === 0) return normBytes;
  const hist = new Uint32Array(256);
  for (let i = 0; i < n; i++) hist[normBytes[i]]++;

  const clipLimit = Math.max(4, Math.floor((n / 256) * 3));
  let excess = 0;
  for (let i = 0; i < 256; i++) {
    if (hist[i] > clipLimit) {
      excess += hist[i] - clipLimit;
      hist[i] = clipLimit;
    }
  }
  const redist = Math.floor(excess / 256);
  for (let i = 0; i < 256; i++) hist[i] += redist;

  const cdf = new Float32Array(256);
  let running = 0;
  let cdfMin = -1;
  for (let i = 0; i < 256; i++) {
    running += hist[i];
    cdf[i] = running;
    if (cdfMin < 0 && running > 0) cdfMin = running;
  }
  const denom = Math.max(1, running - Math.max(0, cdfMin));
  const out = new Uint8Array(n);
  for (let i = 0; i < n; i++) {
    const v = normBytes[i];
    const eq = ((cdf[v] - Math.max(0, cdfMin)) / denom) * 255;
    out[i] = Math.max(0, Math.min(255, Math.round(eq * 0.75 + v * 0.25)));
  }
  return out;
}

export function computeTempGrid(
  pixels: Uint16Array,
  width: number,
  height: number,
  cols: number,
  rows: number,
  rad: RadiometryParams,
): number[][] {
  const out: number[][] = [];
  for (let r = 0; r < rows; r++) {
    const y = Math.min(height - 1, Math.floor(((r * 2 + 1) * height) / (rows * 2)));
    const row: number[] = [];
    for (let c = 0; c < cols; c++) {
      const x = Math.min(width - 1, Math.floor(((c * 2 + 1) * width) / (cols * 2)));
      row.push(rawToObjectC(pixels[y * width + x], rad));
    }
    out.push(row);
  }
  return out;
}
