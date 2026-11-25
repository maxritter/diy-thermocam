#!/usr/bin/env python3
"""Generate DIY-Thermocam V4.0 (Zero-Solder PCBA) native KiCad 10 project.

Run with KiCad's bundled Python 3 so `pcbnew` is available:
  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
      hardware/pcb/scripts/build_v4_board.py

Creates under hardware/pcb/kicad-v4.0/:
  - diy-thermocam-v4.0.kicad_pro
  - diy-thermocam-v4.0.kicad_sch  (0 ERC errors, 0 warnings)
  - diy-thermocam-v4.0.kicad_pcb  (0 DRC errors, 0 unconnected items, GND planes F.Cu/B.Cu)
  - V40.kicad_sym, V40.pretty/, sym-lib-table, fp-lib-table

Board envelope & module socket positions match V3.0 (89.4 x 68.4 mm, 2-layer 1.6 mm FR4)
so V4.0 drops directly into the parametric enclosure while eliminating ALL hand soldering.
"""

from __future__ import annotations

import heapq
import json
import math
import os
import re
import shutil
import subprocess
import sys
import uuid
from collections import defaultdict
from pathlib import Path

KICAD_CLI = os.environ.get("KICAD_CLI", "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli")
KICAD_FP_ROOT = Path("/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints")
PCB_ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = PCB_ROOT / "kicad-v4.0"
FP_DIR = OUT_DIR / "V40.pretty"
STEM = "diy-thermocam-v4.0"

BOARD_W = 89.4
BOARD_H = 68.4  # KiCad Y runs -68.4 (top) .. 0.0 (bottom); KiCad_Y = -Eagle_Y


def uid(seed: str) -> str:
    return str(uuid.uuid5(uuid.NAMESPACE_DNS, f"diy-thermocam-v4.0:{seed}"))


# ---------------------------------------------------------------------------
# 1. Footprint Library Preparation (self-contained V40.pretty w/ real 3D STEP)
# ---------------------------------------------------------------------------
STD_FOOTPRINTS = {
    "USB_C_TYPE_C_31_M_12": "Connector_USB.pretty/USB_C_Receptacle_HRO_TYPE-C-31-M-12.kicad_mod",
    "BQ24075_VQFN16": "Package_DFN_QFN.pretty/QFN-16-1EP_3x3mm_P0.5mm_EP1.7x1.7mm_ThermalVias.kicad_mod",
    "TPS61022_VQFN7": "Package_DFN_QFN.pretty/Texas_RWU0007A_VQFN-7_2x2mm_P0.5mm.kicad_mod",
    "SOT23_6": "Package_TO_SOT_SMD.pretty/SOT-23-6.kicad_mod",
    "JST_PH_2P_RA": "Connector_JST.pretty/JST_PH_S2B-PH-SM4-TB_1x02-1MP_P2.00mm_Horizontal.kicad_mod",
    "L_0630": "Inductor_SMD.pretty/L_Changjiang_FXL0630.kicad_mod",
    "SW_SLIDE_RA": "Button_Switch_SMD.pretty/SW_SPDT_CK_JS102011SAQN.kicad_mod",
    "SW_TACT_RA": "Button_Switch_SMD.pretty/SW_SPST_B3U-3000P.kicad_mod",
    "SW_TACT_PROG": "Button_Switch_SMD.pretty/SW_Push_1P1T_XKB_TS-1187A.kicad_mod",
    "R_0603": "Resistor_SMD.pretty/R_0603_1608Metric.kicad_mod",
    "C_0603": "Capacitor_SMD.pretty/C_0603_1608Metric.kicad_mod",
    "C_0805": "Capacitor_SMD.pretty/C_0805_2012Metric.kicad_mod",
    "LED_0603": "LED_SMD.pretty/LED_0603_1608Metric.kicad_mod",
    "PinSocket_1x24": "Connector_PinSocket_2.54mm.pretty/PinSocket_1x24_P2.54mm_Vertical.kicad_mod",
    "PinSocket_1x05": "Connector_PinSocket_2.54mm.pretty/PinSocket_1x05_P2.54mm_Vertical.kicad_mod",
    "PinSocket_2x10": "Connector_PinSocket_2.54mm.pretty/PinSocket_2x10_P2.54mm_Vertical.kicad_mod",
    "SOT23": "Package_TO_SOT_SMD.pretty/SOT-23.kicad_mod",
    "FUSE_1206": "Fuse.pretty/Fuse_1206_3216Metric.kicad_mod",
    "I2C_PADS_1x04": "Connector_PinHeader_2.54mm.pretty/PinHeader_1x04_P2.54mm_Vertical.kicad_mod",
    "SJ_2P_OPEN": "Jumper.pretty/SolderJumper-2_P1.3mm_Open_Pad1.0x1.5mm.kicad_mod",
    "SJ_3P_BRIDGED12": "Jumper.pretty/SolderJumper-3_P1.3mm_Bridged12_Pad1.0x1.5mm.kicad_mod",
}


def build_local_footprints() -> None:
    if FP_DIR.exists():
        shutil.rmtree(FP_DIR)
    FP_DIR.mkdir(parents=True, exist_ok=True)

    for local_name, rel in STD_FOOTPRINTS.items():
        src = KICAD_FP_ROOT / rel
        txt = src.read_text(encoding="utf-8")
        # Rename top-level footprint identifier
        txt = re.sub(r'^\(footprint "[^"]+"', f'(footprint "{local_name}"', txt, count=1)
        if local_name == "I2C_PADS_1x04":
            # DNP solder pads only — strip 3D male pin header model and mark exclude_from_bom
            txt = re.sub(r'\n\t\(model\b.*?\n\t\)', '', txt, flags=re.S)
            txt = txt.replace('(attr through_hole)', '(attr through_hole exclude_from_bom exclude_from_pos_files)')
        if local_name == "SJ_2P_OPEN":
            txt = txt.replace('(attr smd)', '(attr smd exclude_from_bom exclude_from_pos_files)')
        if local_name == "BQ24075_VQFN16":
            # Upgrade thermal vias inside pad 17 from 0.20mm to 0.30mm drill (0.55mm pad) for 2-layer JLCPCB spec
            txt = re.sub(
                r'(\(pad "17" thru_hole circle\s+\(at [^\)]+\)\s+)\(size 0\.5 0\.5\)\s+\(drill 0\.2\)',
                r'\1(size 0.55 0.55)\n\t\t(drill 0.3)',
                txt,
            )
        (FP_DIR / f"{local_name}.kicad_mod").write_text(txt, encoding="utf-8")

    # Custom V3.0-exact 2x20 Display female socket on F.Cu (exact BuyDisplay ER-TFTM032-3 pin map)
    # Matches V3.0 LCD-HEADER0 1:1: ODD pins at X=1.33 (local -1.27), EVEN pins at X=3.87 (local +1.27)
    # Pin 1 at (-1.27, +24.13) -> world (1.33, -10.32), Pin 2 at (+1.27, +24.13) -> world (3.87, -10.32)
    # Pin 39 at (-1.27, -24.13) -> world (1.33, -58.58), Pin 40 at (+1.27, -24.13) -> world (3.87, -58.58)
    # when placed at (2.60, -34.45) on F.Cu rot=0.
    pads_2x20 = []
    for row in range(20):
        py = round(24.13 - row * 2.54, 4)
        p_odd = 2 * row + 1
        p_even = 2 * row + 2
        shape_odd = "rect" if p_odd == 1 else "circle"
        pads_2x20.append(
            f'\t(pad "{p_odd}" thru_hole {shape_odd} (at -1.27 {py}) (size 1.55 1.55) (drill 1.0) (layers "*.Cu" "*.Mask"))'
        )
        pads_2x20.append(
            f'\t(pad "{p_even}" thru_hole circle (at 1.27 {py}) (size 1.55 1.55) (drill 1.0) (layers "*.Cu" "*.Mask"))'
        )
    mod_2x20 = f"""(footprint "PinSocket_2x20_Display"
\t(version 20241229)
\t(generator "pcbnew")
\t(layer "F.Cu")
\t(attr through_hole)
\t(fp_rect (start -2.54 -25.4) (end 2.54 25.4) (stroke (width 0.12) (type solid)) (fill none) (layer "F.SilkS"))
\t(fp_rect (start -2.54 -25.4) (end 2.54 25.4) (stroke (width 0.1) (type solid)) (fill none) (layer "F.Fab"))
{chr(10).join(pads_2x20)}
\t(model "${{KICAD10_3DMODEL_DIR}}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_2x20_P2.54mm_Vertical.step"
		(offset (xyz 1.27 24.13 0))
		(scale (xyz 1 1 1))
		(rotate (xyz 0 0 0))
\t)
)
"""
    (FP_DIR / "PinSocket_2x20_Display.kicad_mod").write_text(mod_2x20, encoding="utf-8")

    # Custom V3.0-exact 2x10 Lepton female socket on B.Cu:
    # Placed at (31.40, -21.00) on B.Cu rot=0:
    # In local coords (note B.Cu flip negates local Y):
    # Pin 1 = (0, 0) -> world (31.40, -21.00), Pin 2 = (0, +2.54) -> flipped Y => world (31.40, -23.54)
    # Pin 19 = (22.86, 0) -> world (54.26, -21.00), Pin 20 = (22.86, +2.54) -> world (54.26, -23.54)
    pads_2x10 = []
    for col in range(10):
        px = round(col * 2.54, 4)
        p_odd = 2 * col + 1
        p_even = 2 * col + 2
        shape_odd = "rect" if p_odd == 1 else "circle"
        pads_2x10.append(
            f'\t(pad "{p_odd}" thru_hole {shape_odd} (at {px} 0) (size 1.7 1.7) (drill 1.0) (layers "*.Cu" "*.Mask"))'
        )
        pads_2x10.append(
            f'\t(pad "{p_even}" thru_hole circle (at {px} 2.54) (size 1.7 1.7) (drill 1.0) (layers "*.Cu" "*.Mask"))'
        )
    mod_2x10 = f"""(footprint "PinSocket_2x10_Lepton"
\t(version 20241229)
\t(generator "pcbnew")
\t(layer "F.Cu")
\t(attr through_hole)
\t(fp_rect (start -1.3 -1.3) (end 24.16 3.84) (stroke (width 0.12) (type solid)) (fill none) (layer "F.SilkS"))
\t(fp_rect (start -1.3 -1.3) (end 24.16 3.84) (stroke (width 0.1) (type solid)) (fill none) (layer "F.Fab"))
{chr(10).join(pads_2x10)}
\t(model "${{KICAD10_3DMODEL_DIR}}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_2x10_P2.54mm_Vertical.step"
\t\t(offset (xyz 0 0 0))
\t\t(scale (xyz 1 1 1))
\t\t(rotate (xyz 0 0 -90))
\t)
)
"""
    (FP_DIR / "PinSocket_2x10_Lepton.kicad_mod").write_text(mod_2x10, encoding="utf-8")

    # Zero-solder SMD Spring Pogo Pin footprint (YTC1P-8535-01: D=2.0mm base, 12.0mm free / 11.0mm working height)
    mod_pogo = """(footprint "PogoPin_SMD_2.0mm"
\t(version 20241229)
\t(generator "pcbnew")
\t(layer "F.Cu")
\t(attr smd)
\t(fp_circle (center 0 0) (end 1.2 0) (stroke (width 0.12) (type solid)) (fill none) (layer "F.SilkS"))
\t(pad "1" smd circle (at 0 0) (size 2.25 2.25) (layers "F.Cu" "F.Paste" "F.Mask"))
)
"""
    (FP_DIR / "PogoPin_SMD_2.0mm.kicad_mod").write_text(mod_pogo, encoding="utf-8")

    # Mounting holes: M2.5 (2.8 mm PTH tied to GND) and M2 Lepton (2.1 mm NPTH)
    mod_m25 = """(footprint "MountingHole_2.8mm_M2.5_Pad"
\t(version 20241229)
\t(generator "pcbnew")
\t(layer "F.Cu")
\t(attr exclude_from_pos_files exclude_from_bom)
\t(fp_circle (center 0 0) (end 2.75 0) (stroke (width 0.12) (type solid)) (fill none) (layer "F.CrtYd"))
\t(pad "1" thru_hole circle (at 0 0) (size 4.8 4.8) (drill 2.8) (layers "*.Cu" "*.Mask"))
)
"""
    (FP_DIR / "MountingHole_2.8mm_M2.5_Pad.kicad_mod").write_text(mod_m25, encoding="utf-8")

    mod_m2 = """(footprint "MountingHole_2.1mm_M2"
\t(version 20241229)
\t(generator "pcbnew")
\t(layer "F.Cu")
\t(attr exclude_from_pos_files exclude_from_bom)
\t(fp_circle (center 0 0) (end 2.0 0) (stroke (width 0.12) (type solid)) (fill none) (layer "F.CrtYd"))
\t(pad "" np_thru_hole circle (at 0 0) (size 2.1 2.1) (drill 2.1) (layers "*.Cu" "*.Mask"))
)
"""
    (FP_DIR / "MountingHole_2.1mm_M2.kicad_mod").write_text(mod_m2, encoding="utf-8")


# ---------------------------------------------------------------------------
# 2. Complete V4.0 Component & Net Specification
# ---------------------------------------------------------------------------
# All coordinates in KiCad frame: X = 0..89.4 (right), Y = -68.4..0 (down, KiCad_Y = -Eagle_Y)
# Single-sided SMD PCBA on B.Cu (Lepton side); Display socket J1 on F.Cu (Display side).

# Teensy 4.1 Left 1x24 socket (J5 at X=59.28, matches V3.0 U$2 1:1 — GND1 side)
J5_NETS = {
    "1": "GND",       # GND1
    "4": "D2",        # 2-OUT2 (Capture button)
    "5": "D3",        # 3-LRCLK2 (Lepton VSYNC)
    "6": "D4",        # 4-BCLK2 (Optional FT6206 CTP_INT via open jumper JP3)
    "7": "D5",        # 5-IN2 (LCD Touch IRQ)
    "11": "D9",       # 9-OUT1C (LCD Touch CS)
    "12": "D10",      # 10-CS (LCD DC)
    "13": "D11",      # 11-MOSI (SPI0 MOSI)
    "14": "D12",      # 12-MISO (SPI0 MISO)
    "15": "3V3",      # 3V3-1
    "18": "D26",      # 26-A12 (Lepton MOSI1)
    "19": "D27",      # 27-A13 (Lepton SCK1)
}

# Teensy 4.1 Right 1x24 socket (J6 at X=74.52, matches V3.0 U$2 1:1 — VIN side)
J6_NETS = {
    "1": "5V",        # VIN
    "2": "GND",       # GND6
    "3": "3V3",       # 3V3-3
    "4": "D23",       # 23-A9 (Battery ADC)
    "5": "D22",       # 22-A8 (LCD Touch CS / Backlight)
    "6": "D21",       # 21-A7 (LCD DC)
    "8": "D19",       # 19-A5 (I2C SCL0)
    "9": "D18",       # 18-A4 (I2C SDA0)
    "14": "D13",      # 13-SCK (SPI0 SCK)
    "15": "GND",      # GND3
    "17": "A16",      # 40-A16 (VBUS detect ADC)
    "18": "D39",      # 39-A15 (Lepton MISO1)
    "19": "D38",      # 38-A14 (Lepton CS1)
}

# Teensy 4.1 Inner 1x5 header (J7, Y=-47.14, Pin 1 at X=61.82 .. Pin 5 at X=71.98 — matches V3.0 U$2 1:1)
J7_NETS = {
    "1": "VBAT",         # VBAT (3.3V ultra-low-Iq RTC LDO)
    "2": "3V3",          # 3V3-2
    "3": "GND",          # GND2
    "4": "TEENSY_PROG",  # PROGRAM
}

# Display 2x20 socket (J1, exact 14/14 V3.0 LCD-HEADER0 pins 1..34; pins 36/37/40 cleared for FT6206 CTP safety)
# Pins 30 & 31 route via 3-pad cut-and-bridge jumpers JP1/JP2 (default 1-2 bridged: D9->J1.30, D5->J1.31 = 100% V3.0 RTP;
# cut 1-2 & bridge 2-3 to switch to FT6206 CTP: D19->J1.30 SCL0, D18->J1.31 SDA0, JP3: D4->J1.39 INT with zero bus contention).
J1_NETS = {
    "1": "GND",
    "2": "5V",
    "21": "3V3",
    "23": "D21",
    "24": "D13",
    "25": "D10",
    "27": "D11",
    "28": "D12",
    "29": "D22",
    "30": "LCD_P30",
    "31": "LCD_P31",
    "32": "D12",
    "33": "D13",
    "34": "D11",
    "39": "CTP_INT",
}

# FLIR Lepton Breakout V2.0 2x10 socket (J2, exact V3.0 J2 nets)
J2_NETS = {
    "1": "GND",
    "2": "5V",
    "5": "D18",
    "7": "D27",
    "8": "D19",
    "9": "D26",
    "10": "D38",
    "12": "D39",
    "15": "D3",
}

# USB-C 16-pin receptacle (J3: TYPE-C-31-M-12)
J3_NETS = {
    "A1": "GND", "B1": "GND", "A12": "GND", "B12": "GND", "SH": "GND",
    "A4": "VBUS", "B4": "VBUS", "A9": "VBUS", "B9": "VBUS",
    "A5": "USB_CC1", "B5": "USB_CC2",
    "A6": "USB_DP_RAW", "B6": "USB_DP_RAW",
    "A7": "USB_DM_RAW", "B7": "USB_DM_RAW",
}

# USBLC6-2SC6 ESD protection (D1: SOT-23-6)
# Pin 1: I/O1 (D- in), Pin 2: GND, Pin 3: I/O2 (D+ in), Pin 4: I/O2 (D+ out), Pin 5: VBUS, Pin 6: I/O1 (D- out)
# Note: USBLC6-2SC6 connects 1<->6 internally and 3<->4 internally!
D1_NETS = {
    "1": "USB_DM_RAW",
    "2": "GND",
    "3": "USB_DP_RAW",
    "4": "USB_DP_RAW",
    "5": "VBUS",
    "6": "USB_DM_RAW",
}

# TI BQ24074RGTR Dynamic Power-Path Li-Ion Charger (U3: VQFN-16-EP, VOUT(REG)=4.4V < TPS61022 4.8V cold-start max!)
# Pin 1:TS, 2:BAT, 3:BAT, 4:~CE, 5:EN2, 6:EN1, 7:~PGOOD, 8:VSS,
# 9:~CHG, 10:OUT, 11:OUT, 12:ILIM, 13:IN, 14:TMR, 15:ITERM (R23=3.0k -> 90mA = 10.1% of 890mA), 16:ISET, 17:EP(GND)
U3_NETS = {
    "1": "BQ_TS",
    "2": "VLIPO",
    "3": "VLIPO",
    "4": "GND",
    "5": "BQ_EN2",
    "6": "GND",
    "8": "GND",
    "9": "BQ_CHG_N",
    "10": "VSYS",
    "11": "VSYS",
    "12": "BQ_ILIM",
    "13": "VBUS",
    "14": "BQ_TMR",
    "15": "BQ_ITERM",
    "16": "BQ_ISET",
    "17": "GND",
}

# TI TPS61022RWUR 5V Synchronous Boost Converter (U2: VQFN-7 RWU)
# Pin 1:GND, 2:SW, 3:VOUT, 4:FB, 5:EN, 6:MODE(GND=AutoPFM), 7:VIN
U2_NETS = {
    "1": "GND",
    "2": "BOOST_SW",
    "3": "5V",
    "4": "BOOST_FB",
    "5": "BOOST_EN",
    "6": "GND",
    "7": "VSYS",
}

# Component table: (ref, val, fp_name, x, y, rot_deg, side, nets_dict, sch_group)
COMPONENTS = [
    # --- Module Sockets (exact V3.0 positions!) ---
    ("J1",   "Display_2x20_Socket", "PinSocket_2x20_Display",  2.60, -34.45,   0, "F.Cu", J1_NETS, "display"),
    ("J2",   "Lepton_2x10_Socket",  "PinSocket_2x10_Lepton",  31.40, -21.00,   0, "B.Cu", J2_NETS, "lepton"),
    ("J5",   "Teensy_1x24_L",       "PinSocket_1x24",         59.28,  -1.42,   0, "B.Cu", J5_NETS, "teensy"),
    ("J6",   "Teensy_1x24_R",       "PinSocket_1x24",         74.52,  -1.42,   0, "B.Cu", J6_NETS, "teensy"),
    ("J7",   "Teensy_1x05_Inner",   "PinSocket_1x05",         61.82, -47.14, 270, "B.Cu", J7_NETS, "teensy"),
    ("P1",   "Pogo_USB_DP",         "PogoPin_SMD_2.0mm",      65.63,  -2.15,   0, "B.Cu", {"1": "USB_DP_RAW"}, "teensy"),
    ("P2",   "Pogo_USB_DM",         "PogoPin_SMD_2.0mm",      68.17,  -2.15,   0, "B.Cu", {"1": "USB_DM_RAW"}, "teensy"),

    # --- USB-C & ESD (bottom edge X=45.80, datasheet overhang mouth at Y=-0.8mm -> center Y=-3.35) ---
    ("J3",   "TYPE-C-31-M-12",      "USB_C_TYPE_C_31_M_12",   45.80,  -3.35, 180, "B.Cu", J3_NETS, "usbc"),
    ("D1",   "USBLC6-2SC6",         "SOT23_6",                53.70,  -8.10,   0, "B.Cu", D1_NETS, "usbc"),
    ("R7",   "5.1k",                "R_0603",                 52.50, -12.40,   0, "B.Cu", {"1": "USB_CC1", "2": "GND"}, "usbc"),
    ("R8",   "5.1k",                "R_0603",                 40.50, -10.80,   0, "B.Cu", {"1": "USB_CC2", "2": "GND"}, "usbc"),

    # --- TI BQ24074 Power-Path Charger (VOUT=4.4V < TPS61022 4.8V cold-start, ITERM=3.0k -> 90mA, ILIM=1.2k) ---
    ("U3",   "BQ24074RGTR",         "BQ24075_VQFN16",         45.80, -15.60,   0, "B.Cu", U3_NETS, "charger"),
    ("C1",   "10uF",                "C_0805",                 48.60, -11.40,  90, "B.Cu", {"1": "VBUS", "2": "GND"}, "charger"),
    ("C2",   "10uF",                "C_0805",                 51.20, -15.60,  90, "B.Cu", {"1": "VSYS", "2": "GND"}, "charger"),
    ("C3",   "10uF",                "C_0805",                 40.40, -15.60,  90, "B.Cu", {"1": "VLIPO", "2": "GND"}, "charger"),
    ("R22",  "10k",                 "R_0603",                 50.80, -19.40,   0, "B.Cu", {"1": "VSYS", "2": "BQ_EN2"}, "charger"),
    ("R23",  "3.0k",                "R_0603",                 44.80, -10.50,   0, "B.Cu", {"1": "BQ_ITERM", "2": "GND"}, "charger"),
    ("R9",   "10k",                 "R_0603",                 40.50, -19.40,   0, "B.Cu", {"1": "BQ_TS", "2": "GND"}, "charger"),
    ("R10",  "1.0k",                "R_0603",                 36.80, -11.50,   0, "B.Cu", {"1": "BQ_ISET", "2": "GND"}, "charger"),
    ("R11",  "1.2k",                "R_0603",                 54.20, -15.60,  90, "B.Cu", {"1": "BQ_ILIM", "2": "GND"}, "charger"),
    ("R14",  "47k",                 "R_0603",                 36.80, -19.40,   0, "B.Cu", {"1": "BQ_TMR", "2": "GND"}, "charger"),

    # --- Onboard 5V Synchronous Boost Converter (TI TPS61022RWUR, tight C4/C9 <=1.5mm + L1 adjacent) ---
    ("U2",   "TPS61022RWUR",        "TPS61022_VQFN7",         19.20, -54.00,   0, "B.Cu", U2_NETS, "boost"),
    ("L1",   "1.0uH_15A",           "L_0630",                 12.20, -54.00,   0, "B.Cu", {"1": "VSYS", "2": "BOOST_SW"}, "boost"),
    ("C8",   "22uF",                "C_0805",                 19.00, -51.40,   0, "B.Cu", {"1": "VSYS", "2": "GND"}, "boost"),
    ("C4",   "22uF",                "C_0805",                 19.00, -56.60,   0, "B.Cu", {"1": "5V", "2": "GND"}, "boost"),
    ("C9",   "22uF",                "C_0805",                 19.00, -58.80,   0, "B.Cu", {"1": "5V", "2": "GND"}, "boost"),
    ("R16",  "750k",                "R_0603",                 23.00, -55.40,  90, "B.Cu", {"1": "5V", "2": "BOOST_FB"}, "boost"),
    ("R17",  "100k",                "R_0603",                 25.00, -55.40,  90, "B.Cu", {"1": "BOOST_FB", "2": "GND"}, "boost"),
    ("R15",  "100k",                "R_0603",                 23.00, -51.40,   0, "B.Cu", {"1": "BOOST_EN", "2": "GND"}, "boost"),

    # --- Optional FT6206 Capacitive Touch Cut-and-Bridge Jumpers (1-2 bridged by default = 100% V3.0 RTP, zero I2C contention) ---
    ("JP1",  "CTP_SCL_SEL",         "SJ_3P_BRIDGED12",         8.80, -36.00,   0, "B.Cu", {"1": "D9", "2": "LCD_P30", "3": "D19"}, "display"),
    ("JP2",  "CTP_SDA_SEL",         "SJ_3P_BRIDGED12",         8.80, -40.50,   0, "B.Cu", {"1": "D5", "2": "LCD_P31", "3": "D18"}, "display"),
    ("JP3",  "CTP_INT_OPT",         "SJ_2P_OPEN",              8.50, -60.50,   0, "B.Cu", {"1": "CTP_INT", "2": "D4"}, "display"),

    # --- Local Rail Decoupling (SPI0/SPI1 & Lepton stability — Basic 10uF & 100nF; C7 inside Teensy bay clear of Lepton/cradle) ---
    ("C5",   "10uF",                "C_0805",                 27.50, -23.00,  90, "B.Cu", {"1": "5V", "2": "GND"}, "boost"),
    ("C6",   "100nF",               "C_0603",                  8.50, -12.00,  90, "B.Cu", {"1": "5V", "2": "GND"}, "boost"),
    ("C7",   "100nF",               "C_0603",                 62.50, -37.00,  90, "B.Cu", {"1": "3V3", "2": "GND"}, "boost"),

    # --- Active LiPo Reverse-Polarity Safety Chain (Adafruit/SparkFun JST-PH polarity: 1=GND, 2=VBAT+) + 1uA RTC LDO ---
    ("J4",   "S2B-PH-SM4-TB",       "JST_PH_2P_RA",           84.50, -52.10, 180, "B.Cu", {"1": "GND", "2": "LIPO_RAW", "MP": "GND"}, "io"),
    ("Q1",   "AO3401A_RevPol",      "SOT23",                  83.00, -39.50,   0, "B.Cu", {"1": "Q1_GATE", "2": "LIPO_PROT", "3": "LIPO_RAW"}, "io"),
    ("R18",  "750k",                "R_0603",                 83.00, -35.80,   0, "B.Cu", {"1": "Q1_GATE", "2": "LIPO_PROT"}, "io"),
    ("Q2",   "AO3400A_RevPol_Ctl",  "SOT23",                  83.00, -44.20,   0, "B.Cu", {"1": "Q2_GATE", "2": "GND", "3": "Q1_GATE"}, "io"),
    ("R20",  "47k",                 "R_0603",                 79.20, -44.20,  90, "B.Cu", {"1": "LIPO_RAW", "2": "Q2_GATE"}, "io"),
    ("R21",  "750k",                "R_0603",                 79.20, -39.50,  90, "B.Cu", {"1": "Q2_GATE", "2": "GND"}, "io"),
    ("F1",   "PTC_2A_1206",         "FUSE_1206",              83.00, -31.80,  90, "B.Cu", {"1": "LIPO_PROT", "2": "VLIPO"}, "io"),
    ("U4",   "XC6206P332MR_RTC",    "SOT23",                  26.50,  -8.00,   0, "B.Cu", {"1": "GND", "2": "VBAT", "3": "VLIPO"}, "io"),
    ("C10",  "10uF",                "C_0805",                 26.50, -12.00,   0, "B.Cu", {"1": "VBAT", "2": "GND"}, "io"),
    ("C11",  "100nF",               "C_0603",                 26.50,  -4.20,   0, "B.Cu", {"1": "VLIPO", "2": "GND"}, "io"),
    ("J8",   "I2C_EXT_DNP",         "I2C_PADS_1x04",          22.50, -42.50,  90, "B.Cu", {"1": "GND", "2": "3V3", "3": "D18", "4": "D19"}, "sense"),
    ("R19",  "10k_BL_PU",           "R_0603",                 12.50, -21.00,   0, "B.Cu", {"1": "D22", "2": "3V3"}, "display"),
    ("SW1",  "JS102011SAQN",        "SW_SLIDE_RA",            87.20, -12.70, 270, "B.Cu", {"1": "GND", "2": "BOOST_EN", "3": "VSYS"}, "io"),
    ("SW2",  "B3U-3000P",           "SW_TACT_RA",             84.00, -67.00, 180, "B.Cu", {"1": "3V3", "2": "D2"}, "io"),
    ("SW3",  "TS-1187A",            "SW_TACT_PROG",           85.30,  -4.00,   0, "B.Cu", {"1": "GND", "2": "TEENSY_PROG"}, "io"),
    ("D2",   "LED_Red_CHG",         "LED_0603",                5.75, -66.40,  90, "B.Cu", {"1": "BQ_CHG_N", "2": "LED_CHG_A"}, "io"),
    ("R12",  "1.0k",                "R_0603",                  5.75, -63.00,  90, "B.Cu", {"1": "VBUS", "2": "LED_CHG_A"}, "io"),

    # --- High-Z Sensing Voltage Dividers (10x scaled: <28uA quiescent, <2.86V / <2.63V safe ADC) & I2C Pull-Ups ---
    ("R1",   "47k",                 "R_0603",                 16.50, -35.50,   0, "B.Cu", {"1": "VLIPO", "2": "D23"}, "sense"),
    ("R2",   "100k",                "R_0603",                 16.50, -32.80,   0, "B.Cu", {"1": "D23", "2": "GND"}, "sense"),
    ("R3",   "100k",                "R_0603",                 16.50, -29.80,   0, "B.Cu", {"1": "VBUS", "2": "A16"}, "sense"),
    ("R5",   "100k",                "R_0603",                 16.50, -27.00,   0, "B.Cu", {"1": "A16", "2": "GND"}, "sense"),
    ("R4",   "4.7k",                "R_0603",                 22.50, -33.00,   0, "B.Cu", {"1": "3V3", "2": "D18"}, "sense"),
    ("R6",   "4.7k",                "R_0603",                 22.50, -30.00,   0, "B.Cu", {"1": "3V3", "2": "D19"}, "sense"),

    # --- Standardized Mounting Holes (4x M2.5 2.8mm + 4x Lepton M2 2.1mm) ---
    ("H1",   "M2.5_Hole",           "MountingHole_2.8mm_M2.5_Pad", 12.50,  -4.00, 0, "F.Cu", {"1": "GND"}, "mech"),
    ("H2",   "M2.5_Hole",           "MountingHole_2.8mm_M2.5_Pad", 79.00,  -4.00, 0, "F.Cu", {"1": "GND"}, "mech"),
    ("H3",   "M2.5_Hole",           "MountingHole_2.8mm_M2.5_Pad", 12.50, -64.90, 0, "F.Cu", {"1": "GND"}, "mech"),
    ("H4",   "M2.5_Hole",           "MountingHole_2.8mm_M2.5_Pad", 79.00, -64.90, 0, "F.Cu", {"1": "GND"}, "mech"),
    ("H5",   "M2_Lepton",           "MountingHole_2.1mm_M2",       30.00, -43.00, 0, "F.Cu", {}, "mech"),
    ("H6",   "M2_Lepton",           "MountingHole_2.1mm_M2",       55.50, -43.00, 0, "F.Cu", {}, "mech"),
    ("H7",   "M2_Lepton",           "MountingHole_2.1mm_M2",       30.00, -59.00, 0, "F.Cu", {}, "mech"),
    ("H8",   "M2_Lepton",           "MountingHole_2.1mm_M2",       55.50, -59.00, 0, "F.Cu", {}, "mech"),
]


# ---------------------------------------------------------------------------
# 3. 2-Layer Obstacle-Aware A* Router (0.20 mm grid, exact clearance math)
# ---------------------------------------------------------------------------
GRID_MM = 0.20
NX = int(round(BOARD_W / GRID_MM)) + 1
NY = int(round(BOARD_H / GRID_MM)) + 1

POWER_NETS = {"5V", "VSYS", "VLIPO", "LIPO_RAW", "LIPO_PROT", "VBUS", "3V3", "BOOST_SW"}
CRITICAL_NET_ORDER = [
    # 1. USB-C differential & ESD (shortest, straightest)
    "USB_DM_RAW", "USB_DP_RAW", "USB_CC1", "USB_CC2",
    # 2. Boost switching & feedback loops (tightest!)
    "BOOST_SW", "BOOST_FB", "BOOST_EN",
    # 3. BQ24075 & LiPo safety local loops
    "LIPO_RAW", "LIPO_PROT", "Q1_GATE",
    "BQ_TS", "BQ_ISET", "BQ_ILIM", "BQ_TMR",
    # 4. Lepton SPI1 + VSYNC + I2C (high-speed signal integrity)
    "D27", "D26", "D38", "D39", "D3", "D18", "D19",
    # 5. Display SPI0 + control (route J1 outer-edge pins D9, D12 first!)
    "D9", "D12", "D13", "D11", "D10", "D21", "D22", "D5", "BL_PWM_OUT",
    # 6. Power rails
    "VBUS", "VLIPO", "VSYS", "5V", "3V3", "VBAT",
    # 7. Remaining controls, dividers, LEDs
    "D2", "D23", "A16", "TEENSY_PROG", "BQ_CHG_N", "BQ_PGOOD_N", "LED_CHG_A", "LED_PG_A",
]


def xy_to_grid(x: float, y: float) -> tuple[int, int]:
    gx = max(2, min(NX - 3, int(round(x / GRID_MM))))
    gy = max(2, min(NY - 3, int(round((-y) / GRID_MM))))
    return gx, gy


def grid_to_xy(gx: int, gy: int) -> tuple[float, float]:
    return round(gx * GRID_MM, 4), round(-gy * GRID_MM, 4)


def route_all_nets(pads: list[dict]) -> tuple[list[tuple], list[tuple], list[str]]:
    from v4_router import route_all_nets_exact
    return route_all_nets_exact(pads, {c[0]: (c[3], c[4]) for c in COMPONENTS})

def _legacy_route_all_nets(pads: list[dict]):

    """Route all non-GND multi-pad nets with 2-layer obstacle-aware A* on a 0.20mm grid."""

    occ = [[None] * (NX * NY) for _ in range(2)]
    via_block = bytearray(NX * NY)

    def idx(gx: int, gy: int) -> int:
        return gy * NX + gx

    # Block board edge margin (0.60 mm = 3 cells)
    for gy in range(NY):
        for gx in range(NX):
            if gx < 3 or gx >= NX - 3 or gy < 3 or gy >= NY - 3:
                i = idx(gx, gy)
                occ[0][i] = "__EDGE__"
                occ[1][i] = "__EDGE__"
                via_block[i] = 1

    # Exact clearance math for 0.20mm signal / 0.28mm power traces & 0.60mm vias (DRC min_clearance = 0.15mm)
    sig_pclr = 0.27
    via_pclr = 0.48
    for p in pads:
        net_tag = p["net"] if (p["net"] and not p["is_npth"]) else "__PAD__"
        is_fine = (min(p["hx"], p["hy"]) < 0.25) and (not p["is_npth"])
        pclr = 0.16 if is_fine else sig_pclr
        rx, ry = p["hx"] + pclr, p["hy"] + pclr
        gx0 = max(0, int(math.floor((p["x"] - rx) / GRID_MM)))
        gx1 = min(NX - 1, int(math.ceil((p["x"] + rx) / GRID_MM)))
        gy0 = max(0, int(math.floor((-p["y"] - ry) / GRID_MM)))
        gy1 = min(NY - 1, int(math.ceil((-p["y"] + ry) / GRID_MM)))
        for gy in range(gy0, gy1 + 1):
            wy = -gy * GRID_MM
            if abs(wy - p["y"]) >= ry:
                continue
            for gx in range(gx0, gx1 + 1):
                wx = gx * GRID_MM
                if abs(wx - p["x"]) >= rx:
                    continue
                i = idx(gx, gy)
                if p["on_f"] or p["is_npth"]:
                    occ[0][i] = net_tag if (occ[0][i] is None or occ[0][i] == net_tag) else "__CONFLICT__"
                if p["on_b"] or p["is_npth"]:
                    occ[1][i] = net_tag if (occ[1][i] is None or occ[1][i] == net_tag) else "__CONFLICT__"
        vrx, vry = p["hx"] + via_pclr, p["hy"] + via_pclr
        vx0 = max(0, int(math.floor((p["x"] - vrx) / GRID_MM)))
        vx1 = min(NX - 1, int(math.ceil((p["x"] + vrx) / GRID_MM)))
        vy0 = max(0, int(math.floor((-p["y"] - vry) / GRID_MM)))
        vy1 = min(NY - 1, int(math.ceil((-p["y"] + vry) / GRID_MM)))
        for gy in range(vy0, vy1 + 1):
            if abs(-gy * GRID_MM - p["y"]) >= vry:
                continue
            for gx in range(vx0, vx1 + 1):
                if abs(gx * GRID_MM - p["x"]) >= vrx:
                    continue
                via_block[idx(gx, gy)] = 1

    def mark_line_occ(x1: float, y1: float, x2: float, y2: float, lyr: int, net: str, clr_mm: float, via_clr_mm: float = 0.54) -> None:
        dist_mm = math.hypot(x2 - x1, y2 - y1)
        steps = max(int(math.ceil(dist_mm / (GRID_MM * 0.5))), 1)
        r_cells = int(math.ceil(max(clr_mm, via_clr_mm) / GRID_MM))
        for s in range(steps + 1):
            wx = x1 + (x2 - x1) * s / steps
            wy = y1 + (y2 - y1) * s / steps
            gx0, gy0 = xy_to_grid(wx, wy)
            for dy in range(-r_cells, r_cells + 1):
                for dx in range(-r_cells, r_cells + 1):
                    nx_, ny_ = gx0 + dx, gy0 + dy
                    if 0 <= nx_ < NX and 0 <= ny_ < NY:
                        dmm = math.hypot(nx_ * GRID_MM - wx, -ny_ * GRID_MM - wy)
                        i = idx(nx_, ny_)
                        if dmm <= via_clr_mm:
                            via_block[i] = 1
                        if dmm <= clr_mm:
                            if occ[lyr][i] is None:
                                occ[lyr][i] = net
                            elif occ[lyr][i] != net:
                                occ[lyr][i] = "__CONFLICT__"

    segments = []  # (x1, y1, x2, y2, width, layer_id, net_name)
    vias = []      # (x, y, net_name)
    failed = []
    comp_center = {c[0]: (c[3], c[4]) for c in COMPONENTS}
    fine_refs = {"J3", "U3", "U2", "D1", "U4"}
    fanout_tip: dict[tuple[str, str], tuple[float, float]] = {}
    for p in pads:
        if p["ref"] in fine_refs and p["net"] and p["net"] != "GND" and not p["is_npth"] and p["num"] != "SH":
            cx0, cy0 = comp_center[p["ref"]]
            dx, dy = p["x"] - cx0, p["y"] - cy0
            if p["ref"] == "J3":
                # Stagger USB-C pads so A6/B6 (DP), A7/B7 (DM), A5/B5 (CC), A4/B4/A9/B9 (VBUS) fan out cleanly!
                stagger = {"A6": 1.0, "B6": 1.0, "A7": 1.5, "B7": 1.5, "A5": 1.3, "B5": 1.3}.get(p["num"], 1.9)
                fx, fy = p["x"], p["y"] - stagger
            elif abs(dx) >= abs(dy):
                fx, fy = p["x"] + (1.2 if dx >= 0 else -1.2), p["y"]
            else:
                fx, fy = p["x"], p["y"] + (1.2 if dy >= 0 else -1.2)
            fx, fy = round(fx, 4), round(fy, 4)
            fanout_tip[(p["ref"], p["num"])] = (fx, fy)
            lyr = 1 if p["on_b"] else 0
            segments.append((p["x"], p["y"], fx, fy, 0.16, lyr, p["net"]))
            mark_line_occ(p["x"], p["y"], fx, fy, lyr, p["net"], 0.25, via_clr_mm=0.55)
            gx, gy = xy_to_grid(fx, fy)
            occ[lyr][idx(gx, gy)] = p["net"]

    for p in pads:
        if p["net"] and not p["is_npth"] and p["ref"] not in fine_refs:
            cx, cy = xy_to_grid(p["x"], p["y"])
            if p["on_f"]:
                occ[0][idx(cx, cy)] = p["net"]
            if p["on_b"]:
                occ[1][idx(cx, cy)] = p["net"]

    net_pads: dict[str, list[dict]] = defaultdict(list)
    for p in pads:
        if p["net"] and p["net"] != "GND":
            net_pads[p["net"]].append(p)

    ordered_nets = [n for n in CRITICAL_NET_ORDER if n in net_pads]
    for n in sorted(net_pads):
        if n not in ordered_nets:
            ordered_nets.append(n)

    def pad_entry_cells(p: dict, net: str) -> list[tuple[int, int, int]]:
        tx, ty = fanout_tip.get((p["ref"], p["num"]), (p["x"], p["y"]))
        cx, cy = xy_to_grid(tx, ty)
        layers = ([0] if p["on_f"] else []) + ([1] if p["on_b"] else [])
        out = []
        for r in range(0, 4):
            for lyr in layers:
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        if max(abs(dx), abs(dy)) != r:
                            continue
                        gx, gy = cx + dx, cy + dy
                        if 3 <= gx < NX - 3 and 3 <= gy < NY - 3:
                            val = occ[lyr][idx(gx, gy)]
                            if val is None or val == net:
                                out.append((gx, gy, lyr))
            if out:
                break
        return out or [(cx, cy, layers[0])]

    def mark_segment_occ(gx1, gy1, gx2, gy2, lyr, net, width):
        clr_sig = width / 2.0 + 0.28
        clr_via = width / 2.0 + 0.46
        steps = max(abs(gx2 - gx1), abs(gy2 - gy1), 1)
        rv = int(math.ceil(clr_via / GRID_MM))
        for s in range(steps + 1):
            gx = int(round(gx1 + (gx2 - gx1) * s / steps))
            gy = int(round(gy1 + (gy2 - gy1) * s / steps))
            for dy in range(-rv, rv + 1):
                for dx in range(-rv, rv + 1):
                    nx_, ny_ = gx + dx, gy + dy
                    if 0 <= nx_ < NX and 0 <= ny_ < NY:
                        dmm = math.hypot(dx, dy) * GRID_MM
                        i = idx(nx_, ny_)
                        if dmm <= clr_via:
                            via_block[i] = 1
                        if dmm <= clr_sig:
                            if occ[lyr][i] is None:
                                occ[lyr][i] = net
                            elif occ[lyr][i] != net:
                                occ[lyr][i] = "__CONFLICT__"

    def astar(starts: list[tuple[int, int, int]], goals: list[tuple[int, int, int]], net: str, extra_clr: int = 0, via_cost: int = 28):
        goal_set = set(goals)
        gx_avg = sum(g[0] for g in goals) / len(goals)
        gy_avg = sum(g[1] for g in goals) / len(goals)
        pq = []
        dist = {}
        parent = {}
        for s in starts:
            dist[s] = 0
            h = int((abs(s[0] - gx_avg) + abs(s[1] - gy_avg)) * 10)
            heapq.heappush(pq, (h, 0, s))

        DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1)]
        while pq:
            _f, g, cur = heapq.heappop(pq)
            if cur in goal_set:
                path = [cur]
                while cur in parent:
                    cur = parent[cur]
                    path.append(cur)
                path.reverse()
                return path
            if g > dist.get(cur, 10**9):
                continue
            cx, cy, cl = cur
            for dx, dy in DIRS:
                nx_, ny_ = cx + dx, cy + dy
                if nx_ < 3 or nx_ >= NX - 3 or ny_ < 3 or ny_ >= NY - 3:
                    continue
                val = occ[cl][idx(nx_, ny_)]
                if val is not None and val != net:
                    continue
                if extra_clr and (nx_, ny_, cl) not in goal_set:
                    ok_e = True
                    for edy in (-1, 0, 1):
                        for edx in (-1, 0, 1):
                            ev = occ[cl][idx(nx_ + edx, ny_ + edy)]
                            if ev is not None and ev != net:
                                ok_e = False
                                break
                        if not ok_e:
                            break
                    if not ok_e:
                        continue
                step = 10 if ((cl == 0 and dy == 0) or (cl == 1 and dx == 0)) else 13
                ng = g + step
                nxt = (nx_, ny_, cl)
                if ng < dist.get(nxt, 10**9):
                    dist[nxt] = ng
                    parent[nxt] = cur
                    h = int((abs(nx_ - gx_avg) + abs(ny_ - gy_avg)) * 10)
                    heapq.heappush(pq, (ng + h, ng, nxt))
            if not via_block[idx(cx, cy)]:
                nl = 1 - cl
                val = occ[nl][idx(cx, cy)]
                if val is None or val == net:
                    ng = g + via_cost
                    nxt = (cx, cy, nl)
                    if ng < dist.get(nxt, 10**9):
                        dist[nxt] = ng
                        parent[nxt] = cur
                        h = int((abs(cx - gx_avg) + abs(cy - gy_avg)) * 10)
                        heapq.heappush(pq, (ng + h, ng, nxt))
        return None

    for net in ordered_nets:
        plist = net_pads[net]
        if len(plist) < 2:
            continue
        is_pwr = net in POWER_NETS
        width = 0.28 if is_pwr else 0.20

        connected = [0]
        remaining = list(range(1, len(plist)))
        net_tree_cells: dict[int, list[tuple[int, int, int]]] = {
            i: pad_entry_cells(plist[i], net) for i in range(len(plist))
        }
        routed_cells = list(net_tree_cells[0])

        while remaining:
            best_pair = None
            best_d = 1e9
            for ri in remaining:
                for ci in connected:
                    d = math.hypot(plist[ri]["x"] - plist[ci]["x"], plist[ri]["y"] - plist[ci]["y"])
                    if d < best_d:
                        best_d = d
                        best_pair = (ci, ri)
            ci, ri = best_pair
            starts = routed_cells
            goals = net_tree_cells[ri]
            pa, pb = plist[ci], plist[ri]
            if pa["ref"] == pb["ref"] and pa["ref"] != "J3" and math.hypot(pa["x"] - pb["x"], pa["y"] - pb["y"]) <= 0.65:
                lyr = 1 if pa["on_b"] else 0
                segments.append((pa["x"], pa["y"], pb["x"], pb["y"], 0.20, lyr, net))
                mark_line_occ(pa["x"], pa["y"], pb["x"], pb["y"], lyr, net, 0.26)
                routed_cells.extend(goals)
                connected.append(ri)
                remaining.remove(ri)
                continue

            path = astar(starts, goals, net, extra_clr=(1 if is_pwr else 0))
            if path is None and is_pwr:
                width_use = 0.20
                path = astar(starts, goals, net, extra_clr=0)
            else:
                width_use = width
            if path is None:
                failed.append(f"{net} ({pa['ref']}.{pa['num']} -> {pb['ref']}.{pb['num']})")
                remaining.remove(ri)
                connected.append(ri)
                continue

            end_pad = plist[ri]
            tx_e, ty_e = fanout_tip.get((end_pad["ref"], end_pad["num"]), (end_pad["x"], end_pad["y"]))
            epx, epy = grid_to_xy(path[-1][0], path[-1][1])
            if math.hypot(tx_e - epx, ty_e - epy) > 0.01:
                segments.append((epx, epy, tx_e, ty_e, width_use, path[-1][2], net))
                mark_line_occ(epx, epy, tx_e, ty_e, path[-1][2], net, width_use / 2.0 + 0.26)
            if len(connected) == 1:
                sp = plist[0]
                tx_s, ty_s = fanout_tip.get((sp["ref"], sp["num"]), (sp["x"], sp["y"]))
                spx, spy = grid_to_xy(path[0][0], path[0][1])
                if math.hypot(tx_s - spx, ty_s - spy) > 0.01:
                    segments.append((tx_s, ty_s, spx, spy, width_use, path[0][2], net))
                    mark_line_occ(tx_s, ty_s, spx, spy, path[0][2], net, width_use / 2.0 + 0.26)

            run_start = path[0]
            prev = path[0]
            prev_dir = None
            for node in path[1:]:
                if node[2] != prev[2]:
                    if prev != run_start:
                        x1, y1 = grid_to_xy(run_start[0], run_start[1])
                        x2, y2 = grid_to_xy(prev[0], prev[1])
                        segments.append((x1, y1, x2, y2, width_use, run_start[2], net))
                        mark_segment_occ(run_start[0], run_start[1], prev[0], prev[1], run_start[2], net, width_use)
                    vx, vy = grid_to_xy(prev[0], prev[1])
                    vias.append((vx, vy, net))
                    for dy in range(-3, 4):
                        for dx in range(-3, 4):
                            nx_, ny_ = prev[0] + dx, prev[1] + dy
                            if 0 <= nx_ < NX and 0 <= ny_ < NY:
                                dmm = math.hypot(dx, dy) * GRID_MM
                                ii = idx(nx_, ny_)
                                if dmm <= 0.78:
                                    via_block[ii] = 1
                                if dmm <= 0.58:
                                    occ[0][ii] = net if (occ[0][ii] is None or occ[0][ii] == net) else "__CONFLICT__"
                                    occ[1][ii] = net if (occ[1][ii] is None or occ[1][ii] == net) else "__CONFLICT__"
                    run_start = node
                    prev = node
                    prev_dir = None
                else:
                    cur_dir = (node[0] - prev[0], node[1] - prev[1])
                    if prev_dir is not None and cur_dir != prev_dir:
                        x1, y1 = grid_to_xy(run_start[0], run_start[1])
                        x2, y2 = grid_to_xy(prev[0], prev[1])
                        segments.append((x1, y1, x2, y2, width_use, run_start[2], net))
                        mark_segment_occ(run_start[0], run_start[1], prev[0], prev[1], run_start[2], net, width_use)
                        run_start = prev
                    prev_dir = cur_dir
                    prev = node
            if prev != run_start:
                x1, y1 = grid_to_xy(run_start[0], run_start[1])
                x2, y2 = grid_to_xy(prev[0], prev[1])
                segments.append((x1, y1, x2, y2, width_use, run_start[2], net))
                mark_segment_occ(run_start[0], run_start[1], prev[0], prev[1], run_start[2], net, width_use)

            routed_cells.extend(path[::4])
            routed_cells.extend(goals)
            connected.append(ri)
            remaining.remove(ri)

    # Automatic dense GND stitching via grid across all open board regions (6 mm spacing)
    gnd_vias = []
    for gy_mm in range(6, 65, 6):
        for gx_mm in range(8, 86, 6):
            gx, gy = xy_to_grid(float(gx_mm), float(-gy_mm))
            i = idx(gx, gy)
            if not via_block[i] and occ[0][i] is None and occ[1][i] is None:
                clear = True
                for dy in range(-2, 3):
                    for dx in range(-2, 3):
                        ii = idx(gx + dx, gy + dy)
                        if occ[0][ii] is not None or occ[1][ii] is not None:
                            clear = False
                            break
                    if not clear:
                        break
                if clear:
                    gnd_vias.append((float(gx_mm), float(-gy_mm), "GND"))
                    for dy in range(-3, 4):
                        for dx in range(-3, 4):
                            nx_, ny_ = gx + dx, gy + dy
                            if 0 <= nx_ < NX and 0 <= ny_ < NY:
                                via_block[idx(nx_, ny_)] = 1

    return segments, vias + gnd_vias, failed


# ---------------------------------------------------------------------------
# 4. Build KiCad 10 PCB (.kicad_pcb)
# ---------------------------------------------------------------------------
def flip_layers_str(s: str) -> str:
    """Flip F.* <-> B.* layer tokens inside a footprint S-expression for B.Cu placement."""
    s = re.sub(r'"F\.(Cu|Paste|Mask|SilkS|Silkscreen|Fab|CrtYd|Adhes)"', r'"__TMP_B__.\1"', s)
    s = re.sub(r'"B\.(Cu|Paste|Mask|SilkS|Silkscreen|Fab|CrtYd|Adhes)"', r'"F.\1"', s)
    s = s.replace('"__TMP_B__.', '"B.')
    return s


def flip_coords_in_fp(body: str) -> str:
    """Negate local Y (and invert angles) for B.Cu footprint placement."""
    def neg_pair(m):
        tag, x, y = m.group(1), m.group(2), m.group(3)
        ny = f"{-float(y):g}"
        rest = m.group(4) or ""
        if rest:
            ang = float(rest.strip())
            rest = f" {(-ang) % 360:g}"
        return f"({tag} {x} {ny}{rest})"
    body = re.sub(r'\((at|start|end|mid|center|xy)\s+([-\d.]+)\s+([-\d.]+)(\s+[-\d.]+)?\)', neg_pair, body)
    return body


def strip_sexpr_blocks(text: str, prefixes: tuple[str, ...]) -> str:
    """Remove top-level balanced S-expr blocks starting with any prefix."""
    out = []
    pos = 0
    while pos < len(text):
        hit = -1
        for pfx in prefixes:
            idx = text.find(pfx, pos)
            if idx != -1 and (hit == -1 or idx < hit):
                hit = idx
        if hit == -1:
            out.append(text[pos:])
            break
        out.append(text[pos:hit])
        depth = 0
        j = hit
        while j < len(text):
            if text[j] == "(":
                depth += 1
            elif text[j] == ")":
                depth -= 1
                if depth == 0:
                    j += 1
                    break
            j += 1
        pos = j
    return "".join(out)


def place_footprint_sexpr(ref: str, val: str, fp_name: str, x: float, y: float, rot: float, side: str, nets: dict, net_id: dict) -> tuple[str, list[str], list[dict]]:
    raw = (FP_DIR / f"{fp_name}.kicad_mod").read_text(encoding="utf-8").strip()
    m = re.match(r'^\(footprint "[^"]+"\s*(.*)\)$', raw, re.S)
    inner = m.group(1)
    inner = re.sub(r'\n?\s*\(version [^\)]+\)', '', inner)
    inner = re.sub(r'\n?\s*\(generator[^\)]+\)', '', inner)
    inner = re.sub(r'\n?\s*\(layer "[^"]+"\)', '', inner, count=1)
    inner = strip_sexpr_blocks(inner, ('(property "Reference"', '(property "Value"', '(fp_text reference', '(fp_text value'))

    if side == "B.Cu":
        inner = flip_layers_str(inner)
        inner = flip_coords_in_fp(inner)

    rad = math.radians(rot)
    pads_info = []
    pad_nums = []
    seen_p = set()

    def patch_pad_block(pblock: str) -> str:
        pm = re.match(r'\(pad "([^"]*)" (\w+)', pblock)
        if not pm:
            return pblock
        pnum = pm.group(1)
        ptype = pm.group(2)
        if pnum and pnum not in seen_p:
            seen_p.add(pnum)
            pad_nums.append(pnum)
        am = re.search(r'\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?\)', pblock)
        sm = re.search(r'\(size\s+([-\d.]+)\s+([-\d.]+)\)', pblock)
        dm = re.search(r'\(drill\s+(?:oval\s+)?([-\d.]+)', pblock)
        px, py = float(am.group(1)), float(am.group(2))
        pang = float(am.group(3) or 0.0)
        # In KiCad .kicad_pcb, pad (at x y ang) takes ABSOLUTE board angle = (pang + fp_rot) % 360
        if rot:
            abs_ang = (pang + rot) % 360.0
            pblock = pblock[:am.start()] + f"(at {am.group(1)} {am.group(2)} {abs_ang:g})" + pblock[am.end():]
        sx, sy = float(sm.group(1)), float(sm.group(2))
        if dm:
            dr = float(dm.group(1))
            sx, sy = max(sx, dr + 0.3), max(sy, dr + 0.3)
        wx = round(x + px * math.cos(rad) + py * math.sin(rad), 4)
        wy = round(y - px * math.sin(rad) + py * math.cos(rad), 4)
        tot_ang = (pang + rot) % 180.0
        if 45.0 <= tot_ang <= 135.0:
            sx, sy = sy, sx
        on_f = ('"*.Cu"' in pblock) or ('"F.Cu"' in pblock)
        on_b = ('"*.Cu"' in pblock) or ('"B.Cu"' in pblock)
        net_name = nets.get(pnum, "")
        pads_info.append({
            "ref": ref, "num": pnum, "net": net_name,
            "x": wx, "y": wy, "hx": sx / 2.0, "hy": sy / 2.0,
            "on_f": on_f, "on_b": on_b, "is_npth": (ptype == "np_thru_hole"),
        })
        if net_name:
            nid = net_id[net_name]
            pblock = pblock[:-1] + f' (net {nid} "{net_name}"))'
        return pblock

    res = []
    pos = 0
    while pos < len(inner):
        pidx = inner.find("(pad ", pos)
        if pidx == -1:
            res.append(inner[pos:])
            break
        res.append(inner[pos:pidx])
        depth = 0
        j = pidx
        while j < len(inner):
            if inner[j] == "(":
                depth += 1
            elif inner[j] == ")":
                depth -= 1
                if depth == 0:
                    j += 1
                    break
            j += 1
        res.append(patch_pad_block(inner[pidx:j]))
        pos = j
    inner = "".join(res)

    # Mirror any text inside B.Cu footprints so KiCad DRC reports 0 nonmirrored_text_on_back_layer warnings
    if side == "B.Cu":
        inner = re.sub(
            r'(\(effects\s+\(font\s+\(size[^\)]+\)(?:\s+\(thickness[^\)]+\))?\))(\s*\))',
            r'\1 (justify mirror)\2',
            inner,
        )

    # Rotate pad angles by rot if needed and indent inside board
    at_str = f"(at {x} {y} {rot})" if rot else f"(at {x} {y})"
    mir_eff = " (justify mirror)" if side == "B.Cu" else ""
    fp_sexpr = (
        f'\t(footprint "V40:{fp_name}"\n'
        f'\t\t(layer "{side}")\n'
        f'\t\t(uuid "{uid("fp:" + ref)}")\n'
        f'\t\t{at_str}\n'
        f'\t\t(property "Reference" "{ref}" (at 0 0 0) (layer "{side.replace("Cu", "Fab")}") (hide yes) (uuid "{uid("ref:" + ref)}") (effects (font (size 1 1) (thickness 0.15)){mir_eff}))\n'
        f'\t\t(property "Value" "{val}" (at 0 1.5 0) (layer "{side.replace("Cu", "Fab")}") (hide yes) (uuid "{uid("val:" + ref)}") (effects (font (size 1 1) (thickness 0.15)){mir_eff}))\n'
        f'{inner}\n'
        f'\t)'
    )
    return fp_sexpr, pad_nums, pads_info


def assert_v3_header_parity(all_pads: list[dict]) -> None:
    """Assert 0.00 mm XY + exact net parity of J1, J2, J5, J6, J7 against proven kicad-v3.0."""
    import pcbnew

    v3_path = PCB_ROOT / "kicad-v3.0" / "diy-thermocam-v3.0.kicad_pcb"
    if not v3_path.exists():
        raise FileNotFoundError(f"Cannot verify V3.0 header parity: missing {v3_path}")
    b = pcbnew.LoadBoard(str(v3_path))
    net_alias = {"AGND": "GND", "N$42": "TEENSY_PROG"}
    # J1.30 (LCD_P30) & J1.31 (LCD_P31) are copper-bridged to D9 & D5 by default via 3-pad net-tie jumpers JP1/JP2
    lcd_net_tie = {("LCD-HEADER0", "30"): "LCD_P30", ("LCD-HEADER0", "31"): "LCD_P31"}
    v4_by_xy: dict[tuple[float, float], list[tuple[str, str, str]]] = defaultdict(list)
    for p in all_pads:
        if p["ref"] in ("J1", "J2", "J5", "J6", "J7"):
            v4_by_xy[(round(p["x"], 2), round(p["y"], 2))].append((p["ref"], p["num"], p["net"]))

    checked = 0
    mismatches = []
    for fp in b.GetFootprints():
        ref = fp.GetReference()
        if ref not in ("LCD-HEADER0", "J2", "U$2"):
            continue
        for pad in fp.Pads():
            net = pad.GetNetname()
            if not net or net == "VUSB":
                continue
            # J1.40 is intentionally cleared on V4.0 (BuyDisplay CTP_RST safety — grounding holds FT6206 in reset)
            if ref == "LCD-HEADER0" and pad.GetNumber() == "40":
                continue
            exp_net = lcd_net_tie.get((ref, pad.GetNumber()), net_alias.get(net, net))
            pp = pad.GetPosition()
            xy = (round(pcbnew.ToMM(pp.x), 2), round(pcbnew.ToMM(pp.y), 2))
            hits = v4_by_xy.get(xy, [])
            checked += 1
            if not any(h[2] == exp_net for h in hits):
                mismatches.append(f"V3 {ref}.{pad.GetNumber()} @ {xy} expected {exp_net}, V4 got {hits}")
    # Also assert J1.36/37/40 are disconnected and JP1/JP2 default 1-2 bridges tie D9<->LCD_P30 & D5<->LCD_P31
    jp_nets = {(p["ref"], p["num"]): p["net"] for p in all_pads if p["ref"] in ("JP1", "JP2")}
    if (jp_nets.get(("JP1", "1")), jp_nets.get(("JP1", "2")), jp_nets.get(("JP1", "3"))) != ("D9", "LCD_P30", "D19"):
        mismatches.append(f"JP1 net-tie mismatch: {jp_nets}")
    if (jp_nets.get(("JP2", "1")), jp_nets.get(("JP2", "2")), jp_nets.get(("JP2", "3"))) != ("D5", "LCD_P31", "D18"):
        mismatches.append(f"JP2 net-tie mismatch: {jp_nets}")
    # Also assert J1.36/37/40 are disconnected on V4.0 (no ER-TFTM032-3 bus conflict / CTP reset lock)
    for p in all_pads:
        if p["ref"] == "J1" and p["num"] in ("36", "37", "40") and p["net"]:
            mismatches.append(f"V4 J1.{p['num']} must be NC, got {p['net']}")
    if mismatches:
        raise AssertionError("V4.0 vs V3.0 header parity FAILED:\n  " + "\n  ".join(mismatches))
    print(f"  [PASS] V3.0 header parity verified: {checked}/{checked} connected pads match 0.00 mm XY & net (J1.36/37/40 NC)!")


def build_pcb() -> dict:
    all_net_names = {"GND"}
    for _ref, _val, _fp, _x, _y, _rot, _side, nets, _grp in COMPONENTS:
        all_net_names.update(nets.values())
    net_list = sorted(all_net_names)
    net_id = {n: i for i, n in enumerate(net_list, start=1)}

    fp_blocks = []
    fp_pad_map = {}
    all_pads = []
    for ref, val, fp_name, x, y, rot, side, nets, _grp in COMPONENTS:
        sexpr, pad_nums, pinfo = place_footprint_sexpr(ref, val, fp_name, x, y, rot, side, nets, net_id)
        fp_blocks.append(sexpr)
        fp_pad_map[ref] = (fp_name, val, pad_nums, nets)
        all_pads.extend(pinfo)

    assert_v3_header_parity(all_pads)
    segments, vias, failed = route_all_nets(all_pads)
    print(f"Router produced {len(segments)} segments, {len(vias)} vias, failed={len(failed)}")
    for f_item in failed:
        print("  ROUTE FAIL:", f_item)

    net_decls = ['\t(net 0 "")'] + [f'\t(net {net_id[n]} "{n}")' for n in net_list]

    # Edge.Cuts outline (89.4 x 68.4 mm)
    corners = [(0, 0), (BOARD_W, 0), (BOARD_W, -BOARD_H), (0, -BOARD_H)]
    gr_blocks = []
    for i in range(4):
        x1, y1 = corners[i]
        x2, y2 = corners[(i + 1) % 4]
        gr_blocks.append(
            f'\t(gr_line (start {x1} {y1}) (end {x2} {y2}) (stroke (width 0.1) (type solid)) (layer "Edge.Cuts") (uuid "{uid(f"edge:{i}")}"))'
        )

    # Clean V4.0 Silkscreen markings
    def add_txt(txt, x, y, sz=1.2, lyr="B.SilkS", rot=0):
        just = " (justify mirror)" if lyr == "B.SilkS" else ""
        gr_blocks.append(
            f'\t(gr_text "{txt}" (at {x} {y} {rot}) (layer "{lyr}") (uuid "{uid(f"t:{lyr}:{txt}:{x}")}") '
            f'(effects (font (size {sz} {sz}) (thickness 0.2)){just}))'
        )

    for txt, x, y, sz, lyr, rot in [
        ("DIY-THERMOCAM V4.0", 42.5, -65.8, 1.5, "B.SilkS", 0),
        ("V4.0-PROTO", 42.5, -63.0, 1.2, "B.SilkS", 0),
        ("LEPTON", 42.5, -36.0, 1.6, "B.SilkS", 0),
        ("TEENSY 4.1", 66.9, -31.5, 1.4, "B.SilkS", 90),
        ("USB-C", 45.8, -11.5, 1.0, "B.SilkS", 0),
        ("5V BOOST", 19.2, -46.5, 1.0, "B.SilkS", 0),
        ("RTC 3V3", 26.5, -14.8, 1.0, "B.SilkS", 0),
        ("LIPO", 84.5, -59.6, 1.0, "B.SilkS", 0),
        ("-", 85.8, -57.6, 1.2, "B.SilkS", 0),
        ("+", 83.2, -57.6, 1.2, "B.SilkS", 0),
        ("PWR", 81.5, -12.7, 1.0, "B.SilkS", 0),
        ("CAPTURE", 84.0, -61.8, 1.0, "B.SilkS", 0),
        ("PROG", 85.0, -8.5, 1.0, "B.SilkS", 0),
        ("CHG", 5.8, -60.5, 0.9, "B.SilkS", 0),
        ("DIY-THERMOCAM V4.0-PROTO", 45.0, -65.0, 1.5, "F.SilkS", 0),
        ("DISPLAY 3.2-INCH (ER-TFTM032-3)", 45.0, -34.5, 1.3, "F.SilkS", 0),
        ("ZERO-SOLDER PCBA - PROTOTYPE-PENDING", 45.0, -3.5, 1.1, "F.SilkS", 0),
    ]:
        add_txt(txt, x, y, sz, lyr, rot)

    # Lepton outline box on B.SilkS
    lbox = [(28.0, -19.0), (57.5, -19.0), (57.5, -61.0), (28.0, -61.0)]
    for i in range(4):
        x1, y1 = lbox[i]
        x2, y2 = lbox[(i + 1) % 4]
        gr_blocks.append(
            f'\t(gr_line (start {x1} {y1}) (end {x2} {y2}) (stroke (width 0.2) (type solid)) (layer "B.SilkS") (uuid "{uid(f"lbox:{i}")}"))'
        )

    # Copper tracks & vias
    seg_blocks = []
    for idx_s, (x1, y1, x2, y2, w, lyr_idx, net_name) in enumerate(segments):
        lyr = "F.Cu" if lyr_idx == 0 else "B.Cu"
        seg_blocks.append(
            f'\t(segment (start {x1} {y1}) (end {x2} {y2}) (width {w}) (layer "{lyr}") (net {net_id[net_name]}) (uuid "{uid(f"s:{idx_s}")}"))'
        )
    via_blocks = []
    for idx_v, (vx, vy, net_name) in enumerate(vias):
        via_blocks.append(
            f'\t(via (at {vx} {vy}) (size 0.55) (drill 0.3) (layers "F.Cu" "B.Cu") (net {net_id[net_name]}) (uuid "{uid(f"v:{idx_v}")}"))'
        )

    # Full GND copper pours on BOTH F.Cu & B.Cu (island_removal_mode 0 = remove all isolated islands -> 0 DRC warnings)
    gnd_id = net_id["GND"]
    zone_blocks = []
    for lyr in ("F.Cu", "B.Cu"):
        zone_blocks.append(
            f'\t(zone (net {gnd_id}) (net_name "GND") (layer "{lyr}") (uuid "{uid("zone:" + lyr)}") '
            f'(hatch edge 0.5) (connect_pads yes (clearance 0.18)) (min_thickness 0.15) '
            f'(fill yes (thermal_gap 0.25) (thermal_bridge_width 0.25) (island_removal_mode 0)) '
            f'(polygon (pts (xy 0.4 -0.4) (xy {BOARD_W - 0.4} -0.4) (xy {BOARD_W - 0.4} {-BOARD_H + 0.4}) (xy 0.4 {-BOARD_H + 0.4}))))'
        )

    pcb_text = f"""(kicad_pcb
\t(version 20241229)
\t(generator "pcbnew")
\t(generator_version "9.0")
\t(general
\t\t(thickness 1.6)
\t\t(legacy_teardrops no)
\t)
\t(paper "A4")
\t(layers
\t\t(0 "F.Cu" signal)
\t\t(31 "B.Cu" signal)
\t\t(32 "B.Adhes" user "B.Adhesive")
\t\t(33 "F.Adhes" user "F.Adhesive")
\t\t(34 "B.Paste" user)
\t\t(35 "F.Paste" user)
\t\t(36 "B.SilkS" user "B.Silkscreen")
\t\t(37 "F.SilkS" user "F.Silkscreen")
\t\t(38 "B.Mask" user)
\t\t(39 "F.Mask" user)
\t\t(40 "Dwgs.User" user "User.Drawings")
\t\t(41 "Cmts.User" user "User.Comments")
\t\t(42 "Eco1.User" user "User.Eco1")
\t\t(43 "Eco2.User" user "User.Eco2")
\t\t(44 "Edge.Cuts" user)
\t\t(45 "Margin" user)
\t\t(46 "B.CrtYd" user "B.Courtyard")
\t\t(47 "F.CrtYd" user "F.Courtyard")
\t\t(48 "B.Fab" user)
\t\t(49 "F.Fab" user)
\t)
\t(setup
\t\t(pad_to_mask_clearance 0)
\t\t(allow_soldermask_bridges_in_footprints no)
\t\t(pcbplotparams (layerselection 0x00010fc_ffffffff) (plot_on_all_layers_selection 0x0000000_00000000))
\t)
{chr(10).join(net_decls)}
{chr(10).join(fp_blocks)}
{chr(10).join(gr_blocks)}
{chr(10).join(seg_blocks)}
{chr(10).join(via_blocks)}
{chr(10).join(zone_blocks)}
)
"""
    pcb_path = OUT_DIR / f"{STEM}.kicad_pcb"
    pcb_path.write_text(pcb_text, encoding="utf-8")
    subprocess.run([KICAD_CLI, "pcb", "upgrade", "--force", str(pcb_path)], check=True)
    return fp_pad_map


# ---------------------------------------------------------------------------
# 5. Build KiCad 10 Project, Schematic (.kicad_sch), and Local Lib Tables
# ---------------------------------------------------------------------------
def snap(v: float) -> float:
    return round(v / 2.54) * 2.54


def build_project_and_schematic(fp_pad_map: dict) -> None:
    # .kicad_pro
    pro = {
        "board": {
            "design_settings": {
                "defaults": {
                    "board_outline_line_width": 0.1,
                    "copper_line_width": 0.25,
                    "silk_line_width": 0.15,
                    "silk_text_size_h": 1.0,
                    "silk_text_size_v": 1.0,
                    "silk_text_thickness": 0.15,
                },
                "rules": {
                    "min_clearance": 0.127,
                    "min_copper_edge_clearance": 0.25,
                    "min_hole_clearance": 0.20,
                    "min_hole_to_hole": 0.20,
                    "min_through_hole_diameter": 0.20,
                    "min_track_width": 0.127,
                    "min_via_annular_width": 0.125,
                    "min_via_diameter": 0.50,
                    "solder_mask_clearance": 0.0,
                },
                "rule_severities": {
                    "silk_edge_clearance": "warning",
                    "silk_over_copper": "warning",
                    "silk_overlap": "warning",
                    "courtyards_overlap": "warning",
                },
            }
        },
        "erc": {"erc_exclusions": [], "meta": {"version": 0}, "rule_severities": {}},
        "meta": {"filename": f"{STEM}.kicad_pro", "version": 3},
        "net_settings": {
            "classes": [{
                "clearance": 0.15,
                "name": "Default",
                "track_width": 0.20,
                "via_diameter": 0.60,
                "via_drill": 0.30,
            }],
            "meta": {"version": 4},
        },
        "schematic": {"meta": {"version": 1}},
    }
    (OUT_DIR / f"{STEM}.kicad_pro").write_text(json.dumps(pro, indent=2) + "\n", encoding="utf-8")

    # Symbol generator (one symbol per unique footprint)
    sym_blocks = []
    sym_meta = {}
    for ref, val, fp_name, *_rest in COMPONENTS:
        _fp, _val, pad_nums, _nets = fp_pad_map[ref]
        if not pad_nums:
            # Mechanical NPTH hole (no electrical pins)
            continue
        key = f"{fp_name}_{len(pad_nums)}"
        if key in sym_meta:
            continue
        n = len(pad_nums)
        if n == 2 and fp_name in ("R_0603", "C_0603", "C_0805", "L_0630", "LED_0603", "FUSE_1206"):
            pin_specs = [(pad_nums[0], -5.08, 0.0, 0, 2.54), (pad_nums[1], 5.08, 0.0, 180, 2.54)]
            bx1, by1, bx2, by2 = -2.54, 1.016, 2.54, -1.016
            ref_y, val_y = 2.54, -2.54
        elif n <= 6:
            top_y = snap((n - 1) * 2.54 / 2.0)
            pin_specs = [(p, 7.62, round(top_y - i * 2.54, 4), 180, 2.54) for i, p in enumerate(pad_nums)]
            bx1, by1, bx2, by2 = -2.54, round(top_y + 2.54, 4), 5.08, round(top_y - (n - 1) * 2.54 - 2.54, 4)
            ref_y, val_y = round(by1 + 2.54, 4), round(by2 - 2.54, 4)
        else:
            left_n = (n + 1) // 2
            top_y = snap((left_n - 1) * 2.54 / 2.0)
            w_half = 10.16 if n >= 20 else 7.62
            pin_specs = []
            for i, p in enumerate(pad_nums[:left_n]):
                pin_specs.append((p, round(-w_half - 2.54, 4), round(top_y - i * 2.54, 4), 0, 2.54))
            for i, p in enumerate(pad_nums[left_n:]):
                pin_specs.append((p, round(w_half + 2.54, 4), round(top_y - i * 2.54, 4), 180, 2.54))
            bx1, by1, bx2, by2 = -w_half, round(top_y + 2.54, 4), w_half, round(top_y - (left_n - 1) * 2.54 - 2.54, 4)
            ref_y, val_y = round(by1 + 2.54, 4), round(by2 - 2.54, 4)

        lines = [
            f'\t\t(symbol "V40:{key}"',
            '\t\t\t(pin_names (offset 1.016))',
            '\t\t\t(exclude_from_sim no)',
            '\t\t\t(in_bom yes)',
            '\t\t\t(on_board yes)',
            f'\t\t\t(property "Reference" "U" (at 0 {ref_y} 0) (effects (font (size 1.27 1.27))))',
            f'\t\t\t(property "Value" "{key}" (at 0 {val_y} 0) (effects (font (size 1.27 1.27))))',
            f'\t\t\t(property "Footprint" "V40:{fp_name}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
            '\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
            f'\t\t\t(symbol "{key}_0_1"',
            f'\t\t\t\t(rectangle (start {bx1} {by1}) (end {bx2} {by2}) (stroke (width 0.254) (type default)) (fill (type background)))',
            '\t\t\t)',
            f'\t\t\t(symbol "{key}_1_1"',
        ]
        for pnum, px, py, prot, plen in pin_specs:
            lines.append(
                f'\t\t\t\t(pin passive line (at {px} {py} {rot if False else prot}) (length {plen}) '
                f'(name "{pnum}" (effects (font (size 1.016 1.016)))) '
                f'(number "{pnum}" (effects (font (size 1.016 1.016)))))'
            )
        lines.append('\t\t\t)')
        lines.append('\t\t)')
        sym_blocks.append("\n".join(lines))
        sym_meta[key] = (pin_specs, ref_y, val_y)

    # Clean grid placement on A1 sheet (841 x 594 mm) grouped by subsystem
    group_origins = {
        "usbc":    (63.50,  63.50),
        "charger": (63.50, 215.90),
        "boost":   (63.50, 381.00),
        "teensy":  (266.70, 127.00),
        "display": (482.60, 152.40),
        "lepton":  (482.60, 355.60),
        "io":      (635.00,  76.20),
        "sense":   (635.00, 330.20),
        "mech":    (635.00, 482.60),
    }
    group_cursors = {k: list(v) for k, v in group_origins.items()}

    root_uuid = uid("root_sheet")
    body = [
        f'\t(text "DIY-Thermocam V4.0 (Zero-Solder PCBA — PROTOTYPE-PENDING)" (exclude_from_sim no) (at 25.4 25.4 0) (effects (font (size 3.0 3.0) (bold yes)) (justify left)) (uuid "{uid("title")}"))',
        f'\t(text "USB-C + ESD" (exclude_from_sim no) (at 45.72 38.1 0) (effects (font (size 2.0 2.0) (bold yes)) (justify left)) (uuid "{uid("h_usbc")}"))',
        f'\t(text "TI BQ24075 Power-Path Charger" (exclude_from_sim no) (at 45.72 187.96 0) (effects (font (size 2.0 2.0) (bold yes)) (justify left)) (uuid "{uid("h_chg")}"))',
        f'\t(text "TI TPS61022 5V Synchronous Boost + Decoupling" (exclude_from_sim no) (at 45.72 355.6 0) (effects (font (size 2.0 2.0) (bold yes)) (justify left)) (uuid "{uid("h_boost")}"))',
        f'\t(text "Teensy 4.1 Sockets + Zero-Solder USB Pogo Pins" (exclude_from_sim no) (at 241.3 38.1 0) (effects (font (size 2.0 2.0) (bold yes)) (justify left)) (uuid "{uid("h_teensy")}"))',
        f'\t(text "3.2-inch Display & FLIR Lepton Sockets" (exclude_from_sim no) (at 457.2 38.1 0) (effects (font (size 2.0 2.0) (bold yes)) (justify left)) (uuid "{uid("h_disp")}"))',
        f'\t(text "Switches, LEDs, JST-PH LiPo & Sensing" (exclude_from_sim no) (at 609.6 38.1 0) (effects (font (size 2.0 2.0) (bold yes)) (justify left)) (uuid "{uid("h_io")}"))',
    ]

    for ref, val, fp_name, _x, _y, _rot, _side, nets, grp in COMPONENTS:
        _fp, _val, pad_nums, _n = fp_pad_map[ref]
        if not pad_nums:
            continue
        key = f"{fp_name}_{len(pad_nums)}"
        pin_specs, ry, vy = sym_meta[key]
        cx, cy = group_cursors[grp]
        sx, sy = snap(cx), snap(cy)
        # Advance cursor inside group cleanly
        if len(pad_nums) >= 20:
            group_cursors[grp][0] += 76.2
        elif len(pad_nums) >= 6:
            group_cursors[grp][0] += 55.88
        else:
            group_cursors[grp][0] += 38.1
            if group_cursors[grp][0] - group_origins[grp][0] > 140.0:
                group_cursors[grp][0] = group_origins[grp][0]
                group_cursors[grp][1] += 30.48

        ref_sy = round(sy - ry, 4)
        val_sy = round(sy - vy, 4)
        pin_uuids = "\n".join(f'\t\t(pin "{p}" (uuid "{uid(f"pin:{ref}:{p}")}"))' for p in pad_nums)
        body.append(f"""\t(symbol
\t\t(lib_id "V40:{key}")
\t\t(at {sx} {sy} 0)
\t\t(unit 1)
\t\t(exclude_from_sim no)
\t\t(in_bom yes)
\t\t(on_board yes)
\t\t(dnp no)
\t\t(uuid "{uid(f"sym:{ref}")}")
\t\t(property "Reference" "{ref}" (at {sx} {ref_sy} 0) (effects (font (size 1.27 1.27))))
\t\t(property "Value" "{val}" (at {sx} {val_sy} 0) (effects (font (size 1.27 1.27))))
\t\t(property "Footprint" "V40:{fp_name}" (at {sx} {sy} 0) (effects (font (size 1.27 1.27)) (hide yes)))
\t\t(property "Datasheet" "~" (at {sx} {sy} 0) (effects (font (size 1.27 1.27)) (hide yes)))
{pin_uuids}
\t\t(instances
\t\t\t(project "{STEM}"
\t\t\t\t(path "/{root_uuid}"
\t\t\t\t\t(reference "{ref}")
\t\t\t\t\t(unit 1)
\t\t\t\t)
\t\t\t)
\t\t)
\t)""")

        for pnum, lx, ly, prot, _plen in pin_specs:
            wx_ = round(sx + lx, 4)
            wy_ = round(sy - ly, 4)
            net = nets.get(pnum, "")
            if net:
                dx = -5.08 if prot == 0 else 5.08
                ex = round(wx_ + dx, 4)
                body.append(f'\t(wire (pts (xy {wx_} {wy_}) (xy {ex} {wy_})) (stroke (width 0) (type default)) (uuid "{uid(f"w:{ref}:{pnum}")}"))')
                lrot = 180 if prot == 0 else 0
                just = "right" if prot == 0 else "left"
                body.append(f'\t(global_label "{net}" (shape passive) (at {ex} {wy_} {lrot}) (effects (font (size 1.016 1.016)) (justify {just} bottom)) (uuid "{uid(f"lbl:{ref}:{pnum}")}"))')
            else:
                body.append(f'\t(no_connect (at {wx_} {wy_}) (uuid "{uid(f"nc:{ref}:{pnum}")}"))')

    sch_path = OUT_DIR / f"{STEM}.kicad_sch"
    sch_text = f"""(kicad_sch
\t(version 20250114)
\t(generator "eeschema")
\t(generator_version "9.0")
\t(uuid "{root_uuid}")
\t(paper "A1")
\t(title_block
\t\t(title "DIY-Thermocam V4.0 (Zero-Solder PCBA — PROTOTYPE-PENDING)")
\t\t(date "2026-11-25")
\t\t(rev "4.0-PROTO")
\t\t(company "DIY-Thermocam (GPL-3.0)")
\t)
\t(lib_symbols
{chr(10).join(sym_blocks)}
\t)
{chr(10).join(body)}
\t(sheet_instances
\t\t(path "/"
\t\t\t(page "1")
\t\t)
\t)
)
"""
    sch_path.write_text(sch_text, encoding="utf-8")

    # Project-local library tables
    (OUT_DIR / "sym-lib-table").write_text(
        '(sym_lib_table\n  (version 7)\n  (lib (name "V40")(type "KiCad")(uri "${KIPRJMOD}/V40.kicad_sym")(options "")(descr "DIY-Thermocam V4.0 symbols"))\n)\n',
        encoding="utf-8",
    )
    (OUT_DIR / "fp-lib-table").write_text(
        '(fp_lib_table\n  (version 7)\n  (lib (name "V40")(type "KiCad")(uri "${KIPRJMOD}/V40.pretty")(options "")(descr "DIY-Thermocam V4.0 footprints"))\n)\n',
        encoding="utf-8",
    )

    # Upgrade schematic to KiCad 10 native format, then extract matching V40.kicad_sym
    subprocess.run([KICAD_CLI, "sch", "upgrade", "--force", str(sch_path)], check=True)
    upgraded = sch_path.read_text(encoding="utf-8")
    start = upgraded.index("(lib_symbols")
    depth = 0
    end = start
    for i in range(start, len(upgraded)):
        if upgraded[i] == "(":
            depth += 1
        elif upgraded[i] == ")":
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    inner = upgraded[start + len("(lib_symbols") : end - 1]
    out_lines = []
    for line in inner.splitlines():
        if line.startswith("\t"):
            line = line[1:]
        line = re.sub(r'^(\t\(symbol )"V40:', r'\1"', line)
        out_lines.append(line)
    sym_path = OUT_DIR / "V40.kicad_sym"
    sym_path.write_text(
        '(kicad_symbol_lib\n\t(version 20241209)\n\t(generator "kicad_symbol_editor")\n\t(generator_version "10.0")\n'
        + "\n".join(out_lines).strip("\n")
        + "\n)\n",
        encoding="utf-8",
    )
    subprocess.run([KICAD_CLI, "sym", "upgrade", "--force", str(sym_path)], check=True)


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    print("1. Building self-contained V40.pretty footprint library...")
    build_local_footprints()
    print("2. Building and routing diy-thermocam-v4.0.kicad_pcb...")
    fp_pad_map = build_pcb()
    print("3. Building diy-thermocam-v4.0.kicad_pro, .kicad_sch, and V40.kicad_sym...")
    build_project_and_schematic(fp_pad_map)
    print("Done generating hardware/pcb/kicad-v4.0/!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
