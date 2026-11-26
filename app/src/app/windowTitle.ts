import { isTauriRuntime } from "@/lib/backend";

const APP_NAME = "DIY-Thermocam Studio";

/**
 * The window's own title: where you are, then the app ("document — app"). It shows in the
 * Windows and Linux title bar, the macOS Window menu and Mission Control, so it never repeats
 * the brand that Studio's header already shows.
 */
export function windowTitle(page: string, camera: string | null): string {
  const where = camera?.trim() ? `${page} · ${camera.trim()}` : page;
  return `${where} — ${APP_NAME}`;
}

/** Sets the title on the Tauri window (and the document title, which the browser preview shows). */
export async function applyWindowTitle(title: string): Promise<void> {
  if (typeof document !== "undefined") document.title = title;
  if (!isTauriRuntime()) return;
  try {
    const { getCurrentWindow } = await import("@tauri-apps/api/window");
    await getCurrentWindow().setTitle(title);
  } catch {
    // A refused or missing permission leaves the previous title; nothing to tell the person.
  }
}
