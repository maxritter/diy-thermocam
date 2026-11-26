import { describe, expect, mock, test } from "bun:test";
import { act, waitFor, within } from "@testing-library/react";
import { createToastStore, Toasts } from "@/app/Toasts";
import type { RecordingInfo, RenderSpec, SnapshotInfo } from "@/lib/types";
import { renderApp } from "@/test/render";
import { CaptureBar } from "./CaptureBar";
import { type CaptureApi, createCaptureActions } from "./captureActions";
import { RecordingBar } from "./RecordingBar";

const SPEC: RenderSpec = { palette_id: "inferno", scale: 4 };
const SAVED: SnapshotInfo = {
  path: "/Users/me/Pictures/Thermocam/20261126102014.DAT",
  png_path: null,
};

function stubApi(over: Partial<CaptureApi> = {}): CaptureApi {
  return {
    snapshot: mock(async () => SAVED),
    renderPng: mock(async () => new ArrayBuffer(8)),
    reveal: mock(async () => {}),
    writeClipboardPng: mock(async () => {}),
    recordStart: mock(async () => ({ path: "/Users/me/Pictures/Thermocam/20261126102100" })),
    recordStop: mock(async () => ({
      path: "/Users/me/Pictures/Thermocam/20261126102100",
      frames: 312,
      duration_ms: 36_000,
      stopped: "user" as const,
      detail: null,
    })),
    recordStatus: mock(async () => null),
    ...over,
  };
}

function setup(api: CaptureApi, opts: { png?: boolean } = {}) {
  const toasts = createToastStore();
  const chooseFolder = mock(() => {});
  const exportVideo = mock((_info: RecordingInfo) => {});
  const actions = createCaptureActions({
    api,
    toasts,
    spec: () => SPEC,
    pngOnSnapshot: () => opts.png ?? false,
    chooseFolder,
    exportVideo,
    sensorMaterial: () => null,
    platform: "mac",
  });
  const view = renderApp(
    <>
      <RecordingBar recorder={actions.recorder} connection="live" streaming fps={8.7} />
      <CaptureBar
        actions={actions}
        streaming
        connection="live"
        panelHidden={false}
        onTogglePause={() => {}}
        onCalibrate={() => {}}
        onExport={() => {}}
        onTogglePanel={() => {}}
      />
      <Toasts store={toasts} />
    </>,
  );
  return { ...view, actions, toasts, chooseFolder, exportVideo };
}

describe("Snapshot", () => {
  test("a read-only Captures folder says so and offers another folder", async () => {
    const api = stubApi({
      snapshot: mock(async () => {
        throw { kind: "read_only", detail: "Permission denied (os error 13)" };
      }),
    });
    const { user, getByRole, findByRole, chooseFolder } = setup(api);
    await user.click(getByRole("button", { name: "Snapshot" }));
    const alert = await findByRole("alert");
    expect(alert.textContent).toContain("Couldn’t save: the folder is read-only.");
    await user.click(within(alert).getByRole("button", { name: "Choose another folder" }));
    expect(chooseFolder).toHaveBeenCalledTimes(1);
  });

  test("a saved snapshot offers Copy image, Save PNG… and Show in Finder", async () => {
    const api = stubApi();
    const { user, getByRole, findByText } = setup(api);
    await user.click(getByRole("button", { name: "Snapshot" }));
    expect(api.snapshot).toHaveBeenCalledWith(null);
    const msg = (await findByText("Snapshot saved to Captures")).closest("[role=status]");
    if (!(msg instanceof HTMLElement)) throw new Error("no message");
    for (const name of ["Copy image", "Save PNG…", "Show in Finder"]) {
      expect(within(msg).getByRole("button", { name })).toBeTruthy();
    }

    await user.click(within(msg).getByRole("button", { name: "Copy image" }));
    const source = { kind: "local", path: SAVED.path };
    await waitFor(() => expect(api.writeClipboardPng).toHaveBeenCalledTimes(1));
    expect(api.renderPng).toHaveBeenCalledWith(source, SPEC, {});
  });

  test("Save PNG… renders with a Save dialog and Show in Finder reveals the file", async () => {
    const api = stubApi({ renderPng: mock(async () => ({ path: "/tmp/x.png" })) });
    const { user, getByRole, findByText, actions } = setup(api);
    await user.click(getByRole("button", { name: "Snapshot" }));
    let msg = (await findByText("Snapshot saved to Captures")).closest("[role=status]");
    await user.click(within(msg as HTMLElement).getByRole("button", { name: "Save PNG…" }));
    await waitFor(() =>
      expect(api.renderPng).toHaveBeenCalledWith({ kind: "local", path: SAVED.path }, SPEC, {
        dialog: true,
      }),
    );
    await findByText("Exported x.png");

    await act(() => actions.snapshot());
    msg = (await findByText("Snapshot saved to Captures")).closest("[role=status]");
    await user.click(within(msg as HTMLElement).getByRole("button", { name: "Show in Finder" }));
    expect(api.reveal).toHaveBeenCalledWith(SAVED.path);
  });

  test("with PNG on every snapshot, the PNG is rendered with the view's spec", async () => {
    const api = stubApi();
    const { user, getByRole } = setup(api, { png: true });
    await user.click(getByRole("button", { name: "Snapshot" }));
    expect(api.snapshot).toHaveBeenCalledWith(SPEC);
  });
});

describe("Record", () => {
  test("recording shows its bar, waits Pause and Calibrate, and stops with its message", async () => {
    const api = stubApi();
    const { user, getByRole, findByRole, findByText, actions, exportVideo } = setup(api);
    await user.click(getByRole("button", { name: "Record" }));
    expect(api.recordStart).toHaveBeenCalledWith({ sensorMaterial: null });
    await findByRole("button", { name: "Stop recording" });
    expect((getByRole("button", { name: "Pause" }) as HTMLButtonElement).disabled).toBe(true);
    expect((getByRole("button", { name: "Calibrate" }) as HTMLButtonElement).disabled).toBe(true);
    expect((getByRole("button", { name: "Export…" }) as HTMLButtonElement).disabled).toBe(true);
    expect((getByRole("button", { name: "Snapshot" }) as HTMLButtonElement).disabled).toBe(false);
    await findByText(/^Recording/);

    await user.click(getByRole("button", { name: "Stop recording" }));
    // Rust sends `recording-stopped` after every stop; the message comes from it.
    const info = await api.recordStop();
    act(() => actions.recorder.stopped(info));
    const msg = (await findByText("Recording saved to Captures · 36 s")).closest("[role=status]");
    await user.click(within(msg as HTMLElement).getByRole("button", { name: "Export video…" }));
    expect(exportVideo).toHaveBeenCalledWith(info);
    await findByRole("button", { name: "Record" });
  });

  test("a time-lapse starts from the Record menu with its interval", async () => {
    const api = stubApi();
    const { user, getByRole, findByRole, findByText } = setup(api);
    await user.click(getByRole("button", { name: "Recording options" }));
    const menu = await findByRole("dialog", { name: "Recording options" });
    await user.selectOptions(within(menu).getByLabelText("Time-lapse interval"), "10 seconds");
    await user.click(within(menu).getByRole("button", { name: "Start time-lapse" }));
    expect(api.recordStart).toHaveBeenCalledWith({ intervalMs: 10_000, sensorMaterial: null });
    await findByRole("button", { name: "Stop time-lapse" });
    await findByText(/Every 10 s · 0 saved/);
  });

  test("a recording that cannot start because the folder is read-only says so", async () => {
    const api = stubApi({
      recordStart: mock(async () => {
        throw { kind: "read_only", detail: "Permission denied" };
      }),
    });
    const { user, getByRole, findByRole } = setup(api);
    await user.click(getByRole("button", { name: "Record" }));
    const alert = await findByRole("alert");
    expect(alert.textContent).toContain("Couldn’t save: the folder is read-only.");
    await findByRole("button", { name: "Record" });
  });
});
