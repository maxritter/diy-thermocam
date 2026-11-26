import {
  createContext,
  type KeyboardEvent,
  type PointerEvent,
  type ReactNode,
  useContext,
  useEffect,
  useRef,
} from "react";
import { clampPanelWidth, dragPanelWidth, keyPanelWidth, PANEL_WIDTH } from "@/lib/panelWidth";

/** The side panel's width and where a new width is saved (Studio's settings). */
export interface PanelWidthValue {
  width: number;
  onChange: (width: number) => void;
}

export const PanelWidthContext = createContext<PanelWidthValue>({
  width: PANEL_WIDTH.default,
  onChange: () => {},
});

export interface SidePanelProps {
  /** The tab list above the contents. */
  tabs: ReactNode;
  children: ReactNode;
  label?: string;
}

interface Drag {
  startX: number;
  startWidth: number;
  width: number;
}

/**
 * The side panel of Live and Captures: tabs over scrolling contents, resized by dragging its
 * left edge. While dragging only the panel's own style changes; the width is saved once, on
 * release. Double-click on the edge goes back to 320 px; the arrow keys step 16 px.
 */
export function SidePanel({ tabs, children, label = "Side panel" }: SidePanelProps) {
  const value = useContext(PanelWidthContext);
  const width = clampPanelWidth(value.width);
  const onChange = value.onChange;
  const asideRef = useRef<HTMLElement | null>(null);
  const edgeRef = useRef<HTMLDivElement | null>(null);
  const drag = useRef<Drag | null>(null);

  const show = (w: number) => {
    if (asideRef.current) asideRef.current.style.width = `${w}px`;
    edgeRef.current?.setAttribute("aria-valuenow", String(w));
  };

  const save = (w: number) => {
    show(w);
    if (w !== width) onChange(w);
  };

  const stopDragging = () => {
    drag.current = null;
    document.documentElement.classList.remove("st-resizing");
  };

  useEffect(() => () => document.documentElement.classList.remove("st-resizing"), []);

  const onPointerDown = (e: PointerEvent<HTMLDivElement>) => {
    if (e.button !== 0) return;
    e.preventDefault();
    try {
      e.currentTarget.setPointerCapture(e.pointerId);
    } catch {
      // Capture is a nicety: without it the drag still follows the pointer over the panel.
    }
    drag.current = { startX: e.clientX, startWidth: width, width };
    document.documentElement.classList.add("st-resizing");
  };

  const onPointerMove = (e: PointerEvent<HTMLDivElement>) => {
    const d = drag.current;
    if (!d) return;
    const w = dragPanelWidth(d.startWidth, d.startX, e.clientX);
    if (w === d.width) return;
    d.width = w;
    show(w);
  };

  const onPointerUp = () => {
    const d = drag.current;
    if (!d) return;
    stopDragging();
    save(d.width);
  };

  const cancel = () => {
    if (!drag.current) return;
    stopDragging();
    show(width);
  };

  const onKeyDown = (e: KeyboardEvent<HTMLDivElement>) => {
    if (e.key === "Escape") {
      cancel();
      return;
    }
    const w = keyPanelWidth(width, e.key);
    if (w === null) return;
    e.preventDefault();
    save(w);
  };

  return (
    <aside ref={asideRef} className="st-insp" aria-label={label} style={{ width }}>
      {/* biome-ignore lint/a11y/useSemanticElements: a focusable resizer; <hr> cannot take focus or keys. */}
      <div
        ref={edgeRef}
        className="st-insp-edge"
        role="separator"
        aria-orientation="vertical"
        aria-label="Resize side panel"
        aria-valuenow={width}
        aria-valuemin={PANEL_WIDTH.min}
        aria-valuemax={PANEL_WIDTH.max}
        tabIndex={0}
        title="Drag to resize · double-click to reset"
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onPointerCancel={cancel}
        onLostPointerCapture={onPointerUp}
        onDoubleClick={() => save(PANEL_WIDTH.default)}
        onKeyDown={onKeyDown}
      />
      <div className="st-insp-tabs">{tabs}</div>
      <div className="st-insp-body">{children}</div>
    </aside>
  );
}
