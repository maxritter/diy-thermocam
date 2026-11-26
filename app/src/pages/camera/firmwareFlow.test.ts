import { describe, expect, test } from "bun:test";
import type { FirmwareImageInfo } from "@/lib/types";
import {
  elapsedLabel,
  type Flow,
  type FlowEvent,
  type FlowView,
  initialFlow,
  refusedReason,
  step,
  type ViewName,
  writePercent,
} from "./firmwareFlow";

const FILE = { name: "thermocam-4.0.1.hex", text: ":02000004600098\n:00000001FF" };
const INFO: FirmwareImageInfo = {
  sha256_hex: "3f9a07c2".padEnd(64, "0"),
  total_bytes: 412_000,
  block_count: 403,
  min_addr: 0x60000000,
  max_addr: 0x60064a5f,
};

/** One representative of every state the dialog can be in. */
const VIEWS: Record<ViewName, FlowView> = {
  closed: { name: "closed" },
  checking: { name: "checking", file: FILE },
  ready: { name: "ready", file: FILE, info: INFO },
  refused: { name: "refused", file: FILE, reason: "Not firmware for a Teensy 4.1 camera" },
  waiting: { name: "waiting", file: FILE, info: INFO, since: 1000 },
  timeout: { name: "timeout", file: FILE, info: INFO },
  cancelled: { name: "cancelled", file: FILE, info: INFO },
  writing: { name: "writing", file: FILE, info: INFO, done: 169, total: 403 },
  restarting: { name: "restarting", file: FILE, info: INFO },
  done: { name: "done", reconnected: true },
  failed: { name: "failed", file: FILE, info: INFO, kind: "write", detail: "x", percent: 42 },
};

/** Every event, including those that only make sense in other states. */
const EVENTS: Record<string, FlowEvent> = {
  choose: { type: "choose", file: FILE },
  checked: { type: "checked", info: INFO },
  check_failed: { type: "check_failed", reason: "Not firmware for a Teensy 4.1 camera" },
  install: { type: "install", now: 5000 },
  retry: { type: "retry", now: 9000 },
  close: { type: "close" },
  call_started: { type: "call_started" },
  call_settled: { type: "call_settled" },
  verified: { type: "progress", ev: { stage: "verified", sha256: INFO.sha256_hex } },
  waiting_bootloader: { type: "progress", ev: { stage: "waiting_bootloader" } },
  waiting_timeout: { type: "progress", ev: { stage: "waiting_timeout" } },
  flashing: { type: "progress", ev: { stage: "flashing", done: 200, total: 403 } },
  rebooted: { type: "progress", ev: { stage: "rebooted", sha256: INFO.sha256_hex } },
  err_not_found: { type: "progress", ev: { stage: "error", kind: "not_found", detail: "nf" } },
  err_bad_file: { type: "progress", ev: { stage: "error", kind: "bad_file", detail: "bad" } },
  err_permission: {
    type: "progress",
    ev: { stage: "error", kind: "permission", detail: "denied" },
  },
  err_write: { type: "progress", ev: { stage: "error", kind: "write", detail: "hid" } },
  reconnected: { type: "reconnected" },
  reconnect_failed: { type: "reconnect_failed" },
};

type Next = ViewName | { idle: ViewName; running: ViewName };

/**
 * The transitions. Every (state, event) pair not listed keeps the state: an event in the wrong
 * state is ignored. `{idle, running}` depends on whether a flash call is still on its way.
 */
const TABLE: Record<ViewName, Partial<Record<keyof typeof EVENTS, Next>>> = {
  closed: { choose: "checking" },
  checking: { choose: "checking", checked: "ready", check_failed: "refused", close: "closed" },
  ready: { choose: "checking", install: "waiting", close: "closed" },
  refused: { choose: "checking", close: "closed" },
  waiting: {
    close: { idle: "closed", running: "cancelled" },
    waiting_timeout: "timeout",
    err_not_found: "timeout",
    flashing: "writing",
    rebooted: "restarting",
    err_bad_file: "refused",
    err_permission: "failed",
    err_write: "failed",
  },
  timeout: {
    retry: "waiting",
    close: { idle: "closed", running: "cancelled" },
    flashing: "writing",
    rebooted: "restarting",
    err_permission: "failed",
    err_write: "failed",
  },
  cancelled: {
    flashing: "writing",
    rebooted: "restarting",
    call_settled: "closed",
  },
  writing: {
    flashing: "writing",
    rebooted: "restarting",
    err_not_found: "failed",
    err_bad_file: "failed",
    err_permission: "failed",
    err_write: "failed",
  },
  restarting: { reconnected: "done", reconnect_failed: "done" },
  done: { close: "closed" },
  failed: { retry: "waiting", close: "closed" },
};

describe("the firmware update flow", () => {
  test("every state × every event follows the table; everything else is ignored", () => {
    let checked = 0;
    for (const [name, view] of Object.entries(VIEWS) as [ViewName, FlowView][]) {
      for (const running of [false, true]) {
        for (const [evName, ev] of Object.entries(EVENTS)) {
          const from: Flow = { view, running };
          const to = step(from, ev);
          const rule = TABLE[name][evName];
          const want =
            rule === undefined
              ? name
              : typeof rule === "string"
                ? rule
                : running
                  ? rule.running
                  : rule.idle;
          expect({ name, running, evName, to: to.view.name }).toEqual({
            name,
            running,
            evName,
            to: want,
          });
          if (rule === undefined && evName !== "call_started" && evName !== "call_settled") {
            expect(to.view).toBe(view);
          }
          const wantRunning =
            evName === "call_started" ? true : evName === "call_settled" ? false : running;
          expect(to.running).toBe(wantRunning);
          checked++;
        }
      }
    }
    expect(checked).toBe(Object.keys(VIEWS).length * 2 * Object.keys(EVENTS).length);
  });

  test("a whole update: choose, check, install, wait, write, restart, done", () => {
    let f = initialFlow();
    const go = (ev: FlowEvent) => {
      f = step(f, ev);
      return f.view;
    };
    expect(go({ type: "choose", file: FILE }).name).toBe("checking");
    expect(go({ type: "checked", info: INFO }).name).toBe("ready");
    expect(go({ type: "install", now: 100 })).toMatchObject({ name: "waiting", since: 100 });
    go({ type: "call_started" });
    go({ type: "progress", ev: { stage: "verified", sha256: INFO.sha256_hex } });
    go({ type: "progress", ev: { stage: "waiting_bootloader" } });
    expect(go({ type: "progress", ev: { stage: "flashing", done: 0, total: 403 } }).name).toBe(
      "writing",
    );
    const w = go({ type: "progress", ev: { stage: "flashing", done: 169, total: 403 } });
    expect(writePercent(w)).toBe(42);
    expect(go({ type: "progress", ev: { stage: "rebooted", sha256: "x" } }).name).toBe(
      "restarting",
    );
    go({ type: "call_settled" });
    expect(go({ type: "reconnected" })).toEqual({ name: "done", reconnected: true });
    expect(go({ type: "close" }).name).toBe("closed");
  });

  test("a failed write keeps where it stopped; Try again waits for update mode again", () => {
    let f: Flow = { view: VIEWS.writing, running: true };
    f = step(f, EVENTS.err_write);
    expect(f.view).toMatchObject({ name: "failed", kind: "write", percent: 42 });
    f = step(f, { type: "retry", now: 7777 });
    expect(f.view).toMatchObject({ name: "waiting", since: 7777 });
  });

  test("a permission error before writing has no percent", () => {
    const f = step({ view: VIEWS.waiting, running: true }, EVENTS.err_permission);
    expect(f.view).toMatchObject({ name: "failed", kind: "permission", percent: null });
  });

  test("Cancel while the call runs hides the dialog until the call ends", () => {
    let f = step({ view: VIEWS.waiting, running: true }, EVENTS.close);
    expect(f.view.name).toBe("cancelled");
    f = step(f, EVENTS.call_settled);
    expect(f.view.name).toBe("closed");
  });

  test("the counter reads m:ss and stops at the 10-second wait", () => {
    expect(elapsedLabel(1000, 1000)).toBe("0:00");
    expect(elapsedLabel(1000, 5400)).toBe("0:04");
    expect(elapsedLabel(1000, 30_000)).toBe("0:10");
  });

  test("a refused file says why", () => {
    expect(refusedReason("SHA-256 does not match")).toBe("Doesn’t match the expected SHA-256");
    expect(refusedReason("invalid boot header")).toBe("Not firmware for a Teensy 4.1 camera");
  });
});
