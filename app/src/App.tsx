import {
  Activity,
  BellRing,
  Camera,
  Circle,
  CircleDot,
  Copy,
  Cpu,
  Crosshair,
  Flame,
  HardDrive,
  Image,
  LineChart,
  Lock,
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
  Unlock,
  Usb,
  Zap,
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
import { DEFAULT_MATERIALS, DEFAULT_PALETTES, paletteGradientCss } from "@/lib/palettes";
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
    threshold_c: 48.5,
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
    threshold_c: 42.0,
    cooldown_ms: 10000,
    action: "save_dat",
    last_triggered_ms: null,
  },
];

type WorkspaceTier = "standard" | "pro";

const WORKSPACE_STORAGE_KEY = "thermocam.workspace";
const STANDARD_MODES: StudioMode[] = ["live", "sd", "editor"];

const NAV_ITEMS: {
  id: StudioMode;
  label: string;
  proLabel: string;
  title: string;
  standardVisible: boolean;
  icon: React.ComponentType<{ className?: string }>;
}[] = [
  {
    id: "live",
    label: "Live View",
    proLabel: "Live",
    title: "Live Thermal View",
    standardVisible: true,
    icon: Radio,
  },
  {
    id: "measurements",
    label: "Measurements",
    proLabel: "Measure",
    title: "Measurements, Line Profile & Time Plotter",
    standardVisible: false,
    icon: LineChart,
  },
  {
    id: "alarms",
    label: "Alarms",
    proLabel: "Alarms",
    title: "TempSwitch Threshold Alarms",
    standardVisible: false,
    icon: BellRing,
  },
  {
    id: "export",
    label: "Export & Video",
    proLabel: "Export",
    title: "Capture, Time-Lapse & Full-HD/4K Export",
    standardVisible: false,
    icon: Camera,
  },
  {
    id: "sd",
    label: "SD Card",
    proLabel: "SD Card",
    title: "Camera microSD Gallery",
    standardVisible: true,
    icon: HardDrive,
  },
  {
    id: "editor",
    label: "Editor",
    proLabel: "Editor",
    title: "Radiometric .DAT Editor & Sidecar",
    standardVisible: true,
    icon: Image,
  },
  {
    id: "control",
    label: "Control",
    proLabel: "Control",
    title: "Camera Control & SPI Telemetry",
    standardVisible: false,
    icon: Activity,
  },
  {
    id: "updater",
    label: "Updater",
    proLabel: "Updater",
    title: "In-App Teensy Firmware Updater",
    standardVisible: false,
    icon: Cpu,
  },
];

export function App(): React.JSX.Element {
  const [workspace, setWorkspace] = useState<WorkspaceTier>(() => {
    const params = new URLSearchParams(window.location.search);
    const q = params.get("workspace");
    if (q === "standard" || q === "pro") return q;
    const m = params.get("mode") as StudioMode | null;
    if (m && !STANDARD_MODES.includes(m) && NAV_ITEMS.some((i) => i.id === m)) return "pro";
    try {
      const saved = window.localStorage.getItem(WORKSPACE_STORAGE_KEY);
      if (saved === "standard" || saved === "pro") return saved;
    } catch {
      // Ignore storage restrictions in headless/private contexts
    }
    return "standard";
  });
  const isPro = workspace === "pro";

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
    histogram_eq: false,
    isotherm: { mode: "off", low_c: 40, high_c: 60, color: [0, 230, 160] },
  });

  const [rois, setRois] = useState<Roi[]>(INITIAL_ROIS);
  const [selectedRoiId, setSelectedRoiId] = useState<string | null>("roi_mcu");
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
  const [timelapseActive, setTimelapseActive] = useState(false);
  const [timelapseIntervalSec, setTimelapseIntervalSec] = useState(5);
  const [timelapseCount, setTimelapseCount] = useState(0);
  const [timelapseDir, setTimelapseDir] = useState<string | null>(null);
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
  const frameRef = useRef<ThermalFrame>(frame);
  frameRef.current = frame;
  const recordingRef = useRef<boolean>(recording);
  recordingRef.current = recording;
  const paletteIdRef = useRef<string>(paletteId);
  paletteIdRef.current = paletteId;
  const timelapseDirRef = useRef<string | null>(timelapseDir);
  timelapseDirRef.current = timelapseDir;
  const timelapseIndexRef = useRef<number>(0);
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

  const applyWorkspaceTier = useCallback(
    (next: WorkspaceTier, announce = false) => {
      setWorkspace(next);
      try {
        window.localStorage.setItem(WORKSPACE_STORAGE_KEY, next);
      } catch {
        // Ignore storage restrictions
      }
      if (next === "standard") {
        setMode((cur) => (STANDARD_MODES.includes(cur) ? cur : "live"));
        setUnit((cur) => (cur === "kelvin" ? "celsius" : cur));
      }
      if (announce) {
        showToast(
          next === "standard"
            ? "Standard mode — clean everyday thermal view (Shift+P for Pro)"
            : "Pro workbench — all engineering & measurement tools unlocked",
        );
      }
    },
    [showToast],
  );

  useEffect(() => {
    const onKeyDown = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement | null)?.tagName;
      if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") return;
      if (e.shiftKey && !e.metaKey && !e.ctrlKey && !e.altKey && e.key.toLowerCase() === "p") {
        e.preventDefault();
        applyWorkspaceTier(workspace === "pro" ? "standard" : "pro", true);
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [workspace, applyWorkspaceTier]);

  const refreshCameraTelemetry = useCallback(async () => {
    const [cfgRes, diagRes, perfRes, radRes] = await Promise.allSettled([
      backend.getConfig(),
      backend.getDiagnostics(),
      backend.getPerf(false),
      backend.getRadParams(),
    ]);
    if (cfgRes.status === "fulfilled") setDeviceConfig(cfgRes.value);
    if (diagRes.status === "fulfilled") setDiagnostics(diagRes.value);
    if (perfRes.status === "fulfilled") setPerfTelemetry(perfRes.value);
    if (radRes.status === "fulfilled") setRadParams(radRes.value);
  }, []);

  const refreshSdListing = useCallback(async () => {
    try {
      const listing = await backend.sdList("/");
      setSdListing(listing);
    } catch {
      setSdListing(null);
    }
  }, []);

  const handleIncomingFrame = useCallback((fr: ThermalFrame) => {
    setFrame(fr);
    if (recordingRef.current) {
      setRecordedCount((c) => Math.min(6000, c + 1));
    }

    let currentRad = radRef.current;
    if (
      Number.isFinite(fr.cal_slope) &&
      fr.cal_slope > 0 &&
      Number.isFinite(fr.cal_offset) &&
      (Math.abs(fr.cal_slope - currentRad.cal_slope) > 1e-6 ||
        Math.abs(fr.cal_offset - currentRad.cal_offset) > 1e-4)
    ) {
      currentRad = { ...currentRad, cal_offset: fr.cal_offset, cal_slope: fr.cal_slope };
      radRef.current = currentRad;
      setRad(currentRad);
    }

    const nowMs = Date.now();
    const tSec = (nowMs - startMsRef.current) / 1000;
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

  const selectedPortRef = useRef<string>(selectedPort);
  selectedPortRef.current = selectedPort;
  const helloRef = useRef<HelloResp | null>(hello);
  helloRef.current = hello;
  const manualSimChoiceRef = useRef<boolean>(false);
  const connectingRef = useRef<boolean>(false);

  const connectAndStream = useCallback(
    async (port: string, announce = false): Promise<HelloResp> => {
      connectingRef.current = true;
      try {
        const h = await backend.connect(port);
        setSelectedPort(port);
        setHello(h);
        await backend.startStream((fr) => {
          handleIncomingFrame(fr);
        });
        setStreaming(true);
        void Promise.allSettled([refreshCameraTelemetry(), refreshSdListing()]);
        if (announce) {
          showToast(
            port.startsWith("sim://")
              ? `Connected to Demo Simulator (${h.fw_build_str})`
              : `Connected to DIY-Thermocam (${h.fw_build_str})`,
          );
        }
        return h;
      } finally {
        connectingRef.current = false;
      }
    },
    [handleIncomingFrame, refreshCameraTelemetry, refreshSdListing, showToast],
  );

  // Boot: load palettes, material presets, ports; prefer real DIY-Thermocam hardware over Demo Simulator
  useEffect(() => {
    let active = true;
    (async () => {
      try {
        const [pList, mList, portList] = await Promise.all([
          backend.getPalettes(),
          backend.getMaterialPresets(),
          backend.listPorts(),
        ]);
        if (!active) return;
        if (pList.length > 0) setPalettes(pList);
        if (mList.length > 0) setMaterials(mList);
        setPorts(portList);

        const realCamera = portList.find((p) => p.is_teensy && !p.port_name.startsWith("sim://"));
        const simPort =
          portList.find((p) => p.port_name.startsWith("sim://"))?.port_name ??
          "sim://demo-lepton35";
        const initialPort = realCamera?.port_name ?? simPort;
        setSelectedPort(initialPort);

        try {
          await connectAndStream(initialPort, Boolean(realCamera));
        } catch (err) {
          if (!active) return;
          if (realCamera && initialPort !== simPort) {
            showToast(
              `Camera open failed (${err instanceof Error ? err.message : String(err)}) — using Simulator`,
            );
            await connectAndStream(simPort, false);
          } else {
            throw err;
          }
        }
      } catch (err) {
        if (active) {
          showToast(`Connection error: ${err instanceof Error ? err.message : String(err)}`);
        }
      }
    })();
    return () => {
      active = false;
      void backend.stopStream();
    };
  }, [connectAndStream, showToast]);

  // Poll USB ports every 2s for hot-plug detection; auto-switch from Simulator when a real camera connects
  useEffect(() => {
    let active = true;
    let initialized = false;
    let lastKnownRealPorts = new Set<string>();
    const id = setInterval(() => {
      void backend
        .listPorts()
        .then((nextPorts) => {
          if (!active) return;
          setPorts(nextPorts);
          const realPorts = nextPorts.filter(
            (p) => p.is_teensy && !p.port_name.startsWith("sim://"),
          );
          const realNames = new Set(realPorts.map((p) => p.port_name));
          const newlyPlugged = initialized
            ? realPorts.find((p) => !lastKnownRealPorts.has(p.port_name))
            : undefined;
          initialized = true;
          lastKnownRealPorts = realNames;

          if (realPorts.length === 0) {
            manualSimChoiceRef.current = false;
            return;
          }
          const curPort = selectedPortRef.current;
          const onSimOrOffline = !helloRef.current || curPort.startsWith("sim://");
          if (
            onSimOrOffline &&
            !connectingRef.current &&
            (newlyPlugged || !manualSimChoiceRef.current)
          ) {
            const target = newlyPlugged ?? realPorts[0];
            manualSimChoiceRef.current = false;
            void connectAndStream(target.port_name, true).catch((err) => {
              showToast(`Auto-connect failed: ${err instanceof Error ? err.message : String(err)}`);
            });
          }
        })
        .catch(() => {
          // Ignore transient USB enumeration errors
        });
    }, 2000);
    return () => {
      active = false;
      clearInterval(id);
    };
  }, [connectAndStream, showToast]);

  // Listen for USB camera unplug / session-exit events from Rust poll worker
  useEffect(() => {
    let active = true;
    let unlisten: (() => void) | null = null;
    void backend
      .onCameraDisconnected((reason) => {
        if (!active) return;
        setHello(null);
        setStreaming(false);
        setRecording(false);
        setTimelapseActive(false);
        showToast(`Camera disconnected: ${reason}`);
      })
      .then((fn) => {
        if (active) unlisten = fn;
        else fn();
      });
    return () => {
      active = false;
      unlisten?.();
    };
  }, [showToast]);

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
    try {
      if (hello) {
        await backend.disconnect();
        setHello(null);
        setStreaming(false);
        showToast("Camera disconnected");
      } else {
        manualSimChoiceRef.current = selectedPort.startsWith("sim://");
        await connectAndStream(selectedPort, true);
      }
    } catch (err) {
      showToast(`Connection error: ${err instanceof Error ? err.message : String(err)}`);
    }
  };

  const handlePortChange = async (nextPort: string) => {
    setSelectedPort(nextPort);
    manualSimChoiceRef.current = nextPort.startsWith("sim://");
    if (!hello) return;
    try {
      await backend.disconnect();
      await connectAndStream(nextPort, true);
    } catch (err) {
      setHello(null);
      setStreaming(false);
      showToast(`Switch failed: ${err instanceof Error ? err.message : String(err)}`);
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

  const handleCopyThermalImage = async () => {
    const canvas = document.querySelector<HTMLCanvasElement>('canvas[data-thermal-canvas="true"]');
    if (!canvas) {
      showToast("No active thermal canvas to copy");
      return;
    }
    try {
      const blob = await new Promise<Blob | null>((resolve) => canvas.toBlob(resolve, "image/png"));
      if (blob && typeof ClipboardItem !== "undefined" && navigator.clipboard?.write) {
        await navigator.clipboard.write([new ClipboardItem({ "image/png": blob })]);
        showToast("Copied thermal PNG image to clipboard");
        return;
      }
      showToast("Clipboard image API unavailable in this browser context");
    } catch {
      showToast("Could not copy image to clipboard");
    }
  };

  const captureTimelapseNow = useCallback(async () => {
    const fr = frameRef.current;
    const idx = timelapseIndexRef.current;
    timelapseIndexRef.current = idx + 1;
    try {
      const res = await backend.captureTimelapseFrame({
        outputDir: timelapseDirRef.current,
        paletteId: paletteIdRef.current,
        index: idx,
        pixels: Array.from(fr.pixels),
        width: fr.width,
        height: fr.height,
        rad: radRef.current,
      });
      setTimelapseCount((c) => c + 1);
      setRecordedCount(res.recorded_count);
      if (res.saved_path) {
        setLastExportMsg(`Time-lapse .DAT → ${res.saved_path}`);
      }
    } catch (err) {
      setTimelapseActive(false);
      showToast(`Time-lapse capture error: ${err instanceof Error ? err.message : String(err)}`);
    }
  }, [showToast]);

  useEffect(() => {
    if (!timelapseActive) return;
    const id = setInterval(() => {
      void captureTimelapseNow();
    }, timelapseIntervalSec * 1000);
    return () => clearInterval(id);
  }, [timelapseActive, timelapseIntervalSec, captureTimelapseNow]);

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
      <header className="flex h-[48px] shrink-0 items-center justify-between gap-1.5 overflow-hidden border-b border-[var(--tc-border)] bg-[var(--tc-head)] px-2.5">
        {/* Brand Identity */}
        <div className="flex shrink-0 items-center gap-1.5">
          <BrandMark size={22} />
          <div className="flex items-baseline gap-1.5">
            <span className="whitespace-nowrap text-[13px] font-semibold tracking-tight">
              DIY-Thermocam
            </span>
            <span
              className={`rounded bg-[var(--tc-surface-2)] px-1.5 py-0.5 font-mono-tech text-[10px] font-medium text-[var(--tc-text-muted)] ${
                isPro ? "hidden 2xl:inline" : "hidden lg:inline"
              }`}
            >
              Studio
            </span>
          </div>
        </div>

        {/* Desktop Navigation (3 essential views in Standard, full 8 modes in Pro) */}
        <nav
          className="flex min-w-0 items-center gap-0.5 overflow-x-auto"
          aria-label="Studio modes"
        >
          {NAV_ITEMS.filter((item) => isPro || item.standardVisible).map((item) => {
            const Icon = item.icon;
            const active = mode === item.id;
            return (
              <button
                key={item.id}
                type="button"
                title={item.title}
                onClick={() => setMode(item.id)}
                className={`tc-btn h-[30px] shrink-0 whitespace-nowrap ${
                  isPro ? "gap-1 px-1.5 text-[11px]" : "gap-1.5 px-2.5 text-[11.5px]"
                } ${
                  active
                    ? "tc-btn-active font-semibold"
                    : "border-transparent bg-transparent text-[var(--tc-text-muted)] hover:text-[var(--tc-text)]"
                }`}
              >
                <Icon className="h-3.5 w-3.5 shrink-0" />
                <span>{isPro ? item.proLabel : item.label}</span>
                {isPro && item.id === "alarms" && alarmEvents.length > 0 ? (
                  <span className="rounded-full bg-[var(--tc-amber)] px-1.5 py-0 font-mono-tech text-[10px] font-semibold text-black">
                    {alarmEvents.length}
                  </span>
                ) : null}
              </button>
            );
          })}
        </nav>

        {/* Standard | Pro + Connection + Unit + Theme Controls */}
        <div className="flex shrink-0 items-center gap-1">
          {/* Standard | Pro Segmented Toggle */}
          <fieldset
            className="m-0 flex overflow-hidden rounded-[8px] border border-[var(--tc-border)] bg-[var(--tc-surface)] p-0"
            aria-label="Workspace mode"
            title="Switch Standard / Pro workspace (Shift+P)"
          >
            {(["standard", "pro"] as WorkspaceTier[]).map((tier) => (
              <button
                key={tier}
                type="button"
                aria-pressed={workspace === tier}
                onClick={() => applyWorkspaceTier(tier, true)}
                className={`h-[28px] cursor-pointer px-1.5 text-[11px] font-semibold transition-colors ${
                  workspace === tier
                    ? "bg-[var(--tc-accent)] text-[var(--tc-text-on-accent)]"
                    : "text-[var(--tc-text-muted)] hover:bg-[var(--tc-surface-2)] hover:text-[var(--tc-text)]"
                }`}
              >
                {tier === "standard" ? "Standard" : "Pro"}
              </button>
            ))}
          </fieldset>

          <select
            className={`tc-select h-[30px] min-w-0 truncate font-mono-tech text-[11px] ${
              isPro ? "w-[126px]" : "w-[168px]"
            }`}
            value={selectedPort}
            onChange={(e) => void handlePortChange(e.target.value)}
            aria-label="Camera port"
            title={selectedPort}
          >
            {ports.map((p) => (
              <option key={p.port_name} value={p.port_name}>
                {p.port_name.startsWith("sim://")
                  ? "Simulator (Lepton 3.5)"
                  : p.is_teensy
                    ? `Thermocam (${p.port_name.replace(/^\/dev\/cu\./, "")})`
                    : p.port_name.replace(/^\/dev\/cu\./, "")}
              </option>
            ))}
          </select>

          <button
            type="button"
            onClick={() => void handleToggleConnect()}
            className={`tc-btn h-[30px] whitespace-nowrap px-2 text-[11px] ${
              hello ? "" : "tc-btn-primary"
            }`}
          >
            <Usb className="h-3.5 w-3.5 shrink-0" />
            <span className="inline-flex items-center gap-1.5">
              <span
                className={`h-1.5 w-1.5 shrink-0 rounded-full ${
                  hello
                    ? selectedPort.startsWith("sim://")
                      ? "bg-[var(--tc-amber)]"
                      : "bg-[var(--tc-pass)]"
                    : "bg-[var(--tc-text-dim)]"
                }`}
              />
              {hello ? (selectedPort.startsWith("sim://") ? "Demo" : "Live") : "Connect"}
            </span>
          </button>

          <div className="flex overflow-hidden rounded-[8px] border border-[var(--tc-border)]">
            {(
              (isPro
                ? ["celsius", "fahrenheit", "kelvin"]
                : ["celsius", "fahrenheit"]) as TempUnit[]
            ).map((u) => (
              <button
                key={u}
                type="button"
                onClick={() => setUnit(u)}
                className={`h-[28px] px-1.5 font-mono-tech text-[11px] font-medium transition-colors ${
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
            className="tc-btn h-[30px] w-[30px] shrink-0 p-0"
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
              {/* Top Control Bar: Essential Everyday Controls in Standard, Full ROI Ribbon in Pro */}
              {isPro ? (
                <div className="flex flex-wrap items-center justify-between gap-1.5 rounded-[10px] border border-[var(--tc-border)] bg-[var(--tc-surface)] px-2 py-1.5">
                  <div className="flex flex-wrap items-center gap-1">
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
                        onClick={() =>
                          setZoom((z) => Math.max(0.75, Number((z - 0.25).toFixed(2))))
                        }
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
                      onClick={() => void handleCopyThermalImage()}
                      className="tc-btn h-[28px] w-[28px] p-0"
                      title="Copy thermal image (PNG) to clipboard"
                      aria-label="Copy thermal image to clipboard"
                    >
                      <Copy className="h-3.5 w-3.5" />
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
              ) : (
                <div className="flex flex-wrap items-center justify-between gap-2 rounded-[10px] border border-[var(--tc-border)] bg-[var(--tc-surface)] px-3 py-2">
                  <div className="flex flex-wrap items-center gap-2">
                    <div className="flex items-center gap-2">
                      <span className="tc-section-label">Palette</span>
                      <select
                        value={activePalette.id}
                        onChange={(e) => setPaletteId(e.target.value)}
                        className="tc-select h-[30px] min-w-[155px] text-[12px]"
                        aria-label="Thermal color palette"
                      >
                        {palettes.map((p) => (
                          <option key={p.id} value={p.id}>
                            {p.label}
                          </option>
                        ))}
                      </select>
                      <div
                        className="h-3.5 w-20 rounded-full border border-[var(--tc-border)]"
                        style={{ background: paletteGradientCss(activePalette, "to right") }}
                      />
                    </div>

                    <button
                      type="button"
                      onClick={() => {
                        if (autoRange) setManualRange([frame.min_raw, frame.max_raw]);
                        setAutoRange((a) => !a);
                      }}
                      className={`tc-btn h-[30px] px-2.5 text-[12px] ${autoRange ? "tc-btn-active" : ""}`}
                      title="Toggle automatic or locked temperature scale"
                    >
                      {autoRange ? (
                        <Unlock className="h-3.5 w-3.5" />
                      ) : (
                        <Lock className="h-3.5 w-3.5" />
                      )}
                      {autoRange ? "Auto Range" : "Range Locked"}
                    </button>

                    <button
                      type="button"
                      onClick={async () => {
                        await backend.triggerFfc(false);
                        showToast("FFC shutter calibration complete");
                      }}
                      className="tc-btn h-[30px] px-2.5 text-[12px]"
                      title="Trigger Lepton Flat-Field Correction (FFC) shutter"
                    >
                      <Zap className="h-3.5 w-3.5" />
                      FFC Shutter
                    </button>
                  </div>

                  <div className="flex items-center gap-2">
                    <button
                      type="button"
                      onClick={() => void handleToggleStream()}
                      className="tc-btn h-[30px] px-2.5 text-[12px]"
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
                      onClick={() => void handleCopyThermalImage()}
                      className="tc-btn h-[30px] px-2.5 text-[12px]"
                      title="Copy thermal image (PNG) to clipboard"
                    >
                      <Copy className="h-3.5 w-3.5" />
                      Copy PNG
                    </button>
                    <button
                      type="button"
                      onClick={() =>
                        void handleExport("png", {
                          resolution: "full_hd1080p",
                          showColorbar: true,
                          showHud: true,
                          title: "Thermal Snapshot",
                          notes: "",
                        })
                      }
                      className="tc-btn tc-btn-primary h-[30px] px-3 text-[12px]"
                    >
                      <Camera className="h-3.5 w-3.5" />
                      Capture Still
                    </button>
                    <button
                      type="button"
                      onClick={() => void handleToggleRecording()}
                      className={`tc-btn h-[30px] px-3 text-[12px] ${
                        recording ? "tc-btn-danger" : ""
                      }`}
                    >
                      <CircleDot className="h-3.5 w-3.5" />
                      {recording ? `Stop (${recordedCount})` : "Record Video"}
                    </button>
                  </div>
                </div>
              )}

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
                  rois={isPro ? rois : []}
                  selectedRoiId={isPro ? selectedRoiId : null}
                  activeTool={isPro ? activeTool : "select"}
                  showHotCold={showHotCold}
                  showLoupe={isPro && showLoupe}
                  showGridOverlay={isPro && showGridOverlay}
                  minimalHud={!isPro}
                  gridCols={gridCols}
                  gridRows={gridRows}
                  zoom={zoom}
                  onSelectRoi={setSelectedRoiId}
                  onAddRoi={(r) => {
                    if (!isPro) return;
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
                  showHistogram={isPro}
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
                      {formatTemp(hoverPixel.tempC, unit, 2)}
                      {isPro ? ` [raw ${hoverPixel.raw}]` : ""}
                    </span>
                  ) : null}
                </div>
                <div className="flex items-center gap-3">
                  {isPro ? (
                    <>
                      <span>
                        FPA{" "}
                        {Number.isFinite(frame.fpa_temp_c)
                          ? `${frame.fpa_temp_c.toFixed(1)} °C`
                          : "—"}
                      </span>
                      <span>SEQ #{frame.frame_seq}</span>
                    </>
                  ) : (
                    <span>Shift+P for Pro workbench</span>
                  )}
                  <span>{hello?.fw_build_str ?? "Offline"}</span>
                  <span>{frame.bat_pct >= 0 ? `BAT ${frame.bat_pct}%` : "USB"}</span>
                </div>
              </div>
            </main>

            {isPro ? (
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
            ) : null}
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
              timelapseActive={timelapseActive}
              timelapseIntervalSec={timelapseIntervalSec}
              timelapseCount={timelapseCount}
              timelapseDir={timelapseDir}
              onPickTimelapseDir={async () => {
                const dir = await backend.pickDirectory();
                if (dir) {
                  setTimelapseDir(dir);
                  showToast(`Time-lapse folder set → ${dir}`);
                }
              }}
              onTimelapseIntervalChange={setTimelapseIntervalSec}
              onToggleTimelapse={() => {
                const next = !timelapseActive;
                setTimelapseActive(next);
                if (next) {
                  void captureTimelapseNow();
                }
                showToast(
                  next
                    ? `Time-lapse .DAT capture started (every ${timelapseIntervalSec}s)`
                    : `Time-lapse capture stopped (${timelapseCount} frames)`,
                );
              }}
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
                  rois={isPro ? rois : []}
                  selectedRoiId={isPro ? selectedRoiId : null}
                  activeTool={isPro ? activeTool : "select"}
                  showHotCold={showHotCold}
                  showLoupe={isPro && showLoupe}
                  showGridOverlay={isPro && showGridOverlay}
                  minimalHud={!isPro}
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
                  showHistogram={isPro}
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
            {isPro ? (
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
            ) : null}
          </>
        ) : mode === "control" ? (
          <div className="min-w-0 flex-1 overflow-hidden">
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
          </div>
        ) : (
          <div className="min-w-0 flex-1 overflow-hidden">
            <UpdaterPanel connectedPort={selectedPort} />
          </div>
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
