import { describe, expect, test } from "bun:test";
import {
  BOX_HANDLES,
  type BoxGeometry,
  type BoxHandle,
  geometryFromDrag,
  handlePositions,
  hitTest,
  moveGeometry,
  nextRegionName,
  polygonStep,
  resizeBox,
  resizeGeometry,
} from "./regions";
import type { Roi, RoiGeometry } from "./types";

const SIZE = { width: 160, height: 120 };

/** A box in the middle of the frame, far from every edge. */
const BOX: BoxGeometry = { kind: "box", x: 60, y: 40, w: 30, h: 20 };

function edges(b: BoxGeometry) {
  return { left: b.x, right: b.x + b.w, top: b.y, bottom: b.y + b.h };
}

function expectInside(b: BoxGeometry) {
  expect(b.x).toBeGreaterThanOrEqual(0);
  expect(b.y).toBeGreaterThanOrEqual(0);
  expect(b.x + b.w).toBeLessThanOrEqual(SIZE.width);
  expect(b.y + b.h).toBeLessThanOrEqual(SIZE.height);
  expect(b.w).toBeGreaterThanOrEqual(2);
  expect(b.h).toBeGreaterThanOrEqual(2);
  expect(Number.isInteger(b.x) && Number.isInteger(b.y)).toBe(true);
  expect(Number.isInteger(b.w) && Number.isInteger(b.h)).toBe(true);
}

function roi(id: string, geometry: RoiGeometry): Roi {
  return {
    id,
    label: id,
    geometry,
    emissivity_override: null,
    reflected_temp_override_c: null,
    visible: true,
  };
}

/** Which edges a handle moves. */
const MOVES: Record<BoxHandle, { x?: "left" | "right"; y?: "top" | "bottom" }> = {
  nw: { x: "left", y: "top" },
  n: { y: "top" },
  ne: { x: "right", y: "top" },
  e: { x: "right" },
  se: { x: "right", y: "bottom" },
  s: { y: "bottom" },
  sw: { x: "left", y: "bottom" },
  w: { x: "left" },
};

const DIRECTIONS: [number, number][] = [
  [0, -1],
  [1, -1],
  [1, 0],
  [1, 1],
  [0, 1],
  [-1, 1],
  [-1, 0],
  [-1, -1],
];

/** Small, past the opposite edge, and far outside the frame. */
const DISTANCES = [1, 7, 45, 400];

describe("resizing a box", () => {
  test("every handle × every direction × every distance keeps a valid box", () => {
    let cases = 0;
    for (const handle of BOX_HANDLES) {
      const start = handlePositions(BOX).find(
        (h) => h.handle.kind === "box" && h.handle.h === handle,
      );
      expect(start).toBeTruthy();
      if (!start) continue;
      for (const [dx, dy] of DIRECTIONS) {
        for (const d of DISTANCES) {
          const pt: [number, number] = [start.x + dx * d, start.y + dy * d];
          const out = resizeBox(BOX, handle, pt, SIZE);
          expectInside(out);
          cases++;

          const before = edges(BOX);
          const after = edges(out);
          const moves = MOVES[handle];
          // An edge the handle does not own never moves.
          if (!moves.x) {
            expect(after.left).toBe(before.left);
            expect(after.right).toBe(before.right);
          }
          if (!moves.y) {
            expect(after.top).toBe(before.top);
            expect(after.bottom).toBe(before.bottom);
          }
          // The edge it owns follows the pointer, clamped to the frame; past the opposite edge
          // the box flips and the opposite edge stays where it was.
          const px = Math.max(0, Math.min(SIZE.width, Math.round(pt[0])));
          const py = Math.max(0, Math.min(SIZE.height, Math.round(pt[1])));
          if (moves.x) {
            const fixed = moves.x === "left" ? before.right : before.left;
            if (Math.abs(px - fixed) >= 2) {
              expect([after.left, after.right].sort((a, b) => a - b)).toEqual(
                [px, fixed].sort((a, b) => a - b),
              );
            } else {
              expect(out.w).toBe(2);
            }
          }
          if (moves.y) {
            const fixed = moves.y === "top" ? before.bottom : before.top;
            if (Math.abs(py - fixed) >= 2) {
              expect([after.top, after.bottom].sort((a, b) => a - b)).toEqual(
                [py, fixed].sort((a, b) => a - b),
              );
            } else {
              expect(out.h).toBe(2);
            }
          }
        }
      }
    }
    expect(cases).toBe(8 * 8 * DISTANCES.length);
  });

  test("dragging the east handle past the west edge flips the box", () => {
    expect(resizeBox(BOX, "e", [40, 50], SIZE)).toEqual({
      kind: "box",
      x: 40,
      y: 40,
      w: 20,
      h: 20,
    });
  });

  test("dragging a corner outside the frame clamps it to the frame", () => {
    expect(resizeBox(BOX, "se", [900, 900], SIZE)).toEqual({
      kind: "box",
      x: 60,
      y: 40,
      w: 100,
      h: 80,
    });
    expect(resizeBox(BOX, "nw", [-50, -50], SIZE)).toEqual({
      kind: "box",
      x: 0,
      y: 0,
      w: 90,
      h: 60,
    });
  });

  test("a box at the frame edge keeps its 2 × 2 minimum inside the frame", () => {
    const edge: BoxGeometry = { kind: "box", x: 0, y: 0, w: 10, h: 10 };
    const out = resizeBox(edge, "e", [0, 5], SIZE);
    expectInside(out);
    expect(out.w).toBe(2);
  });
});

describe("moving", () => {
  test("a box moves by whole pixels and stays inside the frame", () => {
    expect(moveGeometry(BOX, 3.4, -2.6, SIZE)).toEqual({ ...BOX, x: 63, y: 37 });
    expect(moveGeometry(BOX, 500, 500, SIZE)).toEqual({ ...BOX, x: 130, y: 100 });
    expect(moveGeometry(BOX, -500, -500, SIZE)).toEqual({ ...BOX, x: 0, y: 0 });
  });

  test("points of a line, a polygon and a spot stay on the sensor", () => {
    const line: RoiGeometry = { kind: "line", x0: 10, y0: 10, x1: 150, y1: 20 };
    expect(moveGeometry(line, 50, 0, SIZE)).toEqual({
      kind: "line",
      x0: 19,
      y0: 10,
      x1: 159,
      y1: 20,
    });
    const poly: RoiGeometry = {
      kind: "polygon",
      points: [
        [5, 5],
        [20, 5],
        [10, 30],
      ],
    };
    expect(moveGeometry(poly, -40, -40, SIZE)).toEqual({
      kind: "polygon",
      points: [
        [0, 0],
        [15, 0],
        [5, 25],
      ],
    });
    const spot: RoiGeometry = { kind: "spot", x: 158, y: 3, kernel: 1, show_raw: false };
    expect(moveGeometry(spot, 10, -10, SIZE)).toEqual({ ...spot, x: 159, y: 0 });
  });
});

describe("creating", () => {
  test("a box from a drag in any direction, at least 2 × 2 and clamped", () => {
    expect(geometryFromDrag("box", [10.2, 10.2], [30.4, 20.6], SIZE)).toEqual({
      kind: "box",
      x: 10,
      y: 10,
      w: 20,
      h: 11,
    });
    expect(geometryFromDrag("box", [30, 20], [10, 10], SIZE)).toEqual({
      kind: "box",
      x: 10,
      y: 10,
      w: 20,
      h: 10,
    });
    expect(geometryFromDrag("box", [150, 110], [400, 400], SIZE)).toEqual({
      kind: "box",
      x: 150,
      y: 110,
      w: 10,
      h: 10,
    });
    const thin = geometryFromDrag("box", [10, 10], [30, 10.4], SIZE);
    expect(thin).toEqual({ kind: "box", x: 10, y: 10, w: 20, h: 2 });
  });

  test("a click without a drag makes nothing; a spot needs no drag", () => {
    expect(geometryFromDrag("box", [10, 10], [10.5, 10.5], SIZE)).toBeNull();
    expect(geometryFromDrag("line", [10, 10], [11, 10], SIZE)).toBeNull();
    expect(geometryFromDrag("spot", [10.7, 10.2], [10.7, 10.2], SIZE)).toEqual({
      kind: "spot",
      x: 10,
      y: 10,
      kernel: 1,
      show_raw: false,
    });
  });

  test("lines, differences and circles", () => {
    expect(geometryFromDrag("line", [-5, 10], [200, 50], SIZE)).toEqual({
      kind: "line",
      x0: 0,
      y0: 10,
      x1: 159,
      y1: 50,
    });
    expect(geometryFromDrag("delta", [10, 10], [20, 10], SIZE)).toEqual({
      kind: "delta",
      x0: 10,
      y0: 10,
      x1: 20,
      y1: 10,
    });
    expect(geometryFromDrag("circle", [50.5, 50.5], [53.5, 54.5], SIZE)).toEqual({
      kind: "circle",
      cx: 50,
      cy: 50,
      radius: 5,
    });
  });

  test("a polygon closes on double-click or on its first point, and Esc cancels it", () => {
    let draft: [number, number][] = [];
    const click = (x: number, y: number) => {
      const r = polygonStep(draft, { type: "click", pt: [x, y] }, 3, SIZE);
      draft = r.draft;
      return r.done;
    };
    expect(click(10, 10)).toBeNull();
    expect(click(40, 10)).toBeNull();
    // The second click of a double-click lands on the same pixel and adds nothing.
    expect(click(40.3, 10.2)).toBeNull();
    expect(draft).toHaveLength(2);
    // Two points cannot close.
    expect(polygonStep(draft, { type: "dblclick" }, 3, SIZE).done).toBeNull();
    expect(click(25, 40)).toBeNull();
    expect(click(11, 11)).toEqual([
      [10, 10],
      [40, 10],
      [25, 40],
    ]);
    expect(draft).toEqual([]);

    click(10, 10);
    click(40, 10);
    click(25, 40);
    expect(polygonStep(draft, { type: "dblclick" }, 3, SIZE).done).toHaveLength(3);

    draft = [];
    click(10, 10);
    click(40, 10);
    const cancelled = polygonStep(draft, { type: "cancel" }, 3, SIZE);
    expect(cancelled).toEqual({ draft: [], done: null });
  });
});

describe("hit-testing", () => {
  const rois = [
    roi("a", { kind: "box", x: 10, y: 10, w: 40, h: 40 }),
    roi("b", { kind: "spot", x: 30, y: 30, kernel: 1, show_raw: false }),
    roi("c", { kind: "line", x0: 100, y0: 10, x1: 140, y1: 50 }),
  ];

  test("the top-most region wins, and the selected region's handles come first", () => {
    expect(hitTest(rois, [30.5, 30.5], 2, null)).toEqual({ roiId: "b", part: "body" });
    expect(hitTest(rois, [15, 15], 2, null)).toEqual({ roiId: "a", part: "body" });
    expect(hitTest(rois, [120.5, 30.5], 2, null)).toEqual({ roiId: "c", part: "body" });
    expect(hitTest(rois, [80, 100], 2, null)).toBeNull();
    expect(hitTest(rois, [50.5, 50.5], 2, "a")).toEqual({
      roiId: "a",
      part: "handle",
      handle: { kind: "box", h: "se" },
    });
    expect(hitTest(rois, [100.5, 10.5], 2, "c")).toEqual({
      roiId: "c",
      part: "handle",
      handle: { kind: "point", index: 0 },
    });
  });

  test("hidden regions cannot be hit", () => {
    const hidden = [{ ...rois[0], visible: false }];
    expect(hitTest(hidden, [15, 15], 2, null)).toBeNull();
  });

  test("resizing a line end and a circle", () => {
    const line: RoiGeometry = { kind: "line", x0: 100, y0: 10, x1: 140, y1: 50 };
    expect(resizeGeometry(line, { kind: "point", index: 1 }, [200, 60.4], SIZE)).toEqual({
      ...line,
      x1: 159,
      y1: 60,
    });
    const circle: RoiGeometry = { kind: "circle", cx: 50, cy: 50, radius: 5 };
    expect(resizeGeometry(circle, { kind: "radius" }, [60.5, 50.5], SIZE)).toEqual({
      ...circle,
      radius: 10,
    });
  });
});

test("new regions get plain numbered names", () => {
  const rois = [roi("Box 1", { kind: "box", x: 1, y: 1, w: 4, h: 4 })];
  expect(nextRegionName("box", rois)).toBe("Box 2");
  expect(nextRegionName("spot", rois)).toBe("Spot 1");
  expect(nextRegionName("delta", rois)).toBe("Difference 1");
});
