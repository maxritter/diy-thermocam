import { useCallback, useEffect, useRef, useState } from "react";
import type { CaptureRef, SidecarRegion, ThermoSidecar } from "@/lib/api/captures";
import type { ColorRange } from "@/lib/colorRange";
import {
  canRedo,
  canUndo,
  commit,
  createHistory,
  type History,
  redo,
  undo,
} from "@/lib/editHistory";
import { DEFAULT_MATERIALS } from "@/lib/palettes";
import { objectCToRaw, rawToObjectC } from "@/lib/thermal";
import type { Material, RadiometryParams, Roi, RoiGeometry, ThermalFrame } from "@/lib/types";

/**
 * What an opened capture keeps (captures.html#opened): palette, color scale, material, regions
 * and notes. The color scale is kept in °C so it means the same temperatures after a material
 * change, as the edits file stores it.
 */
export interface Edits {
  paletteId: string;
  range: { mode: "auto" } | { mode: "manual"; lo_c: number; hi_c: number };
  material: { id: string | null; emissivity: number; reflected_c: number };
  rois: Roi[];
  notes: { name: string; text: string };
}

/** Live's starting material: ε 0.95 and a 22 °C room. */
export const DEFAULT_MATERIAL = { id: null, emissivity: 0.95, reflected_c: 22 } as const;

export function originalEdits(paletteId: string): Edits {
  return {
    paletteId,
    range: { mode: "auto" },
    material: { ...DEFAULT_MATERIAL },
    rois: [],
    notes: { name: "", text: "" },
  };
}

function presetFor(emissivity: number): string | null {
  return DEFAULT_MATERIALS.find((m) => Math.abs(m.emissivity - emissivity) < 1e-6)?.id ?? null;
}

/** Rust keeps pixel positions of spots, boxes and lines as whole numbers. */
function wholePixels(g: RoiGeometry): RoiGeometry {
  const r = (v: number) => Math.max(0, Math.round(v));
  switch (g.kind) {
    case "spot":
      return { ...g, x: r(g.x), y: r(g.y), kernel: Math.max(1, Math.round(g.kernel)) };
    case "box":
      return { kind: "box", x: r(g.x), y: r(g.y), w: Math.max(1, r(g.w)), h: Math.max(1, r(g.h)) };
    case "line":
    case "delta":
      return { kind: g.kind, x0: r(g.x0), y0: r(g.y0), x1: r(g.x1), y1: r(g.y1) };
    default:
      return g;
  }
}

export function regionFromRoi(roi: Roi, image: Material): SidecarRegion {
  const own =
    roi.emissivity_override !== null || roi.reflected_temp_override_c !== null
      ? {
          emissivity: roi.emissivity_override ?? image.emissivity,
          reflected_c: roi.reflected_temp_override_c ?? image.reflected_c,
        }
      : null;
  return {
    id: roi.id,
    name: roi.label,
    geometry: wholePixels(roi.geometry),
    material: own,
    visible: roi.visible,
  };
}

export function roiFromRegion(region: SidecarRegion, index: number): Roi {
  return {
    id: region.id,
    label: region.name,
    geometry: region.geometry,
    emissivity_override: region.material?.emissivity ?? null,
    reflected_temp_override_c: region.material?.reflected_c ?? null,
    material_id: region.material ? presetFor(region.material.emissivity) : null,
    color: index,
    visible: region.visible,
  };
}

export function editsFromSidecar(s: ThermoSidecar): Edits {
  return {
    paletteId: s.palette_id,
    range:
      s.range.mode === "manual" && s.range.min_c !== null && s.range.max_c !== null
        ? { mode: "manual", lo_c: s.range.min_c, hi_c: s.range.max_c }
        : { mode: "auto" },
    material: {
      id: presetFor(s.material.emissivity),
      emissivity: s.material.emissivity,
      reflected_c: s.material.reflected_c,
    },
    rois: s.regions.map(roiFromRegion),
    notes: { name: s.notes.name, text: s.notes.text },
  };
}

export function sidecarFromEdits(e: Edits, sensorMaterial: Material | null): ThermoSidecar {
  const image = { emissivity: e.material.emissivity, reflected_c: e.material.reflected_c };
  return {
    schema_version: 2,
    palette_id: e.paletteId,
    range:
      e.range.mode === "manual"
        ? { mode: "manual", min_c: e.range.lo_c, max_c: e.range.hi_c }
        : { mode: "auto", min_c: null, max_c: null },
    material: image,
    sensor_material: sensorMaterial,
    regions: e.rois.map((r) => regionFromRoi(r, image)),
    notes: { ...e.notes },
  };
}

/** Every reading of the capture: its calibration, its material, what its sensor held. */
export function radFor(
  frame: Pick<ThermalFrame, "cal_offset" | "cal_slope"> | null,
  e: Edits,
  sensorMaterial: Material | null,
): RadiometryParams {
  return {
    cal_offset: frame?.cal_offset ?? -273.15,
    cal_slope: frame?.cal_slope ?? 0.01,
    emissivity: e.material.emissivity,
    reflected_temp_c: e.material.reflected_c,
    sensor_material: sensorMaterial,
  };
}

/** The viewer's raw color range for the edits' °C range. */
export function rawRange(e: Edits, rad: RadiometryParams): ColorRange {
  if (e.range.mode === "auto") return { mode: "auto" };
  return {
    mode: "manual",
    lo: Math.round(objectCToRaw(e.range.lo_c, rad)),
    hi: Math.round(objectCToRaw(e.range.hi_c, rad)),
  };
}

export function celsiusRange(r: ColorRange, rad: RadiometryParams): Edits["range"] {
  if (r.mode === "auto") return { mode: "auto" };
  const round = (v: number) => Math.round(v * 100) / 100;
  return {
    mode: "manual",
    lo_c: round(rawToObjectC(r.lo, rad)),
    hi_c: round(rawToObjectC(r.hi, rad)),
  };
}

export type SaveState =
  | { kind: "original" }
  | { kind: "saving" }
  | { kind: "saved" }
  | { kind: "error"; detail: string };

export interface SaveApi {
  sidecarSave: (ref: CaptureRef, s: ThermoSidecar) => Promise<string>;
  sidecarReset: (ref: CaptureRef) => Promise<boolean>;
}

/**
 * The edits of one opened capture: Undo and Redo over 100 steps, saved 500 ms after the last
 * change; Reset to original deletes the edits file (and can itself be undone).
 */
export function useCaptureEdits(args: {
  api: SaveApi;
  captureRef: CaptureRef;
  /** The edits the capture opened with, and whether they came from an edits file. */
  initial: Edits;
  hadSidecar: boolean;
  original: Edits;
  sensorMaterial: Material | null;
  saveDelayMs?: number;
}) {
  const { api, captureRef, original, sensorMaterial, saveDelayMs = 500 } = args;
  const [history, setHistory] = useState<History<Edits>>(() => createHistory(args.initial));
  const [save, setSave] = useState<SaveState>(() =>
    args.hadSidecar ? { kind: "saved" } : { kind: "original" },
  );
  const historyRef = useRef(history);
  historyRef.current = history;
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const pending = useRef<Edits | null>(null);
  const live = useRef({ api, captureRef, sensorMaterial });
  live.current = { api, captureRef, sensorMaterial };

  const writeNow = useCallback(async (edits: Edits) => {
    pending.current = null;
    setSave({ kind: "saving" });
    try {
      const { api: a, captureRef: ref, sensorMaterial: sm } = live.current;
      await a.sidecarSave(ref, sidecarFromEdits(edits, sm));
      setSave({ kind: "saved" });
    } catch (err) {
      const detail = String((err as { detail?: unknown })?.detail ?? err);
      setSave({ kind: "error", detail });
    }
  }, []);

  const schedule = useCallback(
    (edits: Edits) => {
      pending.current = edits;
      setSave({ kind: "saving" });
      if (timer.current) clearTimeout(timer.current);
      timer.current = setTimeout(() => {
        timer.current = null;
        if (pending.current) void writeNow(pending.current);
      }, saveDelayMs);
    },
    [saveDelayMs, writeNow],
  );

  /** Saves a waiting change at once (before another capture opens or the page closes). */
  const flush = useCallback(() => {
    if (timer.current) {
      clearTimeout(timer.current);
      timer.current = null;
    }
    if (pending.current) void writeNow(pending.current);
  }, [writeNow]);

  useEffect(() => flush, [flush]);

  const apply = useCallback(
    (next: History<Edits>) => {
      if (next === historyRef.current) return;
      historyRef.current = next;
      setHistory(next);
      schedule(next.present);
    },
    [schedule],
  );

  const update = useCallback(
    (fn: (e: Edits) => Edits, mergeKey?: string) => {
      const h = historyRef.current;
      apply(commit(h, fn(h.present), mergeKey));
    },
    [apply],
  );

  const reset = useCallback(async () => {
    if (timer.current) clearTimeout(timer.current);
    timer.current = null;
    pending.current = null;
    const next = commit(historyRef.current, original);
    historyRef.current = next;
    setHistory(next);
    try {
      await live.current.api.sidecarReset(live.current.captureRef);
      setSave({ kind: "original" });
    } catch (err) {
      setSave({ kind: "error", detail: String((err as { detail?: unknown })?.detail ?? err) });
    }
  }, [original]);

  return {
    edits: history.present,
    update,
    undo: () => apply(undo(historyRef.current)),
    redo: () => apply(redo(historyRef.current)),
    canUndo: canUndo(history),
    canRedo: canRedo(history),
    reset,
    save,
    flush,
  };
}
