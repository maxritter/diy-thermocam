import { Dialog } from "@base-ui/react/dialog";
import {
  Check,
  ChevronDown,
  CircuitBoard,
  Copy,
  FileText,
  LoaderCircle,
  ShieldCheck,
  TriangleAlert,
  X,
} from "lucide-react";
import { useEffect, useState, useSyncExternalStore } from "react";
import { cameraApi, type FlashProgress, LINKS } from "@/lib/api/camera";
import { backend } from "@/lib/backend";
import type { HelloResp } from "@/lib/types";
import { commandError } from "@/lib/utils";
import {
  elapsedLabel,
  type Flow,
  type FlowEvent,
  type FlowView,
  initialFlow,
  refusedReason,
  step,
  writePercent,
} from "./firmwareFlow";

// ---------- The update outside React, so it survives leaving the Camera page ----------

let flow: Flow = initialFlow();
const subscribers = new Set<() => void>();

function dispatch(ev: FlowEvent): void {
  const next = step(flow, ev);
  if (next === flow) return;
  flow = next;
  for (const cb of subscribers) cb();
  afterChange();
}

function subscribe(cb: () => void): () => void {
  subscribers.add(cb);
  return () => subscribers.delete(cb);
}

export function useFirmwareFlow(): Flow {
  return useSyncExternalStore(
    subscribe,
    () => flow,
    () => flow,
  );
}

/** Test and preview hook: drive the dialog with events, as `flash-progress` would. */
export function dispatchFirmwareEvent(ev: FlowEvent): void {
  dispatch(ev);
}

export function resetFirmwareFlow(): void {
  flow = initialFlow();
  call = null;
  askQueued = false;
  for (const cb of subscribers) cb();
}

/** An update is on its way (the dialog may be hidden after Cancel). */
export function firmwareBusy(f: Flow = flow): boolean {
  return f.running || f.view.name === "cancelled";
}

interface InstallOptions {
  expectedSha256: string | null;
  /** The serial port of a V3.0 camera for the 134-baud restart, or null. */
  legacyPort: string | null;
  /** The camera's port, to ask it again on Try again and to reconnect afterwards. */
  port: string | null;
}

let options: InstallOptions = { expectedSha256: null, legacyPort: null, port: null };
/** "ask": Install or Try again, which asks the camera for update mode. "quiet": waits only. */
let call: "ask" | "quiet" | null = null;
let askQueued = false;
let listening = false;
let reconnectFn: ((port: string) => Promise<void>) | null = null;
let reconnecting = false;

function onProgress(ev: FlashProgress): void {
  // A quiet wait that ends after Try again was pressed belongs to the earlier attempt.
  if (call === "quiet" && flow.view.name === "waiting") {
    if (ev.stage === "waiting_timeout") return;
    if (ev.stage === "error" && ev.kind === "not_found") return;
  }
  dispatch({ type: "progress", ev });
}

function ensureListening(): void {
  if (listening) return;
  listening = true;
  void cameraApi.onFlashProgress(onProgress).catch(() => {
    listening = false;
  });
}

const KNOWN_KINDS = new Set(["permission", "write", "not_found", "bad_file"]);

async function runCall(kind: "ask" | "quiet", reopen: boolean): Promise<void> {
  const view = flow.view;
  if (!("file" in view)) return;
  const text = view.file.text;
  call = kind;
  dispatch({ type: "call_started" });
  try {
    if (reopen && options.port && !options.port.startsWith("sim://")) {
      await cameraApi.reopen(options.port).catch(() => {});
    }
    await cameraApi.flashFirmware(
      text,
      options.expectedSha256,
      kind === "ask" ? options.legacyPort : null,
    );
  } catch (err) {
    const e = commandError(err);
    const stage = flow.view.name;
    const ownWait = !(call === "quiet" && stage === "waiting");
    if (ownWait && (stage === "waiting" || stage === "writing")) {
      // Rust has sent this as an event already; this covers a call that failed before any.
      const k = KNOWN_KINDS.has(e.kind) ? (e.kind as "write") : "not_found";
      dispatch({ type: "progress", ev: { stage: "error", kind: k, detail: e.detail } });
    }
  } finally {
    call = null;
    dispatch({ type: "call_settled" });
  }
}

function afterChange(): void {
  const name = flow.view.name;
  if (flow.running) return;
  if (askQueued && name === "waiting") {
    askQueued = false;
    void runCall("ask", true);
  } else if (name === "timeout") {
    // "The update then continues by itself": keep waiting for update mode, without asking.
    setTimeout(() => {
      if (!flow.running && flow.view.name === "timeout") void runCall("quiet", false);
    }, 250);
  } else if (name === "restarting" && !reconnecting) {
    void reconnectAfterUpdate();
  }
}

async function reconnectAfterUpdate(): Promise<void> {
  reconnecting = true;
  try {
    const deadline = Date.now() + 20_000;
    while (Date.now() < deadline && flow.view.name === "restarting") {
      const ports = await backend.listPorts().catch(() => []);
      const candidates = [
        options.port,
        ...ports.filter((p) => p.is_teensy).map((p) => p.port_name),
      ].filter((p): p is string => Boolean(p));
      for (const port of candidates) {
        if (!reconnectFn) break;
        try {
          await reconnectFn(port);
          dispatch({ type: "reconnected" });
          return;
        } catch {
          // The camera is still restarting; try again shortly.
        }
      }
      await new Promise((r) => setTimeout(r, 1500));
    }
    dispatch({ type: "reconnect_failed" });
  } finally {
    reconnecting = false;
  }
}

let fileChosen: ((path: string) => void) | null = null;

/** Told the path of every firmware file chosen, so Finish update can use it again. */
export function onFirmwareFileChosen(cb: ((path: string) => void) | null): void {
  fileChosen = cb;
}

/** Update firmware…: the system's file dialog, then the dialog with the checked file. */
export async function chooseFirmwareFile(): Promise<void> {
  if (firmwareBusy()) return;
  ensureListening();
  const file = await cameraApi.pickFirmwareFile();
  if (file?.path) fileChosen?.(file.path);
  if (file) await checkFirmwareFile(file);
}

/**
 * Finish update: the dialog again with the file chosen last time (the camera already waits in
 * update mode). Falls back to the file dialog when that file is gone.
 */
export async function reopenFirmwareFile(path: string | null): Promise<void> {
  if (firmwareBusy()) return;
  ensureListening();
  const file = path ? await cameraApi.readFirmwareFile(path).catch(() => null) : null;
  if (file) await checkFirmwareFile(file);
  else await chooseFirmwareFile();
}

async function checkFirmwareFile(file: { name: string; text: string }): Promise<void> {
  dispatch({ type: "choose", file });
  try {
    const info = await cameraApi.verifyFirmware(file.text);
    dispatch({ type: "checked", info });
  } catch (err) {
    dispatch({ type: "check_failed", reason: refusedReason(commandError(err).detail) });
  }
}

function install(opts: InstallOptions): void {
  options = opts;
  ensureListening();
  dispatch({ type: "install", now: Date.now() });
  void runCall("ask", false);
}

function retry(): void {
  const wasWriting = flow.view.name === "failed";
  dispatch({ type: "retry", now: Date.now() });
  if (flow.running) {
    askQueued = true;
    return;
  }
  // After a failed write the camera already waits in update mode: no need to ask it.
  void runCall("ask", !wasWriting);
}

// Development only: drive the dialog from the console or a browser check, without a file dialog.
if (import.meta.env?.DEV && typeof window !== "undefined") {
  (window as unknown as Record<string, unknown>).__studioFirmware = {
    dispatch,
    choose: (name: string, text: string) => {
      ensureListening();
      return checkFirmwareFile({ name, text });
    },
    reset: resetFirmwareFlow,
  };
}

// ---------- The dialog ----------

function StepIcon({ kind }: { kind: "done" | "now" | "todo" | "warn" | "stop" }) {
  if (kind === "done") return <Check size={14} strokeWidth={1.75} aria-hidden="true" />;
  if (kind === "now")
    return <LoaderCircle size={14} strokeWidth={1.75} className="cm-spin" aria-hidden="true" />;
  if (kind === "warn") return <TriangleAlert size={14} strokeWidth={1.75} aria-hidden="true" />;
  if (kind === "stop") return <X size={14} strokeWidth={1.75} aria-hidden="true" />;
  return null;
}

interface StepRow {
  kind: "done" | "now" | "todo" | "warn" | "stop";
  label: string;
  aside?: string;
}

function Steps({ rows }: { rows: StepRow[] }): React.JSX.Element {
  return (
    <ol className="cm-steps">
      {rows.map((r) => (
        <li key={r.label} className={r.kind}>
          <span className="ic">
            <StepIcon kind={r.kind} />
          </span>
          <span>{r.label}</span>
          <span className="mono">{r.aside ?? ""}</span>
        </li>
      ))}
    </ol>
  );
}

function Note({ children }: { children: React.ReactNode }): React.JSX.Element {
  return (
    <div className="cm-note amber">
      <TriangleAlert size={14} strokeWidth={1.75} className="cm-note-ic" aria-hidden="true" />
      <span>{children}</span>
    </div>
  );
}

function shaLine(sha: string, bytes: number, blocks: number): string {
  return `${sha.slice(0, 8)}…${sha.slice(-8)} · ${Math.round(bytes / 1000)} KB · ${blocks} blocks`;
}

export interface FirmwareDialogProps {
  hello: HelloResp | null;
  /** "Thermocam V4", "Demo camera" or null without a camera. */
  cameraLabel: string | null;
  /** The connected camera's port, if any. */
  port: string | null;
  /** Opens the camera again after it restarted with the new firmware. */
  onReconnect: (port: string) => Promise<void>;
}

/** `camera.html#firmware`: one dialog from the chosen file to done. */
export function FirmwareDialog({
  hello,
  cameraLabel,
  port,
  onReconnect,
}: FirmwareDialogProps): React.JSX.Element | null {
  const { view } = useFirmwareFlow();
  const [now, setNow] = useState(() => Date.now());
  const [expected, setExpected] = useState("");
  const [legacy, setLegacy] = useState(false);
  const [copied, setCopied] = useState(false);

  useEffect(() => {
    reconnectFn = onReconnect;
  }, [onReconnect]);

  useEffect(() => {
    if (view.name !== "waiting") return;
    setNow(Date.now());
    const id = setInterval(() => setNow(Date.now()), 250);
    return () => clearInterval(id);
  }, [view.name]);

  useEffect(() => {
    if (view.name === "closed") {
      setExpected("");
      setLegacy(false);
    }
  }, [view.name]);

  useEffect(() => {
    if (!copied) return;
    const id = setTimeout(() => setCopied(false), 1600);
    return () => clearTimeout(id);
  }, [copied]);

  const open = view.name !== "closed" && view.name !== "cancelled";
  const close = () => dispatch({ type: "close" });

  return (
    <Dialog.Root open={open} onOpenChange={(o) => !o && close()}>
      <Dialog.Portal>
        <Dialog.Backdrop className="st-scrim z-50" />
        <Dialog.Popup
          className="st-dialog cm-dialog z-50"
          style={{ width: 520 }}
          role={view.name === "failed" ? "alertdialog" : "dialog"}
        >
          {open ? (
            <Body
              view={view}
              now={now}
              hello={hello}
              cameraLabel={cameraLabel}
              expected={expected}
              onExpected={setExpected}
              legacy={legacy}
              onLegacy={setLegacy}
              copied={copied}
              onCopy={(sha) =>
                void navigator.clipboard
                  .writeText(sha)
                  .then(() => setCopied(true))
                  .catch(() => {})
              }
              onClose={close}
              onInstall={() =>
                install({
                  expectedSha256: expected.trim() || null,
                  legacyPort: legacy ? port : null,
                  port,
                })
              }
            />
          ) : null}
        </Dialog.Popup>
      </Dialog.Portal>
    </Dialog.Root>
  );
}

interface BodyProps {
  view: FlowView;
  now: number;
  hello: HelloResp | null;
  cameraLabel: string | null;
  expected: string;
  onExpected: (v: string) => void;
  legacy: boolean;
  onLegacy: (v: boolean) => void;
  copied: boolean;
  onCopy: (sha: string) => void;
  onClose: () => void;
  onInstall: () => void;
}

function Header({
  title,
  closable,
  onClose,
}: {
  title: string;
  closable: boolean;
  onClose: () => void;
}) {
  return (
    <div className="dlg-h">
      <div className="dlg-t">
        <span className="dlg-ic">
          <CircuitBoard size={16} strokeWidth={1.75} aria-hidden="true" />
        </span>
        <Dialog.Title>{title}</Dialog.Title>
      </div>
      {closable ? (
        <button type="button" className="st-icon-btn" aria-label="Close" onClick={onClose}>
          <X size={16} strokeWidth={1.75} aria-hidden="true" />
        </button>
      ) : null}
    </div>
  );
}

function Body(p: BodyProps): React.JSX.Element | null {
  const { view } = p;
  switch (view.name) {
    case "checking":
    case "ready":
    case "refused": {
      const info = view.name === "ready" ? view.info : null;
      const wanted = p.expected.trim().toLowerCase();
      const mismatch = Boolean(info && wanted && wanted !== info.sha256_hex.toLowerCase());
      return (
        <>
          <Header title="Update firmware" closable onClose={p.onClose} />
          <div className="dlg-b">
            <div className={view.name === "refused" ? "cm-file-row bad" : "cm-file-row"}>
              <span className="fi">
                <FileText size={16} strokeWidth={1.75} aria-hidden="true" />
              </span>
              <div className="info">
                <span className="t">{view.file.name}</span>
                {view.name === "refused" ? (
                  <span className="m bad">
                    <TriangleAlert size={14} strokeWidth={1.75} aria-hidden="true" />
                    {view.reason}
                  </span>
                ) : info ? (
                  <span className="m ok">
                    <ShieldCheck size={14} strokeWidth={1.75} aria-hidden="true" />
                    File checked · {Math.round(info.total_bytes / 1000)} KB
                  </span>
                ) : (
                  <span className="m">
                    <LoaderCircle
                      size={14}
                      strokeWidth={1.75}
                      className="cm-spin"
                      aria-hidden="true"
                    />
                    Checking the file…
                  </span>
                )}
              </div>
              <button type="button" className="cm-link" onClick={() => void chooseFirmwareFile()}>
                Choose another
              </button>
            </div>
            {view.name === "refused" ? (
              <p className="cm-hint">
                Nothing was sent to the camera. Get its firmware from the releases page.
              </p>
            ) : (
              <>
                <dl className="cm-kv">
                  <dt>Installed now</dt>
                  <dd>{p.hello?.fw_build_str ?? "Unknown"}</dd>
                  <dt>Camera</dt>
                  <dd>{p.cameraLabel ? `${p.cameraLabel} · USB` : "No camera connected"}</dd>
                </dl>
                <Note>
                  The live image stops during the update. Keep the camera plugged in until it
                  restarts.
                </Note>
                <details className="cm-more boxed">
                  <summary>
                    Advanced<span className="aside">Checksum, older cameras</span>
                    <ChevronDown size={14} strokeWidth={1.75} aria-hidden="true" />
                  </summary>
                  <div className="cm-adv">
                    <div className="cm-field">
                      <span>SHA-256 of this file</span>
                      <div className="cm-codebox">
                        <span title={info?.sha256_hex}>
                          {info
                            ? shaLine(info.sha256_hex, info.total_bytes, info.block_count)
                            : "…"}
                        </span>
                        <button
                          type="button"
                          className="tc-btn tc-btn-sm"
                          disabled={!info}
                          onClick={() => info && p.onCopy(info.sha256_hex)}
                        >
                          <Copy size={14} strokeWidth={1.75} aria-hidden="true" />
                          {p.copied ? "Copied" : "Copy"}
                        </button>
                      </div>
                    </div>
                    <label className="cm-field">
                      <span>Expected SHA-256 (optional)</span>
                      <input
                        className="cm-input mono"
                        placeholder="Paste from the release page to require a match"
                        value={p.expected}
                        spellCheck={false}
                        onChange={(e) => p.onExpected(e.target.value)}
                      />
                      {mismatch ? <span className="err">Doesn’t match this file</span> : null}
                    </label>
                    <div className="cm-setrow">
                      <span className="lbl">Camera with V3.0 firmware</span>
                      <label className="st-switch">
                        <input
                          type="checkbox"
                          checked={p.legacy}
                          aria-label="Camera with V3.0 firmware"
                          onChange={(e) => p.onLegacy(e.target.checked)}
                        />
                        <span className="track" />
                      </label>
                    </div>
                  </div>
                </details>
              </>
            )}
          </div>
          <div className={view.name === "refused" ? "dlg-f end" : "dlg-f"}>
            {view.name === "refused" ? null : (
              <button
                type="button"
                className="cm-link"
                onClick={() => void cameraApi.openUrl(LINKS.latestFirmware).catch(() => {})}
              >
                Get the latest firmware
              </button>
            )}
            <div className="cm-row-actions">
              <button type="button" className="tc-btn tc-btn-ghost" onClick={p.onClose}>
                Cancel
              </button>
              <button
                type="button"
                className="tc-btn tc-btn-primary"
                disabled={view.name !== "ready" || mismatch}
                onClick={p.onInstall}
              >
                Install
              </button>
            </div>
          </div>
        </>
      );
    }
    case "waiting":
      return (
        <>
          <Header title="Waiting for update mode" closable={false} onClose={p.onClose} />
          <div className="dlg-b">
            <Steps
              rows={[
                { kind: "done", label: "File checked" },
                {
                  kind: "now",
                  label: "Camera restarts in update mode",
                  aside: elapsedLabel(view.since, p.now),
                },
              ]}
            />
            <p className="cm-hint">This takes a few seconds. Keep the camera plugged in.</p>
          </div>
          <div className="dlg-f end">
            <button type="button" className="tc-btn tc-btn-ghost" onClick={p.onClose}>
              Cancel
            </button>
          </div>
        </>
      );
    case "timeout":
      return (
        <>
          <Header title="The camera hasn’t switched yet" closable={false} onClose={p.onClose} />
          <div className="dlg-b">
            <Steps
              rows={[
                { kind: "done", label: "File checked" },
                { kind: "warn", label: "Camera restarts in update mode", aside: "0:10" },
              ]}
            />
            <p className="cm-hint">
              Press the program button on the Teensy board inside the camera and the update
              continues by itself, or choose Try again.
            </p>
          </div>
          <div className="dlg-f end">
            <button type="button" className="tc-btn tc-btn-ghost" onClick={p.onClose}>
              Cancel
            </button>
            <button type="button" className="tc-btn tc-btn-primary" onClick={retry}>
              Try again
            </button>
          </div>
        </>
      );
    case "writing":
    case "restarting": {
      const pct = view.name === "writing" ? writePercent(view) : 100;
      return (
        <>
          <Header title="Updating firmware" closable={false} onClose={p.onClose} />
          <div className="dlg-b">
            <Steps
              rows={[
                { kind: "done", label: "File checked" },
                { kind: "done", label: "Camera restarted in update mode" },
                view.name === "writing"
                  ? { kind: "now", label: "Writing firmware", aside: `${pct}%` }
                  : { kind: "done", label: "Firmware written" },
                {
                  kind: view.name === "writing" ? "todo" : "now",
                  label: "Camera restarts",
                },
              ]}
            />
            <div
              className="cm-bar"
              role="progressbar"
              aria-label="Writing firmware"
              aria-valuenow={pct}
              aria-valuemin={0}
              aria-valuemax={100}
            >
              <i style={{ width: `${pct}%` }} />
            </div>
            <Note>
              Keep the camera plugged in. If Studio closes now, the camera waits in update mode and
              Studio offers to finish next time.
            </Note>
          </div>
        </>
      );
    }
    case "done":
      return (
        <>
          <Header title="Firmware updated" closable={false} onClose={p.onClose} />
          <div className="dlg-b">
            <div style={{ display: "flex", gap: 12, alignItems: "center" }}>
              <span className="cm-done-ic">
                <Check size={16} strokeWidth={1.75} aria-hidden="true" />
              </span>
              <span>
                {view.reconnected
                  ? "The camera restarted with the new firmware and is connected again."
                  : "The new firmware is installed. Studio connects to the camera once it is back."}
              </span>
            </div>
          </div>
          <div className="dlg-f end">
            <button type="button" className="tc-btn tc-btn-primary" onClick={p.onClose}>
              Done
            </button>
          </div>
        </>
      );
    case "failed": {
      const wrote = view.percent !== null;
      return (
        <>
          <Header title="The update stopped" closable={false} onClose={p.onClose} />
          <div className="dlg-b">
            <Steps
              rows={
                wrote
                  ? [
                      { kind: "done", label: "File checked" },
                      { kind: "done", label: "Camera restarted in update mode" },
                      { kind: "stop", label: `Writing firmware stopped at ${view.percent}%` },
                    ]
                  : [
                      { kind: "done", label: "File checked" },
                      { kind: "stop", label: "Camera restarts in update mode" },
                    ]
              }
            />
            <p className="cm-hint">
              {view.kind === "permission"
                ? "This computer didn’t let Studio open the camera in update mode. On Linux, add the udev rule from the user manual, then try again."
                : "The camera waits in update mode, so trying again is safe. Check the USB cable."}
            </p>
            <details className="cm-more boxed">
              <summary>
                Details<span className="aside">For a bug report</span>
                <ChevronDown size={14} strokeWidth={1.75} aria-hidden="true" />
              </summary>
              <p className="cm-details-text">
                {view.kind}: {view.detail}
                {"\n"}SHA-256 {view.info.sha256_hex} · {view.file.name}
              </p>
            </details>
          </div>
          <div className="dlg-f end">
            <button type="button" className="tc-btn tc-btn-ghost" onClick={p.onClose}>
              Close
            </button>
            <button type="button" className="tc-btn tc-btn-primary" onClick={retry}>
              Try again
            </button>
          </div>
        </>
      );
    }
    default:
      return null;
  }
}
