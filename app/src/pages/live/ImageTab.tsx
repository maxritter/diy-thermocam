import { Popover } from "@base-ui/react/popover";
import { ChevronDown, FlipHorizontal2, RotateCw, Upload } from "lucide-react";
import { type ChangeEvent, useMemo, useRef, useState } from "react";
import { type ColorRange, RAW_LIMITS, shownRange } from "@/lib/colorRange";
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

const SMOOTHING: { id: UpscaleMethod; label: string }[] = [
  { id: "edge_preserve", label: "Smooth" },
  { id: "lanczos3", label: "Sharp" },
  { id: "nearest", label: "Pixels" },
];

const NOISE: { id: SpatialFilter; label: string }[] = [
  { id: "none", label: "Off" },
  { id: "gaussian3x3", label: "Light" },
  { id: "median3x3", label: "Strong" },
];

const HIGHLIGHT: { id: IsothermMode; label: string }[] = [
  { id: "off", label: "Off" },
  { id: "above", label: "Above a temperature" },
  { id: "below", label: "Below a temperature" },
  { id: "interval", label: "Between two temperatures" },
];

function smoothingOf(p: DisplayPipeline): UpscaleMethod {
  return p.upscaler === "bicubic" ? "edge_preserve" : p.upscaler;
}

function noiseOf(p: DisplayPipeline): SpatialFilter {
  return p.filter === "box3x3" ? "gaussian3x3" : p.filter;
}

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
      {palette.label}
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

  const iso = pipeline.isotherm;
  const setIso = (patch: Partial<typeof iso>) =>
    onPipelineChange({ ...pipeline, isotherm: { ...iso, ...patch } });
  const smoothing = smoothingOf(pipeline);

  return (
    <div role="tabpanel" aria-label="Image">
      <div className="st-grp">
        <div className="st-grp-h">
          <h3>Palette</h3>
          <AllPalettes
            palettes={palettes}
            paletteId={paletteId}
            onSelectPalette={props.onSelectPalette}
            onImportPalette={props.onImportPalette}
          />
        </div>
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
      </div>

      <div className="st-grp">
        <div className="st-grp-h">
          <h3>Color scale</h3>
          <span className="aside">{range.mode === "auto" ? "Auto" : "Fixed"}</span>
        </div>
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
          Auto follows the coldest and hottest point. Typing a value or dragging a handle on the
          scale fixes it; <strong>Auto</strong> on the scale follows the image again.
        </p>
      </div>

      <div className="st-grp">
        <div className="st-grp-h">
          <h3>Highlight</h3>
        </div>
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
        <p className="st-hint">Paints one temperature band in a bright color.</p>
      </div>

      <details className="st-more">
        <summary>
          More image options
          <span className="aside">{SMOOTHING.find((s) => s.id === smoothing)?.label}</span>
          <ChevronDown {...SM} />
        </summary>
        <div className="st-grp">
          <p className="st-hint" style={{ margin: 0 }}>
            Changes how the image looks, never the temperatures.
          </p>
          <div className="st-field">
            <span>Smoothing</span>
            <fieldset className="st-seg full" aria-label="Smoothing">
              {SMOOTHING.map((s) => (
                <button
                  key={s.id}
                  type="button"
                  aria-pressed={smoothing === s.id}
                  onClick={() => onPipelineChange({ ...pipeline, upscaler: s.id })}
                >
                  {s.label}
                </button>
              ))}
            </fieldset>
          </div>
          <label className="st-field">
            <span style={{ display: "flex", justifyContent: "space-between" }}>
              Detail{" "}
              <span className="font-mono-tech">{Math.round(pipeline.sharpen_amount * 100)}%</span>
            </span>
            <input
              type="range"
              min={0}
              max={150}
              value={Math.round(pipeline.sharpen_amount * 100)}
              aria-label="Detail"
              onChange={(e) =>
                onPipelineChange({ ...pipeline, sharpen_amount: Number(e.target.value) / 100 })
              }
            />
          </label>
          <label className="st-field">
            <span>Noise reduction</span>
            <select
              className="st-select"
              aria-label="Noise reduction"
              value={noiseOf(pipeline)}
              onChange={(e) =>
                onPipelineChange({ ...pipeline, filter: e.target.value as SpatialFilter })
              }
            >
              {NOISE.map((n) => (
                <option key={n.id} value={n.id}>
                  {n.label}
                </option>
              ))}
            </select>
          </label>
          <div className="st-setrow">
            <span className="lbl">Boost contrast</span>
            <Switch
              label="Boost contrast"
              checked={pipeline.histogram_eq}
              onChange={(on) => onPipelineChange({ ...pipeline, histogram_eq: on })}
            />
          </div>
          <p className="st-hint" style={{ marginTop: -6 }}>
            Spreads colors by how often each temperature appears, so the scale is no longer even.
            The scale shows a mark while it is on.
          </p>
          <div className="st-two">
            <button
              className="tc-btn tc-btn-sm"
              type="button"
              aria-pressed={pipeline.rotate_180}
              onClick={() => onPipelineChange({ ...pipeline, rotate_180: !pipeline.rotate_180 })}
            >
              <RotateCw {...SM} />
              Rotate 180°
            </button>
            <button
              className="tc-btn tc-btn-sm"
              type="button"
              aria-pressed={pipeline.flip_horizontal}
              onClick={() =>
                onPipelineChange({ ...pipeline, flip_horizontal: !pipeline.flip_horizontal })
              }
            >
              <FlipHorizontal2 {...SM} />
              Mirror
            </button>
          </div>
          <p className="st-hint">
            Turns only this view and its exports. For a camera mounted upside down, use Camera ›
            Mounted upside down.
          </p>
        </div>
      </details>
    </div>
  );
}
