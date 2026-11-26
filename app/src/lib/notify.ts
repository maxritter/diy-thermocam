import { isTauriRuntime } from "./backend";

/**
 * A system notification for an alarm's "Notify me" (`tauri-plugin-notification`; the web
 * Notification API in the browser preview). Permission is asked the first time it is needed.
 */

let permission: Promise<boolean> | null = null;

async function ask(): Promise<boolean> {
  if (isTauriRuntime()) {
    const n = await import("@tauri-apps/plugin-notification");
    if (await n.isPermissionGranted()) return true;
    return (await n.requestPermission()) === "granted";
  }
  if (typeof Notification === "undefined") return false;
  if (Notification.permission === "granted") return true;
  if (Notification.permission === "denied") return false;
  return (await Notification.requestPermission()) === "granted";
}

/** Asks for permission once (on first use); later calls reuse the answer. */
export function ensureNotifyPermission(): Promise<boolean> {
  permission ??= ask().catch(() => false);
  return permission;
}

export async function systemNotify(title: string, body: string): Promise<void> {
  if (!(await ensureNotifyPermission())) return;
  try {
    if (isTauriRuntime()) {
      const n = await import("@tauri-apps/plugin-notification");
      n.sendNotification({ title, body });
    } else if (typeof Notification !== "undefined") {
      new Notification(title, { body });
    }
  } catch {
    // The banner in Live still shows the alarm.
  }
}
