import { Activity, Cpu, RefreshCw, SlidersHorizontal, Zap } from "lucide-react";
import { useEffect, useState } from "react";
import { MetricTile, StudioBadge, StudioSwitch } from "@/components/ui/Primitives";
import type { DeviceConfig, Diagnostics, PerfTelemetry, RadParams } from "@/lib/types";

const OVERLAY_SPOT = 1 << 0;
const OVERLAY_COLORBAR = 1 << 1;
const OVERLAY_BATTERY = 1 << 2;

function hasFlag(flags: number, mask: number): boolean {
  return (flags & mask) !== 0;
}

function withFlag(flags: number, mask: number, enabled: boolean): number {
  return enabled ? flags | mask : flags & ~mask;
}

interface CameraControlPanelProps {
  config: DeviceConfig | null;
  diagnostics: Diagnostics | null;
  perf: PerfTelemetry | null;
  radParams: RadParams | null;
  onRefreshTelemetry: () => Promise<void>;
  onTriggerFfc: (force: boolean) => Promise<void>;
  onSaveConfig: (cfg: DeviceConfig, persist: boolean) => Promise<void>;
  onSaveRadParams: (params: RadParams) => Promise<void>;
}

export function CameraControlPanel({
  config,
  diagnostics,
  perf,
  radParams,
  onRefreshTelemetry,
  onTriggerFfc,
  onSaveConfig,
  onSaveRadParams,
}: CameraControlPanelProps): React.JSX.Element {
  const [eps, setEps] = useState(() =>
    radParams ? Number((radParams.scene_emissivity / 8192).toFixed(2)) : 0.95,
  );
  const [tBkg, setTBkg] = useState(() =>
    radParams ? Number((radParams.t_bkg_ck / 100 - 273.15).toFixed(1)) : 22.0,
  );
  const [persistEeprom, setPersistEeprom] = useState(true);
  const [msg, setMsg] = useState<string | null>(null);

  useEffect(() => {
    if (!radParams) return;
    setEps(Number((radParams.scene_emissivity / 8192).toFixed(2)));
    setTBkg(Number((radParams.t_bkg_ck / 100 - 273.15).toFixed(1)));
  }, [radParams]);

  const pushRad = async (): Promise<void> => {
    const ck = Math.round((tBkg + 273.15) * 100);
    const next: RadParams = {
      scene_emissivity: Math.round(Math.min(1, Math.max(0.01, eps)) * 8192),
      t_bkg_ck: ck,
      tau_window: radParams?.tau_window ?? 8192,
      t_window_ck: ck,
      tau_atm: radParams?.tau_atm ?? 8192,
      t_atm_ck: ck,
      refl_window: 0,
      t_refl_ck: ck,
      flags: 1,
    };
    await onSaveRadParams(next);
    setMsg(`Updated Lepton CCI flux parameters (ε=${eps.toFixed(2)}, T_bkg=${tBkg.toFixed(1)} °C)`);
  };

  const stages = perf
    ? [
        ["VoSPI Acquire", perf.acquire_us, "var(--tc-accent)"],
        ["Stats", perf.stats_us, "#38bdf8"],
        ["Spatial Filter", perf.filter_us, "#6fd39f"],
        ["Scale 2×", perf.scale_us, "#a78bfa"],
        ["Colorize LUT", perf.colorize_us, "#f7b23b"],
        ["HUD Overlay", perf.overlay_us, "#fb923c"],
        ["TFT DMA", perf.display_us, "#f472b6"],
        ["USB Bulk TX", perf.usb_tx_us, "#2dd4bf"],
      ]
    : [];
  const totalStageUs = Math.max(1, perf?.loop_total_us ?? 1);

  return (
    <div className="grid h-full grid-cols-12 content-start gap-3 overflow-y-auto p-3">
      {/* Top KPI Row */}
      <div className="col-span-12 grid grid-cols-6 gap-2.5">
        <MetricTile
          label="Live FPS"
          value={perf ? `${(perf.live_fps_x100 / 100).toFixed(2)} Hz` : "—"}
          sub="Lepton 3.5 max 8.8 Hz"
          tone="pass"
        />
        <MetricTile
          label="Video / Stream"
          value={perf ? `${(perf.video_fps_x100 / 100).toFixed(2)} Hz` : "—"}
          sub={perf ? `USB ${(perf.stream_fps_x100 / 100).toFixed(2)} Hz` : ""}
          tone="accent"
        />
        <MetricTile
          label="VoSPI Resyncs"
          value={perf ? String(perf.vospi_resyncs) : "0"}
          sub={perf ? `CRC err: ${perf.vospi_crc_errors}` : ""}
          tone={perf && perf.vospi_resyncs > 0 ? "amber" : "pass"}
        />
        <MetricTile
          label="Loop Budget"
          value={perf ? `${(perf.loop_total_us / 1000).toFixed(2)} ms` : "—"}
          sub="113.9 ms frame slot"
        />
        <MetricTile
          label="Lepton FPA"
          value={
            diagnostics && Number.isFinite(diagnostics.lepton_fpa_temp_c)
              ? `${diagnostics.lepton_fpa_temp_c.toFixed(1)} °C`
              : "—"
          }
          sub={diagnostics ? `MCU ${diagnostics.mcu_temp_c.toFixed(1)} °C` : ""}
          tone="thermal"
        />
        <MetricTile
          label="Power & Clock"
          value={
            diagnostics
              ? `${diagnostics.bat_pct >= 0 ? `${diagnostics.bat_pct}%` : "USB"} · ${diagnostics.cpu_mhz} MHz`
              : "—"
          }
          sub={
            diagnostics
              ? diagnostics.bat_pct >= 0
                ? `${diagnostics.bat_mv} mV (${diagnostics.usb_power ? "USB" : "Bat"})`
                : "USB power only"
              : ""
          }
        />
      </div>

      {/* Left: Camera Control + Lepton CCI Radiometry */}
      <div className="tc-card col-span-6 space-y-4 p-4">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <SlidersHorizontal className="h-4 w-4 text-[var(--tc-accent)]" />
            <h2 className="text-[14px] font-semibold">Camera &amp; Lepton CCI Controls</h2>
          </div>
          <button
            type="button"
            onClick={() =>
              onTriggerFfc(true).then(() => setMsg("Forced Lepton FFC shutter calibration"))
            }
            className="tc-btn tc-btn-primary"
          >
            <Zap className="h-3.5 w-3.5" />
            Trigger FFC
          </button>
        </div>

        {/* Lepton CCI Flux Parameters */}
        <div className="rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)] p-3 space-y-2.5">
          <div className="flex items-center justify-between">
            <span className="tc-section-label">On-Sensor Lepton CCI Radiometry</span>
            <StudioBadge tone="accent">Q3.13 / cK</StudioBadge>
          </div>
          <div className="grid grid-cols-2 gap-2.5">
            <div>
              <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">
                Scene Emissivity (ε)
              </label>
              <input
                type="number"
                min={0.05}
                max={1.0}
                step={0.01}
                value={eps}
                onChange={(e) => setEps(Number(e.target.value))}
                className="tc-input w-full font-mono-tech"
              />
            </div>
            <div>
              <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">
                Background T_bkg (°C)
              </label>
              <input
                type="number"
                step={0.5}
                value={tBkg}
                onChange={(e) => setTBkg(Number(e.target.value))}
                className="tc-input w-full font-mono-tech"
              />
            </div>
          </div>
          <button type="button" onClick={pushRad} className="tc-btn w-full">
            Write Emissivity &amp; T_bkg to FLIR Lepton
          </button>
        </div>

        {/* Firmware Config Toggles */}
        {config ? (
          <div className="space-y-2.5">
            <div className="flex items-center justify-between">
              <span className="tc-section-label">Firmware Device Configuration</span>
              <StudioSwitch
                checked={persistEeprom}
                onCheckedChange={setPersistEeprom}
                label="Persist EEPROM"
              />
            </div>
            <div className="grid grid-cols-2 gap-2">
              <StudioSwitch
                checked={hasFlag(config.overlay_flags, OVERLAY_SPOT)}
                onCheckedChange={(v) =>
                  onSaveConfig(
                    { ...config, overlay_flags: withFlag(config.overlay_flags, OVERLAY_SPOT, v) },
                    persistEeprom,
                  )
                }
                label="On-LCD Spot Crosshair"
              />
              <StudioSwitch
                checked={hasFlag(config.overlay_flags, OVERLAY_COLORBAR)}
                onCheckedChange={(v) =>
                  onSaveConfig(
                    {
                      ...config,
                      overlay_flags: withFlag(config.overlay_flags, OVERLAY_COLORBAR, v),
                    },
                    persistEeprom,
                  )
                }
                label="On-LCD Colorbar"
              />
              <StudioSwitch
                checked={config.rotation_vert !== 0}
                onCheckedChange={(v) =>
                  onSaveConfig({ ...config, rotation_vert: v ? 1 : 0 }, persistEeprom)
                }
                label="Camera Rotate 180°"
              />
              <StudioSwitch
                checked={hasFlag(config.overlay_flags, OVERLAY_BATTERY)}
                onCheckedChange={(v) =>
                  onSaveConfig(
                    {
                      ...config,
                      overlay_flags: withFlag(config.overlay_flags, OVERLAY_BATTERY, v),
                    },
                    persistEeprom,
                  )
                }
                label="On-LCD Battery HUD"
              />
            </div>
            <div className="flex items-center justify-between pt-1">
              <span className="text-[12px] text-[var(--tc-text-muted)]">Lepton Gain Mode</span>
              <select
                value={config.lepton_gain_mode}
                onChange={(e) =>
                  onSaveConfig(
                    { ...config, lepton_gain_mode: Number(e.target.value) },
                    persistEeprom,
                  )
                }
                className="tc-select w-48"
              >
                <option value={0}>High Gain (-10..+140 °C)</option>
                <option value={1}>Low Gain (-10..+450 °C)</option>
              </select>
            </div>
          </div>
        ) : null}

        {msg ? (
          <div className="rounded-lg border border-[var(--tc-pass-line)] bg-[var(--tc-pass-soft)] px-3 py-2 font-mono-tech text-[11.5px] text-[var(--tc-pass)]">
            {msg}
          </div>
        ) : null}
      </div>

      {/* Right: Live Firmware Pipeline Telemetry & Diagnostics */}
      <div className="tc-card col-span-6 space-y-4 p-4">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Activity className="h-4 w-4 text-[var(--tc-pass)]" />
            <h2 className="text-[14px] font-semibold">
              Firmware Pipeline Timing &amp; Diagnostics
            </h2>
          </div>
          <button type="button" onClick={onRefreshTelemetry} className="tc-btn">
            <RefreshCw className="h-3.5 w-3.5" />
            Poll Now
          </button>
        </div>

        {/* Stacked Microsecond Bar Breakdown */}
        <div className="space-y-2">
          {stages.map(([label, us, color]) => {
            const pct = Math.min(100, (Number(us) / totalStageUs) * 100);
            return (
              <div key={String(label)} className="space-y-0.5">
                <div className="flex justify-between text-[11.5px]">
                  <span className="text-[var(--tc-text-muted)]">{label}</span>
                  <span className="font-mono-tech">
                    {us} μs ({pct.toFixed(1)}%)
                  </span>
                </div>
                <div className="h-2 w-full overflow-hidden rounded-full bg-[var(--tc-surface-2)]">
                  <div
                    className="h-full rounded-full"
                    style={{ width: `${Math.max(2, pct)}%`, backgroundColor: String(color) }}
                  />
                </div>
              </div>
            );
          })}
        </div>

        {/* Hardware Diagnostics Table */}
        {diagnostics ? (
          <div className="rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)] p-3 font-mono-tech text-[11.5px]">
            <div className="mb-2 flex items-center gap-1.5 font-sans text-[11px] font-semibold uppercase tracking-wider text-[var(--tc-text-dim)]">
              <Cpu className="h-3.5 w-3.5" />
              Hardware Health
            </div>
            <div className="grid grid-cols-2 gap-y-1.5">
              <span className="text-[var(--tc-text-muted)]">Lepton Module</span>
              <span className="text-right">{diagnostics.lepton_part_num}</span>
              <span className="text-[var(--tc-text-muted)]">SPI Clocks (TFT / Lepton)</span>
              <span className="text-right">
                {perf?.display_spi_mhz ?? 40} MHz / {perf?.lepton_spi_mhz ?? 20} MHz
              </span>
              <span className="text-[var(--tc-text-muted)]">Free DTCM / OCRAM</span>
              <span className="text-right">
                {Math.round(diagnostics.free_dtcm_bytes / 1024)} KB /{" "}
                {Math.round(diagnostics.free_ocram_bytes / 1024)} KB
              </span>
              <span className="text-[var(--tc-text-muted)]">Diagnostic Mask</span>
              <span className="text-right text-[var(--tc-pass)]">
                0x{diagnostics.diagnostic_mask.toString(16).toUpperCase()} (ALL OK)
              </span>
            </div>
          </div>
        ) : null}
      </div>
    </div>
  );
}
