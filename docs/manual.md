# User Manual

This manual covers daily operation, on-device controls, charging, file transfer, and firmware updates for the **DIY-Thermocam V3**.

## Charging and Power

- **V3.0 (micro-USB):** Plug any data-capable micro-USB cable into the Teensy 4.1 port on the bottom of the enclosure. The top status LED glows **red** while charging and turns **green** when full.
- **V3.1 (USB-C, prototype revision):** Plug a standard USB-C cable into the board-edge USB-C port. The TI BQ24075 power-path charger powers the camera directly while charging the 2000 mAh LiPo battery at ~890 mA.
- Toggle the side rocker switch to power the camera on or off. A fully charged 2000 mAh LiPo cell delivers roughly 4–6 hours of field operation.

## Live Mode Controls

| Input | Action |
|---|---|
| **Short press** top push button (`< 1 s`) | Save a radiometric `.DAT` capture (and optional `.BMP` preview) to the microSD card. |
| **Long press** top push button (`> 1 s`) | Start or stop continuous video recording or interval time-lapse capture. |
| **Tap screen** | Open the on-device Main Menu. |
| **Long touch** in Auto range mode | Lock or unlock the current temperature range. In Manual mode, cycles through saved range presets. |
| **Long touch** during USB streaming | Exit the PC session cleanly and return to standalone live mode. |

In live mode, the center spot meter shows the target object temperature, and the right-hand color bar maps the active color scheme to absolute temperatures.

## Main Menu

Tap anywhere on the 3.2" touchscreen in live mode to open the Main Menu:

- **Navigation & Return:** Use the left/right arrows to page through menu screens; tap **Return** to go back to live mode.
- **Temperature points:** Place or remove up to 96 custom spot measurement points anywhere in the thermal frame.
- **Change limits:** Switch between **Auto** (tracks the frame's hottest and coldest pixels continuously) and **Manual** (locks custom minimum and maximum bounds).
- **Hot / cold mode:** Highlight pixels above or below an adjustable temperature threshold in a chosen alert color.
- **Load menu:** Browse saved images and video sequences on the microSD card (newest first). Tap the center of a sequence to play or pause. Includes **Find** (jump by date/time), **Convert** (render `.DAT` to `.BMP`), **Delete**, and **Exit**.
- **Settings menu:**
  - **Display:** Temperature unit (°C or °F), 180° screen rotation, and display sleep timeout.
  - **Storage:** Format the microSD card (FAT32 / exFAT on firmware 3.1+) and toggle automatic `.BMP` preview generation.
  - **Other:** Set real-time clock date and time, calibrate battery voltage readout, and configure USB options.
- **Change color:** Select from **19 built-in color schemes** (`Arctic`, `Black-Hot`, `Blue-Red`, `Coldest`, `Contrast`, `Double-Rainbow`, `Gray-Red`, `Glowbow`, `Grayscale`, `Hottest`, `Ironblack`, `Lava`, `Medical`, `Rainbow`, `Wheel 1`, `Wheel 2`, `Wheel 3`, `White-Hot`, `Yellow`).
- **Live display options:** Toggle battery percentage, time, date, spot readout, color bar, hot/cold markers, remaining storage, spatial filter (`Gaussian`, `Box`, or `Off`), and overlay text color.
- **Trigger shutter:** Run a manual Flat-Field Correction (FFC) on the FLIR Lepton shutter to remove thermal drift and non-uniformity.
- **Display off:** Blank the LCD backlight to save battery while keeping settings and calibration active. Tap the screen to wake.

## Connecting to a Computer

### Desktop App & USB Protocol v2 (Recommended)

Connect the camera to macOS, Windows, or Linux with a data USB cable while in live mode and open the **DIY-Thermocam Desktop App**. Over **USB Protocol v2**, the camera streams raw 16-bit radiometric frames at native sensor speed (~8.7 Hz), accepts live setting changes without stalling, exposes the microSD card browser directly (no MTP drivers required), and triggers PC captures when you press the camera's physical shutter button.

### Mass Storage / MTP (Firmware 3.0.x)

On firmware 3.0.x, plugging the camera into a PC while in live mode opens USB MTP file transfer so you can copy `.DAT` and `.BMP` captures directly. Firmware 3.1+ replaces automatic MTP prompts with fast Protocol v2 SD browsing inside the desktop app.

## Updating Firmware

- **Desktop App (recommended):** Open **Camera → Firmware Update** in the DIY-Thermocam Desktop App to flash an official release or local `.hex` file directly over USB using the built-in HalfKay HID flasher (SHA-256 verified, no external tools required).
- **Standalone Teensy Loader:** Download `firmware.hex` from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases) and flash with PJRC's [Teensy Loader](https://www.pjrc.com/teensy/loader.html). See the [Firmware guide](/docs/firmware/) for step-by-step instructions.
