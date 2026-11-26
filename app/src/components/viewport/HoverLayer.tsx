import type { FrameStore } from "@/lib/frameStore";
import type { FrameSize, Pt } from "@/lib/regions";
import type { RoiGeometry } from "@/lib/types";
import { useHover } from "@/lib/useFrame";

interface HoverLayerProps {
  store: FrameStore;
  size: FrameSize;
  /** The shape being dragged out, before it becomes a region. */
  preview: RoiGeometry | null;
  /** Corners of a polygon being drawn. */
  polygonDraft: Pt[];
}

/**
 * What follows the pointer: the shape being drawn and the polygon's open edge to the cursor.
 * Only this layer re-renders on a mouse move.
 */
export function HoverLayer({
  store,
  size,
  preview,
  polygonDraft,
}: HoverLayerProps): React.JSX.Element {
  const hover = useHover(store);
  const { width: W, height: H } = size;
  const c = (v: number) => v + 0.5;

  let shape: React.JSX.Element | null = null;
  if (preview) {
    const g = preview;
    if (g.kind === "box") {
      shape = (
        <g>
          <rect className="o" x={g.x} y={g.y} width={g.w} height={g.h} />
          <rect className="a dash" x={g.x} y={g.y} width={g.w} height={g.h} />
        </g>
      );
    } else if (g.kind === "circle") {
      shape = (
        <g>
          <circle className="o" cx={c(g.cx)} cy={c(g.cy)} r={g.radius} />
          <circle className="a dash" cx={c(g.cx)} cy={c(g.cy)} r={g.radius} />
        </g>
      );
    } else if (g.kind === "line" || g.kind === "delta") {
      const p = { x1: c(g.x0), y1: c(g.y0), x2: c(g.x1), y2: c(g.y1) };
      shape = (
        <g>
          <line className="o" {...p} />
          <line className="a dash" {...p} />
        </g>
      );
    }
  }

  let draft: React.JSX.Element | null = null;
  if (polygonDraft.length > 0) {
    const pts = polygonDraft.map(([x, y]) => `${c(x)},${c(y)}`);
    if (hover) pts.push(`${c(hover.x)},${c(hover.y)}`);
    draft = (
      <g data-polygon-draft="">
        <polyline className="o" points={pts.join(" ")} />
        <polyline className="a dash" points={pts.join(" ")} />
        {polygonDraft.map(([x, y]) => (
          <circle key={`${x}_${y}`} className="dot" cx={c(x)} cy={c(y)} r={1.3} />
        ))}
      </g>
    );
  }

  return (
    <svg className="st-ov" viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" aria-hidden="true">
      {shape}
      {draft}
    </svg>
  );
}
