# Firmware

DIY-Thermocam firmware is open-source C++17 targeting the **Teensy 4.1**. It defaults to **450 MHz** to cut enclosure heat and lower CPU power draw by ~36% compared to 600 MHz. Source code lives in [`firmware/`](https://github.com/maxritter/diy-thermocam/tree/master/firmware).

## Flashing Prebuilt Firmware

### Option 1: DIY-Thermocam Desktop Studio (Recommended)

1. Connect the camera over USB and turn it on.
2. Open **DIY-Thermocam Desktop Studio** and go to **Camera → Firmware Update**.
3. Choose the latest GitHub Release or a local `firmware.hex` file.
4. Click **Flash**. The app verifies SHA-256, triggers the Teensy HalfKay bootloader over USB without opening the case (including from Firmware 3.0.x via Teensy's 134-baud trigger), flashes the image, and reconnects.

### Option 2: PJRC Teensy Loader (First Flash on a Blank Teensy)

1. Download `firmware.hex` from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases).
2. Download and open [Teensy Loader](https://www.pjrc.com/teensy/loader.html) on Windows, macOS, or Linux.
3. Enable **Auto** mode (Operations → Automatic Mode).
4. Open `firmware.hex` in Teensy Loader.
5. Connect the Teensy 4.1 over USB and press the physical **Program** button.

![Flashing Teensy 4.1 with Teensy Loader](/images/manual/flash.png)

The camera reboots automatically and displays the DIY-Thermocam boot screen.

## Building from Source (`arduino-cli`)

The firmware builds on macOS, Linux, and Windows with **`arduino-cli`** and **Teensy core 1.62.0** (pinned in `firmware/sketch.yaml`). PlatformIO is not required.

```bash
# 1. Install arduino-cli and Teensy board support
arduino-cli config add board_manager.additional_urls \
  https://www.pjrc.com/teensy/package_teensy_index.json
arduino-cli core update-index
arduino-cli core install teensy:avr@1.62.0

# 2. Compile the firmware
cd firmware
arduino-cli compile

# 3. Upload to a connected camera
arduino-cli upload -p /dev/cu.usbmodem*   # macOS (/dev/ttyACM0 on Linux, COMx on Windows)
```

## Architecture

- **Double-buffered acquisition and DMA display:** Lepton VoSPI (`SPI1`) fills the back buffer while the main loop renders the front buffer over DMA (`SPI0`) and handles USB Protocol v2.
- **Integer and LUT pipeline:** Precomputed 256-entry RGB565 palettes (19 color schemes) and fixed-point spatial filters sustain the Lepton's **8.77 fps** native frame rate.
- **Buffered SD storage (`SdFs`):** Supports FAT32 microSDHC and exFAT microSDXC cards (> 32 GB) with fast buffered `.DAT` writes.
- **Host unit tests:** Hardware-independent logic in `firmware/src/core/` (Protocol v2 framing/CRC, temperature conversion, palette LUTs, `.DAT` serialization) runs natively on CI without hardware.
- **Hardware regression suite:** `firmware/test/` runs `pytest` against a connected camera to verify frame timing, USB protocol conformance, FFC shutter execution, settings persistence, and SD captures.
