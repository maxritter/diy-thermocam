import { Switch } from "@base-ui/react/switch";
import type { ReactNode } from "react";
import { cn } from "@/lib/utils";

export function StudioSwitch({
  checked,
  onCheckedChange,
  label,
}: {
  checked: boolean;
  onCheckedChange: (next: boolean) => void;
  label?: string;
}): React.JSX.Element {
  return (
    <label className="inline-flex items-center gap-2 cursor-pointer select-none">
      <Switch.Root
        checked={checked}
        onCheckedChange={onCheckedChange}
        className={cn(
          "relative inline-flex h-[18px] w-[32px] shrink-0 items-center rounded-full border transition-colors",
          checked
            ? "bg-[var(--tc-accent)] border-[var(--tc-accent)]"
            : "bg-[var(--tc-surface-3)] border-[var(--tc-border)]",
        )}
      >
        <Switch.Thumb
          className={cn(
            "block h-[12px] w-[12px] rounded-full bg-white shadow-xs transition-transform",
            checked ? "translate-x-[16px]" : "translate-x-[2px]",
          )}
        />
      </Switch.Root>
      {label ? <span className="text-[12.5px] text-[var(--tc-text)]">{label}</span> : null}
    </label>
  );
}

export function StudioBadge({
  children,
  tone = "neutral",
}: {
  children: ReactNode;
  tone?: "neutral" | "accent" | "pass" | "amber" | "danger";
}): React.JSX.Element {
  const styles =
    tone === "accent"
      ? "bg-[var(--tc-accent-soft)] text-[var(--tc-accent)] border-[var(--tc-accent-line)]"
      : tone === "pass"
        ? "bg-[var(--tc-pass-soft)] text-[var(--tc-pass)] border-[var(--tc-pass-line)]"
        : tone === "amber"
          ? "bg-[var(--tc-amber-soft)] text-[var(--tc-amber-ink)] border-[var(--tc-amber-line)]"
          : tone === "danger"
            ? "bg-[var(--tc-danger-soft)] text-[var(--tc-danger)] border-[var(--tc-danger-line)]"
            : "bg-[var(--tc-surface-2)] text-[var(--tc-text-muted)] border-[var(--tc-border)]";
  return (
    <span
      className={cn(
        "inline-flex items-center gap-1 rounded-full border px-2 py-0.5 font-mono-tech text-[10.5px] font-medium",
        styles,
      )}
    >
      {children}
    </span>
  );
}

export function MetricTile({
  label,
  value,
  sub,
  tone = "neutral",
}: {
  label: string;
  value: string;
  sub?: string;
  tone?: "neutral" | "accent" | "thermal" | "pass" | "danger" | "amber";
}): React.JSX.Element {
  const toneClass =
    tone === "accent"
      ? "text-[var(--tc-accent)]"
      : tone === "thermal"
        ? "text-[var(--tc-thermal)]"
        : tone === "pass"
          ? "text-[var(--tc-pass)]"
          : tone === "danger"
            ? "text-[var(--tc-danger)]"
            : tone === "amber"
              ? "text-[var(--tc-amber-ink)]"
              : "text-[var(--tc-text)]";
  return (
    <div className="rounded-[8px] border border-[var(--tc-border)] bg-[var(--tc-surface-2)] px-2.5 py-2">
      <div className="text-[10.5px] font-medium uppercase tracking-wider text-[var(--tc-text-dim)]">
        {label}
      </div>
      <div className={cn("mt-0.5 font-mono-tech text-[15px] font-semibold", toneClass)}>
        {value}
      </div>
      {sub ? <div className="text-[11px] text-[var(--tc-text-muted)]">{sub}</div> : null}
    </div>
  );
}
