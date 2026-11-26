import { Dialog } from "@base-ui/react/dialog";
import { Clock, Cpu, Images, Radio, X } from "lucide-react";
import { type ReactNode, useEffect, useMemo, useRef, useState } from "react";
import { BrandMark } from "@/components/BrandMark";
import { isTauriRuntime } from "@/lib/backend";
import type { StudioSettings } from "@/lib/settings";
import { CameraChip, type CameraChipProps, cameraTitle } from "./CameraChip";
import { GearMenu } from "./GearMenu";
import { ShortcutSheet } from "./ShortcutSheet";
import { isMacPlatform, isTypingTarget, matchShortcut, type ShortcutId } from "./shortcuts";
import { PanelWidthContext, type PanelWidthValue } from "./ui/SidePanel";
import { applyWindowTitle, windowTitle } from "./windowTitle";

export type PageId = "live" | "captures" | "camera";

export const PAGES: { id: PageId; label: string; icon: typeof Radio }[] = [
  { id: "live", label: "Live", icon: Radio },
  { id: "captures", label: "Captures", icon: Images },
  { id: "camera", label: "Camera", icon: Cpu },
];

const PAGE_SHORTCUTS: Partial<Record<ShortcutId, PageId>> = {
  "page-live": "live",
  "page-captures": "captures",
  "page-camera": "camera",
};

export interface ShellProps {
  page: PageId;
  onPageChange: (page: PageId) => void;
  camera: CameraChipProps;
  settings: StudioSettings;
  onSettingsChange: (patch: Partial<StudioSettings>) => void;
  /** Folder label such as "Pictures › Thermocam". */
  capturesFolder: string;
  onChangeCapturesFolder: () => void;
  /** Shortcuts the shell does not handle itself (everything but pages and this list). */
  onShortcut: (id: ShortcutId) => void;
  /** Firmware of the connected camera, for About. */
  firmware: string | null;
  /** The Live tab's mark: a recording, a time-lapse, or an alarm past its limit. */
  liveBadge?: "rec" | "timelapse" | "alarm" | null;
  children: ReactNode;
}

function LiveBadge({ kind }: { kind: "rec" | "timelapse" | "alarm" }): React.JSX.Element {
  if (kind === "timelapse") {
    return (
      <Clock
        size={13}
        strokeWidth={1.75}
        className="lv-nav-clock"
        role="img"
        aria-label="Time-lapse"
      />
    );
  }
  return (
    <span
      className={`lv-nav-dot ${kind}`}
      role="img"
      aria-label={kind === "rec" ? "Recording" : "Alarm"}
    />
  );
}

function AboutDialog({
  open,
  onOpenChange,
  firmware,
}: {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  firmware: string | null;
}): React.JSX.Element {
  const [version, setVersion] = useState<string | null>(null);
  useEffect(() => {
    if (!open || !isTauriRuntime()) return;
    let active = true;
    void import("@tauri-apps/api/app")
      .then((m) => m.getVersion())
      .then((v) => {
        if (active) setVersion(v);
      })
      .catch(() => {});
    return () => {
      active = false;
    };
  }, [open]);

  return (
    <Dialog.Root open={open} onOpenChange={onOpenChange}>
      <Dialog.Portal>
        <Dialog.Backdrop className="st-scrim z-50" />
        <Dialog.Popup className="st-dialog z-50" style={{ width: 440 }}>
          <div className="dlg-h">
            <Dialog.Title>About DIY-Thermocam Studio</Dialog.Title>
            <Dialog.Close className="st-icon-btn" aria-label="Close">
              <X size={16} strokeWidth={1.75} aria-hidden="true" />
            </Dialog.Close>
          </div>
          <div className="dlg-b" style={{ gap: 10 }}>
            <div className="st-brand" style={{ fontSize: 14 }}>
              <BrandMark size={32} />
              DIY-Thermocam Studio
            </div>
            <div style={{ color: "var(--tc-text-muted)" }}>
              {version ? `Version ${version}` : "Browser preview"}
              {firmware ? ` · camera firmware ${firmware}` : ""}
            </div>
            <div style={{ color: "var(--tc-text-muted)" }}>www.diy-thermocam.net</div>
          </div>
        </Dialog.Popup>
      </Dialog.Portal>
    </Dialog.Root>
  );
}

/** The header with Live · Captures · Camera, the camera chip and the gear, above one page. */
export function Shell({
  page,
  onPageChange,
  camera,
  settings,
  onSettingsChange,
  capturesFolder,
  onChangeCapturesFolder,
  onShortcut,
  firmware,
  liveBadge = null,
  children,
}: ShellProps): React.JSX.Element {
  const [sheetOpen, setSheetOpen] = useState(false);
  const [aboutOpen, setAboutOpen] = useState(false);
  const handlers = useRef({ onPageChange, onShortcut, onSettings: onSettingsChange });
  handlers.current = { onPageChange, onShortcut, onSettings: onSettingsChange };

  useEffect(() => {
    const onKeyDown = (e: KeyboardEvent) => {
      if (e.defaultPrevented || e.isComposing || isTypingTarget(e.target)) return;
      const id = matchShortcut(e);
      if (!id) return;
      const target = e.target instanceof Element ? e.target : null;
      // Space and Enter on a focused control belong to that control.
      if (id === "snapshot" && target?.closest("button, a, summary, [role]")) return;
      // While a menu or dialog is open, only Escape and the page keys pass through.
      const overlay = document.querySelector("[role=dialog], [role=menu]");
      if (overlay && id !== "cancel" && !PAGE_SHORTCUTS[id]) return;

      const toPage = PAGE_SHORTCUTS[id];
      e.preventDefault();
      if (toPage) handlers.current.onPageChange(toPage);
      else if (id === "help") setSheetOpen(true);
      else handlers.current.onShortcut(id);
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, []);

  const current = PAGES.find((p) => p.id === page) ?? PAGES[0];
  const cameraName = cameraTitle(camera.state);
  useEffect(() => {
    void applyWindowTitle(windowTitle(current.label, cameraName));
  }, [current.label, cameraName]);
  // On a Mac the window buttons sit inside this header (titleBarStyle "Overlay").
  const macTitleBar = isTauriRuntime() && isMacPlatform();
  const panelWidth = settings.panel_width;
  const panel = useMemo<PanelWidthValue>(
    () => ({ width: panelWidth, onChange: (w) => handlers.current.onSettings({ panel_width: w }) }),
    [panelWidth],
  );

  return (
    <div className="st-shell">
      <header className={macTitleBar ? "st-hdr st-hdr-mac" : "st-hdr"} data-tauri-drag-region>
        <div className="st-brand" data-tauri-drag-region>
          <BrandMark size={22} />
          DIY-Thermocam Studio
        </div>
        <nav className="st-nav" aria-label="Studio">
          {PAGES.map((p) => {
            const Icon = p.icon;
            return (
              <button
                key={p.id}
                type="button"
                aria-current={p.id === page ? "page" : undefined}
                onClick={() => onPageChange(p.id)}
              >
                <Icon size={16} strokeWidth={1.75} aria-hidden="true" />
                {p.label}
                {p.id === "live" && liveBadge ? <LiveBadge kind={liveBadge} /> : null}
              </button>
            );
          })}
        </nav>
        <div className="st-hdr-right">
          <CameraChip {...camera} />
          <GearMenu
            settings={settings}
            onChange={onSettingsChange}
            capturesFolder={capturesFolder}
            onChangeCapturesFolder={onChangeCapturesFolder}
            onOpenShortcuts={() => setSheetOpen(true)}
            onOpenAbout={() => setAboutOpen(true)}
          />
        </div>
      </header>
      <main className="st-page" data-page={current.id} aria-label={current.label}>
        <PanelWidthContext.Provider value={panel}>{children}</PanelWidthContext.Provider>
      </main>
      <ShortcutSheet open={sheetOpen} onOpenChange={setSheetOpen} />
      <AboutDialog open={aboutOpen} onOpenChange={setAboutOpen} firmware={firmware} />
    </div>
  );
}
