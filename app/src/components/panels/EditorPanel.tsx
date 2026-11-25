import { Columns2, FileDown, FileText, FolderOpen, Save } from "lucide-react";
import { useState } from "react";
import { StudioBadge, StudioSwitch } from "@/components/ui/Primitives";
import type { ExportResolution, ThermalFrame } from "@/lib/types";

interface EditorPanelProps {
  sourceLabel: string;
  frame: ThermalFrame;
  compareEnabled: boolean;
  compareSplit: number;
  onToggleCompare: (enabled: boolean) => void;
  onCompareSplitChange: (split: number) => void;
  onLoadLocalDat: () => Promise<void>;
  onSaveSidecar: (title: string, notes: string) => Promise<void>;
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
  statusMsg: string | null;
}

export function EditorPanel({
  sourceLabel,
  frame,
  compareEnabled,
  compareSplit,
  onToggleCompare,
  onCompareSplitChange,
  onLoadLocalDat,
  onSaveSidecar,
  onExport,
  statusMsg,
}: EditorPanelProps): React.JSX.Element {
  const [title, setTitle] = useState("Thermal Board Rev C — Load Audit");
  const [notes, setNotes] = useState(
    "MCU hotspot steady at 64.2 °C under 450 MHz load; heatsink delta +42 °C. Verified emissivity ε=0.95.",
  );
  const [res, setRes] = useState<ExportResolution>("uhd4k");

  return (
    <div className="flex h-full flex-col gap-3 overflow-y-auto p-3">
      <div className="tc-card p-3.5 space-y-3">
        <div className="flex items-center justify-between">
          <div>
            <div className="flex items-center gap-2">
              <h2 className="text-[14px] font-semibold">Radiometric .DAT Editor &amp; Sidecar</h2>
              <StudioBadge tone="accent">Non-Destructive</StudioBadge>
            </div>
            <p className="mt-0.5 font-mono-tech text-[11.5px] text-[var(--tc-text-muted)]">
              Active file: {sourceLabel} ({frame.width}×{frame.height} · 16-bit radiometric)
            </p>
          </div>
          <div className="flex items-center gap-2">
            <button type="button" onClick={onLoadLocalDat} className="tc-btn">
              <FolderOpen className="h-3.5 w-3.5" />
              Open .DAT File
            </button>
            <button
              type="button"
              onClick={() => onSaveSidecar(title, notes)}
              className="tc-btn tc-btn-primary"
            >
              <Save className="h-3.5 w-3.5" />
              Save .thermo.json Sidecar
            </button>
          </div>
        </div>

        {/* Side-by-side Split Compare */}
        <div className="flex items-center justify-between gap-4 rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)] px-3 py-2.5">
          <div className="flex items-center gap-2.5">
            <Columns2 className="h-4 w-4 text-[var(--tc-accent)]" />
            <StudioSwitch
              checked={compareEnabled}
              onCheckedChange={onToggleCompare}
              label="Side-by-Side Split Compare (Active vs Baseline Reference)"
            />
          </div>
          {compareEnabled ? (
            <div className="flex items-center gap-2">
              <span className="font-mono-tech text-[11px] text-[var(--tc-text-muted)]">
                Split {Math.round(compareSplit * 100)}%
              </span>
              <input
                type="range"
                min={0.1}
                max={0.9}
                step={0.01}
                value={compareSplit}
                onChange={(e) => onCompareSplitChange(Number(e.target.value))}
                className="w-36 accent-[var(--tc-accent)]"
              />
            </div>
          ) : null}
        </div>

        {/* Annotations & One-Click Inspection Report */}
        <div className="grid grid-cols-2 gap-3">
          <div className="space-y-2">
            <label className="block text-[11.5px] text-[var(--tc-text-muted)]">
              Inspection Report Title
            </label>
            <input
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              className="tc-input w-full"
            />
            <label className="block text-[11.5px] text-[var(--tc-text-muted)]">
              Engineering Notes &amp; Findings
            </label>
            <textarea
              value={notes}
              onChange={(e) => setNotes(e.target.value)}
              rows={3}
              className="w-full rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface)] p-2 text-[12px]"
            />
          </div>

          <div className="flex flex-col justify-between rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)] p-3">
            <div>
              <span className="tc-section-label">Batch HD / 4K &amp; Report Export</span>
              <div className="mt-2 flex gap-1.5">
                {(
                  [
                    ["full_hd1080p", "1080p"],
                    ["qhd1440p", "1440p"],
                    ["uhd4k", "4K UHD"],
                  ] as [ExportResolution, string][]
                ).map(([r, lbl]) => (
                  <button
                    key={String(r)}
                    type="button"
                    onClick={() => setRes(r)}
                    className={`tc-btn flex-1 text-[12px] ${res === r ? "tc-btn-active" : ""}`}
                  >
                    {lbl}
                  </button>
                ))}
              </div>
            </div>

            <div className="mt-3 grid grid-cols-2 gap-2">
              <button
                type="button"
                onClick={() =>
                  onExport("png", {
                    resolution: res,
                    showColorbar: true,
                    showHud: true,
                    title,
                    notes,
                  })
                }
                className="tc-btn tc-btn-primary"
              >
                <FileDown className="h-3.5 w-3.5" />
                Export 4K PNG
              </button>
              <button
                type="button"
                onClick={() =>
                  onExport("html", {
                    resolution: "full_hd1080p",
                    showColorbar: true,
                    showHud: true,
                    title,
                    notes,
                  })
                }
                className="tc-btn"
              >
                <FileText className="h-3.5 w-3.5" />
                HTML Report
              </button>
              <button
                type="button"
                onClick={() =>
                  onExport("tiff", {
                    resolution: res,
                    showColorbar: false,
                    showHud: false,
                    title,
                    notes,
                  })
                }
                className="tc-btn"
              >
                16-bit Kelvin TIFF
              </button>
              <button
                type="button"
                onClick={() =>
                  onExport("md", {
                    resolution: res,
                    showColorbar: true,
                    showHud: true,
                    title,
                    notes,
                  })
                }
                className="tc-btn"
              >
                Markdown Report
              </button>
            </div>
          </div>
        </div>

        {statusMsg ? (
          <div className="rounded-lg border border-[var(--tc-pass-line)] bg-[var(--tc-pass-soft)] px-3 py-2 font-mono-tech text-[11.5px] text-[var(--tc-pass)]">
            {statusMsg}
          </div>
        ) : null}
      </div>
    </div>
  );
}
