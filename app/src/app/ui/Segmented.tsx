import type { LucideIcon } from "lucide-react";

export interface SegmentedOption<T extends string> {
  id: T;
  label: string;
  /** For options that differ in kind; 13 px, dim until chosen. */
  icon?: LucideIcon;
}

export interface SegmentedProps<T extends string> {
  /** The accessible name of the whole choice. */
  label: string;
  value: T;
  options: SegmentedOption<T>[];
  onChange: (id: T) => void;
  /** Stretches to its container, options sharing the width. */
  full?: boolean;
  /** The compact size for a group header. */
  small?: boolean;
}

/** One choice out of a few, as pressed buttons in a track (`.st-seg`). */
export function Segmented<T extends string>({
  label,
  value,
  options,
  onChange,
  full = false,
  small = false,
}: SegmentedProps<T>): React.JSX.Element {
  const cls = ["st-seg", full ? "full" : "", small ? "sm" : ""].filter(Boolean).join(" ");
  return (
    <fieldset className={cls} aria-label={label}>
      {options.map(({ id, label: text, icon: Icon }) => (
        <button key={id} type="button" aria-pressed={value === id} onClick={() => onChange(id)}>
          {Icon ? <Icon size={13} strokeWidth={1.75} aria-hidden="true" /> : null}
          {text}
        </button>
      ))}
    </fieldset>
  );
}
