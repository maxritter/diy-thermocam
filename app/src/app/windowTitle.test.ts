import { describe, expect, test } from "bun:test";
import { windowTitle } from "./windowTitle";

describe("window title names where you are, not the brand twice", () => {
  test("page and camera, then the app", () => {
    expect(windowTitle("Live", "Thermocam V4")).toBe("Live · Thermocam V4 — DIY-Thermocam Studio");
  });

  test("the demo camera is named like any camera", () => {
    expect(windowTitle("Captures", "Demo camera")).toBe(
      "Captures · Demo camera — DIY-Thermocam Studio",
    );
  });

  test("no camera leaves only the page", () => {
    expect(windowTitle("Camera", null)).toBe("Camera — DIY-Thermocam Studio");
    expect(windowTitle("Camera", "  ")).toBe("Camera — DIY-Thermocam Studio");
  });
});
