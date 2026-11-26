/**
 * The color scale's range and its gestures, in raw sensor counts. Dragging a handle moves one
 * end, dragging the bar shifts both, double-click goes back to Auto.
 */

export type ColorRange = { mode: "auto" } | { mode: "manual"; lo: number; hi: number };

export interface RangeLimits {
  min: number;
  max: number;
  /** Closest the two ends may come (20 counts ≈ 0.2 °C). */
  minSpan: number;
}

export const RAW_LIMITS: RangeLimits = { min: 0, max: 65535, minSpan: 20 };

export type RangeDrag = "min" | "max" | "pan";

/**
 * The range after a drag. `dyFrac` is how far the pointer moved up, as a fraction of the
 * bar's height, so a handle follows the pointer over the range shown when the drag began.
 */
export function dragRange(
  kind: RangeDrag,
  start: [number, number],
  dyFrac: number,
  limits: RangeLimits = RAW_LIMITS,
): [number, number] {
  const [lo, hi] = start;
  const span = Math.max(limits.minSpan, hi - lo);
  const delta = Math.round(dyFrac * span);
  if (kind === "max") {
    return [lo, Math.max(lo + limits.minSpan, Math.min(limits.max, hi + delta))];
  }
  if (kind === "min") {
    return [Math.min(hi - limits.minSpan, Math.max(limits.min, lo + delta)), hi];
  }
  const nextLo = Math.max(limits.min, Math.min(limits.max - span, lo + delta));
  return [nextLo, nextLo + span];
}

/** A drag fixes the range: from Auto it starts at what the scale shows now. */
export function rangeAfterDragStart(range: ColorRange, shown: [number, number]): ColorRange {
  return range.mode === "auto" ? { mode: "manual", lo: shown[0], hi: shown[1] } : range;
}

export function rangeAfterDoubleClick(): ColorRange {
  return { mode: "auto" };
}

/** The range the image is drawn with. */
export function shownRange(
  range: ColorRange,
  frame: { min_raw: number; max_raw: number },
): [number, number] {
  return range.mode === "auto" ? [frame.min_raw, frame.max_raw] : [range.lo, range.hi];
}

const STEPS = [0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000];

/**
 * Round tick values between the scale's ends, hottest first. `frac` is the distance from the
 * top (0) to the bottom (1); `toFrac` maps a value when the scale is not linear in it.
 * Ticks too close to an end, where the end label sits, are left out.
 */
export function scaleTicks(
  lo: number,
  hi: number,
  toFrac: (value: number) => number = (v) => (hi - v) / (hi - lo),
  maxTicks = 3,
): { value: number; frac: number }[] {
  if (!(hi > lo)) return [];
  for (const step of STEPS) {
    const decimals = step < 1 ? 1 : 0;
    const ticks: { value: number; frac: number }[] = [];
    for (let k = Math.ceil(hi / step); k * step >= lo; k--) {
      const value = Number((k * step).toFixed(decimals));
      const frac = toFrac(value);
      if (frac >= 0.1 && frac <= 0.9) ticks.push({ value, frac });
      if (ticks.length > maxTicks) break;
    }
    if (ticks.length <= maxTicks) return ticks;
  }
  return [];
}
