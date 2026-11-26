import { expect, test } from "bun:test";
import { App } from "@/App";
import { renderApp } from "./render";

test("the shell renders with the Live tab", async () => {
  const { findByRole } = renderApp(<App />);
  expect(await findByRole("button", { name: /^Live\b/ })).toBeTruthy();
});
