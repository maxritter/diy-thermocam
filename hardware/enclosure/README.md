# DIY-Thermocam V4.0 Enclosures

![DIY-Thermocam V4.0 in the 3D-printed shell](renders/v4.0-3d-hero-front.jpg)

> **V4 is experimental and not yet tested.** No camera has been built in these enclosures yet.

Two enclosures for the V4.0 board, built from one parametric CAD model: a **3D-printed shell** and a **laser-cut acrylic case**. Both hold the same board stack, so the hardware list and most build steps are shared. You need a PH1 Phillips screwdriver, a 2 mm flat screwdriver, a 4 mm nut driver for the Lepton screws, adhesive tape and fine tweezers. The hex standoffs need a 5 mm nut driver whose socket is at most 7.5 mm across.

| 3D-printed shell | Laser-cut acrylic |
| --- | --- |
| ![3D-printed shell, back with the lens cap open](renders/v4.0-3d-hero-back.jpg) | ![Laser-cut acrylic case](renders/v4.0-laser-hero.jpg) |
| Order MJF PA12 from a print service, or print at home | Order 3 mm black cast acrylic from a laser-cutting service, plus a few small printed parts |
| Sliding lens cap, capture button, switch slider, captive tripod nut | Six interlocking plates; button and switch keys cut from the same sheet |

In both cases the 1/4"-20 tripod thread sits in the middle of the bottom face, and the USB-C port is 23 mm to its left, so a tripod plate up to 30 mm wide, centred on the screw, leaves the cable free. A wider quick-release plate covers the port while the camera is on it.

![Section through the 3D-printed shell at the Lepton](renders/v4.0-3d-section.jpg)

## Make the 3D-printed shell

Files are in [`exports/3d-printed/`](exports/3d-printed/): one STEP, STL and 3MF per part, plus [`diy-thermocam-v4.0-print-plate.3mf`](exports/3d-printed/diy-thermocam-v4.0-print-plate.3mf) with every part at the quantity one camera needs.

- **Print service (recommended):** order **MJF PA12, dyed black**. Unzip [`diy-thermocam-v4.0-shell-print-files.zip`](exports/3d-printed/diy-thermocam-v4.0-shell-print-files.zip) and upload every file in its `step/` folder at the quantity its `ORDER.txt` lists (two battery spacers, one of everything else): one part per file, as JLC3DP and most services want. Every wall is at least 1.0 mm thick and every raised or engraved detail at least 0.8 mm. Those are JLC3DP's MJF minimums for small parts; on larger parts it recommends 1.5–2 mm, so its upload check may flag the 1.0–1.2 mm walls of the shells and the battery cradle as a risk.
- **At home:** PETG, 0.4 mm nozzle, 0.2 mm layers, 3–4 walls; the plate file fits a 220 × 220 mm bed. In the orientation of the plate file, `back_shell`, `lens_cap` and `battery_cradle` need supports touching the build plate only: the back shell's lens-cap track is a 28 × 58 mm bridge, and most of the lens cap's and the cradle's undersides start above the plate. The other parts print without supports. `lens_cap`, `capture_button` and `slide_switch_cap` look good in orange.

| Part | Qty | Part | Qty |
| --- | :-: | --- | :-: |
| `front_shell` | 1 | `capture_button` | 1 |
| `back_shell` | 1 | `slide_switch_cap` | 1 |
| `lens_cap` | 1 | `battery_cradle` | 1 |
| `battery_spacer` | 2 | | |

The two battery spacers hold the 34 mm wide Adafruit 258 LiPo in the middle of the cradle; a 50 × 60 mm cell fills the cradle without them. The battery lead must reach about 45 mm from the cell to J4; check that when the battery arrives. The battery floor and lid are FR4 boards; see [Order the battery floor and lid](#order-the-battery-floor-and-lid).

![Everything for the 3D-printed build](renders/v4.0-3d-kit.jpg)

## Make the laser-cut case

![The acrylic parts as they come off the sheet](renders/v4.0-laser-sheet.jpg)

Upload [`diy-thermocam-v4.0-laser-sheet.svg`](exports/laser-cut/diy-thermocam-v4.0-laser-sheet.svg) (or the [`.dxf`](exports/laser-cut/diy-thermocam-v4.0-laser-sheet.dxf)) and order **3 mm black cast acrylic (GS)**. Ask the service to confirm, before it cuts, that the sheet measures 2.8–3.2 mm: the slots are cut 3.2 mm wide, and a cast 3 mm sheet can be anywhere from [2.3 to 3.7 mm](https://www.formulor.de/en/knowledge/acrylic-thickness-tolerance). The parts take 213.1 × 195.6 mm of sheet, which is Formulor's P2 format (384 × 384 mm). Blue RGB(0, 0, 255) hairlines are cuts and red RGB(255, 0, 0) hairlines the optional logo engraving on the back plate, as [Formulor's file rules](https://www.formulor.de/en/help/files) ask; other services may use other colours. The file allows for Formulor's 0.35 mm kerf. The sheet holds a spare of each key. The keys stay in the sheet on two small bridges each; cut them free with a craft knife.

The acrylic case also needs these printed parts. Print them at home from [`diy-thermocam-v4.0-laser-print-plate.3mf`](exports/laser-cut/diy-thermocam-v4.0-laser-print-plate.3mf) with the shell's settings above; the cradle needs supports here too. Or order them from a print service: unzip [`diy-thermocam-v4.0-acrylic-files.zip`](exports/laser-cut/diy-thermocam-v4.0-acrylic-files.zip), which also holds the laser sheet, and upload every file in its `step/` folder:

| Part | Qty | What it does |
| --- | :-: | --- |
| `battery_cradle` | 1 | Holds the LiPo (same file as the shell) |
| `battery_spacer` | 2 | Hold the 34 mm LiPo in the middle of the cradle (same file as the shell) |
| `tripod_mount` | 1 | Holds the 1/4"-20 nut in the bottom plate |
| `display_spacer` | 2 | Clamp the display between the board stack and the front plate |
| `sd_chute` | 1 | Guides the microSD card from the top plate into the Teensy |

It also needs the FR4 battery floor and lid below.

![Everything for the acrylic build](renders/v4.0-laser-kit.jpg)

## Order the battery floor and lid

Both builds hold the LiPo between two thin FR4 boards: a 0.6 mm floor that rests in the printed cradle and a 0.4 mm lid on top. At that thickness a printed plate would be thinner than a print service accepts. Order them from a PCB maker as bare boards with no copper, black solder mask and the thickness below. The boards have no holes, so the Gerber zips have no drill file. If the maker offers no HASL finish at 0.4 mm, pick ENIG; with no copper on the board the finish makes no difference. The usual minimum order of five covers five cameras.

| Board | Upload | Thickness |
| --- | --- | :-: |
| Floor | [`diy-thermocam-v4.0-battery_floor-gerbers.zip`](exports/fr4/diy-thermocam-v4.0-battery_floor-gerbers.zip) | 0.6 mm |
| Lid | [`diy-thermocam-v4.0-battery_lid-gerbers.zip`](exports/fr4/diy-thermocam-v4.0-battery_lid-gerbers.zip) | 0.4 mm |

[`exports/fr4/`](exports/fr4/) also has a DXF and a STEP of each board, for shops that cut FR4 sheet.

## Buy the hardware

Everything except the last row comes from [Ettinger](https://www.ettinger.de/), which sells packs of 100, so one order covers many cameras.

| Qty | Part | Standard | Ettinger no. | Used in |
| :-: | --- | --- | --- | --- |
| 4 | Hex standoff M2.5 × 20, female/female, SW 5 | – | [005.02.205](https://www.ettinger.de/p/abstandsbolzen-messing-vernickelt-i-i-sw5x20-gewinde-m2-5x6-m2-5x6/005.02.205) | both |
| 4 | Hex standoff M2.5 × 11, male/female, SW 5 | – | [005.12.119](https://www.ettinger.de/p/abstandsbolzen-stahl-rostfrei-blank-i-a-sw5x11-i-m2-5x7-a-m2-5x6-m.-freistich/005.12.119) stainless, in stock; or [005.12.115](https://www.ettinger.de/p/abstandsbolzen-messing-vernickelt-i-a-sw5x11-i-m2-5x6-a-m2-5x6-m.-freistich/005.12.115) brass | both |
| 4 (+8) | Washer M2.5, nylon, 2.7/6.0 × 0.5 mm | ISO 7089 | [003.03.026](https://www.ettinger.de/p/unterlegscheibe-iso7089-oe2-7-6-0-x-0-5-pa6.6-natur/003.03.026) | both (+8 under the acrylic case's screws) |
| 4 | Countersunk screw M2.5 × 6, Phillips | ISO 7046 | [001.12.228](https://www.ettinger.de/p/senkkopfschraube-iso7046-m2-5x6-stahl-rostfrei-a2-blank-phillips/001.12.228) | shell, back |
| 4 | Countersunk screw M2.5 × 14, Phillips | ISO 7046 | [082.22.212](https://www.ettinger.de/p/senkkopfschraube-iso7046-m2-5x14-stahl-rostfrei-a2-blank-phillips/082.22.212) | shell, front |
| 4 | Pan head screw M2.5 × 8 | ISO 7045 | [001.17.232](https://www.ettinger.de/p/flachkopfschraube-iso7045-m2-5x8-stahl-rf-a2-blank-phillips-linsenkopf/001.17.232) | acrylic, back |
| 4 | Pan head screw M2.5 × 14 | ISO 7045 | [001.17.298](https://www.ettinger.de/p/flachkopfschraube-iso7045-m2-5x14-stahl-rf-a2-blank-phillips-linsenkopf/001.17.298) | acrylic, front |
| 4 | Cheese head screw M2 × 18, slotted | ISO 1207 | [001.14.177](https://www.ettinger.de/p/zylinderkopfschraube-iso1207-m2x18-stahl-4.8-verzinkt-schlitz/001.14.177) | both (Lepton) |
| 4 | Washer M2, nylon, 2.2/5.0 × 0.3 mm | – | [003.03.010](https://www.ettinger.de/p/unterlegscheibe-oe2-2-5-0-x-0-3-pa6.0-schwarz/003.03.010) | both (Lepton) |
| 4 | Spacer sleeve 2.2/4.0 × 12 mm, nylon | – | [005.81.120](https://www.ettinger.de/p/distanzhuelse-oe2-2-4-0-x-12-0-pa6.0-gv25-schwarz/005.81.120) | both (Lepton) |
| 4 | Hex nut M2 | ISO 4032 | [002.10.019](https://www.ettinger.de/p/sechskantmutter-iso4032-m2-stahl-rostfrei-a2-blank/002.10.019) | both (Lepton) |
| 1 | Hex nut 1/4"-20 UNC, 7/16" across flats, 5.5 mm | ASME B18.2.2 | [zollshop.de](https://www.zollshop.de/Zollmuttern-Scheiben-Federringe/Zollmuttern-UNC-UNF/Muttern-14-Zoll/14-Zoll-UNC/1061/1/4-Zoll-20-UNC-Sechskantmutter-Stahl-verzinkt/) | both (tripod) |

The nylon washers on the rear standoffs keep their hex corners off the capture and Prog button pads on the back of the board. The small nylon washers keep the M2 screw heads off the tracks around the Lepton holes. The M2 screws and sleeves hold the Lepton carrier, a small board with the Lepton socket whose 2 × 10 pin header plugs straight into J2. Their nuts also need a drop of medium-strength removable threadlocker, such as Loctite 243, so they can't work loose. The carrier, battery and modules are in the [main parts list](../bom/README.md).

## Build the 3D-printed shell

The board and battery steps are the same for the acrylic case. Orange marks the parts you add in each step.

| | |
| --- | --- |
| ![Step 1](renders/assembly-3d-01-lepton-screws.png) | ![Step 2](renders/assembly-3d-02-lepton.png) |
| **1. Lepton screws.** From the display side, push the four M2 × 18 screws through the Lepton holes, each with a nylon M2 washer under its head. Tape over the four heads so the screws stay in when you turn the board over. | **2. Lepton.** Turn the board over. Slide a 12 mm sleeve onto each screw. Snap the Lepton into the carrier's socket, then lower the carrier onto the screws so its header pins go into J2. Put a small drop of threadlocker on each screw tip, keeping it off the Lepton, and tighten the four M2 nuts with a 4 mm nut driver until snug. Peel off the tape. Fit the carrier before the Teensy: the nut driver needs the Teensy's place. |
| ![Step 3](renders/assembly-3d-03-teensy.png) | ![Step 4](renders/assembly-3d-04-rear-standoffs.png) |
| **3. Teensy.** Push the Teensy 4.1 into its two sockets on the back of the board, next to the carrier, USB end toward the bottom edge. | **4. Rear standoffs.** Stand the four 20 mm standoffs in their seats in the back shell and screw them on from outside with the M2.5 × 6 countersunk screws, holding each standoff with the 5 mm nut driver. Two standoffs sit close to the Prog pin guide and the button guide, so the socket must be at most 7.5 mm across. |
| ![Step 5](renders/assembly-3d-05-back-shell-parts.png) | ![Step 6](renders/assembly-3d-06-board.png) |
| **5. Tripod nut and slider.** With the shell open side up, drop the 1/4"-20 nut into its pocket and push it home past the two ribs with a screwdriver; they hold it in line with the tripod hole. Drop the switch slider into its slot. | **6. Battery and board.** Plug the LiPo into J4 on the back of the board and lay its wires round the right board edge, level with J4; J4 is out of reach once the board is in. Put a nylon M2.5 washer on each standoff and lower the board onto them, holding the LiPo above it. Line the slider up with the slide switch so the switch drops into its fork. |
| ![Step 7](renders/assembly-3d-07-capture-button.png) | ![Step 8](renders/assembly-3d-08-mid-standoffs.png) |
| **7. Capture button.** Drop the capture button into its slot in the top wall, flange inside, so the flange slides down past the board edge. Press it: you should feel the switch click. | **8. Middle standoffs.** Screw the four 11 mm standoffs through the board into the standoffs below with the 5 mm nut driver. |
| ![Step 9](renders/assembly-3d-09-cradle.png) | ![Step 10](renders/assembly-3d-10-battery.png) |
| **9. Cradle and floor.** Lower the cradle over the four 11 mm standoffs until its feet stand on the board, with the wire gap in its right wall. Drop the 0.6 mm FR4 floor into the cradle; it rests on the ledge at the bottom. | **10. Battery.** Lay a battery spacer against each long wall of the cradle. Press the LiPo down between them so its wires come out at the corner nearest the gap in the right wall, and lead them straight out through the gap. Fold the spare lead flat on the board beside the cradle. Lay the 0.4 mm FR4 lid on the cradle so its four rounded tabs sit against the standoffs. A 50 × 60 mm cell goes in without the spacers. |
| ![Step 11](renders/assembly-3d-11-display.png) | ![Step 12](renders/assembly-3d-12-front-shell.png) |
| **11. Display.** Press the display onto its 2 × 20 header. It rests on the 11 mm standoffs. | **12. Front shell.** Fit the front shell. Its four bosses press the display onto the standoffs; tighten the M2.5 × 14 countersunk screws until snug. |
| ![Step 13](renders/assembly-3d-13-lens-cap.png) | |
| **13. Lens cap.** Slide the cap into its track from the top. It clicks at the open and closed positions. | |

The microSD card goes in last, through the slot in the top edge: push it in with fine tweezers until it stops, with its end about 9 mm inside the shell. The Teensy's socket holds the card by friction and doesn't click; pull it out the same way. Take the card out before you lift the board out of the shell.

![The 3D-printed shell, exploded](renders/v4.0-3d-exploded.jpg)

## Build the laser-cut case

| | |
| --- | --- |
| ![Step 1](renders/assembly-laser-1-back-plate.png) | ![Step 2](renders/assembly-laser-2-tripod.png) |
| **1. Back plate.** Screw the four 20 mm standoffs to the back plate with the M2.5 × 8 pan head screws, a nylon washer under each head, holding each standoff with a 5 mm nut driver. | **2. Bottom plate and tripod mount.** Slot the bottom plate into the back plate. Slide the 1/4"-20 nut into the tripod mount from its back face until it stops; two ribs hold it there. Set the mount into the hole in the bottom plate, which then keeps the nut in. |
| ![Step 3](renders/assembly-laser-3-stack.png) | ![Step 4](renders/assembly-laser-4-top.png) |
| **3. Board stack.** Follow shell steps 1–3, 6 and 8–11 (the acrylic case has no slider; its switch key goes in at step 5). The board slides down past the tripod mount's fin, which then holds the mount in place. | **4. Top plate.** From the inside, push the button key into the small slot in the top plate. Hold it there and slot the top plate straight down into the back plate. Press the key: you should feel the switch click. |
| ![Step 5](renders/assembly-laser-5-sides.png) | ![Step 6](renders/assembly-laser-6-front-plate.png) |
| **5. Side plates.** From the inside, push the switch key into the slot in the right plate. Slot the right plate straight down so the key's fork goes over the slide switch, then slot in the left plate. | **6. Front plate.** Lay the two spacer bars on the display's mounting holes, put the front plate on and tighten the M2.5 × 14 pan head screws, a nylon washer under each head. |
| ![Step 7](renders/assembly-laser-7-sd-chute.png) | |
| **7. microSD chute.** Push the chute into the slot in the top plate until it clicks. Insert the card through it with fine tweezers until it stops, about 11 mm below the chute's opening; the socket doesn't click. | |

![The laser-cut case, exploded](renders/v4.0-laser-exploded.jpg)

## Change the design

The CAD lives in [`cad/`](cad/) as a Python [build123d](https://github.com/gumyr/build123d) package. Positions come from the KiCad board, [`../pcb/diy-thermocam-v4.0.kicad_pcb`](../pcb/diy-thermocam-v4.0.kicad_pcb); the parts inside the camera are modelled from their datasheets in [`cad/src/thermocam_case/keepout.py`](cad/src/thermocam_case/keepout.py).

```bash
cd hardware/enclosure/cad
uv sync --extra dev --python 3.12
uv run pytest                  # clearances to every part, button and switch travel, screw engagement, Lepton field of view, print minimums
uv run thermocam-case-export   # rebuild exports/ (STEP, STL, 3MF, SVG, DXF, FR4 Gerbers)
uv run thermocam-case-render   # rebuild renders/ with Blender and the KiCad 3D board
```

The export and the renders need KiCad 10; rendering also needs [Blender](https://www.blender.org/) 4.2 or newer (`brew install --cask blender`). [`cad/blender/render_scene.py`](cad/blender/render_scene.py) defines every photo and build step.
