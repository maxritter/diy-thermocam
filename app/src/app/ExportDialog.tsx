import { Dialog } from "@base-ui/react/dialog";
import {
  Binary,
  CircleDot,
  Clock,
  Copy,
  File,
  FileImage,
  FileVideo,
  Film,
  Image,
  type LucideIcon,
  Ruler,
  Scaling,
  Table,
  Tag,
  Upload,
  X,
} from "lucide-react";
import { useEffect, useRef, useState } from "react";
import { type ColorRange, shownRange } from "@/lib/colorRange";
import { paletteGradientCss, samplePalette } from "@/lib/palettes";
import type {
  DisplayPipeline,
  ExportResolution,
  Palette,
  RadiometryParams,
  Roi,
  TempUnit,
  ThermalFrame,
} from "@/lib/types";
import { baseName, commandError } from "@/lib/utils";
import { type Platform, revealLabel, type ToastStore } from "./Toasts";
import { Segmented } from "./ui/Segmented";
import { SettingRow } from "./ui/SettingRow";
import { Switch } from "./ui/Switch";

/**
 * One dialog for every file Studio makes (`live.html#export`): Image, Video, Data and Report,
 * each with only its own choices. Export… opens the system's Save dialog (Rust).
 */

export type ExportTab = "image" | "video" | "data" | "report";
type FixedResolution = Exclude<ExportResolution, { custom: unknown }>;

export const RESOLUTION_SIZES: Record<FixedResolution, [number, number]> = {
  native: [640, 480],
  hd720p: [1280, 720],
  full_hd1080p: [1920, 1080],
  qhd1440p: [2560, 1440],
  uhd4k: [3840, 2160],
};

const IMAGE_SIZES: { id: FixedResolution; label: string; title?: string }[] = [
  { id: "native", label: "Sensor ×4", title: "640 × 480, four screen pixels per sensor pixel" },
  { id: "hd720p", label: "720p" },
  { id: "full_hd1080p", label: "1080p" },
  { id: "qhd1440p", label: "1440p" },
  { id: "uhd4k", label: "4K" },
];
const VIDEO_SIZES = IMAGE_SIZES.slice(1);

type DataKind = "csv" | "tiff" | "dat" | "frames";

const DATA_CHOICES: { id: DataKind; title: string; detail: string; icon: LucideIcon }[] = [
  {
    id: "csv",
    title: "Temperatures as CSV",
    detail: "One value per pixel, for Excel, Numbers or Python",
    icon: Table,
  },
  {
    id: "tiff",
    title: "Thermal TIFF, 16-bit",
    detail: "Exact temperatures for tools like ImageJ",
    icon: FileImage,
  },
  {
    id: "dat",
    title: "Raw capture (.DAT)",
    detail: "Opens again in Studio and ThermoVision",
    icon: Binary,
  },
  {
    id: "frames",
    title: "Recording as raw frames",
    detail: "The last recording as .DAT files with their times",
    icon: Film,
  },
];

export interface ExportChoices {
  tab: ExportTab;
  imageFormat: "png" | "jpg";
  imageSize: FixedResolution;
  colorbar: boolean;
  labels: boolean;
  videoFrom: "video" | "timelapse";
  videoFormat: "mp4" | "gif";
  videoSize: FixedResolution;
  data: DataKind;
  title: string;
  notes: string;
  reportFormat: "html" | "md";
}

const DEFAULT_CHOICES: ExportChoices = {
  tab: "image",
  imageFormat: "png",
  imageSize: "full_hd1080p",
  colorbar: true,
  labels: true,
  videoFrom: "video",
  videoFormat: "mp4",
  videoSize: "full_hd1080p",
  data: "csv",
  title: "",
  notes: "",
  reportFormat: "html",
};

const CHOICES_KEY = "thermocam.export";

/** The dialog remembers its last choices (this browser's storage; notes are not kept). */
function readChoices(): ExportChoices {
  try {
    const raw = JSON.parse(localStorage.getItem(CHOICES_KEY) ?? "null");
    if (raw && typeof raw === "object") return { ...DEFAULT_CHOICES, ...raw, notes: "" };
  } catch {
    // Storage unavailable: the defaults.
  }
  return { ...DEFAULT_CHOICES };
}

function writeChoices(c: ExportChoices): void {
  try {
    const { notes: _notes, ...kept } = c;
    localStorage.setItem(CHOICES_KEY, JSON.stringify(kept));
  } catch {
    // Storage unavailable: choices last for this session.
  }
}

/** For tests: forget the remembered choices. */
export function resetExportChoices(): void {
  try {
    localStorage.removeItem(CHOICES_KEY);
  } catch {
    // Nothing to forget.
  }
}

export interface ExportProgress {
  stage: string;
  done?: number;
  total?: number;
}

export interface ExportApi {
  /** `cmd_export_media`; resolves with the written path, rejects `{kind, detail}`. */
  exportMedia(req: Record<string, unknown>): Promise<string>;
  /** `export-progress` events while a video is written. */
  onProgress(cb: (p: ExportProgress) => void): Promise<() => void>;
  reveal(path: string): Promise<void>;
}

export interface ExportSource {
  frame: ThermalFrame | null;
  palette: Palette;
  range: ColorRange;
  rad: RadiometryParams;
  pipeline: DisplayPipeline;
  rois: Roi[];
  unit: TempUnit;
}

export interface RecordingRef {
  path: string;
  frames: number;
  duration_ms: number;
}

export interface ExportDialogProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  api: ExportApi;
  toasts: ToastStore;
  /** What Live shows now, read when Export… is pressed. */
  source: () => ExportSource;
  recordings: { video: RecordingRef | null; timelapse: RecordingRef | null };
  /** Opens on this tab (Export video… opens Video). */
  initialTab?: ExportTab;
  /** With Export video…: the recording or the time-lapse just saved. */
  initialVideoFrom?: "video" | "timelapse";
  onCopyImage: () => void;
  platform?: Platform;
}

function durationShort(ms: number): string {
  const s = Math.round(ms / 1000);
  return s < 60 ? `${s} s` : `${Math.floor(s / 60)} min ${String(s % 60).padStart(2, "0")} s`;
}

/** What `cmd_export_media` receives for the choices (`kind` is Rust's alias of `format`). */
export function exportRequest(
  c: ExportChoices,
  src: ExportSource,
  recordings: ExportDialogProps["recordings"],
): { req: Record<string, unknown>; label: string } {
  const fr = src.frame;
  const [minRaw, maxRaw] = fr ? shownRange(src.range, fr) : [0, 65535];
  const { sensor_material: _sm, ...rad } = src.rad;
  const base = {
    output_path: null,
    palette_id: src.palette.id,
    min_raw: Math.max(0, Math.min(65535, Math.round(minRaw))),
    max_raw: Math.max(0, Math.min(65535, Math.round(maxRaw))),
    rad,
    pipeline: src.pipeline,
    unit: src.unit,
  };
  const pixels = fr
    ? { pixels: Array.from(fr.pixels), width: fr.width, height: fr.height }
    : ({} as Record<string, unknown>);
  if (c.tab === "video") {
    const rec = c.videoFrom === "video" ? recordings.video : recordings.timelapse;
    return {
      label: c.videoFormat.toUpperCase(),
      req: {
        ...base,
        kind: c.videoFormat,
        resolution: c.videoSize,
        rois: src.rois,
        show_colorbar: true,
        show_hud: true,
        recording: rec?.path ?? null,
      },
    };
  }
  if (c.tab === "data") {
    if (c.data === "frames") {
      return {
        label: ".DAT frames",
        req: {
          ...base,
          kind: "dat",
          rois: [],
          recording: recordings.video?.path ?? null,
        },
      };
    }
    return {
      label: c.data === "dat" ? ".DAT" : c.data.toUpperCase(),
      req: {
        ...base,
        kind: c.data,
        rois: [],
        ...(c.data === "dat" ? {} : pixels),
      },
    };
  }
  if (c.tab === "report") {
    return {
      label: "the report",
      req: {
        ...base,
        kind: c.reportFormat,
        resolution: "full_hd1080p",
        rois: src.rois,
        show_colorbar: true,
        show_hud: true,
        title: c.title.trim() || "Thermal inspection",
        notes: c.notes,
        ...pixels,
      },
    };
  }
  return {
    label: c.imageFormat === "png" ? "PNG" : "JPEG",
    req: {
      ...base,
      kind: c.imageFormat,
      resolution: c.imageSize,
      rois: c.labels ? src.rois : [],
      show_colorbar: c.colorbar,
      show_hud: c.labels,
      ...pixels,
    },
  };
}

/** The frame in the chosen palette and range, small. */
function Preview({ source }: { source: () => ExportSource }): React.JSX.Element {
  const ref = useRef<HTMLCanvasElement | null>(null);
  const [palette, setPalette] = useState<Palette | null>(null);
  useEffect(() => {
    const src = source();
    setPalette(src.palette);
    const cv = ref.current;
    const fr = src.frame;
    if (!cv || !fr) return;
    const ctx = cv.getContext?.("2d");
    if (!ctx) return;
    cv.width = fr.width;
    cv.height = fr.height;
    const img = ctx.createImageData(fr.width, fr.height);
    const [lo, hi] = shownRange(src.range, fr);
    const span = Math.max(1, hi - lo);
    for (let i = 0; i < fr.pixels.length; i++) {
      const t = Math.max(0, Math.min(1, (fr.pixels[i] - lo) / span));
      const [r, g, b] = samplePalette(src.palette, t);
      img.data[i * 4] = r;
      img.data[i * 4 + 1] = g;
      img.data[i * 4 + 2] = b;
      img.data[i * 4 + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
  }, [source]);
  return (
    <div className="lv-preview">
      <div className="pv" aria-hidden="true">
        <div className="im">
          <canvas ref={ref} width={160} height={120} />
        </div>
        <div
          className="bar2"
          style={{ background: palette ? paletteGradientCss(palette) : undefined }}
        />
      </div>
      <span className="cap2">Preview</span>
    </div>
  );
}

const TABS: { id: ExportTab; label: string; icon: typeof Image }[] = [
  { id: "image", label: "Image", icon: Image },
  { id: "video", label: "Video", icon: Film },
  { id: "data", label: "Data", icon: Table },
  { id: "report", label: "Report", icon: File },
];

const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

export function ExportDialog({
  open,
  onOpenChange,
  api,
  toasts,
  source,
  recordings,
  initialTab,
  initialVideoFrom,
  onCopyImage,
  platform,
}: ExportDialogProps): React.JSX.Element {
  const [c, setC] = useState<ExportChoices>(readChoices);
  const [busy, setBusy] = useState<{ label: string; pct: number | null } | null>(null);

  useEffect(() => {
    if (!open) return;
    const stored = readChoices();
    setC({
      ...stored,
      ...(initialTab ? { tab: initialTab } : {}),
      ...(initialTab === "video" && initialVideoFrom ? { videoFrom: initialVideoFrom } : {}),
    });
  }, [open, initialTab, initialVideoFrom]);

  const update = (patch: Partial<ExportChoices>) => {
    setC((prev) => {
      const next = { ...prev, ...patch };
      writeChoices(next);
      return next;
    });
  };

  const videoFrom: ExportChoices["videoFrom"] =
    c.videoFrom === "video" && !recordings.video && recordings.timelapse
      ? "timelapse"
      : c.videoFrom === "timelapse" && !recordings.timelapse && recordings.video
        ? "video"
        : c.videoFrom;
  const choices = { ...c, videoFrom };

  const nothingToExport =
    (c.tab === "video" && !recordings.video && !recordings.timelapse) ||
    (c.tab === "data" && c.data === "frames" && !recordings.video) ||
    (c.tab !== "video" && !(c.tab === "data" && c.data === "frames") && !source().frame);

  const run = async () => {
    const { req, label } = exportRequest(choices, source(), recordings);
    setBusy({ label, pct: null });
    let off: (() => void) | null = null;
    try {
      off = await api.onProgress((p) => {
        if (p.total && p.total > 0 && p.done !== undefined) {
          setBusy({ label, pct: Math.round((p.done / p.total) * 100) });
        }
      });
      const path = await api.exportMedia(req);
      toasts.show({
        tone: "ok",
        text: `Exported ${baseName(path)}`,
        actions: [
          { label: revealLabel(platform), onClick: () => void api.reveal(path).catch(() => {}) },
        ],
      });
      onOpenChange(false);
    } catch (err) {
      const e = commandError(err);
      if (e.kind !== "cancelled") {
        toasts.show({ tone: "error", text: `Couldn’t export ${label}: ${e.detail}` });
      }
    } finally {
      off?.();
      setBusy(null);
    }
  };

  const [w, h] = RESOLUTION_SIZES[c.imageSize];

  return (
    <Dialog.Root open={open} onOpenChange={(o) => !busy && onOpenChange(o)}>
      <Dialog.Portal>
        <Dialog.Backdrop className="st-scrim z-50" />
        <Dialog.Popup className="st-dialog lv-export z-50" style={{ width: 640 }}>
          <div className="dlg-h">
            <div className="dlg-t">
              <span className="dlg-ic">
                <Upload size={16} strokeWidth={1.75} aria-hidden="true" />
              </span>
              <Dialog.Title>Export</Dialog.Title>
            </div>
            <Dialog.Close className="st-icon-btn" aria-label="Close" disabled={Boolean(busy)}>
              <X size={16} strokeWidth={1.75} aria-hidden="true" />
            </Dialog.Close>
          </div>
          <div className="dlg-b">
            <div className="st-tabs" role="tablist" aria-label="What to export">
              {TABS.map((t) => {
                const Icon = t.icon;
                return (
                  <button
                    key={t.id}
                    className="st-tab"
                    type="button"
                    role="tab"
                    aria-selected={c.tab === t.id}
                    tabIndex={c.tab === t.id ? 0 : -1}
                    onClick={() => update({ tab: t.id })}
                  >
                    <Icon {...SM} />
                    {t.label}
                  </button>
                );
              })}
            </div>

            {c.tab === "image" ? (
              <div role="tabpanel" aria-label="Image" className="lv-export-image">
                <Preview source={source} />
                <div className="lv-export-col">
                  <SettingRow
                    icon={FileImage}
                    title="Format"
                    control={
                      <Segmented
                        label="Format"
                        value={c.imageFormat}
                        onChange={(v) => update({ imageFormat: v })}
                        options={[
                          { id: "png", label: "PNG" },
                          { id: "jpg", label: "JPEG" },
                        ]}
                      />
                    }
                  />
                  <SettingRow
                    icon={Scaling}
                    title="Size"
                    desc={`${w} × ${h} pixels`}
                    stacked
                    control={
                      <Segmented
                        label="Size"
                        value={c.imageSize}
                        onChange={(v) => update({ imageSize: v })}
                        options={IMAGE_SIZES}
                      />
                    }
                  />
                  <SettingRow
                    icon={Ruler}
                    title="Temperature scale"
                    control={
                      <Switch
                        label="Temperature scale"
                        checked={c.colorbar}
                        onChange={(v) => update({ colorbar: v })}
                      />
                    }
                  />
                  <SettingRow
                    icon={Tag}
                    title="Measurements and labels"
                    control={
                      <Switch
                        label="Measurements and labels"
                        checked={c.labels}
                        onChange={(v) => update({ labels: v })}
                      />
                    }
                  />
                </div>
              </div>
            ) : c.tab === "video" ? (
              <div role="tabpanel" aria-label="Video" className="lv-export-col">
                <SettingRow
                  icon={Film}
                  title="Source"
                  desc={
                    !recordings.video && !recordings.timelapse
                      ? "Record a video or a time-lapse first, with Record."
                      : undefined
                  }
                  stacked
                  control={
                    <Segmented
                      label="Source"
                      value={videoFrom}
                      onChange={(v) => update({ videoFrom: v })}
                      options={[
                        {
                          id: "video",
                          label: recordings.video
                            ? `Last recording · ${durationShort(recordings.video.duration_ms)}`
                            : "Last recording",
                          icon: CircleDot,
                          disabled: !recordings.video,
                        },
                        {
                          id: "timelapse",
                          label: recordings.timelapse
                            ? `Time-lapse · ${recordings.timelapse.frames} frames`
                            : "Time-lapse",
                          icon: Clock,
                          disabled: !recordings.timelapse,
                        },
                      ]}
                    />
                  }
                />
                <SettingRow
                  icon={FileVideo}
                  title="Format"
                  control={
                    <Segmented
                      label="Format"
                      value={c.videoFormat}
                      onChange={(v) => update({ videoFormat: v })}
                      options={[
                        { id: "mp4", label: "MP4" },
                        { id: "gif", label: "GIF" },
                      ]}
                    />
                  }
                />
                <SettingRow
                  icon={Scaling}
                  title="Size"
                  control={
                    <Segmented
                      label="Size"
                      value={c.videoSize === "native" ? "full_hd1080p" : c.videoSize}
                      onChange={(v) => update({ videoSize: v })}
                      options={VIDEO_SIZES}
                    />
                  }
                />
              </div>
            ) : c.tab === "data" ? (
              <div role="tabpanel" aria-label="Data">
                <div className="lv-radio-list">
                  {DATA_CHOICES.map((d) => {
                    const Icon = d.icon;
                    return (
                      <label key={d.id} className="lv-radio">
                        <input
                          type="radio"
                          name="export-data"
                          checked={c.data === d.id}
                          disabled={d.id === "frames" && !recordings.video}
                          onChange={() => update({ data: d.id })}
                        />
                        <span className="lv-radio-t">
                          <Icon {...SM} className="lv-radio-ic" />
                          {d.title}
                        </span>
                        <span className="lv-radio-d">{d.detail}</span>
                      </label>
                    );
                  })}
                </div>
              </div>
            ) : (
              <div role="tabpanel" aria-label="Report" className="lv-export-col">
                <label className="st-field">
                  <span>Title</span>
                  <input
                    className="st-input"
                    value={c.title}
                    placeholder="Thermal inspection"
                    onChange={(e) => update({ title: e.target.value })}
                  />
                </label>
                <label className="st-field">
                  <span>Notes</span>
                  <textarea
                    className="st-input"
                    rows={3}
                    style={{ height: "auto", padding: "8px 9px", resize: "vertical" }}
                    value={c.notes}
                    onChange={(e) => update({ notes: e.target.value })}
                  />
                </label>
                <div className="st-field">
                  <span>Format</span>
                  <Segmented
                    label="Format"
                    value={c.reportFormat}
                    onChange={(v) => update({ reportFormat: v })}
                    options={[
                      { id: "html", label: "Web page (HTML)" },
                      { id: "md", label: "Markdown" },
                    ]}
                  />
                  <span className="st-hint">
                    Includes the image, the scale and every region’s readings.
                  </span>
                </div>
              </div>
            )}
          </div>
          <div className="dlg-f">
            {busy ? (
              <span className="lv-export-progress" role="status">
                {busy.pct === null
                  ? `Exporting ${busy.label}…`
                  : `Exporting ${busy.label} · ${busy.pct}%`}
                {busy.pct !== null ? (
                  <span className="tt-bar">
                    <i style={{ width: `${busy.pct}%` }} />
                  </span>
                ) : null}
              </span>
            ) : (
              <button
                className="tc-btn tc-btn-ghost"
                type="button"
                title="⌘C anywhere in Live"
                onClick={onCopyImage}
              >
                <Copy size={16} strokeWidth={1.75} aria-hidden="true" />
                Copy image
              </button>
            )}
            <div className="st-row-actions">
              <button
                className="tc-btn tc-btn-ghost"
                type="button"
                disabled={Boolean(busy)}
                onClick={() => onOpenChange(false)}
              >
                Cancel
              </button>
              <button
                className="tc-btn tc-btn-primary"
                type="button"
                disabled={Boolean(busy) || nothingToExport}
                onClick={() => void run()}
              >
                Export…
              </button>
            </div>
          </div>
        </Dialog.Popup>
      </Dialog.Portal>
    </Dialog.Root>
  );
}
