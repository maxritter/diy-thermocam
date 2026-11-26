import { type RenderOptions, type RenderResult, render } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import type { ReactElement } from "react";

/**
 * Renders a component the way Studio runs in the browser preview: no Tauri runtime, so every
 * `backend` call takes its browser fallback (the demo camera, settings in localStorage).
 */
export function renderApp(
  ui: ReactElement,
  options?: Omit<RenderOptions, "queries">,
): RenderResult & { user: ReturnType<typeof userEvent.setup> } {
  if ("__TAURI_INTERNALS__" in window) {
    delete (window as unknown as Record<string, unknown>).__TAURI_INTERNALS__;
  }
  const user = userEvent.setup();
  return { user, ...render(ui, options) };
}
