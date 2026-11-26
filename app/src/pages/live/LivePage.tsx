import { Bell, Maximize, Minus, Plus } from "lucide-react";
import {
  type CSSProperties,
  type ReactNode,
  useEffect,
  useLayoutEffect,
  useRef,
  useState,
} from "react";
import { SidePanel } from "@/app/ui/SidePanel";
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
import { CaptureBar } from "./CaptureBar";
import { CursorReading } from "./CursorReading";
import type { CaptureActions } from "./captureActions";
import { ImageTab } from "./ImageTab";
import { type MaterialChoice, MeasureTab } from "./MeasureTab";
import { type LiveConnection, RecordingBar } from "./RecordingBar";
import "./live.css";

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

  /** What the bar above the image says about the camera. */
  connection: LiveConnection;
  streaming: boolean;
  actions: CaptureActions;
  onTogglePause: () => void;
  onCalibrate: () => void;
  onExport: () => void;
  onChangeFolder: () => void;
  /** Shown instead of the image: Connect your Thermocam, or a connection error. */
  stageNotice: ReactNode | null;

  panelHidden: boolean;
  onTogglePanel: () => void;
  inspectorTab: InspectorTab;
  onInspectorTabChange: (tab: InspectorTab) => void;
  chartsOpen: boolean;
  onChartsOpenChange: (open: boolean) => void;

  /** The Alarms tab. */
  alarmsPanel: ReactNode;
  /** Alarms past their limit now: the tab's count and the banner. */
  alarmCount: number;
  alarmTitle: string | null;
  banner: string | null;
  alertIds: ReadonlySet<string>;
  /** The chart drawer below the image. */
  drawer: ReactNode;
}

const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

/** Columns beside the image: the tool rail (64) and the color scale (108), plus a gutter. */
const SIDE_COLUMNS = 64 + 108 + 16;

/** When the frame on screen arrived (sampled), for "Paused · frame from …". */
function useFrameTime(store: FrameStore): React.RefObject<Date | null> {
  const ref = useRef<Date | null>(null);
  useEffect(() => {
    if (store.getFrame()) ref.current = new Date();
    return store.subscribeSampled(() => {
      ref.current = new Date();
    });
  }, [store]);
  return ref;
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
  const frameTime = useFrameTime(store);

  const tabs: { id: InspectorTab; label: string }[] = [
    { id: "image", label: "Image" },
    { id: "measure", label: "Measure" },
    { id: "alarms", label: "Alarms" },
  ];

  const vacant = props.stageNotice !== null;
  const bodyClass = props.panelHidden || vacant ? "st-body full" : "st-body";

  const stageStyle = { "--ih": `${stage.ih}px`, "--cbh": `${stage.cbh}px` } as CSSProperties;
  const lost = props.connection === "lost";
  const stageClass = [
    "st-stage",
    props.chartsOpen && !vacant ? "short" : "",
    stage.zoomRight ? "zoom-right" : "",
    vacant ? "lv-vacant" : "",
    lost ? "lv-lost" : "",
  ]
    .filter(Boolean)
    .join(" ");

  return (
    <div className={bodyClass}>
      <div className="st-viewer">
        <div className="st-vbar">
          <div className="st-vbar-left">
            {vacant ? null : (
              <RecordingBar
                recorder={props.actions.recorder}
                connection={props.connection}
                streaming={props.streaming}
                store={store}
                frameTime={frameTime.current}
              />
            )}
          </div>
          <div className="st-vbar-right">
            {vacant ? null : (
              <CaptureBar
                actions={props.actions}
                streaming={props.streaming}
                connection={props.connection}
                panelHidden={props.panelHidden}
                onTogglePause={props.onTogglePause}
                onCalibrate={props.onCalibrate}
                onExport={props.onExport}
                onTogglePanel={props.onTogglePanel}
                onChangeFolder={props.onChangeFolder}
              />
            )}
          </div>
        </div>

        <div ref={stage.ref} className={stageClass} style={stageStyle}>
          {vacant ? (
            props.stageNotice
          ) : (
            <>
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
                alertIds={props.alertIds}
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
              {props.banner ? (
                <div className="lv-banner" role="alert">
                  <Bell {...SM} />
                  {props.banner}
                </div>
              ) : null}
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
                <button
                  type="button"
                  aria-label="Fit to window"
                  onClick={() => props.onZoomChange(1)}
                >
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
            </>
          )}
        </div>
        {props.chartsOpen && !vacant ? props.drawer : null}
      </div>

      {props.panelHidden || vacant ? null : (
        <SidePanel
          tabs={
            <div className="st-tabs" role="tablist" aria-label="Side panel">
              {tabs.map((t) => (
                <button
                  key={t.id}
                  className="st-tab"
                  type="button"
                  role="tab"
                  aria-selected={props.inspectorTab === t.id}
                  tabIndex={props.inspectorTab === t.id ? 0 : -1}
                  title={t.id === "alarms" && props.alarmTitle ? props.alarmTitle : undefined}
                  onClick={() => props.onInspectorTabChange(t.id)}
                >
                  {t.label}
                  {t.id === "alarms" && props.alarmCount > 0 ? (
                    <>
                      <span className="lv-count" aria-hidden="true">
                        {props.alarmCount}
                      </span>
                      <span className="st-vh">{props.alarmTitle}</span>
                    </>
                  ) : null}
                </button>
              ))}
            </div>
          }
        >
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
        </SidePanel>
      )}
    </div>
  );
}
