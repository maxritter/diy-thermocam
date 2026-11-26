import { describe, expect, test } from "bun:test";
import { act, fireEvent, waitFor, within } from "@testing-library/react";
import { App } from "@/App";
import { renderApp } from "@/test/render";
import { modifierKey } from "./shortcuts";

const PAGES = ["Live", "Captures", "Camera"] as const;

function currentPage(container: HTMLElement): string | null {
  return container.querySelector("main[data-page]")?.getAttribute("aria-label") ?? null;
}

/** Every visible word and every accessible name or tooltip in the document. */
function allWords(): string {
  const parts: string[] = [document.body.textContent ?? ""];
  for (const el of document.querySelectorAll("*")) {
    for (const attr of ["title", "aria-label", "placeholder", "alt"]) {
      const v = el.getAttribute(attr);
      if (v) parts.push(v);
    }
  }
  return parts.join(" \n ");
}

describe("the Studio shell", () => {
  test("the three tabs switch pages with clicks", async () => {
    const { container, user, findByRole, getByRole } = renderApp(<App />);
    const nav = await findByRole("navigation", { name: "Studio" });
    expect(currentPage(container)).toBe("Live");

    for (const name of ["Captures", "Camera", "Live"] as const) {
      await user.click(within(nav).getByRole("button", { name }));
      expect(currentPage(container)).toBe(name);
      expect(getByRole("button", { name }).getAttribute("aria-current")).toBe("page");
    }
  });

  test("⌘1, ⌘2 and ⌘3 switch pages", async () => {
    const { container, findByRole } = renderApp(<App />);
    await findByRole("navigation", { name: "Studio" });
    const mod = modifierKey();

    for (const [i, name] of [
      [2, "Captures"],
      [3, "Camera"],
      [1, "Live"],
    ] as const) {
      fireEvent.keyDown(window, { key: String(i), code: `Digit${i}`, [mod]: true });
      await waitFor(() => expect(currentPage(container)).toBe(name));
    }
  });

  test("Shift+P leaves the DOM unchanged", async () => {
    const { findByRole, user } = renderApp(<App />);
    await user.click(await findByRole("button", { name: "Pause" }));
    await findByRole("button", { name: "Resume" });
    await act(async () => {
      await new Promise((r) => setTimeout(r, 300));
    });

    const before = document.body.innerHTML;
    fireEvent.keyDown(window, { key: "P", code: "KeyP", shiftKey: true });
    fireEvent.keyUp(window, { key: "P", code: "KeyP", shiftKey: true });
    await act(async () => {
      await new Promise((r) => setTimeout(r, 50));
    });
    expect(document.body.innerHTML).toBe(before);
  });

  test("no element says Pro or Standard, on any page or in the menus", async () => {
    const { findByRole, user, getByRole } = renderApp(<App />);
    const nav = await findByRole("navigation", { name: "Studio" });
    for (const name of PAGES) {
      await user.click(within(nav).getByRole("button", { name }));
      expect(allWords()).not.toMatch(/\bPro\b|Standard/);
    }
    await user.click(getByRole("button", { name: "Settings" }));
    await findByRole("dialog", { name: "Settings" });
    expect(allWords()).not.toMatch(/\bPro\b|Standard/);
  });

  test("the gear menu changes the unit and remembers it", async () => {
    const { findByRole, user } = renderApp(<App />);
    await user.click(await findByRole("button", { name: "Settings" }));
    const menu = await findByRole("dialog", { name: "Settings" });
    const units = within(menu).getByRole("group", { name: "Units" });
    await user.click(within(units).getByRole("button", { name: "K" }));
    expect(within(units).getByRole("button", { name: "K" }).getAttribute("aria-pressed")).toBe(
      "true",
    );
    await waitFor(() =>
      expect(window.localStorage.getItem("thermocam.settings") ?? "").toContain('"kelvin"'),
    );
  });

  test("? opens the keyboard shortcuts", async () => {
    const { findByRole } = renderApp(<App />);
    await findByRole("navigation", { name: "Studio" });
    fireEvent.keyDown(window, { key: "?", code: "Slash", shiftKey: true });
    const sheet = await findByRole("dialog", { name: "Keyboard shortcuts" });
    expect(within(sheet).getByText("Live · Captures · Camera")).toBeTruthy();
  });

  test("Shift+/ also opens the keyboard shortcuts", async () => {
    const { findByRole } = renderApp(<App />);
    await findByRole("navigation", { name: "Studio" });
    fireEvent.keyDown(window, { key: "/", code: "Slash", shiftKey: true });
    expect(await findByRole("dialog", { name: "Keyboard shortcuts" })).toBeTruthy();
  });
});
