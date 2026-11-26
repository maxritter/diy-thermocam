import { AlertDialog } from "@base-ui/react/alert-dialog";
import { Fragment, useState } from "react";
import type { Where } from "@/lib/api/captures";

export type Platform = "mac" | "windows" | "other";

export function currentPlatform(): Platform {
  const ua = typeof navigator === "undefined" ? "" : navigator.userAgent;
  if (/Windows/i.test(ua)) return "windows";
  if (/Mac/i.test(ua)) return "mac";
  return "other";
}

/** "A", "A and B", "A, B and C", "A, B and 3 more". */
function joinNames(names: string[]): string[] {
  if (names.length <= 3) return names;
  return [names[0], names[1], `${names.length - 2} more`];
}

function listText(parts: string[]): string {
  if (parts.length <= 1) return parts.join("");
  return `${parts.slice(0, -1).join(", ")} and ${parts[parts.length - 1]}`;
}

/** The dialog's words (captures.html#delete), for the camera's card or this computer. */
export function deleteText(
  names: string[],
  where: Where,
  platform: Platform = currentPlatform(),
): { title: string; body: string; confirm: string; parts: string[] } {
  const n = names.length;
  const count = `${n} ${n === 1 ? "capture" : "captures"}`;
  const parts = joinNames(names);
  if (where === "camera") {
    return {
      title: `Delete ${count} from the camera?`,
      body: `${listText(parts)} will be removed from the SD card. This can’t be undone. Captures already copied to this computer stay.`,
      confirm: `Delete ${count}`,
      parts,
    };
  }
  const bin = platform === "windows" ? "Recycle Bin" : "Trash";
  return {
    title: `Move ${count} to the ${bin}?`,
    body: `${listText(parts)} will be moved to the ${bin}. Their edits go with them, and you can put them back from there.`,
    confirm: `Move to ${bin}`,
    parts,
  };
}

export interface DeleteDialogProps {
  open: boolean;
  where: Where;
  /** The names shown on the cards, in the order shown. */
  names: string[];
  onConfirm: () => Promise<void>;
  onCancel: () => void;
}

/** Delete asks first and names what goes (captures.html#delete). */
export function DeleteDialog({
  open,
  where,
  names,
  onConfirm,
  onCancel,
}: DeleteDialogProps): React.JSX.Element {
  const [busy, setBusy] = useState(false);
  const t = deleteText(names, where);
  const named = new Set(names.slice(0, 3));
  return (
    <AlertDialog.Root
      open={open}
      onOpenChange={(next) => {
        if (!next && !busy) onCancel();
      }}
    >
      <AlertDialog.Portal>
        <AlertDialog.Backdrop className="st-scrim z-50" />
        <AlertDialog.Popup className="st-dialog z-50" style={{ width: 440 }}>
          <div className="dlg-h">
            <AlertDialog.Title>{t.title}</AlertDialog.Title>
          </div>
          <div className="dlg-b">
            <AlertDialog.Description style={{ margin: 0, color: "var(--tc-text-muted)" }}>
              {t.parts.map((p, i) => (
                <Fragment key={p}>
                  {i > 0 ? (i === t.parts.length - 1 ? " and " : ", ") : null}
                  {named.has(p) ? (
                    <strong style={{ color: "var(--tc-text)", fontWeight: 550 }}>{p}</strong>
                  ) : (
                    p
                  )}
                </Fragment>
              ))}
              {t.body.slice(listText(t.parts).length)}
            </AlertDialog.Description>
          </div>
          <div className="dlg-f" style={{ justifyContent: "flex-end" }}>
            <AlertDialog.Close className="tc-btn tc-btn-ghost" disabled={busy}>
              Cancel
            </AlertDialog.Close>
            <button
              className="tc-btn cp-btn-danger"
              type="button"
              disabled={busy}
              onClick={async () => {
                setBusy(true);
                try {
                  await onConfirm();
                } finally {
                  setBusy(false);
                }
              }}
            >
              {t.confirm}
            </button>
          </div>
        </AlertDialog.Popup>
      </AlertDialog.Portal>
    </AlertDialog.Root>
  );
}
