import { Popover } from "@base-ui/react/popover";
import {
  Camera,
  ChevronDown,
  Clock,
  PanelRight,
  Pause,
  Play,
  Share,
  Square,
  Zap,
} from "lucide-react";
import { useState } from "react";
import type { CaptureActions } from "./captureActions";
import { type LiveConnection, useRecorder } from "./RecordingBar";

const ICON = { size: 16, strokeWidth: 1.75, "aria-hidden": true } as const;
const SM = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

/** The Record menu's intervals, in the order drawn (`live.html#menus`). */
export const TIMELAPSE_INTERVALS: { label: string; ms: number }[] = [
  { label: "5 seconds", ms: 5_000 },
  { label: "1 second", ms: 1_000 },
  { label: "2 seconds", ms: 2_000 },
  { label: "10 seconds", ms: 10_000 },
  { label: "30 seconds", ms: 30_000 },
  { label: "1 minute", ms: 60_000 },
];

export interface CaptureBarProps {
  actions: CaptureActions;
  streaming: boolean;
  connection: LiveConnection;
  panelHidden: boolean;
  onTogglePause: () => void;
  onCalibrate: () => void;
  onExport: () => void;
  onTogglePanel: () => void;
  /** Change… beside "Save to" in the Record menu: the Captures folder. */
  onChangeFolder?: () => void;
}

function RecordMenu({
  actions,
  disabled,
  onChangeFolder,
}: {
  actions: CaptureActions;
  disabled: boolean;
  onChangeFolder?: () => void;
}): React.JSX.Element {
  const [open, setOpen] = useState(false);
  const [intervalMs, setIntervalMs] = useState(TIMELAPSE_INTERVALS[0].ms);
  const sub = { display: "block", color: "var(--tc-text-muted)", fontSize: 12 } as const;
  return (
    <div className="st-splitbtn">
      <button
        className="tc-btn"
        type="button"
        disabled={disabled}
        onClick={() => void actions.recorder.startVideo()}
      >
        <span className="st-rec-dot" aria-hidden="true" />
        Record
      </button>
      <Popover.Root open={open} onOpenChange={setOpen}>
        <Popover.Trigger
          className="tc-btn"
          aria-label="Recording options"
          aria-haspopup="menu"
          disabled={disabled}
        >
          <ChevronDown {...SM} />
        </Popover.Trigger>
        <Popover.Portal>
          <Popover.Positioner side="bottom" align="end" sideOffset={6} className="z-50">
            <Popover.Popup className="st-pop" aria-label="Recording options">
              <button
                className="item"
                type="button"
                onClick={() => {
                  setOpen(false);
                  void actions.recorder.startVideo();
                }}
              >
                <span className="st-rec-dot" aria-hidden="true" />
                <span>
                  Record video
                  <span className="sub">
                    Every frame with full temperatures, for as long as there is disk space
                  </span>
                </span>
                <span className="end st-kbd">R</span>
              </button>
              <hr />
              <div className="pop-h">Time-lapse</div>
              <div className="pop-row">
                <span>One capture every</span>
                <select
                  className="st-select"
                  style={{ width: 118 }}
                  aria-label="Time-lapse interval"
                  value={intervalMs}
                  onChange={(e) => setIntervalMs(Number(e.target.value))}
                >
                  {TIMELAPSE_INTERVALS.map((o) => (
                    <option key={o.ms} value={o.ms}>
                      {o.label}
                    </option>
                  ))}
                </select>
              </div>
              <div className="pop-row">
                <span style={{ minWidth: 0 }}>
                  Save to
                  <span
                    style={{
                      ...sub,
                      overflow: "hidden",
                      textOverflow: "ellipsis",
                      whiteSpace: "nowrap",
                    }}
                  >
                    Captures (this computer)
                  </span>
                </span>
                {onChangeFolder ? (
                  <button
                    className="tc-btn tc-btn-sm"
                    type="button"
                    aria-label="Change the Captures folder"
                    onClick={() => {
                      setOpen(false);
                      onChangeFolder();
                    }}
                  >
                    Change…
                  </button>
                ) : null}
              </div>
              <div className="pop-row">
                <button
                  className="tc-btn tc-btn-sm"
                  type="button"
                  style={{ width: "100%" }}
                  onClick={() => {
                    setOpen(false);
                    void actions.recorder.startTimelapse(intervalMs);
                  }}
                >
                  <Clock {...SM} />
                  Start time-lapse
                </button>
              </div>
            </Popover.Popup>
          </Popover.Positioner>
        </Popover.Portal>
      </Popover.Root>
    </div>
  );
}

/** The right of the bar above the image: Pause, Calibrate, Snapshot, Record and Export. */
export function CaptureBar({
  actions,
  streaming,
  connection,
  panelHidden,
  onTogglePause,
  onCalibrate,
  onExport,
  onTogglePanel,
  onChangeFolder,
}: CaptureBarProps): React.JSX.Element | null {
  const rec = useRecorder(actions.recorder);
  if (connection === "none") return null;

  const recording = rec.mode === "recording" || (rec.mode === "starting" && !rec.timelapse);
  const timelapse = rec.mode === "timelapse" || (rec.mode === "starting" && rec.timelapse);
  const lost = connection === "lost";
  // The recording clock or time-lapse badge needs room: narrow bars then show icons only.
  const compact = rec.mode === "idle" ? "" : " lv-compact";

  const panelToggle = (
    <button
      className="st-icon-btn"
      type="button"
      aria-label={panelHidden ? "Show side panel" : "Hide side panel"}
      aria-pressed={panelHidden}
      title="Hide side panel · ⌘\"
      onClick={onTogglePanel}
    >
      <PanelRight {...ICON} />
    </button>
  );

  const stopButton = recording ? (
    <button
      className="tc-btn tc-btn-danger"
      type="button"
      disabled={rec.mode === "starting"}
      onClick={() => void actions.recorder.stop()}
    >
      <Square {...ICON} />
      Stop recording
    </button>
  ) : timelapse ? (
    <button
      className="tc-btn tc-btn-danger"
      type="button"
      disabled={rec.mode === "starting"}
      onClick={() => void actions.recorder.stop()}
    >
      <Square {...ICON} />
      Stop time-lapse
    </button>
  ) : null;

  return (
    <>
      {lost ? null : (
        <>
          {streaming ? (
            <button
              className={`tc-btn tc-btn-ghost${compact}`}
              type="button"
              aria-label="Pause"
              title="Pause · K"
              disabled={recording}
              onClick={onTogglePause}
            >
              <Pause {...ICON} />
              <span className="lv-lbl">Pause</span>
            </button>
          ) : (
            <button className="tc-btn" type="button" disabled={recording} onClick={onTogglePause}>
              <Play {...ICON} />
              Resume
            </button>
          )}
          <button
            className={`tc-btn tc-btn-ghost${compact}`}
            type="button"
            aria-label="Calibrate"
            disabled={recording}
            title="Closes the shutter for a moment to even out the image"
            onClick={onCalibrate}
          >
            <Zap {...ICON} />
            <span className="lv-lbl">Calibrate</span>
          </button>
          <span className="st-vsep" aria-hidden="true" />
        </>
      )}
      <button
        className="tc-btn tc-btn-primary"
        type="button"
        onClick={() => void actions.snapshot()}
      >
        <Camera {...ICON} />
        Snapshot
      </button>
      {stopButton ??
        (lost ? null : (
          <RecordMenu actions={actions} disabled={!streaming} onChangeFolder={onChangeFolder} />
        ))}
      <button
        className={`tc-btn${compact}`}
        type="button"
        aria-label="Export…"
        title="Export… · ⌘E"
        disabled={recording}
        onClick={onExport}
      >
        <Share {...ICON} />
        <span className="lv-lbl">Export…</span>
      </button>
      {panelToggle}
    </>
  );
}
