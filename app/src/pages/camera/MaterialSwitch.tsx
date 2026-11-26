import { Layers } from "lucide-react";
import { useState } from "react";
import { SettingRow } from "@/app/ui/SettingRow";
import { Switch } from "@/app/ui/Switch";
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
      ? ` It now uses ε ${sensor.emissivity.toFixed(2)}, reflected ${formatTemp(sensor.reflected_c, unit)}.`
      : "";

  return (
    <SettingRow
      icon={Layers}
      title="Use Studio’s material"
      desc={`Sends Live’s emissivity and reflected temperature to the camera, for its screen and its own captures. Off: the camera keeps its values.${holds}`}
      error={error}
      control={
        <Switch
          label="Use Studio’s material"
          checked={checked}
          disabled={disabled || busy}
          onChange={(on) => void toggle(on)}
        />
      }
    />
  );
}
