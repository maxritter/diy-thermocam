import { describe, expect, test } from "bun:test";
import {
  defaultSettings,
  parseSettings,
  parseSettingsText,
  type StudioSettings,
  serializeSettings,
} from "./settings";

describe("settings", () => {
  test("round trip keeps every value", () => {
    const changed: StudioSettings = {
      ...defaultSettings("de-DE"),
      unit: "kelvin",
      theme: "light",
      captures_dir: "/Users/someone/Pictures/Thermocam",
      engineering_details: true,
      png_on_snapshot: true,
      button_action: "record",
      material_on_camera: true,
      drawer_height: 312,
      panel_width: 432,
      last_palette: "ironblack",
      last_range: { mode: "manual", min_c: 20.5, max_c: 55.5 },
    };
    expect(parseSettingsText(serializeSettings(changed), "de-DE")).toEqual(changed);
  });

  test("unknown keys are dropped", () => {
    const parsed = parseSettings(
      { ...defaultSettings("de-DE"), workspace: "pro", extra: 1 },
      "de-DE",
    ) as unknown as Record<string, unknown>;
    expect("workspace" in parsed).toBe(false);
    expect("extra" in parsed).toBe(false);
    expect(serializeSettings(parsed as unknown as StudioSettings)).not.toContain("workspace");
  });

  test("damaged JSON falls back to defaults", () => {
    expect(parseSettingsText("{not json", "de-DE")).toEqual(defaultSettings("de-DE"));
    expect(parseSettingsText("[1,2,3]", "de-DE")).toEqual(defaultSettings("de-DE"));
    expect(parseSettingsText("", "de-DE")).toEqual(defaultSettings("de-DE"));
  });

  test("a field with a wrong value falls back alone", () => {
    const parsed = parseSettings(
      { unit: "rankine", theme: "dark", drawer_height: "tall" },
      "de-DE",
    );
    expect(parsed.unit).toBe("celsius");
    expect(parsed.theme).toBe("dark");
    expect(parsed.drawer_height).toBe(defaultSettings("de-DE").drawer_height);
  });

  test("the side panel width starts at 320 and stays within its limits", () => {
    expect(defaultSettings("de-DE").panel_width).toBe(320);
    expect(parseSettings({ panel_width: 2000 }, "de-DE").panel_width).toBe(520);
    expect(parseSettings({ panel_width: 10 }, "de-DE").panel_width).toBe(280);
    expect(parseSettings({ panel_width: "wide" }, "de-DE").panel_width).toBe(320);
  });

  test("the first start picks the unit from the system region", () => {
    expect(defaultSettings("en-US").unit).toBe("fahrenheit");
    expect(defaultSettings("de-DE").unit).toBe("celsius");
    expect(defaultSettings("en-GB").unit).toBe("celsius");
    expect(defaultSettings("en").unit).toBe("celsius");
  });
});
