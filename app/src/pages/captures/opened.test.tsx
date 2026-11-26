import { describe, expect, test } from "bun:test";
import { screen, waitFor } from "@testing-library/react";
import {
  type CaptureItem,
  type CaptureRef,
  capturesApi,
  type ThermoSidecar,
} from "@/lib/api/captures";
import { DEFAULT_PALETTES } from "@/lib/palettes";
import { generateDemoFrame } from "@/lib/thermal";
import { renderApp } from "@/test/render";
import { OpenedCapture, type OpenedCaptureProps } from "./OpenedCapture";

type Api = typeof capturesApi;

const ITEM: CaptureItem = {
  id: "local:/caps/20261125101500.DAT",
  ref: { kind: "local", path: "/caps/20261125101500.DAT" },
  where: "computer",
  fileName: "20261125101500.DAT",
  kind: "snapshot",
  size: 38_805,
  frames: null,
  mtimeMs: null,
};

function stub(over: Partial<Api> = {}) {
  const saves: ThermoSidecar[] = [];
  const resets: CaptureRef[] = [];
  const api: Api = {
    ...capturesApi,
    loadFrame: async () => generateDemoFrame(3),
    sidecarLoad: async () => ({ sidecar: null, quarantined: null }),
    sidecarSave: async (_ref: CaptureRef, s: ThermoSidecar) => {
      saves.push(structuredClone(s));
      return "/caps/20261125101500.thermo.json";
    },
    sidecarReset: async (ref: CaptureRef) => {
      resets.push(ref);
      return true;
    },
    ...over,
  };
  return { api, saves, resets };
}

function props(api: Api, over: Partial<OpenedCaptureProps> = {}): OpenedCaptureProps {
  return {
    api,
    item: ITEM,
    entries: [],
    candidates: [],
    onBack: () => {},
    onOpen: () => {},
    palettes: DEFAULT_PALETTES,
    paletteId: "ironblack",
    recentPaletteIds: ["ironblack", "rainbow", "inferno"],
    onImportPalette: async () => {},
    unit: "celsius",
    engineeringDetails: false,
    notify: () => {},
    locale: "en-GB",
    // Short enough to keep the tests quick, long enough that a slow CI machine pausing between
    // two clicks of one test does not save in between (Linux container run, 26 Nov).
    saveDelayMs: 300,
    ...over,
  };
}

const wait = (ms: number) => new Promise((r) => setTimeout(r, ms));

describe("an opened capture", () => {
  test("opening changes nothing; edits save once, 500 ms after the last one", async () => {
    const { api, saves } = stub();
    const { user } = renderApp(<OpenedCapture {...props(api)} />);
    expect(await screen.findByText("25 Nov 2026 · 10:15:00")).toBeTruthy();
    await wait(80);
    expect(saves.length).toBe(0);

    await user.click(screen.getByRole("button", { name: "Rainbow" }));
    await user.click(screen.getByRole("tab", { name: "Notes" }));
    await user.type(screen.getByRole("textbox", { name: "Name" }), "Board");
    await waitFor(() => expect(saves.length).toBe(1));
    expect(saves[0].palette_id).toBe("rainbow");
    expect(saves[0].notes.name).toBe("Board");
    expect(await screen.findByText("Saved · the original file is never changed")).toBeTruthy();
    // The typed name is the title now, and the date moves beside it.
    expect(screen.getByText("Board")).toBeTruthy();
  });

  test("Undo twice steps back through the name and the palette; Redo comes forward", async () => {
    const { api, saves } = stub();
    const { user } = renderApp(<OpenedCapture {...props(api)} />);
    await user.click(await screen.findByRole("button", { name: "Rainbow" }));
    await user.click(screen.getByRole("tab", { name: "Notes" }));
    await user.type(screen.getByRole("textbox", { name: "Name" }), "Board");
    await waitFor(() => expect(saves.length).toBe(1));

    await user.click(screen.getByRole("button", { name: "Undo" }));
    expect((screen.getByRole("textbox", { name: "Name" }) as HTMLInputElement).value).toBe("");
    await user.click(screen.getByRole("button", { name: "Undo" }));
    await waitFor(() => expect(saves.length).toBe(2));
    expect(saves[1].palette_id).toBe("ironblack");
    expect(saves[1].notes.name).toBe("");

    await user.click(screen.getByRole("button", { name: "Redo" }));
    await waitFor(() => expect(saves.length).toBe(3));
    expect(saves[2].palette_id).toBe("rainbow");
  });

  test("Reset to original deletes the edits file and shows the capture as taken", async () => {
    const sidecar: ThermoSidecar = {
      schema_version: 2,
      palette_id: "rainbow",
      range: { mode: "manual", min_c: 25, max_c: 40 },
      material: { emissivity: 0.9, reflected_c: 21 },
      sensor_material: null,
      regions: [
        {
          id: "r1",
          name: "MCU",
          geometry: { kind: "box", x: 50, y: 46, w: 24, h: 18 },
          material: null,
          visible: true,
        },
      ],
      notes: { name: "PCB load", text: "Fan off." },
    };
    const { api, resets, saves } = stub({
      sidecarLoad: async () => ({ sidecar, quarantined: null }),
    });
    const { user } = renderApp(<OpenedCapture {...props(api, { initialTab: "notes" })} />);
    expect(await screen.findByText("PCB load")).toBeTruthy();
    expect(screen.getByText("Saved · the original file is never changed")).toBeTruthy();
    await user.click(screen.getByRole("button", { name: "Reset to original" }));
    await waitFor(() => expect(resets.length).toBe(1));
    expect((screen.getByRole("textbox", { name: "Name" }) as HTMLInputElement).value).toBe("");
    expect(
      screen.getByText("Edits save by themselves · the original file is never changed"),
    ).toBeTruthy();
    await wait(80);
    expect(saves.length).toBe(0);
  });

  test("a damaged edits file is set aside and named", async () => {
    const { api } = stub({
      sidecarLoad: async () => ({ sidecar: null, quarantined: "20261125101500.thermo.json.bad" }),
    });
    renderApp(<OpenedCapture {...props(api)} />);
    const msg = await screen.findByRole("alert");
    expect(msg.textContent).toContain("20261125101500.thermo.json.bad");
    expect(msg.textContent).toContain("opens as it was taken");
  });

  test("a damaged .DAT shows the message with Back to Captures", async () => {
    let back = 0;
    const { api } = stub({
      loadFrame: async () => {
        throw { kind: "bad_file", detail: "unexpected file size 1204" };
      },
    });
    const { user } = renderApp(<OpenedCapture {...props(api, { onBack: () => back++ })} />);
    expect(await screen.findByText("This file can’t be opened")).toBeTruthy();
    await user.click(screen.getByRole("button", { name: "Back to Captures" }));
    expect(back).toBeGreaterThan(0);
  });
});
