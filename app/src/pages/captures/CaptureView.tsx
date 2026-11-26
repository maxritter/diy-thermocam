import { Maximize, Minus, PanelRight, Plus } from "lucide-react";
import { type CSSProperties, type ReactNode, useEffect, useMemo, useRef, useState } from "react";
import { isTypingTarget, matchShortcut, type ShortcutId } from "@/app/shortcuts";
import { ColorbarHistogram } from "@/components/ColorbarHistogram";
import { Loupe } from "@/components/Loupe";
import { ThermalViewport } from "@/components/ThermalViewport";
import { Toolbar } from "@/components/viewport/Toolbar";
import type { ColorRange } from "@/lib/colorRange";
import type { FrameStore } from "@/lib/frameStore";
import { DEFAULT_MATERIALS } from "@/lib/palettes";
import { roiContrastRange } from "@/lib/thermal";
import type {
  DisplayPipeline,
  Material,
  Palette,
  RadiometryParams,
  Roi,
  RoiTool,
  TempUnit,
} from "@/lib/types";
import { CursorReading } from "@/pages/live/CursorReading";
import { ImageTab } from "@/pages/live/ImageTab";
import { useStageLayout } from "@/pages/live/LivePage";
import { MeasureTab } from "@/pages/live/MeasureTab";
import { celsiusRange, type Edits, radFor, rawRange } from "./edits";

const ICON = { size: 16, strokeWidth: 1.75, "aria-hidden": true } as const;
const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

export const DEFAULT_PIPELINE: DisplayPipeline = {
  filter: "none",
  upscaler: "edge_preserve",
  sharpen_amount: 0.45,
  rotate_180: false,
  flip_horizontal: false,
  histogram_eq: false,
  isotherm: { mode: "off", low_c: 40, high_c: 60, color: [0, 230, 160] },
};

const TOOL_KEYS: Partial<Record<ShortcutId, RoiTool>> = {
  "tool-select": "select",
  "tool-spot": "spot",
  "tool-box": "box",
  "tool-circle": "circle",
  "tool-polygon": "polygon",
  "tool-line": "line",
  "tool-delta": "delta",
};

export type SideTab = "image" | "measure" | "notes";

/** What the viewer hands to a compare view: the left image and how it is drawn. */
export interface ImageSlot {
  viewport: ReactNode;
  palette: Palette;
  range: ColorRange;
  rad: RadiometryParams;
}

export interface CaptureViewProps {
  store: FrameStore;
  edits: Edits;
  onEdit: (fn: (e: Edits) => Edits, mergeKey?: string) => void;
  sensorMaterial: Material | null;
  palettes: Palette[];
  recentPaletteIds: string[];
  onImportPalette: (name: string, text: string) => Promise<void>;
  unit: TempUnit;
  onUnitChange?: (unit: TempUnit) => void;
  engineeringDetails: boolean;
  /** The bar above the image: back, title, and the buttons on the right. */
  barLeft: ReactNode;
  barRight: (panel: { hidden: boolean; toggle: () => void; button: ReactNode }) => ReactNode;
  /** Replaces the image (Compare); gets the viewer's own image to put on the left. */
  renderImage?: (slot: ImageSlot) => ReactNode;
  /** Laid over the stage (a set-aside edits file, a message instead of the image). */
  stageOverlay?: ReactNode;
  /** The image is not shown at all (a damaged file). */
  noImage?: boolean;
  /** Under the stage: the player and the chart. */
  below?: ReactNode;
  /** The stage leaves room for what is below it. */
  short?: boolean;
  measure?: {
    chartsLabel: string | null;
    chartsOpen: boolean;
    onChartsOpenChange: (open: boolean) => void;
    wholeAside?: string;
    regionsHint: string;
  };
  notes: ReactNode;
  initialTab?: SideTab;
  /** Keys the viewer does not use itself (the player's , . and Space). */
  onKey?: (e: KeyboardEvent, id: ShortcutId | null) => boolean;
  onPrevious?: () => void;
  onNext?: () => void;
  onUndo: () => void;
  onRedo: () => void;
  imageLabel: string;
}

/**
 * The Live viewer with a capture in it (captures.html#opened, #recording): the tool rail, the
 * image and color scale, and the side panel's Image, Measure and Notes.
 */
export function CaptureView(props: CaptureViewProps): React.JSX.Element {
  const { store, edits, onEdit, sensorMaterial, unit } = props;
  const [tool, setTool] = useState<RoiTool>("select");
  const [selectedRoiId, setSelectedRoiId] = useState<string | null>(null);
  const [showHotCold, setShowHotCold] = useState(true);
  const [showLoupe, setShowLoupe] = useState(false);
  const [showGrid, setShowGrid] = useState(false);
  const [gridSize, setGridSize] = useState<[number, number]>([4, 3]);
  const [zoom, setZoom] = useState(1);
  const [pipeline, setPipeline] = useState<DisplayPipeline>(DEFAULT_PIPELINE);
  const [tab, setTab] = useState<SideTab>(props.initialTab ?? "image");
  const [panelHidden, setPanelHidden] = useState(false);
  const stage = useStageLayout(Boolean(props.short));

  const frameCal = store.getFrame();
  const calOffset = frameCal?.cal_offset ?? -273.15;
  const calSlope = frameCal?.cal_slope ?? 0.01;
  const rad = useMemo(
    () => radFor({ cal_offset: calOffset, cal_slope: calSlope }, edits, sensorMaterial),
    [calOffset, calSlope, edits, sensorMaterial],
  );
  const range = useMemo(() => rawRange(edits, rad), [edits, rad]);
  const palette = props.palettes.find((p) => p.id === edits.paletteId) ?? props.palettes[0];

  const onRoisChange = (fn: (rois: Roi[]) => Roi[]) => onEdit((e) => ({ ...e, rois: fn(e.rois) }));
  const onRangeChange = (r: ColorRange) =>
    onEdit((e) => ({ ...e, range: celsiusRange(r, radFor(store.getFrame(), e, sensorMaterial)) }));

  // The page's own keys: Live's tools and toggles, Undo and Redo, the arrows between captures.
  const live = useRef(props);
  live.current = props;
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.isComposing || isTypingTarget(e.target)) return;
      if (document.querySelector("[role=dialog], [role=alertdialog], [role=menu]")) return;
      const id = matchShortcut(e);
      const p = live.current;
      if (p.onKey?.(e, id)) {
        e.preventDefault();
        return;
      }
      if (!id) return;
      const t = TOOL_KEYS[id];
      if (t) setTool(t);
      else if (id === "toggle-hotcold") setShowHotCold((v) => !v);
      else if (id === "toggle-loupe") setShowLoupe((v) => !v);
      else if (id === "toggle-grid") setShowGrid((v) => !v);
      else if (id === "toggle-panel") setPanelHidden((v) => !v);
      else if (id === "zoom-in") setZoom((z) => Math.min(2.5, z + 0.25));
      else if (id === "zoom-out") setZoom((z) => Math.max(0.75, z - 0.25));
      else if (id === "zoom-fit") setZoom(1);
      else if (id === "auto-range") p.onEdit((ed) => ({ ...ed, range: { mode: "auto" } }));
      else if (id === "undo") p.onUndo();
      else if (id === "redo") p.onRedo();
      else if (id === "previous-capture" && p.onPrevious) p.onPrevious();
      else if (id === "next-capture" && p.onNext) p.onNext();
      else return;
      e.preventDefault();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  const viewport = (
    <ThermalViewport
      store={store}
      palette={palette}
      range={range}
      rad={rad}
      pipeline={pipeline}
      unit={unit}
      rois={edits.rois}
      selectedRoiId={selectedRoiId}
      tool={tool}
      showHotCold={showHotCold}
      showGrid={showGrid}
      gridCols={gridSize[0]}
      gridRows={gridSize[1]}
      zoom={props.renderImage ? 1 : zoom}
      label={props.imageLabel}
      style={props.renderImage ? { position: "absolute", inset: 0, height: "100%" } : undefined}
      onRoisChange={onRoisChange}
      onSelectRoi={setSelectedRoiId}
      onToolChange={setTool}
    />
  );

  const panelButton = (
    <button
      className="st-icon-btn"
      type="button"
      aria-label={panelHidden ? "Show side panel" : "Hide side panel"}
      aria-pressed={panelHidden}
      title="Hide side panel · ⌘\"
      onClick={() => setPanelHidden((v) => !v)}
    >
      <PanelRight {...ICON} />
    </button>
  );

  const stageStyle = { "--ih": `${stage.ih}px`, "--cbh": `${stage.cbh}px` } as CSSProperties;
  const tabs: { id: SideTab; label: string }[] = [
    { id: "image", label: "Image" },
    { id: "measure", label: "Measure" },
    { id: "notes", label: "Notes" },
  ];

  return (
    <div className={panelHidden || props.noImage ? "st-body cp-view full" : "st-body cp-view"}>
      <div
        className="st-viewer"
        style={props.below ? { gridTemplateRows: "52px minmax(0, 1fr) auto auto" } : undefined}
      >
        <div className="st-vbar">
          <div className="st-vbar-left" style={{ minWidth: 0 }}>
            {props.barLeft}
          </div>
          <div className="st-vbar-right">
            {props.barRight({
              hidden: panelHidden,
              toggle: () => setPanelHidden((v) => !v),
              button: props.noImage ? null : panelButton,
            })}
          </div>
        </div>
        <div
          ref={stage.ref}
          className={`st-stage${props.short ? " short" : ""}${stage.zoomRight ? " zoom-right" : ""}`}
          style={stageStyle}
        >
          {props.noImage ? null : (
            <>
              <Toolbar
                tool={tool}
                onToolChange={setTool}
                showHotCold={showHotCold}
                onToggleHotCold={() => setShowHotCold((v) => !v)}
                showLoupe={showLoupe}
                onToggleLoupe={() => setShowLoupe((v) => !v)}
                showGrid={showGrid}
                gridSize={gridSize}
                onGridChange={(show, size) => {
                  setShowGrid(show);
                  if (size) setGridSize(size);
                }}
              />
              <span aria-hidden="true" />
              {props.renderImage ? props.renderImage({ viewport, palette, range, rad }) : viewport}
              <ColorbarHistogram
                store={store}
                palette={palette}
                range={range}
                onRangeChange={onRangeChange}
                rad={rad}
                unit={unit}
                onUnitChange={props.onUnitChange}
                boosted={pipeline.histogram_eq}
              />
              <div className="st-zoom">
                <button
                  type="button"
                  aria-label="Zoom out"
                  onClick={() => setZoom((z) => Math.max(0.75, z - 0.25))}
                >
                  <Minus {...SM} />
                </button>
                <span>{Math.round(zoom * 100)}%</span>
                <button
                  type="button"
                  aria-label="Zoom in"
                  onClick={() => setZoom((z) => Math.min(2.5, z + 0.25))}
                >
                  <Plus {...SM} />
                </button>
                <button type="button" aria-label="Fit to window" onClick={() => setZoom(1)}>
                  <Maximize {...SM} />
                </button>
              </div>
              <CursorReading
                store={store}
                rad={rad}
                unit={unit}
                tool={tool}
                engineeringDetails={props.engineeringDetails}
              />
              {showLoupe ? (
                <Loupe
                  store={store}
                  palette={palette}
                  range={range}
                  rad={rad}
                  unit={unit}
                  engineeringDetails={props.engineeringDetails}
                />
              ) : null}
            </>
          )}
          {props.stageOverlay}
        </div>
        {props.below}
      </div>

      {panelHidden || props.noImage ? null : (
        <aside className="st-insp" aria-label="Side panel">
          <div className="st-insp-tabs">
            <div className="st-tabs" role="tablist" aria-label="Side panel">
              {tabs.map((t) => (
                <button
                  key={t.id}
                  className="st-tab"
                  type="button"
                  role="tab"
                  aria-selected={tab === t.id}
                  tabIndex={tab === t.id ? 0 : -1}
                  onClick={() => setTab(t.id)}
                >
                  {t.label}
                </button>
              ))}
            </div>
          </div>
          <div className="st-insp-body">
            {tab === "image" ? (
              <ImageTab
                store={store}
                palettes={props.palettes}
                paletteId={palette.id}
                recentPaletteIds={props.recentPaletteIds}
                onSelectPalette={(id) => onEdit((e) => ({ ...e, paletteId: id }))}
                onImportPalette={props.onImportPalette}
                range={range}
                onRangeChange={onRangeChange}
                rad={rad}
                unit={unit}
                pipeline={pipeline}
                onPipelineChange={setPipeline}
              />
            ) : tab === "measure" ? (
              <MeasureTab
                store={store}
                rad={rad}
                materialId={edits.material.id}
                materials={DEFAULT_MATERIALS}
                unit={unit}
                onMaterialChange={(m) =>
                  onEdit((e) => ({
                    ...e,
                    material: { id: m.id, emissivity: m.emissivity, reflected_c: m.reflected_c },
                  }))
                }
                rois={edits.rois}
                selectedRoiId={selectedRoiId}
                onSelectRoi={setSelectedRoiId}
                onRoisChange={onRoisChange}
                onFitRange={(roi) => {
                  const fr = store.getFrame();
                  if (!fr) return;
                  const r = roiContrastRange(roi, fr.pixels, fr.width, fr.height, 0.06);
                  if (r) onRangeChange({ mode: "manual", lo: r[0], hi: r[1] });
                }}
                chartsOpen={props.measure?.chartsOpen ?? false}
                onChartsOpenChange={props.measure?.onChartsOpenChange ?? (() => {})}
                chartsLabel={props.measure?.chartsLabel ?? null}
                wholeAside={props.measure?.wholeAside}
                materialHint="Changing these recalculates every reading from the capture’s raw data."
                regionsHint={
                  props.measure?.regionsHint ??
                  "None yet. Pick a tool on the left of the image, then click or drag on it. Regions are saved with the capture."
                }
              />
            ) : (
              <div role="tabpanel" aria-label="Notes">
                {props.notes}
              </div>
            )}
          </div>
        </aside>
      )}
    </div>
  );
}
