import { useEffect, useState } from "react";
import { fromCelsius, toCelsius, unitSymbol } from "@/lib/thermal";
import type { TempUnit } from "@/lib/types";

/** The artifact's on/off switch. */
export function Switch({
  checked,
  onChange,
  label,
}: {
  checked: boolean;
  onChange: (next: boolean) => void;
  label: string;
}): React.JSX.Element {
  return (
    <label className="st-switch">
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

/**
 * A number typed as text. It commits on Enter or when it loses focus, never while typing,
 * so "0.9" is not read as "0" on the way; an unreadable entry falls back to the last value.
 */
export function NumberField({
  value,
  decimals,
  onCommit,
  label,
  unit,
  className,
}: {
  value: number;
  decimals: number;
  onCommit: (value: number) => void;
  label: string;
  /** Shown inside the field's right edge, such as "°C". */
  unit?: string;
  className?: string;
}): React.JSX.Element {
  const shown = Number.isFinite(value) ? value.toFixed(decimals) : "";
  const [text, setText] = useState(shown);
  const [editing, setEditing] = useState(false);
  useEffect(() => {
    if (!editing) setText(shown);
  }, [shown, editing]);

  const commit = () => {
    setEditing(false);
    const v = Number.parseFloat(text.replace(",", ".").replace("−", "-"));
    if (Number.isFinite(v)) onCommit(v);
    else setText(shown);
  };

  const input = (
    <input
      className={`st-input num ${className ?? ""}`}
      inputMode="decimal"
      aria-label={label}
      value={editing ? text : shown}
      onFocus={() => {
        setText(shown);
        setEditing(true);
      }}
      onChange={(e) => {
        setEditing(true);
        setText(e.target.value);
      }}
      onBlur={commit}
      onKeyDown={(e) => {
        if (e.key === "Enter") {
          e.preventDefault();
          commit();
        }
        if (e.key === "Escape") {
          setEditing(false);
          setText(shown);
          e.currentTarget.blur();
        }
      }}
    />
  );
  return unit !== undefined ? (
    <span className="st-unit-in" data-unit={unit}>
      {input}
    </span>
  ) : (
    input
  );
}

/** A temperature in the chosen unit; it commits in °C. */
export function TempField({
  valueC,
  unit,
  onCommit,
  label,
}: {
  valueC: number;
  unit: TempUnit;
  onCommit: (valueC: number) => void;
  label: string;
}): React.JSX.Element {
  return (
    <NumberField
      value={fromCelsius(valueC, unit)}
      decimals={1}
      unit={unitSymbol(unit)}
      label={label}
      onCommit={(v) => onCommit(toCelsius(v, unit))}
    />
  );
}
