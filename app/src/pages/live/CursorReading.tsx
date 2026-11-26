import type { FrameStore } from "@/lib/frameStore";
import { perf } from "@/lib/perf";
import { formatTemp, rawToObjectC } from "@/lib/thermal";
import type { RadiometryParams, RoiTool, TempUnit } from "@/lib/types";
import { useHover, useSampledFrame } from "@/lib/useFrame";

/** What the reading says while a tool is picked: the tool and how to use it. */
export const TOOL_HINTS: Partial<Record<RoiTool, string>> = {
  spot: "Spot: click to place it · Esc cancels",
  spot3: "Spot: click to place it · Esc cancels",
  box: "Box: drag across the area · Esc cancels",
  circle: "Circle: drag from the center outwards · Esc cancels",
  polygon: "Polygon: click to add points · double-click to finish · Esc cancels",
  line: "Line: drag from one end to the other · Esc cancels",
  delta: "Difference: drag from the first point to the second · Esc cancels",
};

interface CursorReadingProps {
  store: FrameStore;
  rad: RadiometryParams;
  unit: TempUnit;
  tool: RoiTool;
  engineeringDetails: boolean;
}

/**
 * The reading at the bottom of the image: the temperature under the cursor, or how to use the
 * picked tool. Engineering details add the raw value, the sensor's temperature and the frame.
 */
export function CursorReading({
  store,
  rad,
  unit,
  tool,
  engineeringDetails,
}: CursorReadingProps): React.JSX.Element | null {
  const hover = useHover(store);
  const frame = useSampledFrame(store);
  perf.panelRendered("cursor");
  const hint = TOOL_HINTS[tool];
  if (hint) return <div className="st-readout">{hint}</div>;
  if (!hover || !frame || hover.x >= frame.width || hover.y >= frame.height) return null;
  const raw = frame.pixels[hover.y * frame.width + hover.x];
  return (
    <div className={engineeringDetails ? "st-readout two-line" : "st-readout"} aria-live="off">
      <span>
        Cursor <strong>{formatTemp(rawToObjectC(raw, rad), unit)}</strong> at {hover.x}, {hover.y}
      </span>
      {engineeringDetails ? (
        <span className="st-readout-eng">
          raw {raw} · sensor {formatTemp(frame.fpa_temp_c, unit)} · frame {frame.frame_seq}
        </span>
      ) : null}
    </div>
  );
}
