import {
  ArrowUpRight,
  Ban,
  BatteryMedium,
  BookOpen,
  Bug,
  Camera,
  CircleDot,
  CircuitBoard,
  Crosshair,
  Download,
  Flame,
  FlipVertical2,
  Gauge,
  HardDrive,
  type LucideIcon,
  Microscope,
  MousePointerClick,
  Ruler,
  ScanEye,
  ShieldCheck,
  Thermometer,
  Unplug,
  Upload,
  Usb,
  Zap,
} from "lucide-react";
import { useCallback, useEffect, useState, useSyncExternalStore } from "react";
import type { CameraState } from "@/app/CameraChip";
import { GroupCard } from "@/app/ui/GroupCard";
import { Segmented } from "@/app/ui/Segmented";
import { SettingRow } from "@/app/ui/SettingRow";
import { Switch } from "@/app/ui/Switch";
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

/** One fact on the status card: a muted icon, its name and the value on the right. */
function Fact({
  icon: Icon,
  name,
  children,
  tone,
}: {
  icon: LucideIcon;
  name: string;
  children: React.ReactNode;
  tone?: "pass" | "fail";
}): React.JSX.Element {
  return (
    <div className="cm-fact">
      <dt>
        <Icon size={18} strokeWidth={1.75} aria-hidden="true" className="cm-fact-ic" />
        {name}
      </dt>
      <dd className={tone}>{children}</dd>
    </div>
  );
}

/** A Help row: an icon, the page it opens and the outward arrow. */
function HelpLink({
  icon: Icon,
  label,
  url,
}: {
  icon: LucideIcon;
  label: string;
  url: string;
}): React.JSX.Element {
  return (
    <button
      type="button"
      className="cm-linkrow"
      onClick={() => void cameraApi.openUrl(url).catch(() => {})}
    >
      <Icon size={18} strokeWidth={1.75} aria-hidden="true" className="cm-fact-ic" />
      <span>{label}</span>
      <ArrowUpRight size={14} strokeWidth={1.75} aria-hidden="true" className="cm-linkrow-go" />
    </button>
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
        <div className="cm-cam one">
          <header className="st-page-head cm-head">
            <h1>Camera</h1>
            <p>Status and settings of the connected Thermocam.</p>
          </header>
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
        <header className="st-page-head cm-head">
          <h1>Camera</h1>
          <p>Settings go to the camera at once and stay after it restarts.</p>
        </header>
        <div className="cm-col">
          <GroupCard
            title={demo ? "Demo camera" : "Thermocam V4"}
            aside={
              demo ? (
                <span className="st-chip">
                  <span className="st-status-dot demo" />
                  Read-only
                </span>
              ) : (
                <span className="st-chip">
                  <span className="st-status-dot" />
                  Connected
                </span>
              )
            }
          >
            <dl className="cm-facts">
              <Fact icon={Usb} name="Connection">
                {demo ? "Simulated" : `USB · ${shortPortName(port ?? "")}`}
              </Fact>
              <Fact icon={BatteryMedium} name="Battery">
                {battery !== null ? (
                  <span className="cm-batt">
                    <span className="cm-meter" aria-hidden="true">
                      <i
                        style={{
                          width: `${battery}%`,
                          background: battery < 20 ? "var(--tc-amber)" : undefined,
                        }}
                      />
                    </span>
                    {battery}%
                  </span>
                ) : diagnostics ? (
                  "USB power"
                ) : (
                  "–"
                )}
              </Fact>
              <Fact icon={HardDrive} name="SD card">
                {sdText(diagnostics)}
              </Fact>
              <Fact icon={ScanEye} name="Sensor">
                {sensorName(hello)}
              </Fact>
              <Fact icon={Gauge} name="Frame rate">
                {perf ? `${(perf.live_fps_x100 / 100).toFixed(1)} fps` : "–"}
              </Fact>
              <Fact icon={ShieldCheck} name="Self-test" tone={selfTestOk ? "pass" : "fail"}>
                {selfTestText(mask)}
              </Fact>
              <Fact icon={CircuitBoard} name="Firmware">
                {hello?.fw_build_str ?? "–"}
              </Fact>
            </dl>
            <div className="cm-card-f">
              <button
                type="button"
                className="tc-btn"
                disabled={firmwareBusy(fw)}
                onClick={() => void chooseFirmwareFile()}
              >
                <Upload size={15} strokeWidth={1.75} aria-hidden="true" />
                Update firmware…
              </button>
              <button type="button" className="tc-btn tc-btn-ghost" onClick={onDisconnect}>
                <Unplug size={15} strokeWidth={1.75} aria-hidden="true" />
                Disconnect
              </button>
            </div>
          </GroupCard>
          <GroupCard title="Help">
            <HelpLink icon={BookOpen} label="User manual" url={LINKS.manual} />
            <HelpLink icon={Download} label="Firmware downloads" url={LINKS.firmware} />
            <HelpLink icon={Bug} label="Report a problem on GitHub" url={LINKS.issues} />
          </GroupCard>
        </div>

        {/* biome-ignore lint/a11y/useSemanticElements: a named group of cards; <fieldset> would add form semantics. */}
        <div className="cm-col" role="group" aria-label="Camera settings">
          <GroupCard title="On the camera’s screen">
            <SettingRow
              icon={Crosshair}
              title="Spot temperature"
              desc="Crosshair with the reading in the center"
              error={errors.spot}
              control={
                <Switch
                  label="Spot temperature"
                  checked={cfg ? hasOverlay(cfg, OVERLAY_SPOT) : false}
                  disabled={locked}
                  onChange={(on) => overlay("spot", OVERLAY_SPOT, on)}
                />
              }
            />
            <SettingRow
              icon={Ruler}
              title="Temperature scale"
              desc="Color bar along the edge"
              error={errors.colorbar}
              control={
                <Switch
                  label="Temperature scale"
                  checked={cfg ? hasOverlay(cfg, OVERLAY_COLORBAR) : false}
                  disabled={locked}
                  onChange={(on) => overlay("colorbar", OVERLAY_COLORBAR, on)}
                />
              }
            />
            <SettingRow
              icon={BatteryMedium}
              title="Battery level"
              desc="Shown in the corner"
              error={errors.battery}
              control={
                <Switch
                  label="Battery level"
                  checked={cfg ? hasOverlay(cfg, OVERLAY_BATTERY) : false}
                  disabled={locked}
                  onChange={(on) => overlay("battery", OVERLAY_BATTERY, on)}
                />
              }
            />
            <MaterialSwitch
              checked={materialOnCamera}
              onCheckedChange={onMaterialOnCameraChange}
              liveMaterial={liveMaterial}
              unit={unit}
              disabled={demo}
            />
          </GroupCard>

          <GroupCard title="Image">
            <SettingRow
              icon={Thermometer}
              title="Measuring range"
              desc="Wide reaches hotter objects with less detail. Switching recalibrates the image."
              error={errors.range}
              control={
                <Segmented<number>
                  label="Measuring range"
                  value={cfg ? cfg.lepton_gain_mode : null}
                  disabled={locked}
                  options={[
                    {
                      id: GAIN_HIGH,
                      label: rangeLabel(-10, 140, unit),
                      icon: Microscope,
                      title: "More detail",
                    },
                    {
                      id: GAIN_LOW,
                      label: rangeLabel(-10, 450, unit),
                      icon: Flame,
                      title: "Reaches hotter objects",
                    },
                  ]}
                  onChange={(g) => void controller.setRange(g)}
                />
              }
            />
            <SettingRow
              icon={FlipVertical2}
              title="Mounted upside down"
              desc="Turns the image on the camera and in Studio"
              error={errors.upside_down}
              control={
                <Switch
                  label="Mounted upside down"
                  checked={cfg ? cfg.rotation_vert !== 0 : false}
                  disabled={locked}
                  onChange={(on) =>
                    void controller.toggle("upside_down", { rotation_vert: on ? 1 : 0 })
                  }
                />
              }
            />
            <SettingRow
              icon={Zap}
              title="Calibrate"
              desc="Closes the shutter to even out the image. The camera also does this by itself."
              error={errors.calibrate}
              control={
                <button
                  type="button"
                  className="tc-btn"
                  disabled={locked || settings.busy.includes("calibrate")}
                  onClick={() => void controller.calibrate()}
                >
                  Calibrate now
                </button>
              }
            />
          </GroupCard>

          <GroupCard title="Capture button">
            <SettingRow
              icon={MousePointerClick}
              title="When pressed"
              desc="While Studio is connected. Without Studio, the camera saves on its card."
              control={
                <Segmented<ButtonAction>
                  label="When pressed"
                  value={buttonAction}
                  options={[
                    { id: "snapshot", label: "Snapshot", icon: Camera },
                    { id: "record", label: "Record", icon: CircleDot },
                    { id: "nothing", label: "Nothing", icon: Ban },
                  ]}
                  onChange={onButtonActionChange}
                />
              }
            />
          </GroupCard>

          <GroupCard title="Diagnostics" aside="Frame timing, link and hardware" collapsible>
            <Diagnostics
              diagnostics={diagnostics}
              perf={perf}
              unit={unit}
              onRefresh={() => void refreshTelemetry()}
              reportText={() => reportText({ hello, port, config: cfg, diagnostics, perf })}
            />
          </GroupCard>
        </div>
      </div>
      {dialog}
    </div>
  );
}
