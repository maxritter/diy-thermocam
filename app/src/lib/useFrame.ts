import { useSyncExternalStore } from "react";
import { type FrameStore, frameStore, type HoverPixel } from "./frameStore";
import type { ThermalFrame, TimeSeriesPoint } from "./types";

/** Every frame. Only the viewer should use this. */
export function useFrame(store: FrameStore = frameStore): ThermalFrame | null {
  return useSyncExternalStore(store.subscribeFrame, store.getFrame, store.getFrame);
}

/** The newest frame, at most four times a second. For panels, readings and charts. */
export function useSampledFrame(store: FrameStore = frameStore): ThermalFrame | null {
  return useSyncExternalStore(store.subscribeSampled, store.getSampled, store.getSampled);
}

/** The pixel under the cursor. Re-renders only its readers. */
export function useHover(store: FrameStore = frameStore): HoverPixel | null {
  return useSyncExternalStore(store.subscribeHover, store.getHover, store.getHover);
}

/** The time series, sampled like `useSampledFrame`. */
export function useSeries(store: FrameStore = frameStore): readonly TimeSeriesPoint[] {
  return useSyncExternalStore(store.subscribeSampled, store.getSeries, store.getSeries);
}
