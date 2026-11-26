# DIY-Thermocam V4.0 Enclosures

![DIY-Thermocam V4.0 in the 3D-printed shell](renders/v4.0-3d-hero-front.jpg)

> **V4 is experimental and not yet tested.** No camera has been built in these enclosures yet.

Two enclosures for the V4.0 board, built from one parametric CAD model: a **3D-printed shell** and a **laser-cut acrylic case**. Both hold the same board stack, so the hardware list and most build steps are shared. You need a PH1 Phillips screwdriver, a 2 mm flat screwdriver for the slotted Lepton screws, and tweezers.

| 3D-printed shell | Laser-cut acrylic |
| --- | --- |
| ![3D-printed shell, back with the lens cap open](renders/v4.0-3d-hero-back.jpg) | ![Laser-cut acrylic case](renders/v4.0-laser-hero.jpg) |
| Order MJF PA12 from a print service, or print at home | Order 3 mm black cast acrylic from a laser-cutting service, plus a few small printed parts |
| Sliding lens cap, capture button, switch slider, captive tripod nut | Six interlocking plates; button and switch keys cut from the same sheet |

![Section through the 3D-printed shell at the Lepton](renders/v4.0-3d-section.jpg)

## Make the 3D-printed shell

Files are in [`exports/3d-printed/`](exports/3d-printed/): one STEP, STL and 3MF per part, plus [`diy-thermocam-v4.0-print-plate.3mf`](exports/3d-printed/diy-thermocam-v4.0-print-plate.3mf) with every part at the quantity one camera needs.

- **Print service (recommended):** order **MJF PA12, dyed black**. At JLC3DP, upload the one file [`diy-thermocam-v4.0-service-plate.3mf`](exports/3d-printed/diy-thermocam-v4.0-service-plate.3mf); it holds each part once, with the four C-spacers joined by break-off tabs. For other services, unzip [`diy-thermocam-v4.0-shell-print-files.zip`](exports/3d-printed/diy-thermocam-v4.0-shell-print-files.zip) and upload its `step/` folder. Snip the spacer tabs and file the nubs flush. `lens_cap`, `capture_button` and `slide_switch_cap` look good in orange.
- **At home:** PETG, 0.4 mm nozzle, 0.2 mm layers, 3–4 walls. Print `back_shell` with supports touching the build plate only: the lens-cap track is a 28 × 58 mm bridge. The other parts print without supports in the orientation of the plate file.

| Part | Qty | Part | Qty |
| --- | :-: | --- | :-: |
| `front_shell` | 1 | `slide_switch_cap` | 1 |
| `back_shell` | 1 | `battery_cradle` | 1 |
| `lens_cap` | 1 | `battery_lid` | 1 |
| `capture_button` | 1 | `cradle_spacer` | 4 |

![Everything for the 3D-printed build](renders/v4.0-3d-kit.jpg)

## Make the laser-cut case

![The acrylic parts as they come off the sheet](renders/v4.0-laser-sheet.jpg)

Upload [`diy-thermocam-v4.0-laser-sheet.svg`](exports/laser-cut/diy-thermocam-v4.0-laser-sheet.svg) (or the [`.dxf`](exports/laser-cut/diy-thermocam-v4.0-laser-sheet.dxf)) and order **3.0 mm black cast acrylic**, for example Acryl GS black. The sheet is 213 × 195 mm, so a 220 × 200 mm blank fits it. Red `#CC0000` lines are cuts; blue `#0066FF` is the optional logo engraving on the back plate. Slots are 0.2 mm wider than the stock, so 2.8–3.2 mm acrylic fits.

The acrylic case also needs these printed parts. Print them at home from [`diy-thermocam-v4.0-laser-print-plate.3mf`](exports/laser-cut/diy-thermocam-v4.0-laser-print-plate.3mf), or order them from a print service with [`diy-thermocam-v4.0-laser-service-plate.3mf`](exports/laser-cut/diy-thermocam-v4.0-laser-service-plate.3mf) (JLC3DP) or [`diy-thermocam-v4.0-acrylic-files.zip`](exports/laser-cut/diy-thermocam-v4.0-acrylic-files.zip), which also holds the laser sheet:

| Part | Qty | What it does |
| --- | :-: | --- |
| `battery_cradle`, `battery_lid` | 1 each | Hold and cover the LiPo (same files as the shell) |
| `cradle_spacer` | 4 | Lift the cradle over the solder joints |
| `tripod_mount` | 1 | Holds the 1/4"-20 nut in the bottom plate |
| `display_spacer` | 2 | Clamp the display between the board stack and the front plate |
| `sd_chute` | 1 | Guides the microSD card from the top plate into the Teensy |

![Everything for the acrylic build](renders/v4.0-laser-kit.jpg)

## Buy the hardware

Everything except the last two rows comes from [Ettinger](https://www.ettinger.de/), which sells packs of 100, so one order covers many cameras.

| Qty | Part | Standard | Ettinger no. | Used in |
| :-: | --- | --- | --- | --- |
| 4 | Hex standoff M2.5 × 20, female/female, SW 5 | – | [005.02.205](https://www.ettinger.de/p/abstandsbolzen-messing-vernickelt-i-i-sw5x20-gewinde-m2-5x6-m2-5x6/005.02.205) | both |
| 4 | Hex standoff M2.5 × 11, male/female, SW 5 | – | [005.12.119](https://www.ettinger.de/p/abstandsbolzen-stahl-rostfrei-blank-i-a-sw5x11-i-m2-5x7-a-m2-5x6-m.-freistich/005.12.119) stainless, in stock; or [005.12.115](https://www.ettinger.de/p/abstandsbolzen-messing-vernickelt-i-a-sw5x11-i-m2-5x6-a-m2-5x6-m.-freistich/005.12.115) brass | both |
| 4 (+8) | Washer M2.5, nylon, 2.7/6.0 × 0.5 mm | ISO 7089 | [003.03.026](https://www.ettinger.de/p/unterlegscheibe-iso7089-oe2-7-6-0-x-0-5-pa6.6-natur/003.03.026) | both (+8 under the acrylic case's screws) |
| 4 | Countersunk screw M2.5 × 8, Phillips | ISO 7046 | [001.12.238](https://www.ettinger.de/p/senkkopfschraube-iso7046-m2-5x8-stahl-rostfrei-a2-blank-phillips/001.12.238) | shell, back |
| 4 | Countersunk screw M2.5 × 14, Phillips | ISO 7046 | [082.22.212](https://www.ettinger.de/p/senkkopfschraube-iso7046-m2-5x14-stahl-rostfrei-a2-blank-phillips/082.22.212) | shell, front |
| 4 | Pan head screw M2.5 × 8 | ISO 7045 | [001.17.232](https://www.ettinger.de/p/flachkopfschraube-iso7045-m2-5x8-stahl-rf-a2-blank-phillips-linsenkopf/001.17.232) | acrylic, back |
| 4 | Pan head screw M2.5 × 14 | ISO 7045 | [001.17.298](https://www.ettinger.de/p/flachkopfschraube-iso7045-m2-5x14-stahl-rf-a2-blank-phillips-linsenkopf/001.17.298) | acrylic, front |
| 4 | Cheese head screw M2 × 18, slotted | ISO 1207 | [001.14.177](https://www.ettinger.de/p/zylinderkopfschraube-iso1207-m2x18-stahl-4.8-verzinkt-schlitz/001.14.177) | both (Lepton) |
| 4 | Washer M2, nylon, 2.2/5.0 × 0.3 mm | – | [003.03.010](https://www.ettinger.de/p/unterlegscheibe-oe2-2-5-0-x-0-3-pa6.0-schwarz/003.03.010) | both (Lepton) |
| 4 | Spacer sleeve 2.2/4.0 × 12 mm, nylon | – | [005.81.120](https://www.ettinger.de/p/distanzhuelse-oe2-2-4-0-x-12-0-pa6.0-gv25-schwarz/005.81.120) | both (Lepton) |
| 4 | Hex nut M2 | ISO 4032 | [002.10.019](https://www.ettinger.de/p/sechskantmutter-iso4032-m2-stahl-rostfrei-a2-blank/002.10.019) | both (Lepton) |
| 1 | Hex nut 1/4"-20 UNC, 7/16" across flats, 5.5 mm | ASME B18.2.2 | [zollshop.de](https://www.zollshop.de/Zollmuttern-Scheiben-Federringe/Zollmuttern-UNC-UNF/Muttern-14-Zoll/14-Zoll-UNC/1061/1/4-Zoll-20-UNC-Sechskantmutter-Stahl-verzinkt/) | both (tripod) |
| 1 | Right-angle female header 2 × 10, 2.54 mm | – | [Sullins PPPC102LJBN-RC](https://www.digikey.com/en/products/detail/sullins-connector-solutions/PPPC102LJBN-RC/776021) | both (Lepton to J2) |

The nylon washers on the rear standoffs keep their hex corners off the capture and Prog button pads on the back of the board. The small nylon washers keep the M2 screw heads off the tracks around the Lepton holes. The right-angle header bridges the 16 mm between the Lepton breakout's header and J2. The battery and modules are in the [main parts list](../bom/README.md).

## Build the 3D-printed shell

The board and battery steps are the same for the acrylic case. Orange marks the parts you add in each step.

| | |
| --- | --- |
| ![Step 1](renders/assembly-3d-01-teensy.png) | ![Step 2](renders/assembly-3d-02-lepton-screws.png) |
| **1. Teensy.** Push the Teensy 4.1 into its two sockets on the back of the board, USB end toward the bottom edge. | **2. Lepton screws.** From the display side, push the four M2 × 18 screws through the Lepton holes, each with a nylon M2 washer under its head. |
| ![Step 3](renders/assembly-3d-03-lepton.png) | ![Step 4](renders/assembly-3d-04-back-shell-parts.png) |
| **3. Lepton.** Turn the board over. Slide a 12 mm sleeve onto each screw. Snap the Lepton into the breakout, plug the right-angle adapter onto the breakout's header, then lower the breakout onto the screws so the adapter's legs go into J2. Fit the four M2 nuts finger-tight. | **4. Back shell.** Slide the 1/4"-20 nut into its pocket from the open side. Drop the capture button and the switch slider into their slots. |
| ![Step 5](renders/assembly-3d-05-rear-standoffs.png) | ![Step 6](renders/assembly-3d-06-board.png) |
| **5. Rear standoffs.** Stand the four 20 mm standoffs in their seats and screw them on from outside with the M2.5 × 8 countersunk screws. | **6. Board.** Put a nylon M2.5 washer on each standoff, then lower the board onto them. The slide switch drops into the slider's fork. |
| ![Step 7](renders/assembly-3d-07-mid-standoffs.png) | ![Step 8](renders/assembly-3d-08-spacers.png) |
| **7. Middle standoffs.** Screw the four 11 mm standoffs through the board into the standoffs below. | **8. C-spacers.** Slip a C-spacer over each 11 mm standoff. Turn the gap toward the Teensy socket at the lower right and toward the capture button at the upper right. |
| ![Step 9](renders/assembly-3d-09-battery.png) | ![Step 10](renders/assembly-3d-10-display.png) |
| **9. Battery.** Lay the cradle on the spacers and the LiPo in the cradle. Run the wires through the gap in the cradle's right wall and round the right board edge to J4 on the back. Plug it in now; J4 is out of reach later. Put the lid on. | **10. Display.** Press the display onto its 2 × 20 header. It rests on the 11 mm standoffs. |
| ![Step 11](renders/assembly-3d-11-front-shell.png) | ![Step 12](renders/assembly-3d-12-lens-cap.png) |
| **11. Front shell.** Fit the front shell. Its four bosses press the display onto the standoffs; tighten the M2.5 × 14 countersunk screws until snug. | **12. Lens cap.** Slide the cap into its track from the top. It clicks at the open and closed positions. |

![The 3D-printed shell, exploded](renders/v4.0-3d-exploded.jpg)

## Build the laser-cut case

| | |
| --- | --- |
| ![Step 1](renders/assembly-laser-1-back-plate.png) | ![Step 2](renders/assembly-laser-2-tripod.png) |
| **1. Back plate.** Screw the four 20 mm standoffs to the back plate with the M2.5 × 8 pan head screws, a nylon washer under each head. | **2. Bottom plate and tripod mount.** Slot the bottom plate into the back plate. Press the 1/4"-20 nut into the tripod mount past its two lips, then set the mount into the hole in the bottom plate. |
| ![Step 3](renders/assembly-laser-3-stack.png) | ![Step 4](renders/assembly-laser-4-keys.png) |
| **3. Board stack.** Follow shell steps 1–3 and 6–10. The board slides down past the tripod mount's fin, which then holds the mount in place. | **4. Keys.** From the inside, push the button key into the small slot in the top plate and the switch key into the slot in the right plate. |
| ![Step 5](renders/assembly-laser-5-sides.png) | ![Step 6](renders/assembly-laser-6-front-plate.png) |
| **5. Side plates.** Hold each key in its slot and slot the top, left and right plates into the back plate. The switch key's fork goes over the slide switch. | **6. Front plate.** Lay the two spacer bars on the display's mounting holes, put the front plate on and tighten the M2.5 × 14 pan head screws, a nylon washer under each head. |
| ![Step 7](renders/assembly-laser-7-sd-chute.png) | |
| **7. microSD chute.** Push the chute into the slot in the top plate until it clicks. Insert the card through it with tweezers. | |

![The laser-cut case, exploded](renders/v4.0-laser-exploded.jpg)

## Change the design

The CAD lives in [`cad/`](cad/) as a Python [build123d](https://github.com/gumyr/build123d) package. Positions come from the KiCad board, [`../pcb/diy-thermocam-v4.0.kicad_pcb`](../pcb/diy-thermocam-v4.0.kicad_pcb); the parts inside the camera are modelled from their datasheets in [`cad/src/thermocam_case/keepout.py`](cad/src/thermocam_case/keepout.py).

```bash
cd hardware/enclosure/cad
uv sync --extra dev --python 3.12
uv run pytest                  # clearances to every part, button and switch travel, screw engagement, Lepton field of view
uv run thermocam-case-export   # rebuild exports/ (STEP, STL, 3MF, SVG, DXF)
uv run thermocam-case-render   # rebuild renders/ with Blender and the KiCad 3D board
```

Rendering needs [Blender](https://www.blender.org/) 4.2 or newer (`brew install --cask blender`) and KiCad 10. [`cad/blender/render_scene.py`](cad/blender/render_scene.py) defines every photo and build step.
