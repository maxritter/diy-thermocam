import { LoaderCircle, Unplug, Upload } from "lucide-react";

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
  const Icon = connecting ? LoaderCircle : Unplug;
  return (
    <section className="st-gcard cm-empty" aria-labelledby="cm-nocam-t">
      <span className="cm-empty-ic">
        <Icon
          size={18}
          strokeWidth={1.75}
          aria-hidden="true"
          className={connecting ? "cm-spin" : undefined}
        />
      </span>
      <div className="cm-empty-txt">
        <span className="t" id="cm-nocam-t">
          {connecting ? "Connecting…" : "No camera connected"}
        </span>
        <span className="d">Plug in a Thermocam to see its status and settings.</span>
      </div>
      <div className="acts">
        <button type="button" className="tc-btn" onClick={onUseDemo} disabled={connecting}>
          Use the demo camera
        </button>
        <button
          type="button"
          className="tc-btn tc-btn-ghost"
          onClick={onUpdateFirmware}
          disabled={updateBusy}
        >
          <Upload size={15} strokeWidth={1.75} aria-hidden="true" />
          Update firmware…
        </button>
      </div>
    </section>
  );
}
