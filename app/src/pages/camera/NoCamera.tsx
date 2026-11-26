import { Upload } from "lucide-react";

export interface NoCameraProps {
  /** Studio is opening a camera right now. */
  connecting?: boolean;
  onUpdateFirmware: () => void;
  onUseDemo: () => void;
  /** The firmware dialog is busy with an update, so it cannot open again. */
  updateBusy?: boolean;
}

/** `camera.html#no-camera`: settings need a camera, a firmware update does not. */
export function NoCamera({
  connecting = false,
  onUpdateFirmware,
  onUseDemo,
  updateBusy = false,
}: NoCameraProps): React.JSX.Element {
  return (
    <section className="cm-panel cm-empty" aria-labelledby="cm-nocam-t">
      <span className="t" id="cm-nocam-t">
        <span className="st-status-dot off" />
        {connecting ? "Connecting…" : "No camera connected"}
      </span>
      <span className="d">
        Plug in a Thermocam to see its status and settings. With the demo camera, this page shows
        its values read-only.
      </span>
      <div className="acts">
        <button type="button" className="tc-btn" onClick={onUpdateFirmware} disabled={updateBusy}>
          <Upload size={16} strokeWidth={1.75} aria-hidden="true" />
          Update firmware…
        </button>
        <button
          type="button"
          className="tc-btn tc-btn-ghost"
          onClick={onUseDemo}
          disabled={connecting}
        >
          Use the demo camera
        </button>
      </div>
    </section>
  );
}
