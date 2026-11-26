import { describe, expect, mock, test } from "bun:test";
import { within } from "@testing-library/react";
import { renderApp } from "@/test/render";
import { ConnectionNotice } from "./ConnectionNotice";
import { NoCamera } from "./NoCamera";

const TITLE = "Thermocam found, but Studio can’t open it";

function setup(kind: "busy" | "permission", platform: "mac" | "windows" | "linux") {
  const onRetry = mock(() => {});
  const onUseDemo = mock(() => {});
  const onCopy = mock((_t: string) => {});
  const onOpenHelp = mock((_url: string) => {});
  const view = renderApp(
    <ConnectionNotice
      kind={kind}
      platform={platform}
      onRetry={onRetry}
      onUseDemo={onUseDemo}
      onCopy={onCopy}
      onOpenHelp={onOpenHelp}
    />,
  );
  return { ...view, onRetry, onUseDemo, onCopy, onOpenHelp };
}

describe("connection errors", () => {
  for (const platform of ["mac", "windows", "linux"] as const) {
    test(`busy on ${platform}: in use by another program, with Try again`, async () => {
      const { getByRole, getByText, user, onRetry, onUseDemo } = setup("busy", platform);
      expect(getByRole("heading", { name: TITLE })).toBeTruthy();
      expect(getByText(/It’s in use by another program\./)).toBeTruthy();
      expect(getByText(/Close the other program, then Try again\./)).toBeTruthy();
      await user.click(getByRole("button", { name: "Try again" }));
      expect(onRetry).toHaveBeenCalledTimes(1);
      await user.click(getByRole("button", { name: "Use the demo camera" }));
      expect(onUseDemo).toHaveBeenCalledTimes(1);
    });
  }

  test("permission on Linux shows the one-time udev fix with Copy", async () => {
    const { getByRole, getByText, user, onCopy, onRetry } = setup("permission", "linux");
    expect(getByRole("heading", { name: TITLE })).toBeTruthy();
    expect(
      getByText(
        "On Linux your user needs access to USB serial ports. Run this once, then log out and back in:",
      ),
    ).toBeTruthy();
    const code = getByText("sudo usermod -aG dialout $USER");
    await user.click(
      within(code.parentElement as HTMLElement).getByRole("button", { name: "Copy" }),
    );
    expect(onCopy).toHaveBeenCalledWith("sudo usermod -aG dialout $USER");
    await user.click(getByRole("button", { name: "Try again" }));
    expect(onRetry).toHaveBeenCalledTimes(1);
  });

  test("permission on Linux links to the manual's USB permissions section", async () => {
    const { getByRole, user, onOpenHelp } = setup("permission", "linux");
    await user.click(getByRole("button", { name: "More help in the manual" }));
    expect(onOpenHelp).toHaveBeenCalledWith(
      "https://www.diy-thermocam.net/docs/manual/#linux-usb-permissions",
    );
  });

  for (const platform of ["mac", "windows"] as const) {
    test(`permission on ${platform} has no Linux command`, () => {
      const { queryByText, getByRole } = setup("permission", platform);
      expect(getByRole("heading", { name: TITLE })).toBeTruthy();
      expect(queryByText("sudo usermod -aG dialout $USER")).toBeNull();
      expect(getByRole("button", { name: "Try again" })).toBeTruthy();
    });
  }
});

describe("Connect your Thermocam", () => {
  test("offers the demo camera and Open file…, and starts nothing by itself", async () => {
    const onTryDemo = mock(() => {});
    const onOpenFile = mock(() => {});
    const { getByRole, getByText, user } = renderApp(
      <NoCamera onTryDemo={onTryDemo} onOpenFile={onOpenFile} />,
    );
    expect(getByRole("heading", { name: "Connect your Thermocam" })).toBeTruthy();
    expect(
      getByText("Plug it in with a USB cable and switch it on. Studio finds it by itself."),
    ).toBeTruthy();
    expect(onTryDemo).not.toHaveBeenCalled();
    await user.click(getByRole("button", { name: "Try the demo camera" }));
    expect(onTryDemo).toHaveBeenCalledTimes(1);
    await user.click(getByRole("button", { name: "Open file…" }));
    expect(onOpenFile).toHaveBeenCalledTimes(1);
  });
});
