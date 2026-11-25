import { Contrast, Eye, EyeOff, Plus, RotateCw, Trash2, Upload } from "lucide-react";
import { useState } from "react";
import { StudioBadge, StudioSwitch } from "@/components/ui/Primitives";
import { paletteGradientCss } from "@/lib/palettes";
import { formatTemp } from "@/lib/thermal";
import type {
  DisplayPipeline,
  IsothermMode,
  MaterialPreset,
  Palette,
  RadiometryParams,
  Roi,
  RoiStats,
  SpatialFilter,
  TempUnit,
  UpscaleMethod,
} from "@/lib/types";

interface InspectorPanelProps {
  palettes: Palette[];
  activePaletteId: string;
  onSelectPalette: (id: string) => void;
  onImportPalette: (name: string, text: string) => Promise<void>;
  materials: MaterialPreset[];
  rad: RadiometryParams;
  onRadChange: (rad: RadiometryParams) => void;
  pipeline: DisplayPipeline;
  onPipelineChange: (p: DisplayPipeline) => void;
  unit: TempUnit;
  onUnitChange: (u: TempUnit) => void;
  rois: Roi[];
  roiStats: RoiStats[];
  selectedRoiId: string | null;
  onSelectRoi: (id: string | null) => void;
  onUpdateRoi: (roi: Roi) => void;
  onDeleteRoi: (id: string) => void;
  onAutoScaleToRoi: (roi: Roi) => void;
}

export function InspectorPanel({
  palettes,
  activePaletteId,
  onSelectPalette,
  onImportPalette,
  materials,
  rad,
  onRadChange,
  pipeline,
  onPipelineChange,
  unit,
  onUnitChange,
  rois,
  roiStats,
  selectedRoiId,
  onSelectRoi,
  onUpdateRoi,
  onDeleteRoi,
  onAutoScaleToRoi,
}: InspectorPanelProps): React.JSX.Element {
  const [showCustomImport, setShowCustomImport] = useState(false);
  const [lutName, setLutName] = useState("Thermal Cyan");
  const [lutText, setLutText] = useState("#060914\n#0e5c8a\n#38d4c8\n#f9d648\n#ffffff");
  const [importErr, setImportErr] = useState<string | null>(null);

  const activePalette = palettes.find((p) => p.id === activePaletteId) ?? palettes[0];
  const selectedRoi = rois.find((r) => r.id === selectedRoiId) ?? null;

  const handleImport = async (): Promise<void> => {
    try {
      setImportErr(null);
      await onImportPalette(lutName, lutText);
      setShowCustomImport(false);
    } catch (err) {
      setImportErr(err instanceof Error ? err.message : String(err));
    }
  };

  return (
    <div className="flex h-full flex-col overflow-y-auto">
      {/* 1. Colormap & Custom 256-px LUT */}
      <section className="border-b border-[var(--tc-line)] p-3">
        <div className="mb-2 flex items-center justify-between">
          <span className="tc-section-label">Colormap</span>
          <button
            type="button"
            onClick={() => setShowCustomImport((v) => !v)}
            className="inline-flex items-center gap-1 text-[11px] font-medium text-[var(--tc-accent)] hover:underline cursor-pointer"
          >
            <Upload className="h-3 w-3" />
            Import LUT
          </button>
        </div>

        <div className="mb-2 flex items-center gap-2">
          <select
            value={activePaletteId}
            onChange={(e) => onSelectPalette(e.target.value)}
            className="tc-select flex-1"
            aria-label="Colormap palette"
          >
            <optgroup label="Perceptually Uniform (2026)">
              {palettes
                .filter((p) => p.category === "Perceptual")
                .map((p) => (
                  <option key={p.id} value={p.id}>
                    {p.label} (256)
                  </option>
                ))}
            </optgroup>
            <optgroup label="Firmware Palettes (19 Single-Source)">
              {palettes
                .filter((p) => p.category === "Firmware")
                .map((p) => (
                  <option key={p.id} value={p.id}>
                    {p.label} ({p.rgb.length})
                  </option>
                ))}
            </optgroup>
            {palettes.some((p) => p.category === "Custom") ? (
              <optgroup label="Custom Imported">
                {palettes
                  .filter((p) => p.category === "Custom")
                  .map((p) => (
                    <option key={p.id} value={p.id}>
                      {p.label} (256)
                    </option>
                  ))}
              </optgroup>
            ) : null}
          </select>
        </div>

        {activePalette ? (
          <div
            className="h-3.5 w-full rounded border border-[var(--tc-border)]"
            style={{ background: paletteGradientCss(activePalette, "to right") }}
          />
        ) : null}

        {showCustomImport ? (
          <div className="mt-2.5 space-y-2 rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)] p-2.5">
            <input
              value={lutName}
              onChange={(e) => setLutName(e.target.value)}
              placeholder="Palette name"
              className="tc-input w-full"
            />
            <textarea
              value={lutText}
              onChange={(e) => setLutText(e.target.value)}
              rows={3}
              className="w-full rounded-md border border-[var(--tc-border)] bg-[var(--tc-surface)] p-2 font-mono-tech text-[11px] text-[var(--tc-text)]"
              placeholder="#RRGGBB or R,G,B stops per line"
            />
            {importErr ? <p className="text-[11px] text-[var(--tc-danger)]">{importErr}</p> : null}
            <button type="button" onClick={handleImport} className="tc-btn tc-btn-primary w-full">
              <Plus className="h-3.5 w-3.5" />
              Add 256-entry LUT
            </button>
          </div>
        ) : null}
      </section>

      {/* 2. Pro-Grade Display Upscaler & Sharpening */}
      <section className="border-b border-[var(--tc-line)] p-3">
        <div className="mb-2 flex items-center justify-between">
          <span className="tc-section-label">Upscaling &amp; Enhance</span>
          <StudioBadge tone="accent">Display-only</StudioBadge>
        </div>
        <div className="grid grid-cols-2 gap-1.5">
          {(
            [
              ["nearest", "Nearest"],
              ["bicubic", "Bicubic"],
              ["lanczos3", "Lanczos-3"],
              ["edge_preserve", "Edge + USM"],
            ] as [UpscaleMethod, string][]
          ).map(([id, label]) => (
            <button
              key={id}
              type="button"
              onClick={() => onPipelineChange({ ...pipeline, upscaler: id })}
              className={`tc-btn text-[12px] ${pipeline.upscaler === id ? "tc-btn-active" : ""}`}
            >
              {label}
            </button>
          ))}
        </div>

        <div className="mt-2.5 space-y-2">
          <div>
            <div className="mb-1 flex justify-between text-[11.5px]">
              <span className="text-[var(--tc-text-muted)]">Unsharp Mask Detail</span>
              <span className="font-mono-tech">{Math.round(pipeline.sharpen_amount * 100)}%</span>
            </div>
            <input
              type="range"
              min={0}
              max={1.5}
              step={0.05}
              value={pipeline.sharpen_amount}
              onChange={(e) =>
                onPipelineChange({ ...pipeline, sharpen_amount: Number(e.target.value) })
              }
              className="w-full accent-[var(--tc-accent)]"
            />
          </div>

          <div className="flex items-center justify-between gap-2">
            <span className="text-[11.5px] text-[var(--tc-text-muted)]">Spatial Denoise</span>
            <select
              value={pipeline.filter}
              onChange={(e) =>
                onPipelineChange({ ...pipeline, filter: e.target.value as SpatialFilter })
              }
              className="tc-select w-[134px]"
            >
              <option value="none">None (Raw)</option>
              <option value="gaussian3x3">Gaussian 3×3</option>
              <option value="median3x3">Median 3×3</option>
              <option value="box3x3">Box 3×3</option>
            </select>
          </div>

          <div className="flex items-center justify-between pt-0.5">
            <StudioSwitch
              checked={pipeline.histogram_eq}
              onCheckedChange={(v) => onPipelineChange({ ...pipeline, histogram_eq: v })}
              label="Histogram Eq (HEQ)"
            />
            <span className="font-mono-tech text-[10px] text-[var(--tc-text-dim)]">CLAHE</span>
          </div>

          <div className="flex items-center justify-between pt-0.5">
            <StudioSwitch
              checked={pipeline.rotate_180}
              onCheckedChange={(v) => onPipelineChange({ ...pipeline, rotate_180: v })}
              label="Rotate 180°"
            />
            <button
              type="button"
              onClick={() =>
                onPipelineChange({ ...pipeline, flip_horizontal: !pipeline.flip_horizontal })
              }
              className={`tc-btn text-[11.5px] ${pipeline.flip_horizontal ? "tc-btn-active" : ""}`}
            >
              <RotateCw className="h-3.5 w-3.5" />
              Mirror
            </button>
          </div>
        </div>
      </section>

      {/* 3. Radiometry & Emissivity */}
      <section className="border-b border-[var(--tc-line)] p-3">
        <div className="mb-2 flex items-center justify-between">
          <span className="tc-section-label">Radiometry</span>
          <div className="flex rounded-md border border-[var(--tc-border)] bg-[var(--tc-surface-2)] p-0.5">
            {(
              [
                ["celsius", "°C"],
                ["fahrenheit", "°F"],
                ["kelvin", "K"],
              ] as [TempUnit, string][]
            ).map(([u, sym]) => (
              <button
                key={u}
                type="button"
                onClick={() => onUnitChange(u)}
                className={`rounded px-2 py-0.5 text-[11px] font-medium cursor-pointer ${
                  unit === u
                    ? "bg-[var(--tc-surface)] text-[var(--tc-text)] shadow-xs"
                    : "text-[var(--tc-text-muted)]"
                }`}
              >
                {sym}
              </button>
            ))}
          </div>
        </div>

        <div className="space-y-2.5">
          <div>
            <div className="mb-1 flex justify-between text-[11.5px]">
              <span className="text-[var(--tc-text-muted)]">Material Preset</span>
              <span className="font-mono-tech">ε = {rad.emissivity.toFixed(2)}</span>
            </div>
            <select
              value={
                materials.find((m) => Math.abs(m.emissivity - rad.emissivity) < 0.005)?.id ??
                "custom"
              }
              onChange={(e) => {
                const preset = materials.find((m) => m.id === e.target.value);
                if (preset) onRadChange({ ...rad, emissivity: preset.emissivity });
              }}
              className="tc-select w-full"
            >
              <option value="custom">Custom Emissivity ({rad.emissivity.toFixed(2)})</option>
              {materials.map((m) => (
                <option key={m.id} value={m.id}>
                  {m.name} ({m.emissivity.toFixed(2)})
                </option>
              ))}
            </select>
          </div>

          <input
            type="range"
            min={0.1}
            max={1.0}
            step={0.01}
            value={rad.emissivity}
            onChange={(e) => onRadChange({ ...rad, emissivity: Number(e.target.value) })}
            className="w-full accent-[var(--tc-accent)]"
          />

          <div className="flex items-center justify-between gap-2">
            <span className="text-[11.5px] text-[var(--tc-text-muted)]">Reflected T_refl (°C)</span>
            <input
              type="number"
              step={0.5}
              value={rad.reflected_temp_c}
              onChange={(e) =>
                onRadChange({ ...rad, reflected_temp_c: Number(e.target.value) || 22 })
              }
              className="tc-input w-20 text-right font-mono-tech"
            />
          </div>
        </div>
      </section>

      {/* 4. Isotherm Overlay */}
      <section className="border-b border-[var(--tc-line)] p-3">
        <div className="mb-2 flex items-center justify-between">
          <span className="tc-section-label">Isotherm Highlight</span>
          <select
            value={pipeline.isotherm.mode}
            onChange={(e) =>
              onPipelineChange({
                ...pipeline,
                isotherm: { ...pipeline.isotherm, mode: e.target.value as IsothermMode },
              })
            }
            className="tc-select w-[112px]"
          >
            <option value="off">Off</option>
            <option value="above">Above</option>
            <option value="below">Below</option>
            <option value="interval">Interval</option>
          </select>
        </div>

        {pipeline.isotherm.mode !== "off" ? (
          <div className="grid grid-cols-2 gap-2 pt-1">
            <div>
              <label className="mb-0.5 block text-[10.5px] text-[var(--tc-text-dim)]">
                Low (°C)
              </label>
              <input
                type="number"
                value={pipeline.isotherm.low_c}
                onChange={(e) =>
                  onPipelineChange({
                    ...pipeline,
                    isotherm: { ...pipeline.isotherm, low_c: Number(e.target.value) },
                  })
                }
                className="tc-input w-full font-mono-tech"
              />
            </div>
            <div>
              <label className="mb-0.5 block text-[10.5px] text-[var(--tc-text-dim)]">
                High (°C)
              </label>
              <input
                type="number"
                value={pipeline.isotherm.high_c}
                onChange={(e) =>
                  onPipelineChange({
                    ...pipeline,
                    isotherm: { ...pipeline.isotherm, high_c: Number(e.target.value) },
                  })
                }
                className="tc-input w-full font-mono-tech"
              />
            </div>
          </div>
        ) : null}
      </section>

      {/* 5. Active ROIs + 1-Click ROI Contrast Auto-Scale + Per-ROI Emissivity */}
      <section className="p-3">
        <div className="mb-2 flex items-center justify-between">
          <span className="tc-section-label">Regions of Interest ({rois.length})</span>
        </div>

        <div className="space-y-1.5">
          {rois.map((roi) => {
            const st = roiStats.find((s) => s.id === roi.id);
            const selected = roi.id === selectedRoiId;
            return (
              <div
                key={roi.id}
                onClick={() => onSelectRoi(roi.id)}
                className={`rounded-lg border p-2 transition-colors cursor-pointer ${
                  selected
                    ? "border-[var(--tc-accent-line)] bg-[var(--tc-accent-soft)]"
                    : "border-[var(--tc-border)] bg-[var(--tc-surface-2)] hover:border-[var(--tc-border-strong)]"
                }`}
              >
                <div className="flex items-center justify-between gap-1">
                  <span className="font-medium text-[12px] truncate">{roi.label}</span>
                  <div className="flex items-center gap-1">
                    <button
                      type="button"
                      onClick={(e) => {
                        e.stopPropagation();
                        onAutoScaleToRoi(roi);
                      }}
                      title="Auto-scale contrast to this ROI"
                      className="rounded p-1 text-[var(--tc-text-muted)] hover:bg-[var(--tc-surface-3)] hover:text-[var(--tc-accent)] cursor-pointer"
                    >
                      <Contrast className="h-3.5 w-3.5" />
                    </button>
                    <button
                      type="button"
                      onClick={(e) => {
                        e.stopPropagation();
                        onUpdateRoi({ ...roi, visible: !roi.visible });
                      }}
                      title={roi.visible ? "Hide ROI" : "Show ROI"}
                      className="rounded p-1 text-[var(--tc-text-muted)] hover:bg-[var(--tc-surface-3)] cursor-pointer"
                    >
                      {roi.visible ? (
                        <Eye className="h-3.5 w-3.5" />
                      ) : (
                        <EyeOff className="h-3.5 w-3.5" />
                      )}
                    </button>
                    <button
                      type="button"
                      onClick={(e) => {
                        e.stopPropagation();
                        onDeleteRoi(roi.id);
                      }}
                      title="Delete ROI"
                      className="rounded p-1 text-[var(--tc-text-muted)] hover:bg-[var(--tc-danger-soft)] hover:text-[var(--tc-danger)] cursor-pointer"
                    >
                      <Trash2 className="h-3.5 w-3.5" />
                    </button>
                  </div>
                </div>
                {st ? (
                  <div className="mt-1 flex items-center justify-between font-mono-tech text-[11px] text-[var(--tc-text-muted)]">
                    <span>
                      avg{" "}
                      <strong className="text-[var(--tc-text)]">
                        {formatTemp(st.mean_c, unit, 1)}
                      </strong>
                    </span>
                    <span>
                      {formatTemp(st.min_c, unit, 1)}..{formatTemp(st.max_c, unit, 1)}
                    </span>
                  </div>
                ) : null}
              </div>
            );
          })}
        </div>

        {selectedRoi ? (
          <div className="mt-3 rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)] p-2.5 space-y-2">
            <div className="flex items-center justify-between">
              <span className="tc-section-label">Per-ROI Override: {selectedRoi.label}</span>
              {selectedRoi.emissivity_override !== null ? (
                <button
                  type="button"
                  onClick={() =>
                    onUpdateRoi({
                      ...selectedRoi,
                      emissivity_override: null,
                      reflected_temp_override_c: null,
                    })
                  }
                  className="text-[11px] text-[var(--tc-accent)] hover:underline cursor-pointer"
                >
                  Reset
                </button>
              ) : null}
            </div>
            <div className="flex items-center justify-between gap-2">
              <span className="text-[11.5px] text-[var(--tc-text-muted)]">ROI Emissivity (ε)</span>
              <input
                type="number"
                min={0.05}
                max={1.0}
                step={0.01}
                value={selectedRoi.emissivity_override ?? rad.emissivity}
                onChange={(e) =>
                  onUpdateRoi({
                    ...selectedRoi,
                    emissivity_override: Math.min(1, Math.max(0.05, Number(e.target.value))),
                  })
                }
                className="tc-input w-20 text-right font-mono-tech"
              />
            </div>
            <div className="flex items-center justify-between gap-2">
              <span className="text-[11.5px] text-[var(--tc-text-muted)]">ROI T_refl (°C)</span>
              <input
                type="number"
                step={0.5}
                value={selectedRoi.reflected_temp_override_c ?? rad.reflected_temp_c}
                onChange={(e) =>
                  onUpdateRoi({
                    ...selectedRoi,
                    reflected_temp_override_c: Number(e.target.value),
                  })
                }
                className="tc-input w-20 text-right font-mono-tech"
              />
            </div>
          </div>
        ) : null}
      </section>
    </div>
  );
}
