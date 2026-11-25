# User Manual

This manual covers daily operation, on-device controls, charging, file transfer, and firmware updates for DIY-Thermocam V3 and V4.

## Charging and Power

- **V3.0 (micro-USB):** Plug a data-capable micro-USB cable into the Teensy 4.1 port on the bottom of the enclosure. The status LED glows red while charging and turns green when full.
- **V4.0 (USB-C, `PROTOTYPE-PENDING`):** Plug a standard USB-C cable into the board-edge port. The TI `BQ24074` power-path charger runs the camera directly while charging the 2000 mAh LiPo at ~890 mA.
- Toggle the side switch to power the camera on or off. A charged 2000 mAh LiPo delivers roughly 4–6 hours of field use.

## Live Mode Controls

| Input | Action |
| --- | --- |
| **Short press** capture button (`< 1 s`) | Save a radiometric `.DAT` image (and optional `.BMP` preview) to the microSD card. |
| **Long press** capture button (`> 1 s`) | Start or stop continuous video recording or interval time-lapse capture. |
| **Tap screen** | Open the on-device Main Menu. |
| **Long touch** in Auto range mode | Hold or release the current temperature range. In Manual mode, cycles through saved range presets. |
| **Long touch** during USB streaming | Exit the PC session cleanly and return to standalone live mode. |

In live mode, the center spot meter shows the target temperature. The right-hand color bar maps the active palette to absolute temperatures.

## Main Menu

Tap anywhere on the 3.2-inch touchscreen in live mode to open the Main Menu:

- **Navigation:** Use the left and right arrows to switch pages. Tap **Return** to go back to live mode.
- **Temperature points:** Place or remove up to 96 custom spot measurement points anywhere in the frame.
- **Change limits:** Switch between **Auto** (tracks hottest and coldest pixels) and **Manual** (sets custom minimum and maximum bounds).
- **Hot / cold mode:** Highlight pixels above or below a threshold in a chosen alert color.
- **Load menu:** Browse saved images and video sequences on the microSD card (newest first). Tap a sequence to play or pause. Includes **Find**, **Convert** (`.DAT` to `.BMP`), **Delete**, and **Exit**.
- **Settings menu:**
  - **Display:** Temperature unit (°C or °F), 180° screen rotation, and display sleep timeout.
  - **Storage:** Format the microSD card (FAT32 or exFAT on Firmware 4.0) and toggle automatic `.BMP` previews.
  - **Other:** Set real-time clock date and time, calibrate battery readout, and configure USB options.
- **Change color:** Choose from **19 built-in color schemes** (`Arctic`, `Black-Hot`, `Blue-Red`, `Coldest`, `Contrast`, `Double-Rainbow`, `Gray-Red`, `Glowbow`, `Grayscale`, `Hottest`, `Ironblack`, `Lava`, `Medical`, `Rainbow`, `Wheel 1`, `Wheel 2`, `Wheel 3`, `White-Hot`, `Yellow`).
- **Live display options:** Toggle battery percentage, time, date, spot readout, color bar, hot/cold markers, storage readout, spatial filter (`Gaussian`, `Box`, or `Off`), and text color.
- **Trigger shutter:** Run a manual Flat-Field Correction (FFC) on the FLIR Lepton shutter to clear thermal drift.
- **Display off:** Turn off the LCD backlight to save battery while keeping calibration active. Tap the screen to wake.

## Connecting to a Computer

### Desktop App and USB Protocol v2 (Recommended)

Connect the camera to macOS, Windows, or Linux over USB while in live mode and open **DIY-Thermocam Desktop Studio**.

Over **USB Protocol v2**, the camera streams raw 16-bit radiometric frames at full sensor speed (~8.77 fps) and applies live setting changes without stalling. You can browse the microSD card directly at up to 9.20 MB/s without MTP drivers, and press the camera shutter button to trigger PC captures.

### Mass Storage / MTP (Firmware 3.0.x)

On Firmware 3.0.x, plugging the camera into a computer while in live mode opens USB MTP file transfer for copying `.DAT` and `.BMP` files. Firmware 4.0 replaces MTP prompts with Protocol v2 SD browsing inside the Desktop App.

## Updating Firmware

- **Desktop App (recommended):** Open **Camera → Firmware Update** in DIY-Thermocam Desktop Studio to flash a release or local `.hex` file over USB. The built-in HalfKay HID flasher verifies SHA-256 checksums with no external tools.
- **Standalone Teensy Loader:** Download `firmware.hex` from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases) and flash with PJRC's [Teensy Loader](https://www.pjrc.com/teensy/loader.html). See the [Firmware guide](/docs/firmware/) for step-by-step details.
