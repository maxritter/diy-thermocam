import { describe, expect, test } from "bun:test";
import { decimateMinMax, MAX_POINTS, type SeriesSample, SessionSeries, WINDOWS } from "./series";

/** A noisy signal with one hot spike and one cold dip somewhere in the middle. */
function signal(n: number): SeriesSample[] {
  const out: SeriesSample[] = [];
  for (let i = 0; i < n; i++) {
    let v = 30 + 5 * Math.sin(i / 500) + Math.sin(i * 12.9898) * 0.3;
    if (i === 61_234) v = 99.5;
    if (i === 17_001) v = -12.25;
    out.push({ t: i * 115, v });
  }
  return out;
}

describe("min/max decimation", () => {
  test("decimating 100,000 samples to 2,000 points keeps the global min and max", () => {
    const samples = signal(100_000);
    const out = decimateMinMax(samples, 2000);
    expect(out.length).toBeLessThanOrEqual(2000);
    expect(out.length).toBeGreaterThan(1000);
    expect(Math.max(...out.map((p) => p.v))).toBe(99.5);
    expect(Math.min(...out.map((p) => p.v))).toBe(-12.25);
    for (let i = 1; i < out.length; i++) expect(out[i].t).toBeGreaterThanOrEqual(out[i - 1].t);
  });

  test("a short series passes through unchanged", () => {
    const samples = signal(500);
    expect(decimateMinMax(samples, 2000)).toEqual(samples);
  });
});

describe("the live session series", () => {
  test("every window stays within 2,000 points per region and keeps the extremes", () => {
    const s = new SessionSeries();
    const samples = signal(100_000);
    for (const p of samples) s.append(p.t, { mcu: p.v, max: p.v + 1 });
    const now = samples[samples.length - 1].t;
    for (const w of WINDOWS) {
      const pts = s.points("mcu", w.id, now);
      expect(pts.length).toBeLessThanOrEqual(MAX_POINTS);
      if (w.ms !== null) {
        for (const p of pts) expect(p.t).toBeGreaterThanOrEqual(now - w.ms - w.ms / 1000);
      }
    }
    const all = s.points("mcu", "all", now);
    expect(Math.max(...all.map((p) => p.v))).toBe(99.5);
    expect(Math.min(...all.map((p) => p.v))).toBe(-12.25);
    // Since start begins in the session's first bucket.
    expect(all[0].t).toBeLessThan(now / 500);
    // The last minute holds only recent samples, every one of them at this frame rate.
    const minute = s.points("mcu", "1m", now);
    expect(minute[minute.length - 1].t).toBe(now);
    expect(minute.length).toBeGreaterThan(400);
  });

  test("clear starts over, and a region added later has its own line", () => {
    const s = new SessionSeries();
    s.append(0, { max: 30 });
    s.append(100, { max: 31, box: 25 });
    expect(s.points("box", "all", 100)).toEqual([{ t: 100, v: 25 }]);
    s.clear();
    expect(s.points("max", "all", 100)).toEqual([]);
  });
});
