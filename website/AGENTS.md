# Website Agent Guide

Applies to `website/` (publishes `https://www.diy-thermocam.net` via Netlify).

## Current State & Phase 5 Target

- **Current (Phase 2):** Imported directly from `maxritter/diy-thermocam-website` with full git history (Gatsby 5 + legacy Stackbit theme, `yarn run build` → `website/public/`).
- **Phase 5 Target:** Unified **Astro 5 (static) + Starlight (`/docs/*`) + Pagefind + Tailwind CSS v4** sharing `palette.css` with `app/`, single-sourced from `../docs/` and `../hardware/bom/bom.csv`, built with Bun (`bun run build` → `website/dist/`).

## Hard Invariants (Must Never Break)

1. **Preserve all 13 canonical URL routes** (trailing slashes preserved):
   `/`, `/building-instructions/`, `/device-features/`, `/device-images/`, `/software/`, `/thermal-images/`, `/docs/`, `/docs/manual/`, `/docs/hardware/`, `/docs/partlist/`, `/docs/firmware/`, `/docs/raw-data/`, `/docs/serial/`.
2. **Preserve static image paths:** Keep all 93 existing files under `/images/{device,manual,software,thermal,website}/*` (currently `website/static/images/`; `website/public/images/` in Astro) so GitHub READMEs, forums, and external hotlinks never 404.
3. **Preserve Umami analytics:** Keep `<script defer src="https://cloud.umami.is/script.js" data-website-id="efb7f48a-b0e1-46ff-9e74-2a2c1a76e868"></script>` in the document `<head>`.
4. **Fix legacy bugs during Phase 5 rebuild:** Replace leftover Stackbit metadata (`"Welcome to Libris"`, `"A personal theme for Unibit"`), fix broken `/images/icon.png` favicon to `/images/website/icon.png`, and align `package.json` license with repo GPL-3.0.
5. **Single source of truth:** In Phase 5, pull `/docs/*` content from `../docs/` and `/docs/partlist/` from `../hardware/bom/bom.csv` rather than duplicating markdown or part tables inside `website/`.
6. **Cutover gate:** Do not relink Netlify or delete `maxritter/diy-thermocam-website` without Max's explicit approval.
