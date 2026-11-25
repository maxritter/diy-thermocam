import { Bell, Plus, Trash2 } from "lucide-react";
import { useState } from "react";
import { StudioBadge, StudioSwitch } from "@/components/ui/Primitives";
import { formatTemp } from "@/lib/thermal";
import type {
  AlarmAction,
  AlarmCondition,
  AlarmEvent,
  Roi,
  TempSwitchRule,
  TempUnit,
} from "@/lib/types";

interface AlarmsPanelProps {
  rules: TempSwitchRule[];
  events: AlarmEvent[];
  rois: Roi[];
  unit: TempUnit;
  onAddRule: (rule: TempSwitchRule) => void;
  onUpdateRule: (rule: TempSwitchRule) => void;
  onDeleteRule: (id: string) => void;
  onClearEvents: () => void;
}

export function AlarmsPanel({
  rules,
  events,
  rois,
  unit,
  onAddRule,
  onUpdateRule,
  onDeleteRule,
  onClearEvents,
}: AlarmsPanelProps): React.JSX.Element {
  const [name, setName] = useState("VRM Overtemp Guard");
  const [targetRoiId, setTargetRoiId] = useState<string>("");
  const [condition, setCondition] = useState<AlarmCondition>("above");
  const [thresholdC, setThresholdC] = useState(60);
  const [cooldownMs, setCooldownMs] = useState(5000);
  const [action, setAction] = useState<AlarmAction>("save_dat");

  const handleCreate = (): void => {
    onAddRule({
      id: `alarm_${Date.now()}`,
      name: name.trim() || "Threshold Alarm",
      enabled: true,
      target_roi_id: targetRoiId || null,
      condition,
      threshold_c: thresholdC,
      cooldown_ms: Math.max(500, cooldownMs),
      action,
      last_triggered_ms: null,
    });
  };

  return (
    <div className="grid h-full grid-cols-12 gap-3 overflow-y-auto p-3">
      {/* Rule Builder */}
      <div className="tc-card col-span-5 p-3.5">
        <h2 className="mb-1 text-[14px] font-semibold">TempSwitch Threshold Rules</h2>
        <p className="mb-3 text-[12px] text-[var(--tc-text-muted)]">
          Automatically trigger desktop alerts, radiometric `.DAT` snapshots, or H.264 recording
          when a thermal threshold is crossed.
        </p>

        <div className="space-y-2.5">
          <div>
            <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">
              Rule Name
            </label>
            <input
              value={name}
              onChange={(e) => setName(e.target.value)}
              className="tc-input w-full"
            />
          </div>

          <div className="grid grid-cols-2 gap-2">
            <div>
              <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">Target</label>
              <select
                value={targetRoiId}
                onChange={(e) => setTargetRoiId(e.target.value)}
                className="tc-select w-full"
              >
                <option value="">Frame Max / Min</option>
                {rois.map((r) => (
                  <option key={r.id} value={r.id}>
                    ROI: {r.label}
                  </option>
                ))}
              </select>
            </div>
            <div>
              <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">
                Condition
              </label>
              <select
                value={condition}
                onChange={(e) => setCondition(e.target.value as AlarmCondition)}
                className="tc-select w-full"
              >
                <option value="above">Temp ≥ Threshold</option>
                <option value="below">Temp ≤ Threshold</option>
              </select>
            </div>
          </div>

          <div className="grid grid-cols-2 gap-2">
            <div>
              <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">
                Threshold (°C)
              </label>
              <input
                type="number"
                step={0.5}
                value={thresholdC}
                onChange={(e) => setThresholdC(Number(e.target.value))}
                className="tc-input w-full font-mono-tech"
              />
            </div>
            <div>
              <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">
                Cooldown (ms)
              </label>
              <input
                type="number"
                step={500}
                value={cooldownMs}
                onChange={(e) => setCooldownMs(Number(e.target.value))}
                className="tc-input w-full font-mono-tech"
              />
            </div>
          </div>

          <div>
            <label className="mb-1 block text-[11.5px] text-[var(--tc-text-muted)]">Action</label>
            <select
              value={action}
              onChange={(e) => setAction(e.target.value as AlarmAction)}
              className="tc-select w-full"
            >
              <option value="notify">Desktop Alert + Event Log</option>
              <option value="save_dat">Auto-Capture Lossless .DAT</option>
              <option value="start_recording">Start Full-HD Video Recording</option>
            </select>
          </div>

          <button type="button" onClick={handleCreate} className="tc-btn tc-btn-primary w-full">
            <Plus className="h-3.5 w-3.5" />
            Add TempSwitch Rule
          </button>
        </div>
      </div>

      {/* Active Rules + Trigger Event Log */}
      <div className="col-span-7 flex flex-col gap-3">
        <div className="tc-card p-3.5">
          <div className="mb-2.5 flex items-center justify-between">
            <h3 className="text-[13.5px] font-semibold">Active Rules ({rules.length})</h3>
          </div>
          <div className="space-y-2">
            {rules.map((rule) => (
              <div
                key={rule.id}
                className="flex items-center justify-between gap-3 rounded-lg border border-[var(--tc-border)] bg-[var(--tc-surface-2)] px-3 py-2"
              >
                <div className="flex items-center gap-3">
                  <StudioSwitch
                    checked={rule.enabled}
                    onCheckedChange={(enabled) => onUpdateRule({ ...rule, enabled })}
                  />
                  <div>
                    <div className="text-[12.5px] font-medium">{rule.name}</div>
                    <div className="font-mono-tech text-[11px] text-[var(--tc-text-muted)]">
                      {rule.target_roi_id
                        ? `ROI ${rois.find((r) => r.id === rule.target_roi_id)?.label ?? rule.target_roi_id}`
                        : "Full Frame"}{" "}
                      · {rule.condition === "above" ? "≥" : "≤"}{" "}
                      {formatTemp(rule.threshold_c, unit, 1)} · cooldown{" "}
                      {(rule.cooldown_ms / 1000).toFixed(1)}s
                    </div>
                  </div>
                </div>
                <div className="flex items-center gap-2">
                  <StudioBadge tone={rule.enabled ? "amber" : "neutral"}>
                    {rule.action.replace("_", " ")}
                  </StudioBadge>
                  <button
                    type="button"
                    onClick={() => onDeleteRule(rule.id)}
                    className="rounded p-1.5 text-[var(--tc-text-muted)] hover:bg-[var(--tc-danger-soft)] hover:text-[var(--tc-danger)] cursor-pointer"
                  >
                    <Trash2 className="h-3.5 w-3.5" />
                  </button>
                </div>
              </div>
            ))}
          </div>
        </div>

        <div className="tc-card flex-1 p-3.5">
          <div className="mb-2.5 flex items-center justify-between">
            <div className="flex items-center gap-2">
              <Bell className="h-4 w-4 text-[var(--tc-amber-ink)]" />
              <h3 className="text-[13.5px] font-semibold">
                Alarm Trigger History ({events.length})
              </h3>
            </div>
            <button type="button" onClick={onClearEvents} className="tc-btn h-7 text-[11.5px]">
              Clear Log
            </button>
          </div>
          {events.length === 0 ? (
            <div className="py-8 text-center text-[12px] text-[var(--tc-text-dim)]">
              No threshold crossings recorded yet. Enable a rule below the current peak temperature
              to test live triggering.
            </div>
          ) : (
            <div className="space-y-1.5 font-mono-tech text-[11.5px]">
              {events.map((ev) => (
                <div
                  key={ev.id}
                  className="flex items-center justify-between rounded border border-[var(--tc-amber-line)] bg-[var(--tc-amber-soft)] px-2.5 py-1.5"
                >
                  <span>
                    [{ev.timestamp}] <strong>{ev.rule_name}</strong>
                  </span>
                  <span>
                    {formatTemp(ev.measured_c, unit, 1)} (limit{" "}
                    {formatTemp(ev.threshold_c, unit, 1)}) · {ev.action}
                  </span>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
