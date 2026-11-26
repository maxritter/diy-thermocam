/** Studio's keyboard shortcuts, as listed in the shortcut sheet (live.html#menus). */

export type ShortcutId =
  | "page-live"
  | "page-captures"
  | "page-camera"
  | "help"
  | "tool-select"
  | "tool-spot"
  | "tool-box"
  | "tool-circle"
  | "tool-polygon"
  | "tool-line"
  | "tool-delta"
  | "toggle-hotcold"
  | "toggle-loupe"
  | "toggle-grid"
  | "snapshot"
  | "record"
  | "pause"
  | "export"
  | "copy-image"
  | "auto-range"
  | "toggle-panel"
  | "zoom-in"
  | "zoom-out"
  | "zoom-fit"
  | "delete-region"
  | "previous-capture"
  | "next-capture"
  | "undo"
  | "redo"
  | "cancel";

export interface KeyLike {
  key: string;
  metaKey?: boolean;
  ctrlKey?: boolean;
  shiftKey?: boolean;
  altKey?: boolean;
}

export function isMacPlatform(): boolean {
  if (typeof navigator === "undefined") return false;
  const nav = navigator as Navigator & { userAgentData?: { platform?: string } };
  const platform = nav.userAgentData?.platform ?? nav.platform ?? nav.userAgent ?? "";
  return /mac|iphone|ipad/i.test(platform);
}

/** ⌘ on macOS, Ctrl on Windows and Linux. */
export function modifierKey(mac: boolean = isMacPlatform()): "metaKey" | "ctrlKey" {
  return mac ? "metaKey" : "ctrlKey";
}

const PLAIN: Record<string, ShortcutId> = {
  v: "tool-select",
  s: "tool-spot",
  b: "tool-box",
  c: "tool-circle",
  p: "tool-polygon",
  l: "tool-line",
  d: "tool-delta",
  h: "toggle-hotcold",
  m: "toggle-loupe",
  g: "toggle-grid",
  r: "record",
  k: "pause",
  a: "auto-range",
  " ": "snapshot",
  "+": "zoom-in",
  "=": "zoom-in",
  "-": "zoom-out",
  "−": "zoom-out",
  "0": "zoom-fit",
  Backspace: "delete-region",
  Delete: "delete-region",
  ArrowLeft: "previous-capture",
  ArrowRight: "next-capture",
  Escape: "cancel",
};

const WITH_MOD: Record<string, ShortcutId> = {
  "1": "page-live",
  "2": "page-captures",
  "3": "page-camera",
  e: "export",
  c: "copy-image",
  "\\": "toggle-panel",
  z: "undo",
};

/**
 * Maps a key press to a shortcut. Letters only count without Shift, so Shift+P and other
 * Shift chords do nothing.
 */
export function matchShortcut(e: KeyLike, mac: boolean = isMacPlatform()): ShortcutId | null {
  const mod = mac ? Boolean(e.metaKey) : Boolean(e.ctrlKey);
  const otherMod = mac ? Boolean(e.ctrlKey) : Boolean(e.metaKey);
  if (e.altKey || otherMod) return null;
  const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;

  if (mod) {
    if (key === "z" && e.shiftKey) return "redo";
    if (e.shiftKey) return null;
    return WITH_MOD[key] ?? null;
  }
  // "?" is Shift+/ on US layouts; some input paths report the unshifted key.
  if (e.key === "?" || (e.key === "/" && e.shiftKey)) return "help";
  if (e.key === "+") return "zoom-in";
  if (e.shiftKey) return null;
  return PLAIN[key] ?? null;
}

/** Keys typed into a field belong to the field. */
export function isTypingTarget(target: EventTarget | null): boolean {
  if (!target || !(target instanceof Element)) return false;
  if (target instanceof HTMLElement && target.isContentEditable) return true;
  const tag = target.tagName;
  if (tag === "TEXTAREA" || tag === "SELECT") return true;
  if (tag === "INPUT") {
    const type = (target as HTMLInputElement).type;
    return !["checkbox", "radio", "button", "range", "submit", "reset"].includes(type);
  }
  return false;
}
