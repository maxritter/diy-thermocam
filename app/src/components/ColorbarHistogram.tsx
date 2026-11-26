import { useMemo, useRef } from "react";
import {
  type ColorRange,
  dragRange,
  type RangeDrag,
  rangeAfterDoubleClick,
  rangeAfterDragStart,
  scaleTicks,
  shownRange,
} from "@/lib/colorRange";
import type { FrameStore } from "@/lib/frameStore";
import { paletteGradientCss } from "@/lib/palettes";
import { formatTemp, fromCelsius, objectCToRaw, rawToObjectC, toCelsius } from "@/lib/thermal";
import type { Palette, RadiometryParams, TempUnit } from "@/lib/types";
import { useSampledFrame } from "@/lib/useFrame";

interface ColorbarHistogramProps {
  store: FrameStore;
  palette: Palette;
  range: ColorRange;
  onRangeChange: (range: ColorRange) => void;
  rad: RadiometryParams;
  unit: TempUnit;
  /** Clicking an end's unit switches °C and °F. */
  onUnitChange?: (unit: TempUnit) => void;
  /** Boost contrast is on: the scale is no longer even. */
  boosted?: boolean;
}

const BINS = 48;

/** A vertical histogram path beside the bar, cold at the bottom (studio.js renderHist). */
function histogramPath(pixels: Uint16Array, lo: number, hi: number): string {
  const counts = new Array<number>(BINS).fill(0);
  const span = Math.max(1, hi - lo);
  for (let i = 0; i < pixels.length; i++) {
    const b = Math.floor(((pixels[i] - lo) / span) * BINS);
    if (b >= 0 && b < BINS) counts[b]++;
  }
  let mx = 0;
  for (const c of counts) mx = Math.max(mx, Math.sqrt(c));
  if (mx === 0) return "";
  let d = "M18 100";
  for (let j = 0; j < BINS; j++) {
    const y = 100 - (j / BINS) * 100;
    const w = (Math.sqrt(counts[j]) / mx) * 17;
    d += ` L${(18 - w).toFixed(2)} ${y.toFixed(2)} L${(18 - w).toFixed(2)} ${(y - 100 / BINS).toFixed(2)}`;
  }
  return `${d} L18 0 Z`;
}

/**
 * The color scale beside the image (live.html#live-view): Auto, the histogram, the bar with
 * its two handles and round ticks. Drag a handle to set one end, drag the bar to shift both,
 * double-click to go back to Auto.
 */
export function ColorbarHistogram({
  store,
  palette,
  range,
  onRangeChange,
  rad,
  unit,
  onUnitChange,
  boosted = false,
}: ColorbarHistogramProps): React.JSX.Element {
  const frame = useSampledFrame(store);
  const trackRef = useRef<HTMLDivElement | null>(null);
  const live = useRef({ range, frame, onRangeChange });
  live.current = { range, frame, onRangeChange };

  const [lo, hi] = frame
    ? shownRange(range, frame)
    : range.mode === "manual"
      ? [range.lo, range.hi]
      : [0, 0];
  const loC = rawToObjectC(lo, rad);
  const hiC = rawToObjectC(hi, rad);
  const gradient = useMemo(() => paletteGradientCss(palette, "to top"), [palette]);
  const hist = useMemo(
    () => (frame && hi > lo ? histogramPath(frame.pixels, lo, hi) : ""),
    [frame, lo, hi],
  );
  const ticks =
    hi > lo
      ? scaleTicks(
          fromCelsius(loC, unit),
          fromCelsius(hiC, unit),
          (v) => (hi - objectCToRaw(toCelsius(v, unit), rad)) / (hi - lo),
        )
      : [];

  const startDrag = (kind: RangeDrag, e: React.PointerEvent<HTMLElement>) => {
    if (e.button !== 0) return;
    const fr = live.current.frame;
    if (!fr) return;
    e.preventDefault();
    e.stopPropagation();
    const next = rangeAfterDragStart(live.current.range, shownRange(live.current.range, fr));
    if (next.mode !== "manual") return;
    const start: [number, number] = [next.lo, next.hi];
    const startY = e.clientY;
    const height = Math.max(40, trackRef.current?.getBoundingClientRect().height ?? 0);
    if (next !== live.current.range) live.current.onRangeChange(next);
    let raf: number | null = null;
    let pending: [number, number] | null = null;
    const flush = () => {
      raf = null;
      if (pending) live.current.onRangeChange({ mode: "manual", lo: pending[0], hi: pending[1] });
    };
    const onMove = (ev: PointerEvent) => {
      pending = dragRange(kind, start, (startY - ev.clientY) / height);
      if (raf === null) raf = requestAnimationFrame(flush);
    };
    const onUp = () => {
      window.removeEventListener("pointermove", onMove);
      window.removeEventListener("pointerup", onUp);
      window.removeEventListener("pointercancel", onUp);
      if (raf !== null) cancelAnimationFrame(raf);
      flush();
    };
    window.addEventListener("pointermove", onMove);
    window.addEventListener("pointerup", onUp);
    window.addEventListener("pointercancel", onUp);
  };

  const toggleAuto = () => {
    if (range.mode === "auto") {
      if (frame) onRangeChange(rangeAfterDragStart(range, shownRange(range, frame)));
    } else {
      onRangeChange(rangeAfterDoubleClick());
    }
  };

  const otherUnit: TempUnit = unit === "fahrenheit" ? "celsius" : "fahrenheit";
  const endLabel = (c: number, cls: string) => (
    <button
      className={`end ${cls}`}
      type="button"
      title={otherUnit === "fahrenheit" ? "Show °F" : "Show °C"}
      onClick={() => onUnitChange?.(otherUnit)}
    >
      {Number.isFinite(c) && hi > lo ? formatTemp(c, unit) : ""}
    </button>
  );

  return (
    <div className="st-cbar">
      <button
        className="auto"
        type="button"
        aria-pressed={range.mode === "auto"}
        title="The scale follows the image"
        onClick={toggleAuto}
      >
        Auto
      </button>
      <span />
      <div className="track" ref={trackRef}>
        {endLabel(hiC, "top")}
        <svg className="hist" viewBox="0 0 18 100" preserveAspectRatio="none" aria-hidden="true">
          {hist ? <path d={hist} /> : null}
        </svg>
        <div
          className="grad"
          style={{ background: gradient }}
          onPointerDown={(e) => startDrag("pan", e)}
          onDoubleClick={() => onRangeChange(rangeAfterDoubleClick())}
          title="Drag to shift the range, double-click for Auto"
        />
        <button
          className="handle top"
          type="button"
          aria-label="Drag to fix the highest shown temperature"
          onPointerDown={(e) => startDrag("max", e)}
          onDoubleClick={() => onRangeChange(rangeAfterDoubleClick())}
        />
        <button
          className="handle bot"
          type="button"
          aria-label="Drag to fix the lowest shown temperature"
          onPointerDown={(e) => startDrag("min", e)}
          onDoubleClick={() => onRangeChange(rangeAfterDoubleClick())}
        />
        {ticks.map((t) => (
          <span key={t.value} className="tick" style={{ top: `${t.frac * 100}%` }}>
            {t.value}
          </span>
        ))}
        {endLabel(loC, "bot")}
      </div>
      <span />
      {boosted ? <span className="mark">Contrast boosted</span> : <span />}
    </div>
  );
}
