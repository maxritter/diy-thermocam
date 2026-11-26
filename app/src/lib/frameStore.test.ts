import { describe, expect, test } from "bun:test";
import { createFrameStore, type FrameScheduler } from "./frameStore";
import { generateDemoFrame } from "./thermal";
import type { ThermalFrame } from "./types";

/** A manual clock: timers fire only when `advanceTo` passes their due time. */
function fakeScheduler(): FrameScheduler & { advanceTo(ms: number): void; now(): number } {
  let now = 0;
  let nextId = 1;
  const timers = new Map<number, { due: number; fn: () => void }>();
  return {
    now: () => now,
    schedule(fn, ms) {
      const id = nextId++;
      timers.set(id, { due: now + ms, fn });
      return id;
    },
    cancel(id) {
      timers.delete(id as number);
    },
    advanceTo(ms) {
      for (;;) {
        let first: [number, { due: number; fn: () => void }] | null = null;
        for (const entry of timers) {
          if (entry[1].due <= ms && (!first || entry[1].due < first[1].due)) first = entry;
        }
        if (!first) break;
        timers.delete(first[0]);
        now = first[1].due;
        first[1].fn();
      }
      now = ms;
    },
  };
}

function frameNo(n: number): ThermalFrame {
  return { ...generateDemoFrame(1), frame_seq: n };
}

describe("frameStore", () => {
  test("40 frames within 1 s reach sampled subscribers at most 4 times, ending on frame 40", () => {
    const clock = fakeScheduler();
    const store = createFrameStore({ scheduler: clock });
    const sampled: number[] = [];
    let everyFrame = 0;
    store.subscribeSampled(() => sampled.push(store.getSampled()?.frame_seq ?? -1));
    store.subscribeFrame(() => everyFrame++);

    for (let i = 1; i <= 40; i++) {
      clock.advanceTo((i - 1) * 25);
      store.push(frameNo(i));
    }
    clock.advanceTo(2000);

    expect(everyFrame).toBe(40);
    expect(sampled.length).toBeGreaterThan(0);
    expect(sampled.length).toBeLessThanOrEqual(4);
    expect(sampled[sampled.length - 1]).toBe(40);
    expect(store.counters().received).toBe(40);
  });

  test("one frame after 2 s idle reaches sampled subscribers once", () => {
    const clock = fakeScheduler();
    const store = createFrameStore({ scheduler: clock });
    store.push(frameNo(1));
    clock.advanceTo(2000);

    let calls = 0;
    store.subscribeSampled(() => calls++);
    clock.advanceTo(4000);
    store.push(frameNo(2));
    clock.advanceTo(6000);

    expect(calls).toBe(1);
    expect(store.getSampled()?.frame_seq).toBe(2);
  });

  test("hover, series and the drawn counter", () => {
    const clock = fakeScheduler();
    const store = createFrameStore({ scheduler: clock, seriesCap: 3 });
    let hovers = 0;
    store.subscribeHover(() => hovers++);
    store.setHover({ x: 1, y: 2, raw: 30000, tempC: 26.9 });
    store.setHover({ x: 1, y: 2, raw: 30000, tempC: 26.9 });
    expect(hovers).toBe(1);
    expect(store.getHover()?.x).toBe(1);

    for (let i = 0; i < 5; i++) store.appendSeries({ tSec: i, values: { max: i } });
    clock.advanceTo(1000);
    expect(store.getSeries().map((p) => p.tSec)).toEqual([2, 3, 4]);

    store.markDrawn();
    expect(store.counters().drawn).toBe(1);

    const unsub = store.subscribeFrame(() => {
      throw new Error("unsubscribed listener must not run");
    });
    unsub();
    store.push(frameNo(9));
    expect(store.getFrame()?.frame_seq).toBe(9);
  });
});
