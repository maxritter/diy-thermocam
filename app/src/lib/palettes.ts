import type { MaterialPreset, Palette } from "./types";

function lerpRamp(
  stops: [number, number, number, number][],
  len = 256,
): [number, number, number][] {
  const out: [number, number, number][] = [];
  for (let i = 0; i < len; i++) {
    const t = i / (len - 1);
    let hi = 1;
    while (hi < stops.length - 1 && stops[hi][0] < t) hi++;
    const a = stops[hi - 1];
    const b = stops[hi];
    const u = b[0] === a[0] ? 0 : Math.max(0, Math.min(1, (t - a[0]) / (b[0] - a[0])));
    out.push([
      Math.round(a[1] + (b[1] - a[1]) * u),
      Math.round(a[2] + (b[2] - a[2]) * u),
      Math.round(a[3] + (b[3] - a[3]) * u),
    ]);
  }
  return out;
}

/** Browser-mode fallback palettes; inside Tauri, exact firmware LUTs from colorschemes.cpp hydrate automatically. */
export const DEFAULT_PALETTES: Palette[] = [
  {
    id: "ironblack",
    label: "Ironblack",
    category: "Firmware",
    firmware_id: 10,
    rgb: lerpRamp([
      [0.0, 0, 0, 10],
      [0.18, 42, 10, 110],
      [0.42, 145, 22, 130],
      [0.65, 225, 85, 28],
      [0.85, 252, 195, 25],
      [1.0, 255, 255, 245],
    ]),
  },
  {
    id: "inferno",
    label: "Inferno",
    category: "Perceptual",
    firmware_id: null,
    rgb: lerpRamp([
      [0.0, 0, 0, 4],
      [0.2, 66, 10, 104],
      [0.4, 147, 38, 103],
      [0.6, 221, 81, 58],
      [0.8, 252, 165, 10],
      [1.0, 252, 255, 164],
    ]),
  },
  {
    id: "viridis",
    label: "Viridis",
    category: "Perceptual",
    firmware_id: null,
    rgb: lerpRamp([
      [0.0, 68, 1, 84],
      [0.25, 59, 82, 139],
      [0.5, 33, 145, 140],
      [0.75, 94, 201, 98],
      [1.0, 253, 231, 37],
    ]),
  },
  {
    id: "magma",
    label: "Magma",
    category: "Perceptual",
    firmware_id: null,
    rgb: lerpRamp([
      [0.0, 0, 0, 4],
      [0.25, 81, 18, 124],
      [0.5, 183, 55, 121],
      [0.75, 252, 137, 97],
      [1.0, 252, 253, 191],
    ]),
  },
  {
    id: "plasma",
    label: "Plasma",
    category: "Perceptual",
    firmware_id: null,
    rgb: lerpRamp([
      [0.0, 13, 8, 135],
      [0.25, 126, 3, 168],
      [0.5, 204, 71, 120],
      [0.75, 248, 149, 64],
      [1.0, 240, 249, 33],
    ]),
  },
  {
    id: "turbo",
    label: "Turbo",
    category: "Perceptual",
    firmware_id: null,
    rgb: lerpRamp([
      [0.0, 48, 18, 59],
      [0.22, 65, 104, 226],
      [0.45, 31, 196, 170],
      [0.68, 164, 252, 60],
      [0.85, 245, 140, 24],
      [1.0, 122, 4, 3],
    ]),
  },
  {
    id: "rainbow",
    label: "Rainbow",
    category: "Firmware",
    firmware_id: 13,
    rgb: lerpRamp([
      [0.0, 18, 0, 108],
      [0.22, 0, 115, 255],
      [0.45, 0, 220, 120],
      [0.68, 255, 225, 0],
      [0.88, 255, 60, 0],
      [1.0, 255, 255, 255],
    ]),
  },
  {
    id: "arctic",
    label: "Arctic",
    category: "Firmware",
    firmware_id: 0,
    rgb: lerpRamp(
      [
        [0.0, 15, 16, 146],
        [0.3, 10, 125, 225],
        [0.55, 180, 235, 250],
        [0.8, 250, 175, 45],
        [1.0, 255, 255, 255],
      ],
      240,
    ),
  },
  {
    id: "lava",
    label: "Lava",
    category: "Firmware",
    firmware_id: 11,
    rgb: lerpRamp(
      [
        [0.0, 6, 8, 20],
        [0.3, 62, 18, 92],
        [0.55, 185, 28, 42],
        [0.82, 248, 165, 20],
        [1.0, 255, 255, 240],
      ],
      240,
    ),
  },
  {
    id: "whiteHot",
    label: "White hot",
    category: "Firmware",
    firmware_id: 17,
    rgb: lerpRamp(
      [
        [0.0, 0, 0, 0],
        [1.0, 255, 255, 255],
      ],
      224,
    ),
  },
  {
    id: "blackHot",
    label: "Black hot",
    category: "Firmware",
    firmware_id: 1,
    rgb: lerpRamp(
      [
        [0.0, 255, 255, 255],
        [1.0, 0, 0, 0],
      ],
      224,
    ),
  },
  {
    id: "medical",
    label: "Medical",
    category: "Firmware",
    firmware_id: 12,
    rgb: lerpRamp([
      [0.0, 12, 14, 24],
      [0.25, 36, 90, 210],
      [0.5, 48, 195, 120],
      [0.75, 245, 210, 35],
      [1.0, 235, 45, 45],
    ]),
  },
];

/** The Material list in Live's Measure tab, in the artifact's words and order. */
export const DEFAULT_MATERIALS: MaterialPreset[] = [
  { id: "skin", name: "Human skin", category: "Biological", emissivity: 0.98 },
  {
    id: "electrical_tape",
    name: "Black electrical tape",
    category: "Electronics",
    emissivity: 0.96,
  },
  { id: "water", name: "Water or ice", category: "Building", emissivity: 0.96 },
  { id: "ic_epoxy", name: "IC package (epoxy)", category: "Electronics", emissivity: 0.94 },
  { id: "paint_matte", name: "Matte paint", category: "Coatings", emissivity: 0.94 },
  { id: "brick", name: "Brick, concrete, plaster", category: "Building", emissivity: 0.93 },
  { id: "matte_pcb", name: "PCB solder mask", category: "Electronics", emissivity: 0.91 },
  { id: "wood", name: "Wood, paper, cardboard", category: "Building", emissivity: 0.9 },
  { id: "anodized_al", name: "Anodized aluminium, black", category: "Metals", emissivity: 0.84 },
  { id: "oxidized_steel", name: "Oxidized steel", category: "Metals", emissivity: 0.79 },
  { id: "oxidized_cu", name: "Oxidized copper", category: "Metals", emissivity: 0.65 },
  { id: "solder_joint", name: "Solder joint", category: "Electronics", emissivity: 0.35 },
  { id: "brushed_al", name: "Brushed aluminium", category: "Metals", emissivity: 0.22 },
  { id: "polished_cu", name: "Polished copper or brass", category: "Metals", emissivity: 0.08 },
];

/** "Matte paint · 0.94", as the Material list shows it. */
export function materialLabel(m: MaterialPreset): string {
  return `${m.name} · ${m.emissivity.toFixed(2)}`;
}

export function samplePalette(palette: Palette, t: number): [number, number, number] {
  const n = palette.rgb.length;
  if (n === 0) return [0, 0, 0];
  const clamped = Math.max(0, Math.min(1, t));
  const f = clamped * (n - 1);
  const lo = Math.floor(f);
  const hi = Math.min(n - 1, lo + 1);
  const u = f - lo;
  const a = palette.rgb[lo];
  const b = palette.rgb[hi];
  return [
    Math.round(a[0] + (b[0] - a[0]) * u),
    Math.round(a[1] + (b[1] - a[1]) * u),
    Math.round(a[2] + (b[2] - a[2]) * u),
  ];
}

export function paletteGradientCss(palette: Palette, direction = "to top"): string {
  const stops: string[] = [];
  const count = 12;
  for (let i = 0; i <= count; i++) {
    const t = i / count;
    const [r, g, b] = samplePalette(palette, t);
    stops.push(`rgb(${r},${g},${b}) ${Math.round(t * 100)}%`);
  }
  return `linear-gradient(${direction}, ${stops.join(", ")})`;
}

export function parseCustomPaletteText(name: string, text: string): Palette {
  const stops: [number, number, number][] = [];
  for (const rawLine of text.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith("# ") || line.startsWith("//")) continue;
    if (line.startsWith("#") && line.length >= 7) {
      const r = Number.parseInt(line.slice(1, 3), 16);
      const g = Number.parseInt(line.slice(3, 5), 16);
      const b = Number.parseInt(line.slice(5, 7), 16);
      if (!Number.isNaN(r) && !Number.isNaN(g) && !Number.isNaN(b)) {
        stops.push([r, g, b]);
      }
      continue;
    }
    const parts = line.split(/[,\s]+/).map((p) => Number.parseInt(p, 10));
    if (parts.length >= 3 && parts.every((v) => !Number.isNaN(v))) {
      stops.push([
        Math.max(0, Math.min(255, parts[0])),
        Math.max(0, Math.min(255, parts[1])),
        Math.max(0, Math.min(255, parts[2])),
      ]);
    }
  }
  if (stops.length < 2) {
    throw new Error("Custom palette requires at least 2 RGB or #RRGGBB stops");
  }
  const normalized: [number, number, number, number][] = stops.map((c, idx) => [
    idx / (stops.length - 1),
    c[0],
    c[1],
    c[2],
  ]);
  const id = `custom_${name.toLowerCase().replace(/[^a-z0-9]+/g, "_")}`;
  return {
    id,
    label: name.trim() || "Custom LUT",
    category: "Custom",
    firmware_id: null,
    rgb: lerpRamp(normalized, 256),
  };
}
