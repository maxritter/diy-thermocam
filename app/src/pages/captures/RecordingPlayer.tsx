import { ChevronLeft, ChevronRight, Copy, Download, Pause, Play } from "lucide-react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type {
  CaptureItem,
  RecordingIndex,
  Series,
  SeriesRegion,
  SidecarLoad,
} from "@/lib/api/captures";
import { createFrameStore, type FrameStore } from "@/lib/frameStore";
import {
  bounds,
  createPlay,
  formatClock,
  mediaTimeOf,
  type PlayState,
  pause,
  play,
  prefetchWindow,
  SPEEDS,
  seek,
  setSpeed,
  setTrim,
  step,
  type Timeline,
  tick,
} from "@/lib/playback";
import { rowStat } from "@/lib/regions";
import { fromCelsius } from "@/lib/thermal";
import type { Material, ThermalFrame } from "@/lib/types";
import { chartLines, LineChart } from "@/pages/live/ChartDrawer";
import { CaptureView } from "./CaptureView";
import { ExportMenu } from "./ExportMenu";
import { type Edits, editsFromSidecar, originalEdits, radFor, useCaptureEdits } from "./edits";
import { NotesTab } from "./NotesTab";
import { captureLabel, systemLocale } from "./names";
import {
  BarTitle,
  CantOpen,
  type OpenedCaptureProps,
  QuarantineBanner,
  StepButtons,
  stepTargets,
  whereText,
} from "./OpenedCapture";

const ICON = { size: 16, strokeWidth: 1.75, "aria-hidden": true } as const;
const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

type Loaded =
  | { status: "copying"; done: number; total: number }
  | { status: "opening" }
  | { status: "error"; detail: string }
  | {
      status: "ready";
      path: string;
      rec: RecordingIndex;
      first: ThermalFrame;
      side: SidecarLoad;
    };

/**
 * An opened recording (captures.html#recording): the viewer with a player under the image and
 * the chart over the whole recording below it. A video from the camera's card is copied to this
 * computer first and plays by frame number only.
 */
export function RecordingPlayer(props: OpenedCaptureProps): React.JSX.Element {
  const { api, item } = props;
  const [loaded, setLoaded] = useState<Loaded>({ status: "opening" });

  useEffect(() => {
    let active = true;
    void (async () => {
      try {
        let path: string;
        if (item.ref.kind === "camera") {
          setLoaded({ status: "copying", done: 0, total: item.frames ?? 0 });
          path = await api.fetchCameraFolder(item.ref.sd_path, (done, total) => {
            if (active) setLoaded({ status: "copying", done, total });
          });
        } else {
          path = item.ref.path;
        }
        if (!active) return;
        setLoaded({ status: "opening" });
        const [rec, side] = await Promise.all([
          api.recordingOpen(path),
          api
            .sidecarLoad(item.ref, true)
            .catch((): SidecarLoad => ({ sidecar: null, quarantined: null })),
        ]);
        const first = await api.recordingFrame(path, 0);
        if (active) setLoaded({ status: "ready", path, rec, first, side });
      } catch (err) {
        if (active) {
          setLoaded({
            status: "error",
            detail: String((err as { detail?: unknown })?.detail ?? err),
          });
        }
      }
    })();
    return () => {
      active = false;
    };
  }, [api, item]);

  if (loaded.status === "ready") return <Player key={item.id} {...props} {...loaded} />;
  return <PlayerShell {...props} loaded={loaded} />;
}

function PlayerShell(
  props: OpenedCaptureProps & { loaded: Exclude<Loaded, { status: "ready" }> },
): React.JSX.Element {
  const store = useMemo(() => createFrameStore(), []);
  const locale = props.locale ?? systemLocale();
  const l = props.loaded;
  const overlay =
    l.status === "error" ? (
      <CantOpen detail={l.detail} onBack={props.onBack} />
    ) : l.status === "copying" ? (
      <div className="cp-empty" role="status">
        <h2>Copying the video to this computer</h2>
        <p>
          {l.total > 0 ? `${l.done} of ${l.total} frames` : "Reading the folder on the card"} · it
          plays from there, and next time it opens at once.
        </p>
        <span className="cp-bar" aria-hidden="true">
          <i style={{ width: `${l.total > 0 ? Math.round((l.done / l.total) * 100) : 0}%` }} />
        </span>
      </div>
    ) : (
      <div className="cp-empty" role="status">
        <p>Opening…</p>
      </div>
    );
  return (
    <CaptureView
      store={store}
      edits={originalEdits(props.paletteId)}
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
      imageLabel="Recording"
    />
  );
}

/** Each chart line as a region Rust evaluates in every frame: Hottest, the regions, Coldest. */
export function seriesRegions(edits: Edits, width: number, height: number): SeriesRegion[] {
  const image: Material = {
    emissivity: edits.material.emissivity,
    reflected_c: edits.material.reflected_c,
  };
  const whole = { kind: "box" as const, x: 0, y: 0, w: width, h: height };
  const out: SeriesRegion[] = [{ id: "hottest", geometry: whole, material: image, stat: "max" }];
  for (const r of edits.rois) {
    if (r.geometry.kind === "delta") continue;
    out.push({
      id: r.id,
      geometry: r.geometry,
      material: {
        emissivity: r.emissivity_override ?? image.emissivity,
        reflected_c: r.reflected_temp_override_c ?? image.reflected_c,
      },
      stat: rowStat(r.geometry.kind) === "max" ? "max" : "avg",
    });
  }
  out.push({ id: "coldest", geometry: whole, material: image, stat: "min" });
  return out;
}

/** Fetches frames 8 ahead of the play head, drops the ones behind, and shows the current one. */
function useFrames(
  api: OpenedCaptureProps["api"],
  path: string,
  frames: number,
  index: number,
  store: FrameStore,
) {
  const cache = useRef(new Map<number, ThermalFrame>());
  const inflight = useRef(new Set<number>());
  const indexRef = useRef(index);
  indexRef.current = index;
  useEffect(() => {
    const want = prefetchWindow(index, frames);
    for (const k of [...cache.current.keys()]) {
      if (k < index || k > index + 8) cache.current.delete(k);
    }
    const shown = cache.current.get(index);
    if (shown) store.push(shown);
    for (const i of want) {
      if (cache.current.has(i) || inflight.current.has(i)) continue;
      inflight.current.add(i);
      void api
        .recordingFrame(path, i)
        .then((fr) => {
          const now = indexRef.current;
          if (i >= now && i <= now + 8) cache.current.set(i, fr);
          if (i === now) store.push(fr);
        })
        .catch(() => {})
        .finally(() => inflight.current.delete(i));
    }
  }, [api, path, frames, index, store]);
}

function Player(
  props: OpenedCaptureProps & Extract<Loaded, { status: "ready" }>,
): React.JSX.Element {
  const { api, item, path, rec, first, side } = props;
  const locale = props.locale ?? systemLocale();
  const store = useMemo(() => {
    const s = createFrameStore();
    s.push(first);
    return s;
  }, [first]);
  const sensorMaterial = side.sidecar?.sensor_material ?? rec.sensor_material ?? null;
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
  // The chart opens with the recording unless the window is too short for it and the tools.
  const [chartOpen, setChartOpen] = useState(
    () => typeof window === "undefined" || window.innerHeight >= 820,
  );
  const tl: Timeline = useMemo(
    () => ({ frames: rec.frames, tMs: rec.t_ms, fps: rec.fps_nominal || 8.7 }),
    [rec],
  );
  const timed = rec.t_ms !== null;
  const [ps, setPs] = useState<PlayState>(() => createPlay(tl));
  const psRef = useRef(ps);
  psRef.current = ps;
  useFrames(api, path, rec.frames, ps.index, store);

  // While playing, the play head follows the clock once per screen refresh.
  useEffect(() => {
    if (!ps.playing) return;
    let id = 0;
    const loop = () => {
      setPs((s) => tick(s, tl, performance.now()));
      id = requestAnimationFrame(loop);
    };
    id = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(id);
  }, [ps.playing, tl]);

  const act = useCallback(
    (fn: (s: PlayState, now: number) => PlayState) => setPs((s) => fn(s, performance.now())),
    [],
  );
  const togglePlay = () => act((s, now) => (s.playing ? pause(s, tl, now) : play(s, tl, now)));

  // The chart: every line over the whole recording, from Rust.
  // Only regions and material change the chart; a palette or a typed name does not.
  const { rois, material } = ed.edits;
  const regions = useMemo(
    () => seriesRegions({ ...originalEdits(""), rois, material }, first.width, first.height),
    [rois, material, first.width, first.height],
  );
  const [series, setSeries] = useState<Series | null>(null);
  const [seriesProgress, setSeriesProgress] = useState<[number, number] | null>(null);
  useEffect(() => {
    if (!chartOpen) return;
    let active = true;
    const t = setTimeout(() => {
      setSeriesProgress([0, rec.frames]);
      void api
        .recordingSeries(path, regions, (done, total) => active && setSeriesProgress([done, total]))
        .then((s) => {
          if (!active) return;
          setSeries(s);
          setSeriesProgress(null);
        })
        .catch(() => active && setSeriesProgress(null));
    }, 250);
    return () => {
      active = false;
      clearTimeout(t);
    };
  }, [api, path, regions, chartOpen, rec.frames]);

  const dateLabel = captureLabel(item.fileName, locale);
  const typed = ed.edits.notes.name.trim();
  const lengthMs = mediaTimeOf(tl, rec.frames - 1);
  const where = item.where === "camera" ? "on the camera" : "on this computer";
  const muted = timed
    ? `Recording · ${Math.round(lengthMs / 1000)} s · ${where}`
    : `Video · ${rec.frames} frames · ${where}`;
  const { prev, next } = stepTargets(item, props.entries);
  const go = (to: CaptureItem | null) => {
    if (!to) return;
    ed.flush();
    props.onOpen(to);
  };
  const clock = (i: number) => formatClock(mediaTimeOf(tl, i));
  const [lo, hi] = bounds(tl, ps.trim);

  const exportAs = (format: "mp4" | "gif" | "dat" | "series-csv") => {
    const fr = store.getFrame() ?? first;
    const rad = radFor(fr, ed.edits, sensorMaterial);
    void api
      .exportRecording({
        format,
        recording: path,
        range: ps.trim,
        regions,
        paletteId: ed.edits.paletteId,
        minRaw: fr.min_raw,
        maxRaw: fr.max_raw,
        rad,
        unit: props.unit,
        title: typed || dateLabel,
      })
      .then((saved) => saved && props.notify(`Saved ${saved}`))
      .catch((err) => {
        const e = err as { kind?: string; detail?: unknown };
        if (e?.kind !== "cancelled" && !/cancel/i.test(String(e?.detail ?? err))) {
          props.notify(`Couldn’t export: ${String(e?.detail ?? err)}`);
        }
      });
  };
  const part = ps.trim ? "the trimmed frames" : "every frame";

  const lines = chartLines(ed.edits.rois);
  const xOf = (i: number) => (timed ? mediaTimeOf(tl, i) : i);
  const plots = series
    ? lines.map((l) => {
        const k = regions.findIndex((r) => r.id === l.key);
        const vals = k >= 0 ? (series.values[k] ?? []) : [];
        return { key: l.key, color: l.color, points: vals.map((v, i) => ({ t: xOf(i), v })) };
      })
    : [];
  const xMax = Math.max(1, xOf(rec.frames - 1));
  const xLabels = [0, 0.25, 0.5, 0.75, 1].map((f) =>
    timed ? formatClock(f * xMax) : String(Math.round(f * (rec.frames - 1)) + 1),
  );

  const copyChart = () => {
    if (!series) return;
    const head = ["frame", "t_ms", ...lines.map((l) => l.label)];
    const rows = [head.join(",")];
    for (let i = lo; i <= hi; i++) {
      const vals = lines.map((l) => {
        const k = regions.findIndex((r) => r.id === l.key);
        const v = series.values[k]?.[i];
        return v === undefined ? "" : fromCelsius(v, props.unit).toFixed(2);
      });
      rows.push([i + 1, timed ? mediaTimeOf(tl, i) : "", ...vals].join(","));
    }
    void navigator.clipboard
      ?.writeText(rows.join("\n"))
      .then(() => props.notify("Readings copied"))
      .catch(() => props.notify("Couldn’t copy"));
  };

  const player = (
    <PlayerBar
      ps={ps}
      tl={tl}
      timed={timed}
      clock={clock}
      lengthMs={lengthMs}
      onToggle={togglePlay}
      onStep={(d) => act((s, now) => step(s, tl, d, now))}
      onSeek={(i) => act((s, now) => seek(s, tl, i, now))}
      onSpeed={(sp) => act((s, now) => setSpeed(s, tl, sp, now))}
      onTrim={(t) => act((s, now) => setTrim(s, tl, t, now))}
    />
  );

  const drawer = chartOpen ? (
    <div className="st-drawer" style={{ gridTemplateColumns: "minmax(0, 1fr)", height: 190 }}>
      <section className="st-card" aria-label="Readings over the whole recording">
        <div className="st-card-h">
          <h3>Over time</h3>
          <div className="st-row-actions">
            <button
              className="tc-btn tc-btn-sm tc-btn-ghost"
              type="button"
              disabled={!series}
              onClick={copyChart}
            >
              <Copy {...SM} />
              Copy
            </button>
            <button
              className="tc-btn tc-btn-sm tc-btn-ghost"
              type="button"
              onClick={() => exportAs("series-csv")}
            >
              <Download {...SM} />
              Save CSV…
            </button>
          </div>
        </div>
        <div className="st-legend">
          {lines.map((l) => (
            <span key={l.key}>
              <i style={{ background: l.color }} />
              {l.label}
            </span>
          ))}
          <span>
            {seriesProgress
              ? `Reading every frame · ${seriesProgress[0]} of ${seriesProgress[1]}`
              : timed
                ? `Line: frame ${ps.index + 1} · ${clock(ps.index)}`
                : `Line: frame ${ps.index + 1}`}
          </span>
        </div>
        <LineChart
          plots={plots}
          from={0}
          to={xMax}
          xLabels={xLabels}
          unit={props.unit}
          cursor={xOf(ps.index)}
          label={
            timed
              ? `Each line over the whole ${Math.round(lengthMs / 1000)}-second recording, with a line at the current frame, ${clock(ps.index)} in`
              : `Each line over all ${rec.frames} frames, with a line at frame ${ps.index + 1}`
          }
        />
      </section>
    </div>
  ) : null;

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
      initialTab={props.initialTab ?? "measure"}
      short
      barLeft={
        <BarTitle
          onBack={() => {
            ed.flush();
            props.onBack();
          }}
          title={typed || dateLabel}
          muted={typed ? `${dateLabel} · ${muted}` : muted}
        />
      }
      barRight={(panel) => (
        <>
          <StepButtons item={item} entries={props.entries} onOpen={(to) => go(to)} />
          <span className="st-vsep" aria-hidden="true" />
          <ExportMenu
            choices={[
              { label: "MP4 video…", sub: `Of ${part}`, run: () => exportAs("mp4") },
              { label: "GIF…", sub: `Of ${part}`, run: () => exportAs("gif") },
              {
                label: "CSV of the readings…",
                sub: `One row per frame, each line’s value`,
                run: () => exportAs("series-csv"),
              },
              {
                label: "Frames as .DAT…",
                sub: `With every temperature`,
                run: () => exportAs("dat"),
              },
            ]}
          />
          {panel.button}
        </>
      )}
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
      below={
        <>
          {player}
          {drawer}
        </>
      }
      measure={{
        chartsLabel: "Chart below the image",
        chartsOpen: chartOpen,
        onChartsOpenChange: setChartOpen,
        wholeAside: `frame ${ps.index + 1}`,
        regionsHint:
          "None yet. Pick a tool on the left of the image, then click or drag on it. A region reads every frame and gets its own line in the chart.",
      }}
      notes={
        <NotesTab
          name={ed.edits.notes.name}
          text={ed.edits.notes.text}
          placeholder={dateLabel}
          rows={4}
          onName={(name) => ed.update((e) => ({ ...e, notes: { ...e.notes, name } }), "notes.name")}
          onText={(text) => ed.update((e) => ({ ...e, notes: { ...e.notes, text } }), "notes.text")}
          save={ed.save}
          canUndo={ed.canUndo}
          canRedo={ed.canRedo}
          onUndo={ed.undo}
          onRedo={ed.redo}
          onReset={() => void ed.reset()}
          details={[
            {
              k: "Recorded",
              v: dateLabel === item.fileName ? "Not in the folder name" : dateLabel,
            },
            { k: "Where", v: whereText(item) },
            {
              k: "Length",
              v: timed
                ? `${(lengthMs / 1000).toFixed(1)} s · ${rec.frames} frames`
                : `${rec.frames} frames`,
            },
            { k: "Folder", v: item.fileName, mono: true },
          ]}
        />
      }
      onKey={(e, id) => {
        if (e.metaKey || e.ctrlKey || e.altKey) return false;
        if (e.key === ",") {
          act((s, now) => step(s, tl, -1, now));
          return true;
        }
        if (e.key === ".") {
          act((s, now) => step(s, tl, 1, now));
          return true;
        }
        if (id === "snapshot") {
          togglePlay();
          return true;
        }
        if (ps.trim && (e.key === "[" || e.key === "]")) {
          const cur = psRef.current;
          const t = cur.trim ?? { from: 0, to: rec.frames - 1 };
          act((s, now) =>
            setTrim(
              s,
              tl,
              e.key === "[" ? { from: cur.index, to: t.to } : { from: t.from, to: cur.index },
              now,
            ),
          );
          return true;
        }
        return false;
      }}
      onPrevious={() => go(prev)}
      onNext={() => go(next)}
      onUndo={ed.undo}
      onRedo={ed.redo}
      imageLabel={`Frame ${ps.index + 1} of the recording ${typed || dateLabel}`}
    />
  );
}

/** The player under the image: frame steps, play, time, the scrub bar with the trim, speed. */
export function PlayerBar({
  ps,
  tl,
  timed,
  clock,
  lengthMs,
  onToggle,
  onStep,
  onSeek,
  onSpeed,
  onTrim,
}: {
  ps: PlayState;
  tl: Timeline;
  timed: boolean;
  clock: (i: number) => string;
  lengthMs: number;
  onToggle: () => void;
  onStep: (delta: number) => void;
  onSeek: (index: number) => void;
  onSpeed: (speed: number) => void;
  onTrim: (trim: { from: number; to: number } | null) => void;
}): React.JSX.Element {
  const scrubRef = useRef<HTMLDivElement | null>(null);
  const last = Math.max(1, tl.frames - 1);
  const pos = (i: number) => `${(i / last) * 100}%`;
  const indexAt = (clientX: number) => {
    const r = scrubRef.current?.getBoundingClientRect();
    if (!r || r.width <= 0) return ps.index;
    return Math.round(Math.max(0, Math.min(1, (clientX - r.left) / r.width)) * last);
  };

  const drag = (e: React.PointerEvent, onMove: (i: number) => void) => {
    if (e.button !== 0) return;
    e.preventDefault();
    e.stopPropagation();
    onMove(indexAt(e.clientX));
    const move = (ev: PointerEvent) => onMove(indexAt(ev.clientX));
    const up = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  };

  const trim = ps.trim;
  return (
    <fieldset className="cp-player" aria-label="Recording player">
      <button
        className="st-icon-btn"
        type="button"
        aria-label="Previous frame"
        title="Previous frame · ,"
        onClick={() => onStep(-1)}
      >
        <ChevronLeft {...ICON} />
      </button>
      <button
        className="tc-btn tc-btn-primary cp-play"
        type="button"
        aria-label={ps.playing ? "Pause" : "Play"}
        title={ps.playing ? "Pause · Space" : "Play · Space"}
        onClick={onToggle}
      >
        {ps.playing ? <Pause {...ICON} /> : <Play {...ICON} />}
      </button>
      <button
        className="st-icon-btn"
        type="button"
        aria-label="Next frame"
        title="Next frame · ."
        onClick={() => onStep(1)}
      >
        <ChevronRight {...ICON} />
      </button>
      {timed ? (
        <span className="mono" style={{ minWidth: 118 }}>
          {clock(ps.index)} / {formatClock(lengthMs)}
        </span>
      ) : null}
      <div
        ref={scrubRef}
        className="cp-scrub"
        role="slider"
        aria-label="Position"
        aria-valuemin={1}
        aria-valuemax={tl.frames}
        aria-valuenow={ps.index + 1}
        aria-valuetext={
          timed
            ? `${clock(ps.index)}, frame ${ps.index + 1} of ${tl.frames}`
            : `frame ${ps.index + 1} of ${tl.frames}`
        }
        tabIndex={0}
        onPointerDown={(e) => drag(e, onSeek)}
        onKeyDown={(e) => {
          const to =
            e.key === "ArrowLeft" || e.key === "ArrowDown"
              ? ps.index - 1
              : e.key === "ArrowRight" || e.key === "ArrowUp"
                ? ps.index + 1
                : e.key === "PageDown"
                  ? ps.index - 10
                  : e.key === "PageUp"
                    ? ps.index + 10
                    : e.key === "Home"
                      ? 0
                      : e.key === "End"
                        ? last
                        : null;
          if (to === null) return;
          e.preventDefault();
          e.stopPropagation();
          onSeek(to);
        }}
      >
        <span className="fill" style={{ width: pos(ps.index) }} />
        {trim ? (
          <>
            <span
              className="sel"
              title={
                timed
                  ? `Selected for export: ${clock(trim.from)} – ${clock(trim.to)}`
                  : `Selected for export: frames ${trim.from + 1} – ${trim.to + 1}`
              }
              style={{ left: pos(trim.from), width: `${((trim.to - trim.from) / last) * 100}%` }}
            />
            <span
              className="edge"
              style={{ left: pos(trim.from) }}
              title="Drag to set the first frame to export · ["
              onPointerDown={(e) =>
                drag(e, (i) => onTrim({ from: Math.min(i, trim.to), to: trim.to }))
              }
            />
            <span
              className="edge"
              style={{ left: pos(trim.to) }}
              title="Drag to set the last frame to export · ]"
              onPointerDown={(e) =>
                drag(e, (i) => onTrim({ from: trim.from, to: Math.max(i, trim.from) }))
              }
            />
          </>
        ) : null}
        <span className="knob2" style={{ left: pos(ps.index) }} />
      </div>
      <span className="mono muted">
        frame {ps.index + 1} / {tl.frames}
      </span>
      {timed ? (
        <select
          className="st-select cp-speed"
          aria-label="Speed"
          value={String(ps.speed)}
          onChange={(e) => onSpeed(Number(e.target.value))}
        >
          {SPEEDS.map((s) => (
            <option key={s} value={String(s)}>
              {s}×
            </option>
          ))}
        </select>
      ) : null}
      <button
        className="tc-btn tc-btn-sm"
        type="button"
        aria-pressed={trim !== null}
        title={
          timed
            ? "Set the start and end of the part to export"
            : "Set the first and last frame to export"
        }
        onClick={() => onTrim(trim ? null : { from: 0, to: tl.frames - 1 })}
      >
        Trim
      </button>
    </fieldset>
  );
}
