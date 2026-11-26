import { useState } from "react";
import { materialSync, useSensorMaterial } from "@/lib/sensorMaterial";
import { formatTemp } from "@/lib/thermal";
import type { Material, TempUnit } from "@/lib/types";
import { refusalText } from "./settingsRules";

export interface MaterialSwitchProps {
  /** The switch as Studio's settings remember it (off by default). */
  checked: boolean;
  onCheckedChange: (on: boolean) => void;
  /** Live's material: what switching on sends. */
  liveMaterial: Material;
  unit: TempUnit;
  /** The demo camera, or no camera: read-only. */
  disabled?: boolean;
}

/**
 * "Use Studio's material on the camera's screen". On writes Live's material to the sensor and
 * keeps it in step; off is the only path that resets the camera to its factory values. Readings
 * stay single-corrected either way, through the sensor material the camera reports.
 */
export function MaterialSwitch({
  checked,
  onCheckedChange,
  liveMaterial,
  unit,
  disabled = false,
}: MaterialSwitchProps): React.JSX.Element {
  const sensor = useSensorMaterial();
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const toggle = async (on: boolean) => {
    setError(null);
    setBusy(true);
    onCheckedChange(on);
    try {
      if (on) await materialSync.switchOn(liveMaterial);
      else await materialSync.switchOff();
    } catch (err) {
      // The camera kept its values: the switch flips back and says why.
      materialSync.restore(!on);
      onCheckedChange(!on);
      setError(refusalText(err));
    } finally {
      setBusy(false);
    }
  };

  const holds =
    !checked && sensor
      ? ` The camera now uses emissivity ${sensor.emissivity.toFixed(2)} and reflected temperature ${formatTemp(sensor.reflected_c, unit)}.`
      : "";

  return (
    <div className="cm-set">
      <span className="n">Use Studio’s material on the camera’s screen</span>
      <span className="d">
        Sends the emissivity and reflected temperature set in Live to the sensor, so the camera’s
        own screen and the captures it saves use them too. Off: the camera keeps its own values.
        {holds}
      </span>
      {error ? (
        <span className="err" role="alert">
          {error}
        </span>
      ) : null}
      <label className="st-switch ctl">
        <input
          type="checkbox"
          checked={checked}
          disabled={disabled || busy}
          aria-label="Use Studio's material on the camera's screen"
          onChange={(e) => void toggle(e.target.checked)}
        />
        <span className="track" />
      </label>
    </div>
  );
}
