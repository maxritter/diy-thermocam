# DIY-Thermocam - Firmware

### You only need this guide if you want to change the firmware yourself

To put the newest firmware on your device unchanged, **download it from the [release section of this repo](https://github.com/maxritter/diy-thermocam/releases)**.

The firmware builds with [Arduino CLI](https://arduino.github.io/arduino-cli/), the same way on **Windows, Linux and macOS**. The first build downloads the right Teensy core and compiler by itself; you do not need the Arduino IDE, Teensyduino or PlatformIO.

## 1. Install Arduino CLI (once)

- **Windows:** `winget install ArduinoSA.CLI`
- **macOS:** `brew install arduino-cli`
- **Linux:** `curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh`, then put the `bin` folder it creates on your `PATH`. Also install the [Teensy udev rules](https://www.pjrc.com/teensy/00-teensy.rules) so the Teensy can be flashed without root:

  ```sh
  sudo cp 00-teensy.rules /etc/udev/rules.d/
  ```

## 2. Build

Open a terminal in the `firmware/3.0` folder and run:

```sh
arduino-cli compile
```

The settings are in `sketch.yaml`: Teensy 4.1 at 450 MHz, USB type "Serial + MTP Disk", fastest optimization and Teensy core 1.62.0, whose bundled libraries the firmware uses. The first build takes a few minutes for the downloads; later builds take about 30 seconds.

## 3. Flash

Connect the Thermocam to the PC with a micro USB cable and **turn it on**. Find its port:

```sh
arduino-cli board list
```

It is the USB serial port, for example `COM5` on Windows, `/dev/ttyACM0` on Linux or `/dev/cu.usbmodem12345601` on macOS. Then flash:

```sh
arduino-cli upload -p <port>
```

The Teensy Loader window opens, writes the firmware and restarts the device. After the restart the changes are on the device.

Both commands print `discovery teensy:teensy-discovery not installed` first; you can ignore that line.
