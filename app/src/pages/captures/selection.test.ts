import { describe, expect, test } from "bun:test";
import {
  clearSelection,
  emptySelection,
  retainSelection,
  type Selection,
  selectAll,
  selectClick,
} from "./Selection";

const IDS = ["a", "b", "c", "d", "e", "f", "g"];
const sel = (s: Selection) => IDS.filter((id) => s.ids.has(id));

describe("selection", () => {
  test("shift-click selects the range from the last clicked capture, either way", () => {
    let s = selectClick(emptySelection(), IDS, "b", { toggle: true, range: false });
    s = selectClick(s, IDS, "e", { toggle: false, range: true });
    expect(sel(s)).toEqual(["b", "c", "d", "e"]);
    // A second shift-click moves the range's end from the same anchor.
    s = selectClick(s, IDS, "a", { toggle: false, range: true });
    expect(sel(s)).toEqual(["a", "b"]);
  });

  test("⌘-click adds and removes one, and moves the anchor", () => {
    let s = selectClick(emptySelection(), IDS, "c", { toggle: true, range: false });
    s = selectClick(s, IDS, "f", { toggle: true, range: false });
    expect(sel(s)).toEqual(["c", "f"]);
    s = selectClick(s, IDS, "c", { toggle: true, range: false });
    expect(sel(s)).toEqual(["f"]);
    // The anchor is the capture clicked last (c), even when that click unselected it.
    s = selectClick(s, IDS, "e", { toggle: false, range: true });
    expect(sel(s)).toEqual(["c", "d", "e"]);
  });

  test("⌘-shift-click adds a range to what is selected", () => {
    let s = selectClick(emptySelection(), IDS, "a", { toggle: true, range: false });
    s = selectClick(s, IDS, "f", { toggle: true, range: false });
    s = selectClick(s, IDS, "g", { toggle: true, range: true });
    expect(sel(s)).toEqual(["a", "f", "g"]);
  });

  test("a shift-click with nothing clicked before selects just that one", () => {
    const s = selectClick(emptySelection(), IDS, "d", { toggle: false, range: true });
    expect(sel(s)).toEqual(["d"]);
  });

  test("⌘A selects all shown; clearing and a shorter list drop the rest", () => {
    const all = selectAll(IDS);
    expect(sel(all)).toEqual(IDS);
    expect(sel(retainSelection(all, ["b", "z"]))).toEqual(["b"]);
    expect(sel(clearSelection())).toEqual([]);
  });

  test("a seeded sequence of clicks matches a simple model", () => {
    let seed = 11;
    const rnd = () => {
      seed = (seed * 1103515245 + 12345) & 0x7fffffff;
      return seed / 0x7fffffff;
    };
    let s = emptySelection();
    // Model: a set and an anchor index; ranges replace the last range.
    let model = new Set<string>();
    let base = new Set<string>();
    let anchor: number | null = null;
    for (let step = 0; step < 3000; step++) {
      const i = Math.floor(rnd() * IDS.length);
      const toggle = rnd() < 0.5;
      const range = rnd() < 0.4;
      s = selectClick(s, IDS, IDS[i], { toggle, range });
      if (range && anchor !== null) {
        const [lo, hi] = anchor < i ? [anchor, i] : [i, anchor];
        model = new Set(toggle ? base : []);
        for (let k = lo; k <= hi; k++) model.add(IDS[k]);
      } else {
        if (model.has(IDS[i])) model.delete(IDS[i]);
        else model.add(IDS[i]);
        anchor = i;
        base = new Set(model);
      }
      expect(sel(s)).toEqual(IDS.filter((id) => model.has(id)));
    }
  });
});
