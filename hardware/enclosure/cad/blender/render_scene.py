"""Blender render pipeline for the DIY-Thermocam V4.0 enclosures (run headless, no GUI needed).

    blender -b --factory-startup -P render_scene.py -- --manifest scene/manifest.json \
            --out ../../renders --work build/render [--shots name,name] [--samples 160] [--scale 1.0]

Reads the per-part GLBs and manifest written by `thermocam_case.scene` and renders:
  - photo shots: Cycles, studio lights, seamless light backdrop (JPEG into --out)
  - build steps: EEVEE + Freestyle line art; parts added in a step are orange, the rest light grey,
    dashed guides with arrows show where they go (raw PNG into --work/steps)
  - part icons:  one line-art drawing per part type on a transparent background (--work/icons)
`steps.json` in --work tells `thermocam_case.render` how to compose the final instruction sheets.
All geometry stays in the CAD camera frame (X right, Y up, Z toward the display), in metres.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

MM = 0.001

# ---------------------------------------------------------------------------------------------
# Shot and step definitions
# ---------------------------------------------------------------------------------------------

SHELL_BUILD = ["shell", "modules", "pcba"]
LASER_BUILD = ["laser", "modules", "pcba"]


@dataclass
class Shot:
    name: str
    groups: list[str]                     # Visible part groups ("pcba" = KiCad board)
    build: str = "3d"                     # "3d" hides laser-only parts, "laser" hides shell-only parts
    style: str = "photo"                  # "photo" | "lineart"
    kind: str = "scene"                   # "scene" | "flatlay" | "icon"
    pose: str = "upright"                 # "upright" (standing on tripod side) | "flat" | "flat_flipped"
    azimuth: float = -35.0                # Degrees around the world up axis, 0 = facing the display
    elevation: float = 14.0
    lens_mm: float = 85.0
    hide: list[str] = field(default_factory=list)            # Part names or groups to hide
    offsets: dict[str, float] = field(default_factory=dict)  # Part/group -> mm along device Z (+ toward display)
    slide_y: dict[str, float] = field(default_factory=dict)  # Part/group -> mm along device Y (+ toward top)
    slide_x: dict[str, float] = field(default_factory=dict)  # Part/group -> mm along device X (+ toward the switch)
    highlight: list[str] = field(default_factory=list)       # Line art: parts/groups drawn in orange
    guides: bool = True                   # Line art: dashed guides from moved parts to their place
    frame: list[str] = field(default_factory=list)           # Fit the camera to these parts only
    section_x: float | None = None        # Cut away everything with device X above this value
    units: list[list[str]] = field(default_factory=list)     # Flat lay: parts placed together; "~" lays it down
    res: tuple[int, int] = (2400, 1600)
    margin: float = 0.08
    shift_x: float = 0.0                  # Move the subject left by this fraction of the width (room for an inset)
    cutout: bool = False                  # Photo on a transparent background (PNG), without the floor


@dataclass
class Step:
    file: str                             # Final sheet name in renders/
    number: int
    shot: Shot
    parts: list[tuple[str, int, str]]     # (icon id, count, label) for the "you need" strip
    inset: Shot | None = None             # Optional close-up placed bottom right


STEP_RES = (1800, 1300)
EXPLODE_3D: dict[str, float] = {
    "screws_back": -16, "lepton_module": 16, "lepton_fasteners": 16, "standoff_rear": 26, "rear_washers": 34,
    "teensy": 38, "pcba": 46, "standoff_mid": 58, "battery_stack": 64, "battery_floor": 68, "lipo": 74,
    "battery_spacers": 74, "battery_lid": 82,
    "display": 94, "front_shell": 118, "screws_front": 138,
}

PHOTOS: list[Shot] = [
    Shot("v4.0-3d-hero-front", SHELL_BUILD, hide=["cap_closed"], azimuth=-32, elevation=12),
    Shot("v4.0-3d-hero-back", SHELL_BUILD, hide=["cap_closed"], azimuth=148, elevation=16),
    Shot("v4.0-3d-hero-back-closed", SHELL_BUILD, hide=["cap_open"], azimuth=212, elevation=16),
    Shot("v4.0-3d-exploded", SHELL_BUILD, hide=["cap_closed"], pose="flat", azimuth=-38, elevation=24,
         lens_mm=70, offsets=EXPLODE_3D, res=(2000, 2400)),
    Shot("v4.0-3d-section", SHELL_BUILD, hide=["cap_closed"], azimuth=62, elevation=14, section_x=44.45,
         lens_mm=70),
    Shot("v4.0-3d-kit", [], kind="flatlay", pose="flat", azimuth=0, elevation=72, lens_mm=70, res=(2400, 1800),
         units=[["front_shell"], ["back_shell"], ["display"], ["pcba"], ["battery_cradle"], ["battery_floor"],
                ["battery_lid"], ["lipo", "lipo_lead"], ["battery_spacer_0"], ["battery_spacer_1"], ["^teensy"], ["^lepton_module"], ["lens_cap_open"],
                ["capture_button"], ["slide_switch_cap"], ["tripod_nut"],
                *[[f"~standoff_{t}_{i}"] for t in ("rear", "mid") for i in range(4)],
                *[[f"standoff_washer_{i}"] for i in range(4)],
                *[[f"~shell_screw_{s}_{i}"] for s in ("back", "front") for i in range(4)],
                *[[f"~lepton_screw_{i}"] for i in range(4)], *[[f"~lepton_spacer_{i}"] for i in range(4)],
                *[[f"lepton_washer_{i}"] for i in range(4)], *[[f"lepton_nut_{i}"] for i in range(4)]]),
    Shot("v4.0-laser-hero", LASER_BUILD, build="laser", azimuth=-32, elevation=12),
    Shot("v4.0-laser-hero-back", LASER_BUILD, build="laser", azimuth=148, elevation=16),
    Shot("v4.0-laser-exploded", LASER_BUILD, build="laser", azimuth=-38, elevation=18, lens_mm=70,
         offsets={"laser_front": 46, "screws_front": 62, "washers_front": 54, "laser_spacers": 22,
                  "laser_back": -44, "screws_back": -60, "washers_back": -52},
         slide_x={"laser_left": -30, "laser_right": 30, "laser_switch_key": 42},
         slide_y={"laser_top": 26, "laser_bottom": -26, "laser_tripod": -40, "laser_capture_key": 38,
                  "laser_chute": 40}),
    Shot("v4.0-laser-sheet", ["laser_sheet"], build="laser", pose="flat", azimuth=-12, elevation=66, lens_mm=70),
    # The LiPo (Adafruit 258) on its own, cut out for the website's parts list
    Shot("v4.0-part-lipo", [], kind="flatlay", pose="flat", azimuth=-30, elevation=38, lens_mm=85, res=(1200, 1200),
         units=[["lipo", "lipo_lead"]], margin=0.06, cutout=True),
    Shot("v4.0-laser-kit", [], build="laser", kind="flatlay", pose="flat", azimuth=0, elevation=72, lens_mm=70,
         res=(2400, 1800),
         units=[["display"], ["pcba"], ["battery_cradle"], ["battery_floor"], ["battery_lid"],
                ["lipo", "lipo_lead"], ["battery_spacer_0"], ["battery_spacer_1"], ["^teensy"], ["^lepton_module"],
                *[[f"sheet_part_{i}"] for i in range(8)],
                ["laser_tripod_mount"], ["laser_tripod_nut"], ["laser_sd_chute"],
                ["laser_display_spacer_bottom"], ["laser_display_spacer_top"],
                *[[f"~standoff_{t}_{i}"] for t in ("rear", "mid") for i in range(4)],
                *[[f"standoff_washer_{i}"] for i in range(4)],
                *[[f"~laser_screw_{s}_{i}"] for s in ("back", "front") for i in range(4)],
                *[[f"laser_washer_{s}_{i}"] for s in ("back", "front") for i in range(4)],
                *[[f"~lepton_screw_{i}"] for i in range(4)], *[[f"~lepton_spacer_{i}"] for i in range(4)],
                *[[f"lepton_washer_{i}"] for i in range(4)], *[[f"lepton_nut_{i}"] for i in range(4)]]),
]

# Icon id -> parts shown in the "you need" strip of a step
ICONS: dict[str, list[str]] = {
    "teensy": ["teensy"], "lepton": ["lepton_module"], "m2x18": ["lepton_screw_0"], "sleeve12": ["lepton_spacer_0"],
    "m2washer": ["lepton_washer_0"], "m2nut": ["lepton_nut_0"], "tripod_nut": ["tripod_nut"],
    "capture_button": ["capture_button"], "switch_slider": ["slide_switch_cap"], "standoff20": ["standoff_rear_0"],
    "csk8": ["shell_screw_back_0"], "washer25": ["standoff_washer_0"], "pcba": ["pcba", "teensy", "lepton"],
    "standoff11": ["standoff_mid_0"], "cradle": ["battery_cradle"], "floor": ["battery_floor"],
    "lipo": ["lipo"], "cell_spacer": ["battery_spacer_0"], "lid": ["battery_lid"], "display": ["display"],
    "front_shell": ["front_shell"], "csk14": ["shell_screw_front_0"], "lens_cap": ["lens_cap_open"],
    "laser_back": ["laser_back"], "laser_front": ["laser_front"], "laser_bottom": ["laser_bottom"],
    "pan8": ["laser_screw_back_0"], "pan14": ["laser_screw_front_0"], "washer": ["laser_washer_back_0"],
    "tripod_mount": ["laser_tripod_mount"], "sd_chute": ["laser_sd_chute"],
    "spacer_bar": ["laser_display_spacer_bottom"], "capture_key": ["laser_capture_key"],
    "switch_key": ["laser_switch_key"], "laser_top": ["laser_top"], "laser_lr": ["laser_left", "laser_right"],
}
# Modules drawn component side up in the "you need" strip
ICON_POSES = {"teensy": "flat_flipped", "lepton": "flat_flipped"}
# Icons drawn from laser-case parts (rendered with the laser build so its printed parts are visible)
LASER_ICONS = {"laser_back", "laser_front", "laser_bottom", "laser_top", "laser_lr", "pan8", "pan14", "washer", "capture_key",
               "switch_key", "tripod_mount", "sd_chute", "spacer_bar"}


def _step(file: str, n: int, groups: list[str], parts: list[tuple[str, int, str]], build: str = "3d",
          inset: Shot | None = None, **kw: object) -> Step:
    kw.setdefault("pose", "flat")
    kw.setdefault("azimuth", -35)
    kw.setdefault("elevation", 34)
    if inset is not None:  # Wider canvas, subject on the left two thirds, close-up on the right
        kw.setdefault("res", (2400, 1300))
        kw.setdefault("shift_x", 0.17)
    kw.setdefault("res", STEP_RES)
    kw.setdefault("margin", 0.03)
    shot = Shot(file.removesuffix(".png"), groups, build=build, style="lineart", **kw)  # type: ignore[arg-type]
    return Step(file, n, shot, parts, inset)


BACK_SHELL_ONLY = ["front_shell", "cap_open", "cap_closed", "battery_stack", "shell_screws"]
SHELL_STEPS: list[Step] = [
    # Carrier before Teensy: a nut driver on the H102/H103 nuts needs the Teensy's place
    _step("assembly-3d-01-lepton-screws.png", 1, ["pcba", "lepton_washers", "lepton_screws"],
          [("m2washer", 4, "Nylon washer M2"), ("m2x18", 4, "M2 × 18")],
          offsets={"lepton_washers": 7, "lepton_screws": 16}, highlight=["lepton_washers", "lepton_screws"],
          elevation=40),
    _step("assembly-3d-02-lepton.png", 2, ["pcba", "lepton"],
          [("sleeve12", 4, "Sleeve 12 mm"), ("lepton", 1, "Lepton + breakout"), ("m2nut", 4, "M2 nut")],
          pose="flat_flipped", offsets={"lepton_spacers": -10, "lepton_module": -22, "lepton_nuts": -32},
          highlight=["lepton_spacers", "lepton_module", "lepton_nuts"], elevation=36),
    _step("assembly-3d-03-teensy.png", 3, ["pcba", "lepton", "teensy"], [("teensy", 1, "Teensy 4.1")],
          pose="flat_flipped", offsets={"teensy": -22}, highlight=["teensy"], elevation=32),
    # Rear standoffs before the loose parts: their screws go in from outside, with the shell face down
    _step("assembly-3d-04-rear-standoffs.png", 4, ["shell", "shell_screws"],
          [("standoff20", 4, "M2.5 × 20"), ("csk8", 4, "M2.5 × 6")],
          hide=["front_shell", "cap_open", "cap_closed", "battery_stack", "screws_front", "standoff_mid", "rear_washers",
                "tripod_nut", "capture_button", "slide_switch_cap"],
          offsets={"standoff_rear": 22, "screws_back": -22}, highlight=["standoff_rear", "screws_back"], elevation=22),
    _step("assembly-3d-05-back-shell-parts.png", 5, ["shell", "shell_screws"],
          [("tripod_nut", 1, '1/4"-20 nut'), ("switch_slider", 1, "Slider")],
          hide=["front_shell", "cap_open", "cap_closed", "battery_stack", "screws_front", "standoff_mid", "rear_washers",
                "capture_button"],
          offsets={"tripod_nut": 18, "slide_switch_cap": 14}, highlight=["tripod_nut", "slide_switch_cap"], elevation=40),
    _step("assembly-3d-06-board.png", 6, SHELL_BUILD,
          [("washer25", 4, "Nylon washer M2.5"), ("pcba", 1, "Board + modules")],
          hide=[*BACK_SHELL_ONLY, "standoff_mid", "display", "lipo", "capture_button"],
          offsets={"rear_washers": 8, "pcba": 24, "teensy": 24, "lepton": 24},
          highlight=["rear_washers", "pcba", "teensy", "lepton"]),
    # The button drops in past the board edge once the board is in (it would fall out before)
    _step("assembly-3d-07-capture-button.png", 7, SHELL_BUILD, [("capture_button", 1, "Button")],
          hide=[*BACK_SHELL_ONLY, "standoff_mid", "display", "lipo"],
          offsets={"capture_button": 14}, highlight=["capture_button"]),
    _step("assembly-3d-08-mid-standoffs.png", 8, SHELL_BUILD, [("standoff11", 4, "M2.5 × 11")],
          hide=[*BACK_SHELL_ONLY, "display", "lipo"],
          offsets={"standoff_mid": 16}, highlight=["standoff_mid"]),
    _step("assembly-3d-09-cradle.png", 9, SHELL_BUILD, [("cradle", 1, "Cradle"), ("floor", 1, "FR4 floor")],
          hide=[*BACK_SHELL_ONLY[:3], "shell_screws", "battery_lid", "display", "lipo", "battery_spacers"],
          offsets={"battery_cradle": 14, "battery_floor": 28}, highlight=["battery_cradle", "battery_floor"]),
    _step("assembly-3d-10-battery.png", 10, SHELL_BUILD,
          [("cell_spacer", 2, "Battery spacer"), ("lipo", 1, "LiPo 1200 mAh"), ("lid", 1, "FR4 lid")],
          hide=[*BACK_SHELL_ONLY[:3], "shell_screws", "display"],
          # Bars and cell shown in place (a lifted cell hides the far bar); the lid floats clear above
          offsets={"battery_lid": 64}, highlight=["battery_spacers", "lipo", "battery_lid"], elevation=50),
    _step("assembly-3d-11-display.png", 11, SHELL_BUILD, [("display", 1, "Display")],
          hide=["front_shell", "cap_open", "cap_closed", "shell_screws"],
          offsets={"display": 20}, highlight=["display"]),
    _step("assembly-3d-12-front-shell.png", 12, [*SHELL_BUILD, "shell_screws"],
          [("front_shell", 1, "Front shell"), ("csk14", 4, "M2.5 × 14")],
          hide=["cap_open", "cap_closed", "screws_back"],
          offsets={"front_shell": 22, "screws_front": 44}, highlight=["front_shell", "screws_front"]),
    _step("assembly-3d-13-lens-cap.png", 13, [*SHELL_BUILD, "shell_screws"], [("lens_cap", 1, "Lens cap")],
          hide=["cap_closed"], pose="upright", azimuth=150, elevation=18,
          slide_y={"cap_open": 58}, highlight=["cap_open"]),
]

LASER_BASE = ["laser_back", "standoff_rear", "screws_back", "washers_back"]
LASER_STACK = [*LASER_BASE, "laser_bottom", "laser_tripod", "rear_washers", "standoff_mid", "battery_stack",
               "modules", "pcba"]
LASER_STEPS: list[Step] = [
    _step("assembly-laser-1-back-plate.png", 1, LASER_BASE,
          [("laser_back", 1, "Back plate"), ("standoff20", 4, "M2.5 × 20"), ("pan8", 4, "M2.5 × 8"),
           ("washer", 4, "Nylon washer M2.5")], build="laser",
          offsets={"standoff_rear": 22, "washers_back": -10, "screws_back": -20},
          highlight=["standoff_rear", "washers_back", "screws_back"], elevation=24),
    _step("assembly-laser-2-tripod.png", 2, [*LASER_BASE, "laser_bottom", "laser_tripod"],
          [("laser_bottom", 1, "Bottom plate"), ("tripod_mount", 1, "Tripod mount"), ("tripod_nut", 1, '1/4"-20 nut')],
          build="laser", offsets={"laser_bottom": 24}, slide_y={"laser_tripod": 16},
          highlight=["laser_bottom", "laser_tripod"], azimuth=200, elevation=38,
          # Close-up from the back: the nut slides into the mount's channel along +Z
          inset=Shot("assembly-laser-2-tripod-inset", ["laser_tripod"], build="laser", style="lineart",
                     pose="flat_flipped", azimuth=20, elevation=40, lens_mm=120, offsets={"laser_tripod_nut": -14},
                     highlight=["laser_tripod"], frame=["laser_tripod"], res=(1000, 780), margin=0.10)),
    _step("assembly-laser-3-stack.png", 3, LASER_STACK, [("pcba", 1, "Shell steps 1–3, 6, 8–11")], build="laser",
          offsets={"rear_washers": 6, "pcba": 16, "teensy": 16, "lepton": 16, "standoff_mid": 28, "battery_stack": 34,
                   "battery_floor": 38, "lipo": 44, "battery_spacers": 44, "battery_lid": 52, "display": 64},
          highlight=["rear_washers", "modules", "pcba", "standoff_mid", "battery_stack"], elevation=28,
          guides=False),
    # Plates drop straight onto the back plate, one at a time: top with the button key in its slot,
    # then left and right with the switch key (corner joints: top/bottom own the back third)
    _step("assembly-laser-4-top.png", 4, [*LASER_STACK, "laser_top", "laser_capture_key"],
          [("capture_key", 1, "Button key"), ("laser_top", 1, "Top plate")], build="laser",
          offsets={"laser_top": 30, "laser_capture_key": 30},
          highlight=["laser_top", "laser_capture_key"], elevation=30),
    _step("assembly-laser-5-sides.png", 5,
          [*LASER_STACK, "laser_top", "laser_capture_key", "laser_left", "laser_right", "laser_switch_key"],
          [("switch_key", 1, "Switch key"), ("laser_lr", 2, "Side plates")], build="laser",
          offsets={"laser_left": 30, "laser_right": 30, "laser_switch_key": 30},
          highlight=["laser_left", "laser_right", "laser_switch_key"], elevation=30),
    _step("assembly-laser-6-front-plate.png", 6, ["laser", "modules", "pcba"],
          [("spacer_bar", 2, "Spacer bar"), ("laser_front", 1, "Front plate"), ("pan14", 4, "M2.5 × 14"),
           ("washer", 4, "Nylon washer M2.5")], build="laser",
          hide=["laser_sheet", "laser_chute"],
          offsets={"laser_spacers": 12, "laser_front": 26, "washers_front": 36, "screws_front": 48},
          highlight=["laser_spacers", "laser_front", "washers_front", "screws_front"]),
    _step("assembly-laser-7-sd-chute.png", 7, ["laser", "modules", "pcba"], [("sd_chute", 1, "microSD chute")],
          build="laser", hide=["laser_sheet"], pose="upright", azimuth=-20, elevation=24,
          slide_y={"laser_chute": 14}, highlight=["laser_chute"]),
]

# ---------------------------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------------------------

BG = (0.93, 0.935, 0.945)
LINE_FILL, LINE_NEW, LINE_GUIDE = (0.94, 0.945, 0.955), (0.98, 0.55, 0.22), (0.16, 0.17, 0.19)

PBR = {
    # name: (base rgb, metallic, roughness, extra)
    "print_shell": ((0.030, 0.032, 0.036), 0.0, 0.62, {"bump": 0.06}),
    "print_accent": ((0.86, 0.22, 0.03), 0.0, 0.5, {"bump": 0.04}),
    "print_inner": ((0.22, 0.23, 0.25), 0.0, 0.6, {"bump": 0.04}),
    # Metals stay fairly rough so they read as silver in the top-down kit shots, not as mirrors of the dark sky
    "nickel": ((0.80, 0.78, 0.74), 1.0, 0.32, {}),
    "zinc": ((0.72, 0.74, 0.78), 1.0, 0.33, {}),
    "steel": ((0.66, 0.66, 0.67), 1.0, 0.30, {}),  # A2 stainless, duller than the nickel standoffs
    # Opaque black cast acrylic; the keys come from the same sheet
    "acrylic": ((0.014, 0.014, 0.016), 0.0, 0.14, {"coat": 0.5}),
    "acrylic_key": ((0.014, 0.014, 0.016), 0.0, 0.14, {"coat": 0.5}),
    "nylon_white": ((0.86, 0.85, 0.80), 0.0, 0.45, {}),
    "pcb_green": ((0.02, 0.20, 0.09), 0.0, 0.35, {"coat": 0.6}),
    "pcb_black": ((0.012, 0.012, 0.014), 0.0, 0.35, {"coat": 0.6}),
    "fr4_black": ((0.020, 0.020, 0.022), 0.0, 0.42, {"coat": 0.4}),  # Bare FR4 floor and lid, black mask
    "pcb_blue": ((0.01, 0.07, 0.30), 0.0, 0.35, {"coat": 0.6}),
    "ic_black": ((0.015, 0.015, 0.017), 0.0, 0.45, {}),
    "plastic_black": ((0.012, 0.012, 0.012), 0.0, 0.5, {}),
    "nylon_black": ((0.02, 0.02, 0.02), 0.0, 0.55, {}),
    "lepton_gold": ((0.80, 0.62, 0.32), 1.0, 0.3, {}),
    "gold": ((0.88, 0.68, 0.30), 1.0, 0.22, {}),
    "lens_germanium": ((0.10, 0.10, 0.11), 1.0, 0.04, {}),
    "backlight_white": ((0.82, 0.83, 0.85), 0.0, 0.5, {}),
    "glass_black": ((0.004, 0.004, 0.005), 0.0, 0.04, {"coat": 1.0}),
    "lipo_foil": ((0.70, 0.72, 0.76), 0.9, 0.38, {}),
    "kapton": ((0.85, 0.45, 0.05), 0.0, 0.3, {"transmission": 0.3}),
    # Module detail (Teensy, Lepton, LiPo lead)
    "solder": ((0.80, 0.80, 0.82), 1.0, 0.28, {}),
    "tin_shell": ((0.76, 0.75, 0.71), 0.85, 0.42, {}),  # Stamped, tin-plated connector shells (micro-USB, microSD)
    "button_cream": ((0.80, 0.66, 0.42), 0.0, 0.45, {}),
    "ceramic_tan": ((0.48, 0.36, 0.22), 0.0, 0.5, {}),
    "inductor_grey": ((0.10, 0.10, 0.11), 0.4, 0.55, {}),
    "lens_ar": ((0.38, 0.30, 0.78), 1.0, 0.07, {}),
    "wire_red": ((0.55, 0.02, 0.02), 0.0, 0.42, {"coat": 0.3}),
    "wire_black": ((0.015, 0.015, 0.015), 0.0, 0.42, {"coat": 0.3}),
    "sd_card": ((0.02, 0.02, 0.022), 0.0, 0.35, {}),
    "led_white": ((0.85, 0.85, 0.80), 0.0, 0.3, {}),
}


def _principled(name: str, rgb: tuple[float, float, float], metallic: float, rough: float,
                extra: dict[str, float]) -> bpy.types.Material:
    m = bpy.data.materials.new(name)
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = rough
    if "transmission" in extra:
        bsdf.inputs["Transmission Weight"].default_value = extra["transmission"]
        bsdf.inputs["IOR"].default_value = extra.get("ior", 1.45)
    if "coat" in extra:
        bsdf.inputs["Coat Weight"].default_value = extra["coat"]
        bsdf.inputs["Coat Roughness"].default_value = 0.08
    if "bump" in extra:  # Fine powder-bed / FDM grain
        tex = nt.nodes.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value = 9000.0
        tex.inputs["Detail"].default_value = 4.0
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = extra["bump"]
        bump.inputs["Distance"].default_value = 0.00002
        nt.links.new(tex.outputs["Fac"], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def _screen_material(image_path: str) -> bpy.types.Material:
    m = bpy.data.materials.new("screen")
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    coord = nt.nodes.new("ShaderNodeTexCoord")
    img = nt.nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(image_path)
    img.extension = "CLIP"
    emit = nt.nodes.new("ShaderNodeEmission")
    emit.inputs["Strength"].default_value = 1.6
    gloss = nt.nodes.new("ShaderNodeBsdfGlossy")
    gloss.inputs["Roughness"].default_value = 0.05
    gloss.inputs["Color"].default_value = (0.04, 0.04, 0.04, 1.0)
    add = nt.nodes.new("ShaderNodeAddShader")
    nt.links.new(coord.outputs["UV"], img.inputs["Vector"])
    nt.links.new(img.outputs["Color"], emit.inputs["Color"])
    nt.links.new(emit.outputs[0], add.inputs[0])
    nt.links.new(gloss.outputs[0], add.inputs[1])
    nt.links.new(add.outputs[0], out.inputs["Surface"])
    return m


def _decal_material(base: bpy.types.Material, image_path: Path) -> bpy.types.Material:
    """Copy of `base` whose colour comes from a planar XY decal (silkscreen, chip marking, label)."""
    m = base.copy()
    m.name = f"{base.name}:{image_path.stem}"
    nt = m.node_tree
    img = nt.nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(str(image_path))
    img.extension = "EXTEND"
    img.interpolation = "Cubic"
    nt.links.new(img.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
    return m


def _flat_material(name: str, rgb: tuple[float, float, float]) -> bpy.types.Material:
    """Unlit fill colour for line-art renders."""
    m = bpy.data.materials.new(name)
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    emit = nt.nodes.new("ShaderNodeEmission")
    emit.inputs["Color"].default_value = (*rgb, 1.0)
    emit.inputs["Strength"].default_value = 1.0
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    return m


# ---------------------------------------------------------------------------------------------
# Scene loading
# ---------------------------------------------------------------------------------------------


@dataclass
class Part:
    name: str
    obj: bpy.types.Object      # Parent empty holding the imported meshes
    material: str
    groups: list[str]          # Own name first, then groups from most to least specific
    meshes: list[bpy.types.Object]
    center_mm: Vector
    original_materials: dict[str, list[bpy.types.Material]] = field(default_factory=dict)


def _import_glb(path: Path, name: str) -> tuple[bpy.types.Object, list[bpy.types.Object]]:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before]
    root = bpy.data.objects.new(name, None)
    bpy.context.scene.collection.objects.link(root)
    meshes = []
    for o in new:
        if o.parent is None:
            o.parent = root
        if o.type == "MESH":
            meshes.append(o)
    return root, meshes


def _planar_uv(meshes: list[bpy.types.Object]) -> None:
    """UV-map the screen in the device frame (u along +X, v along +Y), so the image reads upright."""
    bpy.context.view_layer.update()
    for o in meshes:
        mw = o.matrix_world
        pts = [mw @ v.co for v in o.data.vertices]
        x0, x1 = min(p.x for p in pts), max(p.x for p in pts)
        y0, y1 = min(p.y for p in pts), max(p.y for p in pts)
        uv = o.data.uv_layers.new(name="screen") if not o.data.uv_layers else o.data.uv_layers[0]
        for loop in o.data.loops:
            p = pts[loop.vertex_index]
            uv.data[loop.index].uv = ((p.x - x0) / (x1 - x0), (p.y - y0) / (y1 - y0))


def load_scene(manifest_path: Path) -> tuple[dict[str, Part], dict[str, object]]:
    manifest = json.loads(manifest_path.read_text())
    base = manifest_path.parent
    materials = {k: _principled(k, *v[:3], v[3]) for k, v in PBR.items()}
    materials["screen"] = _screen_material(manifest["screen_image"])
    parts: dict[str, Part] = {}
    for spec in manifest["parts"]:
        root, meshes = _import_glb(base / spec["file"], spec["name"])
        mat = materials[spec["material"]]
        if spec.get("texture"):
            mat = _decal_material(mat, base / spec["texture"])
        for o in meshes:
            o.data.materials.clear()
            o.data.materials.append(mat)
        parts[spec["name"]] = Part(spec["name"], root, spec["material"], [spec["name"], *spec["groups"]], meshes,
                                   Vector(spec["center_mm"]))
        if spec["material"] == "screen" or spec.get("texture"):
            _planar_uv(meshes)
    for board in manifest["boards"]:  # Main board, exported from KiCad
        name = board["name"]
        root, meshes = _import_glb(base / board["file"], name)
        for o in meshes:  # KiCad materials: keep colours, tune finish
            for slot in o.material_slots:
                if slot.material and "Principled BSDF" in slot.material.node_tree.nodes:
                    b = slot.material.node_tree.nodes["Principled BSDF"]
                    b.inputs["Roughness"].default_value = max(0.3, b.inputs["Roughness"].default_value)
        parts[name] = Part(name, root, "kicad", [name, *board["groups"]], meshes, Vector(board["center_mm"]))
        BOARD_BASES[name] = Matrix.Translation(Vector(board["offset_mm"]) * MM)
    for p in parts.values():
        p.original_materials = {o.name: [s.material for s in o.material_slots] for o in p.meshes}
    return parts, manifest


# ---------------------------------------------------------------------------------------------
# Studio
# ---------------------------------------------------------------------------------------------

POSES = {
    # device frame -> world: upright stands the camera on its tripod side, display facing -Y
    "upright": Euler((math.radians(90), 0, 0)).to_matrix().to_4x4(),
    "flat": Matrix.Identity(4),
    "flat_flipped": Euler((0, math.radians(180), 0)).to_matrix().to_4x4(),
}
BOARD_BASES: dict[str, Matrix] = {}  # KiCad board -> lift of its F.Cu face to the device frame


def setup_world(style: str, transparent: bool) -> None:
    scene = bpy.context.scene
    world = bpy.data.worlds.get("studio") or bpy.data.worlds.new("studio")
    scene.world = world
    nt = world.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputWorld")
    if style == "lineart":
        bg = nt.nodes.new("ShaderNodeBackground")
        bg.inputs["Color"].default_value = (1, 1, 1, 1)
        bg.inputs["Strength"].default_value = 1.0
        nt.links.new(bg.outputs[0], out.inputs[0])
        return
    # Reflections from Blender's bundled studio HDRI; camera sees a flat light backdrop
    env = nt.nodes.new("ShaderNodeTexEnvironment")
    env.image = bpy.data.images.load(os.path.join(bpy.utils.system_resource("DATAFILES", path="studiolights/world"), "studio.exr"))
    hdr = nt.nodes.new("ShaderNodeBackground")
    hdr.inputs["Strength"].default_value = 0.55
    flat = nt.nodes.new("ShaderNodeBackground")
    flat.inputs["Color"].default_value = (*BG, 1)
    flat.inputs["Strength"].default_value = 1.45
    lp = nt.nodes.new("ShaderNodeLightPath")
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(env.outputs["Color"], hdr.inputs["Color"])
    nt.links.new(lp.outputs["Is Camera Ray"], mix.inputs["Fac"])
    nt.links.new(hdr.outputs[0], mix.inputs[1])
    nt.links.new(flat.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs[0])


def _add_area_light(name, loc, target, size, power, color=(1, 1, 1)):
    light = bpy.data.lights.new(name, "AREA")
    light.shape = "DISK"
    light.size = size
    light.energy = power
    light.color = color
    obj = bpy.data.objects.new(name, light)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = loc
    direction = Vector(target) - Vector(loc)
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    return obj


def setup_studio(center: Vector, radius: float, floor_z: float) -> None:
    r = max(radius, 0.05)
    # (direction, size factor, irradiance at the subject in W/m^2, colour); power = E * pi * d^2
    rig = (
        ("key", Vector((-1.6, -2.0, 2.2)), 1.6, 2.6, (1.0, 0.98, 0.96)),
        ("fill", Vector((2.4, -1.2, 0.9)), 2.4, 0.8, (0.92, 0.95, 1.0)),
        ("rim", Vector((0.6, 2.6, 1.8)), 1.2, 2.2, (1.0, 0.96, 0.9)),
    )
    for name, direction, size, irradiance, color in rig:
        loc = center + direction.normalized() * r * 6.0
        d = (loc - center).length
        _add_area_light(name, loc, center, size * r * 2, irradiance * math.pi * d * d, color)
    floor = bpy.data.meshes.new("floor")
    size = 50 * r
    floor.from_pydata([(-size, -size, 0), (size, -size, 0), (size, size, 0), (-size, size, 0)], [], [(0, 1, 2, 3)])
    fo = bpy.data.objects.new("floor", floor)
    bpy.context.scene.collection.objects.link(fo)
    fo.location = (center.x, center.y, floor_z)
    mat = bpy.data.materials.new("floor")
    b = mat.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*BG, 1)
    b.inputs["Roughness"].default_value = 0.9
    floor.materials.append(mat)


# ---------------------------------------------------------------------------------------------
# Arranging parts
# ---------------------------------------------------------------------------------------------


def _matches(part: Part, keys) -> bool:
    return any(k in part.groups for k in keys)


def _first_match(part: Part, table: dict[str, float]) -> float:
    for g in part.groups:  # Own name first, then groups from most to least specific
        if g in table:
            return table[g]
    return 0.0


def _displacement(p: Part, shot: Shot) -> Vector:
    return Vector((_first_match(p, shot.slide_x), _first_match(p, shot.slide_y), _first_match(p, shot.offsets)))


def _belongs_to_build(p: Part, build: str) -> bool:
    if build == "laser" and "shell" in p.groups and "laser" not in p.groups:
        return False
    if build == "3d" and "laser" in p.groups and "shell" not in p.groups:
        return False
    return True


def _set_visible(p: Part, show: bool) -> None:
    for o in [p.obj, *p.meshes, *p.obj.children_recursive]:
        o.hide_render = not show


def _base(p: Part) -> Matrix:
    return BOARD_BASES.get(p.name, Matrix.Identity(4))


def arrange(parts: dict[str, Part], shot: Shot) -> list[Part]:
    pose = POSES[shot.pose]
    visible = []
    for p in parts.values():
        show = _matches(p, shot.groups) and not _matches(p, shot.hide) and _belongs_to_build(p, shot.build)
        _set_visible(p, show)
        if show:
            p.obj.matrix_world = pose @ Matrix.Translation(_displacement(p, shot) * MM) @ _base(p)
            visible.append(p)
    bpy.context.view_layer.update()
    return visible


def arrange_units(parts: dict[str, Part], shot: Shot) -> list[Part]:
    """Flat lay: place each unit flat on the floor and pack the units into neat rows (knolling)."""
    for p in parts.values():
        _set_visible(p, False)
    pose = POSES[shot.pose]
    placed: list[tuple[list[Part], Matrix]] = []
    for unit in shot.units:
        # "~" lays screws and standoffs on their side so the top view shows their profile;
        # "^" turns a module over so its component side faces up (Teensy, Lepton)
        turn = {"~": 90, "^": 180}.get(unit[0][0], 0)
        keys = [k.lstrip("~^") for k in unit]
        members = [p for p in parts.values() if _matches(p, keys) and _belongs_to_build(p, shot.build)]
        if not members:
            continue
        m = pose.copy()
        for p in members:
            _set_visible(p, True)
            p.obj.matrix_world = m @ _base(p)
        bpy.context.view_layer.update()
        if turn:
            lo, hi = _world_bbox(members)
            c = (lo + hi) / 2
            m = Matrix.Translation(c) @ Euler((math.radians(turn), 0, 0)).to_matrix().to_4x4() @ Matrix.Translation(-c) @ m
            for p in members:
                p.obj.matrix_world = m @ _base(p)
            bpy.context.view_layer.update()
        placed.append((members, m))
    gap = 7 * MM
    boxes = [_world_bbox(members) for members, _ in placed]
    area = sum((hi.x - lo.x + gap) * (hi.y - lo.y + gap) for lo, hi in boxes)
    target_w = math.sqrt(area * shot.res[0] / shot.res[1]) * 1.05
    x = y = row_h = 0.0
    for (members, m), (lo, hi) in zip(placed, boxes):
        w, h = hi.x - lo.x, hi.y - lo.y
        if x > 0 and x + w > target_w:
            x, y, row_h = 0.0, y - row_h - gap, 0.0
        t = Matrix.Translation(Vector((x - lo.x, y - hi.y, -lo.z)))
        for p in members:
            p.obj.matrix_world = t @ m @ _base(p)
        x += w + gap
        row_h = max(row_h, h)
    bpy.context.view_layer.update()
    return [p for members, _ in placed for p in members]


def _world_bbox(parts: list[Part]) -> tuple[Vector, Vector]:
    lo, hi = Vector((1e9,) * 3), Vector((-1e9,) * 3)
    for p in parts:
        for o in p.meshes:
            if o.hide_render:
                continue
            for c in o.bound_box:
                w = o.matrix_world @ Vector(c)
                lo = Vector(map(min, lo, w))
                hi = Vector(map(max, hi, w))
    return lo, hi


# ---------------------------------------------------------------------------------------------
# Build-step guides and cutaway
# ---------------------------------------------------------------------------------------------


def build_guides(visible: list[Part], shot: Shot, anchors: dict[str, list[list[float]]], mat) -> None:
    """Dashed lines with an arrowhead from each moved, highlighted part to where it belongs."""
    pose = POSES[shot.pose]
    segments: list[tuple[Vector, Vector]] = []
    seen: set[str] = set()
    for p in visible:
        if not _matches(p, shot.highlight):
            continue
        d = _displacement(p, shot)
        if d.length < 4:
            continue
        key = next((g for g in p.groups if g in anchors), None)
        if key is not None:
            if key in seen:
                continue
            seen.add(key)
            points = [Vector(a) for a in anchors[key]]
        else:
            points = [p.center_mm]
        segments += [(pt + d, pt) for pt in points]
    if not segments:
        return
    bm = bmesh.new()
    r, dash, gap = 0.22, 1.8, 1.3
    for start_mm, end_mm in segments:
        a, b = pose @ (start_mm * MM), pose @ (end_mm * MM)
        axis = b - a
        length = axis.length
        rot = axis.normalized().to_track_quat("Z", "Y").to_matrix().to_4x4()
        s = 0.0
        while s < length / MM - dash:
            center = a + axis.normalized() * (s + dash / 2) * MM
            bmesh.ops.create_cone(bm, cap_ends=True, segments=8, radius1=r * MM, radius2=r * MM, depth=dash * MM,
                                  matrix=Matrix.Translation(center) @ rot)
            s += dash + gap
        tip = a + axis * 0.5  # Arrowhead halfway, pointing toward the destination
        bmesh.ops.create_cone(bm, cap_ends=True, segments=12, radius1=1.1 * MM, radius2=0.0, depth=3.2 * MM,
                              matrix=Matrix.Translation(tip) @ rot)
    mesh = bpy.data.meshes.new("guides")
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(mat)
    obj = bpy.data.objects.new("guides", mesh)
    bpy.context.scene.collection.objects.link(obj)


def apply_section(visible: list[Part], shot: Shot) -> list[tuple[bpy.types.Object, bpy.types.Mesh]]:
    """Cut every visible mesh at device X = section_x and cap the cut faces; returns originals to restore."""
    pose = POSES[shot.pose]
    co_w = pose @ (Vector((shot.section_x, 0, 0)) * MM)
    no_w = (pose.to_3x3() @ Vector((1, 0, 0))).normalized()
    restore = []
    for p in visible:
        for o in p.meshes:
            if o.hide_render:
                continue
            inv = o.matrix_world.inverted()
            co_l = inv @ co_w
            no_l = (inv.to_3x3() @ no_w).normalized()
            original = o.data
            mesh = original.copy()
            bm = bmesh.new()
            bm.from_mesh(mesh)
            geom = [*bm.verts, *bm.edges, *bm.faces]
            res = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co_l, plane_no=no_l, clear_outer=True)
            cut = [g for g in res["geom_cut"] if isinstance(g, bmesh.types.BMEdge)]
            if cut:
                bmesh.ops.holes_fill(bm, edges=cut, sides=0)
            bm.to_mesh(mesh)
            bm.free()
            o.data = mesh
            restore.append((o, original))
    return restore


# ---------------------------------------------------------------------------------------------
# Camera and render settings
# ---------------------------------------------------------------------------------------------


def place_camera(shot: Shot, lo: Vector, hi: Vector) -> bpy.types.Object:
    scene = bpy.context.scene
    cam_data = bpy.data.cameras.new("cam")
    cam_data.lens = shot.lens_mm
    cam_data.sensor_fit = "AUTO"
    cam_data.clip_start = 0.001
    cam_data.clip_end = 50
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    center = (lo + hi) / 2
    az, el = math.radians(shot.azimuth), math.radians(min(shot.elevation, 89.95))
    # azimuth 0 looks at the device front: upright pose -> from -Y; flat poses -> from +Z with a tilt
    view_dir = Vector((math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el)))
    rot = (-view_dir).to_track_quat("-Z", "Y")
    cam.rotation_euler = rot.to_euler()
    w, h = shot.res
    aspect = w / h
    sensor = cam_data.sensor_width
    fov_x = 2 * math.atan(sensor / 2 / cam_data.lens) if aspect >= 1 else 2 * math.atan(sensor * aspect / 2 / cam_data.lens)
    tan_x = math.tan(fov_x / 2) * (1 - shot.margin) * (1 - 2 * abs(shot.shift_x))
    cam_data.shift_x = shot.shift_x
    tan_y = tan_x / aspect
    rmat = rot.to_matrix()
    right, up, fwd = rmat.col[0], rmat.col[1], -rmat.col[2]
    corners = [Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
    xs = [(c - center).dot(right) for c in corners]
    ys = [(c - center).dot(up) for c in corners]
    center = center + right * (max(xs) + min(xs)) / 2 + up * (max(ys) + min(ys)) / 2
    d = 0.0
    for c in corners:
        v = c - center
        depth = v.dot(fwd)
        d = max(d, depth + abs(v.dot(right)) / tan_x, depth + abs(v.dot(up)) / tan_y)
    cam.location = center - fwd * d
    return cam


def configure_render(shot: Shot, samples: int, scale: float) -> None:
    scene = bpy.context.scene
    r = scene.render
    r.resolution_x, r.resolution_y = shot.res
    r.resolution_percentage = int(100 * scale)
    transparent = shot.kind == "icon" or shot.cutout
    r.film_transparent = transparent
    if shot.style == "photo" and not shot.cutout:  # Photos as high-quality JPEG keep the repo small; line art stays PNG
        r.image_settings.file_format = "JPEG"
        r.image_settings.quality = 92
        r.image_settings.color_mode = "RGB"
    else:
        r.image_settings.file_format = "PNG"
        r.image_settings.compression = 90
        r.image_settings.color_mode = "RGBA" if transparent else "RGB"
    vs = scene.view_settings
    if shot.style == "photo":
        r.engine = "CYCLES"
        prefs = bpy.context.preferences.addons["cycles"].preferences
        try:
            prefs.compute_device_type = "METAL"
            prefs.get_devices()
            for dev in prefs.devices:
                dev.use = dev.type == "METAL"
            scene.cycles.device = "GPU"
        except Exception:  # CI / Linux without Metal
            scene.cycles.device = "CPU"
        scene.cycles.samples = samples
        scene.cycles.use_denoising = True
        scene.cycles.max_bounces = 12
        scene.cycles.transmission_bounces = 12
        r.use_freestyle = False
        for vt in ("AgX", "Filmic", "Standard"):
            try:
                vs.view_transform = vt
                break
            except TypeError:
                continue
        try:
            vs.look = "AgX - Medium High Contrast"
        except TypeError:
            pass
    else:
        r.engine = "BLENDER_EEVEE"
        vs.view_transform = "Standard"
        r.use_freestyle = True
        r.line_thickness_mode = "ABSOLUTE"
        r.line_thickness = max(1.0, 1.6 * scale * (shot.res[0] / 1800) ** 0.5)
        fs = bpy.context.view_layer.freestyle_settings
        fs.crease_angle = math.radians(134)
        ls = fs.linesets[0] if fs.linesets else fs.linesets.new("lines")
        ls.select_by_visibility = True
        ls.visibility = "VISIBLE"
        ls.select_silhouette = ls.select_border = ls.select_crease = True
        ls.select_external_contour = True
        if ls.linestyle is None:
            ls.linestyle = bpy.data.linestyles.new("ink")
        ls.linestyle.color = (0.08, 0.09, 0.11)
        ls.linestyle.thickness = 1.0


def apply_style(visible: list[Part], shot: Shot, flat: dict[str, bpy.types.Material]) -> None:
    for p in visible:
        for o in p.meshes:
            orig = p.original_materials.get(o.name, [])
            if shot.style == "photo":
                for i, m in enumerate(orig):
                    o.material_slots[i].material = m
            else:
                highlighted = shot.kind == "icon" or _matches(p, shot.highlight)
                mat = flat["new"] if highlighted else flat["old"]
                for slot in o.material_slots:
                    slot.material = mat


def render_shot(parts: dict[str, Part], manifest: dict[str, object], shot: Shot, out: Path, samples: int,
                scale: float, flat: dict[str, bpy.types.Material]) -> Path:
    for o in list(bpy.data.objects):  # Clear previous camera / lights / floor / guides
        if o.type in {"CAMERA", "LIGHT"} or o.name.startswith(("floor", "guides")):
            bpy.data.objects.remove(o, do_unlink=True)
    visible = arrange_units(parts, shot) if shot.kind == "flatlay" else arrange(parts, shot)
    apply_style(visible, shot, flat)
    restore = apply_section(visible, shot) if shot.section_x is not None else []
    lo, hi = _world_bbox(visible)
    framed = [p for p in visible if _matches(p, shot.frame)] if shot.frame else visible
    flo, fhi = _world_bbox(framed)
    setup_world(shot.style, shot.kind == "icon")
    if shot.style == "photo":
        setup_studio((lo + hi) / 2, (hi - lo).length / 2, lo.z)
        if shot.cutout:  # Product shot on its own: no floor, no cast shadow
            for o in bpy.data.objects:
                if o.name.startswith("floor"):
                    o.hide_render = True
    if shot.style == "lineart" and shot.guides and shot.kind == "scene":
        build_guides(visible, shot, manifest["anchors_mm"], flat["guide"])  # type: ignore[arg-type]
    place_camera(shot, flo, fhi)
    configure_render(shot, samples, scale)
    out.parent.mkdir(parents=True, exist_ok=True)
    bpy.context.scene.render.filepath = str(out)
    bpy.ops.render.render(write_still=True)
    for o, original in restore:
        tmp = o.data
        o.data = original
        bpy.data.meshes.remove(tmp)
    print(f"[render] {out}")
    return out


# ---------------------------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------------------------


def main() -> None:
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path, help="Final photo renders")
    ap.add_argument("--work", required=True, type=Path, help="Raw step renders, icons and steps.json")
    ap.add_argument("--shots", default="", help="Comma-separated photo or step names (default: all)")
    ap.add_argument("--samples", type=int, default=128)
    ap.add_argument("--scale", type=float, default=1.0)
    ap.add_argument("--save-blend", type=Path, default=None)
    args = ap.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    parts, manifest = load_scene(args.manifest)
    flat = {"old": _flat_material("line_old", LINE_FILL), "new": _flat_material("line_new", LINE_NEW),
            "guide": _flat_material("line_guide", LINE_GUIDE)}
    wanted = {s.strip() for s in args.shots.split(",") if s.strip()}

    def want(name: str) -> bool:
        return not wanted or name in wanted or name.removesuffix(".png") in wanted

    for shot in PHOTOS:
        if want(shot.name):
            ext = "png" if shot.cutout else "jpg"
            render_shot(parts, manifest, shot, args.out / f"{shot.name}.{ext}", args.samples, args.scale, flat)

    sheets = []
    icons_needed: set[str] = set()
    for guide, steps in (("3d", SHELL_STEPS), ("laser", LASER_STEPS)):
        for step in steps:
            if not want(step.file):
                continue
            raw = render_shot(parts, manifest, step.shot, args.work / "steps" / step.file, args.samples, args.scale, flat)
            inset = None
            if step.inset is not None:
                inset = render_shot(parts, manifest, step.inset, args.work / "steps" / f"{step.inset.name}.png",
                                    args.samples, args.scale, flat)
            icons_needed |= {icon for icon, _, _ in step.parts}
            sheets.append({
                "guide": guide, "file": step.file, "number": step.number, "raw": str(raw),
                "inset": str(inset) if inset else None,
                "parts": [{"icon": str(args.work / "icons" / f"{i}.png"), "count": c, "label": lbl}
                          for i, c, lbl in step.parts],
            })
    for icon in sorted(icons_needed):
        shot = Shot(f"icon-{icon}", ICONS[icon], build="laser" if icon in LASER_ICONS else "3d",
                    style="lineart", kind="icon", pose=ICON_POSES.get(icon, "flat"), azimuth=-35, elevation=42,
                    lens_mm=100,
                    res=(420, 420), margin=0.06, guides=False)
        render_shot(parts, manifest, shot, args.work / "icons" / f"{icon}.png", args.samples, args.scale, flat)
    if sheets:
        (args.work / "steps.json").write_text(json.dumps(sheets, indent=1))
    if args.save_blend:
        bpy.ops.wm.save_as_mainfile(filepath=str(args.save_blend))


if __name__ == "__main__":
    main()
