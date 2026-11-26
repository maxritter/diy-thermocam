import { FolderOpen, Usb } from "lucide-react";

/** First start without a camera (`live.html#no-camera`): nothing starts by itself. */
export function NoCamera({
  onTryDemo,
  onOpenFile,
}: {
  onTryDemo: () => void;
  onOpenFile: () => void;
}): React.JSX.Element {
  return (
    <div className="lv-empty">
      <div className="ring">
        <Usb size={30} strokeWidth={1.5} aria-hidden="true" />
      </div>
      <h2>Connect your Thermocam</h2>
      <p>Plug it in with a USB cable and switch it on. Studio finds it by itself.</p>
      <div className="acts">
        <button className="tc-btn tc-btn-primary" type="button" onClick={onTryDemo}>
          Try the demo camera
        </button>
        <button className="tc-btn" type="button" onClick={onOpenFile}>
          <FolderOpen size={16} strokeWidth={1.75} aria-hidden="true" />
          Open file…
        </button>
      </div>
    </div>
  );
}
