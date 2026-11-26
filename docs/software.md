# Desktop App and Software

**DIY-Thermocam Desktop Studio** is an open-source app for **macOS, Windows, and Linux**. It shows the live image, measures temperatures, saves and edits full-temperature captures, exports pictures and video, reads the camera's microSD card, and updates the firmware.

![Live view with four measuring regions, the color scale and the chart below](/images/studio/studio-live-pro.png)

## Supported systems

Download prebuilt installers from [GitHub Releases](https://github.com/maxritter/diy-thermocam/releases).

| Operating System | Architecture | Prerequisites |
| --- | --- | --- |
| **macOS 12+** | Apple Silicon (`arm64`) & Intel (`x64`) | None (USB serial & HalfKay HID work out of the box) |
| **Windows 10 / 11** | `x64` | None (WebView2 & USB CDC/HID drivers are built in) |
| **Linux** | `x86_64` & `ARM64` | Add your user to the `dialout` group and install the PJRC udev rules once; see [Linux USB permissions](/docs/manual/#linux-usb-permissions) |

## How Studio works

Built with [Tauri v2](https://github.com/tauri-apps/tauri), Rust (`thermocam-core`), React 19, and WebGL2, Desktop Studio talks to the camera over [USB Protocol v2](/docs/protocol-v2/). On real Teensy 4.1 + Lepton 3.5 hardware, it sustains **8.77 fps** live streaming, **0.26 ms** median command latency, and **9.20 MB/s** direct microSD reads. `.DAT` captures from every DIY-Thermocam camera open natively.

| Live | An opened capture | Captures library |
| --- | --- | --- |
| ![Studio Live view with the thermal image, the color scale and the palettes beside it](/images/studio/studio-live-standard.png) | ![A saved snapshot opened in Captures, with a measuring box and its readings](/images/studio/studio-editor.png) | ![The Captures library listing the pictures on the camera's card](/images/studio/studio-sd-browser.png) |

Studio has three places, chosen in the header: **Live**, **Captures** and **Camera**. There are no modes to switch: every tool is always one click away.

### First start

With no camera plugged in, Studio shows **Connect your Thermocam**. Plug in the camera and it connects by itself, or click **Try the demo camera** to explore Studio with a simulated image. If the camera is unplugged, the last image stays on screen, dimmed, and Studio reconnects when the camera returns.

### Live

- **One viewer:** the live image fills the window beside the color scale. Drag the scale's handles to fix the range, or leave it on **Auto**. The histogram sits beside the bar.
- **Palettes:** all 19 firmware palettes plus `Inferno`, `Viridis`, `Magma`, `Plasma`, `Turbo`, and your own imported palettes (**Image › All palettes**).
- **Measuring:** draw spots (1 pixel or 3 × 3), boxes, circles, polygons, lines and differences from the tool rail on the image (keys `V S B C P L D`). Move, resize and rename regions on the image. The **Measure** tab lists each region's min, average, max, σ and pixel count, and each region can have its own material.
- **Material:** set emissivity and reflected temperature once for the whole image, or per region.
- **Alarms:** in the **Alarms** tab, notify you, take a snapshot or start a recording when a reading crosses a limit.
- **Charts:** the charts below the image log temperatures for the whole session and show line profiles. **Save CSV…** writes every sample since the start.
- **Rare options:** smoothing, noise reduction, rotation and mirroring sit under **Image › More image options**. Raw sensor values, sensor temperature and frame numbers are under the gear menu's **Engineering details**.
- **Keyboard:** press `?` for the list of shortcuts.

### Snapshot, Record and Export

- **Snapshot** saves the current frame as a full-temperature `.DAT` capture, named by date and time, in your Captures folder. Its message offers **Copy image**, **Save PNG…** and **Show in Finder** (Explorer on Windows, folder on Linux). To get a PNG every time, switch on **Also save a PNG with every snapshot** in the gear menu.
- **Record** writes a recording to disk frame by frame, so its length is limited only by free space. Record won't start with less than 500 MB free, and it stops by itself, keeping every frame, when space runs low. The arrow beside Record sets a time-lapse interval and the folder.
- The Captures folder is **Pictures › Thermocam** by default. Change it in the gear menu.
- **Export…** (`⌘E` on macOS, `Ctrl+E` elsewhere) writes MP4, animated GIF, PNG/JPEG, temperature CSV, 16-bit Kelvin TIFF, raw `.DAT` frames, and HTML/Markdown reports, at any size. MP4 uses the bundled [OpenH264](https://github.com/cisco/openh264), so there is no FFmpeg to install.

### Captures

- **One library** for the camera's microSD card (**On the camera**, read over USB without MTP drivers or reboots) and **On this computer**. Search, sort, and switch between a grid and a list.
- **Opening a capture** uses the Live viewer with every tool. **Compare with…** shows two captures side by side with a divider.
- **Edits save by themselves.** Palette, range, material and regions are kept with **Undo** and **Reset to original**. The original `.DAT` file never changes: edits sit beside it on this computer, or in Studio's own data folder for files on the camera's card.
- **Recordings play back** with time and frame number, a chart for the whole recording, and trim. **Export…** writes only the trimmed part. A video from the camera's card is copied to this computer before it plays.
- **Delete** asks first and names how many files. Files on this computer go to the Trash or Recycle Bin.

### Camera

- **Settings apply at once** and are saved on the camera; there is no separate save step. A setting the camera refuses flips back with its reason.
- **Measuring range**, **Mounted upside down**, and what the camera's screen shows: spot temperature, temperature scale and battery level.
- **When you press the button on the camera:** **Snapshot in Studio**, **Record in Studio** or **Nothing**.
- **Use Studio's material on the camera's screen** sends Live's emissivity and reflected temperature to the sensor. It is off by default, and connecting never changes the camera. Switching it off puts the camera back on its factory values.
- **Update firmware…** installs a Teensy 4.1 `.hex` file. It checks the file as soon as you choose it, then restarts the camera in update mode and writes the firmware. It also works with no camera connected: Studio waits for a Teensy in update mode, for example after you press its program button. See [Firmware and Flashing](/docs/firmware/#with-studio-recommended).
- **Diagnostics** shows frame timing, link health and memory, with **Copy for a bug report**.
