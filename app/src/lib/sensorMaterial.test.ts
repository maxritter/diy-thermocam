import { afterEach, describe, expect, test } from "bun:test";
import {
  getSensorMaterial,
  MaterialSync,
  materialFromRadParams,
  type RadLink,
  radParamsForMaterial,
  setSensorMaterial,
  subscribeSensorMaterial,
} from "./sensorMaterial";
import type { Material, RadParams } from "./types";

const FACTORY: RadParams = {
  scene_emissivity: 8192,
  t_bkg_ck: 29515,
  tau_window: 8192,
  t_window_ck: 29515,
  tau_atm: 8192,
  t_atm_ck: 29515,
  refl_window: 0,
  t_refl_ck: 29515,
  flags: 0,
};

/** A fake camera holding radiometry values the way the firmware does (flags b0 persist, b1 reset). */
function fakeCamera(start: RadParams = FACTORY) {
  let held = { ...start };
  const sets: RadParams[] = [];
  let gets = 0;
  const link: RadLink = {
    async getRadParams() {
      gets++;
      return { ...held };
    },
    async setRadParams(p) {
      sets.push({ ...p });
      held = p.flags & 2 ? { ...FACTORY } : { ...p, flags: 0 };
      return { ...held };
    },
  };
  return {
    link,
    sets,
    gets: () => gets,
    held: () => held,
  };
}

const LIVE: Material = { emissivity: 0.95, reflected_c: 22 };

afterEach(() => setSensorMaterial(null));

describe("the material switch (single correction)", () => {
  test("(e) connect with the switch off sends no SET_RAD_PARAMS", async () => {
    const cam = fakeCamera(radParamsForMaterial({ emissivity: 0.8, reflected_c: 30 }, FACTORY));
    const sync = new MaterialSync(cam.link);
    await sync.connect();
    expect(cam.sets).toEqual([]);
    expect(cam.gets()).toBe(1);
    const m = getSensorMaterial();
    expect(m?.emissivity).toBeCloseTo(0.8, 3);
    expect(m?.reflected_c).toBeCloseTo(30, 2);
  });

  test("on writes Live's material, a change rewrites, off resets, disconnect does nothing", async () => {
    const cam = fakeCamera();
    const sync = new MaterialSync(cam.link);
    await sync.connect();
    expect(cam.sets.length).toBe(0);
    expect(getSensorMaterial()).toBeNull();

    await sync.switchOn(LIVE);
    expect(cam.sets.length).toBe(1);
    expect(cam.sets[0].scene_emissivity).toBe(Math.round(0.95 * 8192));
    expect(cam.sets[0].t_bkg_ck).toBe(Math.round((22 + 273.15) * 100));
    expect(cam.sets[0].flags).toBe(1);
    expect(getSensorMaterial()?.emissivity).toBeCloseTo(0.95, 3);

    await sync.liveMaterialChanged({ emissivity: 0.8, reflected_c: 25 });
    expect(cam.sets.length).toBe(2);
    expect(cam.sets[1].scene_emissivity).toBe(Math.round(0.8 * 8192));
    expect(cam.sets[1].flags).toBe(1);

    // The same material again is not written twice.
    await sync.liveMaterialChanged({ emissivity: 0.8, reflected_c: 25 });
    expect(cam.sets.length).toBe(2);

    await sync.switchOff();
    expect(cam.sets.length).toBe(3);
    expect(cam.sets[2].flags & 2).toBe(2);
    expect(getSensorMaterial()).toBeNull();

    const before = cam.gets();
    sync.disconnect();
    await sync.liveMaterialChanged({ emissivity: 0.5, reflected_c: 10 });
    expect(cam.sets.length).toBe(3);
    expect(cam.gets()).toBe(before);
  });

  test("with the switch off a change of Live's material never writes", async () => {
    const cam = fakeCamera();
    const sync = new MaterialSync(cam.link);
    await sync.connect();
    for (const e of [0.9, 0.8, 0.7])
      await sync.liveMaterialChanged({ emissivity: e, reflected_c: 22 });
    expect(cam.sets).toEqual([]);
  });

  test("a restored switch (Studio started with it on) does not write on connect", async () => {
    const cam = fakeCamera(radParamsForMaterial({ emissivity: 0.8, reflected_c: 22 }, FACTORY));
    const sync = new MaterialSync(cam.link);
    sync.restore(true);
    await sync.connect();
    expect(cam.sets).toEqual([]);
    await sync.liveMaterialChanged({ emissivity: 0.9, reflected_c: 22 });
    expect(cam.sets.length).toBe(1);
  });

  test("a seeded sequence matches the reference model", async () => {
    type Ev =
      | { t: "connect" }
      | { t: "disconnect" }
      | { t: "on"; m: Material }
      | { t: "off" }
      | { t: "live"; m: Material };
    for (const seed of [3, 11, 2024]) {
      let a = seed;
      const r = () => {
        a = (a * 1103515245 + 12345) % 2147483648;
        return a / 2147483648;
      };
      const mat = (): Material => ({
        emissivity: [0.6, 0.8, 0.95, 1][Math.floor(r() * 4)],
        reflected_c: [10, 22, 35][Math.floor(r() * 3)],
      });
      const events: Ev[] = [];
      for (let i = 0; i < 60; i++) {
        const k = r();
        events.push(
          k < 0.12
            ? { t: "connect" }
            : k < 0.2
              ? { t: "disconnect" }
              : k < 0.35
                ? { t: "on", m: mat() }
                : k < 0.45
                  ? { t: "off" }
                  : { t: "live", m: mat() },
        );
      }

      // Reference model: which SET_RAD_PARAMS calls the camera must see.
      const expected: string[] = [];
      let connected = false;
      let on = false;
      let written: string | null = null;
      const key = (m: Material) => {
        const p = radParamsForMaterial(m, FACTORY);
        return `${p.scene_emissivity}/${p.t_bkg_ck}`;
      };
      let held = "8192/29515";
      for (const ev of events) {
        if (ev.t === "connect") {
          connected = true;
          written = held;
        } else if (ev.t === "disconnect") connected = false;
        else if (ev.t === "on") {
          on = true;
          if (connected) {
            expected.push(`set ${key(ev.m)}`);
            held = written = key(ev.m);
          }
        } else if (ev.t === "off") {
          on = false;
          if (connected) {
            expected.push("reset");
            held = written = "8192/29515";
          }
        } else if (on && connected && key(ev.m) !== written) {
          expected.push(`set ${key(ev.m)}`);
          held = written = key(ev.m);
        }
      }

      const cam = fakeCamera();
      const sync = new MaterialSync(cam.link);
      for (const ev of events) {
        if (ev.t === "connect") await sync.connect();
        else if (ev.t === "disconnect") sync.disconnect();
        else if (ev.t === "on") await sync.switchOn(ev.m);
        else if (ev.t === "off") await sync.switchOff();
        else await sync.liveMaterialChanged(ev.m);
      }
      const seen = cam.sets.map((p) =>
        p.flags & 2 ? "reset" : `set ${p.scene_emissivity}/${p.t_bkg_ck}`,
      );
      expect({ seed, seen }).toEqual({ seed, seen: expected });
    }
  });

  test("a refused write keeps the camera's values and reports it", async () => {
    const cam = fakeCamera();
    const link: RadLink = {
      getRadParams: cam.link.getRadParams,
      async setRadParams() {
        throw "device returned error 5: unsupported";
      },
    };
    const sync = new MaterialSync(link);
    await sync.connect();
    await expect(sync.switchOn(LIVE)).rejects.toBe("device returned error 5: unsupported");
    expect(getSensorMaterial()).toBeNull();
  });

  test("factory values read as null; others decode from Q3.13 and centi-kelvin", () => {
    expect(materialFromRadParams(FACTORY)).toBeNull();
    const m = materialFromRadParams({ ...FACTORY, scene_emissivity: 6554, t_bkg_ck: 30315 });
    expect(m?.emissivity).toBeCloseTo(0.8, 3);
    expect(m?.reflected_c).toBeCloseTo(30, 2);
  });

  test("subscribers hear every change", () => {
    const seen: (Material | null)[] = [];
    const off = subscribeSensorMaterial(() => seen.push(getSensorMaterial()));
    setSensorMaterial(LIVE);
    setSensorMaterial(null);
    off();
    setSensorMaterial(LIVE);
    expect(seen).toEqual([LIVE, null]);
  });
});
