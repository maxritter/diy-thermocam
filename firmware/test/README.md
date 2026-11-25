# Firmware regression tests

These tests run on a real DIY-Thermocam connected over USB. They talk to the camera with the same serial protocol as the live viewer, and the camera stays in its normal firmware; no test build is needed.

They check that:

- the camera runs the firmware of this checkout, and its display, touch controller, SD card, battery gauge and Lepton pass their self-test
- the Lepton delivers live frames whose temperatures are plausible, and the spot meter (I2C) agrees with the frame (SPI)
- the color frames use only the selected color scheme, and the screen image contains each of the 19 color bars pixel for pixel, as computed from `src/general/colorschemes.cpp`
- every setting the PC can change is applied, reported back and range-checked
- temperature points, the shutter and saving an image to the SD card work. Each run saves one image on the card.
- frame rates and command latency haven't dropped compared with a saved baseline
- with `--manual`: the push button (short and long press) and the touch screen, which need you to press them

Your settings are saved before the run and restored afterwards.

## Run

Python 3.10 or newer. From `firmware/test`:

```sh
pip install -r requirements.txt
pytest                                   # finds the camera's port by itself, or use --port
pytest --baseline baseline-3.0.4.json    # also fail if performance is clearly worse
pytest --manual -s                       # include button and touch tests, follow the prompts
pytest --save-metrics new-baseline.json  # record a new baseline
```

`baseline-3.0.4.json` was recorded with firmware 3.0.4 (Teensy core 1.58.2) on a Lepton 3.5 camera.

When a test leaves the camera out of sync, the run stops with a message. Fix the first failure, wait a minute and run again.
