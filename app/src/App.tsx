import { useCallback, useEffect, useMemo, useRef, useState, useSyncExternalStore } from "react";
import { isDemoPort } from "@/app/CameraChip";
import { ExportDialog, type ExportTab, type RecordingRef } from "@/app/ExportDialog";
import { PAGES, type PageId, Shell } from "@/app/Shell";
import type { ShortcutId } from "@/app/shortcuts";
import { currentPlatform, Toasts, toasts } from "@/app/Toasts";
import {
  type AlarmRule,
  alarmBanner,
  alarmMessage,
  alarmSentence,
  alarmValue,
  createAlarmEngine,
  reconcileRegions,
  TARGET_COLDEST,
  TARGET_HOTTEST,
} from "@/lib/alarms";
import type { CaptureItem } from "@/lib/api/captures";
import { capturesApi } from "@/lib/api/captures";
import { backend, type SessionLogSample } from "@/lib/backend";
import { handleCameraEvent } from "@/lib/cameraEvents";
import type { ColorRange } from "@/lib/colorRange";
import { chipState, connectedPort, createConnection, liveConnection } from "@/lib/connection";
import { frameStore } from "@/lib/frameStore";
import { ensureNotifyPermission, systemNotify } from "@/lib/notify";
import { DEFAULT_PALETTES } from "@/lib/palettes";
import { perf } from "@/lib/perf";
import { rowValue } from "@/lib/regions";
import { getSensorMaterial, useSensorMaterial } from "@/lib/sensorMaterial";
import { liveSeries } from "@/lib/series";
import { folderLabel, readStoredSettings, resolveTheme, type StudioSettings } from "@/lib/settings";
import { evaluateRoi, rawToObjectC, roiContrastRange } from "@/lib/thermal";
import type {
  DeviceConfig,
  Diagnostics,
  DisplayPipeline,
  HelloResp,
  Palette,
  PerfTelemetry,
  RadiometryParams,
  RadParams,
  RenderSpec,
  Roi,
  RoiTool,
  SerialPortInfo,
  ThermalFrame,
} from "@/lib/types";
import { CameraPage, CameraSync } from "@/pages/camera/CameraPage";
import {
  onFirmwareFileChosen,
  reopenFirmwareFile,
  useFirmwareFlow,
} from "@/pages/camera/FirmwareDialog";
import { CapturesPage } from "@/pages/captures/CapturesPage";
import { regionFromRoi } from "@/pages/captures/edits";
import { type AlarmLogEntry, AlarmsTab, firingSummary } from "@/pages/live/AlarmsTab";
import { ChartDrawer } from "@/pages/live/ChartDrawer";
import { ConnectionNotice } from "@/pages/live/ConnectionNotice";
import { createCaptureActions } from "@/pages/live/captureActions";
import { readRecentPalettes, rememberPalette } from "@/pages/live/ImageTab";
import { type InspectorTab, LivePage, type ViewState } from "@/pages/live/LivePage";
import { NoCamera } from "@/pages/live/NoCamera";
import { useRecorder } from "@/pages/live/RecordingBar";

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

async function writeClipboardPng(bytes: ArrayBuffer): Promise<void> {
  if (typeof ClipboardItem === "undefined" || !navigator.clipboard?.write) {
    throw { kind: "other", detail: "this system doesn’t allow copying pictures" };
  }
  const blob = new Blob([bytes], { type: "image/png" });
  await navigator.clipboard.write([new ClipboardItem({ "image/png": blob })]);
}

interface AlarmView {
  firing: ReadonlySet<string>;
  values: Record<string, number>;
  key: string;
}

const NO_ALARMS: AlarmView = { firing: new Set(), values: {}, key: "" };

export function App(): React.JSX.Element {
  perf.panelRendered("app");
  const [settings, setSettings] = useState<StudioSettings>(readStoredSettings);
  const settingsRef = useRef(settings);
  settingsRef.current = settings;
  const [page, setPage] = useState<PageId>(initialPage);
  const pageRef = useRef(page);
  pageRef.current = page;
  const [capturesDir, setCapturesDir] = useState<string | null>(null);
  const [openRequest, setOpenRequest] = useState<{ item: CaptureItem; at: number } | null>(null);

  const [ports, setPorts] = useState<SerialPortInfo[]>([]);
  const [hello, setHello] = useState<HelloResp | null>(null);
  const [streaming, setStreaming] = useState(false);
  const [batteryPct, setBatteryPct] = useState<number | null>(null);

  const [palettes, setPalettes] = useState<Palette[]>(DEFAULT_PALETTES);
  const [paletteId, setPaletteId] = useState(() => readStoredSettings().last_palette);

  const [colorRange, setColorRange] = useState<ColorRange>({ mode: "auto" });
  const [materialId, setMaterialId] = useState<string | null>(null);
  const [chartsOpen, setChartsOpen] = useState(false);
  const [recentPalettes, setRecentPalettes] = useState<string[]>(readRecentPalettes);
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

  // The Camera page reads these itself; T34 removes them with the old panels.
  const [, setDeviceConfig] = useState<DeviceConfig | null>(null);
  const [, setDiagnostics] = useState<Diagnostics | null>(null);
  const [, setPerfTelemetry] = useState<PerfTelemetry | null>(null);
  const [, setRadParams] = useState<RadParams | null>(null);

  const [alarmRules, setAlarmRules] = useState<AlarmRule[]>([]);
  const [alarmLog, setAlarmLog] = useState<AlarmLogEntry[]>([]);
  const [alarmView, setAlarmView] = useState<AlarmView>(NO_ALARMS);

  const [panelHidden, setPanelHidden] = useState(false);
  const [inspectorTab, setInspectorTab] = useState<InspectorTab>("image");
  const [exportOpen, setExportOpen] = useState(false);
  const [exportTab, setExportTab] = useState<ExportTab | undefined>(undefined);
  const [exportFrom, setExportFrom] = useState<"video" | "timelapse" | undefined>(undefined);
  const [lastVideo, setLastVideo] = useState<RecordingRef | null>(null);
  const [lastTimelapse, setLastTimelapse] = useState<RecordingRef | null>(null);

  const unit = settings.unit;
  const platform = useMemo(() => currentPlatform(), []);

  const startMsRef = useRef<number>(Date.now());
  /** Chart samples waiting for the once-a-second session log append. */
  const logBatchRef = useRef<SessionLogSample[]>([]);
  const paletteIdRef = useRef<string>(paletteId);
  paletteIdRef.current = paletteId;
  const colorRangeRef = useRef<ColorRange>(colorRange);
  colorRangeRef.current = colorRange;
  const roisRef = useRef<Roi[]>(rois);
  roisRef.current = rois;
  // Every reading undoes what the camera's sensor holds (null = factory) before Live's material.
  const sensorMaterial = useSensorMaterial();
  const liveRad = useMemo<RadiometryParams>(
    () => ({ ...rad, sensor_material: sensorMaterial }),
    [rad, sensorMaterial],
  );
  const radRef = useRef<RadiometryParams>(liveRad);
  radRef.current = liveRad;
  const pipelineRef = useRef(pipeline);
  pipelineRef.current = pipeline;
  const palettesRef = useRef(palettes);
  palettesRef.current = palettes;
  const alarmRulesRef = useRef<AlarmRule[]>(alarmRules);
  alarmRulesRef.current = alarmRules;
  const batteryRef = useRef<number | null>(null);
  const engineRef = useRef(createAlarmEngine());

  const notify = useCallback((text: string) => {
    toasts.show({ tone: "info", text });
  }, []);

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

  // Finish update reopens the firmware file chosen last.
  useEffect(() => {
    onFirmwareFileChosen((path) => updateSettings({ last_firmware_path: path }));
    return () => onFirmwareFileChosen(null);
  }, [updateSettings]);

  const selectPalette = useCallback(
    (id: string) => {
      setPaletteId(id);
      setRecentPalettes((list) => rememberPalette(list, id));
      updateSettings({ last_palette: id });
    },
    [updateSettings],
  );

  const changeCapturesFolder = useCallback(async () => {
    const dir = await backend.pickDirectory();
    if (!dir) return;
    try {
      const saved = await backend.setCapturesDir(dir);
      setCapturesDir(saved);
      updateSettings({ captures_dir: saved });
      toasts.show({ tone: "ok", text: `Captures go to ${folderLabel(saved)}` });
    } catch (err) {
      toasts.show({ tone: "error", text: `Couldn’t use that folder: ${errorText(err)}` });
    }
  }, [updateSettings]);
  const changeFolderRef = useRef(changeCapturesFolder);
  changeFolderRef.current = changeCapturesFolder;

  // ---------- Snapshot, Copy image, Record ----------

  /** How Live draws the image now, for pictures Rust renders. */
  const liveSpec = useCallback((): RenderSpec => {
    const r = radRef.current;
    const range = colorRangeRef.current;
    const material = { emissivity: r.emissivity, reflected_c: r.reflected_temp_c };
    const palette =
      palettesRef.current.find((p) => p.id === paletteIdRef.current) ?? palettesRef.current[0];
    return {
      palette_id: palette.id,
      range:
        range.mode === "manual"
          ? { mode: "manual", min_c: rawToObjectC(range.lo, r), max_c: rawToObjectC(range.hi, r) }
          : { mode: "auto", min_c: null, max_c: null },
      material,
      sensor_material: r.sensor_material ?? null,
      overlays: {
        regions: roisRef.current.filter((x) => x.visible).map((x) => regionFromRoi(x, material)),
        colorbar: true,
        hud: false,
        unit: settingsRef.current.unit,
      },
      scale: 4,
    };
  }, []);

  const [actions] = useState(() =>
    createCaptureActions({
      api: {
        snapshot: (png) => backend.snapshot(png),
        renderPng: (source, spec, opts) => backend.renderPng(source, spec, opts),
        reveal: (path) => capturesApi.revealInFolder(path),
        writeClipboardPng,
        recordStart: (opts) => backend.recordStart(opts),
        recordStop: () => backend.recordStop(),
        recordStatus: () => backend.recordStatus(),
      },
      toasts,
      spec: () => liveSpecRef.current(),
      pngOnSnapshot: () => settingsRef.current.png_on_snapshot,
      chooseFolder: () => void changeFolderRef.current(),
      exportVideo: () => {
        setExportTab("video");
        setExportOpen(true);
      },
      sensorMaterial: () => getSensorMaterial(),
      platform: currentPlatform(),
    }),
  );
  const liveSpecRef = useRef(liveSpec);
  liveSpecRef.current = liveSpec;
  const recorder = useRecorder(actions.recorder);

  // Rust sends `recording-stopped` after every stop (the button, an unplug, the disk).
  useEffect(() => {
    let off: (() => void) | null = null;
    let active = true;
    void backend
      .onRecordingStopped((info) => {
        const st = actions.recorder.get();
        const timelapse = st.mode === "timelapse" || (st.mode === "starting" && st.timelapse);
        const ref = { path: info.path, frames: info.frames, duration_ms: info.duration_ms };
        if (info.frames > 0) {
          if (timelapse) setLastTimelapse(ref);
          else setLastVideo(ref);
        }
        setExportFrom(timelapse ? "timelapse" : "video");
        actions.recorder.stopped(info);
      })
      .then((fn) => {
        if (active) off = fn;
        else fn();
      });
    // A recording that kept running while the page reloaded.
    void actions.recorder.poll();
    return () => {
      active = false;
      off?.();
    };
  }, [actions]);

  // The bar's frame count, once a second while recording, not per frame.
  const recording = recorder.mode === "recording" || recorder.mode === "timelapse";
  useEffect(() => {
    if (!recording) return;
    const id = setInterval(() => void actions.recorder.poll(), 1000);
    return () => clearInterval(id);
  }, [recording, actions]);

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

  const runAlarm = useCallback(
    (rule: AlarmRule, valueC: number, atMs: number) => {
      const u = settingsRef.current.unit;
      setAlarmLog((log) => [
        {
          id: `${atMs}_${rule.id}`,
          at: atMs,
          text: alarmSentence(rule, u),
          value: alarmValue(rule, valueC, u),
        },
        ...log.slice(0, 49),
      ]);
      if (rule.action === "notify") {
        void systemNotify(alarmMessage(rule, u), `${alarmValue(rule, valueC, u)} now`);
      } else if (rule.action === "snapshot") {
        void actions.snapshot();
      } else if (rule.action === "record" && actions.recorder.get().mode === "idle") {
        void actions.recorder.startVideo();
      }
      if (pageRef.current !== "live") {
        toasts.show({
          tone: "alarm",
          text: alarmMessage(rule, u),
          key: `alarm-${rule.id}`,
          actions: [{ label: "Go to Live", onClick: () => setPage("live") }],
        });
      }
    },
    [actions],
  );
  const runAlarmRef = useRef(runAlarm);
  runAlarmRef.current = runAlarm;

  /** Runs outside React for every frame: the store, the series and the alarms. */
  const handleIncomingFrame = useCallback((fr: ThermalFrame) => {
    frameStore.push(fr);
    perf.frameArrived(fr.frame_seq);

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
    // The chart's lines: Hottest, Coldest and each region's row value; differences stay out.
    const values: Record<string, number> = {
      [TARGET_HOTTEST]: rawToObjectC(fr.max_raw, currentRad),
      [TARGET_COLDEST]: rawToObjectC(fr.min_raw, currentRad),
    };
    const readings: Record<string, number> = { ...values };
    for (const r of roisRef.current) {
      const st = evaluateRoi(r, fr.pixels, fr.width, fr.height, currentRad);
      if (st.pixel_count <= 0) continue;
      const v = rowValue(r.geometry.kind, st);
      if (v === null) continue;
      readings[r.id] = v;
      if (r.geometry.kind !== "delta") values[r.id] = v;
    }
    const tMs = nowMs - startMsRef.current;
    liveSeries.append(tMs, values);
    for (const [region_id, value_c] of Object.entries(values)) {
      logBatchRef.current.push({ t_ms: tMs, region_id, value_c });
    }

    // Alarms read the value each row shows; they fire once per crossing and cooldown.
    for (const f of engineRef.current.evaluate(alarmRulesRef.current, readings, nowMs)) {
      const rule = alarmRulesRef.current.find((r) => r.id === f.ruleId);
      if (rule) runAlarmRef.current(rule, f.valueC, f.atMs);
    }
  }, []);

  /** A new chart session: the chart starts empty and the session log starts a new file. */
  const startChartSession = useCallback(() => {
    startMsRef.current = Date.now();
    logBatchRef.current = [];
    liveSeries.clear();
    void backend.sessionLogStart().catch(() => {});
  }, []);

  const [conn] = useState(() =>
    createConnection({
      connect: async (port) => {
        const h = await backend.connect(port);
        setHello(h);
        startChartSession();
        await backend.startStream(handleIncomingFrame);
        setStreaming(true);
        void refreshCameraTelemetry();
        if (!isDemoPort(port)) toasts.show({ tone: "ok", text: "Thermocam connected" });
      },
      disconnect: () => backend.disconnect(),
      notify: (text) => toasts.show({ tone: "error", text }),
    }),
  );
  const machine = useSyncExternalStore(conn.subscribe, conn.get, conn.get);
  const connState = machine.state;
  const connection = liveConnection(machine);

  // Leaving "streaming" stops the image; an unplug keeps the last frame on screen.
  useEffect(() => {
    if (connState.name !== "streaming") setStreaming(false);
    if (connState.name === "none" || connState.name === "error") setHello(null);
  }, [connState]);

  // Boot: palettes. Live's material list is the artifact's (palettes.ts).
  useEffect(() => {
    let active = true;
    void backend
      .getPalettes()
      .then((list) => active && list.length > 0 && setPalettes(list))
      .catch(() => {});
    return () => {
      active = false;
      void backend.stopStream();
    };
  }, []);

  // The port poll, every 2 s: a Thermocam that appears is connected; the demo never is.
  useEffect(() => {
    let active = true;
    const poll = async () => {
      const [list, updateMode] = await Promise.all([
        backend.listPorts().catch(() => null),
        backend.updateModePresent().catch(() => false),
      ]);
      if (!active || !list) return;
      setPorts(list);
      conn.dispatch({
        type: "ports",
        ports: list.filter((p) => p.is_teensy && !isDemoPort(p.port_name)).map((p) => p.port_name),
        updateMode,
      });
    };
    void poll();
    const id = setInterval(() => void poll(), 2000);
    return () => {
      active = false;
      clearInterval(id);
    };
  }, [conn]);

  useEffect(() => {
    let active = true;
    let unlisten: (() => void) | null = null;
    void backend
      .onCameraDisconnected(() => {
        if (!active) return;
        const s = conn.get().state;
        if (s.name === "streaming" && s.kind === "camera") {
          toasts.show({
            tone: "info",
            icon: "usb",
            text: "Thermocam unplugged. Plug it back in to continue.",
          });
        }
        conn.dispatch({ type: "unplugged" });
      })
      .then((fn) => {
        if (active) unlisten = fn;
        else fn();
      });
    return () => {
      active = false;
      unlisten?.();
    };
  }, [conn]);

  // The camera's button: Snapshot, Record or nothing, as chosen on the Camera page.
  useEffect(() => {
    let active = true;
    let off: (() => void) | null = null;
    void backend
      .onCameraEvent((ev) =>
        handleCameraEvent(ev, settingsRef.current.button_action, {
          snapshot: () => void actions.snapshot(),
          toggleRecording: () => void actions.recorder.toggle(),
        }),
      )
      .then((fn) => {
        if (active) off = fn;
        else fn();
      });
    return () => {
      active = false;
      off?.();
    };
  }, [actions]);

  // A camera left in update mode (Studio closed during an update) offers to finish.
  const firmware = useFirmwareFlow();
  const firmwareOpen = firmware.view.name !== "closed";
  useEffect(() => {
    if (!machine.updateMode || firmwareOpen) {
      toasts.dismissKey("update-mode");
      return;
    }
    toasts.show({
      tone: "info",
      icon: "usb",
      sticky: true,
      key: "update-mode",
      text: "The camera is waiting in update mode.",
      actions: [
        {
          label: "Finish update",
          onClick: () => {
            setPage("camera");
            void reopenFirmwareFile(settingsRef.current.last_firmware_path);
          },
        },
      ],
    });
  }, [machine.updateMode, firmwareOpen]);

  // Development only: the demo camera can be "unplugged" and "plugged in" again.
  const devUnplug =
    import.meta.env?.DEV &&
    "kind" in connState &&
    connState.kind === "demo" &&
    (connState.name === "streaming" || connState.name === "lost")
      ? {
          unplugged: connState.name === "lost",
          onToggle: () => {
            if (conn.get().state.name === "lost") {
              conn.dispatch({ type: "replug" });
              return;
            }
            void backend
              .disconnect()
              .catch(() => {})
              .then(() => {
                toasts.show({
                  tone: "info",
                  icon: "usb",
                  text: "Thermocam unplugged. Plug it back in to continue.",
                });
                conn.dispatch({ type: "unplugged" });
              });
          },
        }
      : null;

  // Chart samples go to the session log on disk once a second.
  const flushSessionLog = useCallback(async () => {
    const batch = logBatchRef.current;
    if (batch.length === 0) return;
    logBatchRef.current = [];
    await backend.sessionLogAppend(batch).catch(() => {});
  }, []);
  useEffect(() => {
    const id = setInterval(() => void flushSessionLog(), 1000);
    return () => clearInterval(id);
  }, [flushSessionLog]);

  const saveSessionCsv = async () => {
    await flushSessionLog();
    try {
      const saved = await backend.sessionLogExport(null);
      if (saved) toasts.show({ tone: "ok", text: `Saved ${saved}` });
    } catch (err) {
      toasts.show({ tone: "error", text: `Couldn’t save the CSV: ${errorText(err)}` });
    }
  };

  const copyText = async (text: string, what: string) => {
    try {
      await navigator.clipboard.writeText(text);
      toasts.show({ tone: "ok", text: `${what} copied` });
    } catch {
      toasts.show({ tone: "error", text: "Couldn’t copy" });
    }
  };

  // ---------- Alarms ----------

  // A deleted region turns its alarm off; the alarm says so.
  useEffect(() => {
    setAlarmRules((rules) => reconcileRegions(rules, new Set(rois.map((r) => r.id))));
  }, [rois]);

  // What the tab, the banner and the image show, at most four times a second.
  useEffect(() => {
    const id = setInterval(() => {
      const engine = engineRef.current;
      const firing = engine.firing();
      const values: Record<string, number> = {};
      for (const fid of firing) values[fid] = engine.value(fid) ?? Number.NaN;
      const key = [...firing]
        .sort()
        .map((fid) => `${fid}:${(values[fid] ?? 0).toFixed(1)}`)
        .join(",");
      setAlarmView((prev) => (prev.key === key ? prev : { firing: new Set(firing), values, key }));
    }, 250);
    return () => clearInterval(id);
  }, []);

  const changeRules = (next: AlarmRule[]) => {
    if (next.some((r) => r.action === "notify" && r.enabled)) void ensureNotifyPermission();
    setAlarmRules(next);
  };

  const imageLive = (connection === "live" || connection === "demo") && streaming;
  const firingRules = imageLive ? alarmRules.filter((r) => alarmView.firing.has(r.id)) : [];
  const banner =
    firingRules.length > 0
      ? alarmBanner(firingRules[0], alarmView.values[firingRules[0].id] ?? 0, unit)
      : null;
  const alertKey = firingRules.map((r) => r.target).join("\n");
  const alertIds = useMemo(() => new Set(alertKey ? alertKey.split("\n") : []), [alertKey]);
  const firingNow = new Set(firingRules.map((r) => r.id));
  const alarmTitle = firingRules.length > 0 ? firingSummary(alarmRules, firingNow) : null;

  // ---------- Live ----------

  const activePalette = useMemo(
    () => palettes.find((p) => p.id === paletteId) ?? palettes[0],
    [palettes, paletteId],
  );

  const fitRangeToRoi = (roi: Roi, fr: ThermalFrame) => {
    const range = roiContrastRange(roi, fr.pixels, fr.width, fr.height, 0.06);
    if (range) {
      setColorRange({ mode: "manual", lo: range[0], hi: range[1] });
      toasts.show({ tone: "info", text: `Color scale fitted to ${roi.label}` });
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

  const importPalette = async (name: string, text: string) => {
    const pal = await backend.importCustomPalette(name, text, false);
    setPalettes((prev) => [...prev.filter((p) => p.id !== pal.id), pal]);
    selectPalette(pal.id);
    toasts.show({ tone: "ok", text: `Palette “${pal.label}” imported` });
  };

  const openFile = async () => {
    try {
      const item = await capturesApi.openFile();
      if (!item) return;
      capturesApi.rememberOpened(item);
      setOpenRequest({ item, at: Date.now() });
      setPage("captures");
    } catch (err) {
      toasts.show({ tone: "error", text: `Couldn’t open the file: ${errorText(err)}` });
    }
  };

  const openExport = (tab?: ExportTab) => {
    setExportTab(tab);
    setExportFrom(undefined);
    setExportOpen(true);
  };

  const onShortcut = (id: ShortcutId) => {
    if (page !== "live") return;
    const tool = TOOL_SHORTCUTS[id];
    if (tool) {
      setActiveTool(tool);
      return;
    }
    const hasImage = connection !== "none";
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
        if (hasImage) void actions.snapshot();
        break;
      case "record":
        if (recorder.mode !== "idle" || (hasImage && streaming)) void actions.recorder.toggle();
        break;
      case "pause":
        if (recorder.mode !== "recording" && connection !== "lost" && hasImage) {
          void toggleStream();
        }
        break;
      case "export":
        if (hasImage && recorder.mode !== "recording") openExport();
        break;
      case "copy-image":
        if (hasImage) void actions.copyImage("latest");
        break;
      case "auto-range":
        setColorRange({ mode: "auto" });
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
      // Delete and Esc belong to the viewer, which knows what is selected or being drawn.
      default:
        break;
    }
  };

  const view: ViewState = {
    palette: activePalette,
    rad: liveRad,
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
    range: colorRange,
    engineeringDetails: settings.engineering_details,
  };

  const exportSource = useCallback(
    () => ({
      frame: frameStore.getFrame(),
      palette:
        palettesRef.current.find((p) => p.id === paletteIdRef.current) ?? palettesRef.current[0],
      range: colorRangeRef.current,
      rad: radRef.current,
      pipeline: pipelineRef.current,
      rois: roisRef.current,
      unit: settingsRef.current.unit,
    }),
    [],
  );

  const stageNotice =
    connState.name === "error" ? (
      <ConnectionNotice
        kind={connState.kind}
        platform={platform}
        onRetry={() => conn.dispatch({ type: "retry" })}
        onUseDemo={() => conn.dispatch({ type: "demo" })}
        onCopy={(t) => void copyText(t, "Command")}
      />
    ) : connState.name === "none" || (connState.name === "connecting" && !frameStore.getFrame()) ? (
      <NoCamera
        onTryDemo={() => conn.dispatch({ type: "demo" })}
        onOpenFile={() => void openFile()}
      />
    ) : null;

  const chip = chipState(machine);
  const port = connectedPort(machine);
  const liveBadge =
    recorder.mode === "recording"
      ? "rec"
      : alarmView.firing.size > 0 && page !== "live"
        ? "alarm"
        : recorder.mode === "timelapse"
          ? "timelapse"
          : null;

  return (
    <>
      <Shell
        page={page}
        onPageChange={setPage}
        camera={{
          state: chip,
          batteryPct,
          ports,
          connectedPort: port,
          onChoosePort: (p) =>
            isDemoPort(p)
              ? conn.dispatch({ type: "demo" })
              : conn.dispatch({ type: "choose", port: p }),
          onDisconnect: () => conn.dispatch({ type: "disconnect" }),
          onOpenCameraSettings: () => setPage("camera"),
          devUnplug,
        }}
        settings={settings}
        onSettingsChange={updateSettings}
        capturesFolder={folderLabel(capturesDir)}
        onChangeCapturesFolder={() => void changeCapturesFolder()}
        onShortcut={onShortcut}
        firmware={hello && port && !isDemoPort(port) ? hello.fw_build_str : null}
        liveBadge={liveBadge}
      >
        {page === "live" ? (
          <LivePage
            store={frameStore}
            view={view}
            palettes={palettes}
            recentPaletteIds={recentPalettes}
            onSelectPalette={selectPalette}
            onImportPalette={importPalette}
            materialId={materialId}
            onMaterialChange={(m) => {
              setMaterialId(m.id);
              setRad((r) => ({ ...r, emissivity: m.emissivity, reflected_temp_c: m.reflected_c }));
            }}
            onPipelineChange={setPipeline}
            onUnitChange={(u) => updateSettings({ unit: u })}
            onRoisChange={setRois}
            onSelectRoi={setSelectedRoiId}
            onToolChange={setActiveTool}
            onToggleHotCold={() => setShowHotCold((v) => !v)}
            onToggleLoupe={() => setShowLoupe((v) => !v)}
            onGridChange={(show, size) => {
              setShowGridOverlay(show);
              if (size) {
                setGridCols(size[0]);
                setGridRows(size[1]);
              }
            }}
            onZoomChange={setZoom}
            onRangeChange={setColorRange}
            onFitRangeToRoi={fitRangeToRoi}
            chartsOpen={chartsOpen}
            onChartsOpenChange={setChartsOpen}
            drawer={
              <ChartDrawer
                store={frameStore}
                series={liveSeries}
                rois={rois}
                rad={liveRad}
                unit={unit}
                height={settings.drawer_height}
                onHeightChange={(h) => updateSettings({ drawer_height: h })}
                onClear={startChartSession}
                onSaveCsv={() => void saveSessionCsv()}
                onCopy={(text, what) => void copyText(text, what)}
              />
            }
            connection={connection}
            streaming={streaming}
            actions={actions}
            onTogglePause={() => void toggleStream()}
            onCalibrate={() =>
              void backend
                .triggerFfc(false)
                .then(() => toasts.show({ tone: "ok", text: "Calibrated" }))
                .catch((err) =>
                  toasts.show({ tone: "error", text: `Couldn’t calibrate: ${errorText(err)}` }),
                )
            }
            onExport={() => openExport()}
            onChangeFolder={() => void changeCapturesFolder()}
            stageNotice={stageNotice}
            panelHidden={panelHidden}
            onTogglePanel={() => setPanelHidden((v) => !v)}
            inspectorTab={inspectorTab}
            onInspectorTabChange={setInspectorTab}
            alarmCount={firingRules.length}
            alarmTitle={alarmTitle}
            banner={banner}
            alertIds={alertIds}
            alarmsPanel={
              <AlarmsTab
                rules={alarmRules}
                onRulesChange={changeRules}
                firing={firingNow}
                since={(rid) => engineRef.current.since(rid)}
                log={alarmLog}
                onClearLog={() => setAlarmLog([])}
                rois={rois}
                unit={unit}
              />
            }
          />
        ) : page === "captures" ? (
          <CapturesPage
            camera={{ state: chip, connectionId: port }}
            capturesDir={capturesDir}
            palettes={palettes}
            paletteId={activePalette.id}
            recentPaletteIds={recentPalettes}
            onImportPalette={importPalette}
            unit={unit}
            onUnitChange={(u) => updateSettings({ unit: u })}
            engineeringDetails={settings.engineering_details}
            notify={notify}
            openRequest={openRequest}
          />
        ) : (
          <CameraPage
            state={chip}
            port={port}
            hello={hello}
            unit={unit}
            buttonAction={settings.button_action}
            onButtonActionChange={(a) => updateSettings({ button_action: a })}
            materialOnCamera={settings.material_on_camera}
            onMaterialOnCameraChange={(on) => updateSettings({ material_on_camera: on })}
            liveMaterial={{ emissivity: rad.emissivity, reflected_c: rad.reflected_temp_c }}
            onDisconnect={() => conn.dispatch({ type: "disconnect" })}
            onUseDemo={() => conn.dispatch({ type: "demo" })}
            onReconnect={(p) => conn.connectNow(p)}
          />
        )}
      </Shell>
      <ExportDialog
        open={exportOpen}
        onOpenChange={setExportOpen}
        api={{
          exportMedia: (req) => backend.exportMedia(req),
          onProgress: (cb) => backend.onExportProgress(cb),
          reveal: (path) => capturesApi.revealInFolder(path),
        }}
        toasts={toasts}
        source={exportSource}
        recordings={{ video: lastVideo, timelapse: lastTimelapse }}
        initialTab={exportTab}
        initialVideoFrom={exportFrom}
        onCopyImage={() => void actions.copyImage("latest")}
        platform={platform}
      />
      <CameraSync
        connectionId={port}
        switchOn={settings.material_on_camera}
        material={{ emissivity: rad.emissivity, reflected_c: rad.reflected_temp_c }}
      />
      <Toasts />
    </>
  );
}

export default App;
