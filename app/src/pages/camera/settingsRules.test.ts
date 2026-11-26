import { describe, expect, test } from "bun:test";
import type { DeviceConfig } from "@/lib/types";
import {
  type CameraLink,
  convertManualLimits,
  GAIN_HIGH,
  GAIN_LOW,
  hasOverlay,
  LIMITS_MANUAL,
  OVERLAY_BATTERY,
  OVERLAY_COLORBAR,
  OVERLAY_SPOT,
  rawToC,
  refusalText,
  SettingsController,
  withOverlay,
} from "./settingsRules";

const BASE: DeviceConfig = {
  config_ver: 1,
  color_scheme: 10,
  text_color: 0,
  temp_format: 0,
  filter_type: 1,
  min_max_points: 3,
  rotation_vert: 0,
  rotation_horiz: 0,
  limits_mode: 0,
  shutter_mode: 1,
  lepton_gain_mode: GAIN_HIGH,
  hot_cold_mode: 0,
  hot_cold_color: 2,
  screen_off_time: 0,
  overlay_flags: 0x003f,
  manual_min_raw: 30315,
  manual_max_raw: 31315,
  hot_cold_level_c10: 350,
  min_max_preset: 0,
};

type Call =
  | { kind: "set"; config: DeviceConfig; persist: boolean }
  | { kind: "ffc"; force: boolean };

/** A fake camera: echoes what it accepts, refuses the calls it is told to. */
function fakeCamera(start: DeviceConfig) {
  const calls: Call[] = [];
  let refuseNext = false;
  let held: (() => void) | null = null;
  let holding = false;
  const link: CameraLink = {
    async setConfig(config, persist) {
      calls.push({ kind: "set", config: { ...config }, persist });
      if (holding) await new Promise<void>((r) => (held = r));
      if (refuseNext) {
        refuseNext = false;
        throw "device returned error 3: invalid argument";
      }
      return { ...config };
    },
    async triggerFfc(force) {
      calls.push({ kind: "ffc", force });
    },
  };
  return {
    link,
    calls,
    refuseNextCall: () => {
      refuseNext = true;
    },
    hold: () => {
      holding = true;
    },
    release: () => {
      holding = false;
      const r = held;
      held = null;
      r?.();
    },
    start,
  };
}

/** Deterministic PRNG (mulberry32), so a failing sequence can be replayed by its seed. */
function rng(seed: number): () => number {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

type Ev =
  | { type: "toggle"; mask: number }
  | { type: "drag"; value: number }
  | { type: "release" }
  | { type: "refuse" };

/** The reference model: what the camera must receive for each event, applied one at a time. */
function expectedCalls(start: DeviceConfig, events: Ev[]): Call[] {
  let confirmed = { ...start };
  let persisted = { ...start };
  let shown = { ...start };
  let refuse = false;
  const out: Call[] = [];
  const send = (config: DeviceConfig, persist: boolean) => {
    out.push({ kind: "set", config: { ...config }, persist });
    if (refuse) {
      refuse = false;
      shown = { ...confirmed };
      return;
    }
    confirmed = { ...config };
    if (persist) persisted = { ...config };
    shown = { ...config };
  };
  for (const ev of events) {
    if (ev.type === "refuse") refuse = true;
    else if (ev.type === "toggle") {
      const on = (shown.overlay_flags & ev.mask) !== 0;
      send(withOverlay(shown, ev.mask, !on), true);
    } else if (ev.type === "drag") {
      if (shown.hot_cold_level_c10 === ev.value) continue;
      send({ ...shown, hot_cold_level_c10: ev.value }, false);
    } else if (JSON.stringify(shown) !== JSON.stringify(persisted)) {
      send(shown, true);
    }
  }
  return out;
}

function seededEvents(seed: number): Ev[] {
  const r = rng(seed);
  const masks = [OVERLAY_SPOT, OVERLAY_COLORBAR, OVERLAY_BATTERY];
  const events: Ev[] = [];
  for (let round = 0; round < 6; round++) {
    events.push({ type: "toggle", mask: masks[Math.floor(r() * masks.length)] });
    let v = 300 + Math.floor(r() * 100);
    for (let i = 0; i < 20; i++) {
      v += Math.floor(r() * 7) - 3;
      events.push({ type: "drag", value: v });
    }
    events.push({ type: "release" });
    if (r() < 0.5) events.push({ type: "refuse" });
    events.push({ type: "toggle", mask: masks[Math.floor(r() * masks.length)] });
  }
  return events;
}

async function play(ctl: SettingsController, cam: ReturnType<typeof fakeCamera>, events: Ev[]) {
  for (const ev of events) {
    if (ev.type === "refuse") cam.refuseNextCall();
    else if (ev.type === "toggle") {
      const on = hasOverlay(ctl.getState().shown ?? BASE, ev.mask);
      await ctl.toggle(`mask-${ev.mask}`, (c) => withOverlay(c, ev.mask, !on));
    } else if (ev.type === "drag") {
      await ctl.change("level", { hot_cold_level_c10: ev.value });
    } else {
      await ctl.commit("level");
    }
  }
}

describe("camera settings rules", () => {
  test("toggle, drag ×20, release, refuse: exactly the expected setConfig calls", async () => {
    for (const seed of [1, 7, 42, 1234, 99991]) {
      const cam = fakeCamera(BASE);
      const ctl = new SettingsController(cam.link);
      ctl.load(BASE);
      const events = seededEvents(seed);
      await play(ctl, cam, events);
      expect({ seed, calls: cam.calls }).toEqual({ seed, calls: expectedCalls(BASE, events) });
    }
  });

  test("a switch persists at once; drags are RAM-only; release persists once", async () => {
    const cam = fakeCamera(BASE);
    const ctl = new SettingsController(cam.link);
    ctl.load(BASE);
    await ctl.toggle("spot", (c) => withOverlay(c, OVERLAY_SPOT, false));
    for (let i = 1; i <= 20; i++) await ctl.change("level", { hot_cold_level_c10: 350 + i });
    await ctl.commit("level");
    await ctl.commit("level");
    const persists = cam.calls.map((c) => (c.kind === "set" ? c.persist : null));
    expect(persists).toEqual([true, ...Array(20).fill(false), true]);
    expect(ctl.getState().shown?.hot_cold_level_c10).toBe(370);
  });

  test("drags while the camera is busy collapse to the newest value", async () => {
    const cam = fakeCamera(BASE);
    const ctl = new SettingsController(cam.link);
    ctl.load(BASE);
    cam.hold();
    const pending: Promise<void>[] = [];
    for (let i = 1; i <= 20; i++)
      pending.push(ctl.change("level", { hot_cold_level_c10: 350 + i }));
    pending.push(ctl.commit("level"));
    cam.release();
    await Promise.all(pending);
    const sent = cam.calls.map((c) =>
      c.kind === "set" ? [c.config.hot_cold_level_c10, c.persist] : null,
    );
    expect(sent).toEqual([
      [351, false],
      [370, true],
    ]);
  });

  test("a refused setting flips back and says why", async () => {
    const cam = fakeCamera(BASE);
    const ctl = new SettingsController(cam.link);
    ctl.load(BASE);
    cam.refuseNextCall();
    await ctl.toggle("upside", { rotation_vert: 1 });
    const st = ctl.getState();
    expect(st.shown?.rotation_vert).toBe(0);
    expect(st.errors.upside).toBe("The camera doesn’t accept this value.");
    await ctl.toggle("upside", { rotation_vert: 1 });
    expect(ctl.getState().shown?.rotation_vert).toBe(1);
    expect(ctl.getState().errors.upside).toBeUndefined();
  });

  test("the measuring range persists, then recalibrates", async () => {
    const cam = fakeCamera(BASE);
    const ctl = new SettingsController(cam.link);
    ctl.load(BASE);
    await ctl.setRange(GAIN_LOW);
    expect(cam.calls).toEqual([
      { kind: "set", config: { ...BASE, lepton_gain_mode: GAIN_LOW }, persist: true },
      { kind: "ffc", force: false },
    ]);
  });

  test("in manual limits the range switch converts the limits and forces the calibration", async () => {
    const manual = { ...BASE, limits_mode: LIMITS_MANUAL };
    const cam = fakeCamera(manual);
    const ctl = new SettingsController(cam.link);
    ctl.load(manual);
    await ctl.setRange(GAIN_LOW);
    const sent = cam.calls[0];
    if (sent.kind !== "set") throw new Error("expected setConfig first");
    expect(sent.config.lepton_gain_mode).toBe(GAIN_LOW);
    // 0.1 K per count: each limit lands within half a count of its temperature.
    expect(Math.abs(rawToC(sent.config.manual_min_raw, GAIN_LOW) - 30)).toBeLessThan(0.051);
    expect(Math.abs(rawToC(sent.config.manual_max_raw, GAIN_LOW) - 40)).toBeLessThan(0.051);
    expect([sent.config.manual_min_raw, sent.config.manual_max_raw]).toEqual([3032, 3132]);
    expect(cam.calls[1]).toEqual({ kind: "ffc", force: true });
  });

  test("a refused range switch does not recalibrate", async () => {
    const cam = fakeCamera(BASE);
    const ctl = new SettingsController(cam.link);
    ctl.load(BASE);
    cam.refuseNextCall();
    await ctl.setRange(GAIN_LOW);
    expect(cam.calls.filter((c) => c.kind === "ffc")).toEqual([]);
    expect(ctl.getState().shown?.lepton_gain_mode).toBe(GAIN_HIGH);
  });

  test("Calibrate now forces only in manual limits", async () => {
    const cam = fakeCamera(BASE);
    const ctl = new SettingsController(cam.link);
    ctl.load(BASE);
    await ctl.calibrate();
    ctl.load({ ...BASE, limits_mode: LIMITS_MANUAL });
    await ctl.calibrate();
    expect(cam.calls).toEqual([
      { kind: "ffc", force: false },
      { kind: "ffc", force: true },
    ]);
  });

  test("limits round-trip through °C in both directions", () => {
    for (let c = -10; c <= 140; c += 7.3) {
      const high = {
        ...BASE,
        limits_mode: LIMITS_MANUAL,
        manual_min_raw: Math.round((c + 273.15) / 0.01),
      };
      const low = convertManualLimits(high, GAIN_LOW);
      expect(low.lepton_gain_mode).toBe(GAIN_LOW);
      expect(Math.abs(rawToC(low.manual_min_raw, GAIN_LOW) - c)).toBeLessThan(0.051);
      const back = convertManualLimits(low, GAIN_HIGH);
      expect(Math.abs(rawToC(back.manual_min_raw, GAIN_HIGH) - c)).toBeLessThan(0.051);
    }
    // Auto limits: only the range changes.
    expect(convertManualLimits(BASE, GAIN_LOW)).toEqual({ ...BASE, lepton_gain_mode: GAIN_LOW });
  });

  test("refusal texts name the reason", () => {
    expect(refusalText("device returned error 7: forbidden")).toBe(
      "The camera doesn’t allow this right now.",
    );
    expect(refusalText({ kind: "other", detail: "not connected" })).toBe(
      "The camera isn’t connected.",
    );
    expect(refusalText(new Error("timeout waiting for response to seq 4"))).toBe(
      "The camera didn’t answer.",
    );
    expect(refusalText({ kind: "read_only", detail: "x" })).toBe(
      "The demo camera’s settings are read-only.",
    );
  });
});
