import { describe, expect, test } from "bun:test";
import {
  dragRange,
  RAW_LIMITS,
  rangeAfterDoubleClick,
  rangeAfterDragStart,
  scaleTicks,
} from "./colorRange";

const START: [number, number] = [29000, 32000];
const SPAN = START[1] - START[0];

describe("dragging the color scale", () => {
  test("the top handle follows the pointer and never crosses the bottom one", () => {
    expect(dragRange("max", START, 0.1)).toEqual([29000, 32300]);
    expect(dragRange("max", START, -0.5)).toEqual([29000, 30500]);
    for (const dy of [-1, -2, -10]) {
      expect(dragRange("max", START, dy)).toEqual([29000, 29000 + RAW_LIMITS.minSpan]);
    }
    expect(dragRange("max", START, 100)).toEqual([29000, RAW_LIMITS.max]);
  });

  test("the bottom handle never crosses the top one", () => {
    expect(dragRange("min", START, -0.1)).toEqual([28700, 32000]);
    for (const dy of [1, 2, 10]) {
      expect(dragRange("min", START, dy)).toEqual([32000 - RAW_LIMITS.minSpan, 32000]);
    }
    expect(dragRange("min", START, -100)).toEqual([RAW_LIMITS.min, 32000]);
  });

  test("dragging the bar shifts the range and keeps its span inside the limits", () => {
    expect(dragRange("pan", START, 0.5)).toEqual([30500, 33500]);
    const up = dragRange("pan", START, 1000);
    expect(up).toEqual([RAW_LIMITS.max - SPAN, RAW_LIMITS.max]);
    const down = dragRange("pan", START, -1000);
    expect(down).toEqual([RAW_LIMITS.min, RAW_LIMITS.min + SPAN]);
  });

  test("a sweep of drags always leaves lo < hi with the minimum span", () => {
    for (const kind of ["min", "max", "pan"] as const) {
      for (let dy = -3; dy <= 3; dy += 0.05) {
        const [lo, hi] = dragRange(kind, START, dy);
        expect(hi - lo).toBeGreaterThanOrEqual(RAW_LIMITS.minSpan);
        expect(lo).toBeGreaterThanOrEqual(RAW_LIMITS.min);
        expect(hi).toBeLessThanOrEqual(RAW_LIMITS.max);
      }
    }
  });

  test("starting a drag fixes the range at what is shown; double-click returns to Auto", () => {
    expect(rangeAfterDragStart({ mode: "auto" }, [29100, 31900])).toEqual({
      mode: "manual",
      lo: 29100,
      hi: 31900,
    });
    const fixed = { mode: "manual" as const, lo: 1, hi: 2 };
    expect(rangeAfterDragStart(fixed, [29100, 31900])).toBe(fixed);
    expect(rangeAfterDoubleClick()).toEqual({ mode: "auto" });
  });
});

test("ticks sit at round values away from the ends, as drawn for 20.6–55.6 °C", () => {
  const ticks = scaleTicks(20.6, 55.6);
  expect(ticks.map((t) => t.value)).toEqual([50, 40, 30]);
  expect(ticks[0].frac).toBeCloseTo(0.16, 2);
  expect(ticks[1].frac).toBeCloseTo(0.446, 2);
  expect(ticks[2].frac).toBeCloseTo(0.731, 2);
  expect(scaleTicks(30, 30.4).length).toBeLessThanOrEqual(3);
  expect(scaleTicks(10, 10)).toEqual([]);
});
