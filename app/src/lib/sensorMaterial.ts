import { useEffect, useRef, useSyncExternalStore } from "react";
import { cameraApi } from "./api/camera";
import type { Material, RadParams } from "./types";

/**
 * What the camera's sensor holds (Plan, Contracts › Material switch). `null` is factory values
 * (emissivity 1): the sensor corrects nothing. Studio's single-correction function takes this
 * value and undoes the sensor's correction before it applies the wanted material, so no reading
 * is corrected twice. It is read from the camera on every connect and after every write.
 */
export type SensorMaterial = Material | null;

let current: SensorMaterial = null;
const listeners = new Set<() => void>();

export function getSensorMaterial(): SensorMaterial {
  return current;
}

export function setSensorMaterial(next: SensorMaterial): void {
  if (
    next === current ||
    (next &&
      current &&
      next.emissivity === current.emissivity &&
      next.reflected_c === current.reflected_c)
  ) {
    return;
  }
  current = next ? { emissivity: next.emissivity, reflected_c: next.reflected_c } : null;
  for (const cb of listeners) cb();
}

export function subscribeSensorMaterial(cb: () => void): () => void {
  listeners.add(cb);
  return () => listeners.delete(cb);
}

export function useSensorMaterial(): SensorMaterial {
  return useSyncExternalStore(subscribeSensorMaterial, getSensorMaterial, getSensorMaterial);
}

// ---------- RadParams (Lepton CCI flux parameters: Q3.13 emissivity, centi-kelvin) ----------

const Q13 = 8192;
const FLAG_PERSIST = 0x01;
const FLAG_RESET = 0x02;

/** The sensor's material, or null at factory emissivity. */
export function materialFromRadParams(p: RadParams): SensorMaterial {
  if (p.scene_emissivity >= Q13) return null;
  return {
    emissivity: Math.round((p.scene_emissivity / Q13) * 10000) / 10000,
    reflected_c: Math.round((p.t_bkg_ck / 100 - 273.15) * 100) / 100,
  };
}

/** Live's material as a persisted write; the other flux parameters keep the camera's values. */
export function radParamsForMaterial(m: Material, base: RadParams): RadParams {
  const eps = Math.min(1, Math.max(0.01, m.emissivity));
  return {
    ...base,
    scene_emissivity: Math.round(eps * Q13),
    t_bkg_ck: Math.max(0, Math.min(0xffff, Math.round((m.reflected_c + 273.15) * 100))),
    flags: FLAG_PERSIST,
  };
}

/** The reset-defaults write: factory values on the sensor and its saved copy cleared. */
export function resetRadParams(base: RadParams): RadParams {
  return { ...base, flags: FLAG_RESET };
}

function sameWrite(a: RadParams | null, b: RadParams | null): boolean {
  return (
    a !== null &&
    b !== null &&
    a.scene_emissivity === b.scene_emissivity &&
    a.t_bkg_ck === b.t_bkg_ck
  );
}

// ---------- The switch's camera side ----------

export interface RadLink {
  getRadParams(): Promise<RadParams>;
  setRadParams(params: RadParams): Promise<RadParams>;
}

/**
 * The material switch's rules:
 * - connect only reads `GET_RAD_PARAMS` into the store, whatever the switch says;
 * - switching on writes Live's material (persist) at once, and again whenever it changes;
 * - switching off is the only path that resets the camera to factory values;
 * - disconnect sends nothing; the camera keeps what was last written.
 * Writes go one at a time; while one is on its way only Live's newest material waits.
 */
export class MaterialSync {
  private on = false;
  private connected = false;
  private held: RadParams | null = null;
  private chain: Promise<unknown> = Promise.resolve();
  private generation = 0;
  private pendingLive: Material | null = null;
  private liveTimer: ReturnType<typeof setTimeout> | null = null;

  constructor(
    private readonly link: RadLink,
    private readonly opts: { debounceMs?: number } = {},
  ) {}

  /** The switch as Studio's settings remember it, without touching the camera. */
  restore(on: boolean): void {
    this.on = on;
  }

  isOn(): boolean {
    return this.on;
  }

  async connect(): Promise<void> {
    this.connected = true;
    const gen = ++this.generation;
    await this.serial(async () => {
      if (gen !== this.generation) return;
      await this.read();
    });
  }

  disconnect(): void {
    this.connected = false;
    this.generation++;
    this.pendingLive = null;
    if (this.liveTimer) clearTimeout(this.liveTimer);
    this.liveTimer = null;
  }

  /** Switching on (a person's action). Rejects with the camera's reason when it refuses. */
  async switchOn(live: Material): Promise<void> {
    this.on = true;
    if (!this.connected) return;
    const gen = this.generation;
    await this.serial(async () => {
      if (gen !== this.generation) return;
      await this.write(radParamsForMaterial(live, this.held ?? (await this.read())));
    });
  }

  /** Switching off (a person's action): the reset-defaults flag. */
  async switchOff(): Promise<void> {
    this.on = false;
    this.pendingLive = null;
    if (!this.connected) return;
    const gen = this.generation;
    await this.serial(async () => {
      if (gen !== this.generation) return;
      await this.write(resetRadParams(this.held ?? (await this.read())));
    });
  }

  /** Live's material changed; written only while the switch is on and a camera is connected. */
  liveMaterialChanged(live: Material): Promise<void> {
    if (!this.on || !this.connected) return Promise.resolve();
    const delay = this.opts.debounceMs ?? 0;
    this.pendingLive = live;
    if (delay <= 0) return this.flushLive();
    if (this.liveTimer) clearTimeout(this.liveTimer);
    return new Promise<void>((resolve) => {
      this.liveTimer = setTimeout(() => {
        this.liveTimer = null;
        void this.flushLive().finally(resolve);
      }, delay);
    });
  }

  private flushLive(): Promise<void> {
    const gen = this.generation;
    return this.serial(async () => {
      const live = this.pendingLive;
      this.pendingLive = null;
      if (!live || !this.on || !this.connected || gen !== this.generation) return;
      const base = this.held ?? (await this.read());
      const next = radParamsForMaterial(live, base);
      if (sameWrite(next, this.held)) return;
      await this.write(next);
    }).catch(() => {});
  }

  private serial<T>(fn: () => Promise<T>): Promise<T> {
    const run = this.chain.then(fn, fn);
    this.chain = run.catch(() => {});
    return run;
  }

  private async read(): Promise<RadParams> {
    try {
      const p = await this.link.getRadParams();
      this.held = p;
      setSensorMaterial(materialFromRadParams(p));
      return p;
    } catch (err) {
      // Firmware without radiometry access: the sensor is at factory values.
      this.held = null;
      setSensorMaterial(null);
      throw err;
    }
  }

  private async write(p: RadParams): Promise<void> {
    await this.link.setRadParams(p);
    await this.read().catch(() => {});
  }
}

/** The app's one sync, on the camera's real commands. Live's material settles for 400 ms. */
export const materialSync = new MaterialSync(cameraApi, { debounceMs: 400 });

/**
 * Keeps `materialSync` in step with the connection and with Live's material. Mount once, where
 * both are known (App). `connectionId` is the connected port, or null.
 */
export function useMaterialSync(args: {
  connectionId: string | null;
  switchOn: boolean;
  material: Material;
}): void {
  const { connectionId, switchOn, material } = args;
  const restored = useRef(false);
  if (!restored.current) {
    restored.current = true;
    materialSync.restore(switchOn);
  }

  useEffect(() => {
    if (!connectionId) {
      materialSync.disconnect();
      return;
    }
    void materialSync.connect().catch(() => {});
    return () => materialSync.disconnect();
  }, [connectionId]);

  const { emissivity, reflected_c } = material;
  useEffect(() => {
    void materialSync.liveMaterialChanged({ emissivity, reflected_c });
  }, [emissivity, reflected_c]);
}
