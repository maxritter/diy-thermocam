import { Menu } from "@base-ui/react/menu";
import { Share } from "lucide-react";

export interface ExportChoice {
  label: string;
  sub: string;
  run: () => void;
}

/** Export… for an opened capture or recording: what can be saved from it, each asking where. */
export function ExportMenu({ choices }: { choices: ExportChoice[] }): React.JSX.Element {
  return (
    <Menu.Root>
      <Menu.Trigger className="tc-btn tc-btn-primary">
        <Share size={16} strokeWidth={1.75} aria-hidden="true" />
        Export…
      </Menu.Trigger>
      <Menu.Portal>
        <Menu.Positioner side="bottom" align="end" sideOffset={6} className="z-50">
          <Menu.Popup className="st-pop" aria-label="Export">
            {choices.map((c) => (
              <Menu.Item key={c.label} className="item" onClick={c.run}>
                <span>
                  {c.label}
                  <span className="sub">{c.sub}</span>
                </span>
              </Menu.Item>
            ))}
          </Menu.Popup>
        </Menu.Positioner>
      </Menu.Portal>
    </Menu.Root>
  );
}
