# Building Instructions

You can build a DIY-Thermocam in two ways: the **V4 Zero-Solder Kit** *(prototype-pending)* where a factory-assembled PCBA and 3D-printed enclosure snap together with only a Phillips screwdriver, or the **classic V3.0 Through-Hole Build** *(verified baseline)* for traditional hand soldering.

---

## Option A: DIY-Thermocam V4 — Zero-Solder, Screwdriver-Only (Prototype-Pending)

DIY-Thermocam V4 eliminates soldering irons, loose jumper wires, coin cells, and trace cutting. Every power regulator, USB-C port, charger IC (`BQ24074`), 5 V synchronous boost (`TPS61022`), 3.3 V RTC LDO (`XC6206`), active reverse-polarity MOSFET chain, slide switch, tactile shutter button, USB D+/D- pogo pins, and female header socket arrives pre-soldered from the PCB fab. You plug in off-the-shelf modules and fasten the 3D-printed shell with `4× M2.5×6` front and `4× M2.5×8` back DIN 965 countersunk screws.

> **Prototype status:** V4 PCB (`hardware/pcb/`) and 3D enclosure (`hardware/enclosure/exports/3d-v4.0/`) are `PROTOTYPE-PENDING` until physical bring-up is completed. For a field-proven build today, follow the verified V3.0 guide below.

### Minimal Worldwide Ordering Flow (4 Orders Total)

1. **JLCPCB, Seeed Studio Fusion, or PCBWay — PCBA + 3D-Printed Enclosure:** Upload the V4 Gerber, BOM (31 verified LCSC SKUs), and CPL files to receive a factory-populated board (TI `BQ24074` USB-C power-path charger, TI `TPS61022` 5 V boost, `XC6206P332MR-G` 3.3 V RTC LDO, `AO3401A`/`AO3400A` active reverse-polarity protection, slide switch, capture button, pogo pins, JST-PH socket, and female sockets) plus the support-free 3D-printed shell (`front_shell`, `back_shell`, `lens_cap`, `capture_button`, `slide_switch_cap`, `battery_cradle`, `battery_lid`).
2. **DigiKey or Mouser (one consolidated electronics order):** Teensy 4.1 **with pre-soldered pins** (plus 1×5 inner pin header for `J7`), FLIR Lepton 3.5 (`500-0771-01`) or optional wide-angle Lepton 3.1R (`500-0758-03`), FLIR Lepton Breakout Board V2.0 (`250-0577-00`), Class 10 microSD card, M2.5 standoff kit (`4× M2.5×6` front + `4× M2.5×8` back DIN 965 CSK screws), and a metal `1/4"-20` hex nut.
3. **BuyDisplay:** `ER-TFTM032-3` 3.2" 240×320 SPI TFT touch display (ships with pre-soldered 2×20 pin header — select 4-Wire SPI, 5V, Resistive Touch, No Font Chip).
4. **Battery supplier:** Slim **≤5.0 mm UL/IEC 62133 certified protected `505060` (5.0 × 50 × 60 mm) 3.7 V 2000 mAh LiPo** with standard Adafruit/SparkFun JST-PH polarity (always verify `+`/`-` against `J4` silkscreen).

See the interactive [Bill of Materials](/docs/partlist/) for verified links, V3-upgrade reuse options, and live cost breakdowns.

![V4.0 parametric build123d 4-step mechanical assembly overview](/images/build-v4/v4-cad-assembly.png)

### V4 Step 1: Unbox the factory PCBA and snap-in modules

Lay out the factory-assembled V4 PCBA, Teensy 4.1 (with pre-soldered header pins), FLIR Lepton 3.5 (or 3.1R) + Breakout V2.0, BuyDisplay 3.2" TFT, slim `505060` JST-PH LiPo battery, microSD card, and a standard Phillips screwdriver. No soldering iron, coin cell, or wire stripper is needed.

![V4 Step 1: Factory PCBA and modular components laid out with screwdriver](/images/build-v4/v4-step-1-unbox-pcba.png)

### V4 Step 2: Plug in Teensy 4.1, Lepton, Display, and Battery

Snap the FLIR Lepton 3.5 (or 3.1R) into the Breakout Board V2.0 until both clips click, then press the **Teensy 4.1**, **Lepton Breakout V2.0**, and **3.2" TFT Display** squarely into their factory-soldered sockets on the V4 PCB. Seat the slim `505060` LiPo inside the insulating `battery_cradle` + `battery_lid` between the PCB and Display, insert the microSD card, verify JST-PH polarity (`-` / `+`) against the silkscreen, and plug into `J4`.

![V4 Step 2: Plugging Teensy 4.1, FLIR Lepton, 3.2-inch display, and JST-PH battery into the V4 PCBA](/images/build-v4/v4-step-2-plug-modules.png)

### V4 Step 3: Close the 3D-printed enclosure with M2.5 CSK screws and flash over USB-C

Slide the metal `1/4"-20` tripod hex nut into the rear shell pocket (max tripod stud `≤ 8.0 mm`), drop the guided `capture_button` plunger and `slide_switch_cap` into place, seat the electronics stack, and fasten `4× M2.5×6` front and `4× M2.5×8` back DIN 965 countersunk screws with a Phillips screwdriver. Slide on the dovetail lens cap, connect a standard USB-C cable, and flash Firmware 4.0 directly from the [DIY-Thermocam Desktop Studio](/software/).

![V4 Step 3: Exploded 3D-printed enclosure assembled with four screws and a Phillips screwdriver](/images/build-v4/v4-step-3-enclosure-screws.png)

---

## Option B: Classic DIY-Thermocam V3.0 — 45-Step Through-Hole Guide (Verified Baseline)

The verified V3.0 through-hole PCB and 6-plate laser-cut acrylic enclosure can be built at home with standard soldering tools. Every part is listed in the [Bill of Materials](/docs/partlist/).

### Prerequisites

Verify you have all required V3.0 parts from the [Bill of Materials](/docs/partlist/):

![All required DIY-Thermocam V3 components laid out](/images/manual/1.jpg)

Required hand tools:

![Assembly tools needed for DIY-Thermocam V3](/images/manual/2.jpg)

- Soldering iron and solder wire
- Flush wire cutters / cutting pliers
- Sharp craft knife (`VUSB–VIN` trace cut)
- Multimeter (with continuity mode)
- Phillips and flathead screwdrivers
- Tweezers and a small flat needle file

### 1. Cut male header strips for the Teensy 4.1

Grab the **Teensy 4.1** and the **male pin header strips**. Cut two **24-pin** strips with one flat outer edge as shown so they stay flush inside the PCB outline, plus one **5-pin** strip and one **1-pin** strip.

![Step 1: Cutting male header strips for Teensy 4.1](/images/manual/3.jpg)

### 2. Solder male headers to the Teensy 4.1

Insert both 24-pin strips on the underside of the Teensy 4.1 (flat edges facing the USB port), followed by the 5-pin and 1-pin strips. Keep every header perpendicular to the board and solder all pins cleanly.

![Step 2: Soldering male headers to Teensy 4.1](/images/manual/4.jpg)

### 3. Cut the VUSB–VIN pad on Teensy 4.1

Use a sharp craft knife to sever the tiny copper trace between the **VUSB** and **VIN** pads on the underside of the Teensy 4.1. This prevents USB power from bypassing the power switch. Confirm zero continuity across the two pads with your multimeter before proceeding.

![Step 3: Cutting the VUSB-VIN trace on Teensy 4.1](/images/manual/5.jpg)

### 4. Insert the microSD card

Slide a **Class 10 / U1 microSD card** (16–32 GB FAT32 recommended; exFAT supported on firmware 3.1+) into the slot on top of the Teensy 4.1.

![Step 4: Inserting the microSD card into Teensy 4.1](/images/manual/6.jpg)

### 5. Cut female header sockets for the Teensy 4.1

Cut two **24-pin**, one **5-pin**, and one **1-pin** strip from the **female header strips**, keeping at least two **10-pin** sections aside for the Lepton socket later.

![Step 5: Cutting female header sockets](/images/manual/7.jpg)

### 6. Seat female sockets onto the Teensy 4.1

Press the female sockets onto the Teensy 4.1 male pins so the microcontroller remains removable for service. Lightly file the plastic edge of the 5-pin socket if it sits tight against the 24-pin rail.

![Step 6: Fitting female sockets onto Teensy 4.1](/images/manual/8.jpg)

### 7. Place the Teensy assembly onto the PCB

Align the Teensy 4.1 + female socket assembly into its silkscreen outline on the top side of the PCB and press until seated flat.

![Step 7: Seating Teensy assembly onto the PCB](/images/manual/9.jpg)

### 8. Solder the Teensy sockets to the PCB

Flip the PCB over and solder every Teensy socket pin cleanly.

![Step 8: Soldering Teensy socket pins on the PCB backside](/images/manual/10.jpg)

### 9. Mount the Li-Ion charger module

Verify the Teensy headers do not protrude beyond the PCB edges (file flush if needed so the enclosure fits). Cut one 3-pin and four 1-pin male header segments to seat a TP4057 charger module on the top of the PCB. If using the recommended **TP4056** module instead of the obsolete TP4057, follow the wiring notes in [issue #103](https://github.com/maxritter/diy-thermocam/issues/103).

![Step 9: Mounting the charging module on PCB V3.0](/images/manual/11.jpg)

### 10. Solder and trim the charger pins

Flip the PCB, solder all seven charger pins, and trim excess lead length flush.

![Step 10: Soldering charger module pins](/images/manual/12.jpg)

### 11. Orient and mount the 5V step-up converter

Cut a 3-pin male header strip for the **Pololu 5V step-up regulator**. Check your board version carefully before soldering:

- Legacy **U3V12F5** labeled `reg10a - 0J7032 - @2012`: mount with components facing up:

![Step 11a: Pololu U3V12F5 reg10a orientation](/images/manual/13_1.png)

- Legacy **U3V12F5** labeled `reg12b - 0J7517 - @2013` or newer: mount rotated with text facing up:

![Step 11b: Pololu U3V12F5 reg12b orientation](/images/manual/13_2.png)

- Current **Pololu U3V16F5 (recommended)**: install in this orientation:

![Step 11c: Pololu U3V16F5 orientation](/images/manual/13.jpg)

### 12. Solder the step-up regulator pins

Flip the PCB, solder the three regulator pins, and clip the excess leads.

![Step 12: Soldering step-up regulator pins](/images/manual/14.jpg)

### 13. Solder the 2×20 display connector

Insert the **40-pin (2×20) female display header** on the backside of the PCB with the center keying notch facing inward, then solder all 40 pins.

![Step 13: Soldering the 40-pin display connector](/images/manual/15.jpg)

### 14. Position the bi-color charging LED

Place the **bi-color charging LED** on the backside of the PCB with its common cathode (shorter lead) in the pad marked `G`. Leave enough lead length to bend the LED 90° upward so it aligns with the top enclosure indicator hole.

![Step 14: Positioning the bi-color charging LED](/images/manual/16.jpg)

### 15. Solder and trim the charging LED

Flip the PCB, solder the three LED pins, and clip the excess leads.

![Step 15: Soldering the charging LED pins](/images/manual/17.jpg)

### 16. Insert the axial resistors

Insert the four **4.7 kΩ** resistors (`R1, R3, R4, R6`) and two **10 kΩ** resistors (`R2, R5`) into their marked silkscreen positions on the top of the PCB.

![Step 16: Inserting 4.7k and 10k axial resistors](/images/manual/18.jpg)

### 17. Solder and trim the resistors

Flip the PCB, solder all twelve resistor leads, and trim flush.

![Step 17: Soldering axial resistors](/images/manual/19.jpg)

### 18. Place the CR1220 coin cell holder

Seat the **CR1220 coin cell holder** into its marked footprint on the top of the PCB. Do not insert the coin cell yet.

![Step 18: Placing the CR1220 coin cell holder](/images/manual/20.jpg)

### 19. Solder the coin cell holder

Flip the PCB and solder both pads of the coin cell holder.

![Step 19: Soldering the coin cell holder](/images/manual/21.jpg)

### 20. Insert the 2×10 Lepton female headers

Insert the remaining two **10-pin female header strips** (or one 2×10 socket) into the Lepton header footprint (`J2`).

![Step 20: Inserting the Lepton 2x10 female header](/images/manual/22.jpg)

### 21. Solder the Lepton header

Flip the PCB and solder all 20 pins squarely.

![Step 21: Soldering the Lepton header pins](/images/manual/23.jpg)

### 22. Insert the four M2×18 Lepton mounting screws

Feed four **M2×18 screws** up through the four Lepton mounting holes in the PCB.

![Step 22: Inserting M2x18 Lepton mounting screws](/images/manual/24.jpg)

### 23. Apply the battery insulating pad

Hold the M2 screws in place while flipping the PCB over. Trim the **double-sided insulating mounting pad** slightly larger than the battery footprint, peel one liner, and stick it to the PCB backside without covering the two marked mounting holes.

![Step 23: Applying the LiPo insulating pad on PCB backside](/images/manual/25.jpg)

### 24. Slide M2×12 spacers onto the screws

Place the four **M2×12 spacers** onto the four M2×18 screws on the top side of the PCB.

![Step 24: Fitting M2x12 spacers over the screws](/images/manual/26.jpg)

### 25. Fit the header into the Lepton Breakout Board V2.0

Ensure the right-angle 2×10 header is firmly seated on the **FLIR Lepton Breakout Board V2.0**.

![Step 25: Preparing the FLIR Lepton Breakout Board V2.0](/images/manual/27.jpg)

### 26. Mount the Lepton Breakout Board V2.0

Lower the Breakout Board over the four M2×18 screws while guiding its 2×10 header into the PCB socket. Secure with four **M2 nylon nuts**.

![Step 26: Securing the Lepton Breakout Board V2.0](/images/manual/28.jpg)

### 27. Prepare the 3.7 V LiPo battery leads

Double-check battery polarity (`+` red, `-` black) before soldering 26 AWG silicone wire leads to a bare battery pack.

![Step 27: Soldering wire leads to the battery](/images/manual/29.jpg)

### 28. Mount and wire the LiPo battery

Peel the second liner on the insulating pad, press the **2000 mAh LiPo battery** firmly in place, and solder the red wire to `LIPO +` and black wire to `LIPO -` on the PCB backside.

![Step 28: Mounting the battery and soldering LIPO leads](/images/manual/30.jpg)

### 29. Install and wire the power switch

Snap the **miniature rocker power switch** into the right side panel of the enclosure and solder two red 26 AWG wires to the center and right switch terminals.

![Step 29: Wiring the side power switch](/images/manual/31.jpg)

### 30. Solder power switch wires to the PCB

Solder the two red switch wires horizontally to the `SWITCH` pads on the PCB (polarity does not matter). Keep leads flat and neatly insulated (self-fusing silicone tape works well) so the wires clear the enclosure wall cleanly.

![Step 30: Soldering switch leads horizontally to the PCB](/images/manual/32.jpg)

### 31. Install and wire the capture push button

Mount the **momentary capture button** into the top enclosure panel (with the LED hole oriented toward you), secure its locking nut, solder two black 26 AWG wires to the terminals, and insulate with short pieces of **heat shrink tubing** so the terminals clear the display.

![Step 31: Mounting and insulating the capture push button](/images/manual/33.jpg)

### 32. Solder capture button wires to the PCB

Solder the two black button wires to the `BUTTON` pads on the PCB (polarity does not matter).

![Step 32: Soldering capture button leads to the PCB](/images/manual/34.jpg)

### 33. Fit display standoffs

Peel the protective shipping film off the **3.2" TFT LCD Touch Display** and assemble the four **M2.5×5** and four **M2.5×11** threaded standoffs through its corner mounting holes.

![Step 33: Installing M2.5 standoffs on the 3.2-inch TFT display](/images/manual/35.jpg)

### 34. Mate the display to the PCB

Align the 40-pin display header with the PCB socket and guide the four M2.5 standoffs through the PCB corner holes. Ensure no switch or button wire is pinched between the display and PCB.

![Step 34: Mating the TFT display to the main PCB](/images/manual/36.jpg)

### 35. Add M2.5×20 rear standoffs

Thread the four **M2.5×20 female-female standoffs** onto the M2.5×11 studs protruding through the PCB.

![Step 35: Securing M2.5x20 rear standoffs](/images/manual/37.jpg)

### 36. Fit the 1/4"-20 tripod mount

Insert the **1/4"-20 tripod socket** into the bottom enclosure plate using three **M2×8 screws** (or seat a captive 1/4"-20 nut in the 3D-printed shell).

![Step 36: Installing the tripod mount socket](/images/manual/38.jpg)

### 37. Secure the tripod mount nuts

Tighten the three **M2 metal nuts and washers** on the inside of the bottom plate.

![Step 37: Tightening tripod socket M2 nuts](/images/manual/39.jpg)

### 38. Attach the front enclosure plate

Place the front bezel over the display (verify orientation) and secure it with four **M2.5×6 screws**.

![Step 38: Attaching the front enclosure plate](/images/manual/40.jpg)

### 39. Snap in the FLIR Lepton sensor and CR1220 coin cell

Handling the **FLIR Lepton 3.5** (or 2.5) by its edges only, align the keying notch and press firmly into the Breakout Board socket until both retention clips click. Insert the **CR1220 3V coin cell** with the `+` face up.

![Step 39: Installing the FLIR Lepton module and CR1220 coin cell](/images/manual/41.jpg)

### 40. Fit the remaining enclosure side walls

Interlock the top, bottom, left, and right side panels around the front plate.

![Step 40: Fitting the enclosure side panels](/images/manual/42.jpg)

### 41. Check USB port alignment

Verify the Teensy 4.1 USB connector aligns cleanly inside the bottom enclosure cutout before closing the case.

![Step 41: Verifying USB cutout alignment](/images/manual/43.jpg)

### 42. Flash the firmware while the back is open

Before closing the backplate, flash the latest firmware from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases) using PJRC's [Teensy Loader](https://www.pjrc.com/teensy/loader.html) (enable **Auto**, load `firmware.hex`, connect USB, and press the Teensy Program button). Subsequent updates can be flashed directly from the [DIY-Thermocam Desktop App](/software/) without opening the enclosure.

![Step 42: Flashing DIY-Thermocam firmware via Teensy Loader](/images/manual/flash.png)

Once the camera reboots and shows the live thermal feed, turn the power switch off to finish assembly.

### 43. Close the rear enclosure plate

Position the rear plate so the Lepton lens is centered inside the aperture, then fasten the final four **M2.5×6 screws** gently (do not overtighten on acrylic plates).

![Step 43: Securing the rear enclosure plate](/images/manual/44.jpg)

### 44. Lens protection

If using the 3D-printed parametric enclosure, snap on the integrated lens cap. If attaching an adhesive lens cover to an acrylic plate, verify the sensor's 57° HFOV does not clip the cover edges when open (which creates thermal vignetting).

### 45. First power-on and next steps

Your DIY-Thermocam is ready! Explore the [User Manual](/docs/manual/) to learn on-device controls, install the [Desktop App](/software/) for live PC streaming and radiometric reporting, or dig into [USB Protocol v2](/docs/protocol-v2/) to build your own integrations.
