import { useMemo, useRef } from "react";
import { paletteGradientCss } from "@/lib/palettes";
import { computeHistogram256, formatTemp, rawToObjectC } from "@/lib/thermal";
import type {
  IsothermConfig,
  Palette,
  RadiometryParams,
  TempUnit,
  ThermalFrame,
} from "@/lib/types";

const HIST_BUCKETS = Array.from({ length: 64 }, (_, i) => ({
  id: `hb_${i * 4}`,
  offset: i * 4,
  x: i * 2,
}));

interface ColorbarHistogramProps {
  frame: ThermalFrame;
  palette: Palette;
  minRaw: number;
  maxRaw: number;
  autoScale: boolean;
  rad: RadiometryParams;
  unit: TempUnit;
  isotherm: IsothermConfig;
  onRangeChange: (minRaw: number, maxRaw: number) => void;
  onToggleAutoScale: (auto: boolean) => void;
}

export function ColorbarHistogram({
  frame,
  palette,
  minRaw,
  maxRaw,
  autoScale,
  rad,
  unit,
  isotherm,
  onRangeChange,
  onToggleAutoScale,
}: ColorbarHistogramProps): React.JSX.Element {
  const barRef = useRef<HTMLDivElement | null>(null);
  const dragMode = useRef<"min" | "max" | "pan" | null>(null);
  const dragStartY = useRef(0);
  const dragStartRange = useRef<[number, number]>([minRaw, maxRaw]);

  const bins = useMemo(
    () => computeHistogram256(frame.pixels, frame.min_raw, frame.max_raw),
    [frame.pixels, frame.min_raw, frame.max_raw],
  );
  const maxBin = useMemo(() => {
    let m = 1;
    for (let i = 0; i < bins.length; i++) if (bins[i] > m) m = bins[i];
    return m;
  }, [bins]);

  const minC = rawToObjectC(minRaw, rad);
  const maxC = rawToObjectC(maxRaw, rad);
  const ticks = [1, 0.75, 0.5, 0.25, 0];

  const startDrag = (mode: "min" | "max" | "pan", e: React.MouseEvent): void => {
    e.preventDefault();
    dragMode.current = mode;
    dragStartY.current = e.clientY;
    dragStartRange.current = [minRaw, maxRaw];
    onToggleAutoScale(false);

    const onMove = (ev: MouseEvent): void => {
      const rect = barRef.current?.getBoundingClientRect();
      if (!rect || !dragMode.current) return;
      const dyFrac = (dragStartY.current - ev.clientY) / Math.max(40, rect.height);
      const [sMin, sMax] = dragStartRange.current;
      const span = Math.max(50, sMax - sMin);
      const deltaRaw = Math.round(dyFrac * span * 1.4);

      if (dragMode.current === "max") {
        onRangeChange(sMin, Math.max(sMin + 20, Math.min(65000, sMax + deltaRaw)));
      } else if (dragMode.current === "min") {
        onRangeChange(Math.max(100, Math.min(sMax - 20, sMin + deltaRaw)), sMax);
      } else {
        const nextMin = Math.max(100, Math.min(65000 - span, sMin + deltaRaw));
        onRangeChange(nextMin, nextMin + span);
      }
    };

    const onUp = (): void => {
      dragMode.current = null;
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
    };
    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup", onUp);
  };

  return (
    <div className="flex h-full w-[116px] shrink-0 flex-col gap-2 rounded-[10px] border border-[var(--tc-border)] bg-[var(--tc-surface)] p-2">
      <div className="flex items-center justify-between">
        <span className="tc-section-label">Scale</span>
        <button
          type="button"
          onClick={() => onToggleAutoScale(!autoScale)}
          className={`rounded px-1.5 py-0.5 text-[10px] font-semibold uppercase tracking-wide transition-colors cursor-pointer ${
            autoScale
              ? "bg-[var(--tc-accent-soft)] text-[var(--tc-accent)]"
              : "bg-[var(--tc-amber-soft)] text-[var(--tc-amber-ink)]"
          }`}
          title="Click to toggle Auto / Manual level & span"
        >
          {autoScale ? "Auto" : "Lock"}
        </button>
      </div>

      {/* Draggable Colorbar + Ticks */}
      <div className="relative flex flex-1 min-h-[180px] gap-1.5">
        <div
          ref={barRef}
          onMouseDown={(e) => startDrag("pan", e)}
          onDoubleClick={() => onToggleAutoScale(true)}
          className="relative w-6 shrink-0 cursor-ns-resize rounded border border-[var(--tc-border-strong)]"
          style={{ background: paletteGradientCss(palette, "to top") }}
          title="Drag middle to pan level, top/bottom handles to set span, double-click for Auto"
        >
          {/* Max handle */}
          <div
            onMouseDown={(e) => {
              e.stopPropagation();
              startDrag("max", e);
            }}
            className="absolute -top-1.5 -right-1 -left-1 h-3 cursor-n-resize rounded-sm border border-[var(--tc-border-strong)] bg-[var(--tc-surface)] shadow-sm"
            title="Drag to adjust upper limit"
          />
          {/* Min handle */}
          <div
            onMouseDown={(e) => {
              e.stopPropagation();
              startDrag("min", e);
            }}
            className="absolute -right-1 -bottom-1.5 -left-1 h-3 cursor-s-resize rounded-sm border border-[var(--tc-border-strong)] bg-[var(--tc-surface)] shadow-sm"
            title="Drag to adjust lower limit"
          />
        </div>

        {/* Temperature Ticks */}
        <div className="flex flex-1 flex-col justify-between py-0.5 text-[10.5px] font-mono-tech text-[var(--tc-text-muted)]">
          {ticks.map((f) => (
            <div key={f} className="leading-none">
              {formatTemp(minC + f * (maxC - minC), unit, 1)}
            </div>
          ))}
        </div>
      </div>

      {/* Live 256-bin Histogram */}
      <div className="flex flex-col gap-1 border-t border-[var(--tc-line)] pt-2">
        <div className="flex items-center justify-between text-[10px] text-[var(--tc-text-dim)]">
          <span>HISTOGRAM</span>
          <span className="font-mono-tech">
            {isotherm.mode !== "off" ? `ISO:${isotherm.mode.slice(0, 3).toUpperCase()}` : "256b"}
          </span>
        </div>
        <svg viewBox="0 0 128 42" className="h-[44px] w-full rounded bg-[var(--tc-surface-2)]">
          {HIST_BUCKETS.map((b) => {
            const sum =
              bins[b.offset] + bins[b.offset + 1] + bins[b.offset + 2] + bins[b.offset + 3];
            const h = Math.max(1, (sum / (maxBin * 4)) * 38);
            return (
              <rect
                key={b.id}
                x={b.x}
                y={42 - h}
                width={1.6}
                height={h}
                fill="var(--tc-accent)"
                opacity={0.85}
              />
            );
          })}
        </svg>
      </div>
    </div>
  );
}
