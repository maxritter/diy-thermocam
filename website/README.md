# DIY-Thermocam Website

Astro 5 + Starlight source for [www.diy-thermocam.net](https://www.diy-thermocam.net).

Technical pages read directly from `../docs/` and `../hardware/bom/bom.csv` at build time.

## Local commands

```sh
cd website
bun install
bun run dev       # Local dev server
bun run lint      # Biome lint
bun run check     # Astro + TypeScript check
bun run build     # Static production build into dist/
bun scripts/verify-dist.mjs
```
