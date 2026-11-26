import type { DeviceConfig } from "@/lib/types";

/** `lepton_gain_mode`: high gain is −10…140 °C at 0.01 K per count, low gain −10…450 °C at 0.1 K. */
export const GAIN_HIGH = 0;
export const GAIN_LOW = 1;
/** `limits_mode`: 0 auto, 1 auto locked, 2 manual (`manual_min_raw`/`manual_max_raw`). */
export const LIMITS_MANUAL = 2;

export const OVERLAY_SPOT = 1 << 0;
export const OVERLAY_COLORBAR = 1 << 1;
export const OVERLAY_BATTERY = 1 << 2;

const CAL_OFFSET = -273.15;

export function slopeForGain(gain: number): number {
  return gain === GAIN_LOW ? 0.1 : 0.01;
}

export function rawToC(raw: number, gain: number): number {
  return raw * slopeForGain(gain) + CAL_OFFSET;
}

export function cToRaw(c: number, gain: number): number {
  return Math.max(0, Math.min(0xffff, Math.round((c - CAL_OFFSET) / slopeForGain(gain))));
}

/** The config in another measuring range. Manual limits keep their temperatures across the ×10
 * slope change. */
export function convertManualLimits(config: DeviceConfig, toGain: number): DeviceConfig {
  const from = config.lepton_gain_mode;
  if (config.limits_mode !== LIMITS_MANUAL || from === toGain) {
    return { ...config, lepton_gain_mode: toGain };
  }
  return {
    ...config,
    lepton_gain_mode: toGain,
    manual_min_raw: cToRaw(rawToC(config.manual_min_raw, from), toGain),
    manual_max_raw: cToRaw(rawToC(config.manual_max_raw, from), toGain),
  };
}

export function hasOverlay(config: DeviceConfig, mask: number): boolean {
  return (config.overlay_flags & mask) !== 0;
}

export function withOverlay(config: DeviceConfig, mask: number, on: boolean): DeviceConfig {
  return {
    ...config,
    overlay_flags: on ? config.overlay_flags | mask : config.overlay_flags & ~mask,
  };
}

function errorDetail(err: unknown): { kind: string | null; detail: string } {
  if (err && typeof err === "object" && "detail" in err) {
    const e = err as { kind?: unknown; detail?: unknown };
    return { kind: typeof e.kind === "string" ? e.kind : null, detail: String(e.detail) };
  }
  if (err instanceof Error) return { kind: null, detail: err.message };
  return { kind: null, detail: String(err) };
}

/** The firmware's reason for refusing a request, in words. */
export function refusalText(err: unknown): string {
  const { kind, detail } = errorDetail(err);
  if (kind === "read_only") return "The demo camera’s settings are read-only.";
  if (/device returned error 3\b/i.test(detail)) return "The camera doesn’t accept this value.";
  if (/device returned error 7\b/i.test(detail)) return "The camera doesn’t allow this right now.";
  if (/device returned error 5\b/i.test(detail)) return "The camera can’t do this.";
  if (/not connected/i.test(detail)) return "The camera isn’t connected.";
  if (/timeout/i.test(detail)) return "The camera didn’t answer.";
  return detail ? `The camera refused it: ${detail}` : "The camera refused it.";
}

/** The camera side of the settings: `cmd_set_config` and `cmd_trigger_ffc`. */
export interface CameraLink {
  setConfig(config: DeviceConfig, persist: boolean): Promise<DeviceConfig>;
  triggerFfc(force: boolean): Promise<void>;
}

export interface SettingsState {
  /** What the controls show: the camera's values plus changes on their way. */
  shown: DeviceConfig | null;
  /** The reason a setting flipped back, by setting key. */
  errors: Record<string, string>;
  /** Keys with a request on its way. */
  busy: string[];
}

export type ConfigPatch = Partial<DeviceConfig> | ((c: DeviceConfig) => DeviceConfig);

interface Op {
  key: string;
  config: DeviceConfig;
  persist: boolean;
  after?: (sent: DeviceConfig) => Promise<void>;
  waiters: (() => void)[];
}

function same(a: DeviceConfig | null, b: DeviceConfig | null): boolean {
  return JSON.stringify(a) === JSON.stringify(b);
}

function applyPatch(c: DeviceConfig, patch: ConfigPatch): DeviceConfig {
  return typeof patch === "function" ? patch(c) : { ...c, ...patch };
}

/**
 * The Camera page's settings rules (Plan, Contracts › Camera settings):
 * - switches and segmented choices send `set_config` with persist at once;
 * - number fields and drags send RAM-only while changing (the newest value wins while the camera
 *   is busy) and persist once on commit (Enter, blur or pointer up);
 * - a refused setting flips back to the camera's values and keeps the reason;
 * - a measuring range switch converts manual limits through °C, persists, then recalibrates
 *   (`TRIGGER_FFC`, forced in manual limits mode).
 * Requests go to the camera one at a time, in order.
 */
export class SettingsController {
  private confirmed: DeviceConfig | null = null;
  private persisted: DeviceConfig | null = null;
  private state: SettingsState = { shown: null, errors: {}, busy: [] };
  private queue: Op[] = [];
  private running = false;
  private listeners = new Set<() => void>();

  constructor(private readonly link: CameraLink) {}

  getState(): SettingsState {
    return this.state;
  }

  subscribe(cb: () => void): () => void {
    this.listeners.add(cb);
    return () => this.listeners.delete(cb);
  }

  private set(patch: Partial<SettingsState>): void {
    this.state = { ...this.state, ...patch };
    for (const cb of this.listeners) cb();
  }

  private setError(key: string, text: string | null): void {
    const errors = { ...this.state.errors };
    if (text === null) delete errors[key];
    else errors[key] = text;
    this.set({ errors });
  }

  /** The values the camera reported (on connect or after a refresh). Drops changes on their way. */
  load(config: DeviceConfig | null): void {
    this.confirmed = config ? { ...config } : null;
    this.persisted = config ? { ...config } : null;
    this.queue = [];
    this.set({ shown: config ? { ...config } : null, errors: {}, busy: [] });
  }

  /** A switch or segmented choice: shown at once, sent with persist. */
  toggle(key: string, patch: ConfigPatch): Promise<void> {
    const shown = this.state.shown;
    if (!shown) return Promise.resolve();
    const next = applyPatch(shown, patch);
    this.setError(key, null);
    this.set({ shown: next });
    return this.enqueue({ key, config: next, persist: true, waiters: [] });
  }

  /** A number field or drag while it changes: RAM-only. */
  change(key: string, patch: ConfigPatch): Promise<void> {
    const shown = this.state.shown;
    if (!shown) return Promise.resolve();
    const next = applyPatch(shown, patch);
    if (same(next, shown)) return Promise.resolve();
    this.setError(key, null);
    this.set({ shown: next });
    return this.enqueue({ key, config: next, persist: false, waiters: [] });
  }

  /** Enter, blur or pointer up: persists what is shown, once, if it is not saved yet. */
  commit(key: string): Promise<void> {
    const shown = this.state.shown;
    if (!shown) return Promise.resolve();
    const tail = this.queue[this.queue.length - 1];
    const queuedPersist = tail?.persist && same(tail.config, shown);
    if (queuedPersist || (same(shown, this.persisted) && !this.running && !this.queue.length)) {
      return Promise.resolve();
    }
    return this.enqueue({ key, config: shown, persist: true, waiters: [] });
  }

  /** The measuring range: convert manual limits, persist, then recalibrate. */
  setRange(gain: number, key = "range"): Promise<void> {
    const shown = this.state.shown;
    if (!shown || shown.lepton_gain_mode === gain) return Promise.resolve();
    const next = convertManualLimits(shown, gain);
    this.setError(key, null);
    this.set({ shown: next });
    return this.enqueue({
      key,
      config: next,
      persist: true,
      waiters: [],
      after: async (sent) => {
        try {
          await this.link.triggerFfc(sent.limits_mode === LIMITS_MANUAL);
        } catch (err) {
          this.setError(key, `Couldn’t recalibrate: ${refusalText(err)}`);
        }
      },
    });
  }

  /** Calibrate now (`TRIGGER_FFC`); forced in manual limits mode, where it is refused otherwise. */
  async calibrate(key = "calibrate"): Promise<void> {
    const shown = this.state.shown;
    if (!shown) return;
    this.setError(key, null);
    this.set({ busy: [...this.state.busy, key] });
    try {
      await this.link.triggerFfc(shown.limits_mode === LIMITS_MANUAL);
    } catch (err) {
      this.setError(key, refusalText(err));
    } finally {
      this.set({ busy: this.state.busy.filter((k) => k !== key) });
    }
  }

  private enqueue(op: Op): Promise<void> {
    return new Promise<void>((resolve) => {
      const tail = this.queue[this.queue.length - 1];
      // A RAM-only change that has not started yet is replaced by a newer change or a commit.
      if (tail && !tail.persist && !tail.after) {
        this.queue.pop();
        op.waiters.push(...tail.waiters);
      }
      op.waiters.push(resolve);
      this.queue.push(op);
      this.set({ busy: Array.from(new Set([...this.state.busy, op.key])) });
      void this.drain();
    });
  }

  private async drain(): Promise<void> {
    if (this.running) return;
    this.running = true;
    try {
      for (let op = this.queue.shift(); op; op = this.queue.shift()) {
        await this.run(op);
        for (const w of op.waiters) w();
      }
    } finally {
      this.running = false;
      this.set({ busy: [] });
    }
  }

  private async run(op: Op): Promise<void> {
    try {
      const echo = await this.link.setConfig(op.config, op.persist);
      this.confirmed = { ...echo };
      if (op.persist) this.persisted = { ...echo };
      if (this.queue.length === 0) this.set({ shown: { ...echo } });
      if (op.after) await op.after(echo);
    } catch (err) {
      // Changes built on the refused value are dropped with it.
      const dropped = this.queue.splice(0);
      for (const d of dropped) for (const w of d.waiters) w();
      this.set({ shown: this.confirmed ? { ...this.confirmed } : null });
      this.setError(op.key, refusalText(err));
    }
  }
}
