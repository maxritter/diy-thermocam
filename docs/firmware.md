# Firmware

The DIY-Thermocam V3 firmware is open-source C++17 targeting the **Teensy 4.1** (ARM Cortex-M7 running at **450 MHz** default for lower enclosure heat and ~36% lower CPU power draw than 600 MHz). Source code lives in [`firmware/3.0/`](https://github.com/maxritter/diy-thermocam/tree/master/firmware/3.0).

## Flashing Prebuilt Firmware

### Option 1: DIY-Thermocam Desktop App (Recommended)

1. Connect the camera over USB and power it on.
2. Open the **DIY-Thermocam Desktop App** and navigate to **Camera → Firmware Update**.
3. Pick the latest GitHub Release or browse to a local `firmware.hex` file.
4. Click **Flash** — the app verifies the SHA-256 checksum, triggers the Teensy HalfKay bootloader over USB (no case disassembly needed, even from legacy 3.0.x firmware via Teensy's 134-baud trigger), writes the image, and reconnects automatically.

### Option 2: PJRC Teensy Loader (First-Time Flash on a Blank Teensy)

1. Download the latest `firmware.hex` from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases).
2. Download and open [Teensy Loader](https://www.pjrc.com/teensy/loader.html) (Windows, macOS, or Linux).
3. Enable **Auto** mode (Operations → Automatic Mode — the button lights green).
4. Open `firmware.hex` in Teensy Loader.
5. Connect the Teensy 4.1 via USB and press the physical **Program** button on the Teensy board.

![Flashing Teensy 4.1 with Teensy Loader](/images/manual/flash.png)

The camera reboots automatically and displays the DIY-Thermocam boot screen.

## Building from Source (`arduino-cli`)

The firmware builds reproducibly on macOS, Linux, and Windows using **`arduino-cli`** and **Teensy core 1.62.0** (pinned in `firmware/3.0/sketch.yaml`). No PlatformIO installation is required.

```bash
# 1. Install arduino-cli and Teensy board support
arduino-cli config add board_manager.additional_urls \
  https://www.pjrc.com/teensy/package_teensy_index.json
arduino-cli core update-index
arduino-cli core install teensy:avr@1.62.0

# 2. Compile the firmware
cd firmware/3.0
arduino-cli compile

# 3. Upload to a connected camera
arduino-cli upload -p /dev/cu.usbmodem*   # macOS (use /dev/ttyACM0 on Linux or COMx on Windows)
```

## Architecture & Performance Highlights (2026 Overhaul)

- **Double-buffered acquisition & DMA display:** Lepton VoSPI (`SPI1`) fills the back frame buffer while the main loop renders the front buffer to the 3.2" ILI9341 display over DMA (`SPI0`) and services USB Protocol v2 concurrently.
- **Integer & LUT thermal pipeline:** Precomputed 256-entry RGB565 palettes (19 color schemes) and fixed-point 2× bilinear / box / Gaussian filters keep display and USB frame rates near the Lepton's full **~8.78 fps** hardware limit.
- **Fast buffered SD storage (`SdFs`):** Supports both FAT32 microSDHC and exFAT microSDXC cards (> 32 GB) with 4–8 KB buffered `.DAT` writes (< 40 ms save target).
- **Host-side C++17 unit tests:** Pure logic under `firmware/3.0/src/core/` (Protocol v2 framing/CRC/resync, temperature conversion, palette LUTs, Lepton packet mapping, `.DAT` serialization) runs natively on CI without hardware.
- **Hardware regression suite:** Automated `pytest` suite under `firmware/3.0/test/` verifies real camera timing, USB protocol conformance, FFC shutter execution, settings persistence, and SD capture integrity.
