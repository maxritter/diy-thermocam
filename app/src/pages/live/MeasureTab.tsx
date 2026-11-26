import { ChevronDown } from "lucide-react";
import { useMemo } from "react";
import type { FrameStore } from "@/lib/frameStore";
import { materialLabel } from "@/lib/palettes";
import { evaluateRoi, formatTemp, fromCelsius, rawToObjectC, unitSymbol } from "@/lib/thermal";
import type { MaterialPreset, RadiometryParams, Roi, RoiStats, TempUnit } from "@/lib/types";
import { useSampledFrame } from "@/lib/useFrame";
import { NumberField, Switch, TempField } from "./fields";
import { RegionRow } from "./RegionRow";

export interface MaterialChoice {
  /** A material from the list, or null for Custom. */
  id: string | null;
  emissivity: number;
  reflected_c: number;
}

export interface MeasureTabProps {
  store: FrameStore;
  rad: RadiometryParams;
  materialId: string | null;
  materials: MaterialPreset[];
  unit: TempUnit;
  onMaterialChange: (m: MaterialChoice) => void;
  rois: Roi[];
  selectedRoiId: string | null;
  onSelectRoi: (id: string | null) => void;
  onRoisChange: (update: (rois: Roi[]) => Roi[]) => void;
  onFitRange: (roi: Roi) => void;
  chartsOpen: boolean;
  onChartsOpenChange: (open: boolean) => void;
  /** Words of the chart switch; null hides it (an opened snapshot has no chart). */
  chartsLabel?: string | null;
  /** Beside "Whole image", e.g. "frame 108" in the recording player. */
  wholeAside?: string;
  /** The Material hint and the empty Regions hint, where an opened capture words them. */
  materialHint?: string;
  regionsHint?: string;
}

const CUSTOM = "";

function Stat({
  k,
  c,
  unit,
  tone,
}: {
  k: string;
  c: number | null;
  unit: TempUnit;
  tone?: "hot" | "cold";
}): React.JSX.Element {
  return (
    <div className="st-stat">
      <div className="k">{k}</div>
      <div className={`v ${tone ?? ""}`}>
        {c === null ? "–" : fromCelsius(c, unit).toFixed(1)}
        <small> {unitSymbol(unit)}</small>
      </div>
    </div>
  );
}

function MaterialFields({
  rad,
  materialId,
  materials,
  unit,
  onMaterialChange,
}: Pick<MeasureTabProps, "rad" | "materialId" | "materials" | "unit" | "onMaterialChange">) {
  return (
    <>
      <label className="st-field">
        <span className="st-vh">Material</span>
        <select
          className="st-select"
          aria-label="Material"
          value={materialId ?? CUSTOM}
          onChange={(e) => {
            const m = materials.find((x) => x.id === e.target.value);
            onMaterialChange(
              m
                ? { id: m.id, emissivity: m.emissivity, reflected_c: rad.reflected_temp_c }
                : { id: null, emissivity: rad.emissivity, reflected_c: rad.reflected_temp_c },
            );
          }}
        >
          <option value={CUSTOM}>Custom</option>
          {materials.map((m) => (
            <option key={m.id} value={m.id}>
              {materialLabel(m)}
            </option>
          ))}
        </select>
      </label>
      <div className="st-two">
        <label className="st-field">
          <span>Emissivity</span>
          <NumberField
            label="Emissivity"
            value={rad.emissivity}
            decimals={2}
            onCommit={(v) =>
              onMaterialChange({
                id: null,
                emissivity: Math.max(0.05, Math.min(1, v)),
                reflected_c: rad.reflected_temp_c,
              })
            }
          />
        </label>
        <label className="st-field">
          <span>Reflected</span>
          <TempField
            label="Reflected"
            valueC={rad.reflected_temp_c}
            unit={unit}
            onCommit={(c) =>
              onMaterialChange({ id: materialId, emissivity: rad.emissivity, reflected_c: c })
            }
          />
        </label>
      </div>
    </>
  );
}

/** Live's Measure tab: the whole image, the material, and every region (live.html#measuring). */
export function MeasureTab(props: MeasureTabProps): React.JSX.Element {
  const { store, rad, materials, unit, rois, selectedRoiId } = props;
  const frame = useSampledFrame(store);

  const whole = useMemo(() => {
    if (!frame) return null;
    const cx = Math.floor(frame.width / 2);
    const cy = Math.floor(frame.height / 2);
    return {
      hot: rawToObjectC(frame.max_raw, rad),
      cold: rawToObjectC(frame.min_raw, rad),
      center: rawToObjectC(frame.pixels[cy * frame.width + cx], rad),
    };
  }, [frame, rad]);

  const stats = useMemo(() => {
    const map = new Map<string, RoiStats>();
    if (!frame) return map;
    for (const r of rois)
      map.set(r.id, evaluateRoi(r, frame.pixels, frame.width, frame.height, rad));
    return map;
  }, [frame, rois, rad]);

  const material = materials.find((m) => m.id === props.materialId);
  const summary = `${material ? material.name : "Custom"} · ε ${rad.emissivity.toFixed(2)} · ${formatTemp(rad.reflected_temp_c, unit)}`;

  return (
    <div role="tabpanel" aria-label="Measure">
      <div className="st-grp">
        <div className="st-grp-h">
          <h3>Whole image</h3>
          {props.wholeAside ? <span className="aside">{props.wholeAside}</span> : null}
        </div>
        <div className="st-stats">
          <Stat k="Hottest" c={whole?.hot ?? null} unit={unit} tone="hot" />
          <Stat k="Coldest" c={whole?.cold ?? null} unit={unit} tone="cold" />
          <Stat k="Center" c={whole?.center ?? null} unit={unit} />
        </div>
      </div>

      {rois.length === 0 ? (
        <div className="st-grp">
          <div className="st-grp-h">
            <h3>Material</h3>
          </div>
          <MaterialFields {...props} />
          <p className="st-hint">
            {props.materialHint ??
              "Choose what you are measuring so readings come out right; a surface fills in its emissivity, typing a value makes it Custom. Reflected is usually the room temperature."}
          </p>
        </div>
      ) : (
        <details
          className="st-more"
          style={{ borderTop: 0, borderBottom: "1px solid var(--tc-line)" }}
        >
          <summary>
            Material<span className="aside">{summary}</span>
            <ChevronDown size={14} strokeWidth={1.75} aria-hidden="true" />
          </summary>
          <div className="st-grp">
            <MaterialFields {...props} />
          </div>
        </details>
      )}

      <div className="st-grp">
        <div className="st-grp-h">
          <h3>Regions</h3>
          {rois.length > 0 ? <span className="aside">{rois.length}</span> : null}
        </div>
        {rois.length === 0 ? (
          <p className="st-hint">
            {props.regionsHint ??
              "None yet. Pick a tool on the left of the image, then click or drag on it."}
          </p>
        ) : (
          <div className="st-rows">
            {rois.map((roi, i) => (
              <RegionRow
                key={roi.id}
                roi={roi}
                index={i}
                stats={stats.get(roi.id)}
                rad={rad}
                materials={materials}
                unit={unit}
                selected={roi.id === selectedRoiId}
                onSelect={() => props.onSelectRoi(roi.id === selectedRoiId ? null : roi.id)}
                onChange={(next) =>
                  props.onRoisChange((prev) => prev.map((r) => (r.id === next.id ? next : r)))
                }
                onDelete={() => {
                  props.onRoisChange((prev) => prev.filter((r) => r.id !== roi.id));
                  if (roi.id === selectedRoiId) props.onSelectRoi(null);
                }}
                onFitRange={() => props.onFitRange(roi)}
              />
            ))}
          </div>
        )}
      </div>

      {props.chartsLabel === null ? null : (
        <div className="st-grp">
          <div className="st-setrow">
            <span className="lbl">{props.chartsLabel ?? "Charts below the image"}</span>
            <Switch
              checked={props.chartsOpen}
              onChange={props.onChartsOpenChange}
              label={props.chartsLabel ?? "Charts below the image"}
            />
          </div>
        </div>
      )}
    </div>
  );
}
