import { Copy, RefreshCw } from "lucide-react";
import { useEffect, useState } from "react";
import { fromCelsius, unitSymbol } from "@/lib/thermal";
import type { Diagnostics as DiagnosticsData, PerfTelemetry, TempUnit } from "@/lib/types";

/** Bits 0..4 of `diagnostic_mask` (1 = OK): display, touch, SD, battery, Lepton. */
const SELF_TEST_PARTS = ["screen", "touch screen", "SD card", "battery", "sensor"] as const;

export function selfTestFailures(mask: number): string[] {
  return SELF_TEST_PARTS.filter((_, bit) => (mask & (1 << bit)) === 0);
}

export function selfTestText(mask: number): string {
  const failed = selfTestFailures(mask);
  return failed.length === 0 ? "All OK" : `Failed: ${failed.join(", ")}`;
}

function hex(mask: number): string {
  return `0x${mask.toString(16).toUpperCase().padStart(2, "0")}`;
}

function fps(x100: number): string {
  return (x100 / 100).toFixed(2);
}

function tempPair(a: number, b: number, unit: TempUnit): string {
  const f = (c: number) => (Number.isFinite(c) ? fromCelsius(c, unit).toFixed(1) : "–");
  return `${f(a)} · ${f(b)} ${unitSymbol(unit)}`;
}

/** The sensor's part number without the name some firmware appends in brackets. */
function partNumber(part: string): string {
  return part.replace(/\s*\(.*\)\s*$/, "").trim() || part;
}

interface TimingRow {
  label: string;
  us: number;
}

function timingRows(perf: PerfTelemetry): TimingRow[] {
  return [
    { label: "Read sensor", us: perf.acquire_us },
    { label: "Statistics", us: perf.stats_us },
    { label: "Filter", us: perf.filter_us },
    { label: "Scale", us: perf.scale_us },
    { label: "Colors", us: perf.colorize_us },
    { label: "Overlay", us: perf.overlay_us },
    { label: "Camera screen", us: perf.display_us },
    { label: "USB transfer", us: perf.usb_tx_us },
  ];
}

export interface DiagnosticsProps {
  diagnostics: DiagnosticsData | null;
  perf: PerfTelemetry | null;
  unit: TempUnit;
  onRefresh: () => void;
  /** Everything for a bug report as plain text (firmware, settings, telemetry). */
  reportText: () => string;
}

/** `camera.html#diagnostics`: frame timing, rates and link, hardware; behind the Diagnostics row. */
export function Diagnostics({
  diagnostics: d,
  perf: p,
  unit,
  onRefresh,
  reportText,
}: DiagnosticsProps): React.JSX.Element {
  const [copied, setCopied] = useState(false);
  useEffect(() => {
    if (!copied) return;
    const id = setTimeout(() => setCopied(false), 1600);
    return () => clearTimeout(id);
  }, [copied]);

  const copy = async () => {
    try {
      await navigator.clipboard.writeText(reportText());
      setCopied(true);
    } catch {
      setCopied(false);
    }
  };

  const total = Math.max(1, p?.loop_total_us ?? 1);

  return (
    <div className="cm-diag">
      <div className="cm-diag-tools">
        <button type="button" className="tc-btn tc-btn-sm tc-btn-ghost" onClick={onRefresh}>
          <RefreshCw size={14} strokeWidth={1.75} aria-hidden="true" />
          Refresh
        </button>
        <button type="button" className="tc-btn tc-btn-sm" onClick={() => void copy()}>
          <Copy size={14} strokeWidth={1.75} aria-hidden="true" />
          {copied ? "Copied" : "Copy for a bug report"}
        </button>
      </div>
      <div className="cm-sec-h" style={{ borderTop: 0 }}>
        {p
          ? `Frame timing · ${(p.loop_total_us / 1000).toFixed(2)} ms of ${(p.frame_period_us / 1000).toFixed(1)} ms per frame`
          : "Frame timing"}
      </div>
      <div className="cm-timing">
        {p
          ? timingRows(p).map((row) => (
              <div key={row.label}>
                <span>{row.label}</span>
                <span className="cm-bar">
                  <i style={{ width: `${Math.min(100, (row.us / total) * 100).toFixed(1)}%` }} />
                </span>
                <span className="cm-mono">{row.us} µs</span>
              </div>
            ))
          : null}
      </div>
      <div className="cm-two">
        <div>
          <div className="cm-sec-h" style={{ borderTop: 0 }}>
            Rates and link
          </div>
          <dl className="cm-kv nowrap">
            <dt>Sensor</dt>
            <dd>{p ? `${fps(p.live_fps_x100)} fps (max 8.8)` : "–"}</dd>
            <dt>Camera screen</dt>
            <dd>{p ? `${fps(p.video_fps_x100)} fps` : "–"}</dd>
            <dt>USB stream</dt>
            <dd>{p ? `${fps(p.stream_fps_x100)} fps` : "–"}</dd>
            <dt>Resyncs · checksum errors</dt>
            <dd>{p ? `${p.vospi_resyncs} · ${p.vospi_crc_errors + p.usb_crc_errors}` : "–"}</dd>
            <dt>SD card write</dt>
            <dd>{p ? `${(p.sd_write_us / 1000).toFixed(1)} ms` : "–"}</dd>
          </dl>
        </div>
        <div>
          <div className="cm-sec-h" style={{ borderTop: 0 }}>
            Hardware
          </div>
          <dl className="cm-kv nowrap">
            <dt>Sensor part</dt>
            <dd>{d ? partNumber(d.lepton_part_num) : "–"}</dd>
            <dt>Sensor · processor temp.</dt>
            <dd>{d ? tempPair(d.lepton_fpa_temp_c, d.mcu_temp_c, unit) : "–"}</dd>
            <dt>Clock · power</dt>
            <dd>
              {d
                ? `${d.cpu_mhz} MHz · ${d.usb_power ? "USB" : "Battery"} ${(d.bat_mv / 1000).toFixed(2)} V`
                : "–"}
            </dd>
            <dt>SPI (screen · sensor)</dt>
            <dd>{p ? `${p.display_spi_mhz} · ${p.lepton_spi_mhz} MHz` : "–"}</dd>
            <dt>Free memory</dt>
            <dd>
              {d
                ? `${Math.round(d.free_dtcm_bytes / 1024)} · ${Math.round(d.free_ocram_bytes / 1024)} KB`
                : "–"}
            </dd>
            <dt>Self-test</dt>
            <dd className={d ? (selfTestFailures(d.diagnostic_mask).length ? "fail" : "pass") : ""}>
              {d ? `${selfTestText(d.diagnostic_mask)} (${hex(d.diagnostic_mask)})` : "–"}
            </dd>
          </dl>
        </div>
      </div>
    </div>
  );
}
