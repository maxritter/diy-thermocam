import { describe, expect, test } from "bun:test";
import {
  bounds,
  createPlay,
  formatClock,
  indexAtMediaTime,
  mediaTimeOf,
  pause,
  play,
  prefetchWindow,
  seek,
  setSpeed,
  setTrim,
  step,
  type Timeline,
  tick,
} from "./playback";

const TIMED: Timeline = {
  frames: 7,
  tMs: [0, 250, 600, 1000, 1500, 2000, 2600],
  fps: 8.7,
};

describe("playback", () => {
  test("at 2× with frame times, the frame at 1 s of wall time is the one recorded at 2 s", () => {
    let s = play(createPlay(TIMED), TIMED, 10_000);
    s = setSpeed(s, TIMED, 2, 10_000);
    s = tick(s, TIMED, 11_000);
    expect(s.index).toBe(5);
    expect(mediaTimeOf(TIMED, s.index)).toBe(2000);
    // Just before 2 s it is still the frame recorded at 1.5 s.
    const early = tick(setSpeed(play(createPlay(TIMED), TIMED, 0), TIMED, 2, 0), TIMED, 999);
    expect(early.index).toBe(4);
  });

  test("times are taken from the first frame's time", () => {
    const tl: Timeline = { frames: 3, tMs: [5000, 5100, 5300], fps: 10 };
    expect(mediaTimeOf(tl, 2)).toBe(300);
    expect(indexAtMediaTime(tl, 150)).toBe(1);
  });

  test("without frame times, frame numbers advance at the nominal rate", () => {
    const tl: Timeline = { frames: 1000, tMs: null, fps: 8.7 };
    let s = play(createPlay(tl), tl, 0);
    expect(tick(s, tl, 1000).index).toBe(8);
    expect(tick(s, tl, 10_000).index).toBe(87);
    s = setSpeed(s, tl, 2, 0);
    expect(tick(s, tl, 1000).index).toBe(17);
    s = setSpeed(play(createPlay(tl), tl, 0), tl, 0.25, 0);
    expect(tick(s, tl, 4000).index).toBe(8);
  });

  test("the trim clamps the play head, stops at its end and starts again from its start", () => {
    const tl: Timeline = { frames: 100, tMs: null, fps: 10 };
    let s = setTrim(createPlay(tl), tl, { from: 10, to: 20 }, 0);
    expect(s.index).toBe(10);
    expect(bounds(tl, s.trim)).toEqual([10, 20]);
    expect(seek(s, tl, 5, 0).index).toBe(10);
    expect(seek(s, tl, 30, 0).index).toBe(20);
    expect(step(seek(s, tl, 20, 0), tl, 1, 0).index).toBe(20);
    s = play(s, tl, 0);
    s = tick(s, tl, 5000);
    expect(s.index).toBe(20);
    expect(s.playing).toBe(false);
    s = play(s, tl, 6000);
    expect(s.index).toBe(10);
    expect(s.playing).toBe(true);
    // A trim drawn backwards is put the right way round; a cleared trim frees the head.
    expect(setTrim(s, tl, { from: 40, to: 30 }, 0).trim).toEqual({ from: 30, to: 40 });
    expect(bounds(tl, setTrim(s, tl, null, 0).trim)).toEqual([0, 99]);
  });

  test("the end of the recording stops playback", () => {
    const s = tick(play(createPlay(TIMED), TIMED, 0), TIMED, 60_000);
    expect(s.index).toBe(6);
    expect(s.playing).toBe(false);
  });

  test("frames are fetched 8 ahead and none behind", () => {
    expect(prefetchWindow(0, 100)).toEqual([0, 1, 2, 3, 4, 5, 6, 7, 8]);
    expect(prefetchWindow(95, 100)).toEqual([95, 96, 97, 98, 99]);
    expect(prefetchWindow(3, 5, 2)).toEqual([3, 4]);
  });

  test.each([
    [0, "00:00.0"],
    [12_400, "00:12.4"],
    [36_049, "00:36.0"],
    [61_250, "01:01.3"],
    [3_600_000, "60:00.0"],
  ])("%d ms reads %s", (ms, text) => {
    expect(formatClock(ms)).toBe(text);
  });

  for (const tl of [
    { frames: 60, tMs: Array.from({ length: 60 }, (_, i) => i * 113 + (i % 4) * 7), fps: 8.7 },
    { frames: 60, tMs: null, fps: 10 },
  ] as Timeline[]) {
    test(`a seeded sequence matches a simple model (${tl.tMs ? "times" : "frame numbers"})`, () => {
      let seed = 5;
      const rnd = () => {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        return seed / 0x7fffffff;
      };
      const speeds = [0.25, 0.5, 1, 2, 4];
      let s = createPlay(tl);
      let now = 0;
      // Model: media time accumulates while playing; every other action snaps to a frame.
      const m = { index: 0, media: 0, playing: false, speed: 1, lo: 0, hi: tl.frames - 1 };
      const snap = (i: number) => {
        m.index = Math.max(m.lo, Math.min(m.hi, i));
        m.media = mediaTimeOf(tl, m.index);
      };
      for (let n = 0; n < 4000; n++) {
        const p = rnd();
        if (p < 0.35) {
          const dt = 1 + Math.floor(rnd() * 400);
          now += dt;
          s = tick(s, tl, now);
          if (m.playing) {
            m.media += dt * m.speed;
            const i = indexAtMediaTime(tl, m.media);
            if (i >= m.hi) {
              snap(m.hi);
              m.playing = false;
            } else m.index = Math.max(m.lo, i);
          }
        } else if (p < 0.45) {
          s = play(s, tl, now);
          if (m.index >= m.hi) snap(m.lo);
          else snap(m.index);
          m.playing = true;
        } else if (p < 0.55) {
          s = pause(s, tl, now);
          snap(m.index);
          m.playing = false;
        } else if (p < 0.65) {
          const i = Math.floor(rnd() * (tl.frames + 10)) - 5;
          s = seek(s, tl, i, now);
          snap(i);
        } else if (p < 0.75) {
          const d = rnd() < 0.5 ? -1 : 1;
          s = step(s, tl, d, now);
          snap(m.index + d);
          m.playing = false;
        } else if (p < 0.85) {
          const sp = speeds[Math.floor(rnd() * speeds.length)];
          s = setSpeed(s, tl, sp, now);
          m.speed = sp;
          snap(m.index);
        } else {
          if (rnd() < 0.3) {
            s = setTrim(s, tl, null, now);
            m.lo = 0;
            m.hi = tl.frames - 1;
          } else {
            const a = Math.floor(rnd() * tl.frames);
            const b = Math.floor(rnd() * tl.frames);
            s = setTrim(s, tl, { from: a, to: b }, now);
            m.lo = Math.min(a, b);
            m.hi = Math.max(a, b);
          }
          snap(m.index);
        }
        expect([s.index, s.playing]).toEqual([m.index, m.playing]);
      }
    });
  }
});
