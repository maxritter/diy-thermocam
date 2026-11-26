import { Dialog } from "@base-ui/react/dialog";
import {
  Camera,
  ChevronDown,
  Maximize,
  Minus,
  PanelRight,
  Pause,
  Play,
  Plus,
  Share,
  Square,
  X,
  Zap,
} from "lucide-react";
import {
  type CSSProperties,
  type ReactNode,
  useEffect,
  useLayoutEffect,
  useRef,
  useState,
} from "react";
import { ColorbarHistogram } from "@/components/ColorbarHistogram";
import { Loupe } from "@/components/Loupe";
import { ThermalViewport } from "@/components/ThermalViewport";
import { Toolbar } from "@/components/viewport/Toolbar";
import type { ColorRange } from "@/lib/colorRange";
import type { FrameStore } from "@/lib/frameStore";
import { DEFAULT_MATERIALS } from "@/lib/palettes";
import type {
  DisplayPipeline,
  Palette,
  RadiometryParams,
  Roi,
  RoiTool,
  TempUnit,
  ThermalFrame,
} from "@/lib/types";
import { CursorReading } from "./CursorReading";
import { ImageTab } from "./ImageTab";
import { type MaterialChoice, MeasureTab } from "./MeasureTab";

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
  range: ColorRange;
  engineeringDetails: boolean;
}

export interface LivePageProps {
  store: FrameStore;
  view: ViewState;
  palettes: Palette[];
  recentPaletteIds: string[];
  onSelectPalette: (id: string) => void;
  onImportPalette: (name: string, text: string) => Promise<void>;
  materialId: string | null;
  onMaterialChange: (m: MaterialChoice) => void;
  onPipelineChange: (p: DisplayPipeline) => void;
  onUnitChange: (u: TempUnit) => void;
  onRoisChange: (update: (rois: Roi[]) => Roi[]) => void;
  onSelectRoi: (id: string | null) => void;
  onToolChange: (tool: RoiTool) => void;
  onToggleHotCold: () => void;
  onToggleLoupe: () => void;
  onGridChange: (show: boolean, size?: [number, number]) => void;
  onZoomChange: (zoom: number) => void;
  onRangeChange: (range: ColorRange) => void;
  onFitRangeToRoi: (roi: Roi, frame: ThermalFrame) => void;

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
  chartsOpen: boolean;
  onChartsOpenChange: (open: boolean) => void;

  /** The old Alarms and Export panels, until slice 5 replaces them. */
  alarmsPanel: ReactNode;
  exportPanel: ReactNode;
  /** The chart drawer below the image. */
  drawer: ReactNode;
}

const ICON = { size: 16, strokeWidth: 1.75, "aria-hidden": true } as const;
const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

/** Columns beside the image: the tool rail (64) and the color scale (108), plus a gutter. */
const SIDE_COLUMNS = 64 + 108 + 16;

/** Frames per second over the last second, from the store's counter. */
function useFps(store: FrameStore): number | null {
  const [fps, setFps] = useState<number | null>(null);
  useEffect(() => {
    let last = store.counters().received;
    let lastT = performance.now();
    const id = setInterval(() => {
      const now = performance.now();
      const n = store.counters().received;
      setFps(((n - last) * 1000) / Math.max(1, now - lastT));
      last = n;
      lastT = now;
    }, 1000);
    return () => clearInterval(id);
  }, [store]);
  return fps;
}

function LivePill({
  store,
  streaming,
  demo,
  recording,
  recordedCount,
}: {
  store: FrameStore;
  streaming: boolean;
  demo: boolean;
  recording: boolean;
  recordedCount: number;
}): React.JSX.Element {
  const fps = useFps(store);
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

/**
 * The image's height from the stage's size, as the artifact lays it out: the color scale is
 * 92 px taller than the image and fits the stage; with the charts open the stage is short.
 */
export function useStageLayout(short: boolean) {
  const ref = useRef<HTMLDivElement | null>(null);
  const [box, setBox] = useState<[number, number]>([928, 684]);
  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const update = () => {
      const r = el.getBoundingClientRect();
      if (r.width <= 0 || r.height <= 0) return;
      setBox((prev) =>
        prev[0] === Math.round(r.width) && prev[1] === Math.round(r.height)
          ? prev
          : [Math.round(r.width), Math.round(r.height)],
      );
    };
    update();
    if (typeof ResizeObserver === "undefined") return;
    const ro = new ResizeObserver(update);
    ro.observe(el);
    return () => ro.disconnect();
  }, []);
  const [w, h] = box;
  const byWidth = ((w - SIDE_COLUMNS) * 3) / 4;
  const ih = Math.max(120, Math.floor(Math.min(short ? h - 56 : h - 124, byWidth)));
  const cbh = Math.max(160, short ? h - 48 : Math.min(h - 32, ih + 92));
  // The rail reaches about 410 px down; below that the zoom control moves to the right.
  const zoomRight = short || h < 470;
  return { ref, ih, cbh, zoomRight };
}

export function LivePage(props: LivePageProps): React.JSX.Element {
  const { view, store } = props;
  const stage = useStageLayout(props.chartsOpen);

  const tabs: { id: InspectorTab; label: string }[] = [
    { id: "image", label: "Image" },
    { id: "measure", label: "Measure" },
    { id: "alarms", label: "Alarms" },
  ];

  const bodyClass = props.panelHidden
    ? "st-body full"
    : props.inspectorTab === "alarms"
      ? "st-body wide"
      : "st-body";

  const stageStyle = { "--ih": `${stage.ih}px`, "--cbh": `${stage.cbh}px` } as CSSProperties;

  return (
    <div className={bodyClass}>
      <div className="st-viewer">
        <div className="st-vbar">
          <div className="st-vbar-left">
            <LivePill
              store={store}
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
                <ChevronDown {...SM} />
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

        <div
          ref={stage.ref}
          className={`st-stage${props.chartsOpen ? " short" : ""}${stage.zoomRight ? " zoom-right" : ""}`}
          style={stageStyle}
        >
          <Toolbar
            tool={view.activeTool}
            onToolChange={props.onToolChange}
            showHotCold={view.showHotCold}
            onToggleHotCold={props.onToggleHotCold}
            showLoupe={view.showLoupe}
            onToggleLoupe={props.onToggleLoupe}
            showGrid={view.showGrid}
            gridSize={[view.gridCols, view.gridRows]}
            onGridChange={props.onGridChange}
          />
          <span aria-hidden="true" />
          <ThermalViewport
            store={store}
            palette={view.palette}
            range={view.range}
            rad={view.rad}
            pipeline={view.pipeline}
            unit={view.unit}
            rois={view.rois}
            selectedRoiId={view.selectedRoiId}
            tool={view.activeTool}
            showHotCold={view.showHotCold}
            showGrid={view.showGrid}
            gridCols={view.gridCols}
            gridRows={view.gridRows}
            zoom={view.zoom}
            onRoisChange={props.onRoisChange}
            onSelectRoi={props.onSelectRoi}
            onToolChange={props.onToolChange}
          />
          <ColorbarHistogram
            store={store}
            palette={view.palette}
            range={view.range}
            onRangeChange={props.onRangeChange}
            rad={view.rad}
            unit={view.unit}
            onUnitChange={props.onUnitChange}
            boosted={view.pipeline.histogram_eq}
          />
          <div className="st-zoom">
            <button
              type="button"
              aria-label="Zoom out"
              onClick={() => props.onZoomChange(Math.max(0.75, view.zoom - 0.25))}
            >
              <Minus {...SM} />
            </button>
            <span>{Math.round(view.zoom * 100)}%</span>
            <button
              type="button"
              aria-label="Zoom in"
              onClick={() => props.onZoomChange(Math.min(2.5, view.zoom + 0.25))}
            >
              <Plus {...SM} />
            </button>
            <button type="button" aria-label="Fit to window" onClick={() => props.onZoomChange(1)}>
              <Maximize {...SM} />
            </button>
          </div>
          <CursorReading
            store={store}
            rad={view.rad}
            unit={view.unit}
            tool={view.activeTool}
            engineeringDetails={view.engineeringDetails}
          />
          {view.showLoupe ? (
            <Loupe
              store={store}
              palette={view.palette}
              range={view.range}
              rad={view.rad}
              unit={view.unit}
              engineeringDetails={view.engineeringDetails}
            />
          ) : null}
        </div>
        {props.chartsOpen ? props.drawer : null}
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
          <div className="st-insp-body">
            {props.inspectorTab === "image" ? (
              <ImageTab
                store={store}
                palettes={props.palettes}
                paletteId={view.palette.id}
                recentPaletteIds={props.recentPaletteIds}
                onSelectPalette={props.onSelectPalette}
                onImportPalette={props.onImportPalette}
                range={view.range}
                onRangeChange={props.onRangeChange}
                rad={view.rad}
                unit={view.unit}
                pipeline={view.pipeline}
                onPipelineChange={props.onPipelineChange}
              />
            ) : props.inspectorTab === "measure" ? (
              <MeasureTab
                store={store}
                rad={view.rad}
                materialId={props.materialId}
                materials={DEFAULT_MATERIALS}
                unit={view.unit}
                onMaterialChange={props.onMaterialChange}
                rois={view.rois}
                selectedRoiId={view.selectedRoiId}
                onSelectRoi={props.onSelectRoi}
                onRoisChange={props.onRoisChange}
                onFitRange={(roi) => {
                  const fr = store.getFrame();
                  if (fr) props.onFitRangeToRoi(roi, fr);
                }}
                chartsOpen={props.chartsOpen}
                onChartsOpenChange={props.onChartsOpenChange}
              />
            ) : (
              <div role="tabpanel" aria-label="Alarms">
                {props.alarmsPanel}
              </div>
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
