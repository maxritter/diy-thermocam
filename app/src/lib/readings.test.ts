import { describe, expect, test } from "bun:test";
import { readdirSync, readFileSync, statSync } from "node:fs";
import { join, relative } from "node:path";
import vectors from "../../crates/thermocam-core/tests/fixtures/readings.json";
import { objectCToRaw, rawToObjectC, readingC } from "./thermal";

interface Vector {
  id: string;
  case: string;
  raw: number;
  cal_offset: number;
  cal_slope: number;
  sensor_material: { emissivity: number; reflected_c: number } | null;
  want: { emissivity: number; reflected_c: number };
  expected_c: number;
  tolerance_c: number;
}

describe("the single-correction reading (shared vectors with Rust's reading_c)", () => {
  for (const v of vectors.vectors as Vector[]) {
    test(`${v.id}: ${v.case}`, () => {
      const got = readingC(
        v.raw,
        { cal_offset: v.cal_offset, cal_slope: v.cal_slope },
        v.sensor_material,
        v.want,
      );
      expect(Math.abs(got - v.expected_c)).toBeLessThanOrEqual(v.tolerance_c);
    });
  }

  test("(a) a factory sensor gives exactly the reading Studio gave before", () => {
    const cal = { cal_offset: -273.15, cal_slope: 0.01 };
    const want = { emissivity: 0.95, reflected_c: 22 };
    for (const raw of [27315, 29815, 31500, 35000]) {
      const app = raw * cal.cal_slope + cal.cal_offset;
      const tK = app + 273.15;
      const rK = want.reflected_c + 273.15;
      const before =
        ((tK ** 4 - (1 - want.emissivity) * rK ** 4) / want.emissivity) ** 0.25 - 273.15;
      expect(readingC(raw, cal, null, want)).toBe(before);
    }
  });

  test("every image-wide reading goes through it, and objectCToRaw inverts it", () => {
    const sensor = { emissivity: 0.95, reflected_c: 22 };
    const rad = {
      cal_offset: -273.15,
      cal_slope: 0.01,
      emissivity: 0.8,
      reflected_temp_c: 20,
      sensor_material: sensor,
    };
    for (const raw of [29000, 31500, 33000]) {
      expect(rawToObjectC(raw, rad)).toBe(
        readingC(raw, rad, sensor, { emissivity: 0.8, reflected_c: 20 }),
      );
      expect(Math.abs(objectCToRaw(rawToObjectC(raw, rad), rad) - raw)).toBeLessThan(0.05);
    }
    // Live's material equal to the camera's: the camera's reading passes through.
    const same = { ...rad, emissivity: 0.95, reflected_temp_c: 22 };
    expect(rawToObjectC(31500, same)).toBeCloseTo(41.85, 6);
  });
});

/** Every .ts and .tsx file under src/. */
function sourceFiles(dir: string): string[] {
  const out: string[] = [];
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) out.push(...sourceFiles(p));
    else if (/\.(ts|tsx)$/.test(name)) out.push(p);
  }
  return out;
}

test("(d) no module but thermal.ts uses the low-level compensation", () => {
  const src = join(import.meta.dir, "..");
  // Paths use "/" on every OS, so the comparison below also holds on Windows.
  const rel = (f: string) => relative(src, f).replaceAll("\\", "/");
  const here = rel(import.meta.path);
  const low = ["compensate", "EmissivityC"].join("");
  const offenders = sourceFiles(src)
    .map(rel)
    .filter((f) => f !== "lib/thermal.ts" && f !== here)
    .filter((f) => readFileSync(join(src, f), "utf8").includes(low));
  expect(offenders).toEqual([]);
});
