import { describe, expect, test } from "bun:test";
import { clampPanelWidth, dragPanelWidth, keyPanelWidth, PANEL_WIDTH } from "./panelWidth";

describe("side panel width", () => {
  test("limits and default", () => {
    expect(PANEL_WIDTH).toEqual({ min: 280, max: 520, default: 320, step: 16 });
  });

  test("clamps to the limits and rounds", () => {
    expect(clampPanelWidth(320)).toBe(320);
    expect(clampPanelWidth(100)).toBe(280);
    expect(clampPanelWidth(279.6)).toBe(280);
    expect(clampPanelWidth(900)).toBe(520);
    expect(clampPanelWidth(400.4)).toBe(400);
    expect(clampPanelWidth(-5)).toBe(280);
  });

  test("an unreadable width is the default", () => {
    expect(clampPanelWidth(Number.NaN)).toBe(320);
    expect(clampPanelWidth(Number.POSITIVE_INFINITY)).toBe(320);
    expect(clampPanelWidth(Number.NEGATIVE_INFINITY)).toBe(320);
  });

  test("dragging the left edge left widens the panel, right narrows it", () => {
    expect(dragPanelWidth(320, 1000, 960)).toBe(360);
    expect(dragPanelWidth(320, 1000, 1020)).toBe(300);
    expect(dragPanelWidth(320, 1000, 1000)).toBe(320);
  });

  test("a drag past either limit stops at it", () => {
    expect(dragPanelWidth(320, 1000, 1200)).toBe(280);
    expect(dragPanelWidth(320, 1000, 0)).toBe(520);
    expect(dragPanelWidth(500, 1000, 900)).toBe(520);
    expect(dragPanelWidth(290, 1000, 1100)).toBe(280);
  });

  test("arrow keys step 16 px; the left arrow moves the edge left, so the panel widens", () => {
    expect(keyPanelWidth(320, "ArrowLeft")).toBe(336);
    expect(keyPanelWidth(320, "ArrowRight")).toBe(304);
    expect(keyPanelWidth(512, "ArrowLeft")).toBe(520);
    expect(keyPanelWidth(520, "ArrowLeft")).toBe(520);
    expect(keyPanelWidth(288, "ArrowRight")).toBe(280);
    expect(keyPanelWidth(280, "ArrowRight")).toBe(280);
  });

  test("Home and End go to the limits", () => {
    expect(keyPanelWidth(400, "Home")).toBe(280);
    expect(keyPanelWidth(400, "End")).toBe(520);
  });

  test("other keys are not the separator's", () => {
    expect(keyPanelWidth(320, "ArrowUp")).toBeNull();
    expect(keyPanelWidth(320, "Enter")).toBeNull();
    expect(keyPanelWidth(320, "a")).toBeNull();
  });
});
