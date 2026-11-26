import { describe, expect, test } from "bun:test";
import {
  captureLabel,
  cardText,
  dayHeading,
  formatTaken,
  groupByDay,
  parseCaptureName,
} from "./names";

const L = "en-GB";
const pad = (n: number, w = 2) => String(n).padStart(w, "0");

/** A small seeded generator, so the sweep is the same on every run. */
function rng(seed: number): () => number {
  let s = seed >>> 0;
  return () => {
    s = (s * 1664525 + 1013904223) >>> 0;
    return s / 2 ** 32;
  };
}

function daysIn(y: number, m: number): number {
  return new Date(y, m, 0).getDate();
}

describe("parseCaptureName", () => {
  test("every valid date-time name parses to its own time, as a file and as a folder", () => {
    const r = rng(7);
    for (let i = 0; i < 2000; i++) {
      const y = 2000 + Math.floor(r() * 60);
      const mo = 1 + Math.floor(r() * 12);
      const d = 1 + Math.floor(r() * daysIn(y, mo));
      const h = Math.floor(r() * 24);
      const mi = Math.floor(r() * 60);
      const s = Math.floor(r() * 60);
      const stem = `${y}${pad(mo)}${pad(d)}${pad(h)}${pad(mi)}${pad(s)}`;
      for (const name of [`${stem}.DAT`, `${stem}.dat`, stem]) {
        const p = parseCaptureName(name);
        expect(p.taken).not.toBeNull();
        const t = p.taken as Date;
        expect([t.getFullYear(), t.getMonth() + 1, t.getDate()]).toEqual([y, mo, d]);
        expect([t.getHours(), t.getMinutes(), t.getSeconds()]).toEqual([h, mi, s]);
        expect(p.suffix).toBeNull();
      }
    }
  });

  test("a -N suffix keeps the time and is reported", () => {
    for (const n of [2, 3, 9, 10, 123]) {
      const p = parseCaptureName(`20261125102014-${n}.DAT`);
      expect(p.taken?.getSeconds()).toBe(14);
      expect(p.suffix).toBe(n);
    }
    expect(parseCaptureName("20261126101500-2").suffix).toBe(2);
  });

  test.each([
    ["month 0", "20260025102014.DAT"],
    ["month 13", "20261325102014.DAT"],
    ["day 0", "20261100102014.DAT"],
    ["day 32", "20261132102014.DAT"],
    ["30 February", "20260230102014.DAT"],
    ["29 February in a common year", "20270229102014.DAT"],
    ["31 April", "20260431102014.DAT"],
    ["hour 24", "20261125242014.DAT"],
    ["minute 60", "20261125106014.DAT"],
    ["second 60", "20261125102060.DAT"],
  ])("an impossible date (%s) is kept as is", (_why, name) => {
    expect(parseCaptureName(name).taken).toBeNull();
    expect(captureLabel(name, L)).toBe(name);
  });

  test("29 February in a leap year is a real date", () => {
    expect(parseCaptureName("20280229000000.DAT").taken?.getDate()).toBe(29);
  });

  test.each([
    "20261125_101500_PCB_LOAD.DAT",
    "IMG_0001.DAT",
    "2026112510201.DAT",
    "202611251020145.DAT",
    "20261125102014.BMP",
    "20261125102014-.DAT",
    "20261125102014-x.DAT",
    "x20261125102014.DAT",
    "notes.txt",
    "DCIM",
    "",
  ])("an odd name (%p) is kept as is", (name) => {
    expect(parseCaptureName(name).taken).toBeNull();
    expect(captureLabel(name, L)).toBe(name);
  });
});

describe("labels", () => {
  test("a date-time name reads as day, month name, year and time", () => {
    expect(captureLabel("20261125102014.DAT", L)).toBe("25 Nov 2026 · 10:20:14");
    expect(captureLabel("20261126101500", L)).toBe("26 Nov 2026 · 10:15:00");
    expect(captureLabel("20261125102014-2.DAT", L)).toBe("25 Nov 2026 · 10:20:14 (2)");
  });

  test("the month name comes from the locale", () => {
    for (const locale of ["en-GB", "en-US", "de-DE", "fr-FR"]) {
      const fmt = new Intl.DateTimeFormat(locale, { month: "short" });
      for (let m = 1; m <= 12; m++) {
        const d = new Date(2026, m - 1, 5, 7, 8, 9);
        const month = fmt.format(d).replace(/\.$/, "");
        expect(formatTaken(d, locale)).toBe(`5 ${month} 2026 · 07:08:09`);
      }
    }
    expect(formatTaken(new Date(2026, 10, 25, 10, 20, 14), "en-GB")).toBe("25 Nov 2026 · 10:20:14");
    expect(formatTaken(new Date(2026, 10, 25, 10, 20, 14), "de-DE")).toBe("25 Nov 2026 · 10:20:14");
  });

  test("a typed name wins and the date and time move to the line below", () => {
    expect(cardText("20261125101500.DAT", "PCB load", "38 KB", L)).toEqual({
      title: "PCB load",
      meta: "25 Nov 2026 · 10:15:00 · 38 KB",
    });
    expect(cardText("20261125102014.DAT", "", "38 KB", L)).toEqual({
      title: "25 Nov 2026 · 10:20:14",
      meta: "38 KB",
    });
    expect(cardText("20261125102014.DAT", "   ", "38 KB", L).title).toBe("25 Nov 2026 · 10:20:14");
    expect(cardText("IMG_1.DAT", "Radiator", "38 KB", L)).toEqual({
      title: "Radiator",
      meta: "IMG_1.DAT · 38 KB",
    });
  });
});

describe("days", () => {
  const now = new Date(2026, 10, 25, 15, 0, 0); // Wednesday, 25 November 2026

  test.each([
    [new Date(2026, 10, 25, 0, 0, 1), "Today", "Wednesday, 25 November"],
    [new Date(2026, 10, 24, 23, 59, 59), "Yesterday", "Tuesday, 24 November"],
    [new Date(2026, 10, 23, 18, 2, 33), "Monday", "23 November"],
    [new Date(2026, 10, 19, 9, 0, 0), "Thursday", "19 November"],
    [new Date(2026, 10, 18, 9, 0, 0), "18 November 2026", "Wednesday"],
    [new Date(2025, 10, 25, 9, 0, 0), "25 November 2025", "Tuesday"],
  ])("%s heads its day", (d, title, sub) => {
    expect(dayHeading(d, now, L)).toEqual({ title, sub });
  });

  test("groups keep the order they are given, with No date and Other files last", () => {
    const items = [
      { id: "a", taken: new Date(2026, 10, 25, 10, 41, 7), other: false },
      { id: "b", taken: null, other: false },
      { id: "c", taken: new Date(2026, 10, 23, 18, 2, 33), other: false },
      { id: "d", taken: new Date(2026, 10, 25, 10, 15, 0), other: false },
      { id: "e", taken: null, other: true },
    ];
    const groups = groupByDay(items, now, L);
    expect(groups.map((g) => [g.title, g.items.map((i) => i.id)])).toEqual([
      ["Today", ["a", "d"]],
      ["Monday", ["c"]],
      ["No date", ["b"]],
      ["Other files", ["e"]],
    ]);
  });
});
