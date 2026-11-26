import type { FlashErrorKind, FlashProgress } from "@/lib/api/camera";
import type { FirmwareImageInfo } from "@/lib/types";

/** The chosen firmware file. */
export interface FlowFile {
  name: string;
  text: string;
  path?: string | null;
}

/**
 * The firmware dialog's states (`camera.html#firmware`). `cancelled` is a dialog the person
 * closed while Studio still waits for update mode: it is hidden, and it shows again if writing
 * starts after all.
 */
export type FlowView =
  | { name: "closed" }
  | { name: "checking"; file: FlowFile }
  | { name: "ready"; file: FlowFile; info: FirmwareImageInfo }
  | { name: "refused"; file: FlowFile; reason: string }
  | { name: "waiting"; file: FlowFile; info: FirmwareImageInfo; since: number }
  | { name: "timeout"; file: FlowFile; info: FirmwareImageInfo }
  | { name: "cancelled"; file: FlowFile; info: FirmwareImageInfo }
  | { name: "writing"; file: FlowFile; info: FirmwareImageInfo; done: number; total: number }
  | { name: "restarting"; file: FlowFile; info: FirmwareImageInfo }
  | { name: "done"; reconnected: boolean }
  | {
      name: "failed";
      file: FlowFile;
      info: FirmwareImageInfo;
      kind: FlashErrorKind;
      detail: string;
      /** Where writing stopped, or null when it never started. */
      percent: number | null;
    };

export type ViewName = FlowView["name"];

export interface Flow {
  view: FlowView;
  /** A `cmd_flash_firmware` call is on its way. */
  running: boolean;
}

export type FlowEvent =
  | { type: "choose"; file: FlowFile }
  | { type: "checked"; info: FirmwareImageInfo }
  | { type: "check_failed"; reason: string }
  | { type: "install"; now: number }
  | { type: "retry"; now: number }
  /** Cancel, Close, Done, the X and Escape. */
  | { type: "close" }
  | { type: "call_started" }
  | { type: "call_settled" }
  | { type: "progress"; ev: FlashProgress }
  | { type: "reconnected" }
  | { type: "reconnect_failed" };

/** How long Studio waits for update mode before it says so (Rust's `waiting_timeout`). */
export const WAIT_MS = 10_000;

export function initialFlow(): Flow {
  return { view: { name: "closed" }, running: false };
}

export function refusedReason(detail: string): string {
  return /sha|checksum|match/i.test(detail)
    ? "Doesn’t match the expected SHA-256"
    : "Not firmware for a Teensy 4.1 camera";
}

export function writePercent(view: FlowView): number {
  if (view.name !== "writing" || view.total <= 0) return 0;
  return Math.min(100, Math.round((view.done / view.total) * 100));
}

/** "0:04" for the wait counter, which stops at 0:10. */
export function elapsedLabel(since: number, now: number): string {
  const s = Math.max(0, Math.min(WAIT_MS / 1000, Math.floor((now - since) / 1000)));
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`;
}

type Attempt = { file: FlowFile; info: FirmwareImageInfo };

function attemptOf(view: FlowView): Attempt | null {
  return "info" in view ? { file: view.file, info: view.info } : null;
}

function onProgress(flow: Flow, ev: FlashProgress): FlowView {
  const view = flow.view;
  const at = attemptOf(view);
  if (!at) return view;
  const name = view.name;
  const live = name === "waiting" || name === "timeout" || name === "cancelled";

  switch (ev.stage) {
    case "verified":
    case "waiting_bootloader":
      return view;
    case "waiting_timeout":
      return name === "waiting" ? { name: "timeout", ...at } : view;
    case "flashing":
      return live || name === "writing"
        ? { name: "writing", ...at, done: ev.done, total: ev.total }
        : view;
    case "rebooted":
      return live || name === "writing" ? { name: "restarting", ...at } : view;
    case "error": {
      if (name === "writing") {
        return {
          name: "failed",
          ...at,
          kind: ev.kind,
          detail: ev.detail,
          percent: writePercent(view),
        };
      }
      if (name === "waiting") {
        if (ev.kind === "not_found") return { name: "timeout", ...at };
        if (ev.kind === "bad_file") {
          return { name: "refused", file: at.file, reason: refusedReason(ev.detail) };
        }
        return { name: "failed", ...at, kind: ev.kind, detail: ev.detail, percent: null };
      }
      if (name === "timeout" && (ev.kind === "permission" || ev.kind === "write")) {
        return { name: "failed", ...at, kind: ev.kind, detail: ev.detail, percent: null };
      }
      return view;
    }
  }
}

function nextView(flow: Flow, ev: FlowEvent): FlowView {
  const view = flow.view;
  switch (ev.type) {
    case "choose":
      return view.name === "closed" ||
        view.name === "checking" ||
        view.name === "ready" ||
        view.name === "refused"
        ? { name: "checking", file: ev.file }
        : view;
    case "checked":
      return view.name === "checking" ? { name: "ready", file: view.file, info: ev.info } : view;
    case "check_failed":
      return view.name === "checking"
        ? { name: "refused", file: view.file, reason: ev.reason }
        : view;
    case "install":
      return view.name === "ready"
        ? { name: "waiting", file: view.file, info: view.info, since: ev.now }
        : view;
    case "retry":
      return view.name === "timeout" || view.name === "failed"
        ? { name: "waiting", file: view.file, info: view.info, since: ev.now }
        : view;
    case "close":
      switch (view.name) {
        case "checking":
        case "ready":
        case "refused":
        case "done":
        case "failed":
          return { name: "closed" };
        case "waiting":
        case "timeout":
          return flow.running
            ? { name: "cancelled", file: view.file, info: view.info }
            : { name: "closed" };
        default:
          return view;
      }
    case "call_started":
      return view;
    case "call_settled":
      return view.name === "cancelled" ? { name: "closed" } : view;
    case "progress":
      return onProgress(flow, ev.ev);
    case "reconnected":
      return view.name === "restarting" ? { name: "done", reconnected: true } : view;
    case "reconnect_failed":
      return view.name === "restarting" ? { name: "done", reconnected: false } : view;
  }
}

/** One step of the flow. Events that do not apply to the current state are ignored. */
export function step(flow: Flow, ev: FlowEvent): Flow {
  const view = nextView(flow, ev);
  const running =
    ev.type === "call_started" ? true : ev.type === "call_settled" ? false : flow.running;
  if (view === flow.view && running === flow.running) return flow;
  return { view, running };
}
