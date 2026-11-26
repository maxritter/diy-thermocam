import { type ColorRange, shownRange } from "@/lib/colorRange";
import type { FrameStore } from "@/lib/frameStore";
import { samplePalette } from "@/lib/palettes";
import { formatTemp, rawToObjectC } from "@/lib/thermal";
import type { Palette, RadiometryParams, TempUnit } from "@/lib/types";
import { useHover, useSampledFrame } from "@/lib/useFrame";

const CELLS = Array.from({ length: 49 }, (_, i) => ({
  dx: (i % 7) - 3,
  dy: Math.floor(i / 7) - 3,
}));

interface LoupeProps {
  store: FrameStore;
  palette: Palette;
  range: ColorRange;
  rad: RadiometryParams;
  unit: TempUnit;
  engineeringDetails: boolean;
}

/** The magnifier: 7 × 7 sensor pixels around the cursor, or around the hottest point. */
export function Loupe({
  store,
  palette,
  range,
  rad,
  unit,
  engineeringDetails,
}: LoupeProps): React.JSX.Element | null {
  const frame = useSampledFrame(store);
  const hover = useHover(store);
  if (!frame) return null;
  const W = frame.width;
  const H = frame.height;
  const cx = hover ? hover.x : frame.max_x;
  const cy = hover ? hover.y : frame.max_y;
  const raw = frame.pixels[cy * W + cx];
  const [lo, hi] = shownRange(range, frame);
  const span = Math.max(1, hi - lo);
  return (
    <div className="st-loupe" role="img" aria-label="Magnifier">
      <div style={{ display: "flex", justifyContent: "space-between" }}>
        <span>
          {cx}, {cy}
        </span>
        <strong style={{ color: "#ff9a5c" }}>{formatTemp(rawToObjectC(raw, rad), unit, 2)}</strong>
      </div>
      <div className="px" aria-hidden="true">
        {CELLS.map(({ dx, dy }) => {
          const x = Math.max(0, Math.min(W - 1, cx + dx));
          const y = Math.max(0, Math.min(H - 1, cy + dy));
          const [r, g, b] = samplePalette(palette, (frame.pixels[y * W + x] - lo) / span);
          const center = dx === 0 && dy === 0;
          return (
            <i
              key={`${dx}_${dy}`}
              style={{
                background: `rgb(${r} ${g} ${b})`,
                outline: center ? "1px solid #fff" : undefined,
                outlineOffset: center ? -1 : undefined,
              }}
            />
          );
        })}
      </div>
      <div style={{ color: "#a7abb4" }}>
        {engineeringDetails ? `raw ${raw} · ` : ""}7 × 7 pixels
      </div>
    </div>
  );
}
