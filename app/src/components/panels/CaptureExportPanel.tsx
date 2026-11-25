import {
  CircleDot,
  Clock,
  FileSpreadsheet,
  FileText,
  Film,
  FolderOpen,
  Image as ImageIcon,
  Layers,
  Square,
} from "lucide-react";
import { useState } from "react";
import { StudioBadge, StudioSwitch } from "@/components/ui/Primitives";
import type { DisplayPipeline, ExportResolution, UpscaleMethod } from "@/lib/types";

interface CaptureExportPanelProps {
  pipeline: DisplayPipeline;
  onPipelineChange: (p: DisplayPipeline) => void;
  recording: boolean;
  recordedCount: number;
  timelapseActive: boolean;
  timelapseIntervalSec: number;
  timelapseCount: number;
  timelapseDir: string | null;
  onPickTimelapseDir: () => Promise<void>;
  onTimelapseIntervalChange: (sec: number) => void;
  onToggleTimelapse: () => void;
  hwButtonTrigger: boolean;
  onToggleHwButtonTrigger: (enabled: boolean) => void;
  onToggleRecording: () => Promise<void>;
  onExport: (
    format: string,
    opts: {
      resolution: ExportResolution;
      showColorbar: boolean;
      showHud: boolean;
      title: string;
      notes: string;
    },
  ) => Promise<void>;
  lastExportMsg: string | null;
}

export function CaptureExportPanel({
  pipeline,
  onPipelineChange,
  recording,
  recordedCount,
  timelapseActive,
  timelapseIntervalSec,
  timelapseCount,
  timelapseDir,
  onPickTimelapseDir,
  onTimelapseIntervalChange,
  onToggleTimelapse,
  hwButtonTrigger,
  onToggleHwButtonTrigger,
  onToggleRecording,
  onExport,
  lastExportMsg,
}: CaptureExportPanelProps): React.JSX.Element {
  const [resolution, setResolution] = useState<ExportResolution>("full_hd1080p");
  const [showColorbar, setShowColorbar] = useState(true);
  const [showHud, setShowHud] = useState(true);
  const [title, setTitle] = useState("DIY-Thermocam Thermal Inspection");
  const [notes, setNotes] = useState("Steady-state PCB thermal capture at 22.0 °C ambient.");
  const [busyFmt, setBusyFmt] = useState<string | null>(null);

  const runExport = async (fmt: string): Promise<void> => {
    setBusyFmt(fmt);
    try {
      await onExport(fmt, { resolution, showColorbar, showHud, title, notes });
    } finally {
      setBusyFmt(null);
    }
  };

  return (
    <div className="grid h-full grid-cols-12 gap-3 overflow-y-auto p-3">
      {/* Left: Full HD / 1440p / 4K Upscaler & HUD Settings */}
      <div className="tc-card col-span-5 space-y-4 p-4">
        <div>
          <div className="flex items-center justify-between">
            <h2 className="text-[14px] font-semibold">High-Resolution Upscaling &amp; Export</h2>
            <StudioBadge tone="pass">Zero-FFmpeg</StudioBadge>
          </div>
          <p className="mt-1 text-[12px] text-[var(--tc-text-muted)]">
            Separable Lanczos-3 &amp; Edge-Preserving Unsharp Mask resampling with vector-composited
            colorbar, ticks, ROI callouts, and HUD rendered at native Full HD / 1440p / 4K.
          </p>
        </div>

        <div className="space-y-2.5">
          <div>
            <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">
              Output Resolution
            </label>
            <div className="grid grid-cols-2 gap-1.5">
              {(
                [
                  ["full_hd1080p", "Full HD · 1920×1080"],
                  ["qhd1440p", "QHD · 2560×1440"],
                  ["uhd4k", "4K UHD · 3840×2160"],
                  ["hd720p", "HD · 1280×720"],
                  ["native", "4× Native · 640×480"],
                ] as [ExportResolution, string][]
              ).map(([res, lbl]) => (
                <button
                  key={String(res)}
                  type="button"
                  onClick={() => setResolution(res)}
                  className={`tc-btn text-[12px] ${resolution === res ? "tc-btn-active" : ""}`}
                >
                  {lbl}
                </button>
              ))}
            </div>
          </div>

          <div>
            <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">
              Upscaling Kernel
            </label>
            <select
              value={pipeline.upscaler}
              onChange={(e) =>
                onPipelineChange({ ...pipeline, upscaler: e.target.value as UpscaleMethod })
              }
              className="tc-select w-full"
            >
              <option value="edge_preserve">
                Edge-Preserving Thermal Enhance (Bilateral + Lanczos-3 + Unsharp Mask)
              </option>
              <option value="lanczos3">Lanczos-3 Separable High-Accuracy</option>
              <option value="bicubic">Bicubic Catmull-Rom</option>
              <option value="nearest">Nearest Neighbor (Crisp Native Sensor Pixels)</option>
            </select>
          </div>

          <div>
            <div className="mb-1 flex justify-between text-[11.5px]">
              <span className="text-[var(--tc-text-muted)]">Unsharp Mask Detail Boost</span>
              <span className="font-mono-tech">{Math.round(pipeline.sharpen_amount * 100)}%</span>
            </div>
            <input
              type="range"
              min={0}
              max={1.5}
              step={0.05}
              value={pipeline.sharpen_amount}
              onChange={(e) =>
                onPipelineChange({ ...pipeline, sharpen_amount: Number(e.target.value) })
              }
              className="w-full accent-[var(--tc-accent)]"
            />
          </div>

          <div className="flex flex-wrap gap-4 pt-1">
            <StudioSwitch
              checked={showColorbar}
              onCheckedChange={setShowColorbar}
              label="Vector Scale Bar"
            />
            <StudioSwitch checked={showHud} onCheckedChange={setShowHud} label="Radiometry HUD" />
          </div>

          <div className="border-t border-[var(--tc-line)] pt-3">
            <StudioSwitch
              checked={hwButtonTrigger}
              onCheckedChange={onToggleHwButtonTrigger}
              label="Hardware Camera Capture Button triggers Host Snapshot / Record"
            />
          </div>
        </div>
      </div>

      {/* Right: Video Recording & Multi-Format Export Grid */}
      <div className="col-span-7 flex flex-col gap-3">
        {/* Live Video Recorder */}
        <div className="tc-card flex items-center justify-between p-4">
          <div>
            <div className="flex items-center gap-2">
              <h3 className="text-[13.5px] font-semibold">
                Full-HD Radiometric &amp; H.264 Recorder
              </h3>
              {recording ? (
                <StudioBadge tone="danger">REC · {recordedCount} frames</StudioBadge>
              ) : (
                <StudioBadge tone="neutral">{recordedCount} buffered frames</StudioBadge>
              )}
            </div>
            <p className="mt-0.5 text-[12px] text-[var(--tc-text-muted)]">
              Capture lossless raw 16-bit sequences in memory, then encode directly to H.264 MP4
              (built-in OpenH264) or animated GIF with zero external FFmpeg install.
            </p>
          </div>
          <button
            type="button"
            onClick={onToggleRecording}
            className={`tc-btn px-4 ${recording ? "tc-btn-danger" : "tc-btn-primary"}`}
          >
            {recording ? <Square className="h-3.5 w-3.5" /> : <CircleDot className="h-3.5 w-3.5" />}
            {recording ? "Stop Recording" : "Start Recording"}
          </button>
        </div>

        {/* Time-Lapse .DAT Sequence Capture */}
        <div className="tc-card flex items-center justify-between gap-3 p-3.5">
          <div>
            <div className="flex items-center gap-2">
              <h3 className="text-[13px] font-semibold">Time-Lapse Radiometric Capture</h3>
              {timelapseActive ? (
                <StudioBadge tone="amber">ACTIVE · {timelapseCount} frames</StudioBadge>
              ) : (
                <StudioBadge tone="neutral">{timelapseCount} captured</StudioBadge>
              )}
            </div>
            <p className="mt-0.5 text-[11.5px] text-[var(--tc-text-muted)]">
              Capture lossless `.DAT` snapshots on schedule — feeds directly into the Time Plotter
              and Full HD H.264 MP4 exporter.
              {timelapseDir ? ` Saving to ${timelapseDir}` : ""}
            </p>
          </div>
          <div className="flex shrink-0 items-center gap-2">
            <button
              type="button"
              disabled={timelapseActive}
              onClick={() => void onPickTimelapseDir()}
              title={timelapseDir ?? "Choose folder for time-lapse .DAT files"}
              className="tc-btn px-2.5 text-[11.5px]"
            >
              <FolderOpen className="h-3.5 w-3.5" />
              {timelapseDir ? "Folder Set" : "Folder"}
            </button>
            <select
              value={timelapseIntervalSec}
              disabled={timelapseActive}
              onChange={(e) => onTimelapseIntervalChange(Number(e.target.value))}
              className="tc-select w-[92px] font-mono-tech text-[12px]"
              aria-label="Time-lapse interval"
            >
              <option value={1}>Every 1s</option>
              <option value={2}>Every 2s</option>
              <option value={5}>Every 5s</option>
              <option value={10}>Every 10s</option>
              <option value={30}>Every 30s</option>
            </select>
            <button
              type="button"
              onClick={onToggleTimelapse}
              className={`tc-btn px-3 ${timelapseActive ? "tc-btn-danger" : ""}`}
            >
              <Clock className="h-3.5 w-3.5" />
              {timelapseActive ? "Stop" : "Start Time-Lapse"}
            </button>
          </div>
        </div>

        {/* Format Export Grid */}
        <div className="tc-card p-4">
          <h3 className="mb-3 text-[13.5px] font-semibold">Export Formats</h3>
          <div className="grid grid-cols-3 gap-2">
            {(
              [
                ["png", "HD / 4K PNG", "Lossless composited", ImageIcon],
                ["jpg", "HD / 4K JPEG", "94% quality", ImageIcon],
                ["mp4", "H.264 MP4 Video", "Pure Rust OpenH264", Film],
                ["gif", "Animated GIF", "Looping thermal clip", Film],
                ["tiff", "16-bit Kelvin TIFF", "Radiometric cK Gray16", Layers],
                ["csv", "Temp Matrix CSV", "Full 160×120 grid", FileSpreadsheet],
                ["dat", "Lossless .DAT", "100% V1/V2/V3 parity", Layers],
                ["seq", "Sequence Folder", ".DAT + manifest.json", Layers],
                ["html", "HTML Report", "HD PNG + SVG + Table", FileText],
              ] as const
            ).map(([fmt, label, sub, Icon]) => (
              <button
                key={fmt}
                type="button"
                disabled={busyFmt !== null}
                onClick={() => runExport(fmt)}
                className="flex flex-col items-start gap-1 rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)] p-3 text-left transition-colors hover:border-[var(--tc-accent-line)] hover:bg-[var(--tc-accent-soft)] cursor-pointer disabled:opacity-50"
              >
                <div className="flex w-full items-center justify-between">
                  <Icon className="h-4 w-4 text-[var(--tc-accent)]" />
                  <span className="font-mono-tech text-[10px] uppercase text-[var(--tc-text-dim)]">
                    .{fmt}
                  </span>
                </div>
                <div className="text-[12.5px] font-semibold">
                  {busyFmt === fmt ? "Exporting…" : label}
                </div>
                <div className="text-[11px] text-[var(--tc-text-muted)]">{sub}</div>
              </button>
            ))}
          </div>

          <div className="mt-3 grid grid-cols-2 gap-2">
            <input
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              placeholder="Report title"
              className="tc-input"
            />
            <button
              type="button"
              disabled={busyFmt !== null}
              onClick={() => runExport("md")}
              className="tc-btn"
            >
              <FileText className="h-3.5 w-3.5" />
              Export Markdown Report (.md)
            </button>
          </div>
          <textarea
            value={notes}
            onChange={(e) => setNotes(e.target.value)}
            rows={2}
            placeholder="Inspection notes embedded into HTML & Markdown reports"
            className="mt-2 w-full rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface)] p-2 text-[12px]"
          />

          {lastExportMsg ? (
            <div className="mt-2.5 rounded-lg border border-[var(--tc-pass-line)] bg-[var(--tc-pass-soft)] px-3 py-2 font-mono-tech text-[11.5px] text-[var(--tc-pass)]">
              {lastExportMsg}
            </div>
          ) : null}
        </div>
      </div>
    </div>
  );
}
