import { describe, expect, test } from "bun:test";
import {
  type AlarmRule,
  alarmBanner,
  alarmSentence,
  createAlarmEngine,
  reconcileRegions,
} from "./alarms";

function rule(over: Partial<AlarmRule> = {}): AlarmRule {
  return {
    id: "a1",
    enabled: true,
    target: "hottest",
    targetName: "Hottest point",
    direction: "above",
    limitC: 48.5,
    action: "notify",
    cooldownS: 5,
    ...over,
  };
}

/** A small seeded generator (mulberry32), so the sequences are the same on every run. */
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

type Phase = "below" | "above";

/** Readings that stay strictly on one side of 48.5 °C in each phase. */
function sequence(seed: number, phases: [Phase, number][]): number[] {
  const rnd = seeded(seed);
  const out: number[] = [];
  for (const [side, n] of phases) {
    for (let i = 0; i < n; i++) {
      out.push(side === "above" ? 48.6 + rnd() * 8 : 40 + rnd() * 8.4);
    }
  }
  return out;
}

function run(rules: AlarmRule[], values: number[], stepMs: number, key = "hottest") {
  const engine = createAlarmEngine();
  const fired: { id: string; at: number; value: number }[] = [];
  values.forEach((v, i) => {
    for (const f of engine.evaluate(rules, { [key]: v }, i * stepMs)) {
      fired.push({ id: f.ruleId, at: i * stepMs, value: f.valueC });
    }
  });
  return { engine, fired };
}

describe("alarms fire once per crossing and once per cooldown", () => {
  const CROSS_TWICE: [Phase, number][] = [
    ["below", 12],
    ["above", 20],
    ["below", 15],
    ["above", 9],
  ];

  for (const seed of [1, 7, 42, 1234, 99_991]) {
    test(`seed ${seed}: up, staying above, down, up again fires exactly twice`, () => {
      const values = sequence(seed, CROSS_TWICE);
      const { fired } = run([rule()], values, 1000);
      expect(fired.map((f) => f.at)).toEqual([12_000, 47_000]);
      for (const f of fired) expect(f.value).toBeGreaterThan(48.5);
    });

    test(`seed ${seed}: a second crossing inside the cooldown is suppressed`, () => {
      // 100 ms apart: the second crossing comes 3.5 s after the first, inside 5 s.
      const values = sequence(seed, CROSS_TWICE);
      const { fired } = run([rule()], values, 100);
      expect(fired.map((f) => f.at)).toEqual([1_200]);
    });

    test(`seed ${seed}: staying above after the cooldown does not fire again`, () => {
      const values = sequence(seed, [
        ["below", 3],
        ["above", 60],
      ]);
      const { fired } = run([rule({ cooldownS: 1 })], values, 1000);
      expect(fired).toHaveLength(1);
    });
  }

  test("a value already past the limit when watching starts counts as one crossing", () => {
    const { fired } = run([rule()], [50, 51, 52], 1000);
    expect(fired.map((f) => f.at)).toEqual([0]);
  });

  test("exactly at the limit is not past it", () => {
    const { fired } = run([rule()], [40, 48.5, 48.5, 40], 1000);
    expect(fired).toHaveLength(0);
  });

  test("“goes below” fires on the way down", () => {
    const r = rule({ target: "coldest", direction: "below", limitC: 10 });
    const { fired } = run([r], [12, 11, 9.5, 9, 11, 8], 10_000, "coldest");
    expect(fired.map((f) => f.at)).toEqual([20_000, 50_000]);
  });

  test("a switched-off rule never fires, and fires on the next crossing once back on", () => {
    const engine = createAlarmEngine();
    const off = rule({ enabled: false });
    expect(engine.evaluate([off], { hottest: 60 }, 0)).toEqual([]);
    expect(engine.firing().size).toBe(0);
    const on = { ...off, enabled: true };
    expect(engine.evaluate([on], { hottest: 60 }, 1000).map((f) => f.ruleId)).toEqual(["a1"]);
  });

  test("a missing reading changes nothing", () => {
    const engine = createAlarmEngine();
    const r = rule({ target: "mcu", targetName: "MCU" });
    engine.evaluate([r], { mcu: 40 }, 0);
    expect(engine.evaluate([r], {}, 1000)).toEqual([]);
    expect(engine.evaluate([r], { mcu: 50 }, 2000).map((f) => f.ruleId)).toEqual(["a1"]);
  });

  test("firing() and since() name rules past their limit now", () => {
    const engine = createAlarmEngine();
    const a = rule({ id: "a", target: "mcu" });
    const b = rule({ id: "b", target: "vrm", limitC: 42 });
    engine.evaluate([a, b], { mcu: 52.7, vrm: 40 }, 5000);
    expect([...engine.firing()]).toEqual(["a"]);
    expect(engine.since("a")).toBe(5000);
    expect(engine.since("b")).toBeNull();
    engine.evaluate([a, b], { mcu: 40, vrm: 40 }, 6000);
    expect(engine.firing().size).toBe(0);
  });

  test("each rule has its own cooldown", () => {
    const engine = createAlarmEngine();
    const a = rule({ id: "a", cooldownS: 60 });
    const b = rule({ id: "b", cooldownS: 1 });
    const fires: string[] = [];
    [50, 40, 50, 40, 50].forEach((v, i) => {
      for (const f of engine.evaluate([a, b], { hottest: v }, i * 2000)) fires.push(f.ruleId);
    });
    expect(fires).toEqual(["a", "b", "b", "b"]);
  });
});

describe("alarm words", () => {
  test("the sentence is the name", () => {
    expect(alarmSentence(rule({ targetName: "MCU" }), "celsius")).toBe("MCU above 48.5 °C");
    expect(
      alarmSentence(
        rule({ targetName: "Coldest point", direction: "below", limitC: 26.85 }),
        "kelvin",
      ),
    ).toBe("Coldest point below 300.0 K");
  });

  test("the banner says what happened and the value now", () => {
    expect(alarmBanner(rule({ targetName: "MCU" }), 52.7, "celsius")).toBe(
      "MCU is above 48.5 °C · 52.7 °C now",
    );
  });

  test("a difference region's limit is a difference", () => {
    const r = rule({ targetName: "Reg-Fin", limitC: 10, delta: true });
    expect(alarmSentence(r, "fahrenheit")).toBe("Reg-Fin above 18.0 °F");
  });
});

describe("a deleted region turns its alarm off", () => {
  test("rules on missing regions switch off and say why; the others stay", () => {
    const rules = [
      rule({ id: "h" }),
      rule({ id: "m", target: "mcu", targetName: "MCU" }),
      rule({ id: "v", target: "vrm", targetName: "VRM" }),
    ];
    const next = reconcileRegions(rules, new Set(["vrm"]));
    expect(next.map((r) => [r.id, r.enabled, r.regionGone ?? false])).toEqual([
      ["h", true, false],
      ["m", false, true],
      ["v", true, false],
    ]);
    expect(reconcileRegions(next, new Set(["vrm"]))).toBe(next);
  });
});
