import { afterAll, beforeAll, describe, expect, test } from "bun:test";
import { act, fireEvent, waitFor, within } from "@testing-library/react";
import { useState } from "react";
import { ThermalViewport } from "@/components/ThermalViewport";
import { createFrameStore, type FrameStore } from "@/lib/frameStore";
import { DEFAULT_PALETTES } from "@/lib/palettes";
import { generateDemoFrame } from "@/lib/thermal";
import type { DisplayPipeline, RadiometryParams, Roi, RoiTool } from "@/lib/types";
import { renderApp } from "@/test/render";
import { Toolbar } from "./Toolbar";

/** The image is drawn 640 × 480 at the page's origin: 4 screen pixels per sensor pixel. */
const SCALE = 4;
const realRect = HTMLElement.prototype.getBoundingClientRect;
beforeAll(() => {
  HTMLElement.prototype.getBoundingClientRect = function rect() {
    return {
      x: 0,
      y: 0,
      left: 0,
      top: 0,
      width: 160 * SCALE,
      height: 120 * SCALE,
      right: 160 * SCALE,
      bottom: 120 * SCALE,
      toJSON: () => ({}),
    } as DOMRect;
  };
});
afterAll(() => {
  HTMLElement.prototype.getBoundingClientRect = realRect;
});

const RAD: RadiometryParams = {
  cal_offset: -273.15,
  cal_slope: 0.01,
  emissivity: 0.95,
  reflected_temp_c: 22,
};
const PIPELINE: DisplayPipeline = {
  filter: "none",
  upscaler: "nearest",
  sharpen_amount: 0,
  rotate_180: false,
  flip_horizontal: false,
  histogram_eq: false,
  isotherm: { mode: "off", low_c: 40, high_c: 60, color: [0, 230, 160] },
};

interface HarnessState {
  rois: Roi[];
  selected: string | null;
}

function Harness({ store, state }: { store: FrameStore; state: HarnessState }) {
  const [rois, setRois] = useState<Roi[]>([]);
  const [selected, setSelected] = useState<string | null>(null);
  const [tool, setTool] = useState<RoiTool>("select");
  state.rois = rois;
  state.selected = selected;
  return (
    <div>
      <Toolbar
        tool={tool}
        onToolChange={setTool}
        showHotCold={false}
        onToggleHotCold={() => {}}
        showLoupe={false}
        onToggleLoupe={() => {}}
        showGrid={false}
        gridSize={[4, 3]}
        onGridChange={() => {}}
      />
      <ThermalViewport
        store={store}
        palette={DEFAULT_PALETTES[0]}
        range={{ mode: "auto" }}
        rad={RAD}
        pipeline={PIPELINE}
        unit="celsius"
        rois={rois}
        selectedRoiId={selected}
        tool={tool}
        showHotCold={false}
        showGrid={false}
        gridCols={4}
        gridRows={3}
        zoom={1}
        onRoisChange={setRois}
        onSelectRoi={setSelected}
        onToolChange={setTool}
      />
    </div>
  );
}

function setup() {
  const store = createFrameStore({
    scheduler: {
      schedule: (fn) => {
        fn();
        return 0;
      },
      cancel: () => {},
    },
  });
  store.push(generateDemoFrame(3));
  const state: HarnessState = { rois: [], selected: null };

  const r = renderApp(<Harness store={store} state={state} />);
  const surface = r.container.querySelector("[data-viewport-surface]") as HTMLElement;
  expect(surface).toBeTruthy();

  /** Pointer gestures in sensor pixels. */
  const at = (x: number, y: number) => ({
    clientX: x * SCALE,
    clientY: y * SCALE,
    pointerId: 1,
    button: 0,
    buttons: 1,
  });
  const drag = (from: [number, number], to: [number, number]) => {
    fireEvent.pointerDown(surface, at(...from));
    fireEvent.pointerMove(surface, at((from[0] + to[0]) / 2, (from[1] + to[1]) / 2));
    fireEvent.pointerMove(surface, at(...to));
    fireEvent.pointerUp(surface, at(...to));
  };
  const click = (x: number, y: number) => {
    fireEvent.pointerDown(surface, at(x, y));
    fireEvent.pointerUp(surface, at(x, y));
  };
  return { ...r, state, surface, drag, click };
}

describe("editing regions on the image", () => {
  test("draw, move, resize, rename and delete a box; Esc mid-polygon adds nothing", async () => {
    const { state, drag, click, getByRole, container, user } = setup();
    const rail = getByRole("toolbar", { name: "Measuring tools" });

    // Draw a box with the Box tool.
    await user.click(within(rail).getByRole("button", { name: "Box" }));
    act(() => drag([10, 10], [30, 25]));
    expect(state.rois).toHaveLength(1);
    expect(state.rois[0].geometry).toEqual({ kind: "box", x: 10, y: 10, w: 20, h: 15 });
    expect(state.rois[0].label).toBe("Box 1");
    expect(state.selected).toBe(state.rois[0].id);
    // The tool goes back to Select once the box is drawn.
    expect(within(rail).getByRole("button", { name: "Select" }).getAttribute("aria-pressed")).toBe(
      "true",
    );

    // Drag it by its body.
    act(() => drag([20, 15], [25, 20]));
    expect(state.rois[0].geometry).toEqual({ kind: "box", x: 15, y: 15, w: 20, h: 15 });

    // Resize it from its bottom-right corner.
    act(() => drag([35, 30], [40, 35]));
    expect(state.rois[0].geometry).toEqual({ kind: "box", x: 15, y: 15, w: 25, h: 20 });

    // Rename it by double-clicking its label.
    const chip = container.querySelector("[data-region-label]") as HTMLElement;
    expect(chip.textContent).toContain("Box 1");
    await user.dblClick(within(chip).getByText("Box 1"));
    const input = within(chip).getByRole("textbox", { name: "Region name" });
    await user.clear(input);
    await user.type(input, "VRM{Enter}");
    expect(state.rois[0].label).toBe("VRM");

    // Add a spot, then select the box again and delete it: only the box goes.
    await user.click(within(rail).getByRole("button", { name: "Spot" }));
    act(() => click(100, 100));
    expect(state.rois).toHaveLength(2);
    expect(state.rois[1].geometry.kind).toBe("spot");
    act(() => click(20, 20));
    const boxId = state.rois[0].id;
    expect(state.selected).toBe(boxId);
    fireEvent.keyDown(window, { key: "Delete" });
    await waitFor(() => expect(state.rois).toHaveLength(1));
    expect(state.rois[0].geometry.kind).toBe("spot");
    expect(state.rois.some((r) => r.id === boxId)).toBe(false);
    expect(state.selected).toBeNull();

    // Start a polygon and press Esc halfway: nothing is added and the draft is gone.
    await user.click(within(rail).getByRole("button", { name: "Polygon" }));
    act(() => click(10, 60));
    act(() => click(40, 60));
    act(() => click(30, 90));
    expect(container.querySelector("[data-polygon-draft]")).toBeTruthy();
    fireEvent.keyDown(window, { key: "Escape" });
    await waitFor(() => expect(container.querySelector("[data-polygon-draft]")).toBeNull());
    expect(state.rois).toHaveLength(1);
  });

  test("a polygon closes with a double-click", async () => {
    const { state, click, surface, getByRole, user } = setup();
    const rail = getByRole("toolbar", { name: "Measuring tools" });
    await user.click(within(rail).getByRole("button", { name: "Polygon" }));
    act(() => click(10, 60));
    act(() => click(40, 60));
    act(() => click(30, 90));
    act(() => {
      fireEvent.doubleClick(surface, { clientX: 30 * SCALE, clientY: 90 * SCALE });
    });
    expect(state.rois).toHaveLength(1);
    expect(state.rois[0].geometry).toEqual({
      kind: "polygon",
      points: [
        [10, 60],
        [40, 60],
        [30, 90],
      ],
    });
  });
});
