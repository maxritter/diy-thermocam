import { tauriInvoke as invoke, isTauriRuntime, tauriListen } from "@/lib/backend";
import type {
  DeviceConfig,
  Diagnostics,
  FirmwareImageInfo,
  PerfTelemetry,
  RadParams,
} from "@/lib/types";
import { baseName, type CommandError as CameraError, commandError } from "@/lib/utils";

/**
 * The Camera page's calls to Rust (`docs/plans/2026-11-26-studio-ui-redesign/agents/rust-commands.md`).
 * In the browser preview there is no camera but the demo camera: its values are read, its
 * settings are read-only, and a firmware update is simulated end to end.
 */

/** One `flash-progress` event from `cmd_flash_firmware`. */
export type FlashProgress =
  | { stage: "verified"; sha256: string }
  | { stage: "waiting_bootloader" }
  | { stage: "waiting_timeout" }
  | { stage: "flashing"; done: number; total: number }
  | { stage: "rebooted"; sha256: string }
  | { stage: "error"; kind: FlashErrorKind; detail: string };

export type FlashErrorKind = "permission" | "write" | "not_found" | "bad_file";

export interface FirmwareFile {
  /** The file's name, for display. */
  name: string;
  /** Full path when Rust picked it. */
  path: string | null;
  text: string;
}

const READ_ONLY: CameraError = {
  kind: "read_only",
  detail: "The demo camera’s settings are read-only.",
};

/** The demo camera's values: a Lepton 3.5 on USB power at factory radiometry. */
const DEMO_CONFIG: DeviceConfig = {
  config_ver: 1,
  color_scheme: 10,
  text_color: 0,
  temp_format: 0,
  filter_type: 1,
  min_max_points: 3,
  rotation_vert: 0,
  rotation_horiz: 0,
  limits_mode: 0,
  shutter_mode: 1,
  lepton_gain_mode: 0,
  hot_cold_mode: 0,
  hot_cold_color: 2,
  screen_off_time: 0,
  overlay_flags: 0x0007,
  manual_min_raw: 29315,
  manual_max_raw: 31315,
  hot_cold_level_c10: 350,
  min_max_preset: 0,
};

/** Factory radiometry: emissivity 1 (8192 in Q3.13) and 22 °C everywhere. */
export const FACTORY_RAD_PARAMS: RadParams = {
  scene_emissivity: 8192,
  t_bkg_ck: 29515,
  tau_window: 8192,
  t_window_ck: 29515,
  tau_atm: 8192,
  t_atm_ck: 29515,
  refl_window: 0,
  t_refl_ck: 29515,
  flags: 1,
};

let demoFrames = 0;

function demoDiagnostics(): Diagnostics {
  return {
    diagnostic_mask: 0xff,
    bat_pct: -1,
    usb_power: true,
    sd_fs_type: 3,
    bat_mv: 4120,
    cpu_mhz: 450,
    mcu_temp_c: 41.2,
    sd_total_kb: 31268864,
    sd_free_kb: 28412000,
    uptime_us: Date.now() * 1000,
    free_dtcm_bytes: 214016,
    free_ocram_bytes: 342016,
    lepton_fpa_temp_c: 31.4,
    lepton_part_num: "500-0771-01",
  };
}

function demoPerf(): PerfTelemetry {
  demoFrames += 44;
  return {
    acquire_us: 840,
    stats_us: 210,
    filter_us: 390,
    scale_us: 620,
    colorize_us: 410,
    overlay_us: 280,
    display_us: 3100,
    sd_write_us: 24500,
    usb_tx_us: 2850,
    loop_total_us: 8700,
    frame_count: demoFrames,
    frames_dropped: 0,
    vospi_resyncs: 0,
    vospi_crc_errors: 0,
    usb_rx_errors: 0,
    usb_crc_errors: 0,
    live_fps_x100: 878,
    video_fps_x100: 874,
    stream_fps_x100: 878,
    perf_mode: 2,
    flags: 7,
    display_spi_mhz: 40,
    lepton_spi_mhz: 20,
    cpu_mhz: 450,
    mcu_temp_c: 41.2,
    frame_period_us: 113895,
  };
}

// ---------- Firmware simulation (browser preview only) ----------

/** How the simulated update ends; `window.__studioFirmwareSim` or `?fwsim=` picks one. */
export type FirmwareSimOutcome = "ok" | "timeout" | "write_error" | "permission";

const flashListeners = new Set<(ev: FlashProgress) => void>();

function emitFlash(ev: FlashProgress): void {
  for (const cb of flashListeners) cb(ev);
}

function simOutcome(): FirmwareSimOutcome {
  const w = typeof window !== "undefined" ? (window as unknown as Record<string, unknown>) : {};
  const fromWindow = w.__studioFirmwareSim;
  const fromQuery =
    typeof window !== "undefined" ? new URLSearchParams(window.location.search).get("fwsim") : null;
  const v = String(fromWindow ?? fromQuery ?? "ok");
  return (["ok", "timeout", "write_error", "permission"] as const).includes(v as FirmwareSimOutcome)
    ? (v as FirmwareSimOutcome)
    : "ok";
}

/** A stand-in SHA-256 for the preview: stable for the same text. */
function fakeSha256(text: string): string {
  let h = 0x811c9dc5;
  let out = "";
  for (let round = 0; round < 8; round++) {
    for (let i = 0; i < text.length; i++) {
      h ^= text.charCodeAt(i) + round;
      h = Math.imul(h, 0x01000193) >>> 0;
    }
    out += h.toString(16).padStart(8, "0");
  }
  return out;
}

/** Teensy 4.1 images live in FlexSPI flash at 0x6000_0000 (extended linear address 0x6000). */
function simVerify(text: string, expectedSha256?: string | null): FirmwareImageInfo {
  const records = text.split(/\r?\n/).filter((l) => l.startsWith(":"));
  if (records.length === 0 || !records.some((l) => /^:02000004600[0-7]/i.test(l))) {
    throw { kind: "bad_file", detail: "not a Teensy 4.1 image" } satisfies CameraError;
  }
  const dataBytes = records.reduce((n, l) => {
    const len = Number.parseInt(l.slice(1, 3), 16);
    return l.slice(7, 9) === "00" && Number.isFinite(len) ? n + len : n;
  }, 0);
  const total = Math.max(1024, dataBytes);
  const sha = fakeSha256(text);
  if (expectedSha256 && expectedSha256.trim().toLowerCase() !== sha) {
    throw { kind: "bad_file", detail: "SHA-256 does not match" } satisfies CameraError;
  }
  return {
    sha256_hex: sha,
    total_bytes: total,
    block_count: Math.ceil(total / 1024),
    min_addr: 0x60000000,
    max_addr: 0x60000000 + total - 1,
  };
}

const wait = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));

async function simFlash(text: string, expectedSha256?: string | null): Promise<string> {
  let info: FirmwareImageInfo;
  try {
    info = simVerify(text, expectedSha256);
  } catch (err) {
    const e = commandError(err);
    emitFlash({ stage: "error", kind: "bad_file", detail: e.detail });
    throw e;
  }
  const outcome = simOutcome();
  emitFlash({ stage: "verified", sha256: info.sha256_hex });
  emitFlash({ stage: "waiting_bootloader" });
  if (outcome === "timeout") {
    await wait(10_000);
    emitFlash({ stage: "waiting_timeout" });
    const e: CameraError = { kind: "not_found", detail: "update device not found" };
    emitFlash({ stage: "error", kind: "not_found", detail: e.detail });
    throw e;
  }
  await wait(2_400);
  if (outcome === "permission") {
    const e: CameraError = { kind: "permission", detail: "permission denied opening HID device" };
    emitFlash({ stage: "error", kind: "permission", detail: e.detail });
    throw e;
  }
  const total = info.block_count;
  const stopAt = outcome === "write_error" ? Math.round(total * 0.42) : total;
  const step = Math.max(1, Math.round(total / 40));
  for (let done = 0; done <= stopAt; done += step) {
    emitFlash({ stage: "flashing", done, total });
    await wait(90);
  }
  if (outcome === "write_error") {
    const e: CameraError = { kind: "write", detail: `HID write failed at block ${stopAt}` };
    emitFlash({ stage: "error", kind: "write", detail: e.detail });
    throw e;
  }
  emitFlash({ stage: "flashing", done: total, total });
  await wait(600);
  emitFlash({ stage: "rebooted", sha256: info.sha256_hex });
  return info.sha256_hex;
}

function pickWithInput(): Promise<FirmwareFile | null> {
  return new Promise((resolve) => {
    const input = document.createElement("input");
    input.type = "file";
    input.accept = ".hex,.HEX";
    input.addEventListener("change", () => {
      const file = input.files?.[0];
      if (!file) {
        resolve(null);
        return;
      }
      void file.text().then(
        (text) => resolve({ name: file.name, path: null, text }),
        () => resolve(null),
      );
    });
    input.addEventListener("cancel", () => resolve(null));
    input.click();
  });
}

export const cameraApi = {
  async getConfig(): Promise<DeviceConfig> {
    if (isTauriRuntime()) return invoke<DeviceConfig>("cmd_get_config");
    return { ...DEMO_CONFIG };
  },

  /** `cmd_set_config`; the demo camera refuses with `read_only`. */
  async setConfig(config: DeviceConfig, persist: boolean): Promise<DeviceConfig> {
    if (isTauriRuntime()) return invoke<DeviceConfig>("cmd_set_config", { config, persist });
    throw READ_ONLY;
  },

  /** `cmd_trigger_ffc`; force is needed in manual limits mode. */
  async triggerFfc(force: boolean): Promise<void> {
    if (isTauriRuntime()) await invoke<void>("cmd_trigger_ffc", { force });
    else throw READ_ONLY;
  },

  async getDiagnostics(): Promise<Diagnostics> {
    if (isTauriRuntime()) return invoke<Diagnostics>("cmd_get_diagnostics");
    return demoDiagnostics();
  },

  async getPerf(reset = false): Promise<PerfTelemetry> {
    if (isTauriRuntime()) return invoke<PerfTelemetry>("cmd_get_perf", { reset });
    return demoPerf();
  },

  /** `cmd_get_rad_params` (`GET_RAD_PARAMS`). */
  async getRadParams(): Promise<RadParams> {
    if (isTauriRuntime()) return invoke<RadParams>("cmd_get_rad_params");
    return { ...FACTORY_RAD_PARAMS };
  },

  /** `cmd_set_rad_params` (`SET_RAD_PARAMS`); flags b0 persist, b1 reset to factory values. */
  async setRadParams(params: RadParams): Promise<RadParams> {
    if (isTauriRuntime()) return invoke<RadParams>("cmd_set_rad_params", { params });
    throw READ_ONLY;
  },

  /** A firmware file: Rust's file dialog in the app, a file input in the browser. */
  async pickFirmwareFile(): Promise<FirmwareFile | null> {
    if (!isTauriRuntime()) return pickWithInput();
    try {
      const [path, text] = await invoke<[string, string]>("cmd_pick_hex_file");
      return { name: baseName(path), path, text };
    } catch {
      return null;
    }
  },

  /** The firmware file at `path` (`cmd_pick_hex_file` with a path), for Finish update. */
  async readFirmwareFile(path: string): Promise<FirmwareFile | null> {
    if (!isTauriRuntime()) return null;
    const [p, text] = await invoke<[string, string]>("cmd_pick_hex_file", { path });
    return { name: baseName(p), path: p, text };
  },

  /** `cmd_verify_hex`: checks the file without touching the camera. */
  async verifyFirmware(text: string, expectedSha256?: string | null): Promise<FirmwareImageInfo> {
    if (!isTauriRuntime()) return simVerify(text, expectedSha256);
    try {
      return await invoke<FirmwareImageInfo>("cmd_verify_hex", {
        hexText: text,
        expectedSha256: expectedSha256 || null,
      });
    } catch (err) {
      throw { kind: "bad_file", detail: commandError(err).detail } satisfies CameraError;
    }
  },

  /**
   * `cmd_flash_firmware`. Progress arrives as `flash-progress` events; the call rejects with
   * `{kind: "permission" | "write" | "not_found" | "bad_file", detail}`.
   */
  async flashFirmware(
    text: string,
    expectedSha256?: string | null,
    legacyPort?: string | null,
  ): Promise<string> {
    if (!isTauriRuntime()) return simFlash(text, expectedSha256);
    try {
      return await invoke<string>("cmd_flash_firmware", {
        hexText: text,
        expectedSha256: expectedSha256 || null,
        legacyPort: legacyPort || null,
      });
    } catch (err) {
      throw commandError(err);
    }
  },

  /** Opens a serial session again (`cmd_connect`) so Try again can ask for update mode. */
  async reopen(port: string): Promise<void> {
    if (isTauriRuntime()) await invoke<unknown>("cmd_connect", { port });
  },

  async onFlashProgress(cb: (ev: FlashProgress) => void): Promise<() => void> {
    if (!isTauriRuntime()) {
      flashListeners.add(cb);
      return () => flashListeners.delete(cb);
    }
    return tauriListen<FlashProgress>("flash-progress", cb);
  },

  /** Opens a help or release page in the system browser (opener plugin, limited by capability). */
  async openUrl(url: string): Promise<void> {
    if (isTauriRuntime()) {
      await invoke<void>("plugin:opener|open_url", { url });
      return;
    }
    window.open(url, "_blank", "noopener,noreferrer");
  },
};

export const LINKS = {
  manual: "https://www.diy-thermocam.net/docs/manual/",
  firmware: "https://github.com/maxritter/diy-thermocam/releases",
  latestFirmware: "https://github.com/maxritter/diy-thermocam/releases/latest",
  issues: "https://github.com/maxritter/diy-thermocam/issues",
} as const;
