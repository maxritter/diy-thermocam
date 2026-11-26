import { describe, expect, test } from "bun:test";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import {
  DEMO_SCENES,
  type DemoScene,
  FRAMES_PER_SCENE,
  parseDatCelsius,
  sceneFrame,
} from "./demoScenes";

const dir = join(import.meta.dir, "../../public/demo");
const load = (name: string): DemoScene => ({
  name,
  celsius: parseDatCelsius(new Uint8Array(readFileSync(join(dir, `${name}.DAT`)))),
});
const scenes = DEMO_SCENES.map(load);
const celsius = (raw: number) => raw * 0.01 - 273.15;

describe("the browser preview's demo camera plays real captures", () => {
  test("the same eight captures as the app, in the same order", () => {
    expect(DEMO_SCENES).toEqual([
      "HOUSE",
      "SOLAR",
      "PCB",
      "BREAKER",
      "PRINTER",
      "PAN",
      "BEEHIVE",
      "FRIDGE",
    ]);
  });

  test("a capture's pixels read as believable temperatures", () => {
    const house = scenes[0].celsius;
    expect(house.length).toBe(160 * 120);
    for (const c of house) {
      expect(c).toBeGreaterThan(-40);
      expect(c).toBeLessThan(400);
    }
  });

  test("a frame carries its capture's temperatures with a little noise", () => {
    const f = sceneFrame(scenes, 0);
    expect([f.width, f.height]).toEqual([160, 120]);
    for (let i = 0; i < f.pixels.length; i += 97) {
      expect(Math.abs(celsius(f.pixels[i]) - scenes[0].celsius[i])).toBeLessThan(0.15);
    }
  });

  test("the next capture follows after its frames, and hottest and coldest match", () => {
    const f = sceneFrame(scenes, FRAMES_PER_SCENE);
    const i = 60 * 160 + 80;
    expect(Math.abs(celsius(f.pixels[i]) - scenes[1].celsius[i])).toBeLessThan(0.15);
    const min = Math.min(...f.pixels);
    const max = Math.max(...f.pixels);
    expect([f.min_raw, f.max_raw]).toEqual([min, max]);
    expect(f.pixels[f.min_y * 160 + f.min_x]).toBe(min);
    expect(f.pixels[f.max_y * 160 + f.max_x]).toBe(max);
  });
});
