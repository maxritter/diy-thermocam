import { ChevronDown, type LucideIcon } from "lucide-react";
import { type ReactNode, useId } from "react";

export interface GroupCardProps {
  title: string;
  /** Rarely needed: QualityLayer's card headers are words only. */
  icon?: LucideIcon;
  /** The header's right side: a status chip, a count or a small action. */
  aside?: ReactNode;
  /** The card folds under its header, closed at first (Diagnostics). */
  collapsible?: boolean;
  defaultOpen?: boolean;
  children?: ReactNode;
  className?: string;
}

/**
 * A card of settings or facts with a header row (QualityLayer's settings group): the title on
 * the left, an optional aside on the right, then rows separated by hairlines. Named by its title.
 */
export function GroupCard({
  title,
  icon: Icon,
  aside,
  collapsible = false,
  defaultOpen = false,
  children,
  className,
}: GroupCardProps): React.JSX.Element {
  const id = useId();
  const cls = ["st-gcard", className ?? ""].filter(Boolean).join(" ");
  const heading = (
    <h2 id={id} className="st-gcard-t">
      {Icon ? (
        <Icon size={15} strokeWidth={1.75} aria-hidden="true" className="st-gcard-ic" />
      ) : null}
      {title}
    </h2>
  );
  const side =
    aside !== undefined && aside !== null ? <span className="st-gcard-aside">{aside}</span> : null;

  if (collapsible) {
    return (
      <details className={`${cls} st-gcard-fold`} open={defaultOpen} aria-labelledby={id}>
        <summary className="st-gcard-h">
          {heading}
          {side}
          <ChevronDown size={14} strokeWidth={1.75} aria-hidden="true" className="st-gcard-chev" />
        </summary>
        <div className="st-gcard-b">{children}</div>
      </details>
    );
  }

  return (
    <section className={cls} aria-labelledby={id}>
      <div className="st-gcard-h">
        {heading}
        {side}
      </div>
      <div className="st-gcard-b">{children}</div>
    </section>
  );
}
