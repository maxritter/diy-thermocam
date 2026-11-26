import { Contrast, Eye, EyeOff, Trash2 } from "lucide-react";
import { useRef, useState } from "react";
import { formatDifference } from "@/components/viewport/RegionLayer";
import { materialLabel } from "@/lib/palettes";
import { regionKindLabel, roiColor, rowStat, rowValue } from "@/lib/regions";
import { fromCelsius, unitSymbol } from "@/lib/thermal";
import type { MaterialPreset, RadiometryParams, Roi, RoiStats, TempUnit } from "@/lib/types";
import { NumberField, TempField } from "./fields";

const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

export interface RegionRowProps {
  roi: Roi;
  index: number;
  stats: RoiStats | undefined;
  /** The image's material, which "Same as image" follows. */
  rad: RadiometryParams;
  materials: MaterialPreset[];
  unit: TempUnit;
  selected: boolean;
  onSelect: () => void;
  onChange: (roi: Roi) => void;
  onDelete: () => void;
  onFitRange: () => void;
}

const SAME = "__same__";
const CUSTOM = "__custom__";

function RowValue({
  roi,
  stats,
  unit,
}: {
  roi: Roi;
  stats: RoiStats | undefined;
  unit: TempUnit;
}): React.JSX.Element {
  const v = stats && stats.pixel_count > 0 ? rowValue(roi.geometry.kind, stats) : null;
  const stat = rowStat(roi.geometry.kind);
  if (v === null) return <span className="val">–</span>;
  if (stat === "delta") return <span className="val">{formatDifference(v, unit)}</span>;
  return (
    <span className="val">
      <small>{stat}</small>
      {fromCelsius(v, unit).toFixed(1)} {unitSymbol(unit)}
    </span>
  );
}

function RenameField({ name, onDone }: { name: string; onDone: (name: string | null) => void }) {
  const done = useRef(false);
  const finish = (v: string | null) => {
    if (done.current) return;
    done.current = true;
    onDone(v);
  };
  return (
    <input
      className="st-input"
      style={{ height: 24, padding: "0 6px" }}
      aria-label="Region name"
      defaultValue={name}
      // biome-ignore lint/a11y/noAutofocus: the field opens because the person asked to rename.
      autoFocus
      onFocus={(e) => e.currentTarget.select()}
      onKeyDown={(e) => {
        e.stopPropagation();
        if (e.key === "Enter") finish(e.currentTarget.value);
        if (e.key === "Escape") finish(null);
      }}
      onBlur={(e) => finish(e.currentTarget.value)}
    />
  );
}

/** One region in the Measure tab; the selected one opens to its numbers and own material. */
export function RegionRow({
  roi,
  index,
  stats,
  rad,
  materials,
  unit,
  selected,
  onSelect,
  onChange,
  onDelete,
  onFitRange,
}: RegionRowProps): React.JSX.Element {
  const [renaming, setRenaming] = useState(false);
  const kind = regionKindLabel(roi.geometry.kind);
  const own = roi.emissivity_override !== null || roi.reflected_temp_override_c !== null;
  const materialValue = !own ? SAME : (roi.material_id ?? CUSTOM);
  const fmt = (c: number | undefined) =>
    c === undefined || !stats || stats.pixel_count === 0 ? "–" : fromCelsius(c, unit).toFixed(1);
  const sigma =
    stats && stats.pixel_count > 1
      ? unit === "fahrenheit"
        ? (stats.std_dev_c * 9) / 5
        : stats.std_dev_c
      : 0;

  const setMaterial = (value: string) => {
    if (value === SAME) {
      onChange({
        ...roi,
        emissivity_override: null,
        reflected_temp_override_c: null,
        material_id: null,
      });
      return;
    }
    if (value === CUSTOM) {
      onChange({
        ...roi,
        emissivity_override: roi.emissivity_override ?? rad.emissivity,
        reflected_temp_override_c: roi.reflected_temp_override_c ?? rad.reflected_temp_c,
        material_id: null,
      });
      return;
    }
    const m = materials.find((x) => x.id === value);
    if (!m) return;
    onChange({
      ...roi,
      emissivity_override: m.emissivity,
      reflected_temp_override_c: roi.reflected_temp_override_c ?? rad.reflected_temp_c,
      material_id: m.id,
    });
  };

  const head = (
    <>
      <span className="sd" style={{ background: roiColor(roi, index) }} />
      <span
        style={{ minWidth: 0, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}
      >
        {renaming ? (
          <RenameField
            name={roi.label}
            onDone={(name) => {
              setRenaming(false);
              const next = name?.trim();
              if (next) onChange({ ...roi, label: next });
            }}
          />
        ) : (
          <>
            <span
              className="nm"
              onDoubleClick={(e) => {
                e.stopPropagation();
                setRenaming(true);
              }}
            >
              {roi.label}
            </span>
            <span className="kind">{kind}</span>
          </>
        )}
      </span>
      <RowValue roi={roi} stats={stats} unit={unit} />
    </>
  );

  return (
    <>
      {renaming ? (
        <div className="st-row-btn" data-region-row={roi.id} aria-current={selected}>
          {head}
        </div>
      ) : (
        <button
          className="st-row-btn"
          type="button"
          data-region-row={roi.id}
          aria-current={selected}
          onClick={onSelect}
        >
          {head}
        </button>
      )}
      {selected ? (
        <div className="st-row-detail">
          <div className="st-mini-stats">
            <span>
              min <b>{fmt(stats?.min_c)}</b>
            </span>
            <span>
              avg <b>{fmt(stats?.mean_c)}</b>
            </span>
            <span>
              max <b>{fmt(stats?.max_c)}</b>
            </span>
            <span>{unitSymbol(unit)}</span>
          </div>
          <div className="st-mini-stats">
            <span>
              spread σ <b>{sigma.toFixed(2)}</b>
            </span>
            <span>
              pixels <b>{stats?.pixel_count ?? 0}</b>
            </span>
            <span>
              ε <b>{(stats?.effective_emissivity ?? rad.emissivity).toFixed(2)}</b>
            </span>
          </div>
          <div className="st-two wide-first">
            <label className="st-field">
              <span>Material</span>
              <select
                className="st-select"
                aria-label={`Material of ${roi.label}`}
                value={materialValue}
                onChange={(e) => setMaterial(e.target.value)}
              >
                <option value={SAME}>Same as image</option>
                {materials.map((m) => (
                  <option key={m.id} value={m.id}>
                    {materialLabel(m)}
                  </option>
                ))}
                <option value={CUSTOM}>Custom</option>
              </select>
            </label>
            <label className="st-field">
              <span>Reflected</span>
              <TempField
                label={`Reflected for ${roi.label}`}
                valueC={roi.reflected_temp_override_c ?? rad.reflected_temp_c}
                unit={unit}
                onCommit={(c) =>
                  onChange({
                    ...roi,
                    reflected_temp_override_c: c,
                    emissivity_override: roi.emissivity_override ?? rad.emissivity,
                    material_id: own ? (roi.material_id ?? null) : null,
                  })
                }
              />
            </label>
          </div>
          {materialValue === CUSTOM ? (
            <label className="st-field">
              <span>Emissivity</span>
              <NumberField
                label={`Emissivity of ${roi.label}`}
                value={roi.emissivity_override ?? rad.emissivity}
                decimals={2}
                onCommit={(v) =>
                  onChange({
                    ...roi,
                    emissivity_override: Math.max(0.05, Math.min(1, v)),
                    material_id: null,
                  })
                }
              />
            </label>
          ) : null}
          {roi.geometry.kind === "spot" ? (
            <div className="st-setrow">
              <span className="lbl">Spot size</span>
              <fieldset className="st-seg" aria-label="Spot size">
                {(
                  [
                    [1, "1 pixel"],
                    [3, "3 × 3"],
                  ] as const
                ).map(([k, text]) => (
                  <button
                    key={k}
                    type="button"
                    aria-pressed={
                      (roi.geometry.kind === "spot" && roi.geometry.kernel >= 3) === (k === 3)
                    }
                    onClick={() =>
                      roi.geometry.kind === "spot" &&
                      onChange({ ...roi, geometry: { ...roi.geometry, kernel: k } })
                    }
                  >
                    {text}
                  </button>
                ))}
              </fieldset>
            </div>
          ) : null}
          <div className="st-row-actions">
            <button className="tc-btn tc-btn-sm" type="button" onClick={onFitRange}>
              <Contrast {...SM} />
              Fit range to {roi.label}
            </button>
            <span style={{ flex: 1 }} />
            <button
              className="st-icon-btn"
              type="button"
              aria-label={roi.visible ? `Hide ${roi.label}` : `Show ${roi.label}`}
              onClick={() => onChange({ ...roi, visible: !roi.visible })}
            >
              {roi.visible ? <Eye {...SM} /> : <EyeOff {...SM} />}
            </button>
            <button
              className="st-icon-btn"
              type="button"
              aria-label={`Delete ${roi.label}`}
              onClick={onDelete}
            >
              <Trash2 {...SM} />
            </button>
          </div>
        </div>
      ) : null}
    </>
  );
}
