import { describe, expect, test } from "bun:test";
import { Crosshair, Image, Thermometer } from "lucide-react";
import { renderApp } from "@/test/render";
import { GroupCard } from "./GroupCard";
import { Segmented } from "./Segmented";
import { SettingRow } from "./SettingRow";
import { Switch } from "./Switch";

function described(el: Element): string {
  const ids = (el.getAttribute("aria-describedby") ?? "").split(/\s+/).filter(Boolean);
  return ids.map((id) => document.getElementById(id)?.textContent ?? "").join(" ");
}

describe("SettingRow", () => {
  test("its icon is decorative, its title names the control and its description describes it", () => {
    const { container, getByRole } = renderApp(
      <SettingRow
        icon={Crosshair}
        title="Spot temperature"
        desc="Crosshair with the center reading"
        control={<Switch label="Spot temperature" checked onChange={() => {}} />}
      />,
    );
    const icon = container.querySelector("svg");
    expect(icon?.getAttribute("aria-hidden")).toBe("true");
    const sw = getByRole("switch", { name: "Spot temperature" });
    expect(described(sw)).toBe("Crosshair with the center reading");
  });

  test("a native control and a segmented choice take the row's words too", () => {
    const { getByRole } = renderApp(
      <>
        <SettingRow
          icon={Image}
          title="Denoise"
          desc="Smooths speckle"
          control={
            <select aria-label="Filter">
              <option>Off</option>
            </select>
          }
        />
        <SettingRow
          icon={Thermometer}
          title="Measuring range"
          desc="Switching recalibrates the image"
          value="45 %"
          control={
            <Segmented
              label="Range"
              value="a"
              onChange={() => {}}
              options={[
                { id: "a", label: "Normal", icon: Thermometer },
                { id: "b", label: "Wide" },
              ]}
            />
          }
        />
      </>,
    );
    const select = getByRole("combobox", { name: "Denoise" });
    expect(described(select)).toBe("Smooths speckle");
    // The reading beside the title is not part of the name.
    const seg = getByRole("group", { name: "Measuring range" });
    expect(described(seg)).toBe("Switching recalibrates the image");
  });
});

describe("GroupCard", () => {
  test("is a region named by its header", () => {
    const { getByRole } = renderApp(
      <GroupCard title="On the camera's screen" aside="3 shown">
        <p>rows</p>
      </GroupCard>,
    );
    const card = getByRole("region", { name: "On the camera's screen" });
    expect(card.textContent).toContain("3 shown");
    expect(card.textContent).toContain("rows");
  });
});
