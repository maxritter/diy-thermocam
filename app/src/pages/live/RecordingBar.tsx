import { Clock } from "lucide-react";
import { useEffect, useState, useSyncExternalStore } from "react";
import type { FrameStore } from "@/lib/frameStore";
import { clockLabel, intervalLabel, type Recorder, type RecorderState } from "./captureActions";

/** What the camera connection looks like from Live. */
export type LiveConnection = "live" | "demo" | "lost" | "none";

export function useRecorder(recorder: Recorder): RecorderState {
  return useSyncExternalStore(recorder.subscribe, recorder.get, recorder.get);
}

function timeOfDay(d: Date): string {
  return d.toLocaleTimeString(undefined, {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false,
  });
}

/** The recording's clock, ticking every second between Rust's status reads. */
function useClock(state: RecorderState): number {
  const [now, setNow] = useState(() => Date.now());
  const active = state.mode === "recording";
  useEffect(() => {
    if (!active) return;
    setNow(Date.now());
    const id = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(id);
  }, [active]);
  return state.mode === "recording" ? Math.max(state.elapsedMs, now - state.startedAt) : 0;
}

/** Frames per second over the last second, from the store's counter (null without a store). */
function useFps(store: FrameStore | null): number | null {
  const [fps, setFps] = useState<number | null>(null);
  useEffect(() => {
    if (!store) return;
    let last = store.counters().received;
    let lastT = performance.now();
    const id = setInterval(() => {
      const now = performance.now();
      const n = store.counters().received;
      setFps(((n - last) * 1000) / Math.max(1, now - lastT));
      last = n;
      lastT = now;
    }, 1000);
    return () => clearInterval(id);
  }, [store]);
  return fps;
}

export interface RecordingBarProps {
  recorder: Recorder;
  connection: LiveConnection;
  streaming: boolean;
  /** Frames per second: counted from the store, or given (tests). */
  store?: FrameStore;
  fps?: number | null;
  /** When the frame on screen arrived, for Paused and Camera disconnected. */
  frameTime?: Date | null;
}

/** The left of the bar above the image: what Studio is doing right now (`live.html#states`). */
export function RecordingBar({
  recorder,
  connection,
  streaming,
  store,
  fps: givenFps = null,
  frameTime = null,
}: RecordingBarProps): React.JSX.Element | null {
  const state = useRecorder(recorder);
  const elapsed = useClock(state);
  const counted = useFps(store ?? null);
  const fps = store ? counted : givenFps;
  if (connection === "none") return null;

  const demoPill =
    connection === "demo" ? (
      <span className="st-live-pill">
        <span className="st-status-dot demo" />
        Demo <span className="muted">sample captures</span>
      </span>
    ) : null;

  if (state.mode === "recording") {
    return (
      <>
        <span className="lv-rec-pill">
          <span className="st-status-dot rec" />
          Recording{" "}
          <span className="font-mono-tech lv-rec-n">
            {clockLabel(elapsed)} · {state.frames} frames
          </span>
        </span>
        {connection === "demo" ? (
          <span className="st-live-pill" title="Demo camera: real sample captures in turn">
            <span className="st-status-dot demo" />
            Demo
          </span>
        ) : null}
      </>
    );
  }

  let pill: React.JSX.Element;
  if (connection === "lost") {
    pill = (
      <span className="st-live-pill">
        <span className="st-status-dot off" />
        Camera disconnected{" "}
        <span className="muted">
          reconnects by itself{frameTime ? ` · last frame ${timeOfDay(frameTime)}` : ""}
        </span>
      </span>
    );
  } else if (!streaming) {
    pill = (
      <span className="st-live-pill">
        <span className="st-status-dot off" />
        Paused {frameTime ? <span className="muted">frame from {timeOfDay(frameTime)}</span> : null}
      </span>
    );
  } else if (demoPill) {
    pill = demoPill;
  } else {
    pill = (
      <span className="st-live-pill">
        <span className="st-status-dot" />
        Live{" "}
        {fps !== null ? <span className="muted font-mono-tech">{fps.toFixed(1)} fps</span> : null}
      </span>
    );
  }

  return (
    <>
      {pill}
      {demoPill && pill !== demoPill ? demoPill : null}
      {state.mode === "timelapse" ? (
        <span className="lv-badge amber">
          <Clock size={14} strokeWidth={1.75} aria-hidden="true" />
          {state.intervalMs !== null ? `Every ${intervalLabel(state.intervalMs)} · ` : ""}
          {state.frames} saved
          {connection === "lost" ? " · waits for the camera" : ""}
        </span>
      ) : null}
    </>
  );
}
