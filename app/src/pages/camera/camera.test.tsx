import { afterEach, describe, expect, test } from "bun:test";
import { act, waitFor, within } from "@testing-library/react";
import type { CameraState } from "@/app/CameraChip";
import { cameraApi } from "@/lib/api/camera";
import type { HelloResp } from "@/lib/types";
import { renderApp } from "@/test/render";
import { CameraPage, type CameraPageProps } from "./CameraPage";
import { selfTestText } from "./Diagnostics";
import { dispatchFirmwareEvent, resetFirmwareFlow } from "./FirmwareDialog";

const HELLO: HelloResp = {
  proto_major: 2,
  proto_minor: 0,
  fw_version: 400,
  fw_build_str: "4.0.0",
  hw_version: 3,
  lepton_type: 1,
  sensor_w: 160,
  sensor_h: 120,
  display_w: 320,
  display_h: 240,
  diagnostic_mask: 0xff,
  teensy_uid: 1,
  capabilities: 0x01ff,
  max_chunk_size: 8192,
  heartbeat_timeout_ms: 5000,
};

function props(state: CameraState, over: Partial<CameraPageProps> = {}): CameraPageProps {
  const connected = state === "camera" || state === "demo";
  return {
    state,
    port: connected ? (state === "demo" ? "sim://demo-lepton35" : "/dev/cu.usbmodem14101") : null,
    hello: connected ? HELLO : null,
    unit: "celsius",
    buttonAction: "snapshot",
    onButtonActionChange: () => {},
    materialOnCamera: false,
    onMaterialOnCameraChange: () => {},
    liveMaterial: { emissivity: 0.95, reflected_c: 22 },
    onDisconnect: () => {},
    onUseDemo: () => {},
    onReconnect: async () => {},
    ...over,
  };
}

afterEach(() => resetFirmwareFlow());

const FILE = { name: "thermocam-4.0.1.hex", text: ":02000004600098" };
const INFO = {
  sha256_hex: `3f9a07c2${"0".repeat(48)}e41bc21e`,
  total_bytes: 412_000,
  block_count: 403,
  min_addr: 0x60000000,
  max_addr: 0x60064a5f,
};

describe("the Camera page", () => {
  test("with no camera it shows No camera, with Update firmware… still enabled", async () => {
    let demo = 0;
    const { findByRole, getByRole, user } = renderApp(
      <CameraPage {...props("none", { onUseDemo: () => demo++ })} />,
    );
    const panel = await findByRole("region", { name: "No camera connected" });
    expect(
      within(panel).getByText(
        "Plug in a Thermocam to see its status and settings. With the demo camera, this page shows its values read-only.",
      ),
    ).toBeTruthy();
    const update = getByRole("button", { name: "Update firmware…" }) as HTMLButtonElement;
    expect(update.disabled).toBe(false);
    expect(document.querySelector('[aria-label="Camera settings"]')).toBeNull();
    await user.click(getByRole("button", { name: "Use the demo camera" }));
    expect(demo).toBe(1);
  });

  test("the demo camera shows its values read-only", async () => {
    const { findByText, getByRole } = renderApp(<CameraPage {...props("demo")} />);
    expect(await findByText("Read-only")).toBeTruthy();
    await waitFor(() =>
      expect(
        (
          getByRole("checkbox", {
            name: "Spot temperature on the camera screen",
          }) as HTMLInputElement
        ).checked,
      ).toBe(true),
    );
    for (const name of [
      "Spot temperature on the camera screen",
      "Mounted upside down",
      "Use Studio's material on the camera's screen",
    ]) {
      expect((getByRole("checkbox", { name }) as HTMLInputElement).disabled).toBe(true);
    }
    expect((getByRole("button", { name: "Calibrate now" }) as HTMLButtonElement).disabled).toBe(
      true,
    );
    // The button choice is Studio's own setting, so it stays available.
    const group = getByRole("group", { name: "Capture button" });
    expect(
      (within(group).getByRole("button", { name: "Record in Studio" }) as HTMLButtonElement)
        .disabled,
    ).toBe(false);
  });

  test("the material switch starts off", async () => {
    const { findByRole } = renderApp(<CameraPage {...props("demo")} />);
    const sw = (await findByRole("checkbox", {
      name: "Use Studio's material on the camera's screen",
    })) as HTMLInputElement;
    expect(sw.checked).toBe(false);
  });

  test("the button choice is saved through Studio's settings", async () => {
    const chosen: string[] = [];
    const { findByRole, user } = renderApp(
      <CameraPage {...props("demo", { onButtonActionChange: (a) => chosen.push(a) })} />,
    );
    const group = await findByRole("group", { name: "Capture button" });
    await user.click(within(group).getByRole("button", { name: "Record in Studio" }));
    await user.click(within(group).getByRole("button", { name: "Nothing" }));
    expect(chosen).toEqual(["record", "nothing"]);
  });

  test("a failed self-test shows on the status card, in red", async () => {
    const real = cameraApi.getDiagnostics;
    const healthy = await real();
    cameraApi.getDiagnostics = async () => ({ ...healthy, diagnostic_mask: 0xff & ~(1 << 2) });
    try {
      const { findByRole } = renderApp(<CameraPage {...props("camera")} />);
      const card = await findByRole("region", { name: "Thermocam V4" });
      const cell = await within(card).findByText("Failed: SD card");
      expect(cell.className).toContain("fail");
    } finally {
      cameraApi.getDiagnostics = real;
    }
    expect(selfTestText(0xff)).toBe("All OK");
  });

  test("Diagnostics opens below with timing, rates and hardware that never wrap", async () => {
    const { findByText, container, user } = renderApp(<CameraPage {...props("demo")} />);
    await user.click(await findByText("Diagnostics"));
    expect(await findByText(/Frame timing · 8\.70 ms of 113\.9 ms per frame/)).toBeTruthy();
    expect(await findByText("840 µs")).toBeTruthy();
    expect(await findByText("8.78 fps (max 8.8)")).toBeTruthy();
    expect(await findByText("All OK (0xFF)")).toBeTruthy();
    for (const dl of container.querySelectorAll(".cm-diag dl")) {
      expect(dl.classList.contains("nowrap")).toBe(true);
    }
  });

  test("the firmware dialog walks its drawn states", async () => {
    const { findByRole, queryByRole } = renderApp(<CameraPage {...props("camera")} />);
    act(() => {
      dispatchFirmwareEvent({ type: "choose", file: FILE });
      dispatchFirmwareEvent({ type: "checked", info: INFO });
    });
    let dlg = await findByRole("dialog", { name: "Update firmware" });
    expect(within(dlg).getByText("File checked · 412 KB")).toBeTruthy();
    expect(within(dlg).getByText("4.0.0")).toBeTruthy();
    expect(within(dlg).getByRole("button", { name: "Install" })).toBeTruthy();

    act(() => {
      dispatchFirmwareEvent({ type: "install", now: Date.now() });
      dispatchFirmwareEvent({ type: "call_started" });
    });
    dlg = await findByRole("dialog", { name: "Waiting for update mode" });
    expect(within(dlg).getByText("0:00")).toBeTruthy();

    act(() => dispatchFirmwareEvent({ type: "progress", ev: { stage: "waiting_timeout" } }));
    dlg = await findByRole("dialog", { name: "The camera hasn’t switched yet" });
    expect(within(dlg).getByRole("button", { name: "Try again" })).toBeTruthy();

    act(() =>
      dispatchFirmwareEvent({ type: "progress", ev: { stage: "flashing", done: 169, total: 403 } }),
    );
    dlg = await findByRole("dialog", { name: "Updating firmware" });
    expect(within(dlg).getByText("42%")).toBeTruthy();
    expect(within(dlg).queryByRole("button")).toBeNull();

    act(() =>
      dispatchFirmwareEvent({
        type: "progress",
        ev: { stage: "error", kind: "write", detail: "HID write failed" },
      }),
    );
    dlg = await findByRole("alertdialog", { name: "The update stopped" });
    expect(within(dlg).getByText("Writing firmware stopped at 42%")).toBeTruthy();

    act(() => dispatchFirmwareEvent({ type: "close" }));
    await waitFor(() => expect(queryByRole("alertdialog")).toBeNull());
  });

  test("a refused file keeps Install off and says nothing was sent", async () => {
    const { findByRole } = renderApp(<CameraPage {...props("none")} />);
    act(() => {
      dispatchFirmwareEvent({ type: "choose", file: { name: "blink-test.hex", text: "x" } });
      dispatchFirmwareEvent({
        type: "check_failed",
        reason: "Not firmware for a Teensy 4.1 camera",
      });
    });
    const dlg = await findByRole("dialog", { name: "Update firmware" });
    expect(within(dlg).getByText("Not firmware for a Teensy 4.1 camera")).toBeTruthy();
    expect(
      within(dlg).getByText(
        "Download the firmware for your camera from the releases page. Nothing was sent to the camera.",
      ),
    ).toBeTruthy();
    expect(
      (within(dlg).getByRole("button", { name: "Install" }) as HTMLButtonElement).disabled,
    ).toBe(true);
  });

  test("done says the camera is connected again", async () => {
    const { findByRole } = renderApp(<CameraPage {...props("camera")} />);
    act(() => {
      dispatchFirmwareEvent({ type: "choose", file: FILE });
      dispatchFirmwareEvent({ type: "checked", info: INFO });
      dispatchFirmwareEvent({ type: "install", now: Date.now() });
      dispatchFirmwareEvent({ type: "progress", ev: { stage: "flashing", done: 403, total: 403 } });
      dispatchFirmwareEvent({ type: "progress", ev: { stage: "rebooted", sha256: "x" } });
      dispatchFirmwareEvent({ type: "reconnected" });
    });
    const dlg = await findByRole("dialog", { name: "Firmware updated" });
    expect(
      within(dlg).getByText("The camera restarted with the new firmware and is connected again."),
    ).toBeTruthy();
  });
});
