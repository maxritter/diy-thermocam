import { Check, Contrast, Copy, Download, Grid3x3, Trash2 } from "lucide-react";
import { useMemo, useState } from "react";
import { StudioSwitch } from "@/components/ui/Primitives";
import { formatTemp, fromCelsius, sampleLineProfile, unitSymbol } from "@/lib/thermal";
import type {
  RadiometryParams,
  Roi,
  RoiStats,
  TempUnit,
  ThermalFrame,
  TimeSeriesPoint,
} from "@/lib/types";

interface MeasurementsPanelProps {
  frame: ThermalFrame;
  rad: RadiometryParams;
  unit: TempUnit;
  rois: Roi[];
  roiStats: RoiStats[];
  history: TimeSeriesPoint[];
  showGrid: boolean;
  gridSize: [number, number];
  onToggleGrid: (show: boolean) => void;
  onGridSizeChange: (size: [number, number]) => void;
  onAutoScaleToRoi: (roi: Roi) => void;
  onClearHistory: () => void;
}

const SERIES_COLORS = ["#5da3e5", "#f97316", "#6fd39f", "#f7b23b", "#c084fc", "#fb7185"];

export function MeasurementsPanel({
  frame,
  rad,
  unit,
  rois,
  roiStats,
  history,
  showGrid,
  gridSize,
  onToggleGrid,
  onGridSizeChange,
  onAutoScaleToRoi,
  onClearHistory,
}: MeasurementsPanelProps): React.JSX.Element {
  const lineRoi = rois.find((r) => r.geometry.kind === "line") ?? null;
  const lineSamples = useMemo(() => {
    if (lineRoi?.geometry.kind !== "line") return [];
    const { x0, y0, x1, y1 } = lineRoi.geometry;
    return sampleLineProfile(frame.pixels, frame.width, frame.height, x0, y0, x1, y1, rad);
  }, [lineRoi, frame, rad]);

  const seriesKeys = useMemo(() => {
    const keys = ["Frame Max", "Frame Min", ...rois.slice(0, 4).map((r) => r.label)];
    return Array.from(new Set(keys));
  }, [rois]);

  const [copiedCsv, setCopiedCsv] = useState(false);

  const buildCsvText = (): string => {
    const headers = ["time_s", ...seriesKeys];
    const rows = history.map((pt) =>
      [
        pt.tSec.toFixed(2),
        ...seriesKeys.map((k) => {
          const v = pt.values[k];
          return v !== undefined ? fromCelsius(v, unit).toFixed(2) : "";
        }),
      ].join(","),
    );
    return [headers.join(","), ...rows].join("\n");
  };

  const exportCsv = (): void => {
    const csv = buildCsvText();
    const blob = new Blob([csv], { type: "text/csv;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "thermocam_time_series.csv";
    a.click();
    URL.revokeObjectURL(url);
  };

  const copyCsvToClipboard = async (): Promise<void> => {
    const roiHeader = "roi,min,mean,max,std_dev,emissivity";
    const roiRows = roiStats.map((st) =>
      [
        st.label,
        fromCelsius(st.min_c, unit).toFixed(2),
        fromCelsius(st.mean_c, unit).toFixed(2),
        fromCelsius(st.max_c, unit).toFixed(2),
        st.std_dev_c.toFixed(2),
        st.effective_emissivity.toFixed(2),
      ].join(","),
    );
    const payload = [roiHeader, ...roiRows, "", buildCsvText()].join("\n");
    try {
      await navigator.clipboard.writeText(payload);
      setCopiedCsv(true);
      setTimeout(() => setCopiedCsv(false), 2000);
    } catch {
      // Clipboard API unavailable in non-secure headless contexts
    }
  };

  // Compute time-plotter SVG bounds
  let tMin = Number.POSITIVE_INFINITY;
  let tMax = Number.NEGATIVE_INFINITY;
  for (const pt of history) {
    for (const k of seriesKeys) {
      const v = pt.values[k];
      if (v !== undefined) {
        if (v < tMin) tMin = v;
        if (v > tMax) tMax = v;
      }
    }
  }
  if (!Number.isFinite(tMin) || !Number.isFinite(tMax)) {
    tMin = 15;
    tMax = 70;
  }
  const tSpan = Math.max(2, tMax - tMin);

  return (
    <div className="grid h-full grid-cols-12 gap-3 overflow-y-auto p-3">
      {/* 1. Full ROI Measurement Table */}
      <div className="tc-card col-span-12 p-3.5">
        <div className="mb-3 flex items-center justify-between">
          <div>
            <h2 className="text-[14px] font-semibold">Measurements &amp; Per-ROI Radiometry</h2>
            <p className="text-[12px] text-[var(--tc-text-muted)]">
              All values computed directly from raw 16-bit sensor data with per-ROI Stefan-Boltzmann
              emissivity inversion.
            </p>
          </div>
          <div className="flex items-center gap-3">
            <StudioSwitch
              checked={showGrid}
              onCheckedChange={onToggleGrid}
              label="N×M Grid Overlay"
            />
            {showGrid ? (
              <select
                value={`${gridSize[0]}x${gridSize[1]}`}
                onChange={(e) => {
                  const [c, r] = e.target.value.split("x").map(Number);
                  onGridSizeChange([c, r]);
                }}
                className="tc-select w-24"
              >
                <option value="3x3">3 × 3</option>
                <option value="4x3">4 × 3</option>
                <option value="5x4">5 × 4</option>
                <option value="6x4">6 × 4</option>
              </select>
            ) : null}
          </div>
        </div>

        <div className="overflow-x-auto">
          <table className="w-full border-collapse text-left text-[12.5px]">
            <thead>
              <tr className="border-b border-[var(--tc-border)] text-[11px] uppercase tracking-wider text-[var(--tc-text-dim)]">
                <th className="py-2 pr-3">ROI</th>
                <th className="py-2 pr-3">Type</th>
                <th className="py-2 pr-3 text-right">Min</th>
                <th className="py-2 pr-3 text-right">Mean</th>
                <th className="py-2 pr-3 text-right">Max</th>
                <th className="py-2 pr-3 text-right">Std Dev (σ)</th>
                <th className="py-2 pr-3 text-right">ΔT</th>
                <th className="py-2 pr-3 text-right">Emissivity (ε)</th>
                <th className="py-2 pr-3 text-right">Pixels</th>
                <th className="py-2 text-right">Contrast</th>
              </tr>
            </thead>
            <tbody className="font-mono-tech">
              {rois.map((roi) => {
                const st = roiStats.find((s) => s.id === roi.id);
                if (!st) return null;
                return (
                  <tr key={roi.id} className="border-b border-[var(--tc-line)] last:border-b-0">
                    <td className="py-2 pr-3 font-sans font-medium">{roi.label}</td>
                    <td className="py-2 pr-3 font-sans capitalize text-[var(--tc-text-muted)]">
                      {roi.geometry.kind}
                    </td>
                    <td className="py-2 pr-3 text-right">{formatTemp(st.min_c, unit, 2)}</td>
                    <td className="py-2 pr-3 text-right font-semibold">
                      {formatTemp(st.mean_c, unit, 2)}
                    </td>
                    <td className="py-2 pr-3 text-right text-[var(--tc-thermal)]">
                      {formatTemp(st.max_c, unit, 2)}
                    </td>
                    <td className="py-2 pr-3 text-right">{st.std_dev_c.toFixed(2)}</td>
                    <td className="py-2 pr-3 text-right">
                      {st.delta_c !== null
                        ? `${st.delta_c >= 0 ? "+" : ""}${st.delta_c.toFixed(2)} ${unitSymbol(unit)}`
                        : "—"}
                    </td>
                    <td className="py-2 pr-3 text-right">{st.effective_emissivity.toFixed(2)}</td>
                    <td className="py-2 pr-3 text-right">{st.pixel_count}</td>
                    <td className="py-2 text-right">
                      <button
                        type="button"
                        onClick={() => onAutoScaleToRoi(roi)}
                        className="tc-btn h-7 px-2 text-[11px]"
                      >
                        <Contrast className="h-3 w-3" />
                        Scale
                      </button>
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      </div>

      {/* 2. Live Multi-Series Temperature-vs-Time Plotter */}
      <div className="tc-card col-span-7 flex flex-col p-3.5">
        <div className="mb-2.5 flex items-center justify-between">
          <div>
            <h3 className="text-[13.5px] font-semibold">Live Temperature-vs-Time Plotter</h3>
            <p className="text-[11.5px] text-[var(--tc-text-muted)]">
              Rolling multi-series telemetry ({history.length} samples)
            </p>
          </div>
          <div className="flex items-center gap-1.5">
            <button
              type="button"
              onClick={() => void copyCsvToClipboard()}
              className="tc-btn text-[12px]"
            >
              {copiedCsv ? (
                <Check className="h-3.5 w-3.5 text-[var(--tc-pass)]" />
              ) : (
                <Copy className="h-3.5 w-3.5" />
              )}
              {copiedCsv ? "Copied CSV" : "Copy CSV"}
            </button>
            <button type="button" onClick={exportCsv} className="tc-btn text-[12px]">
              <Download className="h-3.5 w-3.5" />
              Export CSV
            </button>
            <button type="button" onClick={onClearHistory} className="tc-btn text-[12px]">
              <Trash2 className="h-3.5 w-3.5" />
              Clear
            </button>
          </div>
        </div>

        <div className="mb-2 flex flex-wrap gap-3 text-[11.5px]">
          {seriesKeys.map((key, idx) => {
            const latest = history[history.length - 1]?.values[key];
            return (
              <div key={key} className="flex items-center gap-1.5">
                <span
                  className="inline-block h-2.5 w-2.5 rounded-full"
                  style={{ backgroundColor: SERIES_COLORS[idx % SERIES_COLORS.length] }}
                />
                <span className="text-[var(--tc-text-muted)]">{key}</span>
                {latest !== undefined ? (
                  <span className="font-mono-tech font-medium">{formatTemp(latest, unit, 1)}</span>
                ) : null}
              </div>
            );
          })}
        </div>

        <svg
          viewBox="0 0 600 200"
          className="h-[200px] w-full rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)]"
        >
          {[0.2, 0.4, 0.6, 0.8].map((f) => (
            <line
              key={f}
              x1={44}
              y1={16 + f * 164}
              x2={586}
              y2={16 + f * 164}
              stroke="var(--tc-border)"
              strokeDasharray="3 3"
            />
          ))}
          <text x={6} y={24} fontSize="10" fill="var(--tc-text-dim)" fontFamily="var(--font-mono)">
            {formatTemp(tMax, unit, 1)}
          </text>
          <text x={6} y={182} fontSize="10" fill="var(--tc-text-dim)" fontFamily="var(--font-mono)">
            {formatTemp(tMin, unit, 1)}
          </text>
          {seriesKeys.map((key, idx) => {
            const pts = history
              .map((pt, i) => {
                const v = pt.values[key];
                if (v === undefined) return null;
                const x = 44 + (i / Math.max(1, history.length - 1)) * 542;
                const y = 180 - ((v - tMin) / tSpan) * 164;
                return `${x.toFixed(1)},${y.toFixed(1)}`;
              })
              .filter(Boolean)
              .join(" ");
            return (
              <polyline
                key={key}
                fill="none"
                stroke={SERIES_COLORS[idx % SERIES_COLORS.length]}
                strokeWidth="2"
                points={pts}
              />
            );
          })}
        </svg>
      </div>

      {/* 3. Bidirectional Line Profile Chart */}
      <div className="tc-card col-span-5 flex flex-col p-3.5">
        <div className="mb-2.5 flex items-center justify-between">
          <div>
            <h3 className="text-[13.5px] font-semibold">Line Profile Cross-Section</h3>
            <p className="text-[11.5px] text-[var(--tc-text-muted)]">
              {lineRoi
                ? `${lineRoi.label} · ${lineSamples.length} px samples`
                : "Draw a Line ROI on the viewport"}
            </p>
          </div>
          <Grid3x3 className="h-4 w-4 text-[var(--tc-text-dim)]" />
        </div>

        {lineSamples.length >= 2 ? (
          (() => {
            const lo = Math.min(...lineSamples.map((s) => s.temp_c));
            const hi = Math.max(...lineSamples.map((s) => s.temp_c));
            const span = Math.max(0.5, hi - lo);
            const pts = lineSamples
              .map((s, i) => {
                const x = 42 + (i / (lineSamples.length - 1)) * 340;
                const y = 178 - ((s.temp_c - lo) / span) * 156;
                return `${x.toFixed(1)},${y.toFixed(1)}`;
              })
              .join(" ");
            return (
              <svg
                viewBox="0 0 400 200"
                className="h-[200px] w-full rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)]"
              >
                <polyline fill="none" stroke="var(--tc-amber)" strokeWidth="2.2" points={pts} />
                <text
                  x={6}
                  y={24}
                  fontSize="10"
                  fill="var(--tc-text-dim)"
                  fontFamily="var(--font-mono)"
                >
                  {formatTemp(hi, unit, 1)}
                </text>
                <text
                  x={6}
                  y={182}
                  fontSize="10"
                  fill="var(--tc-text-dim)"
                  fontFamily="var(--font-mono)"
                >
                  {formatTemp(lo, unit, 1)}
                </text>
              </svg>
            );
          })()
        ) : (
          <div className="flex h-[200px] items-center justify-center rounded-lg border border-dashed border-[var(--tc-border)] text-[12px] text-[var(--tc-text-dim)]">
            No active Line ROI
          </div>
        )}
      </div>
    </div>
  );
}
