import { DEFAULT_MATERIALS, DEFAULT_PALETTES, parseCustomPaletteText } from "./palettes";
import {
  parseSettings,
  readStoredSettings,
  type StudioSettings,
  writeStoredSettings,
} from "./settings";
import { decodeFrameRawBinary, generateDemoFrame } from "./thermal";
import type {
  DeviceConfig,
  Diagnostics,
  DisplayPipeline,
  ExportResolution,
  FirmwareImageInfo,
  HelloResp,
  MaterialPreset,
  Palette,
  PerfTelemetry,
  RadiometryParams,
  RadParams,
  Roi,
  SdListing,
  SerialPortInfo,
  TempUnit,
  ThermalFrame,
} from "./types";

export function isTauriRuntime(): boolean {
  return typeof window !== "undefined" && "__TAURI_INTERNALS__" in window;
}

async function tauriInvoke<T>(cmd: string, args?: Record<string, unknown>): Promise<T> {
  const { invoke } = await import("@tauri-apps/api/core");
  return invoke<T>(cmd, args);
}

/** True when Tauri rejected because the command is not registered (an older Rust build). */
export function isMissingCommand(err: unknown, cmd: string): boolean {
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
let simConfig: DeviceConfig = {
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
  overlay_flags: 0x003f,
  manual_min_raw: 29315,
  manual_max_raw: 31315,
  hot_cold_level_c10: 350,
  min_max_preset: 0,
};
let simRadParams: RadParams = {
  scene_emissivity: 7782,
  t_bkg_ck: 29515,
  tau_window: 8192,
  t_window_ck: 29515,
  tau_atm: 8192,
  t_atm_ck: 29515,
  refl_window: 0,
  t_refl_ck: 29515,
  flags: 1,
};
let simSdFiles: { name: string; size: number; mtime_epoch: number; seq: number }[] = [
  { name: "20261125_101500_PCB_LOAD.DAT", size: 38805, mtime_epoch: 1764065700, seq: 3 },
  { name: "20261125_102030_VRM_PEAK.DAT", size: 38805, mtime_epoch: 1764066030, seq: 16 },
  { name: "20261125_102845_HEATSINK.DAT", size: 38805, mtime_epoch: 1764066525, seq: 29 },
  { name: "20261125_104110_SEQ_001.DAT", size: 38805, mtime_epoch: 1764067270, seq: 42 },
];

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
    return [
      {
        port_name: "sim://demo-lepton35",
        vid: 0x16c0,
        pid: 0x0483,
        manufacturer: "DIY-Thermocam",
        product: "Simulator (Lepton 3.5)",
        is_teensy: false,
      },
    ];
  },

  async getPalettes(): Promise<Palette[]> {
    if (isTauriRuntime()) return tauriInvoke<Palette[]>("cmd_get_palettes");
    return DEFAULT_PALETTES;
  },

  async getMaterialPresets(): Promise<MaterialPreset[]> {
    if (isTauriRuntime()) return tauriInvoke<MaterialPreset[]>("cmd_get_material_presets");
    return DEFAULT_MATERIALS;
  },

  async importCustomPalette(name: string, data: string, isPngBase64: boolean): Promise<Palette> {
    if (isTauriRuntime()) {
      return tauriInvoke<Palette>("cmd_import_custom_palette", { name, data, isPngBase64 });
    }
    return parseCustomPaletteText(name, data);
  },

  async connect(port: string): Promise<HelloResp> {
    if (isTauriRuntime()) return tauriInvoke<HelloResp>("cmd_connect", { port });
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
    onFrame(generateDemoFrame(simSeq++));
    simTimer = setInterval(() => {
      onFrame(generateDemoFrame(simSeq++));
    }, 114);
  },

  async stopStream(): Promise<void> {
    if (simTimer) {
      clearInterval(simTimer);
      simTimer = null;
    }
    if (isTauriRuntime()) await tauriInvoke<void>("cmd_stop_stream");
  },

  async getConfig(): Promise<DeviceConfig> {
    if (isTauriRuntime()) return tauriInvoke<DeviceConfig>("cmd_get_config");
    return { ...simConfig };
  },

  async setConfig(config: DeviceConfig, persist: boolean): Promise<DeviceConfig> {
    if (isTauriRuntime()) return tauriInvoke<DeviceConfig>("cmd_set_config", { config, persist });
    simConfig = { ...config };
    return { ...simConfig };
  },

  async triggerFfc(force: boolean): Promise<void> {
    if (isTauriRuntime()) await tauriInvoke<void>("cmd_trigger_ffc", { force });
  },

  async getDiagnostics(): Promise<Diagnostics> {
    if (isTauriRuntime()) return tauriInvoke<Diagnostics>("cmd_get_diagnostics");
    return {
      diagnostic_mask: 0xff,
      bat_pct: 87,
      usb_power: true,
      sd_fs_type: 3,
      bat_mv: 4120,
      cpu_mhz: 450,
      mcu_temp_c: 41.2,
      sd_total_kb: 31268864,
      sd_free_kb: 28412000,
      uptime_us: simSeq * 113895,
      free_dtcm_bytes: 214016,
      free_ocram_bytes: 342016,
      lepton_fpa_temp_c: 31.4,
      lepton_part_num: "500-0771-01 (Lepton 3.5)",
    };
  },

  async getPerf(reset = false): Promise<PerfTelemetry> {
    if (isTauriRuntime()) return tauriInvoke<PerfTelemetry>("cmd_get_perf", { reset });
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
      frame_count: simSeq,
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
  },

  async getRadParams(): Promise<RadParams> {
    if (isTauriRuntime()) return tauriInvoke<RadParams>("cmd_get_rad_params");
    return { ...simRadParams };
  },

  async setRadParams(params: RadParams): Promise<RadParams> {
    if (isTauriRuntime()) return tauriInvoke<RadParams>("cmd_set_rad_params", { params });
    simRadParams = { ...params };
    return { ...simRadParams };
  },

  async sdList(path = "/"): Promise<SdListing> {
    if (isTauriRuntime()) return tauriInvoke<SdListing>("cmd_sd_list", { path });
    return {
      info: {
        card_present: true,
        fs_type: 3,
        total_kb: 31268864,
        free_kb: 28412000,
        root_entry_count: simSdFiles.length,
      },
      entries: simSdFiles.map((f) => ({
        name: f.name,
        size: f.size,
        mtime_epoch: f.mtime_epoch,
        is_dir: false,
      })),
    };
  },

  async sdReadFrame(name: string): Promise<ThermalFrame> {
    if (isTauriRuntime()) {
      const dat = await tauriInvoke<{
        width: number;
        height: number;
        pixels: number[];
        min_raw: number;
        max_raw: number;
        cal_offset: number;
        cal_slope: number;
        spot_temp?: number;
        spot_temp_c?: number;
      }>("cmd_sd_read_dat", { path: `/${name.replace(/^\//, "")}` });
      return {
        frame_seq: 0,
        timestamp_us: 0,
        rtc_epoch: 1764066000,
        dropped_frames: 0,
        width: dat.width,
        height: dat.height,
        min_raw: dat.min_raw,
        max_raw: dat.max_raw,
        min_x: 0,
        min_y: 0,
        max_x: 0,
        max_y: 0,
        spot_temp_c: dat.spot_temp ?? dat.spot_temp_c ?? 25.0,
        cal_offset: dat.cal_offset,
        cal_slope: dat.cal_slope,
        fpa_temp_c: 31.0,
        bat_pct: 100,
        usb_power: true,
        pixels: Uint16Array.from(dat.pixels),
      };
    }
    const found = simSdFiles.find((f) => f.name === name);
    return generateDemoFrame(found ? found.seq : 7);
  },

  async sdDownload(name: string): Promise<string> {
    if (isTauriRuntime()) {
      return tauriInvoke<string>("cmd_sd_download", {
        path: `/${name.replace(/^\//, "")}`,
        outputPath: null,
      });
    }
    return `~/Downloads/${name}`;
  },

  async sdDelete(name: string): Promise<void> {
    if (isTauriRuntime()) {
      await tauriInvoke<void>("cmd_sd_delete", { path: `/${name.replace(/^\//, "")}` });
      return;
    }
    simSdFiles = simSdFiles.filter((f) => f.name !== name);
  },

  async loadLocalDat(): Promise<{ path: string; frame: ThermalFrame }> {
    if (isTauriRuntime()) {
      const [path, dat] = await tauriInvoke<
        [
          string,
          {
            width: number;
            height: number;
            pixels: number[];
            min_raw: number;
            max_raw: number;
            cal_offset: number;
            cal_slope: number;
            spot_temp?: number;
            spot_temp_c?: number;
          },
          unknown,
        ]
      >("cmd_load_dat_file", { path: null });
      return {
        path,
        frame: {
          frame_seq: 0,
          timestamp_us: 0,
          rtc_epoch: 1764066000,
          dropped_frames: 0,
          width: dat.width,
          height: dat.height,
          min_raw: dat.min_raw,
          max_raw: dat.max_raw,
          min_x: 0,
          min_y: 0,
          max_x: 0,
          max_y: 0,
          spot_temp_c: dat.spot_temp ?? dat.spot_temp_c ?? 25.0,
          cal_offset: dat.cal_offset,
          cal_slope: dat.cal_slope,
          fpa_temp_c: 31.0,
          bat_pct: 100,
          usb_power: true,
          pixels: Uint16Array.from(dat.pixels),
        },
      };
    }
    return {
      path: "20261125_104512_BENCH_CAPTURE.DAT",
      frame: generateDemoFrame(19),
    };
  },

  async saveSidecar(
    datPath: string,
    sidecar: {
      schema_version: number;
      palette_id: string;
      radiometry: RadiometryParams;
      pipeline: DisplayPipeline;
      rois: Roi[];
      title: string;
      notes: string;
    },
  ): Promise<string> {
    if (isTauriRuntime()) {
      return tauriInvoke<string>("cmd_save_sidecar", { datPath, sidecar });
    }
    return datPath.replace(/\.DAT$/i, ".thermo.json");
  },

  async toggleRecording(active: boolean): Promise<number> {
    if (isTauriRuntime()) return tauriInvoke<number>("cmd_toggle_recording", { active });
    return active ? 0 : 27;
  },

  async exportMedia(req: {
    format: string;
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
    title?: string;
    notes?: string;
    pixels?: number[];
    width?: number;
    height?: number;
  }): Promise<string> {
    if (isTauriRuntime()) return tauriInvoke<string>("cmd_export_media", { req });
    return `~/Exports/thermocam_20261125.${req.format}`;
  },

  async pickHexFile(): Promise<{ path: string; text: string } | null> {
    if (isTauriRuntime()) {
      try {
        const [path, text] = await tauriInvoke<[string, string]>("cmd_pick_hex_file");
        return { path, text };
      } catch {
        return null;
      }
    }
    return null;
  },

  async verifyHex(hexText: string, expectedSha256?: string): Promise<FirmwareImageInfo> {
    if (isTauriRuntime()) {
      return tauriInvoke<FirmwareImageInfo>("cmd_verify_hex", {
        hexText,
        expectedSha256: expectedSha256 || null,
      });
    }
    const lines = hexText.split(/\r?\n/).filter((l) => l.startsWith(":"));
    if (lines.length === 0) throw new Error("No valid Intel HEX records found");
    const totalBytes = Math.max(16, (lines.length - 1) * 16);
    const digest = Array.from({ length: 64 }, (_, i) =>
      ((hexText.length * 17 + i * 13) % 16).toString(16),
    ).join("");
    if (expectedSha256 && expectedSha256.trim().toLowerCase() !== digest) {
      throw new Error("SHA-256 mismatch against expected digest");
    }
    return {
      sha256_hex: digest,
      total_bytes: totalBytes,
      block_count: Math.max(1, Math.ceil(totalBytes / 1024)),
      min_addr: 0,
      max_addr: totalBytes - 1,
    };
  },

  async flashFirmware(
    hexText: string,
    expectedSha256?: string,
    legacyPort?: string,
  ): Promise<string> {
    if (isTauriRuntime()) {
      return tauriInvoke<string>("cmd_flash_firmware", {
        hexText,
        expectedSha256: expectedSha256 || null,
        legacyPort: legacyPort || null,
      });
    }
    const info = await this.verifyHex(hexText, expectedSha256);
    return info.sha256_hex;
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

  async captureTimelapseFrame(args: {
    outputDir: string | null;
    paletteId: string;
    index: number;
    pixels: number[];
    width: number;
    height: number;
    rad: RadiometryParams;
  }): Promise<{ recorded_count: number; saved_path: string | null }> {
    if (isTauriRuntime()) {
      return tauriInvoke<{ recorded_count: number; saved_path: string | null }>(
        "cmd_capture_timelapse_frame",
        {
          outputDir: args.outputDir,
          paletteId: args.paletteId,
          index: args.index,
          pixels: args.pixels,
          width: args.width,
          height: args.height,
          rad: args.rad,
        },
      );
    }
    const fname = `timelapse_${String(args.index).padStart(6, "0")}.DAT`;
    return {
      recorded_count: args.index + 1,
      saved_path: args.outputDir ? `${args.outputDir}/${fname}` : null,
    };
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

  async onCameraDisconnected(cb: (reason: string) => void): Promise<() => void> {
    if (!isTauriRuntime()) return () => {};
    const { listen } = await import("@tauri-apps/api/event");
    return listen<{ reason?: string }>("camera-disconnected", (ev) => {
      cb(ev.payload?.reason ?? "USB camera disconnected");
    });
  },

  async onFlashProgress(
    cb: (ev: { stage: string; done?: number; total?: number; sha256?: string }) => void,
  ): Promise<() => void> {
    if (!isTauriRuntime()) return () => {};
    const { listen } = await import("@tauri-apps/api/event");
    return listen<{ stage: string; done?: number; total?: number; sha256?: string }>(
      "flash-progress",
      (ev) => cb(ev.payload),
    );
  },

  async onExportProgress(
    cb: (ev: { stage: string; done?: number; total?: number }) => void,
  ): Promise<() => void> {
    if (!isTauriRuntime()) return () => {};
    const { listen } = await import("@tauri-apps/api/event");
    return listen<{ stage: string; done?: number; total?: number }>("export-progress", (ev) =>
      cb(ev.payload),
    );
  },
};
