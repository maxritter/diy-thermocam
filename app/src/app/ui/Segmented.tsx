import type { LucideIcon } from "lucide-react";
import { useSettingRowIds } from "./SettingRow";

export interface SegmentedOption<T extends string | number> {
  id: T;
  label: string;
  /** For options that differ in kind; 13 px, dim until chosen. */
  icon?: LucideIcon;
  /** A hover title with what the option means. */
  title?: string;
  disabled?: boolean;
}

export interface SegmentedProps<T extends string | number> {
  /** The accessible name of the whole choice (a SettingRow's title wins). */
  label: string;
  /** Null: nothing is chosen yet (a setting not read from the camera). */
  value: T | null;
  options: SegmentedOption<T>[];
  onChange: (id: T) => void;
  /** Stretches to its container, options sharing the width. */
  full?: boolean;
  /** The compact size for a group header. */
  small?: boolean;
  disabled?: boolean;
}

/** One choice out of a few, as pressed buttons in a track (`.st-seg`). */
export function Segmented<T extends string | number>({
  label,
  value,
  options,
  onChange,
  full = false,
  small = false,
  disabled = false,
}: SegmentedProps<T>): React.JSX.Element {
  const row = useSettingRowIds();
  const cls = ["st-seg", full ? "full" : "", small ? "sm" : ""].filter(Boolean).join(" ");
  return (
    <fieldset
      className={cls}
      aria-label={label}
      aria-labelledby={row?.labelId}
      aria-describedby={row?.descId}
    >
      {options.map(({ id, label: text, icon: Icon, title, disabled: off }) => (
        <button
          key={String(id)}
          type="button"
          title={title}
          aria-pressed={value === id}
          disabled={disabled || off}
          onClick={() => value !== id && onChange(id)}
        >
          {Icon ? <Icon size={13} strokeWidth={1.75} aria-hidden="true" /> : null}
          {text}
        </button>
      ))}
    </fieldset>
  );
}
