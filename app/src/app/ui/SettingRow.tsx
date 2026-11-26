import type { LucideIcon } from "lucide-react";
import type { ReactNode } from "react";

export interface SettingRowProps {
  /** What the setting is, from lucide: 15 px, muted, leading the row. */
  icon: LucideIcon;
  title: string;
  /** One short line on what the setting does for the person. */
  desc?: ReactNode;
  /** A reading shown at the title's right, such as "45 %". */
  value?: ReactNode;
  /** The switch, select or segmented control. */
  control: ReactNode;
  /** The control goes under the words, full width (a slider, a wide segmented control). */
  stacked?: boolean;
}

/**
 * One setting: an icon, a title with a one-line description, and its control on the right
 * (or under the words when `stacked`). Shared by the side panels and settings pages.
 */
export function SettingRow({
  icon: Icon,
  title,
  desc,
  value,
  control,
  stacked = false,
}: SettingRowProps): React.JSX.Element {
  return (
    <div className={stacked ? "st-row stacked" : "st-row"}>
      <Icon size={15} strokeWidth={1.75} aria-hidden="true" className="st-row-ic" />
      <div className="st-row-txt">
        <span className="st-row-n">
          {title}
          {value !== undefined ? <span className="st-row-v">{value}</span> : null}
        </span>
        {desc ? <span className="st-row-d">{desc}</span> : null}
      </div>
      <div className="st-row-ctl">{control}</div>
    </div>
  );
}
