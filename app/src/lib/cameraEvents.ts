import type { ButtonAction } from "./settings";
import type { CameraEvent } from "./types";

/**
 * The camera's button during a USB session. Rust forwards every async event as
 * `camera-event`, serde externally tagged: `{"Button": {action, duration_ms}}` with action
 * 0 short, 1 long, 2 down, 3 up (`docs/protocol-v2.md`). Only a short press acts.
 */

const SHORT = 0;

export function isShortPress(ev: CameraEvent | null | undefined): boolean {
  if (!ev || typeof ev !== "object" || !("Button" in ev)) return false;
  const b = (ev as { Button: unknown }).Button;
  if (!b || typeof b !== "object") return false;
  const action = (b as { action?: unknown }).action;
  return action === SHORT || action === "short";
}

export interface ButtonHandlers {
  snapshot: () => void;
  toggleRecording: () => void;
}

/** Runs the chosen button action (Camera › The camera's button) for a short press. */
export function handleCameraEvent(
  ev: CameraEvent | null | undefined,
  action: ButtonAction,
  handlers: ButtonHandlers,
): void {
  if (!isShortPress(ev)) return;
  if (action === "snapshot") handlers.snapshot();
  else if (action === "record") handlers.toggleRecording();
}
