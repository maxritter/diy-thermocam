import { Popover } from "@base-ui/react/popover";
import { ChevronDown, Columns2, X } from "lucide-react";
import { type ReactNode, useEffect, useMemo, useRef, useState } from "react";
import { ThermalViewport } from "@/components/ThermalViewport";
import type { CaptureItem } from "@/lib/api/captures";
import { createFrameStore } from "@/lib/frameStore";
import type { Palette, TempUnit } from "@/lib/types";
import { DEFAULT_PIPELINE, type ImageSlot } from "./CaptureView";
import { editsFromSidecar, originalEdits, radFor, rawRange } from "./edits";
import type { CapturesApi, LibraryEntry } from "./Library";

const ICON = { size: 16, strokeWidth: 1.75, "aria-hidden": true } as const;
const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

/** Where a pointer at `clientX` sits across the split, from 0 (left edge) to 1. */
export function splitFromPointer(clientX: number, rect: { left: number; width: number }): number {
  if (rect.width <= 0) return 0.5;
  return Math.max(0, Math.min(1, (clientX - rect.left) / rect.width));
}

/** The arrow keys move the divider by 5 %, Home and End to the edges. */
export function stepSplit(ratio: number, key: string): number {
  const r = (v: number) => Math.round(Math.max(0, Math.min(1, v)) * 1000) / 1000;
  switch (key) {
    case "ArrowLeft":
    case "ArrowDown":
      return r(ratio - 0.05);
    case "ArrowRight":
    case "ArrowUp":
      return r(ratio + 0.05);
    case "Home":
      return 0;
    case "End":
      return 1;
    default:
      return ratio;
  }
}

export interface CompareSplitProps {
  ratio: number;
  onRatioChange: (ratio: number) => void;
  /** The opened capture, on the left. */
  left: ReactNode;
  /** The capture compared with it, shown right of the divider. */
  right: ReactNode;
  leftLabel: string;
  rightLabel: string;
}

/**
 * The split view (captures.html#opened): the opened capture on the left, the other one right
 * of a divider that drags, or moves by 5 % with the arrow keys.
 */
export function CompareSplit({
  ratio,
  onRatioChange,
  left,
  right,
  leftLabel,
  rightLabel,
}: CompareSplitProps): React.JSX.Element {
  const boxRef = useRef<HTMLDivElement | null>(null);
  const onChange = useRef(onRatioChange);
  onChange.current = onRatioChange;
  const pct = Math.round(ratio * 1000) / 10;

  const startDrag = (e: React.PointerEvent) => {
    if (e.button !== 0) return;
    e.preventDefault();
    const box = boxRef.current;
    if (!box) return;
    const move = (ev: PointerEvent) =>
      onChange.current(splitFromPointer(ev.clientX, box.getBoundingClientRect()));
    const up = (ev: PointerEvent) => {
      move(ev);
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  };

  return (
    <div ref={boxRef} className="cp-compare">
      {left}
      <div className="cp-compare-right" style={{ clipPath: `inset(0 0 0 ${pct}%)` }}>
        {right}
      </div>
      <span className="cp-side-lbl" style={{ left: 10 }}>
        {leftLabel}
      </span>
      <span className="cp-side-lbl" style={{ right: 10 }}>
        {rightLabel}
      </span>
      <div className="cp-split" style={{ left: `${pct}%` }} onPointerDown={startDrag}>
        <button
          className="knob"
          type="button"
          role="slider"
          aria-label="Drag to move the divider"
          aria-valuemin={0}
          aria-valuemax={100}
          aria-valuenow={Math.round(pct)}
          aria-valuetext={`${Math.round(pct)} % of ${leftLabel}`}
          onKeyDown={(e) => {
            const next = stepSplit(ratio, e.key);
            if (next !== ratio) {
              e.preventDefault();
              e.stopPropagation();
              onRatioChange(next);
            }
          }}
        >
          <Columns2 {...SM} />
        </button>
      </div>
    </div>
  );
}

/**
 * The Compare button (captures.html#opened): its arrow picks any capture from the library,
 * × ends comparing.
 */
export function CompareControl({
  candidates,
  current,
  onChange,
}: {
  candidates: LibraryEntry[];
  current: CaptureItem | null;
  onChange: (item: CaptureItem | null) => void;
}): React.JSX.Element {
  const [open, setOpen] = useState(false);
  const currentTitle = candidates.find((c) => c.item.id === current?.id)?.title ?? "";
  const picker = (
    <Popover.Portal>
      <Popover.Positioner side="bottom" align="end" sideOffset={6} className="z-50">
        <Popover.Popup className="st-pop cp-pick" aria-label="Compare with">
          <div className="pop-h">Compare with</div>
          {candidates.length === 0 ? (
            <div className="pop-row">
              <span className="sub">No other captures in this list.</span>
            </div>
          ) : (
            candidates.map((c) => (
              <button
                key={c.item.id}
                className="item"
                type="button"
                aria-pressed={c.item.id === current?.id}
                onClick={() => {
                  onChange(c.item);
                  setOpen(false);
                }}
              >
                <span>
                  {c.title}
                  <span className="sub">{c.meta}</span>
                </span>
              </button>
            ))
          )}
        </Popover.Popup>
      </Popover.Positioner>
    </Popover.Portal>
  );

  if (!current) {
    return (
      <Popover.Root open={open} onOpenChange={setOpen}>
        <Popover.Trigger className="tc-btn" aria-haspopup="dialog">
          <Columns2 {...ICON} />
          Compare
        </Popover.Trigger>
        {picker}
      </Popover.Root>
    );
  }
  return (
    <div className="st-splitbtn">
      <Popover.Root open={open} onOpenChange={setOpen}>
        <Popover.Trigger className="tc-btn" aria-pressed="true" aria-haspopup="dialog">
          <Columns2 {...ICON} />
          Compare: {currentTitle}
          <ChevronDown {...SM} />
        </Popover.Trigger>
        {picker}
      </Popover.Root>
      <button
        className="tc-btn"
        type="button"
        aria-label="Stop comparing"
        onClick={() => onChange(null)}
      >
        <X {...SM} />
      </button>
    </div>
  );
}

/**
 * Compare in the viewer: the opened capture's image on the left, the other capture loaded with
 * its own palette and color scale (from its own edits) on the right.
 */
export function CompareSlot({
  api,
  slot,
  leftTitle,
  right,
  rightTitle,
  paletteId,
  palettes,
  unit,
}: {
  api: CapturesApi;
  slot: ImageSlot;
  leftTitle: string;
  right: CaptureItem;
  rightTitle: string;
  paletteId: string;
  palettes: Palette[];
  unit: TempUnit;
}): React.JSX.Element {
  const [ratio, setRatio] = useState(0.5);
  const store = useMemo(() => createFrameStore(), []);
  const [edits, setEdits] = useState(() => originalEdits(paletteId));
  const [sensor, setSensor] = useState<{ emissivity: number; reflected_c: number } | null>(null);
  const [failed, setFailed] = useState(false);
  const [, setTick] = useState(0);

  useEffect(() => {
    let active = true;
    setFailed(false);
    void Promise.all([
      api.loadFirstFrame(right),
      api.sidecarLoad(right.ref).catch(() => ({ sidecar: null, quarantined: null })),
    ])
      .then(([frame, side]) => {
        if (!active) return;
        store.push(frame);
        setEdits(side.sidecar ? editsFromSidecar(side.sidecar) : originalEdits(paletteId));
        setSensor(side.sidecar?.sensor_material ?? null);
        setTick((n) => n + 1);
      })
      .catch(() => active && setFailed(true));
    return () => {
      active = false;
    };
  }, [api, right, store, paletteId]);

  const rad = radFor(store.getFrame(), edits, sensor);
  const palette = palettes.find((p) => p.id === edits.paletteId) ?? slot.palette;
  return (
    <CompareSplit
      ratio={ratio}
      onRatioChange={setRatio}
      left={slot.viewport}
      right={
        failed ? (
          <div className="cp-empty" style={{ background: "var(--tc-canvas-bg)" }}>
            <p>This capture can’t be opened.</p>
          </div>
        ) : (
          <ThermalViewport
            store={store}
            palette={palette}
            range={rawRange(edits, rad)}
            rad={rad}
            pipeline={DEFAULT_PIPELINE}
            unit={unit}
            rois={[]}
            selectedRoiId={null}
            tool="select"
            showHotCold={false}
            showGrid={false}
            gridCols={4}
            gridRows={3}
            zoom={1}
            label={`Compared capture: ${rightTitle}`}
            style={{ position: "absolute", inset: 0, height: "100%" }}
            onRoisChange={() => {}}
            onSelectRoi={() => {}}
            onToolChange={() => {}}
          />
        )
      }
      leftLabel={leftTitle}
      rightLabel={rightTitle}
    />
  );
}
