import { ChevronDown, type LucideIcon } from "lucide-react";
import { type ReactNode, useId } from "react";

/** A group heading's icon: 14 px, muted, on the heading's first line. */
export const GROUP_ICON = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

export interface GroupProps {
  /** What the group is, from lucide; drawn before the title. */
  icon: LucideIcon;
  title: string;
  /** The header's right side: a count, a link or a small control. */
  aside?: ReactNode;
  /** The group folds under its header; `defaultOpen` says how it starts. */
  collapsible?: boolean;
  defaultOpen?: boolean;
  children?: ReactNode;
}

/**
 * A titled group of settings in a side panel or card: an icon, a title and an optional aside on
 * one header line, then the contents. The group is labelled by its title for assistive tech.
 */
export function Group({
  icon: Icon,
  title,
  aside,
  collapsible = false,
  defaultOpen = true,
  children,
}: GroupProps): React.JSX.Element {
  const id = useId();
  const side =
    typeof aside === "string" ? (
      <span className="st-grp-aside-t" title={aside}>
        {aside}
      </span>
    ) : (
      aside
    );
  const heading = (
    <h3 id={id}>
      <Icon {...GROUP_ICON} className="st-grp-ic" />
      {title}
    </h3>
  );

  if (collapsible) {
    return (
      <details className="st-grp st-fold" open={defaultOpen} aria-labelledby={id}>
        <summary className="st-grp-h">
          {heading}
          {aside !== undefined && aside !== null ? (
            <span className="st-grp-aside">{side}</span>
          ) : null}
          <ChevronDown {...GROUP_ICON} className="st-fold-chev" />
        </summary>
        <div className="st-grp-b">{children}</div>
      </details>
    );
  }

  return (
    // biome-ignore lint/a11y/useSemanticElements: a titled group of mixed content, not form fields; <fieldset> would add form semantics.
    <div className="st-grp" role="group" aria-labelledby={id}>
      <div className="st-grp-h">
        {heading}
        {aside !== undefined && aside !== null ? <div className="st-grp-aside">{side}</div> : null}
      </div>
      {children}
    </div>
  );
}
