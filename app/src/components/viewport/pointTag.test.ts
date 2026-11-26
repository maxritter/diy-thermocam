import { describe, expect, test } from "bun:test";
import { pointTagPlacement } from "./RegionLayer";

const W = 160;
const H = 120;

describe("the hottest and coldest labels stay inside the image", () => {
  test("in the middle the label sits centred above its mark", () => {
    expect(pointTagPlacement(80, 60, W, H)).toBe("pt");
  });

  test("at the left edge the label starts at its mark and runs inwards", () => {
    expect(pointTagPlacement(0.5, 60, W, H)).toBe("pt edge-l");
  });

  test("at the right edge the label ends at its mark", () => {
    expect(pointTagPlacement(159.5, 60, W, H)).toBe("pt edge-r");
  });

  test("at the top the label goes below its mark", () => {
    expect(pointTagPlacement(80, 0.5, W, H)).toBe("pt edge-t");
    expect(pointTagPlacement(0.5, 0.5, W, H)).toBe("pt edge-l edge-t");
  });
});
