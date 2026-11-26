import {
  Bell,
  CircleCheck,
  Download,
  Film,
  Info,
  type LucideIcon,
  TriangleAlert,
  Usb,
  X,
} from "lucide-react";
import { useEffect, useSyncExternalStore } from "react";
import "./toasts.css";

/**
 * Messages at the bottom of the image (`live.html#states`). Errors stay until closed,
 * progress stays until done, the rest fade after a few seconds. Each carries its next step
 * as a button.
 */

export type ToastTone = "ok" | "info" | "error" | "alarm" | "progress";
export type ToastIcon = "check" | "usb" | "warn" | "bell" | "film" | "download" | "info";

export interface ToastAction {
  label: string;
  onClick: () => void;
  /** Keep the message after the click (it is dismissed by default). */
  keepOpen?: boolean;
}

export interface ToastInput {
  tone: ToastTone;
  text: string;
  icon?: ToastIcon;
  actions?: ToastAction[];
  /** 0–1 for a progress message. */
  progress?: number;
  /** A message with the same key replaces the earlier one. */
  key?: string;
  /** Stays until its cause goes away (dismissed by key), like update mode. */
  sticky?: boolean;
}

export interface ToastItem extends ToastInput {
  id: number;
}

export interface ToastStore {
  show(t: ToastInput): number;
  update(id: number, patch: Partial<ToastInput>): void;
  dismiss(id: number): void;
  dismissKey(key: string): void;
  get(): readonly ToastItem[];
  subscribe(cb: () => void): () => void;
}

/** How long a message without an error or progress stays. */
export const FADE_MS = 4500;
const MAX_SHOWN = 4;

export function createToastStore(): ToastStore {
  let items: readonly ToastItem[] = [];
  let seq = 0;
  const subs = new Set<() => void>();
  const emit = () => {
    for (const cb of subs) cb();
  };
  return {
    show(t) {
      const id = ++seq;
      const kept = t.key ? items.filter((i) => i.key !== t.key) : items;
      items = [...kept, { ...t, id }].slice(-MAX_SHOWN);
      emit();
      return id;
    },
    update(id, patch) {
      if (!items.some((i) => i.id === id)) return;
      items = items.map((i) => (i.id === id ? { ...i, ...patch } : i));
      emit();
    },
    dismiss(id) {
      if (!items.some((i) => i.id === id)) return;
      items = items.filter((i) => i.id !== id);
      emit();
    },
    dismissKey(key) {
      if (!items.some((i) => i.key === key)) return;
      items = items.filter((i) => i.key !== key);
      emit();
    },
    get: () => items,
    subscribe(cb) {
      subs.add(cb);
      return () => subs.delete(cb);
    },
  };
}

/** The app's messages. */
export const toasts = createToastStore();

const ICONS: Record<ToastIcon, LucideIcon> = {
  check: CircleCheck,
  usb: Usb,
  warn: TriangleAlert,
  bell: Bell,
  film: Film,
  download: Download,
  info: Info,
};

const DEFAULT_ICON: Record<ToastTone, ToastIcon> = {
  ok: "check",
  info: "info",
  error: "warn",
  alarm: "bell",
  progress: "download",
};

const ICON_CLASS: Record<ToastTone, string> = {
  ok: "ok",
  info: "inf",
  error: "err",
  alarm: "am",
  progress: "inf",
};

function stays(t: ToastItem): boolean {
  return Boolean(t.sticky) || t.tone === "error" || t.tone === "alarm" || t.tone === "progress";
}

function ToastView({ t, store }: { t: ToastItem; store: ToastStore }): React.JSX.Element {
  useEffect(() => {
    if (stays(t)) return;
    const id = setTimeout(() => store.dismiss(t.id), FADE_MS);
    return () => clearTimeout(id);
  }, [t, store]);

  const Icon = ICONS[t.icon ?? DEFAULT_ICON[t.tone]];
  const urgent = t.tone === "error" || t.tone === "alarm";
  return (
    <div className="st-toast" role={urgent ? "alert" : "status"}>
      <Icon size={16} strokeWidth={1.75} className={ICON_CLASS[t.tone]} aria-hidden="true" />
      {t.progress !== undefined ? (
        <span className="tt-progress">
          {t.text}
          <span className="tt-bar">
            <i style={{ width: `${Math.round(Math.max(0, Math.min(1, t.progress)) * 100)}%` }} />
          </span>
        </span>
      ) : (
        <span className="tt-text">{t.text}</span>
      )}
      {(t.actions ?? []).map((a) => (
        <button
          key={a.label}
          className="tc-btn tc-btn-sm tc-btn-ghost"
          type="button"
          onClick={() => {
            if (!a.keepOpen) store.dismiss(t.id);
            a.onClick();
          }}
        >
          {a.label}
        </button>
      ))}
      {urgent ? (
        <button
          className="st-icon-btn tt-close"
          type="button"
          aria-label="Close message"
          onClick={() => store.dismiss(t.id)}
        >
          <X size={14} strokeWidth={1.75} aria-hidden="true" />
        </button>
      ) : null}
    </div>
  );
}

export function Toasts({ store = toasts }: { store?: ToastStore }): React.JSX.Element | null {
  const items = useSyncExternalStore(store.subscribe, store.get, store.get);
  if (items.length === 0) return null;
  return (
    <div className="st-toasts">
      {items.map((t) => (
        <ToastView key={t.id} t={t} store={store} />
      ))}
    </div>
  );
}

/** "Show in Finder" on macOS, "Show in Explorer" on Windows, "Show in folder" on Linux. */
export function revealLabel(platform: string = currentPlatform()): string {
  if (platform === "mac") return "Show in Finder";
  if (platform === "windows") return "Show in Explorer";
  return "Show in folder";
}

export type Platform = "mac" | "windows" | "linux";

export function currentPlatform(): Platform {
  const ua = typeof navigator !== "undefined" ? `${navigator.platform} ${navigator.userAgent}` : "";
  if (/Mac|iPhone|iPad/i.test(ua)) return "mac";
  if (/Win/i.test(ua)) return "windows";
  return "linux";
}
