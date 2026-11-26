import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { type CameraState, isDemoPort } from "@/app/CameraChip";
import { PAGES, type PageId, Shell } from "@/app/Shell";
import type { ShortcutId } from "@/app/shortcuts";
import { ColorbarHistogram } from "@/components/ColorbarHistogram";
import { AlarmsPanel } from "@/components/panels/AlarmsPanel";
import { CameraControlPanel } from "@/components/panels/CameraControlPanel";
import { CaptureExportPanel } from "@/components/panels/CaptureExportPanel";
import { EditorPanel } from "@/components/panels/EditorPanel";
import { SdBrowserPanel } from "@/components/panels/SdBrowserPanel";
import { UpdaterPanel } from "@/components/panels/UpdaterPanel";
import { ThermalViewport } from "@/components/ThermalViewport";
import { backend } from "@/lib/backend";
import { frameStore } from "@/lib/frameStore";
import { DEFAULT_MATERIALS, DEFAULT_PALETTES } from "@/lib/palettes";
import { folderLabel, readStoredSettings, resolveTheme, type StudioSettings } from "@/lib/settings";
import { evaluateRoi, generateDemoFrame, rawToObjectC, roiContrastRange } from "@/lib/thermal";
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
  TempSwitchRule,
  ThermalFrame,
} from "@/lib/types";
import { CameraPage } from "@/pages/camera/CameraPage";
import { CapturesPage } from "@/pages/captures/CapturesPage";
import { type InspectorTab, LivePage, type ViewState } from "@/pages/live/LivePage";

const DEMO_PORT = "sim://demo-lepton35";

const TOOL_SHORTCUTS: Partial<Record<ShortcutId, RoiTool>> = {
  "tool-select": "select",
  "tool-spot": "spot",
  "tool-box": "box",
  "tool-circle": "circle",
  "tool-polygon": "polygon",
  "tool-line": "line",
  "tool-delta": "delta",
};

function initialPage(): PageId {
  const p = new URLSearchParams(window.location.search).get("page");
  return PAGES.some((x) => x.id === p) ? (p as PageId) : "live";
}

/** Rust rejects with `{ kind, detail }` for new commands and with a string for older ones. */
function errorText(err: unknown): string {
  if (err instanceof Error) return err.message;
  if (err && typeof err === "object" && "detail" in err) return String(err.detail);
  return String(err);
}

type ExportOptions = {
  resolution: ExportResolution;
  showColorbar: boolean;
  showHud: boolean;
  title: string;
  notes: string;
};

export function App(): React.JSX.Element {
  const [settings, setSettings] = useState<StudioSettings>(readStoredSettings);
  const settingsRef = useRef(settings);
  settingsRef.current = settings;
  const [page, setPage] = useState<PageId>(initialPage);
  const [capturesDir, setCapturesDir] = useState<string | null>(null);

  const [ports, setPorts] = useState<SerialPortInfo[]>([]);
  const [selectedPort, setSelectedPort] = useState(DEMO_PORT);
  const [hello, setHello] = useState<HelloResp | null>(null);
  const [connecting, setConnecting] = useState(false);
  const [streaming, setStreaming] = useState(false);
  const [batteryPct, setBatteryPct] = useState<number | null>(null);

  const [palettes, setPalettes] = useState<Palette[]>(DEFAULT_PALETTES);
  const [paletteId, setPaletteId] = useState(() => readStoredSettings().last_palette);
  const [materials, setMaterials] = useState<MaterialPreset[]>(DEFAULT_MATERIALS);

  const [editorFrame, setEditorFrame] = useState<ThermalFrame>(() => generateDemoFrame(9));
  const [baselineFrame] = useState<ThermalFrame>(() => generateDemoFrame(24));
  const [editorSourceName, setEditorSourceName] = useState<string | null>(null);
  const [compareEnabled, setCompareEnabled] = useState(false);
  const [compareSplit, setCompareSplit] = useState(0.5);

  const [autoRange, setAutoRange] = useState(true);
  const [manualRange, setManualRange] = useState<[number, number]>([29000, 33800]);
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

  const [rois, setRois] = useState<Roi[]>([]);
  const [selectedRoiId, setSelectedRoiId] = useState<string | null>(null);
  const [activeTool, setActiveTool] = useState<RoiTool>("select");
  const [showHotCold, setShowHotCold] = useState(true);
  const [showLoupe, setShowLoupe] = useState(false);
  const [showGridOverlay, setShowGridOverlay] = useState(false);
  const [gridCols, setGridCols] = useState(4);
  const [gridRows, setGridRows] = useState(3);
  const [zoom, setZoom] = useState(1);

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

  const [alarmRules, setAlarmRules] = useState<TempSwitchRule[]>([]);
  const [alarmEvents, setAlarmEvents] = useState<AlarmEvent[]>([]);
  const [toast, setToast] = useState<string | null>(null);

  const [panelHidden, setPanelHidden] = useState(false);
  const [inspectorTab, setInspectorTab] = useState<InspectorTab>("image");
  const [exportOpen, setExportOpen] = useState(false);

  const unit = settings.unit;

  const startMsRef = useRef<number>(Date.now());
  const recordingRef = useRef<boolean>(recording);
  recordingRef.current = recording;
  const recordedRef = useRef(0);
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
  const batteryRef = useRef<number | null>(null);

  // ---------- Settings ----------

  const updateSettings = useCallback((patch: Partial<StudioSettings>) => {
    const next = { ...settingsRef.current, ...patch };
    settingsRef.current = next;
    setSettings(next);
    void backend.saveSettings(next).catch(() => {});
  }, []);

  useEffect(() => {
    let active = true;
    void backend
      .loadSettings()
      .then((s) => {
        if (!active) return;
        settingsRef.current = s;
        setSettings(s);
        setPaletteId(s.last_palette);
      })
      .catch(() => {});
    void backend
      .capturesDir()
      .then((dir) => active && setCapturesDir(dir))
      .catch(() => {});
    return () => {
      active = false;
    };
  }, []);

  useEffect(() => {
    const apply = () =>
      document.documentElement.setAttribute("data-theme", resolveTheme(settings.theme));
    apply();
    if (settings.theme !== "system" || typeof window.matchMedia !== "function") return;
    const mq = window.matchMedia("(prefers-color-scheme: dark)");
    mq.addEventListener("change", apply);
    return () => mq.removeEventListener("change", apply);
  }, [settings.theme]);

  const showToast = useCallback((msg: string) => {
    setToast(msg);
    setTimeout(() => setToast((cur) => (cur === msg ? null : cur)), 3200);
  }, []);

  const selectPalette = useCallback(
    (id: string) => {
      setPaletteId(id);
      updateSettings({ last_palette: id });
    },
    [updateSettings],
  );

  // ---------- Camera ----------

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
      setSdListing(await backend.sdList("/"));
    } catch {
      setSdListing(null);
    }
  }, []);

  /** Runs outside React for every frame: the store, the series and the alarms. */
  const handleIncomingFrame = useCallback((fr: ThermalFrame) => {
    frameStore.push(fr);
    if (recordingRef.current) recordedRef.current++;

    const bat = fr.bat_pct >= 0 && !fr.usb_power ? fr.bat_pct : null;
    if (bat !== batteryRef.current) {
      batteryRef.current = bat;
      setBatteryPct(bat);
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
    const values: Record<string, number> = {
      "Frame Max": rawToObjectC(fr.max_raw, currentRad),
      "Frame Min": rawToObjectC(fr.min_raw, currentRad),
    };
    const statsById = new Map<string, ReturnType<typeof evaluateRoi>>();
    for (const r of roisRef.current) {
      const st = evaluateRoi(r, fr.pixels, fr.width, fr.height, currentRad);
      values[r.label] = st.mean_c;
      statsById.set(r.id, st);
    }
    frameStore.appendSeries({ tSec: (nowMs - startMsRef.current) / 1000, values });

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
    if (rulesChanged) {
      alarmRulesRef.current = nextRules;
      setAlarmRules(nextRules);
    }
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
      setConnecting(true);
      try {
        const h = await backend.connect(port);
        setSelectedPort(port);
        setHello(h);
        await backend.startStream(handleIncomingFrame);
        setStreaming(true);
        void Promise.allSettled([refreshCameraTelemetry(), refreshSdListing()]);
        if (announce) {
          showToast(isDemoPort(port) ? "Connected to the demo camera" : "Thermocam connected");
        }
        return h;
      } finally {
        connectingRef.current = false;
        setConnecting(false);
      }
    },
    [handleIncomingFrame, refreshCameraTelemetry, refreshSdListing, showToast],
  );

  // Boot: palettes, materials and ports; a real Thermocam wins over the demo camera.
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

        const realCamera = portList.find((p) => p.is_teensy && !isDemoPort(p.port_name));
        const simPort = portList.find((p) => isDemoPort(p.port_name))?.port_name ?? DEMO_PORT;
        const initialPort = realCamera?.port_name ?? simPort;
        setSelectedPort(initialPort);

        try {
          await connectAndStream(initialPort, Boolean(realCamera));
        } catch (err) {
          if (!active) return;
          if (realCamera && initialPort !== simPort) {
            showToast(`Couldn’t open the Thermocam (${errorText(err)}). Showing the demo camera.`);
            await connectAndStream(simPort, false);
          } else {
            throw err;
          }
        }
      } catch (err) {
        if (active) showToast(`Couldn’t connect: ${errorText(err)}`);
      }
    })();
    return () => {
      active = false;
      void backend.stopStream();
    };
  }, [connectAndStream, showToast]);

  // Poll USB ports every 2 s; switch from the demo camera when a real one is plugged in.
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
          const realPorts = nextPorts.filter((p) => p.is_teensy && !isDemoPort(p.port_name));
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
          const onSimOrOffline = !helloRef.current || isDemoPort(selectedPortRef.current);
          if (
            onSimOrOffline &&
            !connectingRef.current &&
            (newlyPlugged || !manualSimChoiceRef.current)
          ) {
            const target = newlyPlugged ?? realPorts[0];
            manualSimChoiceRef.current = false;
            void connectAndStream(target.port_name, true).catch((err) => {
              showToast(`Couldn’t connect: ${errorText(err)}`);
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

  // The frame counter while recording, once a second, not per frame.
  useEffect(() => {
    if (!recording) return;
    const id = setInterval(() => setRecordedCount(recordedRef.current), 1000);
    return () => clearInterval(id);
  }, [recording]);

  const choosePort = async (port: string) => {
    if (port === selectedPort && hello) return;
    manualSimChoiceRef.current = isDemoPort(port);
    try {
      if (hello) await backend.disconnect();
      setHello(null);
      setStreaming(false);
      await connectAndStream(port, true);
    } catch (err) {
      setHello(null);
      setStreaming(false);
      showToast(`Couldn’t connect: ${errorText(err)}`);
    }
  };

  const disconnect = async () => {
    try {
      await backend.disconnect();
    } finally {
      manualSimChoiceRef.current = true;
      setHello(null);
      setStreaming(false);
      showToast("Camera disconnected");
    }
  };

  const activePalette = useMemo(
    () => palettes.find((p) => p.id === paletteId) ?? palettes[0],
    [palettes, paletteId],
  );

  const fitRangeToRoi = (roi: Roi, fr: ThermalFrame) => {
    const range = roiContrastRange(roi, fr.pixels, fr.width, fr.height, 0.06);
    if (range) {
      setAutoRange(false);
      setManualRange(range);
      showToast(`Color scale fitted to ${roi.label}`);
    }
  };

  const toggleStream = async () => {
    if (streaming) {
      await backend.stopStream();
      setStreaming(false);
    } else {
      await backend.startStream(handleIncomingFrame);
      setStreaming(true);
    }
  };

  const copyImage = async () => {
    const canvas = document.querySelector<HTMLCanvasElement>('canvas[data-thermal-canvas="true"]');
    if (!canvas) return;
    try {
      const blob = await new Promise<Blob | null>((resolve) => canvas.toBlob(resolve, "image/png"));
      if (blob && typeof ClipboardItem !== "undefined" && navigator.clipboard?.write) {
        await navigator.clipboard.write([new ClipboardItem({ "image/png": blob })]);
        showToast("Image copied");
        return;
      }
      showToast("Copying images isn’t available here");
    } catch {
      showToast("Couldn’t copy the image");
    }
  };

  const captureTimelapseNow = useCallback(async () => {
    const fr = frameStore.getFrame();
    if (!fr) return;
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
      if (res.saved_path) setLastExportMsg(`Time-lapse frame saved: ${res.saved_path}`);
    } catch (err) {
      setTimelapseActive(false);
      showToast(`Time-lapse stopped: ${errorText(err)}`);
    }
  }, [showToast]);

  useEffect(() => {
    if (!timelapseActive) return;
    const id = setInterval(() => void captureTimelapseNow(), timelapseIntervalSec * 1000);
    return () => clearInterval(id);
  }, [timelapseActive, timelapseIntervalSec, captureTimelapseNow]);

  const toggleRecording = async () => {
    const next = !recording;
    try {
      const count = await backend.toggleRecording(next);
      recordedRef.current = next ? 0 : count;
      setRecording(next);
      setRecordedCount(next ? 0 : count);
      showToast(next ? "Recording" : `Recording stopped · ${count} frames`);
    } catch (err) {
      showToast(`Couldn’t record: ${errorText(err)}`);
    }
  };

  const exportMedia = async (format: string, opts: ExportOptions, source: "live" | "editor") => {
    const fr = source === "editor" ? editorFrame : frameStore.getFrame();
    if (!fr) return;
    try {
      const saved = await backend.exportMedia({
        format,
        palette_id: activePalette.id,
        min_raw: autoRange ? fr.min_raw : manualRange[0],
        max_raw: autoRange ? fr.max_raw : manualRange[1],
        rad,
        pipeline,
        rois,
        resolution: opts.resolution,
        show_colorbar: opts.showColorbar,
        show_hud: opts.showHud,
        unit,
        title: opts.title,
        notes: opts.notes,
        pixels: Array.from(fr.pixels),
        width: fr.width,
        height: fr.height,
      });
      const msg = `Exported ${format.toUpperCase()}: ${saved}`;
      setLastExportMsg(msg);
      setEditorStatusMsg(msg);
      showToast(msg);
    } catch (err) {
      showToast(`Couldn’t export: ${errorText(err)}`);
    }
  };

  const snapshot = () =>
    void exportMedia(
      "png",
      {
        resolution: "full_hd1080p",
        showColorbar: true,
        showHud: true,
        title: "Snapshot",
        notes: "",
      },
      "live",
    );

  const importPalette = async (name: string, text: string) => {
    const pal = await backend.importCustomPalette(name, text, false);
    setPalettes((prev) => [...prev.filter((p) => p.id !== pal.id), pal]);
    selectPalette(pal.id);
    showToast(`Palette “${pal.label}” imported`);
  };

  const changeCapturesFolder = async () => {
    const dir = await backend.pickDirectory();
    if (!dir) return;
    try {
      const saved = await backend.setCapturesDir(dir);
      setCapturesDir(saved);
      updateSettings({ captures_dir: saved });
    } catch (err) {
      showToast(`Couldn’t use that folder: ${errorText(err)}`);
    }
  };

  const setAuto = (auto: boolean, fr: ThermalFrame | null) => {
    if (!auto && autoRange && fr) setManualRange([fr.min_raw, fr.max_raw]);
    setAutoRange(auto);
  };

  const onShortcut = (id: ShortcutId) => {
    if (page !== "live") return;
    const tool = TOOL_SHORTCUTS[id];
    if (tool) {
      setActiveTool(tool);
      return;
    }
    switch (id) {
      case "toggle-hotcold":
        setShowHotCold((v) => !v);
        break;
      case "toggle-loupe":
        setShowLoupe((v) => !v);
        break;
      case "toggle-grid":
        setShowGridOverlay((v) => !v);
        break;
      case "snapshot":
        snapshot();
        break;
      case "record":
        void toggleRecording();
        break;
      case "pause":
        if (!recording) void toggleStream();
        break;
      case "export":
        setExportOpen(true);
        break;
      case "copy-image":
        void copyImage();
        break;
      case "auto-range":
        setAutoRange(true);
        break;
      case "toggle-panel":
        setPanelHidden((v) => !v);
        break;
      case "zoom-in":
        setZoom((z) => Math.min(2.5, z + 0.25));
        break;
      case "zoom-out":
        setZoom((z) => Math.max(0.75, z - 0.25));
        break;
      case "zoom-fit":
        setZoom(1);
        break;
      case "delete-region":
        if (selectedRoiId) {
          setRois((prev) => prev.filter((r) => r.id !== selectedRoiId));
          setSelectedRoiId(null);
        }
        break;
      case "cancel":
        setActiveTool("select");
        break;
      default:
        break;
    }
  };

  const cameraState: CameraState = hello
    ? isDemoPort(selectedPort)
      ? "demo"
      : "camera"
    : connecting
      ? "connecting"
      : "none";

  const view: ViewState = {
    palette: activePalette,
    rad,
    pipeline,
    unit,
    rois,
    selectedRoiId,
    activeTool,
    showHotCold,
    showLoupe,
    showGrid: showGridOverlay,
    gridCols,
    gridRows,
    zoom,
    autoRange,
    manualRange,
    engineeringDetails: settings.engineering_details,
  };

  const editorMin = autoRange ? editorFrame.min_raw : manualRange[0];
  const editorMax = autoRange ? editorFrame.max_raw : manualRange[1];

  return (
    <>
      <Shell
        page={page}
        onPageChange={setPage}
        camera={{
          state: cameraState,
          batteryPct,
          ports,
          connectedPort: hello ? selectedPort : null,
          onChoosePort: (p) => void choosePort(p),
          onDisconnect: () => void disconnect(),
          onOpenCameraSettings: () => setPage("camera"),
        }}
        settings={settings}
        onSettingsChange={updateSettings}
        capturesFolder={folderLabel(capturesDir)}
        onChangeCapturesFolder={() => void changeCapturesFolder()}
        onShortcut={onShortcut}
        firmware={hello && !isDemoPort(selectedPort) ? hello.fw_build_str : null}
      >
        {page === "live" ? (
          <LivePage
            view={view}
            palettes={palettes}
            materials={materials}
            onSelectPalette={selectPalette}
            onImportPalette={importPalette}
            onRadChange={setRad}
            onPipelineChange={setPipeline}
            onUnitChange={(u) => updateSettings({ unit: u })}
            onRoisChange={setRois}
            onSelectRoi={setSelectedRoiId}
            onToolChange={setActiveTool}
            onToggleHotCold={() => setShowHotCold((v) => !v)}
            onToggleLoupe={() => setShowLoupe((v) => !v)}
            onToggleGrid={setShowGridOverlay}
            onGridSizeChange={([c, r]) => {
              setGridCols(c);
              setGridRows(r);
            }}
            onZoomChange={setZoom}
            onAutoRangeChange={setAuto}
            onManualRangeChange={(lo, hi) => {
              setAutoRange(false);
              setManualRange([lo, hi]);
            }}
            onFitRangeToRoi={fitRangeToRoi}
            onClearHistory={() => {
              startMsRef.current = Date.now();
              frameStore.clearSeries();
            }}
            streaming={streaming}
            demo={cameraState === "demo"}
            recording={recording}
            recordedCount={recordedCount}
            onTogglePause={() => void toggleStream()}
            onCalibrate={() =>
              void backend
                .triggerFfc(false)
                .then(() => showToast("Calibrated"))
                .catch((err) => showToast(`Couldn’t calibrate: ${errorText(err)}`))
            }
            onSnapshot={snapshot}
            onToggleRecording={() => void toggleRecording()}
            panelHidden={panelHidden}
            onTogglePanel={() => setPanelHidden((v) => !v)}
            inspectorTab={inspectorTab}
            onInspectorTabChange={setInspectorTab}
            exportOpen={exportOpen}
            onExportOpenChange={setExportOpen}
            alarmsPanel={
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
            }
            exportPanel={
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
                  if (dir) setTimelapseDir(dir);
                }}
                onTimelapseIntervalChange={setTimelapseIntervalSec}
                onToggleTimelapse={() => {
                  const next = !timelapseActive;
                  setTimelapseActive(next);
                  if (next) void captureTimelapseNow();
                  showToast(
                    next
                      ? `Time-lapse started · every ${timelapseIntervalSec} s`
                      : `Time-lapse stopped · ${timelapseCount} frames`,
                  );
                }}
                hwButtonTrigger={hwButtonTrigger}
                onToggleHwButtonTrigger={setHwButtonTrigger}
                onToggleRecording={toggleRecording}
                onExport={(format, opts) => exportMedia(format, opts, "live")}
                lastExportMsg={lastExportMsg}
              />
            }
          />
        ) : page === "captures" ? (
          <CapturesPage
            opened={editorSourceName !== null}
            openedName={editorSourceName}
            onBack={() => setEditorSourceName(null)}
            onOpenFile={async () => {
              try {
                const loaded = await backend.loadLocalDat();
                setEditorSourceName(loaded.path);
                setEditorFrame(loaded.frame);
              } catch (err) {
                showToast(`Couldn’t open the file: ${errorText(err)}`);
              }
            }}
            library={
              <SdBrowserPanel
                listing={sdListing}
                palette={activePalette}
                onRefresh={refreshSdListing}
                onOpenInEditor={async (name) => {
                  const loaded = await backend.sdReadFrame(name);
                  setEditorSourceName(name);
                  setEditorFrame(loaded);
                }}
                onDownload={async (name) => {
                  const dest = await backend.sdDownload(name);
                  const msg = `Copied ${name} to ${dest}`;
                  setSdStatusMsg(msg);
                  showToast(msg);
                }}
                onDelete={async (name) => {
                  await backend.sdDelete(name);
                  await refreshSdListing();
                  const msg = `Deleted ${name} from the camera’s card`;
                  setSdStatusMsg(msg);
                  showToast(msg);
                }}
                onExportSequenceMp4={() =>
                  exportMedia(
                    "mp4",
                    {
                      resolution: "full_hd1080p",
                      showColorbar: true,
                      showHud: true,
                      title: "SD Sequence",
                      notes: "",
                    },
                    "editor",
                  )
                }
                statusMsg={sdStatusMsg}
              />
            }
            editor={
              <div className="flex h-full min-h-0 flex-col">
                <div className="flex min-h-[340px] flex-1 gap-2.5 p-3 pb-0">
                  <ThermalViewport
                    frame={editorFrame}
                    compareFrame={compareEnabled ? baselineFrame : null}
                    splitRatio={compareSplit}
                    palette={activePalette}
                    minRaw={editorMin}
                    maxRaw={editorMax}
                    rad={rad}
                    pipeline={pipeline}
                    unit={unit}
                    rois={rois}
                    selectedRoiId={selectedRoiId}
                    activeTool={activeTool}
                    showHotCold={showHotCold}
                    showLoupe={showLoupe}
                    showGridOverlay={showGridOverlay}
                    minimalHud={!settings.engineering_details}
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
                    minRaw={editorMin}
                    maxRaw={editorMax}
                    autoScale={autoRange}
                    rad={rad}
                    unit={unit}
                    isotherm={pipeline.isotherm}
                    showHistogram
                    onToggleAutoScale={(auto) => setAuto(auto, editorFrame)}
                    onRangeChange={(lo, hi) => {
                      setAutoRange(false);
                      setManualRange([lo, hi]);
                    }}
                  />
                </div>
                <div className="shrink-0">
                  <EditorPanel
                    sourceLabel={editorSourceName ?? ""}
                    frame={editorFrame}
                    compareEnabled={compareEnabled}
                    compareSplit={compareSplit}
                    onToggleCompare={setCompareEnabled}
                    onCompareSplitChange={setCompareSplit}
                    onLoadLocalDat={async () => {
                      const loaded = await backend.loadLocalDat();
                      setEditorSourceName(loaded.path);
                      setEditorFrame(loaded.frame);
                    }}
                    onSaveSidecar={async (title, notes) => {
                      try {
                        const sidecarPath = await backend.saveSidecar(editorSourceName ?? "", {
                          schema_version: 1,
                          palette_id: activePalette.id,
                          radiometry: rad,
                          pipeline,
                          rois,
                          title,
                          notes,
                        });
                        const msg = `Edits saved: ${sidecarPath}`;
                        setEditorStatusMsg(msg);
                        showToast(msg);
                      } catch (err) {
                        showToast(`Couldn’t save the edits: ${errorText(err)}`);
                      }
                    }}
                    onExport={(format, opts) => exportMedia(format, opts, "editor")}
                    statusMsg={editorStatusMsg}
                  />
                </div>
              </div>
            }
          />
        ) : (
          <CameraPage
            controls={
              <CameraControlPanel
                config={deviceConfig}
                diagnostics={diagnostics}
                perf={perfTelemetry}
                radParams={radParams}
                onRefreshTelemetry={refreshCameraTelemetry}
                onTriggerFfc={async (force) => {
                  await backend.triggerFfc(force);
                  showToast("Calibrated");
                }}
                onSaveConfig={async (cfg, persist) => {
                  const updated = await backend.setConfig(cfg, persist);
                  setDeviceConfig(updated);
                  showToast(persist ? "Saved on the camera" : "Sent to the camera");
                }}
                onSaveRadParams={async (params) => {
                  const updated = await backend.setRadParams(params);
                  setRadParams(updated);
                  showToast("Radiometry values sent to the camera");
                }}
              />
            }
            updater={<UpdaterPanel connectedPort={selectedPort} />}
          />
        )}
      </Shell>
      {toast ? (
        <div className="st-toast" role="status">
          {toast}
        </div>
      ) : null}
    </>
  );
}

export default App;
