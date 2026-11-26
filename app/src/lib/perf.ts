/**
 * Developer-only performance counters (Done means A15).
 *
 * Counts frames received and drawn, frame-to-screen latency, and how often each
 * panel renders. Exposed as `window.__studioPerf` in development builds only;
 * nothing here is shown in the UI.
 */

const LATENCY_SAMPLES = 600;
const PENDING_LIMIT = 64;

export interface PerfSnapshot {
  received: number;
  drawn: number;
  frameToScreenP95Ms: number | null;
  frameToScreenMaxMs: number | null;
  panelRendersPerSecond: Record<string, number>;
}

export interface PerfMeter {
  frameArrived(seq: number): void;
  frameDrawn(seq: number): void;
  panelRendered(name: string): void;
  snapshot(): PerfSnapshot;
  reset(): void;
}

/** Nearest-rank percentile; `null` for an empty set. */
export function percentile(values: readonly number[], p: number): number | null {
  if (values.length === 0) return null;
  const sorted = [...values].sort((a, b) => a - b);
  if (p <= 0) return sorted[0];
  const rank = Math.ceil((p / 100) * sorted.length);
  return sorted[Math.min(sorted.length, rank) - 1];
}

export function createPerfMeter({
  now = () => performance.now(),
}: {
  now?: () => number;
} = {}): PerfMeter {
  let received = 0;
  let drawn = 0;
  const arrivals = new Map<number, number>();
  let latencies: number[] = [];
  let renders = new Map<string, number[]>();

  return {
    frameArrived(seq) {
      received += 1;
      arrivals.set(seq, now());
      if (arrivals.size > PENDING_LIMIT) {
        const oldest = arrivals.keys().next().value;
        if (oldest !== undefined) arrivals.delete(oldest);
      }
    },
    frameDrawn(seq) {
      const at = arrivals.get(seq);
      if (at === undefined) return;
      arrivals.delete(seq);
      drawn += 1;
      latencies.push(now() - at);
      if (latencies.length > LATENCY_SAMPLES) latencies = latencies.slice(-LATENCY_SAMPLES);
    },
    panelRendered(name) {
      const list = renders.get(name) ?? [];
      list.push(now());
      if (list.length > 256) list.splice(0, list.length - 256);
      renders.set(name, list);
    },
    snapshot() {
      const t = now();
      const panelRendersPerSecond: Record<string, number> = {};
      for (const [name, times] of renders) {
        panelRendersPerSecond[name] = times.filter((x) => t - x <= 1000).length;
      }
      return {
        received,
        drawn,
        frameToScreenP95Ms: percentile(latencies, 95),
        frameToScreenMaxMs: latencies.length ? Math.max(...latencies) : null,
        panelRendersPerSecond,
      };
    },
    reset() {
      received = 0;
      drawn = 0;
      arrivals.clear();
      latencies = [];
      renders = new Map();
    },
  };
}

/** The app's meter. Cheap enough to always count; only exposed in development. */
export const perf = createPerfMeter();

export function installPerfGlobal(): void {
  if (!import.meta.env?.DEV || typeof window === "undefined") return;
  (window as unknown as { __studioPerf?: PerfMeter }).__studioPerf = perf;
}
