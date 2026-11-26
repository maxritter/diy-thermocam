# Firmware Hardware Regression Tests

Automated USB hardware tests for a real DIY-Thermocam running normal release firmware—no special test build is needed. Uses [pytest](https://github.com/pytest-dev/pytest) and [pySerial](https://github.com/pyserial/pyserial).

## What is tested (45 automated tests + 3 manual checks)

- **Protocol v2 (`test_v2_*.py`, 11 tests):** Checks handshake, `PING` latency, diagnostics, atomic config, temp points, RTC sync, and bootloader guards. Also tests Lepton CCI radiometry, FFC events, SD CRC-32 file roundtrips, and ~8.77 fps push streaming with concurrent commands.
- **Legacy v1 compatibility (`test_device.py`, `test_frames.py`, `test_actions.py`, `test_settings.py`, `test_performance.py`, 34 tests):** Self-test diagnostics, live frame limits, I2C spot-meter agreement, pixel-exact colorbars for all 19 palettes, setting roundtrips, SD capture, and frame-rate/latency baselines.
- **Interactive hardware (`test_manual.py`, 3 `--manual` tests):** Physical push button (short & long press) and touchscreen session exit.

Camera settings are saved before each module and restored automatically afterward.

## Prerequisites and running

- **OS:** macOS, Windows, or Linux (on Linux, install [PJRC `00-teensy.rules`](https://www.pjrc.com/teensy/00-teensy.rules) so the USB serial port is readable without root).
- **Python:** 3.10 or newer.
- **Camera:** Powered on, connected with a data-capable USB cable, and in Live View.

```sh
cd firmware/test
pip install -r requirements.txt

pytest                                   # runs all 45 automated v1 + v2 tests
pytest --baseline baseline-3.0.4.json    # also enforces performance baselines
pytest --manual -s                       # includes interactive button & touch tests
pytest --save-metrics new-baseline.json  # records a new performance baseline
```

`baseline-3.0.4.json` was recorded with Firmware 3.0.4 (Teensy core 1.58.2) on a Lepton 3.5 camera.
