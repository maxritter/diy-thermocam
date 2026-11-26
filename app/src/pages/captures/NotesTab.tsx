import { Check, RefreshCw } from "lucide-react";
import { Fragment } from "react";
import type { SaveState } from "./edits";

const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

function UndoIcon() {
  return (
    <svg
      width={16}
      height={16}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={1.75}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M9 14 4 9l5-5" />
      <path d="M4 9h10.5a5.5 5.5 0 0 1 5.5 5.5a5.5 5.5 0 0 1-5.5 5.5H11" />
    </svg>
  );
}

function RedoIcon() {
  return (
    <svg
      width={16}
      height={16}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={1.75}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="m15 14 5-5-5-5" />
      <path d="M20 9H9.5A5.5 5.5 0 0 0 4 14.5a5.5 5.5 0 0 0 5.5 5.5H13" />
    </svg>
  );
}

/** The line beside Undo and Redo that says where the edits stand. */
export function SavedLine({ save }: { save: SaveState }): React.JSX.Element {
  switch (save.kind) {
    case "saved":
      return (
        <p className="cp-saved" role="status">
          <Check {...SM} className="ok" />
          Saved · the original file is never changed
        </p>
      );
    case "saving":
      return (
        <p className="cp-saved" role="status">
          Saving…
        </p>
      );
    case "error":
      return (
        <p className="cp-saved" role="status">
          <span className="err">Couldn’t save the edits: {save.detail}</span>
        </p>
      );
    default:
      return (
        <p className="cp-saved" role="status">
          Edits save by themselves · the original file is never changed
        </p>
      );
  }
}

export interface NotesTabProps {
  name: string;
  text: string;
  /** The date-time name the capture shows when no name is typed. */
  placeholder: string;
  onName: (name: string) => void;
  onText: (text: string) => void;
  save: SaveState;
  canUndo: boolean;
  canRedo: boolean;
  onUndo: () => void;
  onRedo: () => void;
  onReset: () => void;
  details: { k: string; v: string; mono?: boolean }[];
  rows?: number;
}

/** Notes (captures.html#opened): name, notes, the save line with Undo and Redo, and details. */
export function NotesTab(p: NotesTabProps): React.JSX.Element {
  return (
    <>
      <div className="st-grp">
        <label className="st-field">
          <span>Name</span>
          <input
            className="st-input"
            value={p.name}
            placeholder={p.placeholder}
            onChange={(e) => p.onName(e.target.value)}
          />
        </label>
        <label className="st-field">
          <span>Notes</span>
          <textarea
            className="st-input cp-textarea"
            rows={p.rows ?? 6}
            value={p.text}
            onChange={(e) => p.onText(e.target.value)}
          />
        </label>
        <div style={{ display: "flex", gap: 6, alignItems: "center" }}>
          <SavedLine save={p.save} />
          <button
            className="st-icon-btn"
            type="button"
            aria-label="Undo"
            title="Undo · ⌘Z"
            disabled={!p.canUndo}
            onClick={p.onUndo}
          >
            <UndoIcon />
          </button>
          <button
            className="st-icon-btn"
            type="button"
            aria-label="Redo"
            title="Redo · ⇧⌘Z"
            disabled={!p.canRedo}
            onClick={p.onRedo}
          >
            <RedoIcon />
          </button>
        </div>
        <p className="st-hint">
          The name you type shows in Captures and as the title of exports and reports. Left empty,
          the capture is named by when it was taken.
        </p>
        <div>
          <button className="st-link" type="button" onClick={p.onReset}>
            <RefreshCw {...SM} />
            Reset to original
          </button>
        </div>
      </div>
      <div className="st-grp">
        <div className="st-grp-h">
          <h3>Details</h3>
        </div>
        <dl className="cp-kv">
          {p.details.map((d) => (
            <Fragment key={d.k}>
              <dt>{d.k}</dt>
              <dd className={d.mono ? "mono" : undefined}>{d.v}</dd>
            </Fragment>
          ))}
        </dl>
      </div>
    </>
  );
}
