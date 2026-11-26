/**
 * The recording player's clock (captures.html#recording). A recording made in Studio has a
 * time for every frame and plays by those times; a camera video has none and plays at its
 * nominal frame rate. Speeds 0.25×–4×; a trim limits where the play head can go.
 */
export interface Timeline {
  frames: number;
  /** Milliseconds of each frame; null for a camera video. */
  tMs: number[] | null;
  /** Frames per second used when there are no times. */
  fps: number;
}

export interface Trim {
  from: number;
  to: number;
}

export interface PlayState {
  index: number;
  playing: boolean;
  speed: number;
  trim: Trim | null;
  /** Wall-clock time and media time when playback last (re)started. */
  anchorWall: number;
  anchorMedia: number;
}

export const SPEEDS = [0.25, 0.5, 1, 2, 4];

/** Media time of a frame, from the first frame (ms). */
export function mediaTimeOf(tl: Timeline, index: number): number {
  if (tl.tMs && tl.tMs.length > 0) {
    const i = Math.max(0, Math.min(tl.tMs.length - 1, index));
    return tl.tMs[i] - tl.tMs[0];
  }
  return (index * 1000) / Math.max(0.001, tl.fps);
}

/** The frame shown at a media time: the last one recorded at or before it. */
export function indexAtMediaTime(tl: Timeline, ms: number): number {
  const last = Math.max(0, tl.frames - 1);
  if (tl.tMs && tl.tMs.length > 0) {
    const t0 = tl.tMs[0];
    let lo = 0;
    let hi = Math.min(last, tl.tMs.length - 1);
    if (ms <= 0) return 0;
    while (lo < hi) {
      const mid = (lo + hi + 1) >> 1;
      if (tl.tMs[mid] - t0 <= ms) lo = mid;
      else hi = mid - 1;
    }
    return lo;
  }
  return Math.max(0, Math.min(last, Math.floor((ms * tl.fps) / 1000 + 1e-6)));
}

/** First and last frame the play head may be on. */
export function bounds(tl: Timeline, trim: Trim | null): [number, number] {
  const last = Math.max(0, tl.frames - 1);
  if (!trim) return [0, last];
  return [Math.max(0, Math.min(last, trim.from)), Math.max(0, Math.min(last, trim.to))];
}

export function createPlay(_tl: Timeline): PlayState {
  return { index: 0, playing: false, speed: 1, trim: null, anchorWall: 0, anchorMedia: 0 };
}

function at(s: PlayState, tl: Timeline, index: number, now: number): PlayState {
  const [lo, hi] = bounds(tl, s.trim);
  const i = Math.max(lo, Math.min(hi, Math.round(index)));
  return { ...s, index: i, anchorWall: now, anchorMedia: mediaTimeOf(tl, i) };
}

/** Where the play head is at `now`; playback stops on the last frame it may reach. */
export function tick(s: PlayState, tl: Timeline, now: number): PlayState {
  if (!s.playing) return s;
  const [lo, hi] = bounds(tl, s.trim);
  const media = s.anchorMedia + (now - s.anchorWall) * s.speed;
  const i = indexAtMediaTime(tl, media);
  if (i >= hi) return { ...at(s, tl, hi, now), playing: false };
  const index = Math.max(lo, i);
  return index === s.index ? s : { ...s, index };
}

export function play(s: PlayState, tl: Timeline, now: number): PlayState {
  const [lo, hi] = bounds(tl, s.trim);
  const from = s.index >= hi ? lo : s.index;
  return { ...at(s, tl, from, now), playing: true };
}

export function pause(s: PlayState, tl: Timeline, now: number): PlayState {
  const t = tick(s, tl, now);
  return { ...at(t, tl, t.index, now), playing: false };
}

export function seek(s: PlayState, tl: Timeline, index: number, now: number): PlayState {
  return at(s, tl, index, now);
}

/** One frame back or forward; stepping pauses. */
export function step(s: PlayState, tl: Timeline, delta: number, now: number): PlayState {
  return { ...at(s, tl, s.index + delta, now), playing: false };
}

export function setSpeed(s: PlayState, tl: Timeline, speed: number, now: number): PlayState {
  return at({ ...s, speed }, tl, s.index, now);
}

export function setTrim(s: PlayState, tl: Timeline, trim: Trim | null, now: number): PlayState {
  const t = trim ? { from: Math.min(trim.from, trim.to), to: Math.max(trim.from, trim.to) } : null;
  return at({ ...s, trim: t }, tl, s.index, now);
}

/** Frames to have ready: the one on screen and the next `ahead`; nothing behind is kept. */
export function prefetchWindow(index: number, frames: number, ahead = 8): number[] {
  const out: number[] = [];
  for (let i = Math.max(0, index); i <= Math.min(frames - 1, index + ahead); i++) out.push(i);
  return out;
}

/** "00:12.4": minutes and seconds to a tenth. */
export function formatClock(ms: number): string {
  const tenths = Math.round(Math.max(0, ms) / 100);
  const m = Math.floor(tenths / 600);
  const s = (tenths % 600) / 10;
  return `${String(m).padStart(2, "0")}:${s.toFixed(1).padStart(4, "0")}`;
}
