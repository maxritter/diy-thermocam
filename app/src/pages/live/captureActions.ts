import { type Platform, revealLabel, type ToastStore } from "@/app/Toasts";
import type {
  Material,
  RecordingInfo,
  RecordingStatus,
  RenderSource,
  RenderSpec,
  SnapshotInfo,
} from "@/lib/types";

/**
 * Snapshot, Copy image, Save PNG… and Record, with their messages (`live.html#states`).
 * Everything that talks to Rust comes in through `CaptureApi`, so tests can stand in for it.
 */

export interface CaptureApi {
  snapshot(png: RenderSpec | null): Promise<SnapshotInfo>;
  renderPng(
    source: RenderSource,
    spec: RenderSpec,
    opts: { dialog?: boolean },
  ): Promise<ArrayBuffer | { path: string }>;
  reveal(path: string): Promise<void>;
  writeClipboardPng(bytes: ArrayBuffer): Promise<void>;
  recordStart(opts: { intervalMs?: number; sensorMaterial: Material | null }): Promise<{
    path: string;
  }>;
  recordStop(): Promise<RecordingInfo>;
  recordStatus(): Promise<RecordingStatus | null>;
}

export interface CaptureDeps {
  api: CaptureApi;
  toasts: ToastStore;
  /** How the image is drawn now: palette, range, material, regions. */
  spec: () => RenderSpec;
  pngOnSnapshot: () => boolean;
  /** Choose another Captures folder. */
  chooseFolder: () => void;
  /** Export video… in a recording's message. */
  exportVideo: (info: RecordingInfo) => void;
  /** What the camera's sensor holds now (null = factory), stored with a recording. */
  sensorMaterial: () => Material | null;
  platform?: Platform;
}

export type RecorderState =
  | { mode: "idle" }
  | { mode: "starting"; timelapse: boolean }
  | {
      mode: "recording" | "timelapse";
      path: string;
      /** Local clock when it started, for the bar's clock between status polls. */
      startedAt: number;
      frames: number;
      elapsedMs: number;
      intervalMs: number | null;
    };

export interface Recorder {
  get(): RecorderState;
  subscribe(cb: () => void): () => void;
  startVideo(): Promise<void>;
  startTimelapse(intervalMs: number): Promise<void>;
  stop(): Promise<void>;
  /** R: start a recording, or stop whatever runs. */
  toggle(): Promise<void>;
  /** Reads frames and time from Rust (about once a second while active). */
  poll(): Promise<void>;
  /** `recording-stopped` from Rust. */
  stopped(info: RecordingInfo): void;
  /** Rust says one runs (after a reload of the page). */
  adopt(status: RecordingStatus): void;
}

export interface CaptureActions {
  snapshot(): Promise<void>;
  copyImage(source?: RenderSource): Promise<void>;
  savePng(source?: RenderSource): Promise<void>;
  recorder: Recorder;
}

interface ErrorLike {
  kind: string;
  detail: string;
}

export function toError(err: unknown): ErrorLike {
  if (err && typeof err === "object" && "detail" in err) {
    const e = err as { kind?: unknown; detail?: unknown };
    return { kind: typeof e.kind === "string" ? e.kind : "other", detail: String(e.detail) };
  }
  if (err instanceof Error) return { kind: "other", detail: err.message };
  return { kind: "other", detail: String(err) };
}

export function baseName(path: string): string {
  const parts = path.split(/[\\/]+/).filter(Boolean);
  return parts[parts.length - 1] ?? path;
}

/** "36 s", "3 min 12 s", "1 h 04 min". */
export function durationLabel(ms: number): string {
  const s = Math.max(0, Math.round(ms / 1000));
  if (s < 60) return `${s} s`;
  const m = Math.floor(s / 60);
  if (m < 60) return `${m} min ${String(s % 60).padStart(2, "0")} s`;
  return `${Math.floor(m / 60)} h ${String(m % 60).padStart(2, "0")} min`;
}

/** The recording bar's clock: "00:36", "1:02:05". */
export function clockLabel(ms: number): string {
  const s = Math.max(0, Math.floor(ms / 1000));
  const hh = Math.floor(s / 3600);
  const mm = String(Math.floor((s % 3600) / 60)).padStart(2, "0");
  const ss = String(s % 60).padStart(2, "0");
  return hh > 0 ? `${hh}:${mm}:${ss}` : `${mm}:${ss}`;
}

/** "5 s", "1 min" for a time-lapse interval. */
export function intervalLabel(ms: number): string {
  return ms >= 60_000 && ms % 60_000 === 0 ? `${ms / 60_000} min` : `${Math.round(ms / 1000)} s`;
}

export function createCaptureActions(deps: CaptureDeps): CaptureActions {
  const { api, toasts } = deps;
  const reveal = revealLabel(deps.platform);

  const saveError = (e: ErrorLike, what: "save" | "record") => {
    const folderAction = [{ label: "Choose another folder", onClick: deps.chooseFolder }];
    if (e.kind === "read_only") {
      toasts.show({
        tone: "error",
        text: "Couldn’t save: the folder is read-only.",
        actions: folderAction,
      });
    } else if (e.kind === "no_space" || e.kind === "low_disk") {
      toasts.show({
        tone: "error",
        text:
          what === "record"
            ? "Not enough disk space to record. Free up space or choose another folder."
            : "Couldn’t save: the disk is full.",
        actions: folderAction,
      });
    } else if (e.kind === "no_frame" || e.kind === "not_connected") {
      toasts.show({ tone: "info", text: "No image yet. Connect a camera first." });
    } else {
      toasts.show({
        tone: "error",
        text: what === "record" ? `Couldn’t record: ${e.detail}` : `Couldn’t save: ${e.detail}`,
      });
    }
  };

  const copyImage = async (source: RenderSource = "latest") => {
    try {
      const png = await api.renderPng(source, deps.spec(), {});
      if (!(png instanceof ArrayBuffer)) throw { kind: "other", detail: "no picture" };
      await api.writeClipboardPng(png);
      toasts.show({ tone: "ok", text: "Image copied" });
    } catch (err) {
      toasts.show({ tone: "error", text: `Couldn’t copy the image: ${toError(err).detail}` });
    }
  };

  const savePng = async (source: RenderSource = "latest") => {
    try {
      const res = await api.renderPng(source, deps.spec(), { dialog: true });
      if (res instanceof ArrayBuffer) return;
      toasts.show({
        tone: "ok",
        text: `Exported ${baseName(res.path)}`,
        actions: [{ label: reveal, onClick: () => void api.reveal(res.path).catch(() => {}) }],
      });
    } catch (err) {
      const e = toError(err);
      if (e.kind === "cancelled") return;
      toasts.show({ tone: "error", text: `Couldn’t save the PNG: ${e.detail}` });
    }
  };

  const snapshot = async () => {
    let saved: SnapshotInfo;
    try {
      saved = await api.snapshot(deps.pngOnSnapshot() ? deps.spec() : null);
    } catch (err) {
      saveError(toError(err), "save");
      return;
    }
    const source: RenderSource = { kind: "local", path: saved.path };
    toasts.show({
      tone: "ok",
      text: "Snapshot saved to Captures",
      key: "snapshot",
      actions: [
        { label: "Copy image", onClick: () => void copyImage(source) },
        { label: "Save PNG…", onClick: () => void savePng(source) },
        { label: reveal, onClick: () => void api.reveal(saved.path).catch(() => {}) },
      ],
    });
  };

  // ---------- Record and time-lapse ----------

  let state: RecorderState = { mode: "idle" };
  const subs = new Set<() => void>();
  const set = (next: RecorderState) => {
    state = next;
    for (const cb of subs) cb();
  };

  const start = async (intervalMs: number | null) => {
    if (state.mode !== "idle") return;
    set({ mode: "starting", timelapse: intervalMs !== null });
    try {
      const sensorMaterial = deps.sensorMaterial();
      const res = await api.recordStart(
        intervalMs === null ? { sensorMaterial } : { intervalMs, sensorMaterial },
      );
      set({
        mode: intervalMs === null ? "recording" : "timelapse",
        path: res.path,
        startedAt: Date.now(),
        frames: 0,
        elapsedMs: 0,
        intervalMs,
      });
    } catch (err) {
      set({ mode: "idle" });
      const e = toError(err);
      if (e.kind === "already_recording") {
        void recorder.poll();
        return;
      }
      saveError(e, "record");
    }
  };

  const recorder: Recorder = {
    get: () => state,
    subscribe(cb) {
      subs.add(cb);
      return () => subs.delete(cb);
    },
    startVideo: () => start(null),
    startTimelapse: (ms) => start(ms),
    async stop() {
      if (state.mode !== "recording" && state.mode !== "timelapse") return;
      try {
        await api.recordStop();
      } catch (err) {
        const e = toError(err);
        if (e.kind === "not_recording") set({ mode: "idle" });
        else toasts.show({ tone: "error", text: `Couldn’t stop: ${e.detail}` });
      }
    },
    async toggle() {
      if (state.mode === "idle") await start(null);
      else await recorder.stop();
    },
    async poll() {
      let st: RecordingStatus | null;
      try {
        st = await api.recordStatus();
      } catch {
        return;
      }
      if (!st) return;
      if (state.mode === "recording" || state.mode === "timelapse") {
        if (state.path !== st.path) return;
        set({
          ...state,
          frames: st.frames,
          elapsedMs: st.elapsed_ms,
          startedAt: Date.now() - st.elapsed_ms,
        });
      } else if (state.mode === "idle") {
        recorder.adopt(st);
      }
    },
    adopt(st) {
      set({
        mode: st.source === "timelapse" ? "timelapse" : "recording",
        path: st.path,
        startedAt: Date.now() - st.elapsed_ms,
        frames: st.frames,
        elapsedMs: st.elapsed_ms,
        intervalMs: null,
      });
    },
    stopped(info) {
      const wasTimelapse =
        state.mode === "timelapse" || (state.mode === "starting" && state.timelapse);
      if (state.mode !== "idle" && "path" in state && state.path !== info.path) return;
      set({ mode: "idle" });
      const what = wasTimelapse ? "Time-lapse" : "Recording";
      const length = wasTimelapse ? `${info.frames} frames` : durationLabel(info.duration_ms);
      const exportAction = { label: "Export video…", onClick: () => deps.exportVideo(info) };
      if (info.stopped === "write_error") {
        toasts.show({
          tone: "error",
          text: `${what} stopped: couldn’t write to the folder${info.detail ? ` (${info.detail})` : ""}. ${length} saved.`,
          actions: info.frames > 0 ? [exportAction] : [],
        });
      } else if (info.stopped === "low_disk") {
        toasts.show({
          tone: "error",
          text: `${what} stopped: the disk is almost full. ${length} saved.`,
          actions: info.frames > 0 ? [exportAction] : [],
        });
      } else if (info.frames > 0) {
        toasts.show({
          tone: "ok",
          text: `${what} saved to Captures · ${length}`,
          actions: [exportAction],
        });
      }
    },
  };

  return { snapshot, copyImage, savePng, recorder };
}
