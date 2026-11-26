import { describe, expect, test } from "bun:test";
import { within } from "@testing-library/react";
import { useState } from "react";
import { createFrameStore, type FrameStore } from "@/lib/frameStore";
import { DEFAULT_MATERIALS } from "@/lib/palettes";
import { generateDemoFrame } from "@/lib/thermal";
import type { RadiometryParams, Roi } from "@/lib/types";
import { renderApp } from "@/test/render";
import { type MaterialChoice, MeasureTab } from "./MeasureTab";

/** The option a select shows. */
const shown = (s: HTMLSelectElement) => s.options[s.selectedIndex]?.textContent;

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

const REGIONS: Roi[] = [
  {
    id: "mcu",
    label: "MCU",
    geometry: { kind: "spot", x: 61, y: 58, kernel: 3, show_raw: false },
    emissivity_override: null,
    reflected_temp_override_c: null,
    visible: true,
  },
  {
    id: "vrm",
    label: "VRM",
    geometry: { kind: "box", x: 96, y: 30, w: 26, h: 24 },
    emissivity_override: null,
    reflected_temp_override_c: null,
    visible: true,
  },
];

interface Seen {
  rad: RadiometryParams;
  materialId: string | null;
  rois: Roi[];
}

function Harness({ store, seen, regions }: { store: FrameStore; seen: Seen; regions: Roi[] }) {
  const [rad, setRad] = useState<RadiometryParams>({
    cal_offset: -273.15,
    cal_slope: 0.01,
    emissivity: 0.95,
    reflected_temp_c: 22,
  });
  const [materialId, setMaterialId] = useState<string | null>(null);
  const [rois, setRois] = useState<Roi[]>(regions);
  const [selected, setSelected] = useState<string | null>(null);
  seen.rad = rad;
  seen.materialId = materialId;
  seen.rois = rois;
  return (
    <MeasureTab
      store={store}
      rad={rad}
      materialId={materialId}
      materials={DEFAULT_MATERIALS}
      unit="celsius"
      onMaterialChange={(m: MaterialChoice) => {
        setMaterialId(m.id);
        setRad((r) => ({ ...r, emissivity: m.emissivity, reflected_temp_c: m.reflected_c }));
      }}
      rois={rois}
      selectedRoiId={selected}
      onSelectRoi={setSelected}
      onRoisChange={setRois}
      onFitRange={() => {}}
      chartsOpen={false}
      onChartsOpenChange={() => {}}
    />
  );
}

function setup(regions: Roi[] = []) {
  const store = immediateStore();
  const seen: Seen = { rad: {} as RadiometryParams, materialId: null, rois: [] };
  const r = renderApp(<Harness store={store} seen={seen} regions={regions} />);
  return { ...r, seen };
}

describe("the Measure tab", () => {
  test("choosing a material sets its emissivity; typing one makes it Custom", async () => {
    const { seen, user, getByRole } = setup();
    const material = getByRole("combobox", { name: "Material" }) as HTMLSelectElement;
    const emissivity = getByRole("textbox", { name: "Emissivity" }) as HTMLInputElement;
    expect(shown(material)).toBe("Custom");

    await user.selectOptions(material, "Matte paint · 0.94");
    expect(seen.rad.emissivity).toBe(0.94);
    expect(seen.materialId).toBe("paint_matte");
    expect(emissivity.value).toBe("0.94");
    expect(shown(material)).toBe("Matte paint · 0.94");

    await user.clear(emissivity);
    await user.type(emissivity, "0.9{Enter}");
    expect(seen.rad.emissivity).toBe(0.9);
    expect(seen.materialId).toBeNull();
    expect(shown(material)).toBe("Custom");
  });

  test("a region's own material changes only its row's values", async () => {
    const { seen, user, container } = setup(REGIONS);
    const row = (id: string) => container.querySelector(`[data-region-row="${id}"]`) as HTMLElement;
    const value = (id: string) => row(id).querySelector(".val")?.textContent ?? "";
    const mcuBefore = value("mcu");
    const vrmBefore = value("vrm");
    expect(mcuBefore).toMatch(/°C/);

    await user.click(within(row("mcu")).getByText("MCU"));
    const own = within(container).getByRole("combobox", { name: "Material of MCU" });
    expect(shown(own as HTMLSelectElement)).toBe("Same as image");
    await user.selectOptions(own, "Polished copper or brass · 0.08");

    expect(seen.rois[0].emissivity_override).toBe(0.08);
    expect(seen.rois[1].emissivity_override).toBeNull();
    expect(seen.rad.emissivity).toBe(0.95);
    expect(value("mcu")).not.toBe(mcuBefore);
    expect(value("vrm")).toBe(vrmBefore);
  });
});
