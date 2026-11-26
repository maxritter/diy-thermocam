import type { ThermalFrame, TimeSeriesPoint } from "./types";

/**
 * Per-frame state that lives outside React. The viewer subscribes to every frame; panels
 * subscribe to a sampled snapshot that updates at most four times a second, on the trailing
 * edge so the newest frame is never lost.
 */

export interface HoverPixel {
  x: number;
  y: number;
  raw: number;
  tempC: number;
}

export interface FrameCounters {
  /** Frames pushed into the store. */
  received: number;
  /** Frames the viewer reported as drawn. */
  drawn: number;
}

export interface FrameScheduler {
  schedule(fn: () => void, ms: number): unknown;
  cancel(handle: unknown): void;
}

export interface FrameStoreOptions {
  /** Minimum time between sampled notifications, in ms. Default 250 (4 Hz). */
  sampleIntervalMs?: number;
  /** Number of time-series points kept. Default 180. */
  seriesCap?: number;
  scheduler?: FrameScheduler;
}

type Listener = () => void;

export interface FrameStore {
  push(frame: ThermalFrame): void;
  getFrame(): ThermalFrame | null;
  getSampled(): ThermalFrame | null;
  subscribeFrame(listener: Listener): () => void;
  subscribeSampled(listener: Listener): () => void;
  setHover(hover: HoverPixel | null): void;
  getHover(): HoverPixel | null;
  subscribeHover(listener: Listener): () => void;
  appendSeries(point: TimeSeriesPoint): void;
  clearSeries(): void;
  /** Sampled copy of the series, replaced at most four times a second. */
  getSeries(): readonly TimeSeriesPoint[];
  markDrawn(): void;
  counters(): FrameCounters;
}

const browserScheduler: FrameScheduler = {
  schedule: (fn, ms) => setTimeout(fn, ms),
  cancel: (handle) => clearTimeout(handle as ReturnType<typeof setTimeout>),
};

const EMPTY_SERIES: readonly TimeSeriesPoint[] = Object.freeze([]);

function sameHover(a: HoverPixel | null, b: HoverPixel | null): boolean {
  if (a === b) return true;
  if (!a || !b) return false;
  return a.x === b.x && a.y === b.y && a.raw === b.raw && a.tempC === b.tempC;
}

export function createFrameStore(options: FrameStoreOptions = {}): FrameStore {
  const interval = options.sampleIntervalMs ?? 250;
  const seriesCap = options.seriesCap ?? 180;
  const scheduler = options.scheduler ?? browserScheduler;

  let frame: ThermalFrame | null = null;
  let sampled: ThermalFrame | null = null;
  let hover: HoverPixel | null = null;
  let series: TimeSeriesPoint[] = [];
  let seriesSnapshot: readonly TimeSeriesPoint[] = EMPTY_SERIES;
  let seriesDirty = false;
  let pending: unknown = null;
  const counts: FrameCounters = { received: 0, drawn: 0 };

  const frameListeners = new Set<Listener>();
  const sampledListeners = new Set<Listener>();
  const hoverListeners = new Set<Listener>();

  const emit = (set: Set<Listener>) => {
    for (const fn of set) fn();
  };

  const flushSampled = () => {
    pending = null;
    sampled = frame;
    if (seriesDirty) {
      seriesSnapshot = series.slice();
      seriesDirty = false;
    }
    emit(sampledListeners);
  };

  const scheduleSampled = () => {
    if (pending !== null) return;
    pending = scheduler.schedule(flushSampled, interval);
  };

  const subscribe = (set: Set<Listener>) => (listener: Listener) => {
    set.add(listener);
    return () => {
      set.delete(listener);
    };
  };

  return {
    push(next) {
      frame = next;
      counts.received++;
      emit(frameListeners);
      scheduleSampled();
    },
    getFrame: () => frame,
    getSampled: () => sampled,
    subscribeFrame: subscribe(frameListeners),
    subscribeSampled: subscribe(sampledListeners),
    setHover(next) {
      if (sameHover(hover, next)) return;
      hover = next;
      emit(hoverListeners);
    },
    getHover: () => hover,
    subscribeHover: subscribe(hoverListeners),
    appendSeries(point) {
      series.push(point);
      if (series.length > seriesCap) series = series.slice(series.length - seriesCap);
      seriesDirty = true;
      scheduleSampled();
    },
    clearSeries() {
      series = [];
      seriesDirty = true;
      scheduleSampled();
    },
    getSeries: () => seriesSnapshot,
    markDrawn() {
      counts.drawn++;
    },
    counters: () => ({ ...counts }),
  };
}

/** The app's one store for the live stream. */
export const frameStore = createFrameStore();
