import { describe, expect, test } from "bun:test";
import {
  type ConnEvent,
  DEMO_PORT,
  type Effect,
  initialConnection,
  liveConnection,
  type Machine,
  step,
} from "./connection";

const CAM = "/dev/cu.usbmodem93126801";
const CAM2 = "/dev/cu.usbmodem14101";

function run(events: ConnEvent[], from: Machine = initialConnection()) {
  let m = from;
  const trace: { state: string; effects: Effect[] }[] = [];
  for (const ev of events) {
    const r = step(m, ev);
    m = r.machine;
    trace.push({ state: describe1(m), effects: r.effects });
  }
  return { m, trace };
}

function describe1(m: Machine): string {
  const s = m.state;
  return "port" in s ? `${s.name}:${s.port === DEMO_PORT ? "demo" : s.port}` : s.name;
}

const ports = (list: string[], updateMode = false): ConnEvent => ({
  type: "ports",
  ports: list,
  updateMode,
});

describe("connection: no camera → demo → real camera → unplug → back", () => {
  const events: ConnEvent[] = [
    ports([]),
    ports([]),
    { type: "demo" },
    { type: "connected", port: DEMO_PORT },
    ports([]),
    ports([CAM]),
    { type: "connected", port: CAM },
    { type: "unplugged" },
    ports([]),
    ports([]),
    ports([CAM]),
    { type: "connected", port: CAM },
  ];
  const expected: [string, Effect[]][] = [
    ["none", []],
    ["none", []],
    ["connecting:demo", [{ type: "connect", port: DEMO_PORT }]],
    ["streaming:demo", []],
    ["streaming:demo", []],
    [`connecting:${CAM}`, [{ type: "connect", port: CAM }]],
    [`streaming:${CAM}`, []],
    [`lost:${CAM}`, []],
    [`lost:${CAM}`, []],
    [`lost:${CAM}`, []],
    [`connecting:${CAM}`, [{ type: "connect", port: CAM }]],
    [`streaming:${CAM}`, []],
  ];
  const { trace } = run(events);
  expected.forEach(([state, effects], i) => {
    test(`step ${i + 1}: ${events[i].type} → ${state}`, () => {
      expect(trace[i].state).toBe(state);
      expect(trace[i].effects).toEqual(effects);
    });
  });

  test("Live's view of each step", () => {
    const { m } = run(events.slice(0, 8));
    expect(liveConnection(m)).toBe("lost");
    expect(liveConnection(run(events.slice(0, 4)).m)).toBe("demo");
    expect(liveConnection(run(events.slice(0, 1)).m)).toBe("none");
    // Reconnecting after an unplug keeps the last image with its note.
    expect(liveConnection(run(events.slice(0, 11)).m)).toBe("lost");
  });
});

describe("the demo camera never starts by itself", () => {
  const AUTO: ConnEvent[] = [
    ports([]),
    ports([CAM]),
    ports([CAM2]),
    ports([], true),
    { type: "unplugged" },
    { type: "connected", port: CAM },
    { type: "failed", port: CAM, kind: "not_found", detail: "gone" },
    { type: "failed", port: CAM, kind: "busy", detail: "busy" },
    { type: "connected", port: DEMO_PORT },
  ];

  function seeded(seed: number): () => number {
    let a = seed >>> 0;
    return () => {
      a = (a + 0x6d2b79f5) >>> 0;
      let t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  for (const seed of [3, 17, 2026, 31_337, 65_521]) {
    test(`seed ${seed}: 400 events without a person's choice never connect the demo`, () => {
      const rnd = seeded(seed);
      let m = initialConnection();
      for (let i = 0; i < 400; i++) {
        const ev = AUTO[Math.floor(rnd() * AUTO.length)];
        const r = step(m, ev);
        for (const e of r.effects) {
          expect(e.type === "connect" ? e.port : null).not.toBe(DEMO_PORT);
        }
        m = r.machine;
        expect(m.state.name === "streaming" && m.state.port === DEMO_PORT).toBe(false);
      }
    });
  }
});

describe("connection rules", () => {
  test("a camera plugged in at start connects at the first poll", () => {
    const { trace } = run([ports([CAM])]);
    expect(trace[0].effects).toEqual([{ type: "connect", port: CAM }]);
  });

  test("busy and permission show their card; the poll does not retry the same port", () => {
    for (const kind of ["busy", "permission"] as const) {
      const { m, trace } = run([
        ports([CAM]),
        { type: "failed", port: CAM, kind, detail: "x" },
        ports([CAM]),
        ports([CAM]),
      ]);
      expect(m.state).toEqual({ name: "error", port: CAM, kind, detail: "x" });
      expect(trace[2].effects).toEqual([]);
      expect(trace[3].effects).toEqual([]);
      expect(liveConnection(m)).toBe("none");
    }
  });

  test("Try again asks again; a port that leaves and comes back is tried again", () => {
    const busy = run([ports([CAM]), { type: "failed", port: CAM, kind: "busy", detail: "x" }]).m;
    expect(step(busy, { type: "retry" }).effects).toEqual([{ type: "connect", port: CAM }]);
    const back = run([ports([]), ports([CAM])], busy);
    expect(back.trace[1].effects).toEqual([{ type: "connect", port: CAM }]);
  });

  test("an error card goes away when its camera is unplugged", () => {
    const { m, trace } = run([
      ports([CAM]),
      { type: "failed", port: CAM, kind: "busy", detail: "x" },
      ports([]),
    ]);
    expect(m.state).toEqual({ name: "none" });
    expect(trace[2].effects).toEqual([]);
  });

  test("Use the demo camera from an error card connects the demo", () => {
    const busy = run([ports([CAM]), { type: "failed", port: CAM, kind: "busy", detail: "x" }]).m;
    expect(step(busy, { type: "demo" }).effects).toEqual([{ type: "connect", port: DEMO_PORT }]);
  });

  test("other failures go back to no camera and say why", () => {
    const { m, trace } = run([
      ports([CAM]),
      { type: "failed", port: CAM, kind: "protocol", detail: "no answer" },
    ]);
    expect(m.state.name).toBe("none");
    expect(trace[1].effects).toEqual([
      { type: "notify", text: "Couldn’t open the Thermocam: no answer" },
    ]);
  });

  test("a failed reconnect after an unplug keeps the last image", () => {
    const { m } = run([
      ports([CAM]),
      { type: "connected", port: CAM },
      { type: "unplugged" },
      ports([]),
      ports([CAM]),
      { type: "failed", port: CAM, kind: "not_found", detail: "gone" },
    ]);
    expect(m.state).toEqual({ name: "lost", port: CAM, kind: "camera" });
  });

  test("Disconnect stops, and the poll does not reconnect the same camera", () => {
    const { m, trace } = run([
      ports([CAM]),
      { type: "connected", port: CAM },
      { type: "disconnect" },
      ports([CAM]),
    ]);
    expect(trace[2].effects).toEqual([{ type: "disconnect" }]);
    expect(trace[3].effects).toEqual([]);
    expect(m.state.name).toBe("none");
  });

  test("a streaming camera stays when another one is plugged in", () => {
    const { trace } = run([ports([CAM]), { type: "connected", port: CAM }, ports([CAM, CAM2])]);
    expect(trace[2].effects).toEqual([]);
  });

  test("choosing a port in the chip connects it", () => {
    const { trace } = run([{ type: "choose", port: CAM2 }]);
    expect(trace[0].effects).toEqual([{ type: "connect", port: CAM2 }]);
  });

  test("the demo camera unplugged by the developer control comes back with replug", () => {
    const { m, trace } = run([
      { type: "demo" },
      { type: "connected", port: DEMO_PORT },
      { type: "unplugged" },
      ports([]),
      { type: "replug" },
    ]);
    expect(trace[2].state).toBe("lost:demo");
    expect(trace[3].effects).toEqual([]);
    expect(trace[4].effects).toEqual([{ type: "connect", port: DEMO_PORT }]);
    expect(liveConnection(m)).toBe("lost");
  });

  test("update mode shows only while no serial port answers", () => {
    expect(run([ports([], true)]).m.updateMode).toBe(true);
    expect(run([ports([CAM], true)]).m.updateMode).toBe(false);
    expect(run([ports([], true), ports([])]).m.updateMode).toBe(false);
  });

  test("events for another port are ignored", () => {
    const m = run([ports([CAM])]).m;
    expect(step(m, { type: "connected", port: CAM2 }).machine).toBe(m);
    expect(step(m, { type: "failed", port: CAM2, kind: "busy", detail: "" }).machine).toBe(m);
  });
});
