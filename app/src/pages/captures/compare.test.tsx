import { describe, expect, test } from "bun:test";
import { fireEvent, screen } from "@testing-library/react";
import { useState } from "react";
import { renderApp } from "@/test/render";
import { CompareSplit, splitFromPointer, stepSplit } from "./CompareView";

function Harness({ start = 0.5 }: { start?: number }) {
  const [ratio, setRatio] = useState(start);
  return (
    <CompareSplit
      ratio={ratio}
      onRatioChange={setRatio}
      left={<div>left image</div>}
      right={<div>right image</div>}
      leftLabel="PCB load"
      rightLabel="25 Nov 2026 · 10:20:14"
    />
  );
}

/** happy-dom lays nothing out: the split is 400 px wide, starting 100 px from the left. */
function giveSize(el: Element) {
  el.getBoundingClientRect = () =>
    ({
      left: 100,
      top: 0,
      width: 400,
      height: 300,
      right: 500,
      bottom: 300,
      x: 100,
      y: 0,
    }) as DOMRect;
}

const rightClip = () =>
  (document.querySelector(".cp-compare-right") as HTMLElement).style.clipPath.replace(/\s+/g, " ");

describe("compare", () => {
  test("dragging the divider to 30 % shows 30 % of the left image", () => {
    renderApp(<Harness />);
    const box = document.querySelector(".cp-compare") as HTMLElement;
    giveSize(box);
    expect(rightClip()).toBe("inset(0 0 0 50%)");
    const knob = screen.getByRole("slider", { name: "Drag to move the divider" });
    fireEvent.pointerDown(knob, { clientX: 300, pointerId: 1, button: 0 });
    fireEvent.pointerMove(window, { clientX: 220, pointerId: 1 });
    fireEvent.pointerUp(window, { clientX: 220, pointerId: 1 });
    expect(rightClip()).toBe("inset(0 0 0 30%)");
    expect(knob.getAttribute("aria-valuenow")).toBe("30");
    expect((document.querySelector(".cp-split") as HTMLElement).style.left).toBe("30%");
  });

  test("the arrow keys move it by 5 %", async () => {
    const { user } = renderApp(<Harness />);
    const knob = screen.getByRole("slider", { name: "Drag to move the divider" });
    knob.focus();
    await user.keyboard("{ArrowLeft}");
    expect(rightClip()).toBe("inset(0 0 0 45%)");
    await user.keyboard("{ArrowRight}{ArrowRight}{ArrowRight}");
    expect(rightClip()).toBe("inset(0 0 0 60%)");
  });

  test("both sides are labelled", () => {
    renderApp(<Harness />);
    expect(screen.getByText("PCB load")).toBeTruthy();
    expect(screen.getByText("25 Nov 2026 · 10:20:14")).toBeTruthy();
  });

  test.each([
    [100, 0],
    [50, 0],
    [300, 0.5],
    [220, 0.3],
    [500, 1],
    [900, 1],
  ])("a pointer at x = %d is at %d of the width", (x, want) => {
    expect(splitFromPointer(x, { left: 100, width: 400 })).toBeCloseTo(want, 6);
  });

  test.each([
    [0.5, "ArrowLeft", 0.45],
    [0.5, "ArrowRight", 0.55],
    [0.02, "ArrowLeft", 0],
    [0.98, "ArrowRight", 1],
    [0.5, "Home", 0],
    [0.5, "End", 1],
    [0.5, "x", 0.5],
  ])("from %d, %s goes to %d", (from, key, to) => {
    expect(stepSplit(from, key)).toBeCloseTo(to, 6);
  });
});
