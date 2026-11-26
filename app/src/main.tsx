import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { App } from "./App";
import { readStoredSettings, resolveTheme } from "./lib/settings";
import "./index.css";

// Paint the chosen theme before React mounts, from the local copy of the settings.
document.documentElement.setAttribute("data-theme", resolveTheme(readStoredSettings().theme));

const rootEl = document.getElementById("root");
if (rootEl) {
  createRoot(rootEl).render(
    <StrictMode>
      <App />
    </StrictMode>,
  );
}
