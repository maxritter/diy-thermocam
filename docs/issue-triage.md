> **COMPLETED — VERIFIED 2026-11-25:** Max sent the recovery email to Ron Brown (`#124`), and all **14 GitHub issues** and **2 pull requests** on `maxritter/diy-thermocam` have been replied to in Max's voice and closed (**0 open issues, 0 open PRs remaining**, verified `CLOSED` with `last_by=maxritter`).

# DIY-Thermocam Issue, PR & Support Triage Record

Audit and completion log for `maxritter/diy-thermocam` (**14 issues**, **2 PRs**, all now closed) and `maxritter/diy-thermocam-website` (**0 open issues**, **0 open PRs**), plus forensic recovery documentation for Ron Brown (`#124`).

*(Note: Issue `#103` on the obsolete TP4057 charger was already closed; its TP4056 wiring workaround should be folded into `hardware/bom/` and build docs during Phase 6 so the partlist no longer depends on a closed issue.)*

---

## 1. Urgent Support — Ron Brown (`#124`) — Resolved & Closed

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

### Email & GitHub Resolution Status
- **Email (`ron@sentsys.com.au`):** Threaded Gmail reply (`Re: Bricked DIY-Thermocam problem`, thread `1a20c2184b05bdf8`) reviewed and **sent by Max**, explaining the V1/V2 hardware numbering, why the Teensy 3.2 is not bricked, and step-by-step `Firmware_V1.hex` recovery via Teensy Loader (`Prog` to `GND`).
- **GitHub Issue `#124`:** Replied publicly in Max's "I" voice with the same V1/V2 explanation and `Firmware_V1.hex` recovery steps (without exposing private email/personal details) and **closed (`completed`)**.

---

## 2. Pull Requests (`#122`, `#123`) — Replied & Closed

| PR | Author | Classification | Verified Local Status | Final GitHub State |
|---|---|---|---|---|
| **#122** | `@chrisp429` | Build fix (superseded) | Merged locally at `eb52358` on `arduino-cli-build`, then superseded by the `arduino-cli` migration (`1528e52`). | **Closed** — thanked `@chrisp429` in "I" voice and noted the migration from PlatformIO to `arduino-cli` for the next release. |
| **#123** | `@chrisp429` | USB serial bug fix | Merged locally at `5b66b7f` on `arduino-cli-build` (fixes Python 3 UTF-8 byte bug in live viewer + `serialWait`/`serialRead16` in firmware). | **Closed** — thanked `@chrisp429` in "I" voice and confirmed both commits are merged on the upcoming release branch for the next version. |

---

## 3. All 14 Issues (`maxritter/diy-thermocam`) — Replied & Closed

### Summary Table (0 Open Remaining)

| # | Title | Type | Final State / Reason | Overhaul Tracking |
|---|---|---|---|---|
| **#124** | Updated Thermocam from V2.16 to 2.48 but now black screen | Support (Urgent) | **Closed (`completed`)** + email sent | Clarify `2.4.8` release notes (`Firmware_V1.hex` vs `V2`) |
| **#121** | Increase range? | Support / Hardware | **Closed (`not planned`)** | — |
| **#119** | Help needed troubleshooting Lepton error, try to reset . . | **Confirmed Bug** | **Closed (`completed`)** | **Phase 3 Firmware:** VoSPI 16–20 MHz `SPI_MODE3`, reset & I2C timeout hardening |
| **#115** | Using cam without display? | **Confirmed Bug** | **Closed (`completed`)** | **Phase 3 Firmware:** fix `display_init`/`touch_init` false-pass on `0x00` |
| **#114** | Good alternative for the screen in Europe | Support / BOM | **Closed (`completed`)** | **Phase 6 BOM:** document EU display alternatives & pinouts |
| **#113** | Self Diagnostic failed for Lepton device | Support / Related to #119 | **Closed (`completed`)** | **Phase 3 Firmware:** Lepton I2C/SPI init hardening |
| **#112** | Support for a visual spectrum camera | Feature (Won't-fix V3) | **Closed (`not planned`)** | — |
| **#111** | Why is reflectivity not set? | Feature / Support | **Closed (`completed`)** | **Phases 3–4:** app radiative transfer inversion + optional Lepton RAD params |
| **#110** | Incompatible with large storage microSD cards (SDXC) | **Confirmed Bug / Feature** | **Closed (`completed`)** | **Phases 1/3 Firmware:** `SdFat32` → `SdFs` (exFAT/SDXC) + `uint32_t` free space |
| **#109** | Analysis of thermal images in FLIR TOOLS | Support | **Closed (`not planned`)** | **Phase 4 Desktop App:** cross-platform `.DAT` analysis & exports |
| **#108** | VS Code is cool. What about others? | Feature / Docs (Resolved) | **Closed (`completed`)** | **Phase 1:** `arduino-cli` migration (`1528e52`) |
| **#107** | Help ! "firmware updater.exe" for DIY-thermocam V2 (Teensy 3.6) | Support (Legacy V2) | **Closed (`completed`)** | — |
| **#106** | Raspberry Pi | Support | **Closed (`completed`)** | **Phases 3–4:** Protocol v2 + Linux desktop app |
| **#105** | About the FPS | Support / Hardware limit | **Closed (`not planned`)** | **Phase 3 Firmware:** optimize on-device display FPS toward ~8.78 Hz |

### Verified Technical Evidence & Posted Resolution Notes

- **`#124` (Support — Closed `completed`):** V1 Teensy 3.2 accidentally flashed with `Firmware_V2.hex` (Teensy 3.6); not bricked due to dedicated MKL02 bootloader. Provided step-by-step `Firmware_V1.hex` recovery via both email (sent by Max) and public issue comment.
- **`#121` (Hardware limit — Closed `not planned`):** FLIR Lepton has a fixed 56° HFOV lens; extending range requires external Ge/ZnSe LWIR optics (costly, breaks radiometric calibration).
- **`#119` (Confirmed Bug — Closed `completed`, tracked in Phase 3):** `lepton.cpp:74` clocks VoSPI at **25 MHz `SPI_MODE1`**, exceeding the Lepton 3.x spec (**max 20 MHz `SPI_MODE3`**)—explaining why modules work on Raspberry Pi (16–20 MHz) yet fail on Teensy 4.1. `lepton_reset()` also runs with VSYNC IRQ attached and leaves CS LOW, and `lepton_i2cRead/WriteDataRegister` have unbounded busy-wait loops. Thanked reporter for RPi verification and confirmed fix is tracked for the next firmware version.
- **`#115` (Confirmed Bug — Closed `completed`, tracked in Phase 3):** `display.cpp:429-440` treats `ILI9341_RDSELFDIAG == 0x00` as valid and `touchscreen.cpp:97-99` passes on `(0, 0)` + HIGH IRQ, so `checkNoDisplay()` fails to switch to headless USB mode. Confirmed bug and noted fix for the next firmware version.
- **`#114` (Support / BOM — Closed `completed`):** Any 3.2" 320×240 ILI9341 4-wire SPI + XPT2046/FT6206 5 V display works electrically with jumper wiring to match the V3 pinmap; linked hardware docs.
- **`#113` (Support — Closed `completed`, related to `#119`):** `lepton_init()` sets `diag_lepton` when I2C `0x481C` fails or returns an unsupported part number (not `05-070360`/`05-070170`/`05-070850`). Provided I2C pull-up / seating / part-number checklist, noted init hardening in next firmware, and invited reopen if still active.
- **`#112` (Feature — Closed `not planned`):** Explained V1/V2 visual camera history and why V3 stays thermal-only (no visual camera interface on V3 PCB).
- **`#111` (Feature / Support — Closed `completed`, tracked in Phases 3–4):** `temperature.cpp` and `lepton.cpp` use Lepton's factory T-Linear defaults (`ε = 1.00`) without writing RAD Flux Linear Parameters (`0x4EBC`/`0x4EBD`). Explained current default and noted configurable emissivity/reflected temperature is tracked in the upcoming overhaul.
- **`#110` (Confirmed Bug / Feature — Closed `completed`, tracked in Phases 1/3):** `globalvariables.cpp:47` uses `SdFat32` (FAT16/32 only), `sdcard.cpp:134` rejects >32 GB (`67108864` sectors), and `sdcard.cpp:51` overflows `uint16_t` free space >64 GB. Provided SDHC ≤32 GB / FAT32 workaround and noted `SdFs` exFAT/SDXC support is tracked for the next firmware version.
- **`#109` (Support — Closed `not planned`):** `save.cpp` writes `.BMP` + open `.DAT` radiometric files rather than FLIR proprietary Radiometric JPEG (`FFF`). Provided ThermoVision / Thermal Data Viewer workaround and noted the upcoming cross-platform desktop app.
- **`#108` (Feature / Docs — Closed `completed`):** Explained the migration from VS Code/PlatformIO to `arduino-cli` (`firmware/sketch.yaml`, commit `1528e52`) shipping in the next release.
- **`#107` (Legacy V2 Support — Closed `completed`):** In `2.4.8:hardware.cpp`, `initHardware()` runs `touch_init` → `camera_init` → `lepton_init` → `mlx90614_init` → `initSD` right after `bootScreen()`. Apologized for the delay and gave targeted V2 (Teensy 3.6) isolation steps (I2C pull-ups, `Firmware_V2.hex`, SD/camera/Lepton disconnect).
- **`#106` (Support — Closed `completed`):** Explained Raspberry Pi USB serial (`/dev/ttyACM0`, 115200 baud), Python live viewer, and serial protocol documentation.
- **`#105` (Hardware limit — Closed `not planned`):** Explained the hard-coded ~8.7 Hz (`<9 Hz`) FLIR Lepton US export limit vs on-device ~4.4–5 FPS SPI/rendering overhead targeted for optimization in Phase 3.
