import { afterEach } from "bun:test";
import { GlobalRegistrator } from "@happy-dom/global-registrator";

// A lightweight DOM for component tests. Pure tests run unchanged on top of it.
GlobalRegistrator.register({ url: "http://localhost:1420/" });

// Loaded after the DOM exists. It registers its own act() environment and cleanup hooks.
const { cleanup } = await import("@testing-library/react");

afterEach(() => {
  cleanup();
  try {
    window.localStorage.clear();
  } catch {
    // Storage can be unavailable; tests then run on defaults.
  }
});
