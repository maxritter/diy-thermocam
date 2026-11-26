import { Copy, Download } from "lucide-react";
import { useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import type { FrameStore } from "@/lib/frameStore";
import { perf } from "@/lib/perf";
import { roiColor, rowStat } from "@/lib/regions";
import { type ChartWindow, type SeriesSample, type SessionSeries, WINDOWS } from "@/lib/series";
import { formatTemp, fromCelsius, sampleLineProfile } from "@/lib/thermal";
import type { RadiometryParams, Roi, TempUnit } from "@/lib/types";
import { useSampledFrame } from "@/lib/useFrame";

const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

/** Height of the drawer folded to one line: the cards' title rows. */
export const FOLDED_HEIGHT = 54;
const MIN_HEIGHT = 140;
const MAX_HEIGHT = 560;

export interface ChartLine {
  key: string;
  label: string;
  color: string;
}

/** The chart's lines: Hottest, each region's row value (differences stay out), Coldest. */
export function chartLines(rois: Roi[]): ChartLine[] {
  const lines: ChartLine[] = [{ key: "hottest", label: "Hottest", color: "var(--s-max)" }];
  rois.forEach((r, i) => {
    if (r.geometry.kind === "delta") return;
    const label = rowStat(r.geometry.kind) === "max" ? `${r.label} max` : r.label;
    lines.push({ key: r.id, label, color: roiColor(r, i) });
  });
  lines.push({ key: "coldest", label: "Coldest", color: "var(--s-min)" });
  return lines;
}

/** Round grid values covering lo…hi, about four of them. */
function yGrid(lo: number, hi: number): { lo: number; hi: number; ticks: number[] } {
  if (!(hi > lo)) {
    lo -= 1;
    hi += 1;
  }
  const steps = [0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200];
  const step = steps.find((s) => (hi - lo) / s <= 4) ?? 500;
  const a = Math.floor(lo / step) * step;
  const b = Math.ceil(hi / step) * step;
  const ticks: number[] = [];
  for (let v = a; v <= b + step / 2; v += step) ticks.push(Number(v.toFixed(2)));
  return { lo: a, hi: b === a ? a + step : b, ticks };
}

function useSize<T extends Element>(): [React.RefObject<T | null>, number, number] {
  const ref = useRef<T | null>(null);
  const [size, setSize] = useState<[number, number]>([560, 150]);
  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const update = () => {
      const r = el.getBoundingClientRect();
      if (r.width > 0 && r.height > 0) {
        setSize((p) =>
          p[0] === Math.round(r.width) && p[1] === Math.round(r.height)
            ? p
            : [Math.round(r.width), Math.round(r.height)],
        );
      }
    };
    update();
    if (typeof ResizeObserver === "undefined") return;
    const ro = new ResizeObserver(update);
    ro.observe(el);
    return () => ro.disconnect();
  }, []);
  return [ref, size[0], size[1]];
}

function elapsed(ms: number): string {
  const s = Math.max(0, Math.round(ms / 1000));
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const ss = String(s % 60).padStart(2, "0");
  return h > 0 ? `${h}:${String(m).padStart(2, "0")}:${ss}` : `${m}:${ss}`;
}

function axisLabels(window: ChartWindow, from: number, to: number): string[] {
  switch (window) {
    case "1m":
      return ["−60 s", "−45 s", "−30 s", "−15 s", "now"];
    case "5m":
      return ["−5 min", "−4 min", "−3 min", "−2 min", "−1 min", "now"];
    case "30m":
      return ["−30 min", "−20 min", "−10 min", "now"];
    case "all":
      return [0, 0.25, 0.5, 0.75].map((f) => elapsed(from + f * (to - from))).concat("now");
  }
}

export interface Plot {
  key: string;
  points: SeriesSample[];
  color: string;
}

/**
 * A line chart drawn like studio.js renderTimeChart: gridlines, °-labels and the lines, with an
 * optional line at `cursor` (the recording player's current frame).
 */
export function LineChart({
  plots,
  from,
  to,
  xLabels,
  unit,
  label,
  cursor,
}: {
  plots: Plot[];
  from: number;
  to: number;
  xLabels: string[];
  unit: TempUnit;
  label: string;
  cursor?: number | null;
}): React.JSX.Element {
  const [ref, W, H] = useSize<SVGSVGElement>();
  const L = 34;
  const R = 8;
  const T = 6;
  const B = 18;
  let lo = Number.POSITIVE_INFINITY;
  let hi = Number.NEGATIVE_INFINITY;
  for (const p of plots) {
    for (const s of p.points) {
      const v = fromCelsius(s.v, unit);
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
  }
  const grid = Number.isFinite(lo) ? yGrid(lo, hi) : yGrid(20, 40);
  const span = Math.max(1, to - from);
  const xOf = (t: number) => L + ((t - from) / span) * (W - L - R);
  const yOf = (v: number) => T + (1 - (v - grid.lo) / (grid.hi - grid.lo)) * (H - T - B);
  const last = xLabels.length - 1;
  return (
    <svg ref={ref} className="st-chart" viewBox={`0 0 ${W} ${H}`} role="img" aria-label={label}>
      <g className="grid">
        {grid.ticks.map((t) => (
          <g key={t}>
            <line x1={L} x2={W - R} y1={yOf(t)} y2={yOf(t)} />
            <text x={L - 6} y={yOf(t) + 3.5} textAnchor="end">
              {t}°
            </text>
          </g>
        ))}
      </g>
      {xLabels.map((text, i) => (
        <text
          key={text}
          x={L + (i / Math.max(1, last)) * (W - L - R)}
          y={H - 3}
          textAnchor={i === 0 ? "start" : i === last ? "end" : "middle"}
        >
          {text}
        </text>
      ))}
      {plots.map((p) =>
        p.points.length > 0 ? (
          <path
            key={p.key}
            className="ln"
            style={{ stroke: p.color }}
            d={p.points
              .map(
                (s, i) =>
                  `${i ? "L" : "M"}${xOf(s.t).toFixed(1)} ${yOf(fromCelsius(s.v, unit)).toFixed(1)}`,
              )
              .join(" ")}
          />
        ) : null,
      )}
      {cursor !== undefined && cursor !== null ? (
        <line
          className="cur"
          data-cursor={cursor}
          x1={xOf(cursor)}
          x2={xOf(cursor)}
          y1={T}
          y2={H - B}
        />
      ) : null}
    </svg>
  );
}

export interface ChartDrawerProps {
  store: FrameStore;
  series: SessionSeries;
  rois: Roi[];
  rad: RadiometryParams;
  unit: TempUnit;
  height: number;
  onHeightChange: (height: number) => void;
  onClear: () => void;
  onSaveCsv: () => void;
  /** Puts text on the clipboard (Copy). */
  onCopy: (text: string, what: string) => void;
}

/**
 * The drawer below the image (live.html#measuring): each region's value over time with its
 * window, and the temperature along a line. The grip resizes it; a double-click folds it.
 */
export function ChartDrawer({
  store,
  series,
  rois,
  rad,
  unit,
  height,
  onHeightChange,
  onClear,
  onSaveCsv,
  onCopy,
}: ChartDrawerProps): React.JSX.Element {
  const frame = useSampledFrame(store);
  perf.panelRendered("chart");
  const [chartWindow, setChartWindow] = useState<ChartWindow>("1m");
  const [folded, setFolded] = useState(false);
  const [dragH, setDragH] = useState<number | null>(null);
  const lineRois = rois.filter((r) => r.geometry.kind === "line");
  const [alongId, setAlongId] = useState<string | null>(null);
  const along = lineRois.find((r) => r.id === alongId) ?? lineRois[0] ?? null;
  const lines = useMemo(() => chartLines(rois), [rois]);

  useEffect(() => {
    if (alongId && !lineRois.some((r) => r.id === alongId)) setAlongId(null);
  }, [alongId, lineRois]);

  const now = series.now();
  const w = WINDOWS.find((x) => x.id === chartWindow) ?? WINDOWS[0];
  const from = w.ms === null ? 0 : now - w.ms;
  // `frame` is only read so the chart redraws with the sampled store (at most 4 times a second).
  const plots = frame
    ? lines.map((l) => ({
        key: l.key,
        points: series.points(l.key, chartWindow, now),
        color: l.color,
      }))
    : [];

  const profile = useMemo(() => {
    if (!along || !frame || along.geometry.kind !== "line") return null;
    const g = along.geometry;
    const own: RadiometryParams = {
      ...rad,
      emissivity: along.emissivity_override ?? rad.emissivity,
      reflected_temp_c: along.reflected_temp_override_c ?? rad.reflected_temp_c,
    };
    const samples = sampleLineProfile(
      frame.pixels,
      frame.width,
      frame.height,
      g.x0,
      g.y0,
      g.x1,
      g.y1,
      own,
    );
    return samples;
  }, [along, frame, rad]);
  const profileMax = profile ? Math.max(...profile.map((s) => s.temp_c)) : null;
  const alongIndex = along ? rois.indexOf(along) : 0;

  const startResize = (e: React.PointerEvent<HTMLButtonElement>) => {
    if (e.button !== 0) return;
    e.preventDefault();
    const startY = e.clientY;
    const startH = folded ? FOLDED_HEIGHT : height;
    let next = startH;
    const onMove = (ev: PointerEvent) => {
      // The image keeps at least about 400 px of the window.
      const roomy = Math.max(MIN_HEIGHT, window.innerHeight - 520);
      next = Math.max(MIN_HEIGHT, Math.min(MAX_HEIGHT, roomy, startH + (startY - ev.clientY)));
      setFolded(false);
      setDragH(next);
    };
    const onUp = () => {
      window.removeEventListener("pointermove", onMove);
      window.removeEventListener("pointerup", onUp);
      setDragH(null);
      if (next !== startH) onHeightChange(Math.round(next));
    };
    window.addEventListener("pointermove", onMove);
    window.addEventListener("pointerup", onUp);
  };

  const copyOverTime = () => {
    const rows = ["seconds,line,value"];
    lines.forEach((l, i) => {
      for (const s of plots[i]?.points ?? []) {
        rows.push(`${(s.t / 1000).toFixed(2)},${l.label},${fromCelsius(s.v, unit).toFixed(2)}`);
      }
    });
    onCopy(rows.join("\n"), "Readings");
  };

  const copyProfile = () => {
    if (!profile) return;
    const rows = ["distance_px,x,y,value"];
    for (const s of profile) {
      rows.push(`${s.dist_px.toFixed(2)},${s.x},${s.y},${fromCelsius(s.temp_c, unit).toFixed(2)}`);
    }
    onCopy(rows.join("\n"), `Temperatures along ${along?.label ?? "the line"}`);
  };

  const h = folded ? FOLDED_HEIGHT : (dragH ?? height);

  return (
    <div
      className={`st-drawer${folded ? " folded" : ""}`}
      style={{
        height: h,
        gridTemplateColumns: along ? "minmax(0, 1.6fr) minmax(0, 1fr)" : "minmax(0, 1fr)",
      }}
    >
      <button
        className="st-grip"
        type="button"
        aria-label="Drag to resize the charts, double-click to fold them"
        onPointerDown={startResize}
        onDoubleClick={() => setFolded((f) => !f)}
      />
      <section className="st-card" aria-label="Readings over time">
        <div className="st-card-h">
          <h3>Over time</h3>
          <div className="st-row-actions">
            <select
              className="st-select"
              style={{ height: 28, width: "auto", fontSize: 12 }}
              aria-label="Time window"
              value={chartWindow}
              onChange={(e) => setChartWindow(e.target.value as ChartWindow)}
            >
              {WINDOWS.map((x) => (
                <option key={x.id} value={x.id}>
                  {x.label}
                </option>
              ))}
            </select>
            <button className="tc-btn tc-btn-sm tc-btn-ghost" type="button" onClick={copyOverTime}>
              <Copy {...SM} />
              Copy
            </button>
            <button className="tc-btn tc-btn-sm tc-btn-ghost" type="button" onClick={onSaveCsv}>
              <Download {...SM} />
              Save CSV…
            </button>
            <button className="tc-btn tc-btn-sm tc-btn-ghost" type="button" onClick={onClear}>
              Clear
            </button>
          </div>
        </div>
        {folded ? null : (
          <>
            <div className="st-legend">
              {lines.map((l) => (
                <span key={l.key}>
                  <i style={{ background: l.color }} />
                  {l.label}
                </span>
              ))}
            </div>
            <LineChart
              plots={plots}
              from={from}
              to={Math.max(now, from + 1)}
              xLabels={axisLabels(chartWindow, from, now)}
              unit={unit}
              label={`The value of each row, ${w.label.toLowerCase()}`}
            />
          </>
        )}
      </section>
      {along ? (
        <section className="st-card" aria-label={`Temperature along ${along.label}`}>
          <div className="st-card-h">
            {lineRois.length > 1 ? (
              <h3 style={{ display: "flex", alignItems: "center", gap: 6 }}>
                Along
                <select
                  className="st-select"
                  style={{ height: 28, width: "auto", fontSize: 12 }}
                  aria-label="Line"
                  value={along.id}
                  onChange={(e) => setAlongId(e.target.value)}
                >
                  {lineRois.map((r) => (
                    <option key={r.id} value={r.id}>
                      {r.label}
                    </option>
                  ))}
                </select>
              </h3>
            ) : (
              <h3>Along {along.label}</h3>
            )}
            <button className="tc-btn tc-btn-sm tc-btn-ghost" type="button" onClick={copyProfile}>
              <Copy {...SM} />
              Copy
            </button>
          </div>
          {folded ? null : (
            <>
              <div className="st-legend">
                <span>
                  {profile?.length ?? 0} points
                  {profileMax !== null ? ` · max ${formatTemp(profileMax, unit)}` : ""}
                </span>
              </div>
              <LineChart
                plots={
                  profile
                    ? [
                        {
                          key: along.id,
                          points: profile.map((s) => ({ t: s.dist_px, v: s.temp_c })),
                          color: roiColor(along, alongIndex),
                        },
                      ]
                    : []
                }
                from={0}
                to={Math.max(1, profile?.[profile.length - 1]?.dist_px ?? 1)}
                xLabels={["start", "end"]}
                unit={unit}
                label={`Temperature along ${along.label}`}
              />
            </>
          )}
        </section>
      ) : null}
    </div>
  );
}
