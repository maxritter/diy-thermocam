import {
  Activity,
  BellRing,
  Camera,
  Circle,
  CircleDot,
  Cpu,
  Crosshair,
  Flame,
  HardDrive,
  Image,
  LineChart,
  Minus,
  Moon,
  MousePointer2,
  Pause,
  Pentagon,
  Play,
  Plus,
  Radio,
  Search,
  SlidersHorizontal,
  Square,
  Sun,
  Triangle,
  Usb,
  ZoomIn,
  ZoomOut,
} from "lucide-react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { BrandMark } from "@/components/BrandMark";
import { ColorbarHistogram } from "@/components/ColorbarHistogram";
import { AlarmsPanel } from "@/components/panels/AlarmsPanel";
import { CameraControlPanel } from "@/components/panels/CameraControlPanel";
import { CaptureExportPanel } from "@/components/panels/CaptureExportPanel";
import { EditorPanel } from "@/components/panels/EditorPanel";
import { InspectorPanel } from "@/components/panels/InspectorPanel";
import { MeasurementsPanel } from "@/components/panels/MeasurementsPanel";
import { SdBrowserPanel } from "@/components/panels/SdBrowserPanel";
import { UpdaterPanel } from "@/components/panels/UpdaterPanel";
import { ThermalViewport } from "@/components/ThermalViewport";
import { backend } from "@/lib/backend";
import { DEFAULT_MATERIALS, DEFAULT_PALETTES } from "@/lib/palettes";
import {
  evaluateRoi,
  formatTemp,
  generateDemoFrame,
  rawToObjectC,
  roiContrastRange,
} from "@/lib/thermal";
import type {
  AlarmEvent,
  DeviceConfig,
  Diagnostics,
  DisplayPipeline,
  ExportResolution,
  HelloResp,
  MaterialPreset,
  Palette,
  PerfTelemetry,
  RadiometryParams,
  RadParams,
  Roi,
  RoiTool,
  SdListing,
  SerialPortInfo,
  StudioMode,
  TempSwitchRule,
  TempUnit,
  ThermalFrame,
  TimeSeriesPoint,
} from "@/lib/types";

const INITIAL_ROIS: Roi[] = [
  {
    id: "roi_mcu",
    label: "MCU",
    geometry: { kind: "spot", x: 61, y: 58, kernel: 3, show_raw: false },
    emissivity_override: 0.94,
    reflected_temp_override_c: null,
    visible: true,
  },
  {
    id: "roi_vrm",
    label: "VRM",
    geometry: { kind: "box", x: 96, y: 30, w: 26, h: 24 },
    emissivity_override: 0.91,
    reflected_temp_override_c: null,
    visible: true,
  },
  {
    id: "roi_profile",
    label: "Trace",
    geometry: { kind: "line", x0: 26, y0: 42, x1: 138, y1: 30 },
    emissivity_override: null,
    reflected_temp_override_c: null,
    visible: true,
  },
  {
    id: "roi_delta",
    label: "Reg-Fin",
    geometry: { kind: "delta", x0: 24, y0: 84, x1: 115, y1: 86 },
    emissivity_override: null,
    reflected_temp_override_c: null,
    visible: true,
  },
];

const INITIAL_ALARMS: TempSwitchRule[] = [
  {
    id: "alarm_mcu",
    name: "MCU Thermal Throttling Limit",
    enabled: true,
    target_roi_id: "roi_mcu",
    condition: "above",
    threshold_c: 64.5,
    cooldown_ms: 8000,
    action: "notify",
    last_triggered_ms: null,
  },
  {
    id: "alarm_vrm",
    name: "VRM Over-Temp Auto-Capture",
    enabled: false,
    target_roi_id: "roi_vrm",
    condition: "above",
    threshold_c: 55.0,
    cooldown_ms: 10000,
    action: "save_dat",
    last_triggered_ms: null,
  },
];

const NAV_ITEMS: {
  id: StudioMode;
  label: string;
  icon: React.ComponentType<{ className?: string }>;
}[] = [
  { id: "live", label: "Live View", icon: Radio },
  { id: "measurements", label: "Measurements", icon: LineChart },
  { id: "alarms", label: "Alarms", icon: BellRing },
  { id: "export", label: "Export & Video", icon: Camera },
  { id: "sd", label: "SD Card", icon: HardDrive },
  { id: "editor", label: "Editor", icon: Image },
  { id: "control", label: "Control", icon: Activity },
  { id: "updater", label: "Updater", icon: Cpu },
];

export function App(): React.JSX.Element {
  const [theme, setTheme] = useState<"dark" | "light">(() => {
    const params = new URLSearchParams(window.location.search);
    return params.get("theme") === "light" ? "light" : "dark";
  });
  const [mode, setMode] = useState<StudioMode>(() => {
    const params = new URLSearchParams(window.location.search);
    const m = params.get("mode") as StudioMode | null;
    return m && NAV_ITEMS.some((i) => i.id === m) ? m : "live";
  });

  const [ports, setPorts] = useState<SerialPortInfo[]>([]);
  const [selectedPort, setSelectedPort] = useState("sim://demo-lepton35");
  const [hello, setHello] = useState<HelloResp | null>(null);
  const [streaming, setStreaming] = useState(false);

  const [palettes, setPalettes] = useState<Palette[]>(DEFAULT_PALETTES);
  const [paletteId, setPaletteId] = useState("inferno");
  const [materials, setMaterials] = useState<MaterialPreset[]>(DEFAULT_MATERIALS);

  const [frame, setFrame] = useState<ThermalFrame>(() => generateDemoFrame(1));
  const [editorFrame, setEditorFrame] = useState<ThermalFrame>(() => generateDemoFrame(9));
  const [baselineFrame] = useState<ThermalFrame>(() => generateDemoFrame(24));
  const [editorSourceName, setEditorSourceName] = useState("20261125_101500_PCB_LOAD.DAT");
  const [compareEnabled, setCompareEnabled] = useState(false);
  const [compareSplit, setCompareSplit] = useState(0.5);

  const [autoRange, setAutoRange] = useState(true);
  const [manualRange, setManualRange] = useState<[number, number]>([29000, 33800]);
  const [unit, setUnit] = useState<TempUnit>("celsius");
  const [rad, setRad] = useState<RadiometryParams>({
    cal_offset: -273.15,
    cal_slope: 0.01,
    emissivity: 0.95,
    reflected_temp_c: 22.0,
  });
  const [pipeline, setPipeline] = useState<DisplayPipeline>({
    filter: "none",
    upscaler: "edge_preserve",
    sharpen_amount: 0.45,
    rotate_180: false,
    flip_horizontal: false,
    isotherm: { mode: "off", low_c: 40, high_c: 60, color: [0, 230, 160] },
  });

  const [rois, setRois] = useState<Roi[]>(INITIAL_ROIS);
  const [selectedRoiId, setSelectedRoiId] = useState<string | null>("roi_ldo");
  const [activeTool, setActiveTool] = useState<RoiTool>("select");
  const [showHotCold, setShowHotCold] = useState(true);
  const [showLoupe, setShowLoupe] = useState(true);
  const [showGridOverlay, setShowGridOverlay] = useState(false);
  const [gridCols, setGridCols] = useState(4);
  const [gridRows, setGridRows] = useState(3);
  const [zoom, setZoom] = useState(1);
  const [hoverPixel, setHoverPixel] = useState<{
    x: number;
    y: number;
    raw: number;
    tempC: number;
  } | null>(null);

  const [recording, setRecording] = useState(false);
  const [recordedCount, setRecordedCount] = useState(0);
  const [hwButtonTrigger, setHwButtonTrigger] = useState(true);
  const [lastExportMsg, setLastExportMsg] = useState<string | null>(null);
  const [editorStatusMsg, setEditorStatusMsg] = useState<string | null>(null);
  const [sdStatusMsg, setSdStatusMsg] = useState<string | null>(null);
  const [sdListing, setSdListing] = useState<SdListing | null>(null);

  const [deviceConfig, setDeviceConfig] = useState<DeviceConfig | null>(null);
  const [diagnostics, setDiagnostics] = useState<Diagnostics | null>(null);
  const [perfTelemetry, setPerfTelemetry] = useState<PerfTelemetry | null>(null);
  const [radParams, setRadParams] = useState<RadParams | null>(null);

  const [alarmRules, setAlarmRules] = useState<TempSwitchRule[]>(INITIAL_ALARMS);
  const [alarmEvents, setAlarmEvents] = useState<AlarmEvent[]>([]);
  const [timeSeries, setTimeSeries] = useState<TimeSeriesPoint[]>([]);
  const [toast, setToast] = useState<string | null>(null);

  const startMsRef = useRef<number>(Date.now());
  const roisRef = useRef<Roi[]>(rois);
  roisRef.current = rois;
  const radRef = useRef<RadiometryParams>(rad);
  radRef.current = rad;
  const alarmRulesRef = useRef<TempSwitchRule[]>(alarmRules);
  alarmRulesRef.current = alarmRules;

  useEffect(() => {
    document.documentElement.setAttribute("data-theme", theme);
  }, [theme]);

  const showToast = useCallback((msg: string) => {
    setToast(msg);
    setTimeout(() => setToast((cur) => (cur === msg ? null : cur)), 3200);
  }, []);

  const refreshCameraTelemetry = useCallback(async () => {
    const [cfg, diag, perf, rParams] = await Promise.all([
      backend.getConfig(),
      backend.getDiagnostics(),
      backend.getPerf(false),
      backend.getRadParams(),
    ]);
    setDeviceConfig(cfg);
    setDiagnostics(diag);
    setPerfTelemetry(perf);
    setRadParams(rParams);
  }, []);

  const refreshSdListing = useCallback(async () => {
    const listing = await backend.sdList("/");
    setSdListing(listing);
  }, []);

  const handleIncomingFrame = useCallback((fr: ThermalFrame) => {
    setFrame(fr);
    setRecordedCount((c) => (c > 0 ? c + 1 : c));

    const nowMs = Date.now();
    const tSec = (nowMs - startMsRef.current) / 1000;
    const currentRad = radRef.current;
    const currentRois = roisRef.current;

    const values: Record<string, number> = {
      "Frame Max": rawToObjectC(fr.max_raw, currentRad),
      "Frame Min": rawToObjectC(fr.min_raw, currentRad),
    };
    const statsById = new Map<string, ReturnType<typeof evaluateRoi>>();
    for (const r of currentRois) {
      const st = evaluateRoi(r, fr.pixels, fr.width, fr.height, currentRad);
      values[r.label] = st.mean_c;
      statsById.set(r.id, st);
    }

    setTimeSeries((prev) => {
      const next = [...prev, { tSec, values }];
      return next.length > 180 ? next.slice(next.length - 180) : next;
    });

    // Evaluate TempSwitch alarm rules with cooldown
    let rulesChanged = false;
    const nextRules = alarmRulesRef.current.map((rule) => {
      if (!rule.enabled) return rule;
      const measured = rule.target_roi_id
        ? (statsById.get(rule.target_roi_id)?.max_c ?? values["Frame Max"])
        : values["Frame Max"];
      const hit =
        rule.condition === "above" ? measured >= rule.threshold_c : measured <= rule.threshold_c;
      if (!hit) return rule;
      if (rule.last_triggered_ms !== null && nowMs - rule.last_triggered_ms < rule.cooldown_ms) {
        return rule;
      }
      rulesChanged = true;
      setAlarmEvents((evs) => [
        {
          id: `ev_${nowMs}_${rule.id}`,
          timestamp: new Date(nowMs).toTimeString().slice(0, 8),
          rule_name: rule.name,
          measured_c: measured,
          threshold_c: rule.threshold_c,
          action: rule.action,
        },
        ...evs.slice(0, 49),
      ]);
      return { ...rule, last_triggered_ms: nowMs };
    });
    if (rulesChanged) setAlarmRules(nextRules);
  }, []);

  // Boot: load palettes, material presets, ports, SD listing, telemetry, and auto-connect to Demo Simulator
  useEffect(() => {
    let active = true;
    (async () => {
      const [pList, mList, portList] = await Promise.all([
        backend.getPalettes(),
        backend.getMaterialPresets(),
        backend.listPorts(),
      ]);
      if (!active) return;
      if (pList.length > 0) setPalettes(pList);
      if (mList.length > 0) setMaterials(mList);
      setPorts(portList);
      const initialPort = portList[0]?.port_name ?? "sim://demo-lepton35";
      setSelectedPort(initialPort);

      const h = await backend.connect(initialPort);
      if (!active) return;
      setHello(h);
      await Promise.all([refreshCameraTelemetry(), refreshSdListing()]);
      await backend.startStream((fr) => {
        if (active) handleIncomingFrame(fr);
      });
      setStreaming(true);
    })();
    return () => {
      active = false;
      void backend.stopStream();
    };
  }, [handleIncomingFrame, refreshCameraTelemetry, refreshSdListing]);

  const activePalette = useMemo(
    () => palettes.find((p) => p.id === paletteId) ?? palettes[0],
    [palettes, paletteId],
  );

  const activeFrame = mode === "editor" ? editorFrame : frame;
  const displayMinRaw = autoRange ? activeFrame.min_raw : manualRange[0];
  const displayMaxRaw = autoRange ? activeFrame.max_raw : manualRange[1];

  const activeRoiStats = useMemo(
    () =>
      rois.map((r) =>
        evaluateRoi(r, activeFrame.pixels, activeFrame.width, activeFrame.height, rad),
      ),
    [rois, activeFrame, rad],
  );

  const handleScaleToRoi = (roi: Roi) => {
    const range = roiContrastRange(
      roi,
      activeFrame.pixels,
      activeFrame.width,
      activeFrame.height,
      0.06,
    );
    if (range) {
      setAutoRange(false);
      setManualRange(range);
      showToast(`Contrast auto-scaled to ${roi.label}`);
    }
  };

  const handleToggleConnect = async () => {
    if (hello) {
      await backend.disconnect();
      setHello(null);
      setStreaming(false);
      showToast("Camera disconnected");
    } else {
      const h = await backend.connect(selectedPort);
      setHello(h);
      await Promise.all([refreshCameraTelemetry(), refreshSdListing()]);
      await backend.startStream(handleIncomingFrame);
      setStreaming(true);
      showToast(`Connected: ${h.fw_build_str}`);
    }
  };

  const handleToggleStream = async () => {
    if (streaming) {
      await backend.stopStream();
      setStreaming(false);
    } else {
      await backend.startStream(handleIncomingFrame);
      setStreaming(true);
    }
  };

  const handleToggleRecording = async () => {
    const next = !recording;
    const count = await backend.toggleRecording(next);
    setRecording(next);
    setRecordedCount(next ? 1 : count);
    showToast(
      next ? "Radiometric sequence recording started" : `Stopped recording (${count} frames)`,
    );
  };

  const handleExport = async (
    format: string,
    opts: {
      resolution: ExportResolution;
      showColorbar: boolean;
      showHud: boolean;
      title: string;
      notes: string;
    },
  ): Promise<void> => {
    const saved = await backend.exportMedia({
      format,
      palette_id: activePalette.id,
      min_raw: displayMinRaw,
      max_raw: displayMaxRaw,
      rad,
      pipeline,
      rois,
      resolution: opts.resolution,
      show_colorbar: opts.showColorbar,
      show_hud: opts.showHud,
      unit,
      title: opts.title,
      notes: opts.notes,
      pixels: Array.from(activeFrame.pixels),
      width: activeFrame.width,
      height: activeFrame.height,
    });
    const msg = `Exported ${format.toUpperCase()} → ${saved}`;
    setLastExportMsg(msg);
    setEditorStatusMsg(msg);
    showToast(msg);
  };

  const handleImportCustomPalette = async (name: string, text: string) => {
    const pal = await backend.importCustomPalette(name, text, false);
    setPalettes((prev) => [...prev.filter((p) => p.id !== pal.id), pal]);
    setPaletteId(pal.id);
    showToast(`Imported custom LUT "${pal.label}" (256 entries)`);
  };

  const toolButtons: {
    id: RoiTool;
    label: string;
    title: string;
    icon: React.ComponentType<{ className?: string }>;
  }[] = [
    { id: "select", label: "Inspect", title: "Inspect / Select ROI", icon: MousePointer2 },
    { id: "spot", label: "1×1", title: "Spot Meter 1×1", icon: Crosshair },
    { id: "spot3", label: "3×3", title: "Spot Meter 3×3", icon: Plus },
    { id: "box", label: "Box", title: "Box ROI", icon: Square },
    { id: "circle", label: "Circle", title: "Circle ROI", icon: Circle },
    { id: "polygon", label: "Poly", title: "Freehand Polygon ROI", icon: Pentagon },
    { id: "line", label: "Line", title: "Line Profile", icon: Minus },
    { id: "delta", label: "ΔT", title: "2-Point ΔT Measurement", icon: Triangle },
  ];

  return (
    <div className="flex h-screen w-screen flex-col bg-[var(--tc-bg)] text-[var(--tc-text)]">
      {/* Top Desktop Studio Bar */}
      <header className="flex h-[48px] shrink-0 items-center justify-between gap-2 border-b border-[var(--tc-border)] bg-[var(--tc-head)] px-2.5">
        {/* Brand Identity */}
        <div className="flex shrink-0 items-center gap-2">
          <BrandMark size={24} />
          <div className="flex items-baseline gap-1.5">
            <span className="text-[13.5px] font-semibold tracking-tight">DIY-Thermocam</span>
            <span className="rounded bg-[var(--tc-surface-2)] px-1.5 py-0.5 font-mono-tech text-[10px] font-medium text-[var(--tc-text-muted)]">
              Studio
            </span>
          </div>
        </div>

        {/* 8-Mode Desktop Navigation */}
        <nav className="flex items-center gap-0.5" aria-label="Studio modes">
          {NAV_ITEMS.map((item) => {
            const Icon = item.icon;
            const active = mode === item.id;
            return (
              <button
                key={item.id}
                type="button"
                onClick={() => setMode(item.id)}
                className={`tc-btn h-[30px] gap-1.5 px-2 text-[11.5px] ${
                  active
                    ? "tc-btn-active font-semibold"
                    : "border-transparent bg-transparent text-[var(--tc-text-muted)] hover:text-[var(--tc-text)]"
                }`}
              >
                <Icon className="h-3.5 w-3.5" />
                <span>{item.label}</span>
                {item.id === "alarms" && alarmEvents.length > 0 ? (
                  <span className="rounded-full bg-[var(--tc-amber)] px-1.5 py-0 font-mono-tech text-[10px] font-semibold text-black">
                    {alarmEvents.length}
                  </span>
                ) : null}
              </button>
            );
          })}
        </nav>

        {/* Connection + Unit + Theme Controls */}
        <div className="flex shrink-0 items-center gap-1.5">
          <select
            className="tc-select h-[30px] max-w-[152px] font-mono-tech text-[11px]"
            value={selectedPort}
            onChange={(e) => setSelectedPort(e.target.value)}
            aria-label="Camera port"
          >
            {ports.map((p) => (
              <option key={p.port_name} value={p.port_name}>
                {p.port_name.startsWith("sim://") ? "Simulator (Lepton 3.5)" : p.port_name}
              </option>
            ))}
          </select>

          <button
            type="button"
            onClick={() => void handleToggleConnect()}
            className={`tc-btn h-[30px] px-2 text-[11.5px] ${hello ? "" : "tc-btn-primary"}`}
          >
            <Usb className="h-3.5 w-3.5" />
            <span className="inline-flex items-center gap-1.5">
              <span
                className={`h-1.5 w-1.5 rounded-full ${
                  hello ? "bg-[var(--tc-pass)]" : "bg-[var(--tc-text-dim)]"
                }`}
              />
              {hello ? "Connected" : "Connect"}
            </span>
          </button>

          <div className="flex overflow-hidden rounded-[8px] border border-[var(--tc-border)]">
            {(["celsius", "fahrenheit", "kelvin"] as TempUnit[]).map((u) => (
              <button
                key={u}
                type="button"
                onClick={() => setUnit(u)}
                className={`h-[30px] px-2 font-mono-tech text-[11px] font-medium transition-colors ${
                  unit === u
                    ? "bg-[var(--tc-accent-soft)] text-[var(--tc-accent)]"
                    : "bg-[var(--tc-surface)] text-[var(--tc-text-muted)] hover:bg-[var(--tc-surface-2)]"
                }`}
              >
                {u === "celsius" ? "°C" : u === "fahrenheit" ? "°F" : "K"}
              </button>
            ))}
          </div>

          <button
            type="button"
            className="tc-btn h-[30px] w-[30px] p-0"
            onClick={() => setTheme((t) => (t === "dark" ? "light" : "dark"))}
            title={`Switch to ${theme === "dark" ? "light" : "dark"} theme`}
            aria-label="Toggle theme"
          >
            {theme === "dark" ? <Sun className="h-3.5 w-3.5" /> : <Moon className="h-3.5 w-3.5" />}
          </button>
        </div>
      </header>

      {/* Main Content Area */}
      <div className="flex min-h-0 flex-1 overflow-hidden">
        {mode === "live" ? (
          <>
            <main className="flex min-w-0 flex-1 flex-col gap-2.5 overflow-hidden p-3">
              {/* ROI & Capture Tool Ribbon */}
              <div className="flex items-center justify-between gap-1.5 rounded-[10px] border border-[var(--tc-border)] bg-[var(--tc-surface)] px-2 py-1.5">
                <div className="flex items-center gap-1">
                  {toolButtons.map((tb) => {
                    const Icon = tb.icon;
                    return (
                      <button
                        key={tb.id}
                        type="button"
                        onClick={() => setActiveTool(tb.id)}
                        title={tb.title}
                        className={`tc-btn h-[28px] gap-1 px-1.5 text-[11px] ${
                          activeTool === tb.id ? "tc-btn-active" : ""
                        }`}
                      >
                        <Icon className="h-3.5 w-3.5" />
                        <span>{tb.label}</span>
                      </button>
                    );
                  })}
                </div>

                <div className="flex items-center gap-1">
                  <button
                    type="button"
                    onClick={() => setShowHotCold((s) => !s)}
                    className={`tc-btn h-[28px] gap-1 px-2 text-[11px] ${showHotCold ? "tc-btn-active" : ""}`}
                    title="Toggle Min/Max auto markers"
                  >
                    <Flame className="h-3.5 w-3.5" />
                    Hot/Cold
                  </button>
                  <button
                    type="button"
                    onClick={() => setShowLoupe((s) => !s)}
                    className={`tc-btn h-[28px] gap-1 px-2 text-[11px] ${showLoupe ? "tc-btn-active" : ""}`}
                    title="Toggle 7×7 zoomed pixel-grid loupe"
                  >
                    <Search className="h-3.5 w-3.5" />
                    Loupe
                  </button>
                  <div className="mx-0.5 flex items-center gap-0.5">
                    <button
                      type="button"
                      className="tc-btn h-[28px] w-[26px] p-0"
                      onClick={() => setZoom((z) => Math.max(0.75, Number((z - 0.25).toFixed(2))))}
                      title="Zoom out"
                    >
                      <ZoomOut className="h-3.5 w-3.5" />
                    </button>
                    <span className="w-[36px] text-center font-mono-tech text-[11px] text-[var(--tc-text-muted)] tabular-nums">
                      {Math.round(zoom * 100)}%
                    </span>
                    <button
                      type="button"
                      className="tc-btn h-[28px] w-[26px] p-0"
                      onClick={() => setZoom((z) => Math.min(2.5, Number((z + 0.25).toFixed(2))))}
                      title="Zoom in"
                    >
                      <ZoomIn className="h-3.5 w-3.5" />
                    </button>
                  </div>
                  <button
                    type="button"
                    onClick={() => void handleToggleStream()}
                    className="tc-btn h-[28px] gap-1 px-2 text-[11px]"
                  >
                    {streaming ? (
                      <Pause className="h-3.5 w-3.5" />
                    ) : (
                      <Play className="h-3.5 w-3.5" />
                    )}
                    {streaming ? "Pause" : "Resume"}
                  </button>
                  <button
                    type="button"
                    onClick={() =>
                      void handleExport("png", {
                        resolution: "full_hd1080p",
                        showColorbar: true,
                        showHud: true,
                        title: "Live Snapshot",
                        notes: "",
                      })
                    }
                    className="tc-btn tc-btn-primary h-[28px] gap-1 px-2 text-[11px]"
                  >
                    <Camera className="h-3.5 w-3.5" />
                    Snap 1080p
                  </button>
                  <button
                    type="button"
                    onClick={() => void handleToggleRecording()}
                    className={`tc-btn h-[28px] gap-1 px-2 text-[11px] ${
                      recording ? "tc-btn-danger" : ""
                    }`}
                  >
                    <CircleDot className="h-3.5 w-3.5" />
                    {recording ? `REC (${recordedCount})` : "Rec"}
                  </button>
                </div>
              </div>

              {/* Center Viewport + Draggable Colorbar & 256-bin Histogram */}
              <div className="flex min-h-0 flex-1 gap-2.5">
                <ThermalViewport
                  frame={frame}
                  palette={activePalette}
                  minRaw={displayMinRaw}
                  maxRaw={displayMaxRaw}
                  rad={rad}
                  pipeline={pipeline}
                  unit={unit}
                  rois={rois}
                  selectedRoiId={selectedRoiId}
                  activeTool={activeTool}
                  showHotCold={showHotCold}
                  showLoupe={showLoupe}
                  showGridOverlay={showGridOverlay}
                  gridCols={gridCols}
                  gridRows={gridRows}
                  zoom={zoom}
                  onSelectRoi={setSelectedRoiId}
                  onAddRoi={(r) => {
                    setRois((prev) => [...prev, r]);
                    setSelectedRoiId(r.id);
                    setActiveTool("select");
                  }}
                  onHoverPixel={setHoverPixel}
                />
                <ColorbarHistogram
                  frame={frame}
                  palette={activePalette}
                  minRaw={displayMinRaw}
                  maxRaw={displayMaxRaw}
                  autoScale={autoRange}
                  rad={rad}
                  unit={unit}
                  isotherm={pipeline.isotherm}
                  onToggleAutoScale={(auto) => {
                    if (!auto && autoRange) setManualRange([frame.min_raw, frame.max_raw]);
                    setAutoRange(auto);
                  }}
                  onRangeChange={(lo, hi) => {
                    setAutoRange(false);
                    setManualRange([lo, hi]);
                  }}
                />
              </div>

              {/* Crisp Bottom Telemetry Footer */}
              <div className="flex h-[32px] shrink-0 items-center justify-between rounded-[8px] border border-[var(--tc-border)] bg-[var(--tc-surface)] px-3 font-mono-tech text-[11px] text-[var(--tc-text-muted)] tabular-nums">
                <div className="flex items-center gap-3">
                  <span>
                    MIN{" "}
                    <strong className="text-[var(--tc-accent)]">
                      {formatTemp(rawToObjectC(frame.min_raw, rad), unit, 1)}
                    </strong>
                  </span>
                  <span>
                    SPOT{" "}
                    <strong className="text-[var(--tc-text)]">
                      {formatTemp(frame.spot_temp_c, unit, 1)}
                    </strong>
                  </span>
                  <span>
                    MAX{" "}
                    <strong className="text-[var(--tc-thermal)]">
                      {formatTemp(rawToObjectC(frame.max_raw, rad), unit, 1)}
                    </strong>
                  </span>
                  {hoverPixel ? (
                    <span className="border-l border-[var(--tc-line)] pl-3 text-[var(--tc-text)]">
                      CURSOR ({hoverPixel.x},{hoverPixel.y}) ={" "}
                      {formatTemp(hoverPixel.tempC, unit, 2)} [raw {hoverPixel.raw}]
                    </span>
                  ) : null}
                </div>
                <div className="flex items-center gap-3">
                  <span>FPA {frame.fpa_temp_c.toFixed(1)} °C</span>
                  <span>SEQ #{frame.frame_seq}</span>
                  <span>{hello?.fw_build_str ?? "Offline"}</span>
                  <span>BAT {frame.bat_pct}%</span>
                </div>
              </div>
            </main>

            <InspectorPanel
              palettes={palettes}
              activePaletteId={activePalette.id}
              onSelectPalette={setPaletteId}
              onImportPalette={handleImportCustomPalette}
              materials={materials}
              rad={rad}
              onRadChange={setRad}
              pipeline={pipeline}
              onPipelineChange={setPipeline}
              unit={unit}
              onUnitChange={setUnit}
              rois={rois}
              roiStats={activeRoiStats}
              selectedRoiId={selectedRoiId}
              onSelectRoi={setSelectedRoiId}
              onUpdateRoi={(updated) =>
                setRois((prev) => prev.map((r) => (r.id === updated.id ? updated : r)))
              }
              onDeleteRoi={(id) => {
                setRois((prev) => prev.filter((r) => r.id !== id));
                if (selectedRoiId === id) setSelectedRoiId(null);
              }}
              onAutoScaleToRoi={handleScaleToRoi}
            />
          </>
        ) : mode === "measurements" ? (
          <div className="min-w-0 flex-1 overflow-hidden">
            <MeasurementsPanel
              frame={frame}
              rad={rad}
              unit={unit}
              rois={rois}
              roiStats={activeRoiStats}
              history={timeSeries}
              showGrid={showGridOverlay}
              gridSize={[gridCols, gridRows]}
              onToggleGrid={setShowGridOverlay}
              onGridSizeChange={([c, r]) => {
                setGridCols(c);
                setGridRows(r);
              }}
              onAutoScaleToRoi={handleScaleToRoi}
              onClearHistory={() => {
                startMsRef.current = Date.now();
                setTimeSeries([]);
              }}
            />
          </div>
        ) : mode === "alarms" ? (
          <div className="min-w-0 flex-1 overflow-hidden">
            <AlarmsPanel
              rules={alarmRules}
              events={alarmEvents}
              rois={rois}
              unit={unit}
              onAddRule={(rule) => setAlarmRules((prev) => [...prev, rule])}
              onUpdateRule={(updated) =>
                setAlarmRules((prev) => prev.map((r) => (r.id === updated.id ? updated : r)))
              }
              onDeleteRule={(id) => setAlarmRules((prev) => prev.filter((r) => r.id !== id))}
              onClearEvents={() => setAlarmEvents([])}
            />
          </div>
        ) : mode === "export" ? (
          <div className="min-w-0 flex-1 overflow-hidden">
            <CaptureExportPanel
              pipeline={pipeline}
              onPipelineChange={setPipeline}
              recording={recording}
              recordedCount={recordedCount}
              hwButtonTrigger={hwButtonTrigger}
              onToggleHwButtonTrigger={setHwButtonTrigger}
              onToggleRecording={handleToggleRecording}
              onExport={handleExport}
              lastExportMsg={lastExportMsg}
            />
          </div>
        ) : mode === "sd" ? (
          <div className="min-w-0 flex-1 overflow-hidden">
            <SdBrowserPanel
              listing={sdListing}
              palette={activePalette}
              onRefresh={refreshSdListing}
              onOpenInEditor={async (name) => {
                const loaded = await backend.sdReadFrame(name);
                setEditorSourceName(name);
                setEditorFrame(loaded);
                setMode("editor");
                showToast(`Loaded ${name} into Radiometric Editor`);
              }}
              onDownload={async (name) => {
                const dest = await backend.sdDownload(name);
                const msg = `Downloaded ${name} → ${dest}`;
                setSdStatusMsg(msg);
                showToast(msg);
              }}
              onDelete={async (name) => {
                await backend.sdDelete(name);
                await refreshSdListing();
                const msg = `Deleted ${name} from camera microSD`;
                setSdStatusMsg(msg);
                showToast(msg);
              }}
              onExportSequenceMp4={async () => {
                await handleExport("mp4", {
                  resolution: "full_hd1080p",
                  showColorbar: true,
                  showHud: true,
                  title: "SD Sequence",
                  notes: "",
                });
              }}
              statusMsg={sdStatusMsg}
            />
          </div>
        ) : mode === "editor" ? (
          <>
            <div className="flex min-w-0 flex-1 flex-col overflow-hidden">
              <div className="flex min-h-[340px] flex-1 gap-2.5 p-3 pb-0">
                <ThermalViewport
                  frame={editorFrame}
                  compareFrame={compareEnabled ? baselineFrame : null}
                  splitRatio={compareSplit}
                  palette={activePalette}
                  minRaw={displayMinRaw}
                  maxRaw={displayMaxRaw}
                  rad={rad}
                  pipeline={pipeline}
                  unit={unit}
                  rois={rois}
                  selectedRoiId={selectedRoiId}
                  activeTool={activeTool}
                  showHotCold={showHotCold}
                  showLoupe={showLoupe}
                  showGridOverlay={showGridOverlay}
                  gridCols={gridCols}
                  gridRows={gridRows}
                  zoom={zoom}
                  onSelectRoi={setSelectedRoiId}
                  onAddRoi={(r) => {
                    setRois((prev) => [...prev, r]);
                    setSelectedRoiId(r.id);
                  }}
                />
                <ColorbarHistogram
                  frame={editorFrame}
                  palette={activePalette}
                  minRaw={displayMinRaw}
                  maxRaw={displayMaxRaw}
                  autoScale={autoRange}
                  rad={rad}
                  unit={unit}
                  isotherm={pipeline.isotherm}
                  onToggleAutoScale={(auto) => {
                    if (!auto && autoRange) {
                      setManualRange([editorFrame.min_raw, editorFrame.max_raw]);
                    }
                    setAutoRange(auto);
                  }}
                  onRangeChange={(lo, hi) => {
                    setAutoRange(false);
                    setManualRange([lo, hi]);
                  }}
                />
              </div>
              <div className="shrink-0">
                <EditorPanel
                  sourceLabel={editorSourceName}
                  frame={editorFrame}
                  compareEnabled={compareEnabled}
                  compareSplit={compareSplit}
                  onToggleCompare={setCompareEnabled}
                  onCompareSplitChange={setCompareSplit}
                  onLoadLocalDat={async () => {
                    const loaded = await backend.loadLocalDat();
                    setEditorSourceName(loaded.path);
                    setEditorFrame(loaded.frame);
                    setEditorStatusMsg(`Loaded ${loaded.path}`);
                    showToast(`Opened ${loaded.path}`);
                  }}
                  onSaveSidecar={async (title, notes) => {
                    const sidecarPath = await backend.saveSidecar(editorSourceName, {
                      schema_version: 1,
                      palette_id: activePalette.id,
                      radiometry: rad,
                      pipeline,
                      rois,
                      title,
                      notes,
                    });
                    const msg = `Saved non-destructive sidecar → ${sidecarPath}`;
                    setEditorStatusMsg(msg);
                    showToast(msg);
                  }}
                  onExport={handleExport}
                  statusMsg={editorStatusMsg}
                />
              </div>
            </div>
            <InspectorPanel
              palettes={palettes}
              activePaletteId={activePalette.id}
              onSelectPalette={setPaletteId}
              onImportPalette={handleImportCustomPalette}
              materials={materials}
              rad={rad}
              onRadChange={setRad}
              pipeline={pipeline}
              onPipelineChange={setPipeline}
              unit={unit}
              onUnitChange={setUnit}
              rois={rois}
              roiStats={activeRoiStats}
              selectedRoiId={selectedRoiId}
              onSelectRoi={setSelectedRoiId}
              onUpdateRoi={(updated) =>
                setRois((prev) => prev.map((r) => (r.id === updated.id ? updated : r)))
              }
              onDeleteRoi={(id) => {
                setRois((prev) => prev.filter((r) => r.id !== id));
                if (selectedRoiId === id) setSelectedRoiId(null);
              }}
              onAutoScaleToRoi={handleScaleToRoi}
            />
          </>
        ) : mode === "control" ? (
          <CameraControlPanel
            config={deviceConfig}
            diagnostics={diagnostics}
            perf={perfTelemetry}
            radParams={radParams}
            onRefreshTelemetry={refreshCameraTelemetry}
            onTriggerFfc={async (force) => {
              await backend.triggerFfc(force);
              showToast(
                force ? "Forced FFC shutter calibration triggered" : "FFC calibration triggered",
              );
            }}
            onSaveConfig={async (cfg, persist) => {
              const updated = await backend.setConfig(cfg, persist);
              setDeviceConfig(updated);
              showToast(persist ? "Camera config persisted to EEPROM" : "Camera config updated");
            }}
            onSaveRadParams={async (params) => {
              const updated = await backend.setRadParams(params);
              setRadParams(updated);
              showToast("Lepton CCI radiometry parameters updated");
            }}
          />
        ) : (
          <UpdaterPanel connectedPort={selectedPort} />
        )}
      </div>

      {/* Subtle Floating Toast */}
      {toast ? (
        <div className="fixed bottom-4 left-1/2 z-50 flex -translate-x-1/2 items-center gap-2 rounded-[8px] border border-[var(--tc-accent-line)] bg-[var(--tc-surface)] px-3.5 py-2 text-[12px] shadow-lg">
          <SlidersHorizontal className="h-3.5 w-3.5 text-[var(--tc-accent)]" />
          <span>{toast}</span>
        </div>
      ) : null}
    </div>
  );
}

export default App;
