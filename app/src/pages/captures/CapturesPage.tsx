import { ChevronLeft, FolderOpen } from "lucide-react";
import type { ReactNode } from "react";

export interface CapturesPageProps {
  /** An opened capture shows in the viewer; otherwise the camera's card is listed. */
  opened: boolean;
  openedName: string | null;
  onBack: () => void;
  onOpenFile: () => void;
  /** The old SD card browser, until slice 6 replaces it. */
  library: ReactNode;
  /** The old editor: viewer, color scale and its panel, until slice 7 replaces it. */
  editor: ReactNode;
}

export function CapturesPage({
  opened,
  openedName,
  onBack,
  onOpenFile,
  library,
  editor,
}: CapturesPageProps): React.JSX.Element {
  return (
    <div className="flex h-full min-h-0 flex-col">
      <div className="flex h-[52px] shrink-0 items-center justify-between gap-3 border-b border-[var(--tc-line)] px-5">
        {opened ? (
          <div className="flex min-w-0 items-center gap-2.5">
            <button className="tc-btn tc-btn-ghost" type="button" onClick={onBack}>
              <ChevronLeft size={16} strokeWidth={1.75} aria-hidden="true" />
              Captures
            </button>
            <span className="truncate font-semibold">{openedName}</span>
          </div>
        ) : (
          <span className="font-semibold">Camera</span>
        )}
        <button className="tc-btn" type="button" onClick={onOpenFile}>
          <FolderOpen size={16} strokeWidth={1.75} aria-hidden="true" />
          Open file…
        </button>
      </div>
      <div className="min-h-0 flex-1 overflow-hidden">{opened ? editor : library}</div>
    </div>
  );
}
