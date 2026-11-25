# Website Agent Guide

Applies to `website/` (publishes `https://www.diy-thermocam.net` via Netlify).

## Architecture (Phase 5 — Active)

- **Stack:** Unified **Astro 5 (static) + `@astrojs/starlight` (`/docs/*`) + Pagefind + Tailwind CSS v4 (`@tailwindcss/vite`) + React 19 islands + MDX + `@astrojs/sitemap` + Bun + Biome** (`license: GPL-3.0`).
- **Design tokens:** `website/src/styles/palette.css` (self-hosted Geist / Geist Mono in `website/public/fonts/`, flat neutral light/dark palette synced via `starlight-theme`, calibrated Ironbow Thermal Orange & precision instrument blue).
- **Commands:**
  - Install: `cd website && bun install`
  - Type & template check: `cd website && bun run check`
  - Lint: `cd website && bun run lint`
  - Production build: `cd website && bun run build` → `website/dist/`
  - Offline internal link check: `lychee --offline --root-dir website/dist website/dist`

## Hard Invariants (Must Never Break)

1. **Preserve all 13 legacy canonical URL routes** (`trailingSlash: 'always'`):
   `/`, `/building-instructions/`, `/device-features/`, `/device-images/`, `/software/`, `/thermal-images/`, `/docs/`, `/docs/manual/`, `/docs/hardware/`, `/docs/partlist/`, `/docs/firmware/`, `/docs/raw-data/`, `/docs/serial/` (plus `/docs/protocol-v2/` and `/docs/desktop-app/`).
2. **Preserve static image paths:** Keep all 93 legacy files under `website/public/images/{device,manual,software,thermal,website}/*` verbatim so GitHub READMEs, forums, and external hotlinks never 404. New brand, OG, and V4 assembly visuals live under `website/public/images/{brand,og,build-v4}/`.
3. **Preserve Umami analytics:** Keep `<script defer src="https://cloud.umami.is/script.js" data-website-id="efb7f48a-b0e1-46ff-9e74-2a2c1a76e868"></script>` in both `BaseLayout.astro` and `astro.config.mjs` Starlight head.
4. **Single sources of truth:**
   - `/docs/*` pages are loaded directly from `../docs/*.md` via `website/src/content.config.ts` (never duplicate documentation markdown inside `website/`).
   - `/building-instructions/` renders directly from `../docs/building-instructions.md`.
   - `/docs/partlist/` reads directly from `../hardware/bom/bom.csv` via `website/src/lib/bom.ts`.
5. **Cutover gate:** Do not relink Netlify or delete `maxritter/diy-thermocam-website` without Max's explicit approval.
