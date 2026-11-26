/**
 * The camera connection as a plain state machine (`live.html#no-camera`, `#states`).
 *
 * - Nothing starts by itself except a real Thermocam: the port poll (every 2 s) connects one
 *   when its port appears. The demo camera starts only when a person chooses it.
 * - An unplug keeps the last image ("lost") and reconnects when the port comes back; a
 *   dropped session whose port stayed is retried every few polls.
 * - A port that is busy or refused shows its card with Try again; the poll does not keep
 *   knocking on the same port, only on one that left and came back.
 */

import { commandError } from "./utils";

export const DEMO_PORT = "sim://demo-lepton35";

export type ConnKind = "camera" | "demo";
export type ConnErrorKind = "busy" | "permission" | "not_found" | "protocol" | "other";

export type ConnState =
  | { name: "none" }
  | { name: "connecting"; port: string; kind: ConnKind; fromLost: boolean }
  | { name: "streaming"; port: string; kind: ConnKind }
  | { name: "lost"; port: string; kind: ConnKind }
  | { name: "error"; port: string; kind: "busy" | "permission"; detail: string };

export interface Machine {
  state: ConnState;
  /** Thermocam ports seen at the last poll. */
  known: readonly string[];
  /** A camera waits in update mode (HalfKay on USB, no serial port). */
  updateMode: boolean;
  /** Polls since a lost camera's port, still present, was last tried. */
  lostPolls: number;
}

/** A lost camera whose port stays present is retried after this many polls (about 6 s). */
export const LOST_RETRY_POLLS = 3;

export type ConnEvent =
  /** The poll: Thermocam serial ports present now, and whether HalfKay is on USB. */
  | { type: "ports"; ports: string[]; updateMode: boolean }
  /** A person chose a port in the camera chip. */
  | { type: "choose"; port: string }
  /** A person chose Try the demo camera / Use the demo camera. */
  | { type: "demo" }
  | { type: "connected"; port: string }
  | { type: "failed"; port: string; kind: ConnErrorKind | string; detail: string }
  /** `camera-disconnected`, or the development control's unplug. */
  | { type: "unplugged" }
  /** A person chose Disconnect. */
  | { type: "disconnect" }
  /** Try again on a busy or permission card. */
  | { type: "retry" }
  /** The development control's replug of the demo camera. */
  | { type: "replug" };

export type Effect =
  | { type: "connect"; port: string }
  | { type: "disconnect" }
  | { type: "notify"; text: string };

export function initialConnection(): Machine {
  return { state: { name: "none" }, known: [], updateMode: false, lostPolls: 0 };
}

export function kindOf(port: string): ConnKind {
  return port.startsWith("sim://") ? "demo" : "camera";
}

function connecting(
  m: Machine,
  port: string,
  fromLost = false,
): { machine: Machine; effects: Effect[] } {
  return {
    machine: { ...m, state: { name: "connecting", port, kind: kindOf(port), fromLost } },
    effects: [{ type: "connect", port }],
  };
}

const same = (m: Machine) => ({ machine: m, effects: [] as Effect[] });

export function step(m: Machine, ev: ConnEvent): { machine: Machine; effects: Effect[] } {
  const s = m.state;
  switch (ev.type) {
    case "ports": {
      const appeared = ev.ports.filter((p) => !m.known.includes(p));
      const next: Machine = {
        ...m,
        known: ev.ports,
        updateMode: ev.updateMode && ev.ports.length === 0,
        lostPolls: 0,
      };
      // A refused camera that was unplugged takes its card with it.
      if (s.name === "error" && !ev.ports.includes(s.port)) next.state = { name: "none" };
      const target = appeared[0];
      if (target === undefined) {
        // The session ended but the port stayed (the camera restarted, a heartbeat ran out):
        // the port never appears again, so try it again every few polls.
        if (s.name === "lost" && s.kind === "camera" && ev.ports.includes(s.port)) {
          const polls = m.lostPolls + 1;
          if (polls >= LOST_RETRY_POLLS) return connecting(next, s.port, true);
          return { machine: { ...next, lostPolls: polls }, effects: [] };
        }
        return { machine: next, effects: [] };
      }
      switch (s.name) {
        case "none":
        case "error":
          return connecting(next, target);
        case "lost":
          return connecting(next, target, true);
        case "streaming":
          return s.kind === "demo" ? connecting(next, target) : { machine: next, effects: [] };
        case "connecting":
          return { machine: next, effects: [] };
      }
      return { machine: next, effects: [] };
    }
    case "choose":
      if (s.name === "connecting" && s.port === ev.port) return same(m);
      if (s.name === "streaming" && s.port === ev.port) return same(m);
      return connecting(m, ev.port);
    case "demo":
      if (s.name === "connecting" && s.port === DEMO_PORT) return same(m);
      if (s.name === "streaming" && s.port === DEMO_PORT) return same(m);
      return connecting(m, DEMO_PORT);
    case "replug":
      return s.name === "lost" && s.kind === "demo" ? connecting(m, DEMO_PORT, true) : same(m);
    case "connected":
      if (s.name !== "connecting" || s.port !== ev.port) return same(m);
      return {
        machine: { ...m, state: { name: "streaming", port: s.port, kind: s.kind } },
        effects: [],
      };
    case "failed": {
      if (s.name !== "connecting" || s.port !== ev.port) return same(m);
      if (ev.kind === "busy" || ev.kind === "permission") {
        return {
          machine: {
            ...m,
            state: { name: "error", port: s.port, kind: ev.kind, detail: ev.detail },
          },
          effects: [],
        };
      }
      if (s.fromLost) {
        return {
          machine: { ...m, state: { name: "lost", port: s.port, kind: s.kind } },
          effects: [],
        };
      }
      return {
        machine: { ...m, state: { name: "none" } },
        effects: [
          {
            type: "notify",
            text:
              s.kind === "demo"
                ? `Couldn’t start the demo camera: ${ev.detail}`
                : `Couldn’t open the Thermocam: ${ev.detail}`,
          },
        ],
      };
    }
    case "unplugged":
      if (s.name === "streaming") {
        return {
          machine: { ...m, state: { name: "lost", port: s.port, kind: s.kind } },
          effects: [],
        };
      }
      return same(m);
    case "disconnect":
      if (s.name === "none") return same(m);
      return { machine: { ...m, state: { name: "none" } }, effects: [{ type: "disconnect" }] };
    case "retry":
      return s.name === "error" ? connecting(m, s.port) : same(m);
  }
}

/** How Live draws the connection: the bar's pill and whether the image is dimmed. */
export function liveConnection(m: Machine): "live" | "demo" | "lost" | "none" {
  const s = m.state;
  if (s.name === "streaming") return s.kind === "demo" ? "demo" : "live";
  if (s.name === "lost") return "lost";
  if (s.name === "connecting" && s.fromLost) return "lost";
  return "none";
}

/** The camera chip's state. */
export function chipState(m: Machine): "camera" | "demo" | "connecting" | "none" {
  const s = m.state;
  if (s.name === "streaming") return s.kind;
  if (s.name === "connecting") return "connecting";
  return "none";
}

/** The port Studio holds open now, if any. */
export function connectedPort(m: Machine): string | null {
  return m.state.name === "streaming" ? m.state.port : null;
}

export interface ConnectionDeps {
  /** Opens the port and starts the stream; rejects `{kind, detail}`. */
  connect: (port: string) => Promise<void>;
  disconnect: () => Promise<void>;
  notify: (text: string) => void;
}

export interface Connection {
  get(): Machine;
  subscribe(cb: () => void): () => void;
  dispatch(ev: ConnEvent): void;
  /** Connects `port` as if chosen, and settles when that attempt does. */
  connectNow(port: string): Promise<void>;
}

/** The machine with its effects run: connect, disconnect and messages. */
export function createConnection(deps: ConnectionDeps): Connection {
  let machine = initialConnection();
  const subs = new Set<() => void>();
  const waiters = new Map<string, { resolve: () => void; reject: (e: unknown) => void }[]>();
  const settle = (port: string, err: unknown | null) => {
    const list = waiters.get(port) ?? [];
    waiters.delete(port);
    for (const w of list) {
      if (err === null) w.resolve();
      else w.reject(err);
    }
  };
  const conn: Connection = {
    get: () => machine,
    subscribe(cb) {
      subs.add(cb);
      return () => subs.delete(cb);
    },
    connectNow(port) {
      return new Promise<void>((resolve, reject) => {
        waiters.set(port, [...(waiters.get(port) ?? []), { resolve, reject }]);
        conn.dispatch({ type: "choose", port });
        const s = machine.state;
        if (s.name === "streaming" && s.port === port) settle(port, null);
      });
    },
    dispatch(ev) {
      const r = step(machine, ev);
      if (r.machine !== machine) {
        machine = r.machine;
        for (const cb of subs) cb();
      }
      for (const e of r.effects) {
        if (e.type === "connect") {
          const port = e.port;
          deps.connect(port).then(
            () => {
              conn.dispatch({ type: "connected", port });
              settle(port, null);
            },
            (err) => {
              conn.dispatch({ type: "failed", port, ...commandError(err) });
              settle(port, err);
            },
          );
        } else if (e.type === "disconnect") {
          void deps.disconnect().catch(() => {});
        } else {
          deps.notify(e.text);
        }
      }
    },
  };
  return conn;
}
