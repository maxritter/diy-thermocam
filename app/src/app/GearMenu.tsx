import { Popover } from "@base-ui/react/popover";
import { Settings } from "lucide-react";
import type { StudioSettings, ThemeChoice } from "@/lib/settings";
import type { TempUnit } from "@/lib/types";

export interface GearMenuProps {
  settings: StudioSettings;
  onChange: (patch: Partial<StudioSettings>) => void;
  /** Folder label such as "Pictures › Thermocam". */
  capturesFolder: string;
  onChangeCapturesFolder: () => void;
  onOpenShortcuts: () => void;
  onOpenAbout: () => void;
}

const UNITS: { id: TempUnit; label: string }[] = [
  { id: "celsius", label: "°C" },
  { id: "fahrenheit", label: "°F" },
  { id: "kelvin", label: "K" },
];

const THEMES: { id: ThemeChoice; label: string }[] = [
  { id: "system", label: "System" },
  { id: "light", label: "Light" },
  { id: "dark", label: "Dark" },
];

function Switch({
  checked,
  onChange,
  label,
}: {
  checked: boolean;
  onChange: (next: boolean) => void;
  label: string;
}): React.JSX.Element {
  return (
    <label className="st-switch" style={{ marginTop: 2 }}>
      <input
        type="checkbox"
        role="switch"
        aria-label={label}
        aria-checked={checked}
        checked={checked}
        onChange={(e) => onChange(e.target.checked)}
      />
      <span className="track" />
    </label>
  );
}

export function GearMenu({
  settings,
  onChange,
  capturesFolder,
  onChangeCapturesFolder,
  onOpenShortcuts,
  onOpenAbout,
}: GearMenuProps): React.JSX.Element {
  const subStyle = { display: "block", color: "var(--tc-text-muted)", fontSize: 12 } as const;

  return (
    <Popover.Root>
      <Popover.Trigger className="st-icon-btn" aria-label="Settings">
        <Settings size={16} strokeWidth={1.75} aria-hidden="true" />
      </Popover.Trigger>
      <Popover.Portal>
        <Popover.Positioner side="bottom" align="end" sideOffset={6} className="z-50">
          <Popover.Popup className="st-pop" aria-label="Settings">
            <div className="pop-h">Units</div>
            <div className="pop-row">
              <fieldset className="st-seg full" aria-label="Units">
                {UNITS.map((u) => (
                  <button
                    key={u.id}
                    type="button"
                    aria-pressed={settings.unit === u.id}
                    onClick={() => onChange({ unit: u.id })}
                  >
                    {u.label}
                  </button>
                ))}
              </fieldset>
            </div>
            <div className="pop-h">Appearance</div>
            <div className="pop-row">
              <fieldset className="st-seg full" aria-label="Appearance">
                {THEMES.map((t) => (
                  <button
                    key={t.id}
                    type="button"
                    aria-pressed={settings.theme === t.id}
                    onClick={() => onChange({ theme: t.id })}
                  >
                    {t.label}
                  </button>
                ))}
              </fieldset>
            </div>
            <hr />
            <div className="pop-row" style={{ alignItems: "flex-start" }}>
              <span>
                Engineering details
                <span style={subStyle}>
                  Raw sensor values, sensor temperature and frame number in readings
                </span>
              </span>
              <Switch
                label="Engineering details"
                checked={settings.engineering_details}
                onChange={(v) => onChange({ engineering_details: v })}
              />
            </div>
            <div className="pop-row" style={{ alignItems: "flex-start" }}>
              <span>Also save a PNG with every snapshot</span>
              <Switch
                label="Also save a PNG with every snapshot"
                checked={settings.png_on_snapshot}
                onChange={(v) => onChange({ png_on_snapshot: v })}
              />
            </div>
            <div className="pop-row">
              <span style={{ minWidth: 0 }}>
                Captures folder
                <span
                  style={{
                    ...subStyle,
                    overflow: "hidden",
                    textOverflow: "ellipsis",
                    whiteSpace: "nowrap",
                  }}
                >
                  {capturesFolder}
                </span>
              </span>
              <button
                className="tc-btn tc-btn-sm"
                type="button"
                aria-label="Change the Captures folder"
                onClick={onChangeCapturesFolder}
              >
                Change…
              </button>
            </div>
            <hr />
            <Popover.Close className="item" onClick={onOpenShortcuts}>
              Keyboard shortcuts<span className="end st-kbd">?</span>
            </Popover.Close>
            <Popover.Close className="item" onClick={onOpenAbout}>
              About DIY-Thermocam Studio
            </Popover.Close>
          </Popover.Popup>
        </Popover.Positioner>
      </Popover.Portal>
    </Popover.Root>
  );
}
