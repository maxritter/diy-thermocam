import { formatTemp, fromCelsius, toCelsius, unitSymbol } from "./thermal";
import type { TempUnit } from "./types";

/**
 * Alarms (`live.html#alarms`): a rule reads as a sentence ("MCU above 48.5 °C") and fires when
 * its reading crosses the limit, at most once per crossing and once per cooldown. Readings
 * come keyed by target: "hottest", "coldest", or a region's id (the value its row shows).
 */

export type AlarmAction = "notify" | "snapshot" | "record";

export interface AlarmRule {
  id: string;
  enabled: boolean;
  /** "hottest", "coldest" or a region's id. */
  target: string;
  /** The target's name when the rule was made, kept when the region is deleted. */
  targetName: string;
  direction: "above" | "below";
  /** The limit in °C (a difference in °C for a difference region). */
  limitC: number;
  action: AlarmAction;
  /** At most once every … s. */
  cooldownS: number;
  /** The target is a difference region: its limit is a difference. */
  delta?: boolean;
  /** Switched off because its region was deleted. */
  regionGone?: boolean;
}

export interface AlarmFire {
  ruleId: string;
  valueC: number;
  atMs: number;
}

export interface AlarmEngine {
  /** One set of readings at `nowMs`; returns the rules that fire now. */
  evaluate(
    rules: readonly AlarmRule[],
    readings: Readonly<Record<string, number | undefined>>,
    nowMs: number,
  ): AlarmFire[];
  /** Rules past their limit at the last reading. */
  firing(): ReadonlySet<string>;
  /** When a rule went past its limit, or null. */
  since(ruleId: string): number | null;
  /** The last value each rule read. */
  value(ruleId: string): number | null;
  clear(): void;
}

interface RuleState {
  over: boolean;
  sinceMs: number | null;
  lastFiredMs: number | null;
  value: number | null;
}

export const TARGET_HOTTEST = "hottest";
export const TARGET_COLDEST = "coldest";

export function isPast(rule: AlarmRule, valueC: number): boolean {
  return rule.direction === "above" ? valueC > rule.limitC : valueC < rule.limitC;
}

export function createAlarmEngine(): AlarmEngine {
  const states = new Map<string, RuleState>();
  let firing = new Set<string>();

  return {
    evaluate(rules, readings, nowMs) {
      const fired: AlarmFire[] = [];
      const nowFiring = new Set<string>();
      const live = new Set<string>();
      for (const rule of rules) {
        live.add(rule.id);
        const st = states.get(rule.id) ?? {
          over: false,
          sinceMs: null,
          lastFiredMs: null,
          value: null,
        };
        states.set(rule.id, st);
        if (!rule.enabled) {
          st.over = false;
          st.sinceMs = null;
          continue;
        }
        const v = readings[rule.target];
        if (v === undefined || !Number.isFinite(v)) {
          if (st.over) nowFiring.add(rule.id);
          continue;
        }
        st.value = v;
        const past = isPast(rule, v);
        if (past && !st.over) {
          st.sinceMs = nowMs;
          const cooled = st.lastFiredMs === null || nowMs - st.lastFiredMs >= rule.cooldownS * 1000;
          if (cooled) {
            st.lastFiredMs = nowMs;
            fired.push({ ruleId: rule.id, valueC: v, atMs: nowMs });
          }
        }
        if (!past) st.sinceMs = null;
        st.over = past;
        if (past) nowFiring.add(rule.id);
      }
      for (const id of states.keys()) if (!live.has(id)) states.delete(id);
      firing = nowFiring;
      return fired;
    },
    firing: () => firing,
    since: (id) => states.get(id)?.sinceMs ?? null,
    value: (id) => states.get(id)?.value ?? null,
    clear() {
      states.clear();
      firing = new Set();
    },
  };
}

/** Rules whose region no longer exists switch off and say so. Unchanged input comes back as is. */
export function reconcileRegions(
  rules: readonly AlarmRule[],
  regionIds: ReadonlySet<string>,
): AlarmRule[] {
  let changed = false;
  const next = rules.map((r) => {
    const isRegion = r.target !== TARGET_HOTTEST && r.target !== TARGET_COLDEST;
    if (!isRegion || regionIds.has(r.target) || r.regionGone) return r;
    changed = true;
    return { ...r, enabled: false, regionGone: true };
  });
  return changed ? next : (rules as AlarmRule[]);
}

/** A limit in the chosen unit (a difference converts by scale only). */
export function limitInUnit(rule: Pick<AlarmRule, "limitC" | "delta">, unit: TempUnit): number {
  if (rule.delta) return unit === "fahrenheit" ? (rule.limitC * 9) / 5 : rule.limitC;
  return fromCelsius(rule.limitC, unit);
}

/** A typed limit back to °C. */
export function limitToC(value: number, unit: TempUnit, delta: boolean): number {
  if (delta) return unit === "fahrenheit" ? (value * 5) / 9 : value;
  return toCelsius(value, unit);
}

function formatLimit(rule: AlarmRule, unit: TempUnit): string {
  return `${limitInUnit(rule, unit).toFixed(1)} ${unitSymbol(unit)}`;
}

function formatValue(rule: AlarmRule, valueC: number, unit: TempUnit): string {
  if (!rule.delta) return formatTemp(valueC, unit);
  return `${limitInUnit({ limitC: valueC, delta: true }, unit).toFixed(1)} ${unitSymbol(unit)}`;
}

/** "MCU above 48.5 °C": the rule's name. */
export function alarmSentence(rule: AlarmRule, unit: TempUnit): string {
  return `${rule.targetName} ${rule.direction} ${formatLimit(rule, unit)}`;
}

/** "MCU is above 48.5 °C · 52.7 °C now". */
export function alarmBanner(rule: AlarmRule, valueC: number, unit: TempUnit): string {
  return `${rule.targetName} is ${rule.direction} ${formatLimit(rule, unit)} · ${formatValue(rule, valueC, unit)} now`;
}

/** "MCU is above 48.5 °C": the message away from Live. */
export function alarmMessage(rule: AlarmRule, unit: TempUnit): string {
  return `${rule.targetName} is ${rule.direction} ${formatLimit(rule, unit)}`;
}

export function alarmValue(rule: AlarmRule, valueC: number, unit: TempUnit): string {
  return formatValue(rule, valueC, unit);
}

export const ACTION_LABELS: Record<AlarmAction, string> = {
  notify: "Notify me",
  snapshot: "Take a snapshot",
  record: "Start recording",
};
