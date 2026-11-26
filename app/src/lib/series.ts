/**
 * The chart drawer's data: each line's values over the session, kept min/max-decimated so
 * every window draws at most 2,000 points per line however long the session runs. The whole
 * session, sample by sample, goes to the session log on disk (Save CSV…).
 */

export interface SeriesSample {
  /** Milliseconds since the session started. */
  t: number;
  v: number;
}

export type ChartWindow = "1m" | "5m" | "30m" | "all";

export const WINDOWS: { id: ChartWindow; label: string; ms: number | null }[] = [
  { id: "1m", label: "Last minute", ms: 60_000 },
  { id: "5m", label: "Last 5 minutes", ms: 300_000 },
  { id: "30m", label: "Last 30 minutes", ms: 1_800_000 },
  { id: "all", label: "Since start", ms: null },
];

/** Points per line per window. */
export const MAX_POINTS = 2000;
const BUCKETS = MAX_POINTS / 2;

interface Bucket {
  i: number;
  minT: number;
  minV: number;
  maxT: number;
  maxV: number;
}

function emit(buckets: readonly Bucket[], from = Number.NEGATIVE_INFINITY): SeriesSample[] {
  const out: SeriesSample[] = [];
  for (const b of buckets) {
    const a: SeriesSample = { t: b.minT, v: b.minV };
    const c: SeriesSample = { t: b.maxT, v: b.maxV };
    const [first, second] = a.t <= c.t ? [a, c] : [c, a];
    if (first.t >= from) out.push(first);
    if (second.t !== first.t && second.t >= from) out.push(second);
  }
  return out;
}

function addTo(list: Bucket[], i: number, t: number, v: number): void {
  const last = list[list.length - 1];
  if (last && last.i === i) {
    if (v < last.minV) {
      last.minV = v;
      last.minT = t;
    }
    if (v > last.maxV) {
      last.maxV = v;
      last.maxT = t;
    }
    return;
  }
  list.push({ i, minT: t, minV: v, maxT: t, maxV: v });
}

/**
 * Keeps each bucket's lowest and highest sample, in time order, so a spike or a dip survives
 * whatever the zoom. Series shorter than `maxPoints` pass through unchanged.
 */
export function decimateMinMax(
  samples: readonly SeriesSample[],
  maxPoints = MAX_POINTS,
): SeriesSample[] {
  if (samples.length <= maxPoints) return samples.slice();
  const n = Math.max(1, Math.floor(maxPoints / 2));
  const t0 = samples[0].t;
  const span = Math.max(1, samples[samples.length - 1].t - t0);
  const list: Bucket[] = [];
  for (const s of samples)
    addTo(list, Math.min(n - 1, Math.floor(((s.t - t0) / span) * n)), s.t, s.v);
  return emit(list);
}

interface Level {
  /** Bucket width in ms. */
  width: number;
  /** The window's length, or null for the whole session (the width then doubles as it grows). */
  ms: number | null;
  lines: Map<string, Bucket[]>;
}

export class SessionSeries {
  private levels = new Map<ChartWindow, Level>();
  private latest = 0;

  constructor() {
    this.clear();
  }

  clear(): void {
    this.levels.clear();
    for (const w of WINDOWS) {
      this.levels.set(w.id, {
        width: (w.ms ?? WINDOWS[0].ms ?? 60_000) / BUCKETS,
        ms: w.ms,
        lines: new Map(),
      });
    }
    this.latest = 0;
  }

  /** One sample per line, at `t` ms since the session started. */
  append(t: number, values: Record<string, number>): void {
    this.latest = Math.max(this.latest, t);
    for (const level of this.levels.values()) {
      for (const [key, v] of Object.entries(values)) {
        if (!Number.isFinite(v)) continue;
        let list = level.lines.get(key);
        if (!list) {
          list = [];
          level.lines.set(key, list);
        }
        addTo(list, Math.floor(t / level.width), t, v);
        if (level.ms !== null) {
          const oldest = Math.floor((t - level.ms) / level.width);
          let drop = 0;
          while (drop < list.length && list[drop].i <= oldest) drop++;
          if (drop > 0) list.splice(0, drop);
        }
      }
      if (level.ms === null) this.compact(level);
    }
  }

  /** Whole-session buckets double in width once a line holds more than it may draw. */
  private compact(level: Level): void {
    let longest = 0;
    for (const list of level.lines.values()) longest = Math.max(longest, list.length);
    while (longest > BUCKETS) {
      level.width *= 2;
      longest = 0;
      for (const [key, list] of level.lines) {
        const merged: Bucket[] = [];
        for (const b of list) {
          const i = Math.floor(b.i / 2);
          const last = merged[merged.length - 1];
          if (last && last.i === i) {
            if (b.minV < last.minV) {
              last.minV = b.minV;
              last.minT = b.minT;
            }
            if (b.maxV > last.maxV) {
              last.maxV = b.maxV;
              last.maxT = b.maxT;
            }
          } else {
            merged.push({ ...b, i });
          }
        }
        level.lines.set(key, merged);
        longest = Math.max(longest, merged.length);
      }
    }
  }

  /** A line's points in a window ending at `now`, in time order. */
  points(key: string, window: ChartWindow, now: number = this.latest): SeriesSample[] {
    const level = this.levels.get(window);
    const list = level?.lines.get(key);
    if (!level || !list) return [];
    return emit(list, level.ms === null ? Number.NEGATIVE_INFINITY : now - level.ms);
  }

  /** Time of the newest sample. */
  now(): number {
    return this.latest;
  }
}

/** Studio's one chart series for the live session. */
export const liveSeries = new SessionSeries();
