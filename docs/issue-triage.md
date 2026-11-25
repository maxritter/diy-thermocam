> **DRAFT — PENDING MAX'S APPROVAL (NO GITHUB COMMENTS, CLOSES, OR EMAILS SENT)**

# DIY-Thermocam Issue, PR & Support Triage Packet

Read-only audit of `maxritter/diy-thermocam` (**14 open issues**, **2 open PRs**) and `maxritter/diy-thermocam-website` (**0 open issues**, **0 open PRs**), plus forensic recovery analysis for Ron Brown (`#124`).

*(Note: Issue `#103` on the obsolete TP4057 charger is already **closed**; its TP4056 wiring workaround should be folded into `hardware/bom/` and build docs during the overhaul so the partlist no longer depends on a closed issue.)*

---

## 1. Urgent Support — Ron Brown (`#124`)

### Forensic Root Cause
Verified against `DIY-Thermocam log.txt` and `DIYThermocam_2.48.zip`:

- **Hardware vs. firmware numbering:** **DIY-Thermocam V1** uses a **Teensy 3.1/3.2** (`MK20DX256`, 256 KB flash, 64 KB RAM). **DIY-Thermocam V2** uses a **Teensy 3.6** (`MK66FX1M0`, 1 MB flash, 256 KB RAM), introduced in Firmware 2.30 (29.11.2016). Firmware `2.00`–`2.48` ran on **both** V1 and V2 hardware. Because Firmware 2.16 (25.09.2016) predates V2 hardware, Ron owns a **V1 (Teensy 3.2)**.
- **What failed in the log:** `DIYThermocam_2.48.zip` contains two images:
  - `Firmware_V1.hex` (246,416 B, Initial SP `0x20008000` → Teensy 3.2 / `MK20DX256`, for V1)
  - `Firmware_V2.hex` (246,192 B, Initial SP `0x20030000` → Teensy 3.6 / `MK66FX1M0`, for V2)
  - Lines 5–7 confirm `Board is: Teensy 3.2 (MK20DX256)` flashed with `Firmware_V2.hex` (94% of 256 KB). Without a companion `.elf` file (lines 11–13: `elf file is for Unknown Board / using hex file`), Teensy Loader could not block the MCU mismatch and flashed all 241 blocks cleanly (`IMG_REBOOT_OK`, 2.545 s).
  - On reboot, the Teensy 3.2 loaded Initial SP `0x20030000` (past its 64 KB RAM boundary `0x20008000`) and executed Teensy 3.6 startup code, immediately hard-faulting before display or USB initialized. *(Log lines 262–265 `vid:046D pid:B378` are an unrelated Logitech HID device.)*
- **Not bricked:** Teensy 3.2's HalfKay bootloader runs on a separate dedicated **MKL02** chip, and `Firmware_V2.hex`'s flash security byte (`FSEC` at `0x40C`) is `0xDE` (unsecured, mass-erase enabled). Shorting `Prog` to `GND` enters HalfKay reliably.
- **Follow-up docs action:** Clarify in the `2.4.8` GitHub Release description that `Firmware_V1.hex` is for V1 (Teensy 3.1/3.2) and `Firmware_V2.hex` is for V2 (Teensy 3.6).

### Draft Unsent Email to Ron Brown (`ron@sentsys.com.au`)

> **Gmail Draft Status:** Saved as an **unsent threaded reply draft** (`r2547252423999641720`, thread `1a20c2184b05bdf8`, subject `Re: Bricked DIY-Thermocam problem`) in Max's Gmail for review and manual sending. Original thread left open and unarchived.

> **Subject:** Re: Bricked DIY-Thermocam problem
>
> Hi Ron,
>
> Good news first: your DIY-Thermocam is not bricked, and you can recover it in a couple of minutes.
>
> Thank you for sending the Teensy Loader log — it shows the exact cause right away.
>
> **Why you had Firmware 2.16 on a Teensy 3.2:**
> The hardware version and the firmware version used separate numbering back then. Firmware 2.00 through 2.48 was a software generation that ran on both hardware revisions:
> - **DIY-Thermocam V1** uses a **Teensy 3.1 / 3.2** (which is what you have — Firmware 2.16 was actually released before V2 hardware existed).
> - **DIY-Thermocam V2** uses a **Teensy 3.6**.
>
> **What happened during flashing:**
> Inside `DIYThermocam_2.48.zip`, `Firmware_V1.hex` is compiled for the Teensy 3.2 (V1 hardware) and `Firmware_V2.hex` is compiled for the Teensy 3.6 (V2 hardware). From your log, `Firmware_V2.hex` was flashed onto your Teensy 3.2. Because it happens to be slightly smaller than the Teensy 3.2's flash memory and there is no `.elf` file to warn Teensy Loader of the board mismatch, the flash actually completed cleanly (`IMG_REBOOT_OK`), and then the Teensy 3.2 immediately crashed on reboot because it tried to run Teensy 3.6 code — leaving the screen black.
>
> **How to recover your camera:**
> On a Teensy 3.2, the bootloader runs on a separate dedicated chip, so flashing the wrong `.hex` cannot permanently damage it. Just flash `Firmware_V1.hex` using the same method you used before:
>
> 1. Extract **`Firmware_V1.hex`** (not `Firmware_V2.hex`) from `DIYThermocam_2.48.zip` (available at https://github.com/maxritter/diy-thermocam/releases/tag/2.4.8).
> 2. Open **Teensy Loader**, click **File -> Open HEX File**, and select **`Firmware_V1.hex`**. Confirm the bottom bar of Teensy Loader shows `Firmware_V1.hex`.
> 3. Make sure **Auto** mode is enabled (button illuminated bright green), and open **Help -> Verbose Info** if you want to watch the progress.
> 4. Connect the DIY-Thermocam to your PC via USB and **switch the camera ON**.
> 5. Momentarily short the **`Prog` pad to `GND`** for about half a second and release (just like before).
> 6. Teensy Loader should detect `Teensy 3.2 (MK20DX256)`, flash `Firmware_V1.hex`, and reboot automatically. After a few seconds, the screen should come back with the **DIY-Thermocam V1 / Firmware 2.48** boot screen.
>
> *(If Teensy Loader doesn't detect it when you short `Prog` to `GND`, unplug USB, hold `Prog` to `GND` while plugging the USB cable back in, then release.)*
>
> Please let me know if that brings your Thermocam back to life!
>
> Best regards,
> Max

### Draft Public Reply for Issue `#124`

> Hi Ron, good news — your Teensy 3.2 is not bricked and can be recovered easily.
>
> Your log shows the exact cause: your unit is a **DIY-Thermocam V1** (Teensy 3.2), and `Firmware_V2.hex` (which is compiled for the V2's Teensy 3.6) was flashed to it. The hardware and firmware version numbers were separate back then — Firmware 2.00–2.48 ran on both V1 (Teensy 3.2) and V2 (Teensy 3.6), which is why your V1 was running Firmware 2.16. Because `Firmware_V2.hex` fits inside the Teensy 3.2's flash and has no `.elf` file for a board check, Teensy Loader flashed it cleanly, and the Teensy 3.2 then crashed immediately on reboot, leaving the screen black.
>
> To recover it:
> 1. Extract **`Firmware_V1.hex`** (not `Firmware_V2.hex`) from `DIYThermocam_2.48.zip` (https://github.com/maxritter/diy-thermocam/releases/tag/2.4.8).
> 2. Open **Teensy Loader**, load **`Firmware_V1.hex`**, and enable **Auto** mode.
> 3. Connect USB, turn the camera **ON**, and momentarily short **`Prog` to `GND`** (~0.5 s) just as you did before.
> 4. Teensy Loader will flash `Firmware_V1.hex` via the Teensy 3.2's dedicated bootloader chip and reboot the camera back to the Firmware 2.48 boot screen.
>
> Let me know once it's back up and running!

---

## 2. Open Pull Requests (`#122`, `#123`)

| PR | Author | Classification | Verified Local Status | Recommended Action |
|---|---|---|---|---|
| **#122** | `@chrisp429` | Build fix (superseded) | Merged locally at `eb52358` on `arduino-cli-build`, then superseded by the `arduino-cli` migration (`1528e52`). | Auto-marks merged when `arduino-cli-build` is pushed to `master`; post thank-you comment noting the `arduino-cli` move. |
| **#123** | `@chrisp429` | USB serial bug fix | Merged locally at `5b66b7f` on `arduino-cli-build` (fixes Python 3 UTF-8 byte bug in live viewer + `serialWait`/`serialRead16` in firmware). | Auto-marks merged when `arduino-cli-build` is pushed to `master`; thank contributor and note it ships in the next release. |

- **Draft reply `#122`:**
  > Thanks for the PR and the clean diagnosis! I merged this as the starting point and then migrated the firmware build to `arduino-cli` (`sketch.yaml` with pinned Teensy core and vendored libs) so it builds reliably across Windows, Linux, and macOS without PlatformIO conflicts.
- **Draft reply `#123`:**
  > Thanks a lot for tracking down both sides of the USB serial bug and testing it on real hardware! I've merged this into the upcoming release branch — it will be included in the next firmware/software release.

---

## 3. All 14 Open Issues (`maxritter/diy-thermocam`)

### Summary Table

| # | Title | Type | Action | Overhaul Link |
|---|---|---|---|---|
| **#124** | Updated Thermocam from V2.16 to 2.48 but now black screen | Support (Urgent) | Email + reply; keep open until Ron confirms recovery | Update `2.4.8` release notes |
| **#121** | Increase range? | Support / Hardware | Reply & close | — |
| **#119** | Help needed troubleshooting Lepton error, try to reset . . | **Confirmed Bug** | Reply; keep open until fix ships | Phase 3 Firmware (VoSPI 16–20 MHz `SPI_MODE3`, reset/I2C hardening) |
| **#115** | Using cam without display? | **Confirmed Bug** | Reply; keep open until fix ships | Phase 3 Firmware (fix `display_init`/`touch_init` `0x00` check) |
| **#114** | Good alternative for the screen in Europe | Support / BOM | Reply & close | Phase 6 BOM (document EU display pinout/alternatives) |
| **#113** | Self Diagnostic failed for Lepton device | Support / Related to #119 | Reply & close (invite reopen) | Phase 3 Firmware (Lepton I2C/SPI hardening) |
| **#112** | Support for a visual spectrum camera | Feature (Won't-fix V3) | Reply & close | — |
| **#111** | Why is reflectivity not set? | Feature / Support | Reply; keep open if planned for overhaul (else close) | Phase 3/4 (app radiative inversion + optional Lepton RAD params) |
| **#110** | Incompatible with large storage microSD cards (SDXC) | **Confirmed Bug / Feature** | Reply with FAT32 workaround; keep open until fix ships | Phase 1/3 Firmware (`SdFat32` → `SdFs` + `uint32_t` free space) |
| **#109** | Analysis of thermal images in FLIR TOOLS | Support | Reply & close | Phase 4 Desktop App (`.DAT` analysis & exports) |
| **#108** | VS Code is cool. What about others? | Feature / Docs (Solved) | Reply & close once `arduino-cli-build` lands on `master` | Phase 1 (`arduino-cli` migration) |
| **#107** | Help ! "firmware updater.exe" for DIY-thermocam V2 (Teensy 3.6) | Support (Legacy V2) | Reply with V2 checklist & close | — |
| **#106** | Raspberry Pi | Support | Reply & close | Phase 3/4 (Protocol v2 + Linux desktop app) |
| **#105** | About the FPS | Support / Hardware limit | Reply & close | Phase 3 Firmware (optimize display FPS toward ~8.78 Hz) |

### Detailed Evidence & Draft Replies (Max's "I" Voice)

#### `#124` — Updated Thermocam from V2.16 to 2.48 but now black screen
- **Classification:** Urgent Support. **Action:** Send email + post public reply (see Section 1); keep open until Ron confirms recovery.

#### `#121` — Increase range?
- **Classification:** Support / Hardware limitation.
- **Evidence:** FLIR Lepton has a fixed 56° HFOV lens; extending range requires external Ge/ZnSe LWIR optics, which are costly and invalidate radiometric calibration.
- **Action:** Reply & close.
- **Draft reply:**
  > The FLIR Lepton has a fixed 56° FOV lens built into the module, so increasing detection range isn't possible in software — as @dreiduke mentioned, you would need external LWIR optics (germanium or ZnSe lenses), which are quite expensive and require custom mounting and recalibration. For long-range imaging, a camera built around a replaceable-lens thermal core is usually the better route. Closing this as a hardware/optics limitation.

#### `#119` — Help needed troubleshooting Lepton error, try to reset . .
- **Classification:** Confirmed Bug.
- **Evidence:** `lepton.cpp:74` clocks VoSPI at **25 MHz `SPI_MODE1`**, exceeding the Lepton 3.x specification (**max 20 MHz `SPI_MODE3`**)—explaining why modules work on Raspberry Pi (16–20 MHz) yet fail on Teensy 4.1. Additionally, `lepton_reset()` runs with VSYNC IRQ attached and leaves CS LOW, and `lepton_i2cRead/WriteDataRegister` have unbounded `while (lepton_readReg(0x2) & 0x01);` loops.
- **Action:** Address in Phase 3 Firmware Overhaul; post brief update and keep open until verified fix ships.
- **Draft reply:**
  > Thanks for the thorough debugging and for testing the Lepton modules on a Raspberry Pi — that really helps narrow this down. Looking closely at `lepton.cpp`, the V3 firmware currently clocks the Lepton SPI bus at 25 MHz (`SPI_MODE1`), which is above the Lepton 3.x VoSPI specification (max 20 MHz, `SPI_MODE3`), and the reset/I2C wait paths also need hardening. I'm investigating this as part of the next firmware release.

#### `#115` — Using cam without display?
- **Classification:** Confirmed Bug.
- **Evidence:** `display.cpp:429-440` treats `ILI9341_RDSELFDIAG == 0x00` as valid (which floating/zero SPI MISO returns with no display attached), and `touchscreen.cpp:97-99` similarly passes on `(0, 0)` + HIGH IRQ. Consequently `checkNoDisplay()` never triggers `serialInit()` and the firmware hangs in `firstStart()`/`liveModeHelper()`.
- **Action:** Fix in Phase 3 Firmware Overhaul; post reply and keep open until fix ships.
- **Draft reply:**
  > Thanks for digging into this and sharing what worked. I checked `display_init()` and `touch_init()` in the firmware and confirmed a bug: when no display is attached, reading `0x00` over SPI causes the display/touch check to falsely pass, so `checkNoDisplay()` doesn't switch to headless USB serial mode and the camera gets stuck waiting for a touch. This will be fixed in the next version.

#### `#114` — Good alternative for the screen in Europe
- **Classification:** Support / BOM.
- **Evidence:** Any 3.2" 320×240 ILI9341 4-wire SPI + XPT2046/FT6206 5 V display works electrically, though non-BuyDisplay pinouts require jumper wiring. User already found pinouts.
- **Action:** Reply & close (document pinouts/alternatives in Phase 6 BOM).
- **Draft reply:**
  > Glad you found the pinouts! BuyDisplay ships to Europe, though shipping can take a while. Electrically, any 3.2" 320×240 ILI9341 display with 4-wire SPI, 5V supply, and an XPT2046 (resistive) or FT6206 (capacitive) touch controller will work, but the header pinouts vary between vendors so you'd need jumper wires to match the V3 pinmap (https://www.diy-thermocam.net/docs/hardware/). Closing this since you found the pinouts — feel free to reopen if you run into wiring issues.

#### `#113` — Self Diagnostic failed for Lepton device
- **Classification:** Support / Related to `#119`.
- **Evidence:** `lepton_init()` sets `diag_lepton` when I2C command `0x481C` fails or returns a part number other than `05-070360`, `05-070170`, or `05-070850`.
- **Action:** Reply with checklist and close due to ~2 years of inactivity (inviting reopen).
- **Draft reply:**
  > Sorry for the late reply! A Lepton self-diagnostic failure means the Teensy couldn't read a supported part number over I2C (`0x481C`) during `lepton_init()`. The usual causes are: (1) missing or wrong 4.7k I2C pull-ups / solder bridge on SDA/SCL (pins 18/19), (2) Lepton module not fully seated in the Breakout V2 socket or unstable 3.3V/5V power, or (3) a non-radiometric Lepton variant (V3 supports Lepton 2.5, 3.1R, and 3.5). I'm also tightening Lepton I2C/SPI initialization in the next firmware release. Closing this due to age, but please reopen if you're still troubleshooting it.

#### `#112` — Support for a visual spectrum camera
- **Classification:** Feature Request (Won't-fix for V3).
- **Evidence:** V1/V2 included a VC0706/ArduCAM visual camera; deliberately removed in V3 for cost, size, and reliability, and V3 PCB has no visual camera interface.
- **Action:** Reply & close.
- **Draft reply:**
  > DIY-Thermocam V1 and V2 actually included a visual spectrum camera (VC0706 / ArduCAM) with a combined thermal+visual mode, but I deliberately removed it for V3 to simplify the build, reduce cost/size, and improve thermal reliability. Since the V3 PCB doesn't have a visual camera interface, I'm not planning to add one to V3. Closing as not planned for V3.

#### `#111` — Why is reflectivity not set?
- **Classification:** Feature Request / Support.
- **Evidence:** `temperature.cpp` and `lepton.cpp` leave Lepton at factory T-Linear defaults (`ε = 1.0`) and never write Lepton SDK `RAD Flux Linear Parameters` (`0x4EBC`/`0x4EBD`).
- **Action:** Reply; keep open if emissivity/reflected-temp support is approved for the overhaul (app radiative transfer inversion and/or camera CCI), otherwise close.
- **Draft reply:**
  > Good question — right now the V3 firmware leaves the FLIR Lepton at its factory radiometric defaults (emissivity = 1.00 / 100%) and does not expose emissivity or reflected background temperature via the Lepton SDK's RAD Flux Linear Parameters. I'll keep this open to consider adding configurable emissivity/reflected temperature in the upcoming firmware and desktop app overhaul.

#### `#110` — Incompatible with large storage microSD cards (SDXC)
- **Classification:** Confirmed Bug / Feature.
- **Evidence:** `globalvariables.cpp:47` uses `SdFat32 sd;` (FAT16/FAT32 only), `sdcard.cpp:134` rejects cards >`67108864` sectors (>32 GB) during formatting, and `sdcard.cpp:51` casts free space to `uint16_t` (overflows >64 GB). Teensyduino 1.62 SdFat provides `SdFs` (FAT16/32/exFAT).
- **Action:** Address in Phase 1/3 Firmware Overhaul (`SdFat32` → `SdFs` + `uint32_t`); reply with FAT32/≤32 GB workaround and keep open until fix ships.
- **Draft reply:**
  > Thanks for reporting this! Right now the firmware uses `SdFat32` (FAT16/FAT32 only) and blocks cards larger than 32 GB when formatting, which is why exFAT SDXC cards don't work. As a workaround today, you can use an SDHC card (up to 32 GB) or format a larger card as FAT32. I'm looking into switching to `SdFs` (exFAT/SDXC support) for the next firmware version.

#### `#109` — Analysis of thermal images in FLIR TOOLS
- **Classification:** Support.
- **Evidence:** `save.cpp` saves `.BMP` + open `.DAT` radiometric files rather than FLIR proprietary Radiometric JPEG (`FFF`), so FLIR Tools cannot read temperature metadata directly.
- **Action:** Reply & close.
- **Draft reply:**
  > DIY-Thermocam saves standard `.BMP` images alongside an open `.DAT` raw radiometric file (documented at https://www.diy-thermocam.net/docs/raw-data/), rather than FLIR's proprietary Radiometric JPEG format, so FLIR Tools won't read temperature metadata directly. As a workaround, you can open the `.DAT` files in ThermoVision by Joe-C (which can also export FLIR-compatible radiometric JPEGs) or Thermal Data Viewer. I'm also working on a new cross-platform desktop app for analyzing `.DAT` files. Closing this as answered.

#### `#108` — VS Code is cool. What about others?
- **Classification:** Feature / Docs (Resolved locally on `arduino-cli-build`).
- **Evidence:** Commit `1528e52` replaced PlatformIO/VS Code with `arduino-cli` (`firmware/3.0/sketch.yaml`, `firmware/README.MD`).
- **Action:** Reply & close once `arduino-cli-build` is pushed to `master`.
- **Draft reply:**
  > Great point — I've migrated the firmware build from VS Code + PlatformIO to `arduino-cli` (`firmware/3.0/sketch.yaml`), so you can now compile and flash from any terminal or editor with `arduino-cli compile` and `arduino-cli upload` without installing VS Code. See `firmware/README.MD` once the update lands on `master`. Closing this as resolved.

#### `#107` — Help ! "firmware updater.exe" for DIY-thermocam V2 (Teensy 3.6)
- **Classification:** Support (Legacy V2 hardware).
- **Evidence:** In `2.4.8:hardware.cpp`, `initHardware()` runs `touch_init` → `camera_init` → `lepton_init` → `mlx90614_init` → `initSD` immediately after `bootScreen()`. Hanging on `Booting` points to an I2C bus fault (missing 4.7k pull-ups / shorted sensor), flashing `Firmware_V1.hex` on V2, or a faulty SD/Lepton/visual camera holding the bus.
- **Action:** Reply with apology + isolation checklist and close (legacy V2 hardware, >2 years old).
- **Draft reply:**
  > Hi Olivier, I'm sorry for the very late reply! Glad you found Teensy Loader to flash V2.48. On DIY-Thermocam V2 (Teensy 3.6), getting stuck on the `Booting` screen means `initHardware()` is hanging in one of the peripheral checks right after `bootScreen()` (`touch_init`, `camera_init`, `lepton_init`, `mlx90614_init`, or `initSD`). The most common causes are: (1) an I2C bus issue on SDA/SCL (missing pull-ups or a shorted/miswired sensor), (2) flashing `Firmware_V1.hex` instead of `Firmware_V2.hex` (make sure you used `Firmware_V2.hex` for Teensy 3.6), or (3) an incompatible SD card or Lepton/camera module holding the bus. Try booting with the SD card removed first, then test disconnecting the visual camera or Lepton to isolate which peripheral causes the hang. Since V2 is legacy hardware and this thread is over two years old, I'll close this for now — feel free to comment if you're still working on it.

#### `#106` — Raspberry Pi
- **Classification:** Support.
- **Evidence:** Camera enumerates as standard USB CDC serial (`/dev/ttyACM0`, 115200 baud), usable via `software/thermal_live_viewer/3.0/` (with `#123` fixes) or direct serial scripts.
- **Action:** Reply & close.
- **Draft reply:**
  > You can connect DIY-Thermocam V3 directly to a Raspberry Pi over USB — it shows up as a standard serial port (e.g., `/dev/ttyACM0` at 115200 baud). From there, you can either run the Python `thermal_live_viewer` in `software/thermal_live_viewer/3.0/` (note: upcoming fixes from PR #123 resolve Python 3 USB command bugs) or talk to the camera directly from your own script using the serial protocol documented at https://www.diy-thermocam.net/docs/serial/. Closing as answered.

#### `#105` — About the FPS
- **Classification:** Support / Hardware limit.
- **Evidence:** The `<9 Hz` (~8.7 Hz) ceiling is enforced inside the FLIR Lepton core for US export compliance. On-device ~4.4–5 FPS comes from SPI display transfer and rendering overhead, targeted for optimization in Phase 3.
- **Action:** Reply & close.
- **Draft reply:**
  > The ~8.7 FPS (<9 Hz) upper limit is hard-coded inside the FLIR Lepton sensor itself for US export compliance, so it cannot be increased in DIY-Thermocam firmware or over USB — exceeding 9 Hz requires a different, export-controlled sensor core. The lower ~4.5–5 FPS on the device screen itself is due to SPI display transfer and rendering overhead, which I plan to optimize closer to the sensor's ~8.7 Hz limit in the next firmware version. Closing as answered.
