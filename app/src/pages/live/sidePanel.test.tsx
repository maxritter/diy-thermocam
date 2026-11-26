import { describe, expect, mock, test } from "bun:test";
import { fireEvent, within } from "@testing-library/react";
import { useState } from "react";
import { PanelWidthContext, SidePanel } from "@/app/ui/SidePanel";
import type { ColorRange } from "@/lib/colorRange";
import { createFrameStore, type FrameStore } from "@/lib/frameStore";
import { DEFAULT_MATERIALS } from "@/lib/palettes";
import { generateDemoFrame } from "@/lib/thermal";
import type { DisplayPipeline, Palette, RadiometryParams } from "@/lib/types";
import { NotesTab } from "@/pages/captures/NotesTab";
import { renderApp } from "@/test/render";
import { AlarmsTab } from "./AlarmsTab";
import { ImageTab } from "./ImageTab";
import { MeasureTab } from "./MeasureTab";

function immediateStore(): FrameStore {
  const store = createFrameStore({
    scheduler: {
      schedule: (fn) => {
        fn();
        return 0;
      },
      cancel: () => {},
    },
  });
  store.push(generateDemoFrame(5));
  return store;
}

const RAD: RadiometryParams = {
  cal_offset: -273.15,
  cal_slope: 0.01,
  emissivity: 0.95,
  reflected_temp_c: 22,
};

const PALETTES: Palette[] = [
  {
    id: "inferno",
    label: "Inferno",
    category: "Perceptual",
    firmware_id: null,
    rgb: [
      [0, 0, 0],
      [255, 255, 255],
    ],
  },
];

const PIPELINE: DisplayPipeline = {
  filter: "none",
  upscaler: "edge_preserve",
  sharpen_amount: 0.45,
  rotate_180: false,
  flip_horizontal: false,
  histogram_eq: false,
  isotherm: { mode: "off", low_c: 40, high_c: 60, color: [0, 230, 160] },
};

interface Seen {
  range: ColorRange;
  pipeline: DisplayPipeline;
}

function ImageHarness({ store, seen }: { store: FrameStore; seen: Seen }) {
  const [range, setRange] = useState<ColorRange>({ mode: "manual", lo: 30000, hi: 31000 });
  const [pipeline, setPipeline] = useState<DisplayPipeline>(PIPELINE);
  seen.range = range;
  seen.pipeline = pipeline;
  return (
    <ImageTab
      store={store}
      palettes={PALETTES}
      paletteId="inferno"
      recentPaletteIds={["inferno"]}
      onSelectPalette={() => {}}
      onImportPalette={async () => {}}
      range={range}
      onRangeChange={setRange}
      rad={RAD}
      unit="celsius"
      pipeline={pipeline}
      onPipelineChange={setPipeline}
    />
  );
}

function setupImage() {
  const store = immediateStore();
  const seen = {} as Seen;
  const r = renderApp(<ImageHarness store={store} seen={seen} />);
  return { ...r, seen, store };
}

/** Every group heading in `root` and whether an icon sits beside its words. */
function headings(root: HTMLElement): { title: string; icon: boolean }[] {
  return Array.from(root.querySelectorAll(".st-grp-h")).map((h) => ({
    title: h.querySelector("h3")?.textContent ?? "",
    icon: h.querySelector("h3 svg, .st-grp-ic svg") !== null,
  }));
}

describe("the color scale", () => {
  test("Auto returns a fixed range to automatic; Fixed keeps what the scale shows", async () => {
    const { seen, user, getByRole, store } = setupImage();
    const scale = getByRole("group", { name: "Color scale" });
    const auto = within(scale).getByRole("button", { name: "Auto" });
    const fixed = within(scale).getByRole("button", { name: "Fixed" });
    expect(fixed.getAttribute("aria-pressed")).toBe("true");
    expect(auto.getAttribute("aria-pressed")).toBe("false");

    await user.click(auto);
    expect(seen.range).toEqual({ mode: "auto" });
    expect(auto.getAttribute("aria-pressed")).toBe("true");

    await user.click(fixed);
    const frame = store.getFrame();
    expect(seen.range).toEqual({
      mode: "manual",
      lo: frame?.min_raw ?? -1,
      hi: frame?.max_raw ?? -1,
    });
    expect(fixed.getAttribute("aria-pressed")).toBe("true");
  });
});

describe("Image options", () => {
  test("open from the start, with the four upscalers by name", async () => {
    const { seen, user, getByRole } = setupImage();
    const upscaler = getByRole("combobox", { name: "Upscaler" }) as HTMLSelectElement;
    expect(upscaler.checkVisibility?.() ?? true).toBe(true);
    const names = Array.from(upscaler.options).map((o) => o.textContent);
    expect(names).toEqual(["Nearest", "Bicubic", "Lanczos-3", "Edge + USM"]);

    await user.selectOptions(upscaler, "Bicubic");
    expect(seen.pipeline.upscaler).toBe("bicubic");
    expect(upscaler.value).toBe("bicubic");
  });

  test("denoise offers the Box filter and keeps it", async () => {
    const { seen, user, getByRole } = setupImage();
    const denoise = getByRole("combobox", { name: "Denoise" }) as HTMLSelectElement;
    expect(Array.from(denoise.options).map((o) => o.textContent)).toEqual([
      "Off",
      "Gaussian 3×3",
      "Median 3×3",
      "Box 3×3",
    ]);
    await user.selectOptions(denoise, "Box 3×3");
    expect(seen.pipeline.filter).toBe("box3x3");
    expect(denoise.value).toBe("box3x3");
  });

  test("sharpening, boost contrast and rotate stay", async () => {
    const { seen, user, getByRole } = setupImage();
    const sharpen = getByRole("slider", { name: "Sharpening" }) as HTMLInputElement;
    fireEvent.change(sharpen, { target: { value: "80" } });
    expect(seen.pipeline.sharpen_amount).toBeCloseTo(0.8);
    await user.click(getByRole("switch", { name: "Boost contrast" }));
    expect(seen.pipeline.histogram_eq).toBe(true);
    await user.click(getByRole("switch", { name: "Rotate 180°" }));
    expect(seen.pipeline.rotate_180).toBe(true);
    await user.click(getByRole("switch", { name: "Mirror" }));
    expect(seen.pipeline.flip_horizontal).toBe(true);
  });
});

describe("group headings", () => {
  test("every heading in Image, Measure, Alarms and Notes has an icon", () => {
    const store = immediateStore();
    const image = setupImage().container;
    const measure = renderApp(
      <MeasureTab
        store={store}
        rad={RAD}
        materialId={null}
        materials={DEFAULT_MATERIALS}
        unit="celsius"
        onMaterialChange={() => {}}
        rois={[]}
        selectedRoiId={null}
        onSelectRoi={() => {}}
        onRoisChange={() => {}}
        onFitRange={() => {}}
        chartsOpen={false}
        onChartsOpenChange={() => {}}
      />,
    ).container;
    const alarms = renderApp(
      <AlarmsTab
        rules={[]}
        onRulesChange={() => {}}
        firing={new Set()}
        since={() => null}
        log={[]}
        onClearLog={() => {}}
        rois={[]}
        unit="celsius"
      />,
    ).container;
    const notes = renderApp(
      <NotesTab
        name=""
        text=""
        placeholder="2026-11-26 10:00"
        onName={() => {}}
        onText={() => {}}
        save={{ kind: "original" }}
        canUndo={false}
        canRedo={false}
        onUndo={() => {}}
        onRedo={() => {}}
        onReset={() => {}}
        details={[{ k: "Size", v: "160 × 120" }]}
      />,
    ).container;

    const all = [image, measure, alarms, notes].flatMap(headings);
    expect(all.map((h) => h.title)).toEqual([
      "Palette",
      "Color scale",
      "Highlight",
      "Image options",
      "Whole image",
      "Material",
      "Regions",
      "Chart",
      "Alarms",
      "Recent",
      "Notes",
      "Details",
    ]);
    expect(all.filter((h) => !h.icon)).toEqual([]);
  });
});

describe("the side panel's edge", () => {
  function setupPanel(width = 320) {
    const onChange = mock((_w: number) => {});
    const r = renderApp(
      <PanelWidthContext.Provider value={{ width, onChange }}>
        <SidePanel tabs={<div />}>
          <p>Body</p>
        </SidePanel>
      </PanelWidthContext.Provider>,
    );
    const edge = r.getByRole("separator", { name: "Resize side panel" });
    return { ...r, onChange, edge };
  }

  test("is a vertical separator that reports the width and its limits", () => {
    const { edge, getByRole } = setupPanel(400);
    expect(edge.getAttribute("aria-orientation")).toBe("vertical");
    expect(edge.getAttribute("aria-valuenow")).toBe("400");
    expect(edge.getAttribute("aria-valuemin")).toBe("280");
    expect(edge.getAttribute("aria-valuemax")).toBe("520");
    expect(edge.tabIndex).toBe(0);
    expect((getByRole("complementary") as HTMLElement).style.width).toBe("400px");
  });

  test("arrow keys step 16 px and double-click resets to 320", () => {
    const { edge, onChange } = setupPanel(400);
    fireEvent.keyDown(edge, { key: "ArrowLeft" });
    expect(onChange).toHaveBeenLastCalledWith(416);
    fireEvent.keyDown(edge, { key: "ArrowRight" });
    expect(onChange).toHaveBeenLastCalledWith(384);
    fireEvent.keyDown(edge, { key: "End" });
    expect(onChange).toHaveBeenLastCalledWith(520);
    fireEvent.doubleClick(edge);
    expect(onChange).toHaveBeenLastCalledWith(320);
  });

  test("a drag past the limit stops at it and is saved once, on release", () => {
    const { edge, onChange, getByRole } = setupPanel(320);
    fireEvent.pointerDown(edge, { clientX: 1000, pointerId: 1, button: 0 });
    fireEvent.pointerMove(edge, { clientX: 960, pointerId: 1 });
    fireEvent.pointerMove(edge, { clientX: 200, pointerId: 1 });
    expect(onChange).not.toHaveBeenCalled();
    expect((getByRole("complementary") as HTMLElement).style.width).toBe("520px");
    expect(edge.getAttribute("aria-valuenow")).toBe("520");
    fireEvent.pointerUp(edge, { clientX: 200, pointerId: 1 });
    expect(onChange).toHaveBeenCalledTimes(1);
    expect(onChange).toHaveBeenLastCalledWith(520);
  });
});
