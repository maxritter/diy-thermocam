export type TempUnit = "celsius" | "fahrenheit" | "kelvin";
export type SpatialFilter = "none" | "box3x3" | "gaussian3x3" | "median3x3";
export type UpscaleMethod = "nearest" | "bicubic" | "lanczos3" | "edge_preserve";
export type IsothermMode = "off" | "above" | "below" | "interval";
export type AlarmCondition = "above" | "below";
export type AlarmAction = "notify" | "save_dat" | "start_recording";

export type StudioMode =
  | "live"
  | "measurements"
  | "alarms"
  | "export"
  | "sd"
  | "editor"
  | "control"
  | "updater";

export type RoiTool = "select" | "spot" | "spot3" | "box" | "circle" | "polygon" | "line" | "delta";

export interface Palette {
  id: string;
  label: string;
  category: "Firmware" | "Perceptual" | "Custom";
  firmware_id: number | null;
  rgb: [number, number, number][];
}

export interface MaterialPreset {
  id: string;
  name: string;
  category: string;
  emissivity: number;
}

export interface RadiometryParams {
  cal_offset: number;
  cal_slope: number;
  emissivity: number;
  reflected_temp_c: number;
}

export interface IsothermConfig {
  mode: IsothermMode;
  low_c: number;
  high_c: number;
  color: [number, number, number];
}

export interface DisplayPipeline {
  filter: SpatialFilter;
  upscaler: UpscaleMethod;
  sharpen_amount: number;
  rotate_180: boolean;
  flip_horizontal: boolean;
  isotherm: IsothermConfig;
}

export type RoiGeometry =
  | { kind: "spot"; x: number; y: number; kernel: number; show_raw: boolean }
  | { kind: "box"; x: number; y: number; w: number; h: number }
  | { kind: "circle"; cx: number; cy: number; radius: number }
  | { kind: "polygon"; points: [number, number][] }
  | { kind: "line"; x0: number; y0: number; x1: number; y1: number }
  | { kind: "delta"; x0: number; y0: number; x1: number; y1: number };

export interface Roi {
  id: string;
  label: string;
  geometry: RoiGeometry;
  emissivity_override: number | null;
  reflected_temp_override_c: number | null;
  visible: boolean;
}

export interface RoiStats {
  id: string;
  label: string;
  pixel_count: number;
  min_raw: number;
  max_raw: number;
  mean_raw: number;
  min_c: number;
  max_c: number;
  mean_c: number;
  std_dev_c: number;
  delta_c: number | null;
  min_xy: [number, number];
  max_xy: [number, number];
  effective_emissivity: number;
}

export interface LineSample {
  dist_px: number;
  x: number;
  y: number;
  raw: number;
  temp_c: number;
}

export interface TempSwitchRule {
  id: string;
  name: string;
  enabled: boolean;
  target_roi_id: string | null;
  condition: AlarmCondition;
  threshold_c: number;
  cooldown_ms: number;
  action: AlarmAction;
  last_triggered_ms: number | null;
}

export interface AlarmEvent {
  id: string;
  timestamp: string;
  rule_name: string;
  measured_c: number;
  threshold_c: number;
  action: AlarmAction;
}

export interface ThermalFrame {
  frame_seq: number;
  timestamp_us: number;
  rtc_epoch: number;
  dropped_frames: number;
  width: number;
  height: number;
  min_raw: number;
  max_raw: number;
  min_x: number;
  min_y: number;
  max_x: number;
  max_y: number;
  spot_temp_c: number;
  cal_offset: number;
  cal_slope: number;
  fpa_temp_c: number;
  bat_pct: number;
  usb_power: boolean;
  pixels: Uint16Array;
}

export interface SerialPortInfo {
  port_name: string;
  vid: number | null;
  pid: number | null;
  manufacturer: string | null;
  product: string | null;
  is_teensy: boolean;
}

export interface HelloResp {
  proto_major: number;
  proto_minor: number;
  fw_version: number;
  fw_build_str: string;
  hw_version: number;
  lepton_type: number;
  sensor_w: number;
  sensor_h: number;
  display_w: number;
  display_h: number;
  diagnostic_mask: number;
  teensy_uid: number;
  capabilities: number;
  max_chunk_size: number;
  heartbeat_timeout_ms: number;
}

export interface DeviceConfig {
  color_scheme: number;
  temp_format: number;
  filter_type: number;
  spot_enabled: boolean;
  colorbar_enabled: boolean;
  points_enabled: boolean;
  battery_enabled: boolean;
  time_enabled: boolean;
  date_enabled: boolean;
  storage_enabled: boolean;
  rotation_enabled: boolean;
  hot_cold_mode: number;
  gain_mode: number;
  min_max_mode: number;
  text_color: number;
  hot_cold_color: number;
  hot_cold_level: number;
  locked_min_raw: number;
  locked_max_raw: number;
  cal_offset: number;
  cal_slope: number;
}

export interface Diagnostics {
  diagnostic_mask: number;
  bat_pct: number;
  usb_power: boolean;
  sd_fs_type: number;
  bat_mv: number;
  cpu_mhz: number;
  mcu_temp_c: number;
  sd_total_kb: number;
  sd_free_kb: number;
  uptime_us: number;
  free_dtcm_bytes: number;
  free_ocram_bytes: number;
  lepton_fpa_temp_c: number;
  lepton_part_num: string;
}

export interface PerfTelemetry {
  acquire_us: number;
  stats_us: number;
  filter_us: number;
  scale_us: number;
  colorize_us: number;
  overlay_us: number;
  display_us: number;
  sd_write_us: number;
  usb_tx_us: number;
  loop_total_us: number;
  frame_count: number;
  frames_dropped: number;
  vospi_resyncs: number;
  vospi_crc_errors: number;
  usb_rx_errors: number;
  usb_crc_errors: number;
  live_fps_x100: number;
  video_fps_x100: number;
  stream_fps_x100: number;
  perf_mode: number;
  flags: number;
  display_spi_mhz: number;
  lepton_spi_mhz: number;
  cpu_mhz: number;
  mcu_temp_c: number;
  frame_period_us: number;
}

export interface RadParams {
  scene_emissivity: number;
  t_bkg_ck: number;
  tau_window: number;
  t_window_ck: number;
  tau_atm: number;
  t_atm_ck: number;
  refl_window: number;
  t_refl_ck: number;
  flags: number;
}

export interface FsEntry {
  name: string;
  size: number;
  mtime_epoch: number;
  is_dir: boolean;
}

export interface FsInfoResp {
  card_present: boolean;
  fs_type: number;
  total_kb: number;
  free_kb: number;
  root_entry_count: number;
}

export interface SdListing {
  info: FsInfoResp;
  entries: FsEntry[];
}

export interface FirmwareImageInfo {
  sha256_hex: string;
  total_bytes: number;
  block_count: number;
  min_addr: number;
  max_addr: number;
}

export type ExportResolution =
  | "native"
  | "hd720p"
  | "full_hd1080p"
  | "qhd1440p"
  | "uhd4k"
  | { custom: { width: number; height: number } };

export interface TimeSeriesPoint {
  tSec: number;
  values: Record<string, number>;
}
