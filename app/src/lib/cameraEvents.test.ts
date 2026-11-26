import { describe, expect, mock, test } from "bun:test";
import { handleCameraEvent, isShortPress } from "./cameraEvents";
import type { ButtonAction } from "./settings";
import type { CameraEvent } from "./types";

function handlers() {
  return { snapshot: mock(() => {}), toggleRecording: mock(() => {}) };
}

const SHORT_WIRE: CameraEvent = { Button: { action: 0, duration_ms: 180 } };
const SHORT_NAMED: CameraEvent = { Button: { action: "short", duration_ms: 180 } };

describe("the camera's button runs the chosen action", () => {
  const table: [ButtonAction, "snapshot" | "toggleRecording" | null][] = [
    ["snapshot", "snapshot"],
    ["record", "toggleRecording"],
    ["nothing", null],
  ];

  for (const [setting, expected] of table) {
    for (const ev of [SHORT_WIRE, SHORT_NAMED]) {
      test(`short press with "${setting}" (${JSON.stringify(ev)})`, () => {
        const h = handlers();
        handleCameraEvent(ev, setting, h);
        expect(h.snapshot).toHaveBeenCalledTimes(expected === "snapshot" ? 1 : 0);
        expect(h.toggleRecording).toHaveBeenCalledTimes(expected === "toggleRecording" ? 1 : 0);
      });
    }
  }

  const ignored: [string, CameraEvent][] = [
    ["long (wire)", { Button: { action: 1, duration_ms: 1400 } }],
    ["long (named)", { Button: { action: "long", duration_ms: 1400 } }],
    ["down", { Button: { action: 2, duration_ms: 0 } }],
    ["up", { Button: { action: 3, duration_ms: 180 } }],
    ["battery", { Battery: { bat_pct: 80, charging: false, low_warning: false, bat_mv: 4000 } }],
    ["touch", { Touch: { action: 0, x: 10, y: 10 } }],
    ["FFC", { Ffc: { state: 1, duration_ms: 300 } }],
    ["session exit", { SessionExit: { reason: 0 } }],
  ];

  for (const setting of ["snapshot", "record", "nothing"] as ButtonAction[]) {
    for (const [name, ev] of ignored) {
      test(`${name} does nothing with "${setting}"`, () => {
        const h = handlers();
        handleCameraEvent(ev, setting, h);
        expect(h.snapshot).not.toHaveBeenCalled();
        expect(h.toggleRecording).not.toHaveBeenCalled();
      });
    }
  }

  test("odd payloads are ignored", () => {
    for (const ev of [null, undefined, "Button", 3, {}, { Button: null }, { Button: {} }]) {
      expect(isShortPress(ev as unknown as CameraEvent)).toBe(false);
      const h = handlers();
      handleCameraEvent(ev as unknown as CameraEvent, "snapshot", h);
      expect(h.snapshot).not.toHaveBeenCalled();
    }
  });
});
