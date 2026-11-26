import { demoCameraFrame } from "./demoCamera";
import { DEFAULT_PALETTES, parseCustomPaletteText, samplePalette } from "./palettes";
import {
  parseSettings,
  readStoredSettings,
  type StudioSettings,
  writeStoredSettings,
} from "./settings";
import { decodeFrameRawBinary, objectCToRaw } from "./thermal";
import type {
  CameraEvent,
  DisplayPipeline,
  ExportResolution,
  HelloResp,
  Palette,
  RadiometryParams,
  RecordingInfo,
  RecordingStatus,
  RenderSource,
  RenderSpec,
  Roi,
  SerialPortInfo,
  SnapshotInfo,
  TempUnit,
  ThermalFrame,
} from "./types";

export function isTauriRuntime(): boolean {
  return typeof window !== "undefined" && "__TAURI_INTERNALS__" in window;
}

export async function tauriInvoke<T>(cmd: string, args?: Record<string, unknown>): Promise<T> {
  const { invoke } = await import("@tauri-apps/api/core");
  return invoke<T>(cmd, args);
}

/** Listens to a Rust event's payload; resolves to the function that stops listening. */
export async function tauriListen<T>(event: string, cb: (payload: T) => void): Promise<() => void> {
  const { listen } = await import("@tauri-apps/api/event");
  return listen<T>(event, (ev) => cb(ev.payload));
}

/** True when Tauri rejected because the command is not registered (an older Rust build). */
function isMissingCommand(err: unknown, cmd: string): boolean {
  const msg = err instanceof Error ? err.message : String(err);
  return msg.includes(cmd) && /not found|unknown command/i.test(msg);
}

/**
 * Calls a Tauri command, or the browser fallback. The fallback also runs when the running Rust
 * build does not have the command yet, so the frontend can ship ahead of it.
 */
async function invokeOr<T>(
  cmd: string,
  args: Record<string, unknown> | undefined,
  fallback: () => T | Promise<T>,
): Promise<T> {
  if (!isTauriRuntime()) return fallback();
  try {
    return await tauriInvoke<T>(cmd, args);
  } catch (err) {
    if (isMissingCommand(err, cmd)) return fallback();
    throw err;
  }
}

const DEFAULT_CAPTURES_DIR = "~/Pictures/Thermocam";

/** One chart sample in the session log: `t_ms,region_id,value_c`. */
export interface SessionLogSample {
  t_ms: number;
  region_id: string;
  value_c: number;
}

/** The session log in the browser preview, and until the Rust commands exist. */
let memorySessionLog: SessionLogSample[] = [];

let simTimer: ReturnType<typeof setInterval> | null = null;
let simSeq = 1;

// ---------- Browser preview: snapshots, recordings and camera events ----------

/** The browser's stand-in Captures folder. */
const SIM_CAPTURES_DIR = "~/Pictures/Thermocam";

/** A capture the browser preview "saved", listed on Captures › On this computer. */
export interface SimSavedCapture {
  name: string;
  path: string;
  kind: "snapshot" | "recording";
  mtimeMs: number;
  /** The snapshot, or a recording's first frame. */
  frame: ThermalFrame;
  frames: number | null;
  t_ms: number[] | null;
}

const simSaved: SimSavedCapture[] = [];
let simLastFrame: ThermalFrame | null = null;

interface SimRecording {
  path: string;
  startedAt: number;
  intervalMs: number | null;
  lastSavedAt: number | null;
  t_ms: number[];
  first: ThermalFrame | null;
}
let simRecording: SimRecording | null = null;
const recordingStoppedListeners = new Set<(info: RecordingInfo) => void>();
const cameraEventListeners = new Set<(ev: CameraEvent) => void>();
const exportProgressListeners = new Set<
  (ev: { stage: string; done?: number; total?: number }) => void
>();

/** Development switches for the browser preview (`window.__studioSim`). */
const simFlags: {
  readOnlyFolder: boolean;
  updateMode: boolean;
  /** A Thermocam port that is listed but refuses to open with this kind. */
  refusedCamera: "busy" | "permission" | null;
} = { readOnlyFolder: false, updateMode: false, refusedCamera: null };

const SIM_REFUSED_PORT = "/dev/cu.usbmodem-preview";

function pad2(n: number): string {
  return String(n).padStart(2, "0");
}

/** `YYYYMMDDHHMMSS` in local time, the name Rust gives a capture. */
function captureStamp(d: Date): string {
  return `${d.getFullYear()}${pad2(d.getMonth() + 1)}${pad2(d.getDate())}${pad2(d.getHours())}${pad2(d.getMinutes())}${pad2(d.getSeconds())}`;
}

function simUniqueName(base: string, ext: string): string {
  let name = `${base}${ext}`;
  for (let i = 2; simSaved.some((c) => c.name === name); i++) name = `${base}-${i}${ext}`;
  return name;
}

/** Captures saved in the browser preview, newest first. */
export function simSavedCaptures(): readonly SimSavedCapture[] {
  return simSaved;
}

function simReadOnly(): { kind: string; detail: string } {
  return { kind: "read_only", detail: "Permission denied (os error 13)" };
}

function simFinishRecording(stopped: RecordingInfo["stopped"]): RecordingInfo | null {
  const rec = simRecording;
  if (!rec) return null;
  simRecording = null;
  const frames = rec.t_ms.length;
  const info: RecordingInfo = {
    path: rec.path,
    frames,
    duration_ms: frames > 0 ? (rec.t_ms[frames - 1] ?? 0) : 0,
    stopped,
    detail: null,
  };
  if (frames > 0 && rec.first) {
    simSaved.unshift({
      name: rec.path.split("/").pop() ?? rec.path,
      path: rec.path,
      kind: "recording",
      mtimeMs: Date.now(),
      frame: rec.first,
      frames,
      t_ms: rec.intervalMs ? null : rec.t_ms,
    });
  }
  for (const cb of recordingStoppedListeners) cb(info);
  return info;
}

/** Every frame of the browser's demo stream: what a recording or a time-lapse writes. */
function simOnFrame(fr: ThermalFrame): void {
  simLastFrame = fr;
  const rec = simRecording;
  if (!rec) return;
  const now = Date.now();
  if (rec.intervalMs !== null && rec.lastSavedAt !== null && now - rec.lastSavedAt < rec.intervalMs)
    return;
  rec.lastSavedAt = now;
  rec.first ??= fr;
  rec.t_ms.push(now - rec.startedAt);
}

/** The camera went away (simulated unplug): a recording stops and keeps its frames. */
function simCameraLost(): void {
  if (simRecording && simRecording.intervalMs === null) simFinishRecording("disconnected");
}

/** A picture of a frame in the palette, as Rust would render it (browser preview). */
async function simRenderPng(frame: ThermalFrame, spec: RenderSpec): Promise<ArrayBuffer> {
  const palette = DEFAULT_PALETTES.find((p) => p.id === spec.palette_id) ?? DEFAULT_PALETTES[0];
  const scale = Math.max(1, Math.min(8, Math.round(spec.scale ?? 1)));
  let lo = frame.min_raw;
  let hi = frame.max_raw;
  const r = spec.range;
  if (r?.mode === "manual" && r.min_c !== null && r.max_c !== null) {
    const rad: RadiometryParams = {
      cal_offset: frame.cal_offset,
      cal_slope: frame.cal_slope,
      emissivity: spec.material?.emissivity ?? 0.95,
      reflected_temp_c: spec.material?.reflected_c ?? 22,
      sensor_material: spec.sensor_material ?? null,
    };
    lo = objectCToRaw(r.min_c, rad);
    hi = objectCToRaw(r.max_c, rad);
  }
  const cv = document.createElement("canvas");
  cv.width = frame.width * scale;
  cv.height = frame.height * scale;
  const ctx = cv.getContext?.("2d");
  if (!ctx) throw { kind: "other", detail: "Pictures can't be drawn here." };
  const small = document.createElement("canvas");
  small.width = frame.width;
  small.height = frame.height;
  const sctx = small.getContext("2d");
  if (!sctx) throw { kind: "other", detail: "Pictures can't be drawn here." };
  const img = sctx.createImageData(frame.width, frame.height);
  const span = Math.max(1, hi - lo);
  for (let i = 0; i < frame.pixels.length; i++) {
    const t = Math.max(0, Math.min(1, (frame.pixels[i] - lo) / span));
    const [cr, cg, cb] = samplePalette(palette, t);
    img.data[i * 4] = cr;
    img.data[i * 4 + 1] = cg;
    img.data[i * 4 + 2] = cb;
    img.data[i * 4 + 3] = 255;
  }
  sctx.putImageData(img, 0, 0);
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(small, 0, 0, cv.width, cv.height);
  const blob = await new Promise<Blob | null>((resolve) => cv.toBlob(resolve, "image/png"));
  if (!blob) throw { kind: "other", detail: "The picture couldn't be made." };
  return blob.arrayBuffer();
}

function simDownload(bytes: ArrayBuffer, name: string, type: string): void {
  const url = URL.createObjectURL(new Blob([bytes], { type }));
  const a = document.createElement("a");
  a.href = url;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

if (import.meta.env?.DEV && typeof window !== "undefined") {
  (window as unknown as Record<string, unknown>).__studioSim = {
    /** Snapshot and Record refuse as a read-only Captures folder would. */
    readOnlyFolder: (on = true) => {
      simFlags.readOnlyFolder = on;
    },
    /** A camera waits in update mode (HalfKay present, no serial port). */
    updateMode: (on = true) => {
      simFlags.updateMode = on;
    },
    /** A Thermocam appears whose port refuses to open: "busy", "permission", or null to unplug. */
    refusedCamera: (kind: "busy" | "permission" | null = "busy") => {
      simFlags.refusedCamera = kind;
    },
    /** The camera's button: "short" or "long". */
    button: (action: "short" | "long" = "short") => {
      const ev: CameraEvent = { Button: { action: action === "short" ? 0 : 1, duration_ms: 180 } };
      for (const cb of cameraEventListeners) cb(ev);
    },
  };
}

export const backend = {
  /** Studio's settings (`cmd_settings_load`); localStorage in the browser. */
  async loadSettings(): Promise<StudioSettings> {
    const settings = await invokeOr<unknown>("cmd_settings_load", undefined, readStoredSettings);
    const parsed = parseSettings(settings);
    writeStoredSettings(parsed);
    return parsed;
  },

  /** Saves settings (`cmd_settings_save`). A local copy is kept so the next start paints the
   * chosen theme before Rust answers. */
  async saveSettings(settings: StudioSettings): Promise<void> {
    writeStoredSettings(settings);
    await invokeOr<void>("cmd_settings_save", { settings: parseSettings(settings) }, () => {});
  },

  /** The Captures folder (`cmd_captures_dir`), created on first use by Rust. */
  async capturesDir(): Promise<string> {
    return invokeOr<string>(
      "cmd_captures_dir",
      undefined,
      () => readStoredSettings().captures_dir ?? DEFAULT_CAPTURES_DIR,
    );
  },

  /** Sets the Captures folder (`cmd_set_captures_dir`); Rust checks it is writable. */
  async setCapturesDir(path: string): Promise<string> {
    return invokeOr<string>("cmd_set_captures_dir", { path }, () => {
      writeStoredSettings({ ...readStoredSettings(), captures_dir: path });
      return path;
    });
  },

  async listPorts(): Promise<SerialPortInfo[]> {
    if (isTauriRuntime()) {
      const raw =
        await tauriInvoke<
          Array<
            Partial<SerialPortInfo> & {
              name?: string;
              label?: string;
              is_thermocam?: boolean;
            }
          >
        >("cmd_list_ports");
      return raw.map((p) => {
        const portName = p.port_name ?? p.name ?? "sim://demo-lepton35";
        return {
          port_name: portName,
          vid: p.vid ?? null,
          pid: p.pid ?? null,
          manufacturer: p.manufacturer ?? null,
          product: p.product ?? p.label ?? null,
          is_teensy: !portName.startsWith("sim://") && (p.is_teensy ?? p.is_thermocam ?? false),
        };
      });
    }
    const list: SerialPortInfo[] = [
      {
        port_name: "sim://demo-lepton35",
        vid: 0x16c0,
        pid: 0x0483,
        manufacturer: "DIY-Thermocam",
        product: "Simulator (Lepton 3.5)",
        is_teensy: false,
      },
    ];
    if (simFlags.refusedCamera) {
      list.push({
        port_name: SIM_REFUSED_PORT,
        vid: 0x16c0,
        pid: 0x0483,
        manufacturer: "Teensyduino",
        product: "USB Serial",
        is_teensy: true,
      });
    }
    return list;
  },

  async getPalettes(): Promise<Palette[]> {
    if (isTauriRuntime()) return tauriInvoke<Palette[]>("cmd_get_palettes");
    return DEFAULT_PALETTES;
  },

  async importCustomPalette(name: string, data: string, isPngBase64: boolean): Promise<Palette> {
    if (isTauriRuntime()) {
      return tauriInvoke<Palette>("cmd_import_custom_palette", { name, data, isPngBase64 });
    }
    return parseCustomPaletteText(name, data);
  },

  async connect(port: string): Promise<HelloResp> {
    if (isTauriRuntime()) return tauriInvoke<HelloResp>("cmd_connect", { port });
    if (port === SIM_REFUSED_PORT) {
      throw simFlags.refusedCamera === "permission"
        ? { kind: "permission", detail: `Permission denied: ${port}` }
        : { kind: "busy", detail: `Resource busy: ${port}` };
    }
    return {
      proto_major: 2,
      proto_minor: 0,
      fw_version: 400,
      fw_build_str: "4.0.0-sim",
      hw_version: 3,
      lepton_type: 1,
      sensor_w: 160,
      sensor_h: 120,
      display_w: 320,
      display_h: 240,
      diagnostic_mask: 0xff,
      teensy_uid: 0x0011223344556677,
      capabilities: 0x01ff,
      max_chunk_size: 8192,
      heartbeat_timeout_ms: 5000,
    };
  },

  async disconnect(): Promise<void> {
    if (simTimer) {
      clearInterval(simTimer);
      simTimer = null;
    }
    if (isTauriRuntime()) await tauriInvoke<void>("cmd_disconnect");
    else simCameraLost();
  },

  async startStream(onFrame: (frame: ThermalFrame) => void): Promise<void> {
    if (isTauriRuntime()) {
      const { Channel } = await import("@tauri-apps/api/core");
      const ch = new Channel<ArrayBuffer | Uint8Array | number[]>();
      ch.onmessage = (raw) => {
        try {
          if (raw instanceof ArrayBuffer) {
            onFrame(decodeFrameRawBinary(raw));
          } else if (ArrayBuffer.isView(raw)) {
            const view = raw as Uint8Array;
            const copy = new Uint8Array(view.byteLength);
            copy.set(new Uint8Array(view.buffer, view.byteOffset, view.byteLength));
            onFrame(decodeFrameRawBinary(copy.buffer));
          } else if (Array.isArray(raw)) {
            onFrame(decodeFrameRawBinary(Uint8Array.from(raw).buffer));
          }
        } catch (err) {
          console.error("Failed to decode binary FrameRaw packet:", err);
        }
      };
      await tauriInvoke<void>("cmd_start_stream", { channel: ch });
      return;
    }
    if (simTimer) clearInterval(simTimer);
    const tick = () => {
      const fr = demoCameraFrame(simSeq++);
      simOnFrame(fr);
      onFrame(fr);
    };
    tick();
    simTimer = setInterval(tick, 114);
  },

  async stopStream(): Promise<void> {
    if (simTimer) {
      clearInterval(simTimer);
      simTimer = null;
    }
    if (isTauriRuntime()) await tauriInvoke<void>("cmd_stop_stream");
  },

  async triggerFfc(force: boolean): Promise<void> {
    if (isTauriRuntime()) await tauriInvoke<void>("cmd_trigger_ffc", { force });
  },

  /**
   * `cmd_export_media`: Rust asks where to save and returns the path; `kind` (or `format`)
   * picks the file. The browser preview pretends, with progress for videos.
   */
  async exportMedia(
    req: { format?: string; kind?: string } & Partial<{
      palette_id: string;
      min_raw: number;
      max_raw: number;
      rad: RadiometryParams;
      pipeline: DisplayPipeline;
      rois: Roi[];
      resolution: ExportResolution;
      show_colorbar: boolean;
      show_hud: boolean;
      unit: TempUnit;
      title: string;
      notes: string;
      pixels: number[];
      width: number;
      height: number;
      recording: string | null;
    }> &
      Record<string, unknown>,
  ): Promise<string> {
    if (isTauriRuntime()) return tauriInvoke<string>("cmd_export_media", { req });
    const kind = String(req.kind ?? req.format ?? "png");
    if (kind === "mp4" || kind === "gif") {
      const total = 40;
      for (let done = 1; done <= total; done++) {
        await new Promise((r) => setTimeout(r, 35));
        for (const cb of exportProgressListeners) cb({ stage: "encoding", done, total });
      }
    }
    const ext = kind === "series-csv" ? "csv" : kind;
    return `~/Downloads/thermocam_capture.${ext}`;
  },

  async pickDirectory(): Promise<string | null> {
    if (isTauriRuntime()) {
      try {
        return await tauriInvoke<string>("cmd_pick_directory");
      } catch {
        return null;
      }
    }
    return "~/Exports/timelapse";
  },

  /** Starts a new session log for the chart (`cmd_session_log_start`); in memory in the browser. */
  async sessionLogStart(): Promise<void> {
    await invokeOr<unknown>("cmd_session_log_start", undefined, () => {
      memorySessionLog = [];
    });
  },

  /** Adds about a second of chart samples to the session log (`cmd_session_log_append`). */
  async sessionLogAppend(batch: SessionLogSample[]): Promise<void> {
    if (batch.length === 0) return;
    await invokeOr<void>("cmd_session_log_append", { batch }, () => {
      memorySessionLog.push(...batch);
    });
  },

  /**
   * Writes the whole session log as CSV (`cmd_session_log_export`); without `outPath` Rust asks
   * where with a Save dialog. The browser downloads the samples kept in memory. Returns the
   * path, or null when nothing was saved.
   */
  async sessionLogExport(outPath: string | null = null): Promise<string | null> {
    return invokeOr<string | null>("cmd_session_log_export", outPath ? { outPath } : {}, () => {
      const lines = ["t_ms,region_id,value_c"];
      for (const s of memorySessionLog) lines.push(`${s.t_ms},${s.region_id},${s.value_c}`);
      const name = "thermocam-session.csv";
      try {
        const url = URL.createObjectURL(new Blob([`${lines.join("\n")}\n`], { type: "text/csv" }));
        const a = document.createElement("a");
        a.href = url;
        a.download = name;
        a.click();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
      } catch {
        return null;
      }
      return name;
    });
  },

  /**
   * Snapshot (`cmd_snapshot`): the latest frame as `YYYYMMDDHHMMSS.DAT` in the Captures folder,
   * with no dialog; with `png`, also a PNG beside it. Rejects `{kind: "read_only" | "no_space" |
   * "no_frame", detail}`.
   */
  async snapshot(png: RenderSpec | null): Promise<SnapshotInfo> {
    if (isTauriRuntime()) return tauriInvoke<SnapshotInfo>("cmd_snapshot", png ? { png } : {});
    if (simFlags.readOnlyFolder) throw simReadOnly();
    const frame = simLastFrame;
    if (!frame) throw { kind: "no_frame", detail: "No image has arrived from the camera yet." };
    const name = simUniqueName(captureStamp(new Date()), ".DAT");
    const path = `${SIM_CAPTURES_DIR}/${name}`;
    simSaved.unshift({
      name,
      path,
      kind: "snapshot",
      mtimeMs: Date.now(),
      frame,
      frames: null,
      t_ms: null,
    });
    return { path, png_path: png ? path.replace(/\.DAT$/, ".png") : null };
  },

  /**
   * A picture rendered by Rust (`cmd_render_png`): PNG bytes, or `{path}` with `dialog` (Save
   * PNG…). A cancelled dialog rejects with `{kind: "cancelled"}`. The browser draws the frame
   * itself and downloads instead of a Save dialog.
   */
  async renderPng(
    source: RenderSource,
    spec: RenderSpec,
    opts: { dialog?: boolean; outPath?: string } = {},
  ): Promise<ArrayBuffer | { path: string }> {
    if (isTauriRuntime()) {
      return tauriInvoke<ArrayBuffer | { path: string }>("cmd_render_png", {
        req: {
          source,
          ...spec,
          ...(opts.outPath ? { out_path: opts.outPath } : {}),
          ...(opts.dialog ? { dialog: true } : {}),
        },
      });
    }
    const frame =
      source === "latest"
        ? simLastFrame
        : source.kind === "local"
          ? (simSaved.find((c) => c.path === source.path)?.frame ?? simLastFrame)
          : simLastFrame;
    if (!frame) throw { kind: "no_frame", detail: "No image has arrived from the camera yet." };
    const bytes = await simRenderPng(frame, spec);
    if (!opts.dialog && !opts.outPath) return bytes;
    const base =
      source !== "latest" && source.kind === "local"
        ? (source.path.split("/").pop() ?? "thermocam").replace(/\.dat$/i, "")
        : "thermocam";
    const name = `${base}.png`;
    simDownload(bytes, name, "image/png");
    return { path: `~/Downloads/${name}` };
  },

  /**
   * Record (`cmd_record_start`): frames go to a new `YYYYMMDDHHMMSS/` folder as they arrive;
   * with `intervalMs` it is a time-lapse. Rejects `low_disk`, `already_recording`,
   * `not_connected`, `read_only`.
   */
  async recordStart(opts: {
    intervalMs?: number;
    sensorMaterial?: RadiometryParams["sensor_material"];
  }): Promise<{ path: string }> {
    if (isTauriRuntime()) {
      return tauriInvoke<{ path: string }>("cmd_record_start", {
        intervalMs: opts.intervalMs ?? null,
        sensorMaterial: opts.sensorMaterial ?? null,
      });
    }
    if (simFlags.readOnlyFolder) throw simReadOnly();
    if (simRecording) throw { kind: "already_recording", detail: "A recording is running." };
    if (!simTimer && !simLastFrame) throw { kind: "not_connected", detail: "No camera." };
    const path = `${SIM_CAPTURES_DIR}/${simUniqueName(captureStamp(new Date()), "")}`;
    simRecording = {
      path,
      startedAt: Date.now(),
      intervalMs: opts.intervalMs ?? null,
      lastSavedAt: null,
      t_ms: [],
      first: null,
    };
    return { path };
  },

  /** Stops the recording or time-lapse (`cmd_record_stop`); `recording-stopped` follows. */
  async recordStop(): Promise<RecordingInfo> {
    if (isTauriRuntime()) return tauriInvoke<RecordingInfo>("cmd_record_stop");
    const info = simFinishRecording("user");
    if (!info) throw { kind: "not_recording", detail: "No recording is running." };
    return info;
  },

  /** The running recording or time-lapse (`cmd_record_status`), or null. */
  async recordStatus(): Promise<RecordingStatus | null> {
    if (isTauriRuntime()) return tauriInvoke<RecordingStatus | null>("cmd_record_status");
    const rec = simRecording;
    if (!rec) return null;
    return {
      path: rec.path,
      frames: rec.t_ms.length,
      elapsed_ms: Date.now() - rec.startedAt,
      source: rec.intervalMs === null ? "studio" : "timelapse",
    };
  },

  /** `recording-stopped`: every stop, by the user, an unplug, a write error or low disk. */
  async onRecordingStopped(cb: (info: RecordingInfo) => void): Promise<() => void> {
    if (!isTauriRuntime()) {
      recordingStoppedListeners.add(cb);
      return () => recordingStoppedListeners.delete(cb);
    }
    return tauriListen<RecordingInfo>("recording-stopped", cb);
  },

  /** `camera-event`: the camera's button, battery and the rest, as Rust decodes them. */
  async onCameraEvent(cb: (ev: CameraEvent) => void): Promise<() => void> {
    if (!isTauriRuntime()) {
      cameraEventListeners.add(cb);
      return () => cameraEventListeners.delete(cb);
    }
    return tauriListen<CameraEvent>("camera-event", cb);
  },

  /**
   * A Thermocam waiting in update mode (`cmd_update_mode_present`): the HalfKay bootloader is
   * on USB. Rust only lists HID devices; it never opens one.
   */
  async updateModePresent(): Promise<boolean> {
    return invokeOr<boolean>("cmd_update_mode_present", undefined, () => simFlags.updateMode);
  },

  async onCameraDisconnected(cb: (reason: string) => void): Promise<() => void> {
    if (!isTauriRuntime()) return () => {};
    return tauriListen<{ reason?: string } | null>("camera-disconnected", (p) => {
      cb(p?.reason ?? "USB camera disconnected");
    });
  },

  async onExportProgress(
    cb: (ev: { stage: string; done?: number; total?: number }) => void,
  ): Promise<() => void> {
    if (!isTauriRuntime()) {
      exportProgressListeners.add(cb);
      return () => exportProgressListeners.delete(cb);
    }
    return tauriListen<{ stage: string; done?: number; total?: number }>("export-progress", cb);
  },
};
