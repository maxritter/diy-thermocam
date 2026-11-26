import type { Roi, RoiGeometry, RoiStats, RoiTool } from "./types";

/**
 * Region geometry on the sensor, as plain functions. Coordinates are sensor pixels:
 * points (spots, line ends, polygon corners) are pixel indices 0…width−1, and a box covers
 * the pixels from `x` to `x + w − 1`, so its edges run from 0 to `width`. Pointer positions
 * are continuous sensor coordinates (pixel 12 spans 12.0–13.0).
 */

export type Pt = [number, number];

export interface FrameSize {
  width: number;
  height: number;
}

export type BoxGeometry = Extract<RoiGeometry, { kind: "box" }>;
export type RegionKind = RoiGeometry["kind"];

export type BoxHandle = "nw" | "n" | "ne" | "e" | "se" | "s" | "sw" | "w";
export const BOX_HANDLES: BoxHandle[] = ["nw", "n", "ne", "e", "se", "s", "sw", "w"];

export type Handle =
  | { kind: "box"; h: BoxHandle }
  | { kind: "radius" }
  | { kind: "point"; index: number };

export type Hit =
  | { roiId: string; part: "body" }
  | { roiId: string; part: "handle"; handle: Handle };

/** Smallest box side, in pixels. */
const MIN_SIDE = 2;
/** A drag shorter than this (in sensor pixels) is a click, not a shape. */
const MIN_DRAG = 2;

const clamp = (v: number, lo: number, hi: number) => Math.max(lo, Math.min(hi, v));

/** The pixel under a continuous position, kept on the sensor. */
export function toPixel(pt: Pt, size: FrameSize): Pt {
  return [
    clamp(Math.floor(pt[0]), 0, size.width - 1),
    clamp(Math.floor(pt[1]), 0, size.height - 1),
  ];
}

/**
 * One side of a box between a fixed edge and a moving one: ordered, at least `MIN_SIDE`
 * long (grown towards the moving edge) and kept inside 0…max.
 */
function side(fixed: number, moving: number, max: number): [number, number] {
  const m = clamp(Math.round(moving), 0, max);
  let lo = Math.min(fixed, m);
  let hi = Math.max(fixed, m);
  if (hi - lo < MIN_SIDE) {
    if (m >= fixed) {
      lo = fixed;
      hi = fixed + MIN_SIDE;
    } else {
      hi = fixed;
      lo = fixed - MIN_SIDE;
    }
  }
  if (lo < 0) {
    hi -= lo;
    lo = 0;
  }
  if (hi > max) {
    lo -= hi - max;
    hi = max;
  }
  return [lo, hi];
}

export function resizeBox(
  box: BoxGeometry,
  handle: BoxHandle,
  pt: Pt,
  size: FrameSize,
): BoxGeometry {
  const left = box.x;
  const right = box.x + box.w;
  const top = box.y;
  const bottom = box.y + box.h;
  let [x0, x1] = [left, right];
  let [y0, y1] = [top, bottom];
  if (handle.includes("w")) [x0, x1] = side(right, pt[0], size.width);
  else if (handle.includes("e")) [x0, x1] = side(left, pt[0], size.width);
  if (handle.includes("n")) [y0, y1] = side(bottom, pt[1], size.height);
  else if (handle.includes("s")) [y0, y1] = side(top, pt[1], size.height);
  return { kind: "box", x: x0, y: y0, w: x1 - x0, h: y1 - y0 };
}

export function resizeGeometry(
  geom: RoiGeometry,
  handle: Handle,
  pt: Pt,
  size: FrameSize,
): RoiGeometry {
  if (handle.kind === "box" && geom.kind === "box") return resizeBox(geom, handle.h, pt, size);
  if (handle.kind === "radius" && geom.kind === "circle") {
    const r = Math.hypot(pt[0] - (geom.cx + 0.5), pt[1] - (geom.cy + 0.5));
    return { ...geom, radius: Math.max(1, Math.round(r)) };
  }
  if (handle.kind === "point") {
    const [px, py] = toPixel(pt, size);
    if (geom.kind === "line" || geom.kind === "delta") {
      return handle.index === 0 ? { ...geom, x0: px, y0: py } : { ...geom, x1: px, y1: py };
    }
    if (geom.kind === "polygon" && handle.index < geom.points.length) {
      const points = geom.points.map((p, i) => (i === handle.index ? ([px, py] as Pt) : p));
      return { ...geom, points };
    }
  }
  return geom;
}

/** Where the handles of a region sit, in continuous sensor coordinates. */
export function handlePositions(geom: RoiGeometry): { handle: Handle; x: number; y: number }[] {
  switch (geom.kind) {
    case "box": {
      const { x, y, w, h } = geom;
      const at: Record<BoxHandle, Pt> = {
        nw: [x, y],
        n: [x + w / 2, y],
        ne: [x + w, y],
        e: [x + w, y + h / 2],
        se: [x + w, y + h],
        s: [x + w / 2, y + h],
        sw: [x, y + h],
        w: [x, y + h / 2],
      };
      return BOX_HANDLES.map((h) => ({ handle: { kind: "box", h }, x: at[h][0], y: at[h][1] }));
    }
    case "circle":
      return [{ handle: { kind: "radius" }, x: geom.cx + 0.5 + geom.radius, y: geom.cy + 0.5 }];
    case "line":
    case "delta":
      return [
        { handle: { kind: "point", index: 0 }, x: geom.x0 + 0.5, y: geom.y0 + 0.5 },
        { handle: { kind: "point", index: 1 }, x: geom.x1 + 0.5, y: geom.y1 + 0.5 },
      ];
    case "polygon":
      return geom.points.map(([x, y], index) => ({
        handle: { kind: "point", index },
        x: x + 0.5,
        y: y + 0.5,
      }));
    case "spot":
      return [];
  }
}

/** Bounds of the pixels a region's points use (for moving). */
function pointBounds(points: Pt[]): { minX: number; maxX: number; minY: number; maxY: number } {
  let minX = Number.POSITIVE_INFINITY;
  let maxX = Number.NEGATIVE_INFINITY;
  let minY = Number.POSITIVE_INFINITY;
  let maxY = Number.NEGATIVE_INFINITY;
  for (const [x, y] of points) {
    minX = Math.min(minX, x);
    maxX = Math.max(maxX, x);
    minY = Math.min(minY, y);
    maxY = Math.max(maxY, y);
  }
  return { minX, maxX, minY, maxY };
}

/** Moves a region by whole pixels; it stops at the frame's edge instead of leaving it. */
export function moveGeometry(
  geom: RoiGeometry,
  dx: number,
  dy: number,
  size: FrameSize,
): RoiGeometry {
  const rx = Math.round(dx);
  const ry = Math.round(dy);
  const shift = (
    b: { minX: number; maxX: number; minY: number; maxY: number },
    maxX: number,
    maxY: number,
  ): Pt => [clamp(rx, -b.minX, maxX - b.maxX), clamp(ry, -b.minY, maxY - b.maxY)];
  const W = size.width - 1;
  const H = size.height - 1;
  switch (geom.kind) {
    case "box": {
      const [sx, sy] = shift(
        { minX: geom.x, maxX: geom.x + geom.w, minY: geom.y, maxY: geom.y + geom.h },
        size.width,
        size.height,
      );
      return { ...geom, x: geom.x + sx, y: geom.y + sy };
    }
    case "spot": {
      const [sx, sy] = shift({ minX: geom.x, maxX: geom.x, minY: geom.y, maxY: geom.y }, W, H);
      return { ...geom, x: geom.x + sx, y: geom.y + sy };
    }
    case "circle": {
      const [sx, sy] = shift({ minX: geom.cx, maxX: geom.cx, minY: geom.cy, maxY: geom.cy }, W, H);
      return { ...geom, cx: geom.cx + sx, cy: geom.cy + sy };
    }
    case "line":
    case "delta": {
      const [sx, sy] = shift(
        pointBounds([
          [geom.x0, geom.y0],
          [geom.x1, geom.y1],
        ]),
        W,
        H,
      );
      return { ...geom, x0: geom.x0 + sx, y0: geom.y0 + sy, x1: geom.x1 + sx, y1: geom.y1 + sy };
    }
    case "polygon": {
      const [sx, sy] = shift(pointBounds(geom.points), W, H);
      return { ...geom, points: geom.points.map(([x, y]) => [x + sx, y + sy] as Pt) };
    }
  }
}

export type DrawTool = Exclude<RoiTool, "select" | "polygon">;

/** The shape a drag from `a` to `b` draws, or null when the drag is only a click. */
export function geometryFromDrag(
  tool: DrawTool,
  a: Pt,
  b: Pt,
  size: FrameSize,
): RoiGeometry | null {
  if (tool === "spot" || tool === "spot3") {
    const [x, y] = toPixel(a, size);
    return { kind: "spot", x, y, kernel: tool === "spot3" ? 3 : 1, show_raw: false };
  }
  const dist = Math.hypot(b[0] - a[0], b[1] - a[1]);
  if (dist < MIN_DRAG) return null;
  if (tool === "box") {
    const ax = clamp(Math.round(a[0]), 0, size.width);
    const ay = clamp(Math.round(a[1]), 0, size.height);
    const [x0, x1] = side(ax, b[0], size.width);
    const [y0, y1] = side(ay, b[1], size.height);
    return { kind: "box", x: x0, y: y0, w: x1 - x0, h: y1 - y0 };
  }
  if (tool === "circle") {
    const [cx, cy] = toPixel(a, size);
    return { kind: "circle", cx, cy, radius: Math.max(1, Math.round(dist)) };
  }
  const [x0, y0] = toPixel(a, size);
  const [x1, y1] = toPixel(b, size);
  return { kind: tool, x0, y0, x1, y1 };
}

export type PolygonEvent = { type: "click"; pt: Pt } | { type: "dblclick" } | { type: "cancel" };

/**
 * One step of drawing a polygon. A click adds a corner (a repeat click on the same pixel adds
 * nothing); clicking near the first corner or double-clicking closes it once it has three
 * corners; Esc cancels. `done` holds the finished corners.
 */
export function polygonStep(
  draft: Pt[],
  ev: PolygonEvent,
  tolerance: number,
  size: FrameSize,
): { draft: Pt[]; done: Pt[] | null } {
  if (ev.type === "cancel") return { draft: [], done: null };
  if (ev.type === "dblclick") {
    return draft.length >= 3 ? { draft: [], done: draft } : { draft, done: null };
  }
  const p = toPixel(ev.pt, size);
  const last = draft[draft.length - 1];
  if (last && last[0] === p[0] && last[1] === p[1]) return { draft, done: null };
  const first = draft[0];
  if (draft.length >= 3 && first && Math.hypot(p[0] - first[0], p[1] - first[1]) <= tolerance) {
    return { draft: [], done: draft };
  }
  return { draft: [...draft, p], done: null };
}

function distToSegment(p: Pt, a: Pt, b: Pt): number {
  const dx = b[0] - a[0];
  const dy = b[1] - a[1];
  const len2 = dx * dx + dy * dy;
  const t = len2 > 0 ? clamp(((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / len2, 0, 1) : 0;
  return Math.hypot(p[0] - (a[0] + t * dx), p[1] - (a[1] + t * dy));
}

function insidePolygon(p: Pt, poly: Pt[]): boolean {
  let inside = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const [xi, yi] = poly[i];
    const [xj, yj] = poly[j];
    if (yi > p[1] !== yj > p[1] && p[0] < ((xj - xi) * (p[1] - yi)) / (yj - yi || 1e-9) + xi) {
      inside = !inside;
    }
  }
  return inside;
}

/** Whether a pointer at `p` touches a region's shape. */
export function hitsBody(geom: RoiGeometry, p: Pt, tol: number): boolean {
  const c = (x: number, y: number): Pt => [x + 0.5, y + 0.5];
  switch (geom.kind) {
    case "spot": {
      const [x, y] = c(geom.x, geom.y);
      return Math.hypot(p[0] - x, p[1] - y) <= Math.max(tol, geom.kernel >= 3 ? 2.5 : 2);
    }
    case "box":
      return (
        p[0] >= geom.x - tol &&
        p[0] <= geom.x + geom.w + tol &&
        p[1] >= geom.y - tol &&
        p[1] <= geom.y + geom.h + tol
      );
    case "circle": {
      const [x, y] = c(geom.cx, geom.cy);
      return Math.hypot(p[0] - x, p[1] - y) <= geom.radius + tol;
    }
    case "line":
    case "delta":
      return distToSegment(p, c(geom.x0, geom.y0), c(geom.x1, geom.y1)) <= tol;
    case "polygon": {
      const pts = geom.points.map(([x, y]) => c(x, y));
      if (pts.length >= 3 && insidePolygon(p, pts)) return true;
      return pts.some((a, i) => distToSegment(p, a, pts[(i + 1) % pts.length]) <= tol);
    }
  }
}

/**
 * What a pointer at `p` would grab: the selected region's handles first, then the top-most
 * visible region (the last one drawn).
 */
export function hitTest(rois: Roi[], p: Pt, tol: number, selectedId: string | null): Hit | null {
  const selected = selectedId ? rois.find((r) => r.id === selectedId && r.visible) : undefined;
  if (selected) {
    for (const h of handlePositions(selected.geometry)) {
      if (Math.hypot(p[0] - h.x, p[1] - h.y) <= tol) {
        return { roiId: selected.id, part: "handle", handle: h.handle };
      }
    }
  }
  for (let i = rois.length - 1; i >= 0; i--) {
    const r = rois[i];
    if (r.visible && hitsBody(r.geometry, p, tol)) return { roiId: r.id, part: "body" };
  }
  return null;
}

/** Each region keeps one color in the list, on the image and in the chart. */
export const REGION_COLORS = ["var(--s-1)", "var(--s-2)", "var(--s-3)", "var(--s-4)"];

export function regionColor(index: number): string {
  return REGION_COLORS[
    ((index % REGION_COLORS.length) + REGION_COLORS.length) % REGION_COLORS.length
  ];
}

/** A region's color: the one it got when drawn, so deleting another region never changes it. */
export function roiColor(roi: Roi, index: number): string {
  return regionColor(roi.color ?? index);
}

/** The first color no region uses yet (cycling once all are taken). */
export function nextColorIndex(rois: Roi[]): number {
  const used = new Set(rois.map((r, i) => (r.color ?? i) % REGION_COLORS.length));
  for (let c = 0; c < REGION_COLORS.length; c++) if (!used.has(c)) return c;
  return rois.length % REGION_COLORS.length;
}

const KIND_LABEL: Record<RegionKind, string> = {
  spot: "Spot",
  box: "Box",
  circle: "Circle",
  polygon: "Polygon",
  line: "Line",
  delta: "Difference",
};

export function regionKindLabel(kind: RegionKind): string {
  return KIND_LABEL[kind];
}

/** "Box 3": the kind and the next free number for it. */
export function nextRegionName(kind: RegionKind, rois: Roi[]): string {
  const base = KIND_LABEL[kind];
  let n = 0;
  for (const r of rois) {
    const m = new RegExp(`^${base} (\\d+)$`).exec(r.label);
    if (m) n = Math.max(n, Number(m[1]));
  }
  return `${base} ${n + 1}`;
}

/** The value a region's row, label and chart line show. */
export type RowStat = "avg" | "max" | "delta";

export function rowStat(kind: RegionKind): RowStat {
  if (kind === "spot") return "avg";
  if (kind === "delta") return "delta";
  return "max";
}

export function rowValue(kind: RegionKind, stats: RoiStats): number | null {
  const stat = rowStat(kind);
  if (stat === "delta") return stats.delta_c;
  return stat === "avg" ? stats.mean_c : stats.max_c;
}

/** Where a region's label chip sits, in continuous sensor coordinates. */
export function labelAnchor(
  geom: RoiGeometry,
  size: FrameSize,
): { x: number; y: number; place: "above" | "below" } {
  const nearTop = (y: number) => y < size.height * 0.12;
  switch (geom.kind) {
    case "spot": {
      const y = geom.y + 0.5;
      return { x: geom.x + 0.5, y, place: nearTop(y) ? "below" : "above" };
    }
    case "box": {
      const x = geom.x + geom.w / 2;
      const below = geom.y + geom.h;
      return below > size.height * 0.88
        ? { x, y: geom.y, place: "above" }
        : { x, y: below, place: "below" };
    }
    case "circle": {
      const x = geom.cx + 0.5;
      const top = geom.cy + 0.5 - geom.radius;
      return nearTop(top)
        ? { x, y: geom.cy + 0.5 + geom.radius, place: "below" }
        : { x, y: top, place: "above" };
    }
    case "line":
    case "delta": {
      const x = (geom.x0 + geom.x1) / 2 + 0.5;
      const y = (geom.y0 + geom.y1) / 2 + 0.5;
      const place = geom.kind === "delta" || nearTop(y) ? "below" : "above";
      return { x, y, place };
    }
    case "polygon": {
      const b = pointBounds(geom.points);
      const x = (b.minX + b.maxX) / 2 + 0.5;
      return nearTop(b.minY)
        ? { x, y: b.maxY + 1, place: "below" }
        : { x, y: b.minY, place: "above" };
    }
  }
}
