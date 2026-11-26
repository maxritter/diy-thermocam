import { describe, expect, mock, test } from "bun:test";
import { screen } from "@testing-library/react";
import { renderApp } from "@/test/render";
import { DeleteDialog, deleteText } from "./DeleteDialog";

describe("the Delete dialog", () => {
  test("names the count and moves to the Trash only after confirming", async () => {
    const onConfirm = mock(async () => {});
    const onCancel = mock(() => {});
    const { user } = renderApp(
      <DeleteDialog
        open
        where="computer"
        names={["PCB load", "25 Nov 2026 · 10:20:14"]}
        onConfirm={onConfirm}
        onCancel={onCancel}
      />,
    );
    const dialog = screen.getByRole("alertdialog");
    expect(dialog.textContent).toContain("2 captures");
    expect(dialog.textContent).toContain("PCB load");
    expect(onConfirm).not.toHaveBeenCalled();
    await user.click(screen.getByRole("button", { name: "Cancel" }));
    expect(onCancel).toHaveBeenCalledTimes(1);
    expect(onConfirm).not.toHaveBeenCalled();
  });

  test("confirming calls once", async () => {
    const onConfirm = mock(async () => {});
    const { user } = renderApp(
      <DeleteDialog
        open
        where="camera"
        names={["PCB load", "25 Nov 2026 · 10:20:14"]}
        onConfirm={onConfirm}
        onCancel={() => {}}
      />,
    );
    expect(screen.getByText("Delete 2 captures from the camera?")).toBeTruthy();
    await user.click(screen.getByRole("button", { name: "Delete 2 captures" }));
    expect(onConfirm).toHaveBeenCalledTimes(1);
  });

  test.each([
    [["A"], "camera", "Delete 1 capture from the camera?", "A will be removed from the SD card."],
    [["A", "B"], "camera", "Delete 2 captures from the camera?", "A and B will be removed"],
    [["A", "B", "C"], "camera", "Delete 3 captures from the camera?", "A, B and C will be removed"],
    [["A", "B", "C", "D", "E"], "camera", "Delete 5 captures from the camera?", "A, B and 3 more"],
    [
      ["A", "B"],
      "computer",
      "Move 2 captures to the Trash?",
      "A and B will be moved to the Trash with their edits.",
    ],
  ] as const)("%p on the %s", (names, where, title, body) => {
    const t = deleteText([...names], where, "mac");
    expect(t.title).toBe(title);
    expect(t.body).toContain(body);
  });

  test("Windows says Recycle Bin", () => {
    expect(deleteText(["A"], "computer", "windows").title).toBe(
      "Move 1 capture to the Recycle Bin?",
    );
  });
});
