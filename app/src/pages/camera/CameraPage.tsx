import type { ReactNode } from "react";

export interface CameraPageProps {
  /** The old camera controls, until slice 8 replaces them. */
  controls: ReactNode;
  /** The old firmware updater, until slice 8 replaces it. */
  updater: ReactNode;
}

export function CameraPage({ controls, updater }: CameraPageProps): React.JSX.Element {
  return (
    <div className="h-full min-h-0 overflow-y-auto">
      <section aria-label="Camera settings">{controls}</section>
      <section aria-label="Firmware update" className="border-t border-[var(--tc-line)]">
        {updater}
      </section>
    </div>
  );
}
