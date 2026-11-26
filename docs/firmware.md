# Firmware and Flashing

DIY-Thermocam Firmware 4.0 is open-source C++17 code for **DIY-Thermocam V3 and V4** (**Teensy 4.1**). It runs at **450 MHz** by default to cut enclosure heat and lower CPU power draw by ~36% compared to 600 MHz. Source code lives in [`firmware/`](https://github.com/maxritter/diy-thermocam/tree/master/firmware).

> **Hardware compatibility:** Firmware 4.0 supports **DIY-Thermocam V3 and V4 (Teensy 4.1)** only. Owners of **DIY-Thermocam V1 or V2 (Teensy 3.2 / 3.6)** must continue using [Firmware 2.4.8](https://github.com/maxritter/diy-thermocam/releases/tag/2.4.8). Radiometric `.DAT` files remain 100% compatible across all hardware revisions.

| Live view | Main menu | Gallery | System status |
| --- | --- | --- | --- |
| ![Live HUD](/images/firmware/live-hud.png) | ![Main Menu](/images/firmware/main-menu-dark.png) | ![Gallery](/images/firmware/gallery.png) | ![System Status](/images/firmware/status-screen.png) |

## What you need

Flashing prebuilt `.hex` releases and compiling from source both work on **Windows**, **macOS**, and **Linux**.

| Operating System | Desktop Studio | PJRC Teensy Loader | Build & Flash ([Arduino CLI](https://github.com/arduino/arduino-cli)) | Notes |
| --- | --- | --- | --- | --- |
| **Windows 10 / 11** (x64) | Supported | Supported | `winget install ArduinoSA.CLI` | USB CDC and HalfKay HID drivers are built into Windows. |
| **macOS 12+** (Apple Silicon & Intel) | Supported | Supported | `brew install arduino-cli` | No extra drivers needed. |
| **Linux** (x86_64 & ARM64) | Supported | Supported | [Install script](https://github.com/arduino/arduino-cli) | Install [PJRC `00-teensy.rules`](https://www.pjrc.com/teensy/00-teensy.rules) into `/etc/udev/rules.d/` for non-root flashing. |

**Before you plug in:**
- Use a **data-capable USB-C cable**. Charge-only cables will not detect the Teensy.
- Turn the camera **power switch ON** before flashing.
- Only a **brand-new blank Teensy 4.1** needs a physical press of its **Program** button. Once Firmware 3.0.x or 4.0.x is installed, later updates trigger the bootloader over USB without opening the enclosure.

On Linux, install the PJRC udev rules once:

```bash
curl -fsSLO https://www.pjrc.com/teensy/00-teensy.rules
sudo cp 00-teensy.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Studio also needs your user in the `dialout` group to open the camera's serial port; see [Linux USB permissions](/docs/manual/#linux-usb-permissions).

## Installing a release

### With Studio (recommended)

1. Connect the camera over USB and switch it on.
2. Open **DIY-Thermocam Desktop Studio** and choose **Camera › Update firmware…**.
3. Choose `firmware.hex` from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases). Studio checks the file at once and refuses a damaged one with the reason. To require a matching checksum, paste the release's SHA-256 under **Advanced**.
4. Click **Install**. Studio restarts the camera in update mode over USB, writes the firmware, and the camera restarts with it. For a camera on Firmware 3.0.x, switch on **Camera with V3.0 firmware** under **Advanced** first.

If the camera hasn't switched to update mode after 10 seconds, Studio says so. Press the program button on the Teensy board inside the camera and the update continues by itself, or click **Try again**.

**Update firmware…** also works with no camera connected, for example on a blank Teensy 4.1. Choose the file and click **Install**. When Studio waits for update mode, press the Teensy's **Program** button.

### With Teensy Loader (first install on a blank Teensy)

1. Download `firmware.hex` from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases).
2. Download and open [Teensy Loader](https://www.pjrc.com/teensy/loader.html) on Windows, macOS, or Linux.
3. Enable **Auto** mode (**Operations → Automatic Mode**).
4. Open `firmware.hex` in Teensy Loader.
5. Connect the Teensy 4.1 over USB and press its physical **Program** button once.

![Flashing Teensy 4.1 with PJRC Teensy Loader](/images/manual/flash.png)

The camera reboots automatically and displays the DIY-Thermocam boot screen.

## Building from source

The firmware compiles with [Arduino CLI](https://github.com/arduino/arduino-cli) and [Teensy core 1.62.0](https://github.com/PaulStoffregen/cores), pinned in `firmware/sketch.yaml`. Arduino IDE, Teensyduino, and PlatformIO are not required.

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

- **Triple-buffered VoSPI acquisition:** FLIR Lepton frames stream over `SPI1` in the background while the main loop renders into `bigBuffer`, pushes atomic frames over `SPI0`, and serves [USB Protocol v2](/docs/protocol-v2/).
- **Fixed-point LUT pipeline:** Precomputed 256-entry RGB565 tables (19 color schemes) and integer spatial filters sustain the Lepton's **8.77 fps** native frame rate.
- **Zero-flicker RAM-composited UI:** Live HUD overlays, video playback controls, and the System Status telemetry screen render into RAM before a single atomic LCD write.
- **FAT32 & exFAT microSD storage:** Powered by [SdFat](https://github.com/greiman/SdFat) (`SdFs`), supporting cards above 32 GB with 8 KB buffered `.DAT` writes.
- **Three-tier automated test suite:**
  - **Host C++17 core tests (`make -C firmware/tests_host test`):** Validates Protocol v2 framing, CRC-8/CRC-32, temperature math, and `.DAT` codecs without hardware.
  - **Interactive UI simulator (`make -C firmware/sim test`):** Runs 18 automated touch, XPT2046 `PENIRQ`-glitch, menu navigation, video recording, and redraw-flicker checks on the real firmware GUI code.
  - **Live hardware suite (`firmware/test/`):** Runs 45 automated [pytest](https://github.com/pytest-dev/pytest) checks against a USB-connected camera covering Protocol v2, legacy v1, frame rates, FFC shutter, settings persistence, and SD captures.
