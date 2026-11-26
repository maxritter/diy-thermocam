# User manual

For DIY-Thermocam V3 and V4 on firmware 4.0 (Teensy 4.1). V1 and V2 cameras (Teensy 3.2 / 3.6) stay on [firmware 2.4.8](https://github.com/maxritter/diy-thermocam/releases/tag/2.4.8).

## Charging and power

- Charge from the board-edge USB-C port, with a charger or port rated 1.5 A or more.
- The camera runs while it charges.
- Full charge: about 2.5 hours for the 1200 mAh Adafruit cell, 4 hours for 2000 mAh.
- Runtime: about 4–6 hours on 2000 mAh, about 60 % of that on 1200 mAh.
- The side slide switch turns the camera on and off.

## Live view controls

Four scenes from the firmware's simulator:

| Circuit board (Glowbow) | Window and radiator (Rainbow) |
| --- | --- |
| ![Simulated live view of a circuit board](/images/firmware/live-hud.png) | ![Simulated live view of a window and radiator](/images/firmware/live-building.png) |

| Breaker panel (Ironblack) | Underfloor heating (Lava) |
| --- | --- |
| ![Simulated live view of a breaker panel with one hot breaker](/images/firmware/live-breaker.png) | ![Simulated live view of underfloor heating loops](/images/firmware/live-floor.png) |

| Input | Action |
| --- | --- |
| Press the top button (under 1 s) | Save a `.DAT` image, plus an optional `.BMP`, to the microSD card |
| Hold the top button (over 1 s) | Open video recording, or stop it |
| Tap the screen | Open the main menu |
| Hold the screen, auto range | Lock or release the current min/max range |
| Hold the screen, manual range | Cycle range presets 1–3 |
| Hold the screen, USB streaming | Leave the PC session and return to live view |

## Main menu

Tap the screen in live view to open the two-page menu. **More >** and **< Page 1** switch pages; **< Live** goes back.

| Main menu, dark theme | Main menu, light theme | Palette picker |
| --- | --- | --- |
| ![Firmware 4.0 Main Menu in Dark theme](/images/firmware/main-menu-dark.png) | ![Firmware 4.0 Main Menu in Daylight Light theme](/images/firmware/main-menu-light.png) | ![Firmware 4.0 Palette Picker with live gradient bars](/images/firmware/palette-picker.png) |

### Page 1

| Item | What it does |
| --- | --- |
| **Palette** | Pick one of 19 color schemes |
| **Range** | Auto follows the scene; Manual sets bounds and saves up to 3 presets |
| **FFC** | Run a flat-field correction now to clear thermal drift |
| **Points** | Add, remove or clear up to 96 measurement spots |
| **Gallery** | Find, play, convert to `.BMP` or delete saved images and videos |
| **Settings** | Time, date, sensor gain, SD card, display and theme |

<details>
<summary>The 19 palettes</summary>

Arctic, Black-Hot, Blue-Red, Coldest, Contrast, Double-Rainbow, Gray-Red, Glowbow, Grayscale, Hottest, Ironblack, Lava, Medical, Rainbow, Wheel 1, Wheel 2, Wheel 3, White-Hot, Yellow.

</details>

<details>
<summary>All settings</summary>

| Group | Options |
| --- | --- |
| General | Time, date, BMP conversion |
| Hardware | Battery gauge calibration, Lepton gain (high −10 to +140 °C, low −10 to +450 °C), format SD as FAT32 or exFAT, USB MTP |
| Display | °C or °F, 180° rotation, horizontal flip, screen timeout |
| UI theme | Dark, light or high contrast |

</details>

| Gallery | Overlays | System status |
| --- | --- | --- |
| ![Gallery image viewer](/images/firmware/gallery-viewer.png) | ![Overlays 3x3 toggle grid](/images/firmware/overlays-grid-dark.png) | ![System Status telemetry](/images/firmware/status-screen.png) |

### Page 2

| Item | What it does |
| --- | --- |
| **Overlays** | Show or hide battery, time, date, spot, scale, storage, filter, theme, min/max |
| **Isotherm** | Color pixels above or below a threshold |
| **Status** | Frame rate, VoSPI errors, battery, SD space, SPI speeds, CPU temperature |
| **USB MTP** | Restart into file-transfer mode |
| **Video** | Record clips at about 8.8 fps, or time-lapse every 1 s to 10 min |
| **Theme** | Dark, Light or High-Contrast |

- While recording, tap the screen to turn the backlight off or on.
- Press the top button to stop recording.
- The image filter can be Gaussian, Box or Off.

## Connecting to a computer

### Studio (recommended)

Connect a data USB cable and open DIY-Thermocam Desktop Studio on macOS 12+, Windows 10/11 or Linux.

- Studio connects by itself. No camera? Click **Try the demo camera**.
- **Live** shows the image, **Captures** the files, **Camera** the settings.
- Settings you change in Studio apply on the camera at once and stay saved.
- **Snapshot** and **Record** save `.DAT` files to **Pictures › Thermocam**.
- **Captures › On the camera** reads the microSD card without MTP.
- Set what the top button does under **Camera**.

See [Studio](/docs/desktop-app/) for a tour.

### Linux USB permissions

Do this once if Studio shows **Thermocam found, but Studio can't open it**, or a firmware update can't open the camera in update mode.

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

Some distributions use the `uucp` group instead. `ls -l /dev/ttyACM0` shows which.

### Copying files over USB (MTP)

- Open **Main menu → Page 2 → USB MTP**, or **Settings → Hardware → USB MTP**.
- Copy files with Windows Explorer, a Linux file manager or OpenMTP on macOS.
- Plugging in USB in live view does not start MTP.

## Updating the firmware

1. In Studio, open **Camera › Update firmware…**.
2. Choose `firmware.hex` from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases).
3. Click **Install**. Studio restarts the camera in update mode and writes the firmware.

On Linux, set up [USB permissions](#linux-usb-permissions) first. For a blank Teensy 4.1 or your own build, see [Firmware](/docs/firmware/).
