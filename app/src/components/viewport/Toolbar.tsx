import { Menu } from "@base-ui/react/menu";
import type { ReactNode } from "react";
import type { RoiTool } from "@/lib/types";

/** The artifact's tool icons (lucide 0.511 shapes, some redrawn for measuring). */
function Icon({ children }: { children: ReactNode }): React.JSX.Element {
  return (
    <svg
      width="16"
      height="16"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.75"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      {children}
    </svg>
  );
}

const ICONS: Record<string, ReactNode> = {
  select: (
    <path d="M4.037 4.688a.495.495 0 0 1 .651-.651l16 6.5a.5.5 0 0 1-.063.947l-6.124 1.58a2 2 0 0 0-1.438 1.435l-1.579 6.126a.5.5 0 0 1-.947.063z" />
  ),
  spot: (
    <>
      <circle cx="12" cy="12" r="9" />
      <path d="M22 12h-5M7 12H2M12 7V2M12 22v-5" />
    </>
  ),
  box: <rect width="16" height="16" x="4" y="4" rx="1.5" />,
  circle: <circle cx="12" cy="12" r="8.5" />,
  polygon: (
    <path d="M3.5 8.7c-.7.5-1 1.4-.7 2.2l2.8 8.7c.3.8 1 1.4 1.9 1.4h9.1c.9 0 1.6-.6 1.9-1.4l2.8-8.7c.3-.8 0-1.7-.7-2.2l-7.4-5.3a2.1 2.1 0 0 0-2.4 0Z" />
  ),
  line: (
    <>
      <path d="M5.5 18.5 18.5 5.5" />
      <circle cx="4.5" cy="19.5" r="1.8" />
      <circle cx="19.5" cy="4.5" r="1.8" />
    </>
  ),
  delta: <path d="M13.73 4a2 2 0 0 0-3.46 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3Z" />,
  flame: (
    <path d="M8.5 14.5A2.5 2.5 0 0 0 11 12c0-1.38-.5-2-1-3-1.072-2.143-.224-4.054 2-6 .5 2.5 2 4.9 4 6.5 2 1.6 3 3.5 3 5.5a7 7 0 1 1-14 0c0-1.153.433-2.294 1-3a2.5 2.5 0 0 0 2.5 2.5z" />
  ),
  loupe: (
    <>
      <circle cx="11" cy="11" r="7.5" />
      <path d="m21 21-4.6-4.6M8 11h6M11 8v6" />
    </>
  ),
  grid: (
    <>
      <rect width="18" height="18" x="3" y="3" rx="2" />
      <path d="M3 9h18M3 15h18M9 3v18M15 3v18" />
    </>
  ),
};

/** Tools and their one-key shortcuts, as in live.html#live-view. */
export const TOOLS: { id: RoiTool; label: string; title: string }[] = [
  { id: "select", label: "Select", title: "Select · V" },
  { id: "spot", label: "Spot", title: "Spot · S" },
  { id: "box", label: "Box", title: "Box · B" },
  { id: "circle", label: "Circle", title: "Circle · C" },
  { id: "polygon", label: "Polygon", title: "Polygon · P" },
  { id: "line", label: "Line", title: "Line · L" },
  { id: "delta", label: "Difference between two points", title: "Difference · D" },
];

/** Grid readings sizes, columns × rows. */
export const GRID_SIZES: [number, number][] = [
  [3, 3],
  [4, 3],
  [4, 4],
  [5, 4],
  [6, 4],
];

export interface ToolbarProps {
  tool: RoiTool;
  onToolChange: (tool: RoiTool) => void;
  showHotCold: boolean;
  onToggleHotCold: () => void;
  showLoupe: boolean;
  onToggleLoupe: () => void;
  showGrid: boolean;
  gridSize: [number, number];
  /** Turns grid readings on with a size, or off. */
  onGridChange: (show: boolean, size?: [number, number]) => void;
}

/** The tool rail at the left of the image. */
export function Toolbar({
  tool,
  onToolChange,
  showHotCold,
  onToggleHotCold,
  showLoupe,
  onToggleLoupe,
  showGrid,
  gridSize,
  onGridChange,
}: ToolbarProps): React.JSX.Element {
  const gridValue = showGrid ? `${gridSize[0]}x${gridSize[1]}` : "off";
  return (
    <div
      className="st-rail"
      role="toolbar"
      aria-label="Measuring tools"
      aria-orientation="vertical"
    >
      {TOOLS.map((t) => (
        <button
          key={t.id}
          className="st-tool"
          type="button"
          aria-pressed={tool === t.id || (t.id === "spot" && tool === "spot3")}
          aria-label={t.label}
          title={t.title}
          onClick={() => onToolChange(t.id)}
        >
          <Icon>{ICONS[t.id]}</Icon>
        </button>
      ))}
      <hr />
      <button
        className="st-tool"
        type="button"
        aria-pressed={showHotCold}
        aria-label="Mark hottest and coldest point"
        title="Hottest and coldest · H"
        onClick={onToggleHotCold}
      >
        <Icon>{ICONS.flame}</Icon>
      </button>
      <button
        className="st-tool"
        type="button"
        aria-pressed={showLoupe}
        aria-label="Magnifier"
        title="Magnifier · M"
        onClick={onToggleLoupe}
      >
        <Icon>{ICONS.loupe}</Icon>
      </button>
      <Menu.Root>
        <Menu.Trigger
          className="st-tool"
          aria-pressed={showGrid}
          aria-label="Grid readings"
          title="Grid readings · G"
        >
          <Icon>{ICONS.grid}</Icon>
        </Menu.Trigger>
        <Menu.Portal>
          <Menu.Positioner side="right" align="end" sideOffset={10} className="z-50">
            <Menu.Popup className="st-pop" style={{ width: 200 }} aria-label="Grid readings">
              <div className="pop-h">Grid readings</div>
              <Menu.RadioGroup
                value={gridValue}
                onValueChange={(v) => {
                  if (v === "off") {
                    onGridChange(false);
                    return;
                  }
                  const [c, r] = String(v).split("x").map(Number);
                  onGridChange(true, [c, r]);
                }}
              >
                <Menu.RadioItem value="off" className="item">
                  Off
                </Menu.RadioItem>
                {GRID_SIZES.map(([c, r]) => (
                  <Menu.RadioItem key={`${c}x${r}`} value={`${c}x${r}`} className="item">
                    {c} × {r}
                    <Menu.RadioItemIndicator className="end" keepMounted={false}>
                      <svg
                        width="16"
                        height="16"
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        strokeWidth="1.75"
                        strokeLinecap="round"
                        strokeLinejoin="round"
                        aria-hidden="true"
                      >
                        <path d="M20 6 9 17l-5-5" />
                      </svg>
                    </Menu.RadioItemIndicator>
                  </Menu.RadioItem>
                ))}
              </Menu.RadioGroup>
            </Menu.Popup>
          </Menu.Positioner>
        </Menu.Portal>
      </Menu.Root>
    </div>
  );
}
