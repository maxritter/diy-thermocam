import { AlertTriangle, ChevronLeft, ChevronRight, X } from "lucide-react";
import { type ReactNode, useEffect, useMemo, useState } from "react";
import type { CaptureItem, SidecarLoad } from "@/lib/api/captures";
import { createFrameStore } from "@/lib/frameStore";
import type { Palette, TempUnit, ThermalFrame } from "@/lib/types";
import { CaptureView, type SideTab } from "./CaptureView";
import { CompareControl, CompareSlot } from "./CompareView";
import { ExportMenu } from "./ExportMenu";
import { editsFromSidecar, originalEdits, useCaptureEdits } from "./edits";
import type { CapturesApi, LibraryEntry } from "./Library";
import { NotesTab } from "./NotesTab";
import { captureLabel, systemLocale } from "./names";

const ICON = { size: 16, strokeWidth: 1.75, "aria-hidden": true } as const;
const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

export interface OpenedCaptureProps {
  api: CapturesApi;
  item: CaptureItem;
  /** The library's list in the order shown, for Previous and Next. */
  entries: LibraryEntry[];
  /** Captures that Compare can put beside this one. */
  candidates: LibraryEntry[];
  onBack: () => void;
  onOpen: (item: CaptureItem) => void;
  palettes: Palette[];
  /** Captures without edits open in this palette (the last one used). */
  paletteId: string;
  recentPaletteIds: string[];
  onImportPalette: (name: string, text: string) => Promise<void>;
  unit: TempUnit;
  onUnitChange?: (unit: TempUnit) => void;
  engineeringDetails: boolean;
  notify: (msg: string) => void;
  locale?: string;
  saveDelayMs?: number;
  initialTab?: SideTab;
}

/** "On the camera" / "On this computer". */
export function whereText(item: CaptureItem): string {
  return item.where === "camera" ? "On the camera" : "On this computer";
}

/** Previous and Next step through the library's captures without going back to it. */
export function StepButtons({
  item,
  entries,
  onOpen,
}: {
  item: CaptureItem;
  entries: LibraryEntry[];
  onOpen: (item: CaptureItem) => void;
}): React.JSX.Element {
  const list = entries.filter((e) => !e.other);
  const i = list.findIndex((e) => e.item.id === item.id);
  const prev = i > 0 ? list[i - 1].item : null;
  const next = i >= 0 && i < list.length - 1 ? list[i + 1].item : null;
  return (
    <>
      <button
        className="st-icon-btn"
        type="button"
        aria-label="Previous capture"
        title="Previous capture · ←"
        disabled={!prev}
        onClick={() => prev && onOpen(prev)}
      >
        <ChevronLeft {...ICON} />
      </button>
      <button
        className="st-icon-btn"
        type="button"
        aria-label="Next capture"
        title="Next capture · →"
        disabled={!next}
        onClick={() => next && onOpen(next)}
      >
        <ChevronRight {...ICON} />
      </button>
    </>
  );
}

export function stepTargets(item: CaptureItem, entries: LibraryEntry[]) {
  const list = entries.filter((e) => !e.other);
  const i = list.findIndex((e) => e.item.id === item.id);
  return {
    prev: i > 0 ? list[i - 1].item : null,
    next: i >= 0 && i < list.length - 1 ? list[i + 1].item : null,
  };
}

export function BarTitle({
  onBack,
  title,
  muted,
}: {
  onBack: () => void;
  title: string;
  muted: string;
}): React.JSX.Element {
  return (
    <>
      <button className="tc-btn tc-btn-ghost cp-back" type="button" onClick={onBack}>
        <ChevronLeft {...ICON} />
        Captures
      </button>
      <span className="st-vsep" aria-hidden="true" />
      <span className="cp-vtitle">
        <span className="t">{title}</span>
        {muted ? <span className="muted">{muted}</span> : null}
      </span>
    </>
  );
}

/** A banner on the stage naming the edits file that was set aside. */
export function QuarantineBanner({
  file,
  onDismiss,
}: {
  file: string;
  onDismiss: () => void;
}): React.JSX.Element {
  return (
    <div className="cp-stage-banner" role="alert">
      <AlertTriangle {...SM} />
      <span>
        The edits file was damaged and is set aside as {file}. The capture opens as it was taken.
      </span>
      <button type="button" aria-label="Dismiss" onClick={onDismiss}>
        <X {...SM} />
      </button>
    </div>
  );
}

/** The viewer's message when a file can't be opened, with the way back. */
export function CantOpen({
  detail,
  onBack,
}: {
  detail: string;
  onBack: () => void;
}): React.JSX.Element {
  return (
    <div className="cp-empty">
      <div className="ring danger">
        <AlertTriangle size={28} strokeWidth={1.75} aria-hidden="true" />
      </div>
      <h2>This file can’t be opened</h2>
      <p>It’s damaged, or it isn’t a Thermocam .DAT file.</p>
      {detail ? <p className="cp-detail">{detail}</p> : null}
      <div className="acts">
        <button className="tc-btn" type="button" onClick={onBack}>
          <ChevronLeft {...ICON} />
          Back to Captures
        </button>
      </div>
    </div>
  );
}

type Loaded =
  | { status: "loading" }
  | { status: "error"; detail: string }
  | { status: "ready"; frame: ThermalFrame; side: SidecarLoad };

/**
 * An opened snapshot (captures.html#opened): the Live viewer with the capture in it. Edits save
 * by themselves beside the capture (or in Studio's folder for the camera's), never in it.
 */
export function OpenedCapture(props: OpenedCaptureProps): React.JSX.Element {
  const { api, item } = props;
  const [loaded, setLoaded] = useState<Loaded>({ status: "loading" });

  useEffect(() => {
    let active = true;
    setLoaded({ status: "loading" });
    void Promise.all([
      api.loadFrame(item.ref),
      api
        .sidecarLoad(item.ref, true)
        .catch((): SidecarLoad => ({ sidecar: null, quarantined: null })),
    ])
      .then(([frame, side]) => active && setLoaded({ status: "ready", frame, side }))
      .catch((err) => {
        if (active) {
          setLoaded({
            status: "error",
            detail: String((err as { detail?: unknown })?.detail ?? err),
          });
        }
      });
    return () => {
      active = false;
    };
  }, [api, item.ref]);

  if (loaded.status !== "ready") {
    return <OpenedShell {...props} loaded={loaded} />;
  }
  return <OpenedSnapshot key={item.id} {...props} frame={loaded.frame} side={loaded.side} />;
}

/** While a capture loads, or when it can't be opened. */
function OpenedShell(
  props: OpenedCaptureProps & { loaded: Exclude<Loaded, { status: "ready" }> },
): React.JSX.Element {
  const store = useMemo(() => createFrameStore(), []);
  const locale = props.locale ?? systemLocale();
  const edits = originalEdits(props.paletteId);
  let overlay: ReactNode = null;
  if (props.loaded.status === "error") {
    overlay = <CantOpen detail={props.loaded.detail} onBack={props.onBack} />;
  } else {
    overlay = (
      <div className="cp-empty">
        <p>Opening…</p>
      </div>
    );
  }
  return (
    <CaptureView
      store={store}
      edits={edits}
      onEdit={() => {}}
      sensorMaterial={null}
      palettes={props.palettes}
      recentPaletteIds={props.recentPaletteIds}
      onImportPalette={props.onImportPalette}
      unit={props.unit}
      engineeringDetails={props.engineeringDetails}
      barLeft={
        <BarTitle
          onBack={props.onBack}
          title={captureLabel(props.item.fileName, locale)}
          muted=""
        />
      }
      barRight={() => (
        <StepButtons item={props.item} entries={props.entries} onOpen={props.onOpen} />
      )}
      noImage
      stageOverlay={overlay}
      notes={null}
      onUndo={() => {}}
      onRedo={() => {}}
      imageLabel="Opened capture"
    />
  );
}

function OpenedSnapshot(
  props: OpenedCaptureProps & { frame: ThermalFrame; side: SidecarLoad },
): React.JSX.Element {
  const { api, item, frame, side } = props;
  const locale = props.locale ?? systemLocale();
  const store = useMemo(() => {
    const s = createFrameStore();
    s.push(frame);
    return s;
  }, [frame]);
  const sensorMaterial = side.sidecar?.sensor_material ?? null;
  const original = useMemo(() => originalEdits(props.paletteId), [props.paletteId]);
  const ed = useCaptureEdits({
    api,
    captureRef: item.ref,
    initial: side.sidecar ? editsFromSidecar(side.sidecar) : original,
    hadSidecar: side.sidecar !== null,
    original,
    sensorMaterial,
    saveDelayMs: props.saveDelayMs,
  });
  const [quarantined, setQuarantined] = useState(side.quarantined);
  const [compareWith, setCompareWith] = useState<CaptureItem | null>(null);

  const dateLabel = captureLabel(item.fileName, locale);
  const typed = ed.edits.notes.name.trim();
  const { prev, next } = stepTargets(item, props.entries);
  const go = (to: CaptureItem | null) => {
    if (!to) return;
    ed.flush();
    props.onOpen(to);
  };

  const savePng = () => {
    const r = ed.edits.range;
    void api
      .savePng(item.ref, {
        palette_id: ed.edits.paletteId,
        range:
          r.mode === "manual"
            ? { mode: "manual", min_c: r.lo_c, max_c: r.hi_c }
            : { mode: "auto", min_c: null, max_c: null },
      })
      .then((path) => path && props.notify(`Saved ${path}`))
      .catch((err) => props.notify(`Couldn’t save the PNG: ${String(err?.detail ?? err)}`));
  };

  return (
    <CaptureView
      store={store}
      edits={ed.edits}
      onEdit={ed.update}
      sensorMaterial={sensorMaterial}
      palettes={props.palettes}
      recentPaletteIds={props.recentPaletteIds}
      onImportPalette={props.onImportPalette}
      unit={props.unit}
      onUnitChange={props.onUnitChange}
      engineeringDetails={props.engineeringDetails}
      initialTab={props.initialTab}
      barLeft={
        <BarTitle
          onBack={() => {
            ed.flush();
            props.onBack();
          }}
          title={typed || dateLabel}
          muted={typed ? dateLabel : ""}
        />
      }
      barRight={(panel) => (
        <>
          <StepButtons item={item} entries={props.entries} onOpen={(to) => go(to)} />
          <span className="st-vsep" aria-hidden="true" />
          <CompareControl
            candidates={props.candidates.filter((c) => c.item.id !== item.id && !c.other)}
            current={compareWith}
            onChange={setCompareWith}
          />
          <ExportMenu
            choices={[{ label: "PNG image…", sub: "As shown, with the color scale", run: savePng }]}
          />
          {panel.button}
        </>
      )}
      renderImage={
        compareWith
          ? (slot) => (
              <CompareSlot
                api={api}
                slot={slot}
                leftTitle={typed || dateLabel}
                right={compareWith}
                rightTitle={
                  props.candidates.find((c) => c.item.id === compareWith.id)?.title ??
                  captureLabel(compareWith.fileName, locale)
                }
                paletteId={props.paletteId}
                palettes={props.palettes}
                unit={props.unit}
              />
            )
          : undefined
      }
      stageOverlay={
        quarantined ? (
          <QuarantineBanner
            file={quarantined}
            onDismiss={() => {
              api.forgetSetAside(item.ref);
              setQuarantined(null);
            }}
          />
        ) : null
      }
      notes={
        <NotesTab
          name={ed.edits.notes.name}
          text={ed.edits.notes.text}
          placeholder={dateLabel}
          onName={(name) => ed.update((e) => ({ ...e, notes: { ...e.notes, name } }), "notes.name")}
          onText={(text) => ed.update((e) => ({ ...e, notes: { ...e.notes, text } }), "notes.text")}
          save={ed.save}
          canUndo={ed.canUndo}
          canRedo={ed.canRedo}
          onUndo={ed.undo}
          onRedo={ed.redo}
          onReset={() => void ed.reset()}
          details={[
            { k: "Taken", v: dateLabel === item.fileName ? "Not in the file name" : dateLabel },
            { k: "Where", v: whereText(item) },
            { k: "Resolution", v: `${frame.width} × ${frame.height}` },
            { k: "File", v: item.fileName, mono: true },
          ]}
        />
      }
      onPrevious={() => go(prev)}
      onNext={() => go(next)}
      onUndo={ed.undo}
      onRedo={ed.redo}
      imageLabel={`Opened capture: ${typed || dateLabel}`}
    />
  );
}
