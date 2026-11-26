import { isTauriRuntime } from "@/lib/backend";
import { DEFAULT_PALETTES, samplePalette } from "@/lib/palettes";
import { decodeFrameRawBinary, evaluateRoi, generateDemoFrame } from "@/lib/thermal";
import type {
  Material,
  RadiometryParams,
  Roi,
  RoiGeometry,
  SdListing,
  ThermalFrame,
} from "@/lib/types";

/**
 * The Captures page's calls to Rust (`agents/rust-commands.md`, plus `cmd_captures_list`).
 * In the browser preview a simulated camera card holds a few `YYYYMMDDHHMMSS.DAT` captures and
 * the seeded `20261126101500/` video folder (60 frames); "this computer" is kept in memory and
 * card pictures are drawn here instead of in Rust.
 */

export type CaptureRef =
  | { kind: "local"; path: string }
  | { kind: "camera"; serial: string; sd_path: string };

export type CaptureKind = "snapshot" | "recording" | "other";
export type Where = "camera" | "computer";

export interface CaptureItem {
  /** Stable key: where it is plus its path. */
  id: string;
  ref: CaptureRef;
  where: Where;
  /** File or folder name, e.g. `20261125102014.DAT` or `20261126101500`. */
  fileName: string;
  kind: CaptureKind;
  /** Bytes of a file, or of a recording's frames when known. */
  size: number | null;
  frames: number | null;
  mtimeMs: number | null;
}

export interface CardInfo {
  present: boolean;
  totalKb: number;
  freeKb: number;
}

export interface CaptureError {
  kind: string;
  detail: string;
}

/** Rejections come as `{kind, detail}` from new commands and as strings from older ones. */
export function toCaptureError(err: unknown): CaptureError {
  if (err && typeof err === "object" && "detail" in err) {
    const e = err as { kind?: unknown; detail?: unknown };
    return { kind: typeof e.kind === "string" ? e.kind : "other", detail: String(e.detail) };
  }
  if (err instanceof Error) return { kind: "other", detail: err.message };
  return { kind: "other", detail: String(err) };
}

// ---------- Edits (sidecar v2) ----------

export interface SidecarRegion {
  id: string;
  name: string;
  geometry: RoiGeometry;
  material: Material | null;
  visible: boolean;
}

export interface ThermoSidecar {
  schema_version: 2;
  palette_id: string;
  range: { mode: "auto" | "manual"; min_c: number | null; max_c: number | null };
  material: Material;
  sensor_material: Material | null;
  regions: SidecarRegion[];
  notes: { name: string; text: string };
}

export interface SidecarLoad {
  sidecar: ThermoSidecar | null;
  /** The damaged edits file was set aside under this name. */
  quarantined: string | null;
}

// ---------- Recordings ----------

export interface RecordingIndex {
  frames: number;
  /** Milliseconds since the start, one per frame; null for camera videos. */
  t_ms: number[] | null;
  width: number;
  height: number;
  source: "studio" | "timelapse" | "camera";
  fps_nominal: number;
  sensor_material: Material | null;
}

export interface SeriesRegion {
  id: string;
  geometry: RoiGeometry;
  material: Material;
  stat?: "min" | "max" | "avg";
}

export interface Series {
  t_ms: number[] | null;
  /** `values[region][frame]` in °C. */
  values: number[][];
}

export interface FrameRange {
  from: number;
  to: number;
}

// ---------- Tauri ----------

async function invoke<T>(cmd: string, args?: Record<string, unknown>): Promise<T> {
  const core = await import("@tauri-apps/api/core");
  return core.invoke<T>(cmd, args);
}

async function listen<T>(event: string, cb: (payload: T) => void): Promise<() => void> {
  const ev = await import("@tauri-apps/api/event");
  return ev.listen<T>(event, (e) => cb(e.payload));
}

interface DatJson {
  width: number;
  height: number;
  pixels: number[];
  min_raw: number;
  max_raw: number;
  cal_offset: number;
  cal_slope: number;
  spot_temp?: number;
}

/** A `.DAT` read by Rust, as the frame the viewer draws (hottest and coldest found here). */
export function frameFromDat(dat: DatJson): ThermalFrame {
  const pixels = Uint16Array.from(dat.pixels);
  let min = 65535;
  let max = 0;
  let minI = 0;
  let maxI = 0;
  for (let i = 0; i < pixels.length; i++) {
    const v = pixels[i];
    if (v < min) {
      min = v;
      minI = i;
    }
    if (v > max) {
      max = v;
      maxI = i;
    }
  }
  return {
    frame_seq: 0,
    timestamp_us: 0,
    rtc_epoch: 0,
    dropped_frames: 0,
    width: dat.width,
    height: dat.height,
    min_raw: min,
    max_raw: max,
    min_x: minI % dat.width,
    min_y: Math.floor(minI / dat.width),
    max_x: maxI % dat.width,
    max_y: Math.floor(maxI / dat.width),
    spot_temp_c: dat.spot_temp ?? 0,
    cal_offset: dat.cal_offset,
    cal_slope: dat.cal_slope,
    fpa_temp_c: 0,
    bat_pct: -1,
    usb_power: true,
    pixels,
  };
}

function isDatName(name: string): boolean {
  return /\.dat$/i.test(name);
}

const DATE_FOLDER = /^\d{14}(?:-\d+)?$/;

function cardPath(name: string): string {
  return `/${name.replace(/^\/+/, "")}`;
}

/** The card's root listing as library items: `.DAT` files, date-named video folders, the rest. */
export function itemsFromListing(listing: SdListing, serial: string): CaptureItem[] {
  return listing.entries
    .filter((e) => !e.name.startsWith("."))
    .map((e) => {
      const kind: CaptureKind = e.is_dir
        ? DATE_FOLDER.test(e.name)
          ? "recording"
          : "other"
        : isDatName(e.name)
          ? "snapshot"
          : "other";
      const sd_path = cardPath(e.name);
      return {
        id: `camera:${sd_path}`,
        ref: { kind: "camera", serial, sd_path },
        where: "camera",
        fileName: e.name,
        kind,
        size: e.is_dir ? null : e.size,
        frames: null,
        mtimeMs: e.mtime_epoch > 0 ? e.mtime_epoch * 1000 : null,
      } satisfies CaptureItem;
    });
}

interface LocalCaptureJson {
  name: string;
  path: string;
  kind: CaptureKind;
  size: number;
  frames: number | null;
  mtime_ms: number;
}

function localItem(c: LocalCaptureJson): CaptureItem {
  return {
    id: `local:${c.path}`,
    ref: { kind: "local", path: c.path },
    where: "computer",
    fileName: c.name,
    kind: c.kind,
    size: c.size,
    frames: c.frames,
    mtimeMs: c.mtime_ms || null,
  };
}

function baseName(path: string): string {
  const parts = path.split(/[\\/]+/).filter(Boolean);
  return parts[parts.length - 1] ?? path;
}

function parentDir(path: string): string {
  const i = Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\"));
  return i > 0 ? path.slice(0, i) : path;
}

// ---------- Files opened before (listed on this computer) ----------

const OPENED_KEY = "thermocam.opened";

function readOpened(): CaptureItem[] {
  try {
    const raw = JSON.parse(localStorage.getItem(OPENED_KEY) ?? "[]");
    return Array.isArray(raw) ? (raw as CaptureItem[]).filter((i) => i?.ref?.kind === "local") : [];
  } catch {
    return [];
  }
}

function writeOpened(items: CaptureItem[]): void {
  try {
    localStorage.setItem(OPENED_KEY, JSON.stringify(items.slice(0, 50)));
  } catch {
    // Private mode or full storage: files opened before are not listed then.
  }
}

// ---------- Browser fallback ----------

const DEMO_SERIAL = "demo";
const DEMO_VIDEO = "20261126101500";
const DEMO_VIDEO_FRAMES = 60;
const DEMO_FPS = 8.7;
const DEMO_COMPUTER_DIR = "~/Pictures/Thermocam";

interface SimFile {
  name: string;
  seq: number;
  mtime: number;
  size: number;
  /** A file the viewer cannot open (damaged). */
  damaged?: boolean;
}

const simCard = {
  present: true,
  totalKb: 31_268_864,
  freeKb: 28_412_000,
  files: [
    { name: "20261125101500.DAT", seq: 3, mtime: 1764065700, size: 38_805 },
    { name: "20261125102014.DAT", seq: 16, mtime: 1764066014, size: 38_805 },
    { name: "20261125102847.DAT", seq: 29, mtime: 1764066527, size: 38_805 },
    { name: "20261123180233.DAT", seq: 42, mtime: 1763917353, size: 38_805 },
    { name: "20261123174809.DAT", seq: 55, mtime: 1763916489, size: 1_204, damaged: true },
  ] as SimFile[],
  video: true,
};

interface SimLocal {
  item: CaptureItem;
  seq: number;
  damaged?: boolean;
  /** A recording made in Studio: frame times. */
  t_ms?: number[];
}

function simRecordingTimes(frames: number): number[] {
  // About 8.7 fps with a little jitter, as the camera's USB stream arrives.
  return Array.from({ length: frames }, (_, i) => Math.round(i * 114.9 + (i % 3) * 3));
}

const simComputer: SimLocal[] = [
  {
    item: localItem({
      name: "20261125105218",
      path: `${DEMO_COMPUTER_DIR}/20261125105218`,
      kind: "recording",
      size: 120 * 38_805,
      frames: 120,
      mtime_ms: 1764067938000,
    }),
    seq: 200,
    t_ms: simRecordingTimes(120),
  },
  {
    item: localItem({
      name: "20261125104512.DAT",
      path: `${DEMO_COMPUTER_DIR}/20261125104512.DAT`,
      kind: "snapshot",
      size: 38_805,
      frames: null,
      mtime_ms: 1764067512000,
    }),
    seq: 19,
  },
];

const simSidecars = new Map<string, ThermoSidecar>();
const simQuarantine = new Set<string>(["camera:/20261123180233.DAT"]);

function refKey(ref: CaptureRef): string {
  return ref.kind === "local" ? `local:${ref.path}` : `camera:${ref.sd_path}`;
}

function seedSidecar(name: string): ThermoSidecar {
  return {
    schema_version: 2,
    palette_id: "ironblack",
    range: { mode: "auto", min_c: null, max_c: null },
    material: { emissivity: 0.95, reflected_c: 22 },
    sensor_material: null,
    regions: [],
    notes: { name, text: "" },
  };
}
simSidecars.set("camera:/20261125101500.DAT", seedSidecar("PCB load"));

function simFrameFor(seq: number): ThermalFrame {
  return generateDemoFrame(seq);
}

function simCardFile(sdPath: string): SimFile | undefined {
  return simCard.files.find((f) => cardPath(f.name) === sdPath);
}

function simLocal(path: string): SimLocal | undefined {
  return simComputer.find((c) => c.item.ref.kind === "local" && c.item.ref.path === path);
}

function badFile(): CaptureError {
  return { kind: "bad_file", detail: "unexpected .DAT size: 1204 bytes" };
}

/** Edits files Rust set aside whose capture has not been opened since. */
const setAsideNotShown = new Map<string, string>();

const delay = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));

/** A card picture drawn in the browser, as Rust would render it: 320 × 240 in the palette. */
function drawThumbnail(frame: ThermalFrame, paletteId: string): string {
  const palette = DEFAULT_PALETTES.find((p) => p.id === paletteId) ?? DEFAULT_PALETTES[0];
  if (typeof document === "undefined") return "";
  const cv = document.createElement("canvas");
  cv.width = frame.width;
  cv.height = frame.height;
  const ctx = cv.getContext?.("2d");
  if (!ctx) return "";
  const img = ctx.createImageData(frame.width, frame.height);
  const span = Math.max(1, frame.max_raw - frame.min_raw);
  for (let i = 0; i < frame.pixels.length; i++) {
    const [r, g, b] = samplePalette(palette, (frame.pixels[i] - frame.min_raw) / span);
    img.data[i * 4] = r;
    img.data[i * 4 + 1] = g;
    img.data[i * 4 + 2] = b;
    img.data[i * 4 + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
  return cv.toDataURL("image/png");
}

function simSeries(frames: ThermalFrame[], regions: SeriesRegion[]): number[][] {
  return regions.map((r) =>
    frames.map((fr) => {
      const rad: RadiometryParams = {
        cal_offset: fr.cal_offset,
        cal_slope: fr.cal_slope,
        emissivity: r.material.emissivity,
        reflected_temp_c: r.material.reflected_c,
        sensor_material: null,
      };
      const roi: Roi = {
        id: r.id,
        label: r.id,
        geometry: r.geometry,
        emissivity_override: null,
        reflected_temp_override_c: null,
        visible: true,
      };
      const st = evaluateRoi(roi, fr.pixels, fr.width, fr.height, rad);
      return r.stat === "min" ? st.min_c : r.stat === "avg" ? st.mean_c : st.max_c;
    }),
  );
}

/** The local folder a camera video plays from after it is copied (browser preview). */
const SIM_CACHE = "~/Library/Caches/Studio/sd/demo";

function simRecordingAt(path: string): { seq: number; index: RecordingIndex } | null {
  if (path === `${SIM_CACHE}/${DEMO_VIDEO}`) {
    return {
      seq: 1000,
      index: {
        frames: DEMO_VIDEO_FRAMES,
        t_ms: null,
        width: 160,
        height: 120,
        source: "camera",
        fps_nominal: DEMO_FPS,
        sensor_material: null,
      },
    };
  }
  const local = simLocal(path);
  if (local?.item.kind === "recording") {
    const frames = local.item.frames ?? 0;
    return {
      seq: local.seq,
      index: {
        frames,
        t_ms: local.t_ms ?? null,
        width: 160,
        height: 120,
        source: local.t_ms ? "studio" : "camera",
        fps_nominal: DEMO_FPS,
        sensor_material: null,
      },
    };
  }
  return null;
}

// ---------- The API ----------

const thumbUrls = new Map<string, string>();

export const capturesApi = {
  /** The connected camera's serial (`cmd_camera_serial`), used in camera capture refs. */
  async cameraSerial(): Promise<string | null> {
    if (!isTauriRuntime()) return DEMO_SERIAL;
    return invoke<string | null>("cmd_camera_serial");
  },

  /** The camera card's root (`cmd_sd_list("/")`): free space and the library items. */
  async listCamera(serial: string): Promise<{ card: CardInfo; items: CaptureItem[] }> {
    if (isTauriRuntime()) {
      const listing = await invoke<SdListing>("cmd_sd_list", { path: "/" });
      return {
        card: {
          present: listing.info.card_present,
          totalKb: listing.info.total_kb,
          freeKb: listing.info.free_kb,
        },
        items: listing.info.card_present ? itemsFromListing(listing, serial) : [],
      };
    }
    await delay(250);
    const entries = simCard.files.map((f) => ({
      name: f.name,
      size: f.size,
      mtime_epoch: f.mtime,
      is_dir: false,
    }));
    if (simCard.video) {
      entries.push({ name: DEMO_VIDEO, size: 0, mtime_epoch: 1764148500, is_dir: true });
    }
    const listing: SdListing = {
      info: {
        card_present: simCard.present,
        fs_type: 3,
        total_kb: simCard.totalKb,
        free_kb: simCard.freeKb,
        root_entry_count: entries.length,
      },
      entries,
    };
    return {
      card: { present: simCard.present, totalKb: simCard.totalKb, freeKb: simCard.freeKb },
      items: simCard.present ? itemsFromListing(listing, serial) : [],
    };
  },

  /** Frames in a video folder on the card (one listing page per lock in Rust). */
  async cameraFolderFrames(sdPath: string): Promise<number> {
    if (isTauriRuntime()) {
      const listing = await invoke<SdListing>("cmd_sd_list", { path: sdPath });
      return listing.entries.filter((e) => !e.is_dir && isDatName(e.name)).length;
    }
    await delay(60);
    return sdPath === cardPath(DEMO_VIDEO) ? DEMO_VIDEO_FRAMES : 0;
  },

  /** The Captures folder (`cmd_captures_list`) plus files opened before from elsewhere. */
  async listComputer(): Promise<CaptureItem[]> {
    const opened = readOpened();
    if (isTauriRuntime()) {
      const listed = (await invoke<LocalCaptureJson[]>("cmd_captures_list")).map(localItem);
      const ids = new Set(listed.map((i) => i.id));
      return [...listed, ...opened.filter((o) => !ids.has(o.id))];
    }
    await delay(120);
    const ids = new Set(simComputer.map((c) => c.item.id));
    return [...simComputer.map((c) => c.item), ...opened.filter((o) => !ids.has(o.id))];
  },

  /** Remembers a file opened from elsewhere, so "On this computer" lists it. */
  rememberOpened(item: CaptureItem): void {
    if (item.where !== "computer") return;
    writeOpened([item, ...readOpened().filter((o) => o.id !== item.id)]);
  },

  /**
   * A card picture (`cmd_thumbnail`, 320 × 240, cached in Rust) as an image URL. Kept for the
   * session so switching tabs does not ask again.
   */
  async thumbnail(item: CaptureItem, paletteId: string): Promise<string> {
    const key = `${item.id}\0${item.mtimeMs ?? ""}\0${item.size ?? ""}\0${paletteId}`;
    const known = thumbUrls.get(key);
    if (known) return known;
    let url: string;
    if (isTauriRuntime()) {
      const png = await invoke<ArrayBuffer>("cmd_thumbnail", { capture: item.ref, paletteId });
      url = URL.createObjectURL(new Blob([png], { type: "image/png" }));
    } else {
      await delay(90);
      url = drawThumbnail(await capturesApi.loadFirstFrame(item), paletteId);
    }
    thumbUrls.set(key, url);
    return url;
  },

  /** A capture's first frame (a snapshot, or a recording's first frame). */
  async loadFirstFrame(item: CaptureItem): Promise<ThermalFrame> {
    if (item.kind === "recording") {
      if (!isTauriRuntime()) {
        if (item.ref.kind === "camera") return simFrameFor(1000);
        const local = simLocal(item.ref.path);
        return simFrameFor(local?.seq ?? 1);
      }
      if (item.ref.kind === "local") return capturesApi.recordingFrame(item.ref.path, 0);
    }
    return capturesApi.loadFrame(item.ref);
  },

  /** One `.DAT` as a frame: `cmd_load_dat_file(path)` here, `cmd_sd_read_dat` on the card. */
  async loadFrame(ref: CaptureRef): Promise<ThermalFrame> {
    if (isTauriRuntime()) {
      try {
        if (ref.kind === "local") {
          const [, dat] = await invoke<[string, DatJson, unknown]>("cmd_load_dat_file", {
            path: ref.path,
          });
          return frameFromDat(dat);
        }
        return frameFromDat(await invoke<DatJson>("cmd_sd_read_dat", { path: ref.sd_path }));
      } catch (err) {
        const e = toCaptureError(err);
        throw /dat|layout|size|parse|invalid|unexpected/i.test(e.detail)
          ? { kind: "bad_file", detail: e.detail }
          : e;
      }
    }
    await delay(80);
    if (ref.kind === "camera") {
      const f = simCardFile(ref.sd_path);
      if (!f) throw { kind: "not_found", detail: "The file is no longer on the card." };
      if (f.damaged) throw badFile();
      return simFrameFor(f.seq);
    }
    const local = simLocal(ref.path);
    if (local?.damaged) throw badFile();
    return simFrameFor(local?.seq ?? 19);
  },

  /**
   * A capture's edits (`cmd_sidecar_load`). Rust sets a damaged edits file aside the first time
   * anything reads it, often the library looking up names; that is remembered here, so the
   * capture's message (`opening`) still names the file until it is dismissed or new edits save.
   */
  async sidecarLoad(ref: CaptureRef, opening = false): Promise<SidecarLoad> {
    const key = refKey(ref);
    let res: SidecarLoad;
    if (isTauriRuntime()) {
      res = await invoke<SidecarLoad>("cmd_sidecar_load", { capture: ref });
    } else if (simQuarantine.has(key)) {
      simQuarantine.delete(key);
      const name = ref.kind === "local" ? baseName(ref.path) : baseName(ref.sd_path);
      res = { sidecar: null, quarantined: `${name.replace(/\.dat$/i, "")}.thermo.json.bad` };
    } else {
      const s = simSidecars.get(key);
      res = { sidecar: s ? structuredClone(s) : null, quarantined: null };
    }
    if (res.quarantined) setAsideNotShown.set(key, res.quarantined);
    if (!opening) return res;
    return { sidecar: res.sidecar, quarantined: setAsideNotShown.get(key) ?? null };
  },

  /** The set-aside message was read (dismissed), or new edits replaced the damaged file. */
  forgetSetAside(ref: CaptureRef): void {
    setAsideNotShown.delete(refKey(ref));
  },

  async sidecarSave(ref: CaptureRef, sidecar: ThermoSidecar): Promise<string> {
    setAsideNotShown.delete(refKey(ref));
    if (isTauriRuntime()) return invoke<string>("cmd_sidecar_save", { capture: ref, sidecar });
    simSidecars.set(refKey(ref), structuredClone(sidecar));
    const name = ref.kind === "local" ? ref.path : ref.sd_path;
    return `${name.replace(/\.dat$/i, "")}.thermo.json`;
  },

  /** Reset to original: deletes the edits file. */
  async sidecarReset(ref: CaptureRef): Promise<boolean> {
    if (isTauriRuntime()) return invoke<boolean>("cmd_sidecar_reset", { capture: ref });
    return simSidecars.delete(refKey(ref));
  },

  /**
   * Open file…: a `.DAT` (or a frame inside a recording folder, which opens the recording).
   * Null when the dialog was cancelled.
   */
  async openFile(): Promise<CaptureItem | null> {
    if (isTauriRuntime()) {
      let path: string;
      try {
        [path] = await invoke<[string, DatJson, unknown]>("cmd_load_dat_file", { path: null });
      } catch (err) {
        const e = toCaptureError(err);
        if (/cancel/i.test(e.detail)) return null;
        throw e;
      }
      const name = baseName(path);
      if (/^\d{6}\.dat$/i.test(name)) {
        const folder = parentDir(path);
        try {
          const rec = await capturesApi.recordingOpen(folder);
          return {
            id: `local:${folder}`,
            ref: { kind: "local", path: folder },
            where: "computer",
            fileName: baseName(folder),
            kind: "recording",
            size: null,
            frames: rec.frames,
            mtimeMs: null,
          };
        } catch {
          // Not a recording after all: open the single frame.
        }
      }
      return {
        id: `local:${path}`,
        ref: { kind: "local", path },
        where: "computer",
        fileName: name,
        kind: "snapshot",
        size: null,
        frames: null,
        mtimeMs: null,
      };
    }
    return simComputer[1]?.item ?? null;
  },

  /** Moves captures on this computer to the Trash (`cmd_trash`), with their edits. */
  async trash(
    paths: string[],
  ): Promise<{ trashed: string[]; failed: { path: string; kind: string; detail: string }[] }> {
    if (isTauriRuntime()) return invoke("cmd_trash", { paths });
    const trashed: string[] = [];
    for (const p of paths) {
      const i = simComputer.findIndex((c) => c.item.ref.kind === "local" && c.item.ref.path === p);
      if (i >= 0) simComputer.splice(i, 1);
      writeOpened(readOpened().filter((o) => !(o.ref.kind === "local" && o.ref.path === p)));
      trashed.push(p);
    }
    return { trashed, failed: [] };
  },

  /** Deletes files from the camera's card, one by one (`cmd_sd_delete`). */
  async deleteFromCamera(
    sdPaths: string[],
  ): Promise<{ deleted: string[]; failed: { path: string; kind: string; detail: string }[] }> {
    const deleted: string[] = [];
    const failed: { path: string; kind: string; detail: string }[] = [];
    for (const p of sdPaths) {
      try {
        if (isTauriRuntime()) {
          await invoke<void>("cmd_sd_delete", { path: p });
        } else {
          await delay(40);
          if (p === cardPath(DEMO_VIDEO)) simCard.video = false;
          else simCard.files = simCard.files.filter((f) => cardPath(f.name) !== p);
          simSidecars.delete(`camera:${p}`);
        }
        deleted.push(p);
      } catch (err) {
        failed.push({ path: p, ...toCaptureError(err) });
      }
    }
    return { deleted, failed };
  },

  /** Copies a `.DAT` from the card into the Captures folder (`cmd_sd_download`). */
  async copyToComputer(item: CaptureItem, capturesDir: string): Promise<string> {
    if (item.ref.kind !== "camera") throw { kind: "other", detail: "Already on this computer." };
    const target = `${capturesDir.replace(/[\\/]+$/, "")}/${item.fileName}`;
    if (isTauriRuntime()) {
      return invoke<string>("cmd_sd_download", { path: item.ref.sd_path, outputPath: target });
    }
    await delay(120);
    const f = simCardFile(item.ref.sd_path);
    const path = `${DEMO_COMPUTER_DIR}/${item.fileName}`;
    if (!simLocal(path)) {
      simComputer.push({
        item: localItem({
          name: item.fileName,
          path,
          kind: "snapshot",
          size: item.size ?? 38_805,
          frames: null,
          mtime_ms: Date.now(),
        }),
        seq: f?.seq ?? 1,
        damaged: f?.damaged,
      });
    }
    return path;
  },

  async recordingOpen(path: string): Promise<RecordingIndex> {
    if (isTauriRuntime()) return invoke<RecordingIndex>("cmd_recording_open", { path });
    await delay(60);
    const rec = simRecordingAt(path);
    if (!rec) throw { kind: "bad_file", detail: "The folder holds no .DAT frames." };
    return rec.index;
  },

  /** One frame of a recording on this computer (`cmd_recording_frame`, raw bytes). */
  async recordingFrame(path: string, index: number): Promise<ThermalFrame> {
    if (isTauriRuntime()) {
      const buf = await invoke<ArrayBuffer>("cmd_recording_frame", { path, index });
      return decodeFrameRawBinary(buf);
    }
    const rec = simRecordingAt(path);
    if (!rec || index < 0 || index >= rec.index.frames) {
      throw { kind: "bad_range", detail: `frame ${index} is outside the recording` };
    }
    return simFrameFor(rec.seq + index);
  },

  /** Copies a video folder from the card to this computer first (`cmd_sd_fetch_folder`). */
  async fetchCameraFolder(
    sdPath: string,
    onProgress: (done: number, total: number) => void,
  ): Promise<string> {
    if (isTauriRuntime()) {
      const off = await listen<{ done: number; total: number }>("sd-fetch-progress", (p) =>
        onProgress(p.done, p.total),
      );
      try {
        return await invoke<string>("cmd_sd_fetch_folder", { sdPath });
      } finally {
        off();
      }
    }
    const total = DEMO_VIDEO_FRAMES;
    for (let done = 0; done <= total; done += 6) {
      onProgress(Math.min(done, total), total);
      await delay(45);
    }
    onProgress(total, total);
    return `${SIM_CACHE}/${baseName(sdPath)}`;
  },

  /** Each region's value in every frame (`cmd_recording_series`), with progress. */
  async recordingSeries(
    path: string,
    regions: SeriesRegion[],
    onProgress?: (done: number, total: number) => void,
  ): Promise<Series> {
    if (isTauriRuntime()) {
      const off = onProgress
        ? await listen<{ done: number; total: number }>("series-progress", (p) =>
            onProgress(p.done, p.total),
          )
        : null;
      try {
        return await invoke<Series>("cmd_recording_series", { path, regions });
      } finally {
        off?.();
      }
    }
    const rec = simRecordingAt(path);
    if (!rec) throw { kind: "bad_file", detail: "The folder holds no .DAT frames." };
    const frames = Array.from({ length: rec.index.frames }, (_, i) => simFrameFor(rec.seq + i));
    return { t_ms: rec.index.t_ms, values: simSeries(frames, regions) };
  },

  /**
   * Exports a recording's trimmed range (`cmd_export_media` with `recording` and `range`):
   * MP4, GIF, `.DAT` frames or the series CSV. Rust asks where to save. Returns the path.
   */
  async exportRecording(req: {
    format: "mp4" | "gif" | "dat" | "series-csv";
    recording: string;
    range: FrameRange | null;
    regions: SeriesRegion[];
    paletteId: string;
    minRaw: number;
    maxRaw: number;
    rad: RadiometryParams;
    unit: string;
    title: string;
  }): Promise<string> {
    if (isTauriRuntime()) {
      return invoke<string>("cmd_export_media", {
        req: {
          format: req.format,
          output_path: null,
          palette_id: req.paletteId,
          min_raw: Math.round(req.minRaw),
          max_raw: Math.round(req.maxRaw),
          rad: req.rad,
          pipeline: {
            filter: "none",
            upscaler: "edge_preserve",
            sharpen_amount: 0.45,
            rotate_180: false,
            flip_horizontal: false,
            histogram_eq: false,
            isotherm: { mode: "off", low_c: 40, high_c: 60, color: [0, 230, 160] },
          },
          rois: [],
          resolution: "full_hd1080p",
          show_colorbar: true,
          show_hud: true,
          unit: req.unit,
          title: req.title,
          notes: null,
          recording: req.recording,
          range: req.range,
          regions: req.regions,
        },
      });
    }
    await delay(200);
    const ext = req.format === "series-csv" ? "csv" : req.format;
    return `~/Downloads/${baseName(req.recording)}.${ext}`;
  },

  /** Save PNG… of a capture (`cmd_render_png` with a Save dialog). Returns the path. */
  async savePng(
    source: CaptureRef | { kind: "recording"; path: string; index: number },
    spec: {
      palette_id: string;
      range?: { mode: string; min_c: number | null; max_c: number | null };
    },
  ): Promise<string | null> {
    if (isTauriRuntime()) {
      try {
        const res = await invoke<{ path: string }>("cmd_render_png", {
          req: { source, ...spec, overlays: { colorbar: true }, scale: 4, dialog: true },
        });
        return res.path;
      } catch (err) {
        const e = toCaptureError(err);
        if (e.kind === "cancelled") return null;
        throw e;
      }
    }
    return "~/Downloads/thermocam.png";
  },

  /** Show in Finder / Explorer / the file manager (opener plugin). Does nothing in the browser. */
  async revealInFolder(path: string): Promise<void> {
    if (!isTauriRuntime()) return;
    const { revealItemInDir } = await import("@tauri-apps/plugin-opener");
    await revealItemInDir(path);
  },

  /** For tests and the browser preview: the simulated card is empty or has no card. */
  _simulateCard(state: { present?: boolean }): void {
    if (state.present !== undefined) simCard.present = state.present;
  },
};
