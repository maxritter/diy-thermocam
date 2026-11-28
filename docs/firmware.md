# Firmware and flashing

Firmware 4.0.0 is open-source C++17 (GPL v3.0) for DIY-Thermocam V3 and V4 (Teensy 4.1). The source is in [`firmware/`](https://github.com/maxritter/diy-thermocam/tree/master/firmware).

> **V1 or V2 camera (Teensy 3.2 / 3.6)?** Use [firmware 2.5.0](https://github.com/maxritter/diy-thermocam/releases/tag/2.5.0) (source on branch [`v2.4.x`](https://github.com/maxritter/diy-thermocam/tree/v2.4.x/Firmware_V2)). `.DAT` files work across all hardware versions.

| Live view | Main menu | Gallery | System status |
| --- | --- | --- | --- |
| ![Live HUD](/images/firmware/live-hud.png) | ![Main Menu](/images/firmware/main-menu-dark.png) | ![Gallery](/images/firmware/gallery.png) | ![System Status](/images/firmware/status-screen.png) |

## What you need

Studio, Teensy Loader and Arduino CLI all run on Windows 10/11, macOS 12+ and Linux.

- A data USB-C cable. Charge-only cables don't find the Teensy.
- The camera's power switch on.
- A blank Teensy 4.1 needs one press of its **Program** button.
- With firmware 3.0.x or 4.0.x installed, updates start over USB.
- Windows and macOS need no extra drivers.

On Linux, install the PJRC udev rules once:

```bash
curl -fsSLO https://www.pjrc.com/teensy/00-teensy.rules
sudo cp 00-teensy.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Studio also needs your user in the `dialout` group; see [Linux USB permissions](/docs/manual/#linux-usb-permissions).

## Installing a release

### With Studio (recommended)

1. Connect the camera over USB and switch it on.
2. In Studio, choose **Camera › Update firmware…**.
3. Choose `firmware.hex` from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases).
4. Click **Install**. The camera restarts with the new firmware.

- Camera on firmware 3.0.x? First turn on **Camera with V3.0 firmware** under **Advanced**.
- Studio refuses a damaged file and says why.
- To require a matching checksum, paste the release's SHA-256 under **Advanced**.
- No update mode after 10 seconds? Press the Teensy's **Program** button, or click **Try again**.
- It also works with no camera connected, such as a blank Teensy 4.1.
- Then click **Install** and press **Program** when Studio waits for update mode.

### With Teensy Loader (first install on a blank Teensy)

1. Download `firmware.hex` from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases).
2. Open [Teensy Loader](https://www.pjrc.com/teensy/loader.html).
3. Turn on **Operations → Automatic Mode**.
4. Open `firmware.hex`.
5. Connect the Teensy 4.1 over USB and press its **Program** button once.

![Flashing Teensy 4.1 with PJRC Teensy Loader](/images/manual/flash.png)

The camera restarts and shows the DIY-Thermocam boot screen.

## Building from source

Uses [Arduino CLI](https://github.com/arduino/arduino-cli) and [Teensy core 1.62.0](https://github.com/PaulStoffregen/cores), pinned in `firmware/sketch.yaml`. No Arduino IDE, Teensyduino or PlatformIO needed.

| System | Install Arduino CLI |
| --- | --- |
| Windows | `winget install ArduinoSA.CLI` |
| macOS | `brew install arduino-cli` |
| Linux | [Install script](https://github.com/arduino/arduino-cli) |

```bash
# 1. Compile (downloads Teensy core 1.62.0 and toolchain on first run)
cd firmware
arduino-cli compile

# 2. Find your connected camera's serial port
arduino-cli board list

# 3. Upload to the camera (replace <port> with COMx, /dev/ttyACM0, or /dev/cu.usbmodem*)
arduino-cli upload -p <port>
```

## How the firmware works

- Runs at 450 MHz by default: less heat, about 36 % less CPU power than 600 MHz.
- Keeps the Lepton's native 8.77 fps.
- Reads FAT32 and exFAT microSD cards, including cards above 32 GB.

<details>
<summary>Internals</summary>

- **Triple-buffered VoSPI:** Lepton frames stream over `SPI1` in the background. The main loop renders into `bigBuffer`, pushes frames over `SPI0` and serves [USB Protocol v2](/docs/protocol-v2/).
- **Fixed-point pipeline:** 256-entry RGB565 tables for the 19 color schemes, and integer spatial filters.
- **No flicker:** overlays, playback controls and the status screen draw in RAM, then go to the LCD in one write.
- **Storage:** [SdFat](https://github.com/greiman/SdFat) (`SdFs`) with 8 KB buffered `.DAT` writes.

</details>

<details>
<summary>Tests</summary>

| Suite | Command | Checks |
| --- | --- | --- |
| Host C++17 core | `make -C firmware/tests_host test` | Protocol v2 framing, CRC-8/CRC-32, temperature math, `.DAT` codecs; no hardware |
| UI simulator | `make -C firmware/sim test` | 18 checks on the real GUI code: touch, XPT2046 `PENIRQ` glitches, menus, video recording, redraw flicker |
| Live hardware | `firmware/test/` | 45 [pytest](https://github.com/pytest-dev/pytest) checks on a USB-connected camera: Protocol v2, legacy v1, frame rates, FFC, settings, SD captures |

</details>
