import { describe, expect, test } from "bun:test";
import { canRedo, canUndo, commit, createHistory, type History, redo, undo } from "./editHistory";

/** The reference: every state in a list and a cursor; at most limit + 1 states are kept. */
class Model {
  states: number[];
  at = 0;
  constructor(
    initial: number,
    private limit: number,
  ) {
    this.states = [initial];
  }
  edit(v: number) {
    this.states = this.states.slice(0, this.at + 1);
    this.states.push(v);
    if (this.states.length > this.limit + 1) this.states.shift();
    this.at = this.states.length - 1;
  }
  undo() {
    if (this.at > 0) this.at--;
  }
  redo() {
    if (this.at < this.states.length - 1) this.at++;
  }
  get value() {
    return this.states[this.at];
  }
}

function rng(seed: number): () => number {
  let s = seed >>> 0;
  return () => {
    s = (s * 1664525 + 1013904223) >>> 0;
    return s / 2 ** 32;
  };
}

describe("edit history", () => {
  test.each([1, 2, 3, 42, 1234])(
    "seeded edits, undos and redos match the list model (seed %d)",
    (seed) => {
      const r = rng(seed);
      const limit = 100;
      let h: History<number> = createHistory(0, limit);
      const m = new Model(0, limit);
      let next = 1;
      for (let step = 0; step < 4000; step++) {
        const p = r();
        if (p < 0.5) {
          h = commit(h, next);
          m.edit(next);
          next++;
        } else if (p < 0.8) {
          h = undo(h);
          m.undo();
        } else {
          h = redo(h);
          m.redo();
        }
        expect(h.present).toBe(m.value);
        expect(canUndo(h)).toBe(m.at > 0);
        expect(canRedo(h)).toBe(m.at < m.states.length - 1);
      }
    },
  );

  test("after 101 edits the oldest step is gone", () => {
    let h = createHistory("original", 100);
    for (let i = 1; i <= 101; i++) h = commit(h, `edit ${i}`);
    let undos = 0;
    while (canUndo(h)) {
      h = undo(h);
      undos++;
    }
    expect(undos).toBe(100);
    expect(h.present).toBe("edit 1");
  });

  test("an edit after undo drops the redo steps", () => {
    let h = commit(commit(createHistory(0), 1), 2);
    h = undo(h);
    h = commit(h, 3);
    expect(canRedo(h)).toBe(false);
    expect(undo(h).present).toBe(1);
  });

  test("typing into one field is one step; another field starts a new one", () => {
    let h = createHistory("");
    for (const v of ["B", "Bo", "Boa", "Boar", "Board"]) h = commit(h, v, "notes.name");
    expect(h.past).toEqual([""]);
    h = commit(h, "Board!", "notes.text");
    expect(h.past).toEqual(["", "Board"]);
    // After an undo the same field starts a new step.
    h = undo(h);
    h = commit(h, "Boards", "notes.name");
    expect(h.past).toEqual(["", "Board"]);
    h = commit(h, "Boardsx", "notes.name");
    expect(h.past).toEqual(["", "Board"]);
  });

  test("committing the same value adds no step", () => {
    const h = createHistory({ a: 1 });
    expect(commit(h, h.present)).toBe(h);
  });
});
