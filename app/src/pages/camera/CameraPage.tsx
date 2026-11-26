import { ChevronDown, Upload, Zap } from "lucide-react";
import { useCallback, useEffect, useState, useSyncExternalStore } from "react";
import type { CameraState } from "@/app/CameraChip";
import { cameraApi, LINKS } from "@/lib/api/camera";
import { getSensorMaterial, useMaterialSync } from "@/lib/sensorMaterial";
import type { ButtonAction } from "@/lib/settings";
import { fromCelsius, unitSymbol } from "@/lib/thermal";
import type {
  DeviceConfig,
  Diagnostics as DiagnosticsData,
  HelloResp,
  Material,
  PerfTelemetry,
  TempUnit,
} from "@/lib/types";
import { Diagnostics, selfTestFailures, selfTestText } from "./Diagnostics";
import {
  chooseFirmwareFile,
  FirmwareDialog,
  firmwareBusy,
  useFirmwareFlow,
} from "./FirmwareDialog";
import { MaterialSwitch } from "./MaterialSwitch";
import { NoCamera } from "./NoCamera";
import {
  GAIN_HIGH,
  GAIN_LOW,
  hasOverlay,
  OVERLAY_BATTERY,
  OVERLAY_COLORBAR,
  OVERLAY_SPOT,
  SettingsController,
  withOverlay,
} from "./settingsRules";
import "./camera.css";

export interface CameraPageProps {
  state: CameraState;
  /** The connected port (`/dev/cu.usbmodem…`, `sim://…`), or null. */
  port: string | null;
  hello: HelloResp | null;
  unit: TempUnit;
  /** Studio's own setting: what the camera's button does while Studio is connected. */
  buttonAction: ButtonAction;
  onButtonActionChange: (action: ButtonAction) => void;
  /** The material switch as Studio's settings remember it (off by default). */
  materialOnCamera: boolean;
  onMaterialOnCameraChange: (on: boolean) => void;
  /** Live's image-wide material. */
  liveMaterial: Material;
  onDisconnect: () => void;
  onUseDemo: () => void;
  /** Connects to a port again (after a firmware update restarted the camera). */
  onReconnect: (port: string) => Promise<void>;
}

const POLL_MS = 5000;

/**
 * Keeps the sensor material in step on every page: reads `GET_RAD_PARAMS` on connect and, while
 * the material switch is on, writes Live's material when it changes. Mount once, always.
 */
export function CameraSync(props: {
  connectionId: string | null;
  switchOn: boolean;
  material: Material;
}): null {
  useMaterialSync(props);
  return null;
}

function shortPortName(port: string): string {
  return port.replace(/^\/dev\/(cu|tty)\./, "").replace(/^\/dev\//, "");
}

function sensorName(hello: HelloResp | null): string {
  if (!hello) return "–";
  const name =
    hello.lepton_type === 0
      ? "FLIR Lepton 2.5"
      : hello.lepton_type === 1
        ? "FLIR Lepton 3.5"
        : "No sensor";
  return hello.lepton_type === 255 ? name : `${name} · ${hello.sensor_w} × ${hello.sensor_h}`;
}

function fsName(t: number): string {
  return t === 1 ? "FAT16" : t === 2 ? "FAT32" : t === 3 ? "exFAT" : "";
}

function sdText(d: DiagnosticsData | null): string {
  if (!d) return "–";
  if (d.sd_total_kb <= 0) return "No card";
  const gb = (kb: number) => (kb / 1048576).toFixed(1);
  const fs = fsName(d.sd_fs_type);
  return `${gb(d.sd_free_kb)} GB free of ${gb(d.sd_total_kb)} GB${fs ? ` · ${fs}` : ""}`;
}

/** "−10 to 140 °C" in the person's unit. */
function rangeLabel(lowC: number, highC: number, unit: TempUnit): string {
  const f = (c: number) => {
    const v = Math.round(fromCelsius(c, unit));
    return v < 0 ? `−${Math.abs(v)}` : String(v);
  };
  return `${f(lowC)} to ${f(highC)} ${unitSymbol(unit)}`;
}

interface SwitchRowProps {
  name: string;
  desc: string;
  ariaLabel: string;
  checked: boolean;
  disabled: boolean;
  error?: string;
  onChange: (on: boolean) => void;
}

function SwitchRow(p: SwitchRowProps): React.JSX.Element {
  return (
    <div className="cm-set">
      <span className="n">{p.name}</span>
      <span className="d">{p.desc}</span>
      {p.error ? (
        <span className="err" role="alert">
          {p.error}
        </span>
      ) : null}
      <label className="st-switch ctl">
        <input
          type="checkbox"
          checked={p.checked}
          disabled={p.disabled}
          aria-label={p.ariaLabel}
          onChange={(e) => p.onChange(e.target.checked)}
        />
        <span className="track" />
      </label>
    </div>
  );
}

interface SegRowProps<T extends string | number> {
  name: string;
  desc: string;
  ariaLabel: string;
  value: T | null;
  options: { value: T; label: string }[];
  disabled: boolean;
  error?: string;
  onChange: (v: T) => void;
}

function SegRow<T extends string | number>(p: SegRowProps<T>): React.JSX.Element {
  return (
    <div className="cm-set">
      <span className="n">{p.name}</span>
      <span className="d">{p.desc}</span>
      {p.error ? (
        <span className="err" role="alert">
          {p.error}
        </span>
      ) : null}
      <fieldset className="st-seg ctl" aria-label={p.ariaLabel}>
        {p.options.map((o) => (
          <button
            key={String(o.value)}
            type="button"
            aria-pressed={p.value === o.value}
            disabled={p.disabled}
            onClick={() => p.value !== o.value && p.onChange(o.value)}
          >
            {o.label}
          </button>
        ))}
      </fieldset>
    </div>
  );
}

function reportText(args: {
  hello: HelloResp | null;
  port: string | null;
  config: DeviceConfig | null;
  diagnostics: DiagnosticsData | null;
  perf: PerfTelemetry | null;
}): string {
  const sensor = getSensorMaterial();
  return [
    "DIY-Thermocam Studio · camera diagnostics",
    `Firmware: ${args.hello?.fw_build_str ?? "unknown"} (protocol ${args.hello?.proto_major ?? "?"}.${args.hello?.proto_minor ?? "?"}, hw ${args.hello?.hw_version ?? "?"})`,
    `Port: ${args.port ?? "none"}`,
    `Sensor: ${sensorName(args.hello)}`,
    `Sensor material: ${sensor ? `emissivity ${sensor.emissivity}, reflected ${sensor.reflected_c} °C` : "factory"}`,
    `Config: ${JSON.stringify(args.config)}`,
    `Diagnostics: ${JSON.stringify(args.diagnostics)}`,
    `Perf: ${JSON.stringify(args.perf)}`,
  ].join("\n");
}

/** `camera.html#camera`: status on the left, settings on the right; No camera without one. */
export function CameraPage({
  state,
  port,
  hello,
  unit,
  buttonAction,
  onButtonActionChange,
  materialOnCamera,
  onMaterialOnCameraChange,
  liveMaterial,
  onDisconnect,
  onUseDemo,
  onReconnect,
}: CameraPageProps): React.JSX.Element {
  const connected = state === "camera" || state === "demo";
  const demo = state === "demo";
  const [controller] = useState(() => new SettingsController(cameraApi));
  const settings = useSyncExternalStore(
    (cb) => controller.subscribe(cb),
    () => controller.getState(),
    () => controller.getState(),
  );
  const [diagnostics, setDiagnostics] = useState<DiagnosticsData | null>(null);
  const [perf, setPerf] = useState<PerfTelemetry | null>(null);
  const fw = useFirmwareFlow();
  const updating = firmwareBusy(fw) || fw.view.name !== "closed";

  const refreshTelemetry = useCallback(async () => {
    const [d, p] = await Promise.allSettled([cameraApi.getDiagnostics(), cameraApi.getPerf()]);
    if (d.status === "fulfilled") setDiagnostics(d.value);
    if (p.status === "fulfilled") setPerf(p.value);
  }, []);

  // The camera's settings on connect; telemetry while this page shows, paused during an update.
  useEffect(() => {
    if (!connected || !port) {
      controller.load(null);
      setDiagnostics(null);
      setPerf(null);
      return;
    }
    let active = true;
    void cameraApi
      .getConfig()
      .then((cfg) => active && controller.load(cfg))
      .catch(() => active && controller.load(null));
    return () => {
      active = false;
    };
  }, [connected, controller, port]);

  useEffect(() => {
    if (!connected || !port || updating) return;
    void refreshTelemetry();
    const id = setInterval(() => void refreshTelemetry(), POLL_MS);
    return () => clearInterval(id);
  }, [connected, updating, refreshTelemetry, port]);

  const cameraLabel = connected ? (demo ? "Demo camera" : "Thermocam V4") : null;
  const dialog = (
    <FirmwareDialog hello={hello} cameraLabel={cameraLabel} port={port} onReconnect={onReconnect} />
  );

  if (!connected) {
    return (
      <div className="cm-page">
        <div className="cm-cam" style={{ gridTemplateColumns: "minmax(0, 1fr)" }}>
          <NoCamera
            connecting={state === "connecting"}
            onUpdateFirmware={() => void chooseFirmwareFile()}
            onUseDemo={onUseDemo}
            updateBusy={firmwareBusy(fw)}
          />
        </div>
        {dialog}
      </div>
    );
  }

  const cfg = settings.shown;
  const locked = demo || !cfg;
  const errors = settings.errors;
  const mask = diagnostics?.diagnostic_mask ?? hello?.diagnostic_mask ?? 0xff;
  const selfTestOk = selfTestFailures(mask).length === 0;
  const battery = diagnostics && diagnostics.bat_pct >= 0 ? diagnostics.bat_pct : null;

  const overlay = (key: string, bit: number, on: boolean) =>
    void controller.toggle(key, (c) => withOverlay(c, bit, on));

  return (
    <div className="cm-page">
      <div className="cm-cam">
        <div className="cm-col">
          <section className="cm-panel" aria-labelledby="cm-dev-t">
            <div className="cm-panel-h">
              <h2 id="cm-dev-t">{demo ? "Demo camera" : "Thermocam V4"}</h2>
              {demo ? (
                <span className="cm-badge amber">Read-only</span>
              ) : (
                <span className="cm-badge pass">
                  <span className="st-status-dot" />
                  Connected
                </span>
              )}
            </div>
            <dl className="cm-kv">
              <dt>Connection</dt>
              <dd>{demo ? "Simulated" : `USB · ${shortPortName(port ?? "")}`}</dd>
              <dt>Battery</dt>
              <dd
                style={{
                  display: "flex",
                  justifyContent: "flex-end",
                  alignItems: "center",
                  gap: 8,
                }}
              >
                {battery !== null ? (
                  <>
                    <span className="cm-meter" aria-hidden="true">
                      <i
                        style={{
                          width: `${battery}%`,
                          background: battery < 20 ? "var(--tc-amber)" : undefined,
                        }}
                      />
                    </span>
                    {battery}%
                  </>
                ) : diagnostics ? (
                  "USB power"
                ) : (
                  "–"
                )}
              </dd>
              <dt>SD card</dt>
              <dd>{sdText(diagnostics)}</dd>
              <dt>Sensor</dt>
              <dd>{sensorName(hello)}</dd>
              <dt>Frame rate</dt>
              <dd>{perf ? `${(perf.live_fps_x100 / 100).toFixed(1)} fps` : "–"}</dd>
              <dt>Self-test</dt>
              <dd className={selfTestOk ? "pass" : "fail"}>{selfTestText(mask)}</dd>
              <dt>Firmware</dt>
              <dd>{hello?.fw_build_str ?? "–"}</dd>
            </dl>
            <div className="cm-actions">
              <button
                type="button"
                className="tc-btn"
                disabled={firmwareBusy(fw)}
                onClick={() => void chooseFirmwareFile()}
              >
                <Upload size={16} strokeWidth={1.75} aria-hidden="true" />
                Update firmware…
              </button>
              <button type="button" className="tc-btn tc-btn-ghost" onClick={onDisconnect}>
                Disconnect
              </button>
            </div>
          </section>
          <section className="cm-panel" aria-labelledby="cm-help-t">
            <div className="cm-panel-h">
              <h2 id="cm-help-t" style={{ fontSize: 13 }}>
                Help
              </h2>
            </div>
            <div className="cm-help">
              <button
                type="button"
                className="cm-link"
                onClick={() => void cameraApi.openUrl(LINKS.manual).catch(() => {})}
              >
                User manual
              </button>
              <button
                type="button"
                className="cm-link"
                onClick={() => void cameraApi.openUrl(LINKS.firmware).catch(() => {})}
              >
                Firmware downloads
              </button>
              <button
                type="button"
                className="cm-link"
                onClick={() => void cameraApi.openUrl(LINKS.issues).catch(() => {})}
              >
                Report a problem on GitHub
              </button>
            </div>
          </section>
          <p className="cm-hint" style={{ padding: "0 4px" }}>
            Settings are sent to the camera right away and kept after it restarts. Number fields
            apply when you press Enter or leave them; a setting the camera refuses flips back and
            says why.
          </p>
        </div>

        <section className="cm-panel" aria-label="Camera settings" style={{ alignSelf: "start" }}>
          <div className="cm-sec-h">On the camera’s screen</div>
          <div className="cm-set-list">
            <SwitchRow
              name="Spot temperature"
              desc="Crosshair with the reading in the center"
              ariaLabel="Spot temperature on the camera screen"
              checked={cfg ? hasOverlay(cfg, OVERLAY_SPOT) : false}
              disabled={locked}
              error={errors.spot}
              onChange={(on) => overlay("spot", OVERLAY_SPOT, on)}
            />
            <SwitchRow
              name="Temperature scale"
              desc="Color bar along the edge"
              ariaLabel="Temperature scale on the camera screen"
              checked={cfg ? hasOverlay(cfg, OVERLAY_COLORBAR) : false}
              disabled={locked}
              error={errors.colorbar}
              onChange={(on) => overlay("colorbar", OVERLAY_COLORBAR, on)}
            />
            <SwitchRow
              name="Battery level"
              desc="Shown in the corner"
              ariaLabel="Battery level on the camera screen"
              checked={cfg ? hasOverlay(cfg, OVERLAY_BATTERY) : false}
              disabled={locked}
              error={errors.battery}
              onChange={(on) => overlay("battery", OVERLAY_BATTERY, on)}
            />
          </div>
          <div className="cm-sec-h">Image</div>
          <div className="cm-set-list">
            <SegRow
              name="Measuring range"
              desc="The wide range reaches hot objects but shows less detail. Switching recalibrates the image for a moment."
              ariaLabel="Measuring range"
              value={cfg ? cfg.lepton_gain_mode : null}
              options={[
                { value: GAIN_HIGH, label: rangeLabel(-10, 140, unit) },
                { value: GAIN_LOW, label: rangeLabel(-10, 450, unit) },
              ]}
              disabled={locked}
              error={errors.range}
              onChange={(g) => void controller.setRange(g)}
            />
            <SwitchRow
              name="Mounted upside down"
              desc="Turns the image on the camera and in Studio"
              ariaLabel="Mounted upside down"
              checked={cfg ? cfg.rotation_vert !== 0 : false}
              disabled={locked}
              error={errors.upside_down}
              onChange={(on) =>
                void controller.toggle("upside_down", { rotation_vert: on ? 1 : 0 })
              }
            />
            <MaterialSwitch
              checked={materialOnCamera}
              onCheckedChange={onMaterialOnCameraChange}
              liveMaterial={liveMaterial}
              unit={unit}
              disabled={demo}
            />
            <div className="cm-set">
              <span className="n">Calibrate</span>
              <span className="d">
                The shutter closes for a moment to even out the image. The camera also does this by
                itself.
              </span>
              {errors.calibrate ? (
                <span className="err" role="alert">
                  {errors.calibrate}
                </span>
              ) : null}
              <button
                type="button"
                className="tc-btn ctl"
                disabled={locked || settings.busy.includes("calibrate")}
                onClick={() => void controller.calibrate()}
              >
                <Zap size={16} strokeWidth={1.75} aria-hidden="true" />
                Calibrate now
              </button>
            </div>
          </div>
          <div className="cm-sec-h">Capture button</div>
          <div className="cm-set-list">
            <SegRow<ButtonAction>
              name="When you press the button on the camera"
              desc="While Studio is connected. Without Studio, the button saves on the camera."
              ariaLabel="Capture button"
              value={buttonAction}
              options={[
                { value: "snapshot", label: "Snapshot in Studio" },
                { value: "record", label: "Record in Studio" },
                { value: "nothing", label: "Nothing" },
              ]}
              disabled={false}
              onChange={onButtonActionChange}
            />
          </div>
          <details className="cm-more">
            <summary>
              Diagnostics<span className="aside">Frame timing, link health and sensor details</span>
              <ChevronDown size={14} strokeWidth={1.75} aria-hidden="true" />
            </summary>
            <Diagnostics
              diagnostics={diagnostics}
              perf={perf}
              unit={unit}
              onRefresh={() => void refreshTelemetry()}
              reportText={() => reportText({ hello, port, config: cfg, diagnostics, perf })}
            />
          </details>
        </section>
      </div>
      {dialog}
    </div>
  );
}
