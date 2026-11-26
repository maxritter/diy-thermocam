import type { LucideIcon } from "lucide-react";
import {
  cloneElement,
  createContext,
  isValidElement,
  type ReactNode,
  useContext,
  useId,
} from "react";

/** The ids of a row's title and description, for the control inside it. */
export interface SettingRowIds {
  labelId: string;
  descId?: string;
}

const RowIds = createContext<SettingRowIds | null>(null);

/** Inside a SettingRow: the ids its control is labelled and described by. */
export function useSettingRowIds(): SettingRowIds | null {
  return useContext(RowIds);
}

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
  /** Why the camera refused the last change, under the description. */
  error?: string | null;
}

const NATIVE_CONTROLS = new Set(["input", "select", "textarea", "button"]);

/**
 * One setting: an icon, a title with a one-line description, and its control on the right
 * (or under the words when `stacked`). Shared by the side panels and settings pages.
 *
 * The icon is decorative; the title names the control and the description (with a refusal,
 * when there is one) describes it. A native control gets the ids directly; `Switch` and
 * `Segmented` read them from the row.
 */
export function SettingRow({
  icon: Icon,
  title,
  desc,
  value,
  control,
  stacked = false,
  error = null,
}: SettingRowProps): React.JSX.Element {
  const id = useId();
  const labelId = `${id}t`;
  const described = [desc ? `${id}d` : "", error ? `${id}e` : ""].filter(Boolean).join(" ");
  const ids: SettingRowIds = { labelId, descId: described || undefined };

  let ctl = control;
  if (isValidElement<Record<string, unknown>>(control) && typeof control.type === "string") {
    if (NATIVE_CONTROLS.has(control.type)) {
      const own = control.props["aria-describedby"];
      ctl = cloneElement(control, {
        // A button keeps the words it shows ("Calibrate now"); a field takes the row's title.
        "aria-labelledby": control.type === "button" ? undefined : labelId,
        "aria-describedby":
          [typeof own === "string" ? own : "", described].filter(Boolean).join(" ") || undefined,
      });
    }
  }

  return (
    <div className={stacked ? "st-row stacked" : desc || error ? "st-row" : "st-row solo"}>
      <Icon size={15} strokeWidth={1.75} aria-hidden="true" className="st-row-ic" />
      <div className="st-row-txt">
        <span className="st-row-n">
          <span id={labelId}>{title}</span>
          {value !== undefined ? <span className="st-row-v">{value}</span> : null}
        </span>
        {desc ? (
          <span className="st-row-d" id={`${id}d`}>
            {desc}
          </span>
        ) : null}
        {error ? (
          <span className="st-row-err" id={`${id}e`} role="alert">
            {error}
          </span>
        ) : null}
      </div>
      <div className="st-row-ctl">
        <RowIds.Provider value={ids}>{ctl}</RowIds.Provider>
      </div>
    </div>
  );
}
