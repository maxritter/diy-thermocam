import { memo, useMemo, useRef } from "react";
import type { FrameStore } from "@/lib/frameStore";
import {
  type FrameSize,
  handlePositions,
  labelAnchor,
  roiColor,
  rowStat,
  rowValue,
} from "@/lib/regions";
import { evaluateRoi, formatTemp, rawToObjectC, unitSymbol } from "@/lib/thermal";
import type { RadiometryParams, Roi, RoiStats, TempUnit } from "@/lib/types";
import { useSampledFrame } from "@/lib/useFrame";

/** "Δ 14.7 °C": a difference, with a minus sign only when it is negative. */
export function formatDifference(deltaC: number, unit: TempUnit): string {
  const scaled = unit === "fahrenheit" ? (deltaC * 9) / 5 : deltaC;
  return `Δ ${scaled < 0 ? "−" : ""}${Math.abs(scaled).toFixed(1)} ${unitSymbol(unit)}`;
}

/** The value a region's label shows: "52.7 °C" for a spot, "max 50.8 °C", "Δ 14.7 °C". */
export function labelValue(roi: Roi, stats: RoiStats | undefined, unit: TempUnit): string {
  if (!stats || stats.pixel_count === 0) return "";
  const stat = rowStat(roi.geometry.kind);
  const v = rowValue(roi.geometry.kind, stats);
  if (v === null) return "";
  if (stat === "delta") return formatDifference(v, unit);
  return stat === "max" ? `max ${formatTemp(v, unit)}` : formatTemp(v, unit);
}

const pct = (v: number, of: number) => `${(v / of) * 100}%`;

interface RegionLayerProps {
  store: FrameStore;
  size: FrameSize;
  rois: Roi[];
  selectedRoiId: string | null;
  /** Regions whose alarm is firing; drawn amber. */
  alertIds?: ReadonlySet<string>;
  rad: RadiometryParams;
  unit: TempUnit;
  showHotCold: boolean;
  showGrid: boolean;
  gridCols: number;
  gridRows: number;
  renamingId: string | null;
  onStartRename: (id: string) => void;
  onRename: (id: string, name: string | null) => void;
}

function Shape({
  roi,
  cls,
}: {
  roi: Roi;
  /** Stroke class on top of the dark outline: w (white), a (selected), am (alarm). */
  cls: string;
}): React.JSX.Element | null {
  const g = roi.geometry;
  switch (g.kind) {
    case "spot": {
      const cx = g.x + 0.5;
      const cy = g.y + 0.5;
      const r = g.kernel >= 3 ? 3 : 2.2;
      const t = r + 2.5;
      const ticks = `M${cx} ${cy - t}v2.5M${cx} ${cy + r}v2.5M${cx - t} ${cy}h2.5M${cx + r} ${cy}h2.5`;
      return (
        <g>
          <circle className="o" cx={cx} cy={cy} r={r} />
          <path className="o" d={ticks} />
          <circle className={cls} cx={cx} cy={cy} r={r} />
          <path className={cls} d={ticks} />
        </g>
      );
    }
    case "box":
      return (
        <g>
          <rect className="o" x={g.x} y={g.y} width={g.w} height={g.h} />
          <rect className={cls} x={g.x} y={g.y} width={g.w} height={g.h} />
        </g>
      );
    case "circle":
      return (
        <g>
          <circle className="o" cx={g.cx + 0.5} cy={g.cy + 0.5} r={g.radius} />
          <circle className={cls} cx={g.cx + 0.5} cy={g.cy + 0.5} r={g.radius} />
        </g>
      );
    case "polygon": {
      const pts = g.points.map(([x, y]) => `${x + 0.5},${y + 0.5}`).join(" ");
      return (
        <g>
          <polygon className="o" points={pts} />
          <polygon className={cls} points={pts} />
        </g>
      );
    }
    case "line":
    case "delta": {
      const dash = g.kind === "delta" ? " dash" : "";
      const p = { x1: g.x0 + 0.5, y1: g.y0 + 0.5, x2: g.x1 + 0.5, y2: g.y1 + 0.5 };
      return (
        <g>
          <line className="o" {...p} />
          <line className={`${cls}${dash}`} {...p} />
          <circle className="dot" cx={p.x1} cy={p.y1} r={1.3} />
          <circle className="dot" cx={p.x2} cy={p.y2} r={1.3} />
        </g>
      );
    }
  }
}

function RenameInput({
  name,
  onDone,
}: {
  name: string;
  onDone: (name: string | null) => void;
}): React.JSX.Element {
  const done = useRef(false);
  const finish = (value: string | null) => {
    if (done.current) return;
    done.current = true;
    onDone(value);
  };
  return (
    <input
      className="st-tag-input"
      aria-label="Region name"
      defaultValue={name}
      // biome-ignore lint/a11y/noAutofocus: the field opens because the person asked to rename.
      autoFocus
      size={Math.max(4, name.length + 1)}
      onFocus={(e) => e.currentTarget.select()}
      onKeyDown={(e) => {
        e.stopPropagation();
        if (e.key === "Enter") finish(e.currentTarget.value);
        if (e.key === "Escape") finish(null);
      }}
      onBlur={(e) => finish(e.currentTarget.value)}
    />
  );
}

/**
 * Regions, their labels and handles, the hottest and coldest marks and grid readings. It
 * re-renders when a region changes and at most four times a second for the values.
 */
export const RegionLayer = memo(function RegionLayer({
  store,
  size,
  rois,
  selectedRoiId,
  alertIds,
  rad,
  unit,
  showHotCold,
  showGrid,
  gridCols,
  gridRows,
  renamingId,
  onStartRename,
  onRename,
}: RegionLayerProps): React.JSX.Element {
  const frame = useSampledFrame(store);
  const { width: W, height: H } = size;

  const stats = useMemo(() => {
    const map = new Map<string, RoiStats>();
    if (!frame || frame.width !== W || frame.height !== H) return map;
    for (const r of rois) map.set(r.id, evaluateRoi(r, frame.pixels, W, H, rad));
    return map;
  }, [rois, frame, rad, W, H]);

  const selected = rois.find((r) => r.id === selectedRoiId && r.visible) ?? null;

  const grid = useMemo(() => {
    if (!showGrid || !frame) return [];
    const out: { key: string; x: number; y: number; c: number }[] = [];
    for (let r = 0; r < gridRows; r++) {
      const y = Math.min(H - 1, Math.floor(((r * 2 + 1) * H) / (gridRows * 2)));
      for (let c = 0; c < gridCols; c++) {
        const x = Math.min(W - 1, Math.floor(((c * 2 + 1) * W) / (gridCols * 2)));
        out.push({ key: `${x}_${y}`, x, y, c: rawToObjectC(frame.pixels[y * W + x], rad) });
      }
    }
    return out;
  }, [showGrid, frame, gridCols, gridRows, W, H, rad]);

  const marks = useMemo(() => {
    if (!showHotCold || !frame) return null;
    const cx = Math.floor(W / 2);
    const cy = Math.floor(H / 2);
    return {
      hot: { x: frame.max_x + 0.5, y: frame.max_y + 0.5, c: rawToObjectC(frame.max_raw, rad) },
      cold: { x: frame.min_x + 0.5, y: frame.min_y + 0.5, c: rawToObjectC(frame.min_raw, rad) },
      center: { x: cx + 0.5, y: cy + 0.5, c: rawToObjectC(frame.pixels[cy * W + cx], rad) },
    };
  }, [showHotCold, frame, W, H, rad]);

  return (
    <>
      <svg
        className="st-ov"
        viewBox={`0 0 ${W} ${H}`}
        preserveAspectRatio="none"
        aria-hidden="true"
      >
        {marks ? (
          <g>
            <circle className="o" cx={marks.hot.x} cy={marks.hot.y} r={2.6} />
            <circle className="w" cx={marks.hot.x} cy={marks.hot.y} r={2.6} />
            <circle className="o" cx={marks.cold.x} cy={marks.cold.y} r={2.6} />
            <circle className="w" cx={marks.cold.x} cy={marks.cold.y} r={2.6} />
            <path
              className="o"
              d={`M${marks.center.x} ${marks.center.y - 3.5}v7M${marks.center.x - 3.5} ${marks.center.y}h7`}
            />
            <path
              className="w"
              d={`M${marks.center.x} ${marks.center.y - 3.5}v7M${marks.center.x - 3.5} ${marks.center.y}h7`}
            />
          </g>
        ) : null}
        {grid.map((p) => (
          <circle key={p.key} className="dot" cx={p.x + 0.5} cy={p.y + 0.5} r={1.1} />
        ))}
        {rois.map((roi) =>
          roi.visible ? (
            <Shape
              key={roi.id}
              roi={roi}
              cls={alertIds?.has(roi.id) ? "am" : roi.id === selectedRoiId ? "a" : "w"}
            />
          ) : null,
        )}
      </svg>
      <div className="st-chips">
        {marks ? (
          <>
            <span
              className="st-tag pt"
              style={{ left: pct(marks.hot.x, W), top: pct(marks.hot.y, H) }}
            >
              <span className="st-tri hot" />
              {formatTemp(marks.hot.c, unit)}
            </span>
            <span
              className="st-tag pt"
              style={{ left: pct(marks.cold.x, W), top: pct(marks.cold.y, H) }}
            >
              <span className="st-tri cold" />
              {formatTemp(marks.cold.c, unit)}
            </span>
            <span
              className="st-tag right"
              style={{ left: pct(marks.center.x + 3.5, W), top: pct(marks.center.y, H) }}
            >
              {formatTemp(marks.center.c, unit)}
            </span>
          </>
        ) : null}
        {grid.map((p) => (
          <span
            key={p.key}
            className="st-tag grid"
            style={{ left: pct(p.x + 0.5, W), top: pct(p.y + 0.5, H) }}
          >
            {formatTemp(p.c, unit)}
          </span>
        ))}
        {rois.map((roi, i) => {
          if (!roi.visible) return null;
          const a = labelAnchor(roi.geometry, size);
          const cls = [
            "st-tag",
            a.place === "below" ? "below" : "",
            alertIds?.has(roi.id) ? "alert" : roi.id === selectedRoiId ? "sel" : "",
          ]
            .filter(Boolean)
            .join(" ");
          return (
            <span
              key={roi.id}
              className={cls}
              data-region-label={roi.id}
              style={{ left: pct(a.x, W), top: pct(a.y, H) }}
              onDoubleClick={(e) => {
                e.stopPropagation();
                onStartRename(roi.id);
              }}
            >
              <span className="sd" style={{ background: roiColor(roi, i) }} />
              {renamingId === roi.id ? (
                <RenameInput name={roi.label} onDone={(name) => onRename(roi.id, name)} />
              ) : (
                <span className="nm">{roi.label}</span>
              )}
              {labelValue(roi, stats.get(roi.id), unit)}
            </span>
          );
        })}
        {selected
          ? handlePositions(selected.geometry).map((h) => (
              <span
                key={`${h.handle.kind}-${"h" in h.handle ? h.handle.h : "index" in h.handle ? h.handle.index : "r"}`}
                className="st-handle"
                style={{ left: pct(h.x, W), top: pct(h.y, H) }}
              />
            ))
          : null}
      </div>
    </>
  );
});
