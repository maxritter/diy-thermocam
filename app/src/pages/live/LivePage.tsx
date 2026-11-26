import { Dialog } from "@base-ui/react/dialog";
import {
  Camera,
  ChevronDown,
  Circle,
  Crosshair,
  Flame,
  Grid3x3,
  Maximize,
  Minus,
  MousePointer2,
  PanelRight,
  Pause,
  Pentagon,
  Play,
  Plus,
  Search,
  Share,
  Square,
  Triangle,
  X,
  Zap,
} from "lucide-react";
import { type ReactNode, useEffect, useMemo, useState } from "react";
import { ColorbarHistogram } from "@/components/ColorbarHistogram";
import { InspectorPanel } from "@/components/panels/InspectorPanel";
import { MeasurementsPanel } from "@/components/panels/MeasurementsPanel";
import { ThermalViewport } from "@/components/ThermalViewport";
import { frameStore } from "@/lib/frameStore";
import { evaluateRoi } from "@/lib/thermal";
import type {
  DisplayPipeline,
  MaterialPreset,
  Palette,
  RadiometryParams,
  Roi,
  RoiTool,
  TempUnit,
  ThermalFrame,
  TimeSeriesPoint,
} from "@/lib/types";
import { useFrame, useSampledFrame, useSeries } from "@/lib/useFrame";

export type InspectorTab = "image" | "measure" | "alarms";

export interface ViewState {
  palette: Palette;
  rad: RadiometryParams;
  pipeline: DisplayPipeline;
  unit: TempUnit;
  rois: Roi[];
  selectedRoiId: string | null;
  activeTool: RoiTool;
  showHotCold: boolean;
  showLoupe: boolean;
  showGrid: boolean;
  gridCols: number;
  gridRows: number;
  zoom: number;
  autoRange: boolean;
  manualRange: [number, number];
  engineeringDetails: boolean;
}

export interface LivePageProps {
  view: ViewState;
  palettes: Palette[];
  materials: MaterialPreset[];
  onSelectPalette: (id: string) => void;
  onImportPalette: (name: string, text: string) => Promise<void>;
  onRadChange: (rad: RadiometryParams) => void;
  onPipelineChange: (p: DisplayPipeline) => void;
  onUnitChange: (u: TempUnit) => void;
  onRoisChange: (update: (rois: Roi[]) => Roi[]) => void;
  onSelectRoi: (id: string | null) => void;
  onToolChange: (tool: RoiTool) => void;
  onToggleHotCold: () => void;
  onToggleLoupe: () => void;
  onToggleGrid: (show: boolean) => void;
  onGridSizeChange: (size: [number, number]) => void;
  onZoomChange: (zoom: number) => void;
  /** Auto range on or off; turning it off seeds the fixed range from this frame. */
  onAutoRangeChange: (auto: boolean, frame: ThermalFrame | null) => void;
  onManualRangeChange: (lo: number, hi: number) => void;
  onFitRangeToRoi: (roi: Roi, frame: ThermalFrame) => void;
  onClearHistory: () => void;

  streaming: boolean;
  demo: boolean;
  recording: boolean;
  recordedCount: number;
  onTogglePause: () => void;
  onCalibrate: () => void;
  onSnapshot: () => void;
  onToggleRecording: () => void;

  panelHidden: boolean;
  onTogglePanel: () => void;
  inspectorTab: InspectorTab;
  onInspectorTabChange: (tab: InspectorTab) => void;
  exportOpen: boolean;
  onExportOpenChange: (open: boolean) => void;

  /** The old Alarms and Export panels, until slice 4 and 5 replace them. */
  alarmsPanel: ReactNode;
  exportPanel: ReactNode;
}

const TOOLS: { id: RoiTool; label: string; title: string; icon: typeof Crosshair }[] = [
  { id: "select", label: "Select", title: "Select · V", icon: MousePointer2 },
  { id: "spot", label: "Spot", title: "Spot · S", icon: Crosshair },
  { id: "box", label: "Box", title: "Box · B", icon: Square },
  { id: "circle", label: "Circle", title: "Circle · C", icon: Circle },
  { id: "polygon", label: "Polygon", title: "Polygon · P", icon: Pentagon },
  { id: "line", label: "Line", title: "Line · L", icon: Minus },
  {
    id: "delta",
    label: "Difference between two points",
    title: "Difference · D",
    icon: Triangle,
  },
];

const ICON = { size: 16, strokeWidth: 1.75, "aria-hidden": true } as const;

/** Frames per second over the last second, from the store's counter. */
function useFps(): number | null {
  const [fps, setFps] = useState<number | null>(null);
  useEffect(() => {
    let last = frameStore.counters().received;
    let lastT = performance.now();
    const id = setInterval(() => {
      const now = performance.now();
      const n = frameStore.counters().received;
      setFps(((n - last) * 1000) / Math.max(1, now - lastT));
      last = n;
      lastT = now;
    }, 1000);
    return () => clearInterval(id);
  }, []);
  return fps;
}

function LivePill({
  streaming,
  demo,
  recording,
  recordedCount,
}: {
  streaming: boolean;
  demo: boolean;
  recording: boolean;
  recordedCount: number;
}): React.JSX.Element {
  const fps = useFps();
  if (recording) {
    return (
      <span className="st-live-pill" style={{ color: "var(--tc-danger)" }}>
        <span className="st-status-dot rec" />
        Recording{" "}
        <span className="font-mono-tech" style={{ fontWeight: 500, color: "var(--tc-text)" }}>
          {recordedCount} frames
        </span>
      </span>
    );
  }
  if (!streaming) {
    return (
      <span className="st-live-pill">
        <span className="st-status-dot off" />
        Paused
      </span>
    );
  }
  if (demo) {
    return (
      <span className="st-live-pill">
        <span className="st-status-dot demo" />
        Demo <span className="muted">simulated image</span>
      </span>
    );
  }
  return (
    <span className="st-live-pill">
      <span className="st-status-dot" />
      Live{" "}
      {fps !== null ? <span className="muted font-mono-tech">{fps.toFixed(1)} fps</span> : null}
    </span>
  );
}

/** The image. The only part of Live that redraws on every frame. */
function LiveViewport({ view, ...handlers }: ViewportHandlers & { view: ViewState }) {
  const frame = useFrame();
  useEffect(() => {
    if (frame) frameStore.markDrawn();
  }, [frame]);
  if (!frame) return null;
  return (
    <ThermalViewport
      frame={frame}
      palette={view.palette}
      minRaw={view.autoRange ? frame.min_raw : view.manualRange[0]}
      maxRaw={view.autoRange ? frame.max_raw : view.manualRange[1]}
      rad={view.rad}
      pipeline={view.pipeline}
      unit={view.unit}
      rois={view.rois}
      selectedRoiId={view.selectedRoiId}
      activeTool={view.activeTool}
      showHotCold={view.showHotCold}
      showLoupe={view.showLoupe}
      showGridOverlay={view.showGrid}
      minimalHud={!view.engineeringDetails}
      gridCols={view.gridCols}
      gridRows={view.gridRows}
      zoom={view.zoom}
      onSelectRoi={handlers.onSelectRoi}
      onAddRoi={handlers.onAddRoi}
      onHoverPixel={frameStore.setHover}
    />
  );
}

interface ViewportHandlers {
  onSelectRoi: (id: string | null) => void;
  onAddRoi: (roi: Roi) => void;
}

export function LivePage(props: LivePageProps): React.JSX.Element {
  const { view } = props;
  const sampled = useSampledFrame();
  const series = useSeries();
  const roiStats = useMemo(
    () =>
      sampled
        ? view.rois.map((r) =>
            evaluateRoi(r, sampled.pixels, sampled.width, sampled.height, view.rad),
          )
        : [],
    [view.rois, view.rad, sampled],
  );

  const tabs: { id: InspectorTab; label: string }[] = [
    { id: "image", label: "Image" },
    { id: "measure", label: "Measure" },
    { id: "alarms", label: "Alarms" },
  ];

  const bodyClass = props.panelHidden
    ? "st-body full"
    : props.inspectorTab === "image"
      ? "st-body"
      : "st-body wide";

  return (
    <div className={bodyClass}>
      <div className="st-viewer">
        <div className="st-vbar">
          <div className="st-vbar-left">
            <LivePill
              streaming={props.streaming}
              demo={props.demo}
              recording={props.recording}
              recordedCount={props.recordedCount}
            />
          </div>
          <div className="st-vbar-right">
            <button
              className="tc-btn tc-btn-ghost"
              type="button"
              disabled={props.recording}
              onClick={props.onTogglePause}
            >
              {props.streaming ? <Pause {...ICON} /> : <Play {...ICON} />}
              {props.streaming ? "Pause" : "Resume"}
            </button>
            <button
              className="tc-btn tc-btn-ghost"
              type="button"
              disabled={props.recording}
              title="Closes the shutter for a moment to even out the image"
              onClick={props.onCalibrate}
            >
              <Zap {...ICON} />
              Calibrate
            </button>
            <span className="st-vsep" aria-hidden="true" />
            <button className="tc-btn tc-btn-primary" type="button" onClick={props.onSnapshot}>
              <Camera {...ICON} />
              Snapshot
            </button>
            <div className="st-splitbtn">
              <button
                className={`tc-btn ${props.recording ? "tc-btn-danger" : ""}`}
                type="button"
                onClick={props.onToggleRecording}
              >
                {props.recording ? (
                  <Square {...ICON} />
                ) : (
                  <span className="st-rec-dot" aria-hidden="true" />
                )}
                {props.recording ? "Stop recording" : "Record"}
              </button>
              <button
                className="tc-btn"
                type="button"
                aria-label="Recording options"
                onClick={() => props.onExportOpenChange(true)}
              >
                <ChevronDown size={14} strokeWidth={1.75} aria-hidden="true" />
              </button>
            </div>
            <button className="tc-btn" type="button" onClick={() => props.onExportOpenChange(true)}>
              <Share {...ICON} />
              Export…
            </button>
            <button
              className="st-icon-btn"
              type="button"
              aria-label={props.panelHidden ? "Show side panel" : "Hide side panel"}
              aria-pressed={props.panelHidden}
              title="Hide side panel · ⌘\"
              onClick={props.onTogglePanel}
            >
              <PanelRight {...ICON} />
            </button>
          </div>
        </div>

        <div className="st-stage">
          <div
            className="st-rail"
            role="toolbar"
            aria-label="Measuring tools"
            aria-orientation="vertical"
          >
            {TOOLS.map((t) => {
              const Icon = t.icon;
              return (
                <button
                  key={t.id}
                  className="st-tool"
                  type="button"
                  aria-pressed={view.activeTool === t.id}
                  aria-label={t.label}
                  title={t.title}
                  onClick={() => props.onToolChange(t.id)}
                >
                  <Icon {...ICON} />
                </button>
              );
            })}
            <hr />
            <button
              className="st-tool"
              type="button"
              aria-pressed={view.showHotCold}
              aria-label="Mark hottest and coldest point"
              title="Hottest and coldest · H"
              onClick={props.onToggleHotCold}
            >
              <Flame {...ICON} />
            </button>
            <button
              className="st-tool"
              type="button"
              aria-pressed={view.showLoupe}
              aria-label="Magnifier"
              title="Magnifier · M"
              onClick={props.onToggleLoupe}
            >
              <Search {...ICON} />
            </button>
            <button
              className="st-tool"
              type="button"
              aria-pressed={view.showGrid}
              aria-label="Grid readings"
              title="Grid readings · G"
              onClick={() => props.onToggleGrid(!view.showGrid)}
            >
              <Grid3x3 {...ICON} />
            </button>
          </div>
          <span aria-hidden="true" />
          <div className="st-stage-img">
            <LiveViewport
              view={view}
              onSelectRoi={props.onSelectRoi}
              onAddRoi={(r) => {
                props.onRoisChange((prev) => [...prev, r]);
                props.onSelectRoi(r.id);
                props.onToolChange("select");
              }}
            />
          </div>
          {sampled ? (
            <div className="flex min-h-0 justify-end">
              <ColorbarHistogram
                frame={sampled}
                palette={view.palette}
                minRaw={view.autoRange ? sampled.min_raw : view.manualRange[0]}
                maxRaw={view.autoRange ? sampled.max_raw : view.manualRange[1]}
                autoScale={view.autoRange}
                rad={view.rad}
                unit={view.unit}
                isotherm={view.pipeline.isotherm}
                showHistogram
                onToggleAutoScale={(auto) => props.onAutoRangeChange(auto, frameStore.getFrame())}
                onRangeChange={props.onManualRangeChange}
              />
            </div>
          ) : (
            <span />
          )}
          <div className="st-zoom">
            <button
              type="button"
              aria-label="Zoom out"
              onClick={() => props.onZoomChange(Math.max(0.75, view.zoom - 0.25))}
            >
              <Minus size={14} strokeWidth={1.75} aria-hidden="true" />
            </button>
            <span>{Math.round(view.zoom * 100)}%</span>
            <button
              type="button"
              aria-label="Zoom in"
              onClick={() => props.onZoomChange(Math.min(2.5, view.zoom + 0.25))}
            >
              <Plus size={14} strokeWidth={1.75} aria-hidden="true" />
            </button>
            <button type="button" aria-label="Fit to window" onClick={() => props.onZoomChange(1)}>
              <Maximize size={14} strokeWidth={1.75} aria-hidden="true" />
            </button>
          </div>
        </div>
        <span />
      </div>

      {props.panelHidden ? null : (
        <aside className="st-insp" aria-label="Side panel">
          <div className="st-insp-tabs">
            <div className="st-tabs" role="tablist" aria-label="Side panel">
              {tabs.map((t) => (
                <button
                  key={t.id}
                  className="st-tab"
                  type="button"
                  role="tab"
                  aria-selected={props.inspectorTab === t.id}
                  tabIndex={props.inspectorTab === t.id ? 0 : -1}
                  onClick={() => props.onInspectorTabChange(t.id)}
                >
                  {t.label}
                </button>
              ))}
            </div>
          </div>
          <div className="st-insp-body" role="tabpanel" aria-label={props.inspectorTab}>
            {props.inspectorTab === "image" ? (
              <InspectorPanel
                palettes={props.palettes}
                activePaletteId={view.palette.id}
                onSelectPalette={props.onSelectPalette}
                onImportPalette={props.onImportPalette}
                materials={props.materials}
                rad={view.rad}
                onRadChange={props.onRadChange}
                pipeline={view.pipeline}
                onPipelineChange={props.onPipelineChange}
                unit={view.unit}
                onUnitChange={props.onUnitChange}
                rois={view.rois}
                roiStats={roiStats}
                selectedRoiId={view.selectedRoiId}
                onSelectRoi={props.onSelectRoi}
                onUpdateRoi={(updated) =>
                  props.onRoisChange((prev) => prev.map((r) => (r.id === updated.id ? updated : r)))
                }
                onDeleteRoi={(id) => {
                  props.onRoisChange((prev) => prev.filter((r) => r.id !== id));
                  if (view.selectedRoiId === id) props.onSelectRoi(null);
                }}
                onAutoScaleToRoi={(roi) => sampled && props.onFitRangeToRoi(roi, sampled)}
              />
            ) : props.inspectorTab === "measure" ? (
              sampled ? (
                <MeasurementsPanel
                  frame={sampled}
                  rad={view.rad}
                  unit={view.unit}
                  rois={view.rois}
                  roiStats={roiStats}
                  history={series as TimeSeriesPoint[]}
                  showGrid={view.showGrid}
                  gridSize={[view.gridCols, view.gridRows]}
                  onToggleGrid={props.onToggleGrid}
                  onGridSizeChange={props.onGridSizeChange}
                  onAutoScaleToRoi={(roi) => props.onFitRangeToRoi(roi, sampled)}
                  onClearHistory={props.onClearHistory}
                />
              ) : null
            ) : (
              props.alarmsPanel
            )}
          </div>
        </aside>
      )}

      <Dialog.Root open={props.exportOpen} onOpenChange={props.onExportOpenChange}>
        <Dialog.Portal>
          <Dialog.Backdrop className="st-scrim z-50" />
          <Dialog.Popup
            className="st-dialog z-50"
            style={{ width: 1080, gridTemplateRows: "auto minmax(0, 1fr)" }}
          >
            <div className="dlg-h">
              <Dialog.Title>Export</Dialog.Title>
              <Dialog.Close className="st-icon-btn" aria-label="Close">
                <X {...ICON} />
              </Dialog.Close>
            </div>
            <div style={{ minHeight: 0, height: "min(720px, calc(100vh - 120px))" }}>
              {props.exportPanel}
            </div>
          </Dialog.Popup>
        </Dialog.Portal>
      </Dialog.Root>
    </div>
  );
}
