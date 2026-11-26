import { beforeEach, describe, expect, mock, test } from "bun:test";
import { act, waitFor, within } from "@testing-library/react";
import { DEFAULT_PALETTES } from "@/lib/palettes";
import { generateDemoFrame } from "@/lib/thermal";
import type { DisplayPipeline } from "@/lib/types";
import { renderApp } from "@/test/render";
import {
  type ExportApi,
  ExportDialog,
  type ExportProgress,
  RESOLUTION_SIZES,
  resetExportChoices,
} from "./ExportDialog";
import { createToastStore, Toasts } from "./Toasts";

const PIPELINE: DisplayPipeline = {
  filter: "none",
  upscaler: "edge_preserve",
  sharpen_amount: 0.45,
  rotate_180: false,
  flip_horizontal: false,
  histogram_eq: false,
  isotherm: { mode: "off", low_c: 40, high_c: 60, color: [0, 230, 160] },
};

const VIDEO = {
  path: "/Users/me/Pictures/Thermocam/20261126102100",
  frames: 312,
  duration_ms: 36_000,
};
const TIMELAPSE = {
  path: "/Users/me/Pictures/Thermocam/20261126090000",
  frames: 48,
  duration_ms: 240_000,
};

function setup(over: Partial<ExportApi> = {}) {
  let progress: ((p: ExportProgress) => void) | null = null;
  let finish: ((path: string) => void) | null = null;
  const api: ExportApi = {
    exportMedia: mock(
      () =>
        new Promise<string>((resolve) => {
          finish = resolve;
        }),
    ),
    onProgress: mock(async (cb: (p: ExportProgress) => void) => {
      progress = cb;
      return () => {
        progress = null;
      };
    }),
    reveal: mock(async () => {}),
    ...over,
  };
  const toasts = createToastStore();
  const onOpenChange = mock((_open: boolean) => {});
  const onCopyImage = mock(() => {});
  const frame = generateDemoFrame(5);
  const view = renderApp(
    <>
      <ExportDialog
        open
        onOpenChange={onOpenChange}
        api={api}
        toasts={toasts}
        source={() => ({
          frame,
          palette: DEFAULT_PALETTES[0],
          range: { mode: "auto" },
          rad: { cal_offset: -273.15, cal_slope: 0.01, emissivity: 0.95, reflected_temp_c: 22 },
          pipeline: PIPELINE,
          rois: [],
          unit: "celsius",
        })}
        recordings={{ video: VIDEO, timelapse: TIMELAPSE }}
        onCopyImage={onCopyImage}
        platform="mac"
      />
      <Toasts store={toasts} />
    </>,
  );
  return {
    ...view,
    api,
    onOpenChange,
    onCopyImage,
    progress: (p: ExportProgress) => progress?.(p),
    finish: (path: string) => finish?.(path),
  };
}

beforeEach(() => resetExportChoices());

describe("the Export dialog", () => {
  test("Video › MP4 at 1080p exports the last recording and shows progress, then the result", async () => {
    const { user, findByRole, getByRole, api, progress, finish, onOpenChange, findByText } =
      setup();
    const dialog = await findByRole("dialog", { name: "Export" });
    await user.click(within(dialog).getByRole("tab", { name: "Video" }));
    const panel = within(dialog).getByRole("tabpanel", { name: "Video" });
    const from = within(panel).getByRole("group", { name: "Source" });
    expect(within(from).getByRole("button", { name: "Last recording · 36 s" })).toBeTruthy();
    await user.click(within(panel).getByRole("button", { name: "MP4" }));
    await user.click(within(panel).getByRole("button", { name: "1080p" }));
    await user.click(getByRole("button", { name: "Export…" }));

    await waitFor(() => expect(api.exportMedia).toHaveBeenCalledTimes(1));
    const req = (api.exportMedia as ReturnType<typeof mock>).mock.calls[0][0] as Record<
      string,
      unknown
    >;
    expect(req.kind).toBe("mp4");
    expect(req.resolution).toBe("full_hd1080p");
    expect(RESOLUTION_SIZES[req.resolution as keyof typeof RESOLUTION_SIZES]).toEqual([1920, 1080]);
    expect(req.recording).toBe(VIDEO.path);
    expect(req.pixels).toBeUndefined();

    act(() => progress({ stage: "encoding", done: 42, total: 100 }));
    expect(await within(dialog).findByText("Exporting MP4 · 42%")).toBeTruthy();

    await act(async () => finish("/Users/me/Movies/board.mp4"));
    const msg = (await findByText("Exported board.mp4")).closest("[role=status]");
    // The stub keeps the dialog open, so the page behind it is hidden from the tree.
    const reveal = within(msg as HTMLElement).getByRole("button", {
      name: "Show in Finder",
      hidden: true,
    });
    expect(reveal).toBeTruthy();
    expect(onOpenChange).toHaveBeenCalledWith(false);
  });

  test("Video can export the time-lapse as a GIF at 4K", async () => {
    const { user, findByRole, getByRole, api } = setup();
    const dialog = await findByRole("dialog", { name: "Export" });
    await user.click(within(dialog).getByRole("tab", { name: "Video" }));
    const panel = within(dialog).getByRole("tabpanel", { name: "Video" });
    await user.click(within(panel).getByRole("button", { name: "Time-lapse · 48 frames" }));
    await user.click(within(panel).getByRole("button", { name: "GIF" }));
    await user.click(within(panel).getByRole("button", { name: "4K" }));
    await user.click(getByRole("button", { name: "Export…" }));
    await waitFor(() => expect(api.exportMedia).toHaveBeenCalledTimes(1));
    const req = (api.exportMedia as ReturnType<typeof mock>).mock.calls[0][0] as Record<
      string,
      unknown
    >;
    expect([req.kind, req.resolution, req.recording]).toEqual(["gif", "uhd4k", TIMELAPSE.path]);
  });

  test("Image sizes name their pixels, and PNG sends the frame on screen", async () => {
    const { user, findByRole, getByRole, api } = setup();
    const dialog = await findByRole("dialog", { name: "Export" });
    const panel = within(dialog).getByRole("tabpanel", { name: "Image" });
    expect(within(panel).getByText("1920 × 1080 pixels")).toBeTruthy();
    await user.click(within(panel).getByRole("button", { name: "Sensor ×4" }));
    expect(within(panel).getByText("640 × 480 pixels")).toBeTruthy();
    await user.click(within(panel).getByRole("switch", { name: "Include temperature scale" }));
    await user.click(getByRole("button", { name: "Export…" }));
    await waitFor(() => expect(api.exportMedia).toHaveBeenCalledTimes(1));
    const req = (api.exportMedia as ReturnType<typeof mock>).mock.calls[0][0] as Record<
      string,
      unknown
    >;
    expect([req.kind, req.resolution, req.show_colorbar, req.show_hud]).toEqual([
      "png",
      "native",
      false,
      true,
    ]);
    expect((req.pixels as number[]).length).toBe(160 * 120);
  });

  test("Data and Report send their formats", async () => {
    const { user, findByRole, getByRole, api, finish } = setup();
    const dialog = await findByRole("dialog", { name: "Export" });
    await user.click(within(dialog).getByRole("tab", { name: "Data" }));
    await user.click(within(dialog).getByRole("radio", { name: /Thermal TIFF, 16-bit/ }));
    await user.click(getByRole("button", { name: "Export…" }));
    await waitFor(() => expect(api.exportMedia).toHaveBeenCalledTimes(1));
    const calls = (api.exportMedia as ReturnType<typeof mock>).mock.calls;
    expect((calls[0][0] as { kind: string }).kind).toBe("tiff");
    await act(async () => finish("/tmp/a.tiff"));

    await user.click(within(dialog).getByRole("tab", { name: "Report" }));
    const panel = within(dialog).getByRole("tabpanel", { name: "Report" });
    await user.type(within(panel).getByLabelText("Title"), "Board Rev C");
    await user.click(within(panel).getByRole("button", { name: "Markdown" }));
    await user.click(getByRole("button", { name: "Export…" }));
    await waitFor(() => expect(api.exportMedia).toHaveBeenCalledTimes(2));
    const r = calls[1][0] as { kind: string; title: string };
    expect([r.kind, r.title]).toEqual(["md", "Board Rev C"]);
  });

  test("a cancelled Save dialog leaves the dialog open with no message", async () => {
    const api = {
      exportMedia: mock(async () => {
        throw { kind: "cancelled", detail: "No file was chosen." };
      }),
    };
    const { user, findByRole, getByRole, onOpenChange, queryByRole } = setup(api);
    await findByRole("dialog", { name: "Export" });
    await user.click(getByRole("button", { name: "Export…" }));
    await waitFor(() => expect(api.exportMedia).toHaveBeenCalledTimes(1));
    expect(onOpenChange).not.toHaveBeenCalledWith(false);
    expect(queryByRole("status")).toBeNull();
  });

  test("Copy image is in the footer", async () => {
    const { user, findByRole, onCopyImage } = setup();
    const dialog = await findByRole("dialog", { name: "Export" });
    await user.click(within(dialog).getByRole("button", { name: "Copy image" }));
    expect(onCopyImage).toHaveBeenCalledTimes(1);
  });
});
