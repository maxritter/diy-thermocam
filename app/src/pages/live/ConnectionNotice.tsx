import { Copy, TriangleAlert } from "lucide-react";
import type { Platform } from "@/app/Toasts";

export const UDEV_COMMAND = "sudo usermod -aG dialout $USER";

export interface ConnectionNoticeProps {
  kind: "busy" | "permission";
  platform: Platform;
  onRetry: () => void;
  onUseDemo: () => void;
  onCopy: (text: string) => void;
}

/**
 * "Thermocam found, but Studio can’t open it" (`live.html#states`): another program holds the
 * port, or the system refused it (on Linux with the one-time fix).
 */
export function ConnectionNotice({
  kind,
  platform,
  onRetry,
  onUseDemo,
  onCopy,
}: ConnectionNoticeProps): React.JSX.Element {
  return (
    <div className="lv-empty" role="alert">
      <div className="ring warn">
        <TriangleAlert size={28} strokeWidth={1.75} aria-hidden="true" />
      </div>
      <h2>Thermocam found, but Studio can’t open it</h2>
      {kind === "busy" ? (
        <p>
          It’s in use by another program.
          <br />
          Close the other program, then Try again.
        </p>
      ) : platform === "linux" ? (
        <>
          <p>
            On Linux your user needs access to USB serial ports. Run this once, then log out and
            back in:
          </p>
          <div className="lv-codebox">
            <span>{UDEV_COMMAND}</span>
            <button className="tc-btn tc-btn-sm" type="button" onClick={() => onCopy(UDEV_COMMAND)}>
              <Copy size={14} strokeWidth={1.75} aria-hidden="true" />
              Copy
            </button>
          </div>
        </>
      ) : (
        <p>
          This computer didn’t let Studio open its port.
          <br />
          Unplug the camera, plug it back in, then Try again.
        </p>
      )}
      <div className="acts">
        <button className="tc-btn tc-btn-primary" type="button" onClick={onRetry}>
          Try again
        </button>
        <button className="tc-btn" type="button" onClick={onUseDemo}>
          Use the demo camera
        </button>
      </div>
    </div>
  );
}
