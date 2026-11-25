# DIY-Thermocam 2026 Overhaul Plan

Comprehensive roadmap to modernize DIY-Thermocam into a single, agent-friendly 2026 monorepo covering Teensy 4.1 firmware, a clean-slate USB v2 protocol, a cross-platform Tauri v2 desktop app, a QualityLayer-inspired website + searchable documentation site, and open-source KiCad + parametric CAD hardware.

> **Status:** Draft plan for Max's review on branch `overhaul-plan`. Local tool installs via `brew` / `uv` (`git-filter-repo`, `kicad`, `gerbv`, `build123d`) are pre-approved for implementation phases. All outward-facing actions (pushes, GitHub comments/closes, emails, Netlify live cutover, and deleting the old website repo) remain gated on Max's explicit approval.

---

## 1. Goals & Guiding Principles

- **Single 2026 agentic-friendly monorepo (QualityLayer-inspired):** Consolidate firmware, desktop app, hardware (PCB, enclosure, BOM), documentation, and website into `maxritter/diy-thermocam` with zero duplicated sources of truth (docs, BOM CSV, colormaps, protocol spec, shared `palette.css` design tokens) and concise `AGENTS.md` files per subsystem.
- **2026 Buildable-by-Anyone Hardware North Star:** Anyone can build DIY-Thermocam again via **one-stop fabrication services** (Seeed Studio Fusion PCB/3D/CNC, JLCPCB/PCBA/3D, PCBWay) **or their own tools** (home 3D printer, laser cutter, hand soldering), backed by verified, best-price BOM links kept fresh by automated CI link-checking.
- **Hardware & product proof at every gate:** The real DIY-Thermocam V3 is connected and available for hardware verification. Unit tests and host simulations support—never replace—end-to-end verification on the real camera, real desktop app, and built site.
- **Zero legacy wire compromises:** Clean-slate binary USB Protocol v2 replaces the blocking 1-byte v1 serial protocol; `.DAT` radiometric file compatibility stays 100% intact across V1/V2/V3.
- **Strict outward-facing approval gates:** Sending Ron Brown's recovery email, posting/closing GitHub issues and PRs, pushing branches, relinking Netlify, and deleting `maxritter/diy-thermocam-website` all wait for Max's explicit sign-off.

---

## 2. Monorepo Layout, README Style & Website Migration

### Target Directory Structure

Modeled on QualityLayer's clean monorepo conventions (`AGENTS.md` + thin `CLAUDE.md` pointer, shared design tokens across app/website/docs, `CONTRIBUTING.md`, `CHANGELOG.md`):

```text
diy-thermocam/
├── README.md                 # QualityLayer-style visual README (2-step git mv from README.MD on APFS)
├── AGENTS.md                 # Repo-wide agent index & cross-cutting invariants
├── CLAUDE.md                 # Thin pointer to AGENTS.md (unless RuleSync is opted in)
├── CONTRIBUTING.md           # Build, test, and hardware contribution guide
├── CHANGELOG.md              # Unified release history
├── LICENSE                   # GPL-3.0
├── netlify.toml              # Root monorepo Netlify config
├── .github/workflows/        # ci.yml (path-filtered) + release.yml (firmware-v* / app-v*)
├── firmware/                 # AGENTS.md, README.md, 3.0/ (sketch.yaml, src/{core,...}, test/, tests_host/)
├── app/                      # Tauri v2 + Rust (thermocam-core) + React 19 + Tailwind v4 + Bun
├── hardware/                 # AGENTS.md, README.md
│   ├── pcb/                  # KiCad 9/10 project (V3.0 1:1 port + optional V3.1/V4) & legacy Eagle archive
│   ├── enclosure/            # build123d parametric CAD (uv) -> SVG/DXF + STEP/STL/3MF
│   └── bom/                  # Single-source bom.csv + schema.json
├── docs/                     # Single-source docs (overhaul-plan.md, issue-triage.md, protocol-v2.md,
│                             #   raw-data-format.md, manual.md, building-instructions.md, hardware.md, firmware.md)
└── website/                  # Imported from diy-thermocam-website (Astro 5 marketing + Starlight /docs/ site)
```

- **Root `README.md` direction (inspired by QualityLayer):** Centered light/dark `<picture>` brand mark, crisp tagline, `for-the-badge` release/downloads/stars badges, inline jump bar (`Build · Features · Docs · Website · Releases · Issues`), OS-marked Desktop App download CTA, light/dark product & thermal hero visuals, scannable capability tables, collapsible `<details>` sections for secondary paths, and clean footer.
- **`software/` retirement plan:** Currently 66 MiB of binaries (`ffmpeg.exe` 33 MiB + 3 Windows-only zips + Python `thermal_live_viewer`). Keep `software/thermal_live_viewer/3.0/` working until the Tauri app reaches live-view + `.DAT` analysis + export parity. At Tauri v1.0, delete `software/` from `HEAD` and attach the legacy Windows tools (`ThermoVision.zip`, `ThermalDataViewer`, `VideoConverter`) to a pinned `legacy-software-v3.0` GitHub Release linked from `/software/` and `README.md`. Do **not** rewrite `diy-thermocam` git history (preserves tags `2.4.8`–`3.0.4` and external links). Update internal/website links when relocating `pcb/3.0` and `enclosure/3.0` under `hardware/`.

### Website History Import (`git filter-repo` vs `git subtree`)

Importing `diy-thermocam-website` (890 commits) adds only **19.3 MiB (+6.3%)** to the existing 304 MiB git pack.

| Approach | History & `git blame` | Risk / Friction | Verdict |
|---|---|---|---|
| **`git filter-repo --to-subdirectory-filter website` + `--allow-unrelated-histories` merge** | Every website commit sits cleanly under `website/`; `git log website/<file>` and `git blame` work natively. | Runs on a scratch clone only (zero rewrite on `diy-thermocam`); `git-filter-repo` pre-approved via `brew`. | **Recommended** (clean one-way migration before deleting old repo) |
| `git subtree add` | Historical commits remain at repo root `/`, breaking naive path-scoped history queries. | Also missing from macOS stock git; `subtree push` is unneeded once old repo is deleted. | Skip |

Merge `arduino-cli-build` (and Teensy 1.62.0) into `master` first, then run the single `--no-ff` import commit (zero path collisions).

### Netlify Configuration, Cutover & Old Repo Deletion

Netlify is already configured as an Antigravity connector (and `netlify-cli` is available); live cutover and deletion of `diy-thermocam-website` remain gated on Max's final confirmation once Phase 5 is live-verified.

- **Root `netlify.toml`:**
  - `base = "website"`, `command = "bun run build"`, `publish = "dist"` (resolves to `website/dist`).
  - `ignore = "git diff --quiet $CACHED_COMMIT_REF $COMMIT_REF -- website/ docs/ hardware/bom/"` — skips deploys on firmware/app-only commits while rebuilding on doc or BOM edits (full monorepo checkout gives Astro read access to `../docs/` and `../hardware/bom/bom.csv`).
  - `[[redirects]]`: `/images/icon.png` → `/images/website/icon.png` (`301`), apex → `www.diy-thermocam.net` (`301`), `/bom/*` → `/docs/partlist/` (`301`), keeping all 13 canonical routes at HTTP `200`.
  - `[[headers]]`: immutable long-lived caching on `/_astro/*` and `/images/*`.
- **Analytics preservation:** Carry over Umami (`https://cloud.umami.is/script.js`, `data-website-id="efb7f48a-b0e1-46ff-9e74-2a2c1a76e868"`).
- **Cutover & deletion sequence:** Merge `website/` to `master` → relink `www.diy-thermocam.net` to `maxritter/diy-thermocam` via Netlify connector/CLI → verify all 13 routes HTTP 200, `/docs/` search, `/images/*` hotlinks, Umami beacon, OG tags, sitemap, and `robots.txt` → **after Max's final confirmation**, delete `maxritter/diy-thermocam-website` on GitHub (and remove the local clone if desired).

---

## 3. Technology Stack & Rationale

### Website & Documentation Site (`website/`)
- **QualityLayer design & docs inspiration:** QualityLayer pairs a crisp, flat-surface marketing site (`docs/site/`, Vite + React 19 + Tailwind v4 + shadcn/Base UI) with a full-featured documentation site (`docs/docusaurus/`, Docusaurus 3.10 at `/docs/`) sharing a single **`palette.css`** + **Geist / Geist Mono** light/dark design system, categorized sidebar, right-hand TOC, local offline search, GitHub "Edit this page" links, and strict broken-link build errors.
- **Recommended stack: Unified Astro 5 (static) + Starlight (`/docs/*`) + Pagefind + Tailwind CSS v4 (`@tailwindcss/vite`) + React 19 islands + MDX + `@astrojs/sitemap` + Bun + Biome.**
  - **Why unified Astro 5 + Starlight over QualityLayer's separate Vite + Docusaurus split:** Delivers the **exact QualityLayer website + documentation site experience**—bespoke `impeccable`-crafted landing/showcase pages alongside a structured, categorized `/docs/` site with right-hand TOC, Cmd+K offline search (**Pagefind**), light/dark toggle, prev/next nav, and GitHub edit links—in **one Bun package, one `palette.css` shared with the Tauri app, one build, and one sitemap** (avoiding QualityLayer's React 18/19 dual toolchain and `build-all.sh` asset-merging glue). *(Option to mirror QualityLayer's exact Vite + Docusaurus split instead is listed in §8.)*
- **Single-source & 100% URL fidelity:**
  - Astro Content Layer (`glob({ base: '../docs' })`) renders repo markdown directly into the `/docs/` sidebar (`Get Started`, `Hardware & Building`, `Firmware & Protocol`, `Desktop App`, `Reference`); a Zod-validated loader parses `../hardware/bom/bom.csv` to power an interactive filter/copy `/docs/partlist/` table (+ CSV download).
  - `trailingSlash: 'always'` preserves all **13 legacy Gatsby routes** (`/`, `/building-instructions/`, `/device-features/`, `/device-images/`, `/software/`, `/thermal-images/`, `/docs/`, `/docs/manual/`, `/docs/hardware/`, `/docs/partlist/`, `/docs/firmware/`, `/docs/raw-data/`, `/docs/serial/`). Keep all 93 files (12 MiB) in `website/public/images/` verbatim for external hotlinks while serving Astro-optimized WebP/AVIF on-page. Fix legacy Stackbit/Gatsby bugs (`"Welcome to Libris"`, `"A personal theme for Unibit"`, broken `/images/icon.png` favicon, `package.json` MIT → GPL-3.0).

### Desktop App (`app/`)
- **Stack (mirrored from QualityLayer):** **Tauri v2 + Rust** (`app/crates/thermocam-core` headless library + thin `app/src-tauri` shell) + **React 19 + Tailwind CSS v4 + shadcn/ui (`base-nova`, `@base-ui/react`, not Radix) + Vite + Bun + Biome + Knip**, sharing the project's `palette.css`, Geist / Geist Mono, and desktop-first 3-pane layout (`--control-h: 32px`).
- **Core pipeline:**
  - **Transport:** Rust `serialport` (MPL-2.0) behind a `Transport` trait (`SerialTransport` + `MockTransport` for deterministic tests).
  - **Bit-exact palettes & `.DAT`:** `build.rs` codegen parses `firmware/3.0/src/general/colorschemes.cpp` + `globaldefines.h` + `thermal.cpp` (all 19 firmware LUTs with exact 192/224/240/256 lengths) plus app-only perceptually uniform palettes (`Inferno`, `Viridis`, `Magma`, `Plasma`). Full V1/V2/V3 `.DAT` reader/writer (all 4 valid sizes: `9,621`, `10,005`, `38,421`, `38,805` B + legacy 192-grid point reader), keeping `.DAT` 100% camera-compatible and storing non-destructive ROIs, emissivity, and annotations in `<stem>.thermo.json`.
  - **60 fps rendering:** Stream raw `u16` frame + 64-byte metadata over Tauri v2 `Channel<InvokeResponseBody::Raw>` (~345 KB/s at 9 fps, zero JSON/Base64 overhead). Colorize, zoom/pan, drag level/span, and highlight isotherms in a lightweight **WebGL2** fragment shader (with automatic Canvas 2D fallback), while `thermocam-core` runs the matching CPU pipeline for exports and ROI math.
  - **Zero-FFmpeg video:** **`openh264`** (BSD-2-Clause, compiled from bundled source) + **`mp4`** (MIT) for universal H.264 MP4; **`gif`** (MIT/Apache-2.0) for animated GIF; plus lossless `.DAT` + `manifest.json` radiometric sequences.
  - **Native HalfKay HID flasher:** Pure Rust using **`hidapi`** (MIT, VID `0x16C0` / PID `0x0478`, 1088-byte Teensy 4.1 reports) + **`ihex`** (MIT/Apache-2.0) with SHA-256 and target verification—zero `tycmd`/`teensy_loader_cli` dependency.

### PCB & Hardware (`hardware/pcb/`, `hardware/bom/`)
- **1:1 Eagle 9.6.2 → KiCad 9/10 port first:** One-time KiCad GUI import of `pcb/3.0/pcb.{sch,brd}` (21 THT parts, 89.4×68.4 mm, 2-layer 1.6 mm, 8/10/12 mil rules), auditing known V3 quirks (`LASER1` refdes = charger LED, `BUTTON1` = `LIPO (+/-)`, floating `N$2`/`N$3` pours, 2.7 vs 2.8 mm corner mounting hole). Verify via:
  1. `kicad-cli sch erc` & `kicad-cli pcb drc` → 0 errors.
  2. Automated 1:1 netlist diff vs Eagle `<signals>` (all 31 nets / 21 parts).
  3. `gerbv` 600–1200 DPI raster XOR diff vs `pcb/3.0/gerber.zip` + drill XY/diameter check (≤0.05 mm).
  4. `kicad-cli pcb export step` fit check inside the parametric enclosure.
- **Agentic PCB workflow & `assalas/pcb-designer-ai-agent` assessment:**
  - **Do NOT adopt `assalas/pcb-designer-ai-agent`:** It uses a custom non-commercial license incompatible with GPL-3.0/kit builders, and it is a greenfield LLM text-to-board generator (SKiDL/Genkit) unsuitable for exact Eagle→KiCad porting or precision revisions.
  - **Adopt instead:** Native KiCad 9/10 S-expression files as git-diffable source of truth, scripted **`kicad-cli`** fabrication pipeline emitting standard **Gerber + Excellon drills + BOM + CPL (pick-and-place) + STEP + SVG/PDF** compatible with **Seeed Studio Fusion PCB, JLCPCB, and PCBWay** (bare PCB or optional PCBA), **`kicadiff`** for visual PR diffs, and optional **`kipy`**/`kicad-skip` or KiCad MCP for assisted layout edits.
- **Single-source BOM (`hardware/bom/bom.csv` + `schema.json`):** Feeds both KiCad and `/docs/partlist/` with MPN, LCSC/Seeed OPL part, PCBA tier, verified best-price primary/alt suppliers, and status—validated on every PR with **automated weekly CI link-checking (`lychee`)** so no dead supplier links accumulate.
  - **Active & kept:** Teensy 4.1, FLIR Lepton 3.5/2.5 (3.1R currently sold out), Lepton Breakout V2.0 (`250-0577-00`), BuyDisplay `ER-TFTM032-3` resistive (fix misleading "capacitive" URL slug), Pololu `U3V16F5`, active DigiKey THT parts (add backup 12 mm push button & generic 2×20 female header).
  - **Obsolete/problematic replacements (for V3.1/V4 & BOM docs):**
    - **TP4057 (obsolete, #103) → TI `BQ24075` + USB-C (2× 5.1 kΩ CC pulldowns):** True dynamic power-path (1.5 A in / 1 A charge, clean termination while running ~300–450 mA camera—unlike TP4056 which lacks power-path, MCP73831 at 500 mA max, or IP5306 auto-shutoff). Routing USB-C D+/D- to Teensy pads also eliminates the error-prone VUSB trace cut (#96).
    - **Samsung `EB-BG355BBE` (legacy clones only) → protected 3.7 V 2000 mAh `103450` LiPo with JST-PH 2-pin**.
    - **Broken webcam cover (#101) → integrated snap/slide lens cap** in the 3D-printed enclosure; regional fastener links → standard ISO/DIN specs.

### Enclosure CAD (`hardware/enclosure/`)

| Criterion | **`build123d` (Recommended)** | CadQuery | OpenSCAD |
|---|---|---|---|
| Kernel & STEP I/O | OCCT B-Rep (**STEP export + KiCad `pcb.step` import**) | OCCT B-Rep (STEP I/O) | Mesh CSG (**no STEP**) |
| 2D Laser + 3D Print | Native `BuildSketch` → SVG/DXF + 3D STEP/STL/3MF from one model | Supported | Separate 2D/3D flows |
| Agent & CI ergonomics | Explicit Pythonic API; `pytest` interference & FOV checks; headless PNG renders | Implicit fluent stack state | Custom DSL; brittle fillets |
| 2026 macOS arm64 / CI | Clean `uv` wheels (`cadquery-ocp`) | Heavier conda/wheel setup | CLI PNG only |

- **Community 3D-printable `3.0b` integration (`helmarw/DIY-Thermocam` + #93/#101):** Build both improved 6-plate laser-cut acrylic (**SVG/DXF** for home/shop laser cutters) and a 2-piece 3D-printable/CNC-ready shell (**STEP/STL/3MF** ready for Seeed Studio Fusion 3D/CNC, JLCPCB 3D, PCBWay, or home 3D printers) from one `build123d` package—widening the Lepton aperture (≥8 mm, eliminating top-edge thermal vignetting), aligning the USB cutout and charge-LED hole, countersinking M2.5 screws, adding a 1/4"-20 tripod heat-set/T-nut pocket, increasing switch/wire clearance, adding a Teensy Prog pinhole, and integrating a lens cap.

---

## 4. Firmware Overhaul & New USB Protocol (v2)

### Teensyduino 1.62.0 Merge, Bug Fixes & Performance
- **Teensy 1.62.0 merge (Phase 1):** Rebase and merge the existing worktree (`363f8a8`, `93165e5` — deletes 170+ vendored files / -57,504 lines, upgrades to GCC 15.2, stock `Metro`, core `MTP_Teensy.h`) onto `arduino-cli-build`. Flash real V3 camera, run the 34-test suite against `baseline-3.0.4.json`, verify MTP manually, and clean up the 38 pre-existing compiler warnings.
- **Confirmed bugs fixed during overhaul:**
  - `#119`: Lepton VoSPI uses 25 MHz `SPI_MODE1` vs Lepton 3.x max 20 MHz `SPI_MODE3` (test 16–20 MHz `SPI_MODE3` on real hardware); fix `lepton_reset()` VSYNC/CS handling and infinite I2C busy-waits.
  - `#115`: `display_init()`/`touch_init()` falsely pass on `0x00`, breaking headless USB serial mode.
  - `#110`: Switch `SdFat32` → `SdFs` (exFAT/SDXC >32 GB) + `uint32_t` free-space calculation.
  - Core hygiene: `convertColors()` `min==max` divide-by-zero guard; remove up to 1000 ms busy-waits in `touchIRQ()`/`buttonIRQ()` and 9 ms SPI1 polling in VSYNC ISR; fix `floatToChar()` dropped minus sign for `-1 < x < 0`; stop per-frame EEPROM reads in manual mode.
- **Performance roadmap (baseline → target):**
  - **Command latency (`114 ms` → `<2 ms`) & Display FPS (`4.39 fps` → `~8.78 fps`):** Double-buffer `smallBuf[2]` (+38.4 KB) so Lepton VSYNC fills `back` while the loop immediately services USB and renders `front`.
  - **DMA display writes:** Move double-buffered `bigBuffer[2]` (307 KB) to Teensy 4.1 OCRAM (`DMAMEM`) and flush over LPSPI4 via `DMAChannel` (separate from Lepton `SPI1`, removing `disableSPIIRQ`; test 40–60 MHz display clock, as video mode already runs 60 MHz).
  - **Integer/LUT pipeline & bulk I/O:** Precompute 256-entry RGB565 LUT, fixed-point 2× bilinear & box/gaussian filters, fused min/max scan, bulk `Serial.write(buf, len)` (<2 ms per 38.4 KB frame), and 4–8 KB buffered `.DAT` SD writes + cached free space (**save time `313 ms` → `<40 ms`**).
  - **Timing telemetry:** `ARM_DWT_CYCCNT`/`micros()` counters (`acquire_us`..`loop_total_us`, drops, resyncs, CRC errors, `tempmonGetTemp()`, free DTCM/OCRAM).
- **Why keep 450 MHz default (vs 600 MHz):** Verified in Teensyduino 1.62 `clockspeed.c:74-82`—below 460 MHz core DCDC stays at **1150 mV** vs **1250 mV** at 600 MHz, cutting CPU heat/power by ~36% (600 MHz draws ~1.57×) inside a closed enclosure right next to the Lepton sensor and LiPo. At 8.7 Hz (114 ms budget), 450 MHz has ample headroom once blocking I/O is removed.
- **Host-side C++17 unit tests:** Extract hardware-independent logic into `firmware/3.0/src/core/` (zero `<Arduino.h>`) tested on CI in `firmware/3.0/tests_host/`: Protocol v2 codec/CRC/resync, temperature math, thermal pipeline/LUTs/colormaps, Lepton packet index mapping (all 8 sensor/rotation combos), and `.DAT`/`.BMP`/EEPROM/float serialization.

### Clean-Slate USB Protocol v2
- **Framing (length-prefixed LE binary, 20 B overhead, zero-copy pixels):**
  - 16-byte header: `magic[4] = "TC2\0"`, `version (2)`, `msg_type`, `flags` (`ACK_REQ`, `ERROR`, `MORE_CHUNKS`, `COMPRESSED`), `hdr_crc8` (CRC-8/SMBUS over header for instant false-magic rejection on mid-stream resync), `seq: u16`, `status_or_sub: u16`, `payload_len: u32` (max 163,840 B) + `payload` + 4-byte IEEE `crc32`.
- **Capabilities & lifecycle (`0x01–0x03`):** `HELLO_REQ/RESP` negotiates version, firmware build, hardware rev, Teensy UID, sensor/display geometry, diagnostic masks, and capability flags; `PING` heartbeat (5 s timeout) and USB DTR drop cleanly exit back to standalone live mode.
- **Push streaming (`0x10`, `0x80–0x81`):** `STREAM_CTRL` enables native-rate `FRAME_RAW` (64-byte LE metadata header: seq, timestamps, drops, geometry, gain/FFC state, min/max + coords, spot, cal offset/slope, FPA temp, battery + raw `u16` LE pixels + optional sparse temp points) and optional `FRAME_RENDERED` mirror, with non-blocking backpressure drop counting.
- **Seq-matched concurrent commands (`0x11–0x19`):** Serviced during live streaming—full `GET/SET_CONFIG` (with RAM-only vs EEPROM `persist` flag), `SET_TIME` (RTC sync), `TRIGGER_FFC`, sparse `GET/SET_TEMP_POINTS`, `SAVE_CAPTURE`, `GET_DIAGNOSTICS`, `GET_PERF`, and optional Lepton CCI emissivity/T_bkg.
- **Async events (`0xC0–0xC5`):** `EVT_BUTTON` (short/long press for PC capture/record), `EVT_TOUCH`, `EVT_BATTERY`, `EVT_FFC`, `EVT_SESSION_EXIT`, and optional 1 Hz `EVT_PERF`.
- **SD file access (`0xE0–0xE5`, replacing MTP):** `FS_INFO`, paginated `FS_LIST`, `FS_STAT`, windowed 8 KB `FS_READ` (~5–15 MB/s over USB HS + SDIO), and root-jailed `FS_DELETE`. Removes the intrusive MTP plug-in prompt (macOS lacks native MTP; avoids MTP 1000-file cache limits and `usb=serialmtp` core blockers); move MTP to Settings during transition, then switch `sketch.yaml` to clean `usb=serial`.
- **Bootloader & cutover (`0xF0`):** `ENTER_BOOTLOADER` (magic `0xB00710AD`) cleanly invokes `_reboot_Teensyduino_()` → HalfKay (`16C0:0478`); the desktop app also retains Teensy's built-in USB CDC **134-baud** trigger (`usb.c:841`) to upgrade legacy v3.0.x cameras without touching the hardware button. During Phase 3, temporarily auto-detect `0x64` (v1) vs `"TC2\0"` (v2) while porting `firmware/3.0/test` + adding v2 stream/SD/telemetry/bootloader tests, then delete v1 upon cutover (configurable in §8).

---

## 5. Unified Desktop App Feature Set

### Open-Source Thermal Analysis References (Clean-Room GPL-3.0 Implementation)

| Project | License | GPL-3.0 Compat. | Standout Ideas Adopted |
|---|---|---|---|
| **ThermoVision_JoeC** (`JoeC-de/ThermoVision_JoeC`) | **MIT** | Yes | Measurement table/grid, Time Plotter, TempSwitch threshold triggers, measurement-safe filters, 1×1/3×3 spots, `ShowRaw`, custom 256-px LUT import, inspection reports, 16-bit TIFF |
| **Thermal-Image-Analysis** (`detecttechnologies/...`) | **MIT** | Yes | **ROI-driven contrast auto-scaling**, freehand polygon/lasso ROI, full radiometric params (ε, T_refl), Kelvin CSV & 16-bit TIFF |
| **Warmish** (`grazianoEnzoMarchesani/Warmish`) | **GPL-3.0** | Yes | **Per-ROI emissivity overrides** (e.g., shiny metal next to matte PCB in one frame), live ROI stats, JSON session sidecar |
| **P3 Thermal Studio** (`r41nt/p3-thermal-studio`) | **Apache-2.0** | Yes | Zoomed sensor pixel-grid loupe, unified live + sequence radiometric workspace |
| **PyThermalCamera** / **Thermogram** / **GetThermal** | Apache-2.0 / MIT | Yes | Keyboard HUD cycling, dual-view compare, batch heatmap export, Lepton live UX |
| **FLIR Thermal Studio** | Commercial | UX ref only | Draggable level/span directly on colorbar, above/below/interval isotherms |

### Feature Modules
1. **Live View:** Auto-detect/hotplug, push streaming, all 19 firmware LUTs + perceptually uniform extras + custom 256-px LUT import, Auto/Locked/Manual range via draggable colorbar + 256-bin histogram, measurement-safe display filters (None/Box/Gaussian/Median/Bicubic/Lanczos), rotate/flip, WebGL2 zoom/pan + pixel-grid loupe, cursor readout, keyboard shortcuts (`Space`/`S`/`R`/`F`/`L`/`C`).
2. **Measurements, ROIs & Emissivity:** 1×1 and 3×3 spot meters (`ShowRaw` toggle), Box, Circle, and Freehand Polygon/Lasso ROIs, **1-click ROI-driven contrast scaling**, **global + per-ROI emissivity & reflected-temp correction** ($T_{\text{obj}} = ((T_{\text{app}}^4 - (1-\varepsilon)T_{\text{refl}}^4)/\varepsilon)^{1/4}$ + material presets), bidirectional Line Profile chart, 2-point ΔT, hot/cold markers, sortable Measurement Table + N×M grid, °C/°F/K.
3. **Time Plotter:** Live and sequence multi-channel temperature-vs-time chart tracking spots/ROIs/global min-max with event markers and CSV export.
4. **TempSwitch Alarms & Isotherms:** Above/below/interval isotherm highlighting plus threshold rules that auto-save `.DAT`, start recording, or trigger a desktop alert with cooldown.
5. **Capture & Zero-FFmpeg Video:** Instant still capture (`.DAT` + PNG/JPEG + `.thermo.json`), hardware button-triggered capture/record via Protocol v2 async events, PC recording to H.264 **MP4** (`openh264`+`mp4`), **GIF** (`gif`), or lossless radiometric sequence.
6. **SD Card Browser:** Protocol v2 file/sequence listing (no MTP reboot), lazy `.DAT` thumbnails, multi-select download, delete with confirmation, free/total space, 1-click "Open in Editor" or "Convert Sequence to MP4".
7. **Radiometric Editor & Reports:** Open V1/V2/V3 `.DAT`, `.thermo.json`, or sequence folders; non-destructive edits, sequence scrubber/trim, side-by-side compare, annotations, batch export to PNG/JPEG (high-DPI scale bar), temperature CSV, **16-bit radiometric Kelvin TIFF**, `.DAT`, MP4/GIF, and **one-click HTML/PDF/Markdown inspection reports**.
8. **Camera Control & In-App Updater:** Full settings panel (gain, shutter/FFC, filter, overlays, rotation, screen-off, RTC sync, alarms), self-test diagnostics, live firmware timing telemetry, and native Rust HalfKay HID updater (GitHub Releases or local `.hex`, SHA-256 + target verification, auto-reboot & reconnect).

### 3-Layer Testing Strategy
- **Layer 1 (`cargo test` in `thermocam-core`):** `.DAT` round-trip (all 4 sizes + legacy points), palette parity vs `colorschemes.cpp`, radiometry/emissivity/ROI/histogram math, Protocol v2 CRC/resync/fuzzing against fixtures, MP4/GIF output validation, mock-HID HalfKay packet tests.
- **Layer 2 (`bun test` + Playwright + `axe-core`):** Full React UI against a fixture/mock Tauri backend (canvas rendering, ROI drag, sliders, exports, SD browser, shortcuts, overflow, a11y).
- **Layer 3 (Real-camera E2E):** Connected DIY-Thermocam V3 verifying handshake, ≥8.5 fps streaming, FFC, settings round-trip, SD download, hardware button capture, and MP4/PNG/CSV/TIFF exports.

---

## 6. Order of Work & Verification Gates

| Phase | Scope | Verification Gate (What Proves It Done) |
|---|---|---|
| **1. Teensy 1.62.0 Merge** | Rebase/merge Teensy 1.62.0 worktree onto `arduino-cli-build`; fix 38 warnings; verify `#110` `SdFs` readiness. | Compiles cleanly with `arduino-cli`; flashes real V3 camera; 34-test suite passes vs `baseline-3.0.4.json` + manual MTP check. |
| **2. Monorepo + CI Skeleton** | Install `git-filter-repo` (`brew`), import `diy-thermocam-website` history into `website/`; restructure `hardware/` + `docs/`; add `AGENTS.md` + path-filtered `ci.yml` & `release.yml` (`firmware-v*`, `app-v*`). | Clean git history (`git log website/...`); all path-filtered CI checks pass locally/in CI; existing firmware + website builds green. |
| **3. Firmware Perf & Protocol v2** | Double-buffer + DMA display + integer LUT + buffered SD + timing telemetry; fix `#119`, `#115`, `#110` & core bugs; implement Protocol v2 + host C++17 tests + ported hardware test suite. | Host C++17 suite passes; real V3 hardware suite passes with cmd latency `<2 ms`, display/USB `~8.78 fps`, save `<40 ms`, SD read/list & bootloader verified. |
| **4. Tauri v2 Desktop App** | Build `thermocam-core` + Tauri v2 + React 19 UI (all 8 modules incl. zero-ffmpeg MP4/GIF, HTML/PDF reports, 16-bit TIFF, HalfKay HID flasher) with shared `palette.css`. | Layer 1 (`cargo test`) + Layer 2 (`bun test` + Playwright/`axe-core`) pass; Layer 3 verified end-to-end on the real V3 camera (incl. firmware flash & hardware button). |
| **5. Website & Docs Redesign** | Rebuild `website/` in Astro 5 + Starlight (`/docs/*`) + Pagefind + Tailwind v4 single-sourced from `docs/` & `bom.csv`, styled with `impeccable` & shared `palette.css`; retire `software/` to legacy release once app hits v1.0. | `astro check` + `bun run build` + automated check confirming all 13 legacy URLs, `/docs/` search, `/images/*`, Umami, and internal links; preview approved by Max. |
| **6. Hardware Overhaul** | Install `kicad`, `gerbv`, `build123d`; author `hardware/bom/bom.csv` with verified best-price links + CI link-checker; 1:1 Eagle→KiCad V3.0 port (Gerber/Excellon/BOM/CPL for Seeed Fusion/JLCPCB/PCBWay); `build123d` parametric enclosure (SVG/DXF + STEP/STL/3MF); optional V3.1/V4 PCB revision if approved. | KiCad ERC/DRC 0 errors + 1:1 netlist diff + `gerbv` XOR diff (≤0.05 mm) vs V3.0 Gerbers; fab packages complete; `build123d` STEP clearance/FOV tests + BOM link checks pass. *(See prototype caveat below.)* |
| **7. Docs, Releases & Cutovers** | Polish QualityLayer-style `README.md` + `CONTRIBUTING.md`; publish `firmware-v*`, `app-v*`, and `legacy-software-v3.0` releases; execute Max-approved GitHub/email/Netlify cutovers & delete old website repo. | Max's explicit sign-off on each outward action; live `www.diy-thermocam.net` verified on all 13 routes + search + Umami before deleting `maxritter/diy-thermocam-website`. |

> **Physical prototype gate:** Any **new** PCB or enclosure revision (V3.1/V4 USB-C + BQ24075 board or redesigned 2-piece 3D shell) is only considered proven once Max manufactures, assembles, and tests a physical prototype. The verified 1:1 V3.0 KiCad port and V3.0 acrylic files stay intact and buildable as the default until physical verification succeeds.

---

## 7. Issue & PR Cleanup Plan

A complete, read-only audit of all **14 open issues** and **2 open PRs** on `maxritter/diy-thermocam` (`maxritter/diy-thermocam-website` has 0 open issues/PRs; `#103` is already closed) is ready for review in [docs/issue-triage.md](issue-triage.md). It includes the forensic root cause and recovery reply for **Ron Brown (#124)** (V1 Teensy 3.2 accidentally flashed with `Firmware_V2.hex`—not bricked and recoverable in 2 minutes; an **unsent Gmail reply draft** is already saved in Max's Gmail for review), local merge status for PRs **#122** and **#123** (both already merged on `arduino-cli-build`), codebase-verified bug assignments (`#119`, `#115`, `#110`, and optional `#111`), and concise draft replies written in Max's voice.

---

## 8. Open Questions for Max's Approval

1. **Hardware revision & one-stop PCBA scope:** Deliver the verified **1:1 KiCad V3.0 port + verified best-price 2026 `bom.csv` + `build123d` laser/3D/CNC enclosure** first (recommended), or also design a **V3.1/V4 board** in Phase 6 optimized for **Seeed Studio Fusion / JLCPCB / PCBWay** turnkey PCBA + DIY hand-soldering (board-edge USB-C + TI BQ24075 power-path charger + D+/D- tap removing Teensy VUSB cut + protected `103450` LiPo JST-PH, keeping Teensy/Lepton/Display as plug-in headers)?
2. **Legacy USB v1 protocol:** Once the Tauri v2 app and ported v2 hardware regression suite pass on real hardware, perform a **clean v2-only cutover** deleting legacy v1 from firmware 3.1+ (recommended; `.DAT` files stay 100% compatible and legacy v3.0.x cameras still upgrade via 134-baud bootloader), or keep the ~300-LOC v1 fallback behind `0x64` auto-detect?
3. **Website + Docs framework:** Use unified **Astro 5 + Starlight (`/docs/*`) + Pagefind + Tailwind v4** sharing `palette.css` with the Tauri app (recommended—QualityLayer's exact marketing + searchable docs UX in one Bun build without React 18/19 split), or literally mirror QualityLayer's separate **Vite + React 19 site + Docusaurus 3 docs** build?
4. **RuleSync configuration:** Keep plain **`AGENTS.md` + `CLAUDE.md` pointer** (recommended default—matches current repo with zero extra tooling), or initialize RuleSync for `diy-thermocam`?
5. **Outward-facing approvals (when ready):**
   - **(a) Support & GitHub triage:** Review & send the saved unsent Gmail draft (`Re: Bricked DIY-Thermocam problem`) to Ron Brown (`ron@sentsys.com.au`), and approve posting/closing the reviewed GitHub drafts in [docs/issue-triage.md](issue-triage.md) (starting with urgent #124 right away, and pushing/closing #122/#123/#108 once `arduino-cli-build` lands on `master`).
   - **(b) Publishing, Netlify cutover & repo deletion:** Approve pushing overhaul branches, relinking `www.diy-thermocam.net` via the Netlify connector/CLI after Phase 5 preview sign-off, and—once live-verified—deleting `maxritter/diy-thermocam-website`.
6. **Plan adjustments:** Any tweaks to the phase order, desktop app scope (e.g., emissivity on camera vs app-only), or enclosure targets (both laser-cut 6-plate acrylic and 2-piece 3D-printable/CNC shell)?
