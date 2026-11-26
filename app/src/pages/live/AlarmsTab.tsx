import { Bell, History, Pencil, Plus } from "lucide-react";
import { useState } from "react";
import { Group } from "@/app/ui/Group";
import {
  ACTION_LABELS,
  type AlarmAction,
  type AlarmRule,
  alarmSentence,
  limitInUnit,
  limitToC,
  TARGET_COLDEST,
  TARGET_HOTTEST,
} from "@/lib/alarms";
import { unitSymbol } from "@/lib/thermal";
import type { Roi, TempUnit } from "@/lib/types";
import { NumberField, Switch } from "./fields";

/** One line of Recent: when, which alarm, the value then. */
export interface AlarmLogEntry {
  id: string;
  at: number;
  text: string;
  value: string;
}

export interface AlarmsTabProps {
  rules: AlarmRule[];
  onRulesChange: (rules: AlarmRule[]) => void;
  /** Rules past their limit now, with the time they went past it. */
  firing: ReadonlySet<string>;
  since: (ruleId: string) => number | null;
  log: AlarmLogEntry[];
  onClearLog: () => void;
  rois: Roi[];
  unit: TempUnit;
}

function clock(ms: number): string {
  return new Date(ms).toLocaleTimeString(undefined, {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false,
  });
}

let ruleSeq = 0;

interface Draft {
  target: string;
  direction: "above" | "below";
  limit: number;
  action: AlarmAction;
  cooldownS: number;
}

function targetsFor(rois: Roi[]): { id: string; name: string; delta: boolean }[] {
  return [
    { id: TARGET_HOTTEST, name: "Hottest point", delta: false },
    { id: TARGET_COLDEST, name: "Coldest point", delta: false },
    ...rois.map((r) => ({ id: r.id, name: r.label, delta: r.geometry.kind === "delta" })),
  ];
}

function AlarmForm({
  initial,
  editing,
  rois,
  unit,
  onCancel,
  onSave,
  onDelete,
}: {
  initial: Draft;
  editing: boolean;
  rois: Roi[];
  unit: TempUnit;
  onCancel: () => void;
  onSave: (d: Draft) => void;
  onDelete?: () => void;
}): React.JSX.Element {
  const [d, setD] = useState<Draft>(initial);
  const targets = targetsFor(rois);
  const target = targets.find((t) => t.id === d.target);
  return (
    <fieldset className="lv-alarm-edit" aria-label={editing ? "Edit alarm" : "New alarm"}>
      <label className="st-field">
        <span>When</span>
        <select
          className="st-select"
          value={d.target}
          onChange={(e) => setD({ ...d, target: e.target.value })}
        >
          {targets.map((t) => (
            <option key={t.id} value={t.id}>
              {t.name}
            </option>
          ))}
          {target ? null : <option value={d.target}>Deleted region</option>}
        </select>
      </label>
      <div className="st-two">
        <label className="st-field">
          <span className="st-vh">Direction</span>
          <select
            className="st-select"
            value={d.direction}
            onChange={(e) => setD({ ...d, direction: e.target.value as Draft["direction"] })}
          >
            <option value="above">goes above</option>
            <option value="below">goes below</option>
          </select>
        </label>
        <div className="st-field">
          <span className="st-vh">Limit</span>
          <NumberField
            label="Limit"
            value={d.limit}
            decimals={1}
            unit={unitSymbol(unit)}
            onCommit={(v) => setD({ ...d, limit: v })}
          />
        </div>
      </div>
      <label className="st-field">
        <span>Then</span>
        <select
          className="st-select"
          value={d.action}
          onChange={(e) => setD({ ...d, action: e.target.value as AlarmAction })}
        >
          {(Object.keys(ACTION_LABELS) as AlarmAction[]).map((a) => (
            <option key={a} value={a}>
              {ACTION_LABELS[a]}
            </option>
          ))}
        </select>
      </label>
      <div className="st-setrow lv-every">
        <span>At most once every</span>
        <span style={{ width: 84 }}>
          <NumberField
            label="Seconds between alerts"
            value={d.cooldownS}
            decimals={0}
            unit="s"
            onCommit={(v) => setD({ ...d, cooldownS: Math.max(0, Math.round(v)) })}
          />
        </span>
      </div>
      <div className="st-row-actions" style={{ justifyContent: "flex-end" }}>
        {onDelete ? (
          <button
            className="tc-btn tc-btn-sm tc-btn-ghost"
            type="button"
            style={{ marginRight: "auto", color: "var(--tc-danger)" }}
            onClick={onDelete}
          >
            Delete
          </button>
        ) : null}
        <button className="tc-btn tc-btn-sm tc-btn-ghost" type="button" onClick={onCancel}>
          Cancel
        </button>
        <button className="tc-btn tc-btn-sm tc-btn-primary" type="button" onClick={() => onSave(d)}>
          {editing ? "Save alarm" : "Add alarm"}
        </button>
      </div>
    </fieldset>
  );
}

function subLine(rule: AlarmRule, firing: boolean, since: number | null): string {
  const action = ACTION_LABELS[rule.action];
  if (rule.regionGone) return `${action} · off, its region was deleted`;
  if (!rule.enabled) return `${action} · off`;
  if (firing) {
    const side = rule.direction === "above" ? "above" : "below";
    return since !== null
      ? `${action} · ${side} the limit since ${clock(since)}`
      : `${action} · ${side} the limit`;
  }
  return `${action} · at most once every ${rule.cooldownS} s`;
}

/** "1 above its limit", "2 past their limits". */
export function firingSummary(rules: AlarmRule[], firing: ReadonlySet<string>): string {
  const past = rules.filter((r) => firing.has(r.id));
  const word = past.every((r) => r.direction === "above")
    ? "above"
    : past.every((r) => r.direction === "below")
      ? "below"
      : "past";
  return `${past.length} ${word} ${past.length === 1 ? "its limit" : "their limits"}`;
}

/** The Alarms tab (`live.html#alarms`). */
export function AlarmsTab({
  rules,
  onRulesChange,
  firing,
  since,
  log,
  onClearLog,
  rois,
  unit,
}: AlarmsTabProps): React.JSX.Element {
  const [editing, setEditing] = useState<string | "new" | null>(null);
  const firingCount = rules.filter((r) => firing.has(r.id)).length;
  const targets = targetsFor(rois);

  const draftFor = (rule: AlarmRule | null): Draft =>
    rule
      ? {
          target: rule.target,
          direction: rule.direction,
          limit: Number(limitInUnit(rule, unit).toFixed(1)),
          action: rule.action,
          cooldownS: rule.cooldownS,
        }
      : {
          target: rois[0]?.id ?? TARGET_HOTTEST,
          direction: "above",
          limit: Number(limitInUnit({ limitC: 60 }, unit).toFixed(1)),
          action: "notify",
          cooldownS: 5,
        };

  const save = (d: Draft, existing: AlarmRule | null) => {
    const t = targets.find((x) => x.id === d.target);
    const delta = t?.delta ?? existing?.delta ?? false;
    const next: AlarmRule = {
      id: existing?.id ?? `alarm_${Date.now()}_${ruleSeq++}`,
      enabled: true,
      target: d.target,
      targetName: t?.name ?? existing?.targetName ?? d.target,
      direction: d.direction,
      limitC: limitToC(d.limit, unit, delta),
      action: d.action,
      cooldownS: d.cooldownS,
      delta,
      regionGone: t ? undefined : existing?.regionGone,
    };
    if (!t) next.enabled = false;
    onRulesChange(
      existing ? rules.map((r) => (r.id === existing.id ? next : r)) : [...rules, next],
    );
    setEditing(null);
  };

  return (
    <>
      <Group
        icon={Bell}
        title="Alarms"
        aside={firingCount > 0 ? firingSummary(rules, firing) : undefined}
      >
        <div className="lv-alarms">
          {rules.map((rule) =>
            editing === rule.id ? (
              <AlarmForm
                key={rule.id}
                initial={draftFor(rule)}
                editing
                rois={rois}
                unit={unit}
                onCancel={() => setEditing(null)}
                onSave={(d) => save(d, rule)}
                onDelete={() => {
                  onRulesChange(rules.filter((r) => r.id !== rule.id));
                  setEditing(null);
                }}
              />
            ) : (
              <div
                key={rule.id}
                className={`lv-alarm${firing.has(rule.id) ? " firing" : ""}${rule.enabled ? "" : " off"}`}
              >
                <Switch
                  label={`${rule.targetName} alarm on`}
                  checked={rule.enabled}
                  onChange={(on) =>
                    onRulesChange(
                      rules.map((r) =>
                        r.id === rule.id ? { ...r, enabled: on && !r.regionGone } : r,
                      ),
                    )
                  }
                />
                <span className="t">{alarmSentence(rule, unit)}</span>
                <button
                  className="st-icon-btn"
                  type="button"
                  aria-label={`Edit ${rule.targetName} alarm`}
                  onClick={() => setEditing(rule.id)}
                >
                  <Pencil size={14} strokeWidth={1.75} aria-hidden="true" />
                </button>
                <span className="s">{subLine(rule, firing.has(rule.id), since(rule.id))}</span>
              </div>
            ),
          )}
          {editing === "new" ? (
            <AlarmForm
              initial={draftFor(null)}
              editing={false}
              rois={rois}
              unit={unit}
              onCancel={() => setEditing(null)}
              onSave={(d) => save(d, null)}
            />
          ) : (
            <div className="st-row-actions">
              <button className="tc-btn tc-btn-sm" type="button" onClick={() => setEditing("new")}>
                <Plus size={14} strokeWidth={1.75} aria-hidden="true" />
                New alarm
              </button>
            </div>
          )}
        </div>
      </Group>
      <Group
        icon={History}
        title="Recent"
        aside={
          log.length > 0 ? (
            <button className="st-link" type="button" onClick={onClearLog}>
              Clear
            </button>
          ) : undefined
        }
      >
        {log.length > 0 ? (
          <div className="lv-log">
            {log.map((e) => (
              <div key={e.id}>
                <span className="tm">{clock(e.at)}</span>
                <span>{e.text}</span>
                <span className="v">{e.value}</span>
              </div>
            ))}
          </div>
        ) : (
          <p className="lv-log-empty">Alarms that fire are listed here.</p>
        )}
      </Group>
    </>
  );
}
