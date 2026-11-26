/** The side panel's width in px: its limits, the default and one keyboard step. */
export const PANEL_WIDTH = { min: 280, max: 520, default: 320, step: 16 } as const;

/** A width within the limits, in whole pixels; an unreadable one is the default. */
export function clampPanelWidth(width: number): number {
  if (!Number.isFinite(width)) return PANEL_WIDTH.default;
  return Math.min(PANEL_WIDTH.max, Math.max(PANEL_WIDTH.min, Math.round(width)));
}

/**
 * The width while the panel's left edge is dragged: the drag began at `startX` with the panel
 * `startWidth` wide, and the pointer is now at `x`. Moving left widens the panel.
 */
export function dragPanelWidth(startWidth: number, startX: number, x: number): number {
  return clampPanelWidth(startWidth + (startX - x));
}

/**
 * The width after a key on the edge: the arrows move the edge 16 px (left widens), Home and End
 * go to the limits. Null for a key the edge does not use.
 */
export function keyPanelWidth(width: number, key: string): number | null {
  switch (key) {
    case "ArrowLeft":
      return clampPanelWidth(width + PANEL_WIDTH.step);
    case "ArrowRight":
      return clampPanelWidth(width - PANEL_WIDTH.step);
    case "Home":
      return PANEL_WIDTH.min;
    case "End":
      return PANEL_WIDTH.max;
    default:
      return null;
  }
}
