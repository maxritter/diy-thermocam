import type { TempUnit } from "./types";

/** Studio's own settings. Rust stores them in `app_config_dir()/settings.json`; the browser
 * preview, and the app until that command exists, keeps the same keys in localStorage. */
export interface StudioSettings {
  unit: TempUnit;
  theme: ThemeChoice;
  /** Absolute path, or null for the default (Pictures › Thermocam). */
  captures_dir: string | null;
  engineering_details: boolean;
  png_on_snapshot: boolean;
  button_action: ButtonAction;
  material_on_camera: boolean;
  drawer_height: number;
  last_palette: string;
  last_range: ColorRangeSetting;
  /** The firmware file chosen last, so Finish update can use it again. */
  last_firmware_path: string | null;
}

export type ThemeChoice = "system" | "light" | "dark";
export type ButtonAction = "snapshot" | "record" | "nothing";
export interface ColorRangeSetting {
  mode: "auto" | "manual";
  min_c: number | null;
  max_c: number | null;
}

export const SETTINGS_STORAGE_KEY = "thermocam.settings";

/** Regions that read temperatures in °F. */
const FAHRENHEIT_REGIONS = new Set(["US"]);

function systemLocale(): string {
  if (typeof navigator !== "undefined" && navigator.language) return navigator.language;
  return Intl.DateTimeFormat().resolvedOptions().locale;
}

export function unitForLocale(locale: string): TempUnit {
  try {
    const region = new Intl.Locale(locale).region;
    return region && FAHRENHEIT_REGIONS.has(region) ? "fahrenheit" : "celsius";
  } catch {
    return "celsius";
  }
}

export function defaultSettings(locale: string = systemLocale()): StudioSettings {
  return {
    unit: unitForLocale(locale),
    theme: "system",
    captures_dir: null,
    engineering_details: false,
    png_on_snapshot: false,
    button_action: "snapshot",
    material_on_camera: false,
    drawer_height: 216,
    last_palette: "inferno",
    last_range: { mode: "auto", min_c: null, max_c: null },
    last_firmware_path: null,
  };
}

function oneOf<T extends string>(value: unknown, allowed: readonly T[], fallback: T): T {
  return typeof value === "string" && (allowed as readonly string[]).includes(value)
    ? (value as T)
    : fallback;
}

function bool(value: unknown, fallback: boolean): boolean {
  return typeof value === "boolean" ? value : fallback;
}

function finiteOrNull(value: unknown): number | null {
  return typeof value === "number" && Number.isFinite(value) ? value : null;
}

function parseRange(value: unknown, fallback: ColorRangeSetting): ColorRangeSetting {
  if (!value || typeof value !== "object") return { ...fallback };
  const v = value as Record<string, unknown>;
  return {
    mode: oneOf(v.mode, ["auto", "manual"] as const, fallback.mode),
    min_c: finiteOrNull(v.min_c),
    max_c: finiteOrNull(v.max_c),
  };
}

/** Keeps only known keys; a missing or wrong value falls back to its default. */
export function parseSettings(value: unknown, locale?: string): StudioSettings {
  const d = defaultSettings(locale);
  if (!value || typeof value !== "object" || Array.isArray(value)) return d;
  const v = value as Record<string, unknown>;
  const height = v.drawer_height;
  return {
    unit: oneOf(v.unit, ["celsius", "fahrenheit", "kelvin"] as const, d.unit),
    theme: oneOf(v.theme, ["system", "light", "dark"] as const, d.theme),
    captures_dir:
      typeof v.captures_dir === "string" && v.captures_dir.length > 0 ? v.captures_dir : null,
    engineering_details: bool(v.engineering_details, d.engineering_details),
    png_on_snapshot: bool(v.png_on_snapshot, d.png_on_snapshot),
    button_action: oneOf(
      v.button_action,
      ["snapshot", "record", "nothing"] as const,
      d.button_action,
    ),
    material_on_camera: bool(v.material_on_camera, d.material_on_camera),
    drawer_height:
      typeof height === "number" && Number.isFinite(height) && height >= 0
        ? Math.round(height)
        : d.drawer_height,
    last_palette:
      typeof v.last_palette === "string" && v.last_palette.length > 0
        ? v.last_palette
        : d.last_palette,
    last_range: parseRange(v.last_range, d.last_range),
    last_firmware_path:
      typeof v.last_firmware_path === "string" && v.last_firmware_path.length > 0
        ? v.last_firmware_path
        : null,
  };
}

export function parseSettingsText(
  text: string | null | undefined,
  locale?: string,
): StudioSettings {
  if (!text) return defaultSettings(locale);
  try {
    return parseSettings(JSON.parse(text), locale);
  } catch {
    return defaultSettings(locale);
  }
}

export function serializeSettings(settings: StudioSettings): string {
  return JSON.stringify(parseSettings(settings));
}

export function readStoredSettings(): StudioSettings {
  try {
    return parseSettingsText(window.localStorage.getItem(SETTINGS_STORAGE_KEY));
  } catch {
    return defaultSettings();
  }
}

export function writeStoredSettings(settings: StudioSettings): void {
  try {
    window.localStorage.setItem(SETTINGS_STORAGE_KEY, serializeSettings(settings));
  } catch {
    // Storage can be unavailable (private window); settings then last for this session.
  }
}

/** The theme to paint: "system" follows the operating system. */
export function resolveTheme(choice: ThemeChoice): "light" | "dark" {
  if (choice !== "system") return choice;
  if (typeof window !== "undefined" && typeof window.matchMedia === "function") {
    return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
  }
  return "dark";
}

/** "Pictures › Thermocam" for a folder path: its last two names. */
export function folderLabel(path: string | null): string {
  if (!path) return "Pictures › Thermocam";
  const parts = path.split(/[\\/]+/).filter(Boolean);
  return parts.slice(-2).join(" › ") || path;
}
