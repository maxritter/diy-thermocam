import { describe, expect, test } from "bun:test";
import { createPerfMeter, percentile } from "./perf";

describe("percentile (nearest rank)", () => {
  test("p95 of 1..100 is 95", () => {
    const values = Array.from({ length: 100 }, (_, i) => i + 1);
    expect(percentile(values, 95)).toBe(95);
  });

  test("order of the input does not matter", () => {
    expect(percentile([9, 1, 5, 3, 7], 50)).toBe(5);
  });

  test("an empty set has no percentile", () => {
    expect(percentile([], 95)).toBeNull();
  });

  test("p100 is the maximum and p0 the minimum", () => {
    expect(percentile([4, 2, 8], 100)).toBe(8);
    expect(percentile([4, 2, 8], 0)).toBe(2);
  });
});

describe("perf meter", () => {
  test("frame-to-screen latency pairs each drawn frame with its arrival", () => {
    let now = 0;
    const meter = createPerfMeter({ now: () => now });
    for (let i = 0; i < 20; i++) {
      now = i * 100;
      meter.frameArrived(i);
      now += i === 19 ? 30 : 5; // one slow frame
      meter.frameDrawn(i);
    }
    const s = meter.snapshot();
    expect(s.received).toBe(20);
    expect(s.drawn).toBe(20);
    expect(s.frameToScreenP95Ms).toBe(5);
    expect(s.frameToScreenMaxMs).toBe(30);
  });

  test("a frame drawn twice counts once, and a dropped frame is not drawn", () => {
    let now = 0;
    const meter = createPerfMeter({ now: () => now });
    meter.frameArrived(1);
    meter.frameArrived(2);
    now = 4;
    meter.frameDrawn(2);
    meter.frameDrawn(2);
    const s = meter.snapshot();
    expect(s.received).toBe(2);
    expect(s.drawn).toBe(1);
  });

  test("panel renders per second use the last second only", () => {
    let now = 0;
    const meter = createPerfMeter({ now: () => now });
    for (let i = 0; i < 10; i++) {
      now = i * 50; // 10 renders in the first 0.5 s
      meter.panelRendered("measure");
    }
    now = 2000;
    for (let i = 0; i < 3; i++) meter.panelRendered("measure");
    expect(meter.snapshot().panelRendersPerSecond.measure).toBe(3);
  });

  test("reset clears every counter", () => {
    let now = 0;
    const meter = createPerfMeter({ now: () => now });
    meter.frameArrived(1);
    now = 3;
    meter.frameDrawn(1);
    meter.panelRendered("chart");
    meter.reset();
    const s = meter.snapshot();
    expect(s.received).toBe(0);
    expect(s.drawn).toBe(0);
    expect(s.frameToScreenP95Ms).toBeNull();
    expect(s.panelRendersPerSecond).toEqual({});
  });
});
