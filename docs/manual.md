# User Manual

This manual covers daily operation, on-device touchscreen controls, charging, file transfer, and firmware updates for **DIY-Thermocam V3 and V4** running **Firmware 4.0** (Teensy 4.1). Owners of **V1 and V2** cameras (Teensy 3.2 / 3.6) should continue using [Firmware 2.4.8](https://github.com/maxritter/diy-thermocam/releases/tag/2.4.8).

## Charging and power

- Plug any standard USB-C cable into the board-edge port. The TI `BQ24074` charger powers the camera directly while charging the 2000 mAh LiPo at ~890 mA.
- Flip the side slide switch to power the camera on or off. A full 2000 mAh LiPo delivers roughly 4–6 hours of field use.

## Live view controls

The live view in four scenes, drawn by the firmware's simulator:

| Circuit board (Glowbow) | Window and radiator (Rainbow) |
| --- | --- |
| ![Simulated live view of a circuit board](/images/firmware/live-hud.png) | ![Simulated live view of a window and radiator](/images/firmware/live-building.png) |

| Breaker panel (Ironblack) | Underfloor heating (Lava) |
| --- | --- |
| ![Simulated live view of a breaker panel with one hot breaker](/images/firmware/live-breaker.png) | ![Simulated live view of underfloor heating loops](/images/firmware/live-floor.png) |

| Input | Action |
| --- | --- |
| **Short press** top button (`< 1 s`) | Save a radiometric `.DAT` image (plus optional `.BMP` preview) to the microSD card. |
| **Long press** top button (`> 1 s`) | Open Video Recording (or stop an active recording). |
| **Short tap** screen | Open the Main Menu. |
| **Long touch** in Auto range mode | Hold or release the current min/max temperature range. |
| **Long touch** in Manual range mode | Cycle through saved temperature range presets (`Preset 1`–`3`). |
| **Long touch** during USB streaming | Exit the PC session cleanly and return to standalone live mode. |

## Main menu

Tap the 3.2-inch touchscreen in live mode to open the 2-page Main Menu. Use **`More >`** / **`< Page 1`** to switch pages and **`< Live`** to return to the live thermal view.

| Main menu, dark theme | Main menu, light theme | Palette picker |
| --- | --- | --- |
| ![Firmware 4.0 Main Menu in Dark theme](/images/firmware/main-menu-dark.png) | ![Firmware 4.0 Main Menu in Daylight Light theme](/images/firmware/main-menu-light.png) | ![Firmware 4.0 Palette Picker with live gradient bars](/images/firmware/palette-picker.png) |

### Page 1

- **Palette:** Visual picker showing live gradient bars for all **19 built-in color schemes** (`Arctic`, `Black-Hot`, `Blue-Red`, `Coldest`, `Contrast`, `Double-Rainbow`, `Gray-Red`, `Glowbow`, `Grayscale`, `Hottest`, `Ironblack`, `Lava`, `Medical`, `Rainbow`, `Wheel 1`, `Wheel 2`, `Wheel 3`, `White-Hot`, `Yellow`).
- **Range:** Switch between **Auto** (tracks scene min/max) and **Manual** (set custom bounds and save up to 3 presets in EEPROM).
- **FFC:** Trigger an immediate Flat-Field Correction shutter cycle on the FLIR Lepton to clear thermal drift.
- **Points:** Add, remove, or clear up to 96 custom spot measurement points anywhere in the frame.
- **Gallery:** Browse saved **Images** (`.DAT` / `.BMP`) and **Videos** on the microSD card newest first, with `Find`, `<` / `>`, `Play`, `Convert` (to `.BMP`), and `Delete`.
- **Settings:** Configure **General** (Time, Date, BMP Conversion), **Hardware** (Battery Gauge calibration, Lepton High/Low Gain `-10..+140 °C` vs `-10..+450 °C`, Format SD as FAT32/exFAT, USB MTP), **Display** (Temp. format °C/°F, 180° Rotation / Horizontal Flip, Screen Timeout), and **UI Theme**.

| Gallery | Overlays | System status |
| --- | --- | --- |
| ![Gallery image viewer](/images/firmware/gallery-viewer.png) | ![Overlays 3x3 toggle grid](/images/firmware/overlays-grid-dark.png) | ![System Status telemetry](/images/firmware/status-screen.png) |

### Page 2

- **Overlays:** One-tap 3×3 toggle grid for Battery, Time, Date, Spot, Scale bar, Storage, Filter (`Gaussian` / `Box` / `Off`), UI Theme, and Min/Max markers.
- **Isotherm:** Highlight pixels hotter or colder than a chosen threshold in a contrast color.
- **Status:** Live telemetry screen showing actual live FPS, VoSPI resync/CRC counters, battery voltage, SD capacity, SPI bus speeds, and Teensy MCU temperature.
- **USB MTP:** Reboot into USB MTP file transfer mode to copy files with Windows Explorer, Linux file managers, or macOS OpenMTP.
- **Video:** Record full-rate **Normal** radiometric clips (~8.8 fps) or **Interval** time-lapse sequences (`1 s` to `10 min` cadence). Tap the screen during recording to toggle the backlight; press the top button to stop.
- **Theme:** Switch between **Dark** (Obsidian + Amber), **Light** (Daylight Outdoor), and **High-Contrast** (Glove/Sun).

## Connecting to a computer

### Studio (recommended)

Connect the camera to **macOS 12+**, **Windows 10/11**, or **Linux** with a data-capable USB cable and open **DIY-Thermocam Desktop Studio**.

On first start Studio shows **Connect your Thermocam**; plug in the camera and it connects by itself, or click **Try the demo camera**. Studio has three places: **Live** for the live image and measuring, **Captures** for the camera's microSD card and the files on your computer, and **Camera** for settings and firmware updates. See [Desktop App and Software](/docs/software/) for a tour.

Over [USB Protocol v2](/docs/protocol-v2/), the camera streams raw 16-bit radiometric frames at **8.77 fps**. Settings changed in Studio apply on the camera at once and stay saved. **Snapshot** and **Record** save full-temperature `.DAT` captures in **Pictures › Thermocam** on your computer (change the folder in the gear menu). In **Captures › On the camera**, browse, preview, download and delete microSD files at up to **9.20 MB/s** without MTP drivers or reboots. The top button on the camera takes a snapshot in Studio, starts a recording, or does nothing, as set under **Camera**.

### Linux USB permissions

On Linux, Studio needs permission to open the camera's USB serial port and, for firmware updates, the Teensy bootloader. If Studio shows **Thermocam found, but Studio can't open it**, or a firmware update stops because Studio couldn't open the camera in update mode, set this up once.

1. Add your user to the `dialout` group, then log out and back in:

   ```bash
   sudo usermod -aG dialout $USER
   ```

2. Install the PJRC udev rules for the Teensy 4.1:

   ```bash
   curl -fsSLO https://www.pjrc.com/teensy/00-teensy.rules
   sudo cp 00-teensy.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```

3. Unplug the camera, plug it back in, and click **Try again** in Studio.

Some distributions call the group `uucp` instead of `dialout`; `ls -l /dev/ttyACM0` shows which group owns the port.

### Copying files over USB (MTP)

If you want to copy files without opening Desktop Studio, go to **Main Menu → Page 2 → USB MTP** (or **Settings → Hardware → USB MTP**). Plugging in USB during live mode never interrupts filming with an unwanted MTP popup.

## Updating the firmware

- **Desktop Studio (recommended):** Open **Camera › Update firmware…**, choose `firmware.hex` from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases), and click **Install**. Studio checks the file, restarts the camera in update mode and writes the firmware. Works on Windows, macOS, and Linux (see [Linux USB permissions](#linux-usb-permissions)).
- **Standalone Teensy Loader or Arduino CLI:** For a brand-new blank Teensy 4.1 or custom builds from source, follow the [Firmware and Flashing guide](/docs/firmware/).
