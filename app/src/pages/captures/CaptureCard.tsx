import { Check, Download, Film, FolderOpen, ImageOff, Monitor, Trash2 } from "lucide-react";
import type { MouseEvent } from "react";
import type { CaptureItem } from "@/lib/api/captures";

const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

export interface CaptureCardProps {
  item: CaptureItem;
  title: string;
  meta: string;
  /** The picture's URL; undefined while it loads, null when it could not be made. */
  picture: string | null | undefined;
  selected: boolean;
  /** Also on this computer (a camera capture copied before). */
  copied: boolean;
  /** The telling temperature: the hottest, or the coldest for a cold scene. */
  chip?: { text: string; hot: boolean } | null;
  /** Opens the capture, or selects with ⌘/Ctrl- and Shift-click. */
  onActivate: (e: MouseEvent) => void;
  onToggleSelect: (e: MouseEvent) => void;
  onCopy?: () => void;
  onReveal?: () => void;
  onDelete: () => void;
}

/**
 * One capture in the library grid (captures.html#library): the picture with its kind chip, a
 * checkbox and quick actions on hover, and the name over the date or size.
 */
export function CaptureCard({
  item,
  title,
  meta,
  picture,
  selected,
  copied,
  chip,
  onActivate,
  onToggleSelect,
  onCopy,
  onReveal,
  onDelete,
}: CaptureCardProps): React.JSX.Element {
  const recording = item.kind === "recording";
  return (
    <div className={`cp-cap${selected ? " sel" : ""}`} data-capture-id={item.id}>
      <div className="th">
        {picture === undefined ? (
          <div className="cp-skel" role="img" aria-label="Picture loading" />
        ) : picture === null ? (
          <div className="cp-nopic" role="img" aria-label="No picture">
            <ImageOff {...SM} />
            No picture
          </div>
        ) : (
          <img
            src={picture}
            alt={recording ? `Thermal recording ${title}` : `Thermal picture ${title}`}
            draggable={false}
          />
        )}
        <label
          className="check"
          onClick={(e) => {
            e.preventDefault();
            onToggleSelect(e);
          }}
        >
          <input
            className="st-vh"
            type="checkbox"
            checked={selected}
            readOnly
            aria-label={`Select ${title}`}
          />
          {selected ? <Check {...SM} /> : null}
        </label>
        <span className="acts">
          {onCopy ? (
            <button
              type="button"
              aria-label={`Copy ${title} to this computer`}
              title="Copy to this computer"
              onClick={onCopy}
            >
              <Download {...SM} />
            </button>
          ) : null}
          {onReveal ? (
            <button
              type="button"
              aria-label={`Show ${title} in its folder`}
              title={revealLabel()}
              onClick={onReveal}
            >
              <FolderOpen {...SM} />
            </button>
          ) : null}
          <button type="button" aria-label={`Delete ${title}`} title="Delete" onClick={onDelete}>
            <Trash2 {...SM} />
          </button>
        </span>
        {chip ? (
          <span className="maxchip" title={chip.hot ? "Hottest point" : "Coldest point"}>
            <span className={`st-tri ${chip.hot ? "hot" : "cold"}`} />
            {chip.text}
          </span>
        ) : null}
        {recording ? (
          <span className="kind">
            <Film {...SM} />
            Recording
          </span>
        ) : null}
        {copied ? (
          <span className="copied" title="Also on this computer">
            <Monitor {...SM} />
            Copied
          </span>
        ) : null}
      </div>
      <button className="cp-card-cap" type="button" onClick={onActivate}>
        <span className="t">{title}</span>
        <span className="m">{meta}</span>
      </button>
    </div>
  );
}

/** "Show in Finder" on a Mac, "Show in Explorer" on Windows, "Show in folder" elsewhere. */
export function revealLabel(): string {
  const ua = typeof navigator === "undefined" ? "" : navigator.userAgent;
  if (/Mac/i.test(ua)) return "Show in Finder";
  if (/Windows/i.test(ua)) return "Show in Explorer";
  return "Show in folder";
}
