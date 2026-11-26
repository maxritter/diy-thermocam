import { Menu } from "@base-ui/react/menu";
import { Check, ChevronDown, Cpu, Unplug, Usb } from "lucide-react";
import { useState } from "react";
import type { SerialPortInfo } from "@/lib/types";

const MENU_ICON = { size: 14, strokeWidth: 1.75, "aria-hidden": true } as const;

export type CameraState = "camera" | "demo" | "connecting" | "none";

export interface CameraChipProps {
  state: CameraState;
  /** Battery charge in percent, or null on USB power or when unknown. */
  batteryPct: number | null;
  ports: SerialPortInfo[];
  /** The port Studio is connected to, if any. */
  connectedPort: string | null;
  onChoosePort: (port: string) => void;
  onDisconnect: () => void;
  onOpenCameraSettings: () => void;
  /** Development only: simulate an unplug or replug of the demo camera. */
  devUnplug?: { unplugged: boolean; onToggle: () => void } | null;
}

const DEMO_PORT = "sim://demo-lepton35";

export function isDemoPort(port: string | null | undefined): boolean {
  return Boolean(port?.startsWith("sim://"));
}

function shortPortName(port: string): string {
  return port.replace(/^\/dev\/(cu|tty)\./, "").replace(/^\/dev\//, "");
}

const LABELS: Record<CameraState, string> = {
  camera: "Thermocam V4",
  demo: "Demo camera",
  connecting: "Connecting…",
  none: "No camera",
};

/** The camera's name for the window title, or null while none is connected. */
export function cameraTitle(state: CameraState): string | null {
  return state === "camera" || state === "demo" ? LABELS[state] : null;
}

const DOTS: Record<CameraState, string> = {
  camera: "st-status-dot",
  demo: "st-status-dot demo",
  connecting: "st-status-dot off",
  none: "st-status-dot off",
};

/** Battery outline with a level bar, as drawn in the artifact (lucide battery + fill). */
function BatteryIcon(): React.JSX.Element {
  return (
    <svg
      width="14"
      height="14"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.75"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <rect width="16" height="10" x="2" y="7" rx="2" ry="2" />
      <line x1="22" x2="22" y1="11" y2="13" />
      <rect x="4.5" y="9.5" width="9.5" height="5" rx="0.6" fill="currentColor" stroke="none" />
    </svg>
  );
}

export function CameraChip({
  state,
  batteryPct,
  ports,
  connectedPort,
  onChoosePort,
  onDisconnect,
  onOpenCameraSettings,
  devUnplug = null,
}: CameraChipProps): React.JSX.Element {
  const [showOther, setShowOther] = useState(false);
  const cameras = ports.filter((p) => p.is_teensy && !isDemoPort(p.port_name));
  const others = ports.filter((p) => !p.is_teensy && !isDemoPort(p.port_name));
  const demoPort = ports.find((p) => isDemoPort(p.port_name))?.port_name ?? DEMO_PORT;
  const value = state === "camera" || state === "demo" ? (connectedPort ?? "") : "";

  return (
    <Menu.Root onOpenChange={(open) => !open && setShowOther(false)}>
      <Menu.Trigger className="st-device" aria-label={`Camera: ${LABELS[state]}`}>
        <span className={DOTS[state]} />
        {LABELS[state]}
        {state === "camera" && batteryPct !== null ? (
          <span className="st-batt">
            <BatteryIcon />
            {batteryPct}%
          </span>
        ) : null}
        <ChevronDown size={14} strokeWidth={1.75} aria-hidden="true" />
      </Menu.Trigger>
      <Menu.Portal>
        <Menu.Positioner side="bottom" align="end" sideOffset={6} className="z-50">
          <Menu.Popup className="st-pop" aria-label="Camera">
            <div className="pop-h">Camera</div>
            <Menu.RadioGroup value={value} onValueChange={(v) => v && onChoosePort(String(v))}>
              {cameras.map((p) => (
                <Menu.RadioItem key={p.port_name} value={p.port_name} className="item">
                  <span className="st-status-dot" />
                  <span>
                    Thermocam V4
                    <span className="sub">USB · {shortPortName(p.port_name)}</span>
                  </span>
                  <Menu.RadioItemIndicator className="end" keepMounted={false}>
                    <Check size={16} strokeWidth={1.75} aria-hidden="true" />
                  </Menu.RadioItemIndicator>
                </Menu.RadioItem>
              ))}
              <Menu.RadioItem value={demoPort} className="item">
                <span className="st-status-dot demo" />
                <span>
                  Demo camera
                  <span className="sub">Simulated, no hardware needed</span>
                </span>
                <Menu.RadioItemIndicator className="end" keepMounted={false}>
                  <Check size={16} strokeWidth={1.75} aria-hidden="true" />
                </Menu.RadioItemIndicator>
              </Menu.RadioItem>
              {showOther
                ? others.map((p) => (
                    <Menu.RadioItem key={p.port_name} value={p.port_name} className="item">
                      <span className="st-status-dot off" />
                      <span>
                        {shortPortName(p.port_name)}
                        <span className="sub">{p.product ?? p.manufacturer ?? "Serial port"}</span>
                      </span>
                      <Menu.RadioItemIndicator className="end" keepMounted={false}>
                        <Check size={16} strokeWidth={1.75} aria-hidden="true" />
                      </Menu.RadioItemIndicator>
                    </Menu.RadioItem>
                  ))
                : null}
            </Menu.RadioGroup>
            {showOther && others.length === 0 ? (
              <div className="pop-row" style={{ color: "var(--tc-text-muted)", fontSize: 12 }}>
                No other serial ports
              </div>
            ) : null}
            {showOther ? null : (
              <Menu.Item className="item" closeOnClick={false} onClick={() => setShowOther(true)}>
                <Usb {...MENU_ICON} className="item-ic" />
                <span>
                  Other serial port…
                  <span className="sub">Every port this computer sees</span>
                </span>
              </Menu.Item>
            )}
            <hr />
            <Menu.Item className="item" onClick={onOpenCameraSettings}>
              <Cpu {...MENU_ICON} className="item-ic" />
              Camera settings…
            </Menu.Item>
            <Menu.Item
              className="item"
              disabled={state === "none" || state === "connecting"}
              onClick={onDisconnect}
            >
              <Unplug {...MENU_ICON} className="item-ic" />
              Disconnect
            </Menu.Item>
            {import.meta.env?.DEV && devUnplug ? (
              <>
                <hr />
                <Menu.Item className="item" onClick={devUnplug.onToggle}>
                  <span>
                    {devUnplug.unplugged ? "Simulate replug" : "Simulate unplug"}
                    <span className="sub">Development only · demo camera</span>
                  </span>
                </Menu.Item>
              </>
            ) : null}
          </Menu.Popup>
        </Menu.Positioner>
      </Menu.Portal>
    </Menu.Root>
  );
}
