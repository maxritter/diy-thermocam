import { describe, expect, test } from "bun:test";
import { act, screen, waitFor, within } from "@testing-library/react";
import type { CameraState } from "@/app/CameraChip";
import { type CaptureItem, type CaptureRef, capturesApi } from "@/lib/api/captures";
import { DEFAULT_PALETTES } from "@/lib/palettes";
import { generateDemoFrame } from "@/lib/thermal";
import type { ThermalFrame } from "@/lib/types";
import { renderApp } from "@/test/render";
import { CapturesPage, type CapturesPageProps } from "./CapturesPage";
import { tellingPoint } from "./Library";

type Api = typeof capturesApi;

function camItem(name: string, opts: Partial<CaptureItem> = {}): CaptureItem {
  const sd_path = `/${name}`;
  const isDir = !/\.dat$/i.test(name);
  return {
    id: `camera:${sd_path}`,
    ref: { kind: "camera", serial: "abc", sd_path },
    where: "camera",
    fileName: name,
    kind: isDir ? "recording" : "snapshot",
    size: isDir ? null : 38_805,
    frames: null,
    mtimeMs: null,
    ...opts,
  };
}

interface Deferred {
  item: CaptureItem;
  resolve: (url: string) => void;
}

/** A stub API whose pictures wait until the test lets them finish. */
function stubApi(over: Partial<Api> = {}) {
  const thumbs: Deferred[] = [];
  const api: Api = {
    ...capturesApi,
    cameraSerial: async () => "abc",
    listCamera: async () => ({
      card: { present: true, totalKb: 31_268_864, freeKb: 28_412_000 },
      items: [],
    }),
    listComputer: async () => [],
    cameraFolderFrames: async () => 60,
    loadFirstFrame: async () => {
      throw { kind: "other", detail: "not in this test" };
    },
    sidecarLoad: async () => ({ sidecar: null, quarantined: null }),
    thumbnail: (item: CaptureItem) =>
      new Promise<string>((resolve) => {
        thumbs.push({ item, resolve });
      }),
    ...over,
  };
  return { api, thumbs };
}

function props(api: Api, camera: CameraState = "camera"): CapturesPageProps {
  return {
    api,
    camera: { state: camera, connectionId: camera === "none" ? null : "port" },
    capturesDir: "/Users/me/Pictures/Thermocam",
    palettes: DEFAULT_PALETTES,
    paletteId: "ironblack",
    recentPaletteIds: [],
    unit: "celsius",
    engineeringDetails: false,
    notify: () => {},
    locale: "en-GB",
    now: () => new Date(2026, 10, 25, 15, 0, 0),
  };
}

const CARD_FILES = [
  "20261123180233.DAT",
  "20261125101500.DAT",
  "20261125104107",
  "20261125102014.DAT",
  "20261125102847.DAT",
];

/** A 4 × 2 frame of raw values at the demo calibration (raw = (°C + 273.15) × 100). */
function tinyFrame(celsius: number[]): ThermalFrame {
  const pixels = Uint16Array.from(celsius.map((c) => Math.round((c + 273.15) * 100)));
  return {
    ...generateDemoFrame(1, 4, 2),
    pixels,
    min_raw: Math.min(...pixels),
    max_raw: Math.max(...pixels),
  };
}

describe("the telling temperature", () => {
  test.each([
    ["a hot spot on a room-temperature scene", [22, 22, 22, 22, 22, 22, 23, 56], true, 56],
    ["a cold window in a warm room", [21, 21, 21, 21, 21, 21, 20, 9.8], false, 9.8],
    ["an even scene", [22, 22, 22, 22, 22, 22, 22, 22], true, 22],
  ] as const)("%s", (_why, cs, hot, c) => {
    const t = tellingPoint(tinyFrame([...cs]));
    expect(t.hot).toBe(hot);
    expect(t.raw).toBe(Math.round((c + 273.15) * 100));
  });

  test("cards show it in the unit chosen, with the capture's own material", async () => {
    const { api } = stubApi({
      thumbnail: async () => "blob:x",
      listCamera: async () => ({
        card: { present: true, totalKb: 100, freeKb: 50 },
        items: [camItem("20261125101500.DAT")],
      }),
      loadFirstFrame: async () => tinyFrame([22, 22, 22, 22, 22, 22, 23, 56]),
    });
    renderApp(<CapturesPage {...props(api)} />);
    // ε 0.95 against a 22 °C room reads a little above the apparent 56 °C.
    const chip = await screen.findByTitle("Hottest point");
    expect(chip.textContent).toMatch(/^5[67]\.\d °C$/);
  });
});

describe("the library", () => {
  test("camera cards show their names at once and pictures fill in newest first, two at a time", async () => {
    const { api, thumbs } = stubApi({
      listCamera: async () => ({
        card: { present: true, totalKb: 31_268_864, freeKb: 28_412_000 },
        items: CARD_FILES.map((n) => camItem(n)),
      }),
    });
    renderApp(<CapturesPage {...props(api)} />);
    expect(await screen.findByText("25 Nov 2026 · 10:20:14")).toBeTruthy();
    expect(screen.getByText("23 Nov 2026 · 18:02:33")).toBeTruthy();
    expect(screen.getByRole("button", { name: /On the camera · 5/ })).toBeTruthy();
    expect(screen.getByText("27.1 GB free of 29.8 GB")).toBeTruthy();
    // Names are on screen while every picture is still loading.
    expect(screen.getAllByRole("img", { name: "Picture loading" }).length).toBe(5);

    await waitFor(() => expect(thumbs.length).toBe(2));
    expect(thumbs.map((t) => t.item.fileName)).toEqual(["20261125104107", "20261125102847.DAT"]);
    await act(async () => thumbs[0].resolve("blob:one"));
    await waitFor(() => expect(thumbs.length).toBe(3));
    expect(thumbs[2].item.fileName).toBe("20261125102014.DAT");
    await act(async () => {
      for (const t of thumbs.slice(1)) t.resolve("blob:x");
    });
    await waitFor(() => expect(thumbs.length).toBe(5));
    expect(thumbs.map((t) => t.item.fileName)).toEqual([
      "20261125104107",
      "20261125102847.DAT",
      "20261125102014.DAT",
      "20261125101500.DAT",
      "20261123180233.DAT",
    ]);
  });

  test("a folder of frames is a recording and a typed name wins", async () => {
    const { api } = stubApi({
      listCamera: async () => ({
        card: { present: true, totalKb: 100, freeKb: 50 },
        items: [camItem("20261125104107"), camItem("20261125101500.DAT")],
      }),
      sidecarLoad: async (ref: CaptureRef) => ({
        sidecar:
          ref.kind === "camera" && ref.sd_path === "/20261125101500.DAT"
            ? {
                schema_version: 2,
                palette_id: "ironblack",
                range: { mode: "auto", min_c: null, max_c: null },
                material: { emissivity: 0.95, reflected_c: 22 },
                sensor_material: null,
                regions: [],
                notes: { name: "PCB load", text: "" },
              }
            : null,
        quarantined: null,
      }),
    });
    renderApp(<CapturesPage {...props(api)} />);
    expect(await screen.findByText("PCB load")).toBeTruthy();
    expect(screen.getByText("25 Nov 2026 · 10:15:00 · 38 KB")).toBeTruthy();
    const rec = screen.getByText("25 Nov 2026 · 10:41:07").closest(".cp-cap") as HTMLElement;
    expect(within(rec).getByText("Recording")).toBeTruthy();
    expect(await within(rec).findByText("60 frames")).toBeTruthy();
    expect(screen.getByText("Today")).toBeTruthy();
    expect(screen.getByText("Wednesday, 25 November")).toBeTruthy();
  });

  test("changing the tab drops the pictures still waiting", async () => {
    const { api, thumbs } = stubApi({
      listCamera: async () => ({
        card: { present: true, totalKb: 100, freeKb: 50 },
        items: CARD_FILES.map((n) => camItem(n)),
      }),
    });
    const { user } = renderApp(<CapturesPage {...props(api)} />);
    await waitFor(() => expect(thumbs.length).toBe(2));
    await user.click(screen.getByRole("button", { name: /On this computer/ }));
    await act(async () => {
      for (const t of thumbs) t.resolve("blob:x");
    });
    await new Promise((r) => setTimeout(r, 30));
    expect(thumbs.filter((t) => t.item.where === "camera").length).toBe(2);
    expect(await screen.findByText("Nothing on this computer yet")).toBeTruthy();
  });

  test.each([
    ["none", "Camera not connected", "Connect it with USB to see what is on its SD card."],
  ] as const)("with no camera the camera tab says %s", async (state, title, body) => {
    const { api } = stubApi();
    renderApp(<CapturesPage {...props(api, state)} />);
    expect(await screen.findByText(title)).toBeTruthy();
    expect(screen.getByText(body)).toBeTruthy();
  });

  test("no card, a read error with Try again, and an empty computer", async () => {
    const { api } = stubApi({
      listCamera: async () => ({ card: { present: false, totalKb: 0, freeKb: 0 }, items: [] }),
    });
    const first = renderApp(<CapturesPage {...props(api)} />);
    expect(await screen.findByText("No SD card in the camera")).toBeTruthy();
    first.unmount();

    let calls = 0;
    const failing = stubApi({
      listCamera: async () => {
        calls++;
        throw { kind: "other", detail: "timeout" };
      },
    });
    const { user } = renderApp(<CapturesPage {...props(failing.api)} />);
    expect(await screen.findByText("Couldn’t read the SD card")).toBeTruthy();
    expect(screen.getByText("The camera stopped answering while the list loaded.")).toBeTruthy();
    await user.click(screen.getByRole("button", { name: "Try again" }));
    await waitFor(() => expect(calls).toBe(2));

    await user.click(screen.getByRole("button", { name: /On this computer/ }));
    expect(await screen.findByText("Nothing on this computer yet")).toBeTruthy();
    expect(
      screen.getByText(
        "Snapshots and recordings you take in Live land here, and so do files you open.",
      ),
    ).toBeTruthy();
  });

  test("shift-click selects a range; a failed delete keeps that one selected and names it", async () => {
    let items = CARD_FILES.filter((n) => n.endsWith(".DAT")).map((n) => camItem(n));
    const deleted: string[][] = [];
    const { api } = stubApi({
      thumbnail: async () => "blob:x",
      listCamera: async () => ({ card: { present: true, totalKb: 100, freeKb: 50 }, items }),
      deleteFromCamera: async (paths: string[]) => {
        deleted.push(paths);
        items = items.filter((i) => i.ref.kind === "camera" && !paths.includes(i.ref.sd_path));
        items.push(camItem("20261125102014.DAT"));
        return {
          deleted: paths.filter((p) => p !== "/20261125102014.DAT"),
          failed: [{ path: "/20261125102014.DAT", kind: "other", detail: "the card is busy" }],
        };
      },
    });
    const { user } = renderApp(<CapturesPage {...props(api)} />);
    await user.click(
      await screen.findByRole("checkbox", { name: "Select 25 Nov 2026 · 10:28:47" }),
    );
    await user.keyboard("{Shift>}");
    await user.click(screen.getByRole("checkbox", { name: "Select 25 Nov 2026 · 10:15:00" }));
    await user.keyboard("{/Shift}");
    expect(screen.getByText("3 selected")).toBeTruthy();

    await user.click(within(screen.getByRole("toolbar")).getByRole("button", { name: "Delete" }));
    const dialog = await screen.findByRole("alertdialog");
    expect(dialog.textContent).toContain("Delete 3 captures from the camera?");
    expect(deleted).toEqual([]);
    await user.click(within(dialog).getByRole("button", { name: "Delete 3 captures" }));
    await waitFor(() => expect(deleted.length).toBe(1));
    expect(deleted[0].sort()).toEqual([
      "/20261125101500.DAT",
      "/20261125102014.DAT",
      "/20261125102847.DAT",
    ]);
    expect(
      (await screen.findByRole("alert")).textContent?.includes(
        "Couldn’t delete 25 Nov 2026 · 10:20:14: the card is busy",
      ),
    ).toBe(true);
    expect(await screen.findByText("1 selected")).toBeTruthy();
  });
});
