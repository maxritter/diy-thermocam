import { Dialog } from "@base-ui/react/dialog";
import { X } from "lucide-react";
import { Fragment } from "react";

export interface ShortcutSheetProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
}

/** Rows in reading order: the sheet's grid fills left column, right column, left, right… */
const ROWS: [string, string[]][] = [
  ["Select", ["V"]],
  ["Snapshot", ["Space"]],
  ["Spot · Box · Circle", ["S", "B", "C"]],
  ["Record / stop", ["R"]],
  ["Polygon · Line · Difference", ["P", "L", "D"]],
  ["Pause / resume", ["K"]],
  ["Hottest and coldest", ["H"]],
  ["Export", ["⌘E"]],
  ["Magnifier", ["M"]],
  ["Copy image", ["⌘C"]],
  ["Auto range", ["A"]],
  ["Hide side panel", ["⌘\\"]],
  ["Zoom · fit", ["+", "−", "0"]],
  ["Live · Captures · Camera", ["⌘1", "⌘2", "⌘3"]],
  ["Delete region", ["⌫"]],
  ["Previous · next capture", ["←", "→"]],
  ["Grid readings", ["G"]],
  ["Undo · redo", ["⌘Z", "⇧⌘Z"]],
  ["Cancel drawing", ["Esc"]],
  ["This list", ["?"]],
];

export function ShortcutSheet({ open, onOpenChange }: ShortcutSheetProps): React.JSX.Element {
  return (
    <Dialog.Root open={open} onOpenChange={onOpenChange}>
      <Dialog.Portal>
        <Dialog.Backdrop className="st-scrim z-50" />
        <Dialog.Popup className="st-dialog z-50" style={{ width: 520 }}>
          <div className="dlg-h">
            <Dialog.Title>Keyboard shortcuts</Dialog.Title>
            <Dialog.Close className="st-icon-btn" aria-label="Close">
              <X size={16} strokeWidth={1.75} aria-hidden="true" />
            </Dialog.Close>
          </div>
          <div
            className="dlg-b"
            style={{ gridTemplateColumns: "1fr 1fr", gap: "6px 28px", fontSize: 13 }}
          >
            {ROWS.map(([label, keys]) => (
              <div key={label} className="st-setrow" style={{ minHeight: 26 }}>
                <span>{label}</span>
                {keys.length === 1 ? (
                  <span className="st-kbd">{keys[0]}</span>
                ) : (
                  <span>
                    {keys.map((k, i) => (
                      <Fragment key={k}>
                        {i > 0 ? " " : null}
                        <span className="st-kbd">{k}</span>
                      </Fragment>
                    ))}
                  </span>
                )}
              </div>
            ))}
          </div>
          <div className="dlg-f">
            <span className="muted">On Windows and Linux, Ctrl takes the place of ⌘.</span>
            <span />
          </div>
        </Dialog.Popup>
      </Dialog.Portal>
    </Dialog.Root>
  );
}
