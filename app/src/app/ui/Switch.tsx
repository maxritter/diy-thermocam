import { useSettingRowIds } from "./SettingRow";

export interface SwitchProps {
  checked: boolean;
  onChange: (next: boolean) => void;
  /** The accessible name when the switch stands outside a SettingRow (a row's title wins). */
  label: string;
  disabled?: boolean;
}

/** The on/off switch (`.st-switch`). Inside a SettingRow it is named by the row's title. */
export function Switch({ checked, onChange, label, disabled }: SwitchProps): React.JSX.Element {
  const row = useSettingRowIds();
  return (
    <label className="st-switch">
      <input
        type="checkbox"
        role="switch"
        aria-label={label}
        aria-labelledby={row?.labelId}
        aria-describedby={row?.descId}
        aria-checked={checked}
        checked={checked}
        disabled={disabled}
        onChange={(e) => onChange(e.target.checked)}
      />
      <span className="track" />
    </label>
  );
}
