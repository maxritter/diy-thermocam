import { Popover } from "@base-ui/react/popover";
import {
  ChartColumn,
  FlipHorizontal2,
  Focus,
  Highlighter,
  Image as ImageIcon,
  Lock,
  Palette as PaletteIcon,
  RotateCw,
  Scaling,
  Sparkles,
  Thermometer,
  Upload,
  Waves,
} from "lucide-react";
import { type ChangeEvent, useMemo, useRef, useState } from "react";
import { Group } from "@/app/ui/Group";
import { Segmented } from "@/app/ui/Segmented";
import { SettingRow } from "@/app/ui/SettingRow";
import { type ColorRange, RAW_LIMITS, rangeAfterDragStart, shownRange } from "@/lib/colorRange";
import type { FrameStore } from "@/lib/frameStore";
import { paletteGradientCss } from "@/lib/palettes";
import { objectCToRaw, rawToObjectC } from "@/lib/thermal";
import type {
  DisplayPipeline,
  IsothermMode,
  Palette,
  RadiometryParams,
  SpatialFilter,
  TempUnit,
  UpscaleMethod,
} from "@/lib/types";
import { useSampledFrame } from "@/lib/useFrame";
import { Switch, TempField } from "./fields";

export interface ImageTabProps {
  store: FrameStore;
  palettes: Palette[];
  paletteId: string;
  /** The palettes used last, shown as swatches (at most 12). */
  recentPaletteIds: string[];
  onSelectPalette: (id: string) => void;
  onImportPalette: (name: string, text: string) => Promise<void>;
  range: ColorRange;
  onRangeChange: (range: ColorRange) => void;
  rad: RadiometryParams;
  unit: TempUnit;
  pipeline: DisplayPipeline;
  onPipelineChange: (pipeline: DisplayPipeline) => void;
}

const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

/** The swatches the tab starts with, in the artifact's order. */
export const STARTING_PALETTES = [
  "inferno",
  "ironblack",
  "rainbow",
  "whiteHot",
  "blackHot",
  "arctic",
  "lava",
  "medical",
  "magma",
  "plasma",
  "viridis",
  "turbo",
];

const RECENT_KEY = "thermocam.recentPalettes";

/** The palettes used last; kept in this browser's storage only, as a convenience. */
export function readRecentPalettes(): string[] {
  try {
    const v = JSON.parse(window.localStorage.getItem(RECENT_KEY) ?? "null");
    if (Array.isArray(v) && v.every((x) => typeof x === "string")) return v.slice(0, 12);
  } catch {
    // Storage can be unavailable or damaged; start from the default swatches.
  }
  return STARTING_PALETTES;
}

/** Puts a palette in the swatch grid: a new one replaces the one used longest ago. */
export function rememberPalette(list: string[], id: string): string[] {
  if (list.includes(id)) return list;
  const next = [id, ...list].slice(0, 12);
  try {
    window.localStorage.setItem(RECENT_KEY, JSON.stringify(next));
  } catch {
    // Not saved; the grid still updates for this session.
  }
  return next;
}

/** The four upscalers the pipeline offers, by their names. */
export const UPSCALERS: { id: UpscaleMethod; label: string }[] = [
  { id: "nearest", label: "Nearest" },
  { id: "bicubic", label: "Bicubic" },
  { id: "lanczos3", label: "Lanczos-3" },
  { id: "edge_preserve", label: "Edge + USM" },
];

/** The spatial filters, Off first. */
export const DENOISE: { id: SpatialFilter; label: string }[] = [
  { id: "none", label: "Off" },
  { id: "gaussian3x3", label: "Gaussian 3×3" },
  { id: "median3x3", label: "Median 3×3" },
  { id: "box3x3", label: "Box 3×3" },
];

const HIGHLIGHT: { id: IsothermMode; label: string }[] = [
  { id: "off", label: "Off" },
  { id: "above", label: "Above a temperature" },
  { id: "below", label: "Below a temperature" },
  { id: "interval", label: "Between two temperatures" },
];

function Swatch({
  palette,
  pressed,
  onClick,
}: {
  palette: Palette;
  pressed: boolean;
  onClick: () => void;
}): React.JSX.Element {
  return (
    <button className="st-sw" type="button" aria-pressed={pressed} onClick={onClick}>
      <span className="chip" style={{ background: paletteGradientCss(palette, "to right") }} />
      <span className="nm" title={palette.label}>
        {palette.label}
      </span>
    </button>
  );
}

/** The "All palettes" popover (live.html#menus). */
function AllPalettes({
  palettes,
  paletteId,
  onSelectPalette,
  onImportPalette,
}: Pick<ImageTabProps, "palettes" | "paletteId" | "onSelectPalette" | "onImportPalette">) {
  const [query, setQuery] = useState("");
  const [expanded, setExpanded] = useState<Record<string, boolean>>({});
  const [error, setError] = useState<string | null>(null);
  const fileRef = useRef<HTMLInputElement | null>(null);
  const q = query.trim().toLowerCase();
  const match = (p: Palette) => !q || p.label.toLowerCase().includes(q);
  const groups: { key: string; title: string; items: Palette[]; first: number }[] = [
    {
      key: "camera",
      title: "From the camera",
      items: palettes.filter((p) => p.category === "Firmware"),
      first: 7,
    },
    {
      key: "even",
      title: "Even steps",
      items: palettes.filter((p) => p.category === "Perceptual"),
      first: 4,
    },
  ];
  const yours = palettes.filter((p) => p.category === "Custom" && match(p));

  const onFile = async (e: ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    e.target.value = "";
    if (!file) return;
    try {
      setError(null);
      await onImportPalette(file.name.replace(/\.[^.]+$/, ""), await file.text());
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    }
  };

  return (
    <Popover.Root>
      <Popover.Trigger className="st-link">All palettes</Popover.Trigger>
      <Popover.Portal>
        <Popover.Positioner side="left" align="start" sideOffset={12} className="z-50">
          <Popover.Popup className="st-pop" style={{ width: 340 }} aria-label="All palettes">
            <div className="pop-row" style={{ padding: "6px 6px 2px" }}>
              <span className="st-unit-in" data-unit="" style={{ width: "100%" }}>
                <input
                  className="st-input"
                  placeholder="Search palettes"
                  aria-label="Search palettes"
                  value={query}
                  onChange={(e) => setQuery(e.target.value)}
                />
              </span>
            </div>
            {groups.map((g) => {
              const items = g.items.filter(match);
              if (items.length === 0) return null;
              const open = expanded[g.key] || q !== "" || items.length <= g.first + 1;
              const shown = open ? items : items.slice(0, g.first);
              return (
                <div key={g.key}>
                  <div className="pop-h">
                    {g.title} · {g.items.length}
                  </div>
                  <div className="st-swatches" style={{ padding: "2px 10px 6px" }}>
                    {shown.map((p) => (
                      <Swatch
                        key={p.id}
                        palette={p}
                        pressed={p.id === paletteId}
                        onClick={() => onSelectPalette(p.id)}
                      />
                    ))}
                    {open ? null : (
                      <button
                        className="st-link"
                        type="button"
                        style={{ fontSize: 12, justifyContent: "center" }}
                        onClick={() => setExpanded((x) => ({ ...x, [g.key]: true }))}
                      >
                        + {items.length - g.first} more
                      </button>
                    )}
                  </div>
                </div>
              );
            })}
            <div className="pop-h">Yours</div>
            {yours.length > 0 ? (
              <div className="st-swatches" style={{ padding: "2px 10px 6px" }}>
                {yours.map((p) => (
                  <Swatch
                    key={p.id}
                    palette={p}
                    pressed={p.id === paletteId}
                    onClick={() => onSelectPalette(p.id)}
                  />
                ))}
              </div>
            ) : null}
            <div style={{ padding: "2px 10px 8px", display: "flex", gap: 8, alignItems: "center" }}>
              <button
                className="tc-btn tc-btn-sm"
                type="button"
                onClick={() => fileRef.current?.click()}
              >
                <Upload {...SM} />
                Import palette…
              </button>
              <span style={{ fontSize: 12, color: "var(--tc-text-muted)" }}>
                From a list of colors
              </span>
              <input
                ref={fileRef}
                type="file"
                accept=".txt,.csv,.pal,.lut,text/plain"
                hidden
                onChange={(e) => void onFile(e)}
              />
            </div>
            {error ? (
              <div className="pop-row" style={{ color: "var(--tc-danger)", fontSize: 12 }}>
                {error}
              </div>
            ) : null}
          </Popover.Popup>
        </Popover.Positioner>
      </Popover.Portal>
    </Popover.Root>
  );
}

/** Live's Image tab: palette, color scale, highlight and the image options (live.html#live-view). */
export function ImageTab(props: ImageTabProps): React.JSX.Element {
  const { store, palettes, paletteId, rad, unit, pipeline, onPipelineChange, range } = props;
  const frame = useSampledFrame(store);
  const recent = useMemo(() => {
    const byId = new Map(palettes.map((p) => [p.id, p]));
    const list = props.recentPaletteIds.map((id) => byId.get(id)).filter((p): p is Palette => !!p);
    for (const p of palettes) {
      if (list.length >= 12) break;
      if (!list.includes(p)) list.push(p);
    }
    return list.slice(0, 12);
  }, [palettes, props.recentPaletteIds]);

  const [loRaw, hiRaw] = frame
    ? shownRange(range, frame)
    : range.mode === "manual"
      ? [range.lo, range.hi]
      : [NaN, NaN];
  const loC = rawToObjectC(loRaw, rad);
  const hiC = rawToObjectC(hiRaw, rad);

  const fix = (end: "lo" | "hi", c: number) => {
    const raw = Math.round(objectCToRaw(c, rad));
    const lo = end === "lo" ? raw : loRaw;
    const hi = end === "hi" ? raw : hiRaw;
    if (!Number.isFinite(lo) || !Number.isFinite(hi)) return;
    const a = Math.max(RAW_LIMITS.min, Math.min(lo, hi - RAW_LIMITS.minSpan));
    const b = Math.min(RAW_LIMITS.max, Math.max(hi, a + RAW_LIMITS.minSpan));
    props.onRangeChange({ mode: "manual", lo: a, hi: b });
  };

  const setScaleMode = (mode: "auto" | "fixed") => {
    if (mode === "auto") {
      if (range.mode !== "auto") props.onRangeChange({ mode: "auto" });
      return;
    }
    if (range.mode === "manual" || !Number.isFinite(loRaw) || !Number.isFinite(hiRaw)) return;
    props.onRangeChange(rangeAfterDragStart(range, [loRaw, hiRaw]));
  };

  const iso = pipeline.isotherm;
  const setIso = (patch: Partial<typeof iso>) =>
    onPipelineChange({ ...pipeline, isotherm: { ...iso, ...patch } });
  const set = (patch: Partial<DisplayPipeline>) => onPipelineChange({ ...pipeline, ...patch });
  const sharpen = Math.round(pipeline.sharpen_amount * 100);

  return (
    <div role="tabpanel" aria-label="Image">
      <Group
        icon={PaletteIcon}
        title="Palette"
        aside={
          <AllPalettes
            palettes={palettes}
            paletteId={paletteId}
            onSelectPalette={props.onSelectPalette}
            onImportPalette={props.onImportPalette}
          />
        }
      >
        <fieldset className="st-swatches" aria-label="Palette">
          {recent.map((p) => (
            <Swatch
              key={p.id}
              palette={p}
              pressed={p.id === paletteId}
              onClick={() => props.onSelectPalette(p.id)}
            />
          ))}
        </fieldset>
      </Group>

      <Group
        icon={Thermometer}
        title="Color scale"
        aside={
          <Segmented
            label="Color scale mode"
            small
            value={range.mode === "auto" ? "auto" : "fixed"}
            onChange={setScaleMode}
            options={[
              { id: "auto", label: "Auto", icon: Sparkles },
              { id: "fixed", label: "Fixed", icon: Lock },
            ]}
          />
        }
      >
        <div className="st-two">
          <label className="st-field">
            <span>From</span>
            <TempField label="From" valueC={loC} unit={unit} onCommit={(c) => fix("lo", c)} />
          </label>
          <label className="st-field">
            <span>To</span>
            <TempField label="To" valueC={hiC} unit={unit} onCommit={(c) => fix("hi", c)} />
          </label>
        </div>
        <p className="st-hint">
          {range.mode === "auto"
            ? "Follows the coldest and hottest point."
            : "Stays put. Type a value or drag the scale."}
        </p>
      </Group>

      <Group icon={Highlighter} title="Highlight">
        <label className="st-field">
          <span className="st-vh">Highlight</span>
          <select
            className="st-select"
            aria-label="Highlight"
            value={iso.mode}
            onChange={(e) => setIso({ mode: e.target.value as IsothermMode })}
          >
            {HIGHLIGHT.map((h) => (
              <option key={h.id} value={h.id}>
                {h.label}
              </option>
            ))}
          </select>
        </label>
        {iso.mode === "above" ? (
          <label className="st-field">
            <span>Above</span>
            <TempField
              label="Highlight above"
              valueC={iso.high_c}
              unit={unit}
              onCommit={(c) => setIso({ high_c: c })}
            />
          </label>
        ) : iso.mode === "below" ? (
          <label className="st-field">
            <span>Below</span>
            <TempField
              label="Highlight below"
              valueC={iso.low_c}
              unit={unit}
              onCommit={(c) => setIso({ low_c: c })}
            />
          </label>
        ) : iso.mode === "interval" ? (
          <div className="st-two">
            <label className="st-field">
              <span>From</span>
              <TempField
                label="Highlight from"
                valueC={iso.low_c}
                unit={unit}
                onCommit={(c) => setIso({ low_c: Math.min(c, iso.high_c) })}
              />
            </label>
            <label className="st-field">
              <span>To</span>
              <TempField
                label="Highlight to"
                valueC={iso.high_c}
                unit={unit}
                onCommit={(c) => setIso({ high_c: Math.max(c, iso.low_c) })}
              />
            </label>
          </div>
        ) : null}
        <p className="st-hint">Paints one band in a bright color.</p>
      </Group>

      <Group icon={ImageIcon} title="Image options" aside="Display only" collapsible defaultOpen>
        <div className="st-rows-set">
          <SettingRow
            icon={Scaling}
            title="Upscaler"
            desc="How pixels grow"
            control={
              <select
                className="st-select"
                aria-label="Upscaler"
                value={pipeline.upscaler}
                onChange={(e) => set({ upscaler: e.target.value as UpscaleMethod })}
              >
                {UPSCALERS.map((u) => (
                  <option key={u.id} value={u.id}>
                    {u.label}
                  </option>
                ))}
              </select>
            }
          />
          <SettingRow
            icon={Focus}
            title="Sharpening"
            desc="Unsharp mask amount"
            value={`${sharpen} %`}
            stacked
            control={
              <input
                type="range"
                min={0}
                max={150}
                value={sharpen}
                aria-label="Sharpening"
                onChange={(e) => set({ sharpen_amount: Number(e.target.value) / 100 })}
              />
            }
          />
          <SettingRow
            icon={Waves}
            title="Denoise"
            desc="Smooths speckle"
            control={
              <select
                className="st-select"
                aria-label="Denoise"
                value={pipeline.filter}
                onChange={(e) => set({ filter: e.target.value as SpatialFilter })}
              >
                {DENOISE.map((n) => (
                  <option key={n.id} value={n.id}>
                    {n.label}
                  </option>
                ))}
              </select>
            }
          />
          <SettingRow
            icon={ChartColumn}
            title="Boost contrast"
            desc="Histogram equalization"
            control={
              <Switch
                label="Boost contrast"
                checked={pipeline.histogram_eq}
                onChange={(on) => set({ histogram_eq: on })}
              />
            }
          />
          <SettingRow
            icon={RotateCw}
            title="Rotate 180°"
            desc="This view and its exports"
            control={
              <Switch
                label="Rotate 180°"
                checked={pipeline.rotate_180}
                onChange={(on) => set({ rotate_180: on })}
              />
            }
          />
          <SettingRow
            icon={FlipHorizontal2}
            title="Mirror"
            desc="Swaps left and right"
            control={
              <Switch
                label="Mirror"
                checked={pipeline.flip_horizontal}
                onChange={(on) => set({ flip_horizontal: on })}
              />
            }
          />
        </div>
      </Group>
    </div>
  );
}
