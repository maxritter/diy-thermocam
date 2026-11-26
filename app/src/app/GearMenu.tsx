import { Popover } from "@base-ui/react/popover";
import {
  Binary,
  FileImage,
  FolderOpen,
  Info,
  Keyboard,
  Monitor,
  Moon,
  Settings,
  Sun,
  SunMoon,
  Thermometer,
} from "lucide-react";
import type { StudioSettings, ThemeChoice } from "@/lib/settings";
import type { TempUnit } from "@/lib/types";
import { Segmented } from "./ui/Segmented";
import { SettingRow } from "./ui/SettingRow";
import { Switch } from "./ui/Switch";

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

const THEMES: { id: ThemeChoice; label: string; icon: typeof Sun }[] = [
  { id: "system", label: "System", icon: Monitor },
  { id: "light", label: "Light", icon: Sun },
  { id: "dark", label: "Dark", icon: Moon },
];

const MENU_ICON = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

export function GearMenu({
  settings,
  onChange,
  capturesFolder,
  onChangeCapturesFolder,
  onOpenShortcuts,
  onOpenAbout,
}: GearMenuProps): React.JSX.Element {
  return (
    <Popover.Root>
      <Popover.Trigger className="st-icon-btn" aria-label="Settings">
        <Settings size={16} strokeWidth={1.75} aria-hidden="true" />
      </Popover.Trigger>
      <Popover.Portal>
        <Popover.Positioner side="bottom" align="end" sideOffset={6} className="z-50">
          <Popover.Popup className="st-pop st-pop-set" aria-label="Settings">
            <div className="pop-rows">
              <SettingRow
                icon={Thermometer}
                title="Units"
                control={
                  <Segmented
                    label="Units"
                    small
                    value={settings.unit}
                    options={UNITS}
                    onChange={(unit) => onChange({ unit })}
                  />
                }
              />
              <SettingRow
                icon={SunMoon}
                title="Appearance"
                stacked
                control={
                  <Segmented
                    label="Appearance"
                    full
                    value={settings.theme}
                    options={THEMES}
                    onChange={(theme) => onChange({ theme })}
                  />
                }
              />
            </div>
            <hr />
            <div className="pop-rows">
              <SettingRow
                icon={Binary}
                title="Engineering details"
                desc="Raw values, sensor temperature and frame number in readings"
                control={
                  <Switch
                    label="Engineering details"
                    checked={settings.engineering_details}
                    onChange={(v) => onChange({ engineering_details: v })}
                  />
                }
              />
              <SettingRow
                icon={FileImage}
                title="Also save a PNG"
                desc="With every snapshot"
                control={
                  <Switch
                    label="Also save a PNG"
                    checked={settings.png_on_snapshot}
                    onChange={(v) => onChange({ png_on_snapshot: v })}
                  />
                }
              />
              <SettingRow
                icon={FolderOpen}
                title="Captures folder"
                desc={
                  <span className="st-path" title={capturesFolder}>
                    {capturesFolder}
                  </span>
                }
                control={
                  <button
                    className="tc-btn tc-btn-sm"
                    type="button"
                    aria-label="Change the Captures folder"
                    onClick={onChangeCapturesFolder}
                  >
                    Change…
                  </button>
                }
              />
            </div>
            <hr />
            <Popover.Close className="item" onClick={onOpenShortcuts}>
              <Keyboard {...MENU_ICON} className="item-ic" />
              Keyboard shortcuts<span className="end st-kbd">?</span>
            </Popover.Close>
            <Popover.Close className="item" onClick={onOpenAbout}>
              <Info {...MENU_ICON} className="item-ic" />
              About DIY-Thermocam Studio
            </Popover.Close>
          </Popover.Popup>
        </Popover.Positioner>
      </Popover.Portal>
    </Popover.Root>
  );
}
