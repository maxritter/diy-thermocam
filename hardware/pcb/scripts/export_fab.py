#!/usr/bin/env python3
"""Turnkey KiCad 10 fabrication & PCBA package exporter for DIY-Thermocam.

Generates ready-to-upload packages for Seeed Studio Fusion PCB/PCBA, JLCPCB,
and PCBWay from a KiCad 10 project directory:
  - Standard Gerber zip (F_Cu, B_Cu, F_Mask, B_Mask, F_Silkscreen, B_Silkscreen,
    F_Paste, B_Paste, Edge_Cuts) + Excellon .drl drills
  - PCBA BOM CSVs joined with hardware/bom/bom.csv (JLCPCB, Seeed Fusion, PCBWay)
  - Pick-and-Place CPL CSVs (JLCPCB & Seeed Fusion column conventions)
  - 3D STEP model
  - Schematic PDF
  - Board top & bottom SVG and 3D PNG renders
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

KICAD_CLI = os.environ.get(
    "KICAD_CLI",
    "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli",
)

# Normalize V3.0 KiCad-imported Eagle refdes back to hardware/bom/bom.csv designators.
REF_ALIASES = {
    "SWITCH0": "SWITCH",
    "BUTTON0": "BUTTON",
    "CHG-LED0": "CHG-LED",
    "LCD-HEADER0": "LCD-HEADER",
    "PROGRAM0": "PROGRAM",
    "BOOSTER0": "BOOSTER",
    "BAT0": "BAT",
}


def run_cmd(cmd: list[str]) -> None:
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(
            f"Command failed ({proc.returncode}): {' '.join(cmd)}\n"
            f"STDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
        )


# V4.0 Zero-Solder PCBA consolidated LCSC / Seeed OPL lookup (overrides/extends hardware/bom/bom.csv
# until the post-merge BOM update adds v4.0 rows directly into hardware/bom/bom.csv).
V40_BOM_OVERRIDES: dict[str, dict[str, str]] = {
    "J1":   {"mpn": "2.54-2*20",            "manufacturer": "ZHOURI",              "lcsc_part": "C2977589",  "seeed_opl_part": "", "pcba_tier": "extended"},
    "J2":   {"mpn": "DS1023-02-2x10SF11",   "manufacturer": "CONNFLY Elec",        "lcsc_part": "C18164970", "seeed_opl_part": "", "pcba_tier": "extended"},
    "J3":   {"mpn": "TYPE-C-31-M-12",       "manufacturer": "Korean Hroparts Elec","lcsc_part": "C165948",   "seeed_opl_part": "", "pcba_tier": "extended"},
    "J4":   {"mpn": "S2B-PH-SM4-TB(LF)(SN)","manufacturer": "JST",                 "lcsc_part": "C295747",   "seeed_opl_part": "", "pcba_tier": "extended"},
    "J5":   {"mpn": "HC-PM254-8.5H-1x24PZ", "manufacturer": "Hong Cheng",          "lcsc_part": "C41376163", "seeed_opl_part": "", "pcba_tier": "extended"},
    "J6":   {"mpn": "HC-PM254-8.5H-1x24PZ", "manufacturer": "Hong Cheng",          "lcsc_part": "C41376163", "seeed_opl_part": "", "pcba_tier": "extended"},
    "J7":   {"mpn": "2.54-1*5PFemale",      "manufacturer": "BOOMELE",             "lcsc_part": "C50950",    "seeed_opl_part": "", "pcba_tier": "extended"},
    "P1":   {"mpn": "YTC1P-8535-01",        "manufacturer": "YIYUAN",              "lcsc_part": "C5221289",  "seeed_opl_part": "", "pcba_tier": "extended"},
    "P2":   {"mpn": "YTC1P-8535-01",        "manufacturer": "YIYUAN",              "lcsc_part": "C5221289",  "seeed_opl_part": "", "pcba_tier": "extended"},
    "D1":   {"mpn": "USBLC6-2SC6",          "manufacturer": "STMicroelectronics",  "lcsc_part": "C7519",     "seeed_opl_part": "", "pcba_tier": "extended"},
    "D2":   {"mpn": "KT-0603R",             "manufacturer": "Hubei KENTO Elec",    "lcsc_part": "C2286",     "seeed_opl_part": "", "pcba_tier": "basic"},
    "U2":   {"mpn": "TPS61022RWUR",         "manufacturer": "Texas Instruments",   "lcsc_part": "C915088",   "seeed_opl_part": "", "pcba_tier": "extended"},
    "U3":   {"mpn": "BQ24074RGTR",          "manufacturer": "Texas Instruments",   "lcsc_part": "C54313",    "seeed_opl_part": "", "pcba_tier": "extended"},
    "U4":   {"mpn": "XC6206P332MR-G",       "manufacturer": "Torex Semicon",       "lcsc_part": "C5446",     "seeed_opl_part": "", "pcba_tier": "basic"},
    "L1":   {"mpn": "FXL0630-1R0-M",        "manufacturer": "cjiang",              "lcsc_part": "C167216",   "seeed_opl_part": "", "pcba_tier": "extended"},
    "Q1":   {"mpn": "AO3401A",              "manufacturer": "Alpha & Omega",       "lcsc_part": "C15127",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "Q2":   {"mpn": "AO3400A",              "manufacturer": "Alpha & Omega",       "lcsc_part": "C20917",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "F1":   {"mpn": "SMD1206P200TF",        "manufacturer": "RUILON",              "lcsc_part": "C20988",    "seeed_opl_part": "", "pcba_tier": "extended"},
    "SW1":  {"mpn": "JS102011SAQN",         "manufacturer": "C&K",                 "lcsc_part": "C221660",   "seeed_opl_part": "", "pcba_tier": "extended"},
    "SW2":  {"mpn": "B3U-3000P",            "manufacturer": "Omron Electronics",   "lcsc_part": "C963349",   "seeed_opl_part": "", "pcba_tier": "extended"},
    "SW3":  {"mpn": "TS-1187A-B-A-B",       "manufacturer": "XKB Connection",      "lcsc_part": "C318884",   "seeed_opl_part": "", "pcba_tier": "basic"},
    "C1":   {"mpn": "CL21A106KAYNNNE",      "manufacturer": "Samsung",             "lcsc_part": "C15850",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "C2":   {"mpn": "CL21A106KAYNNNE",      "manufacturer": "Samsung",             "lcsc_part": "C15850",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "C3":   {"mpn": "CL21A106KAYNNNE",      "manufacturer": "Samsung",             "lcsc_part": "C15850",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "C5":   {"mpn": "CL21A106KAYNNNE",      "manufacturer": "Samsung",             "lcsc_part": "C15850",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "C10":  {"mpn": "CL21A106KAYNNNE",      "manufacturer": "Samsung",             "lcsc_part": "C15850",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "C4":   {"mpn": "CL21A226MAQNNNE",      "manufacturer": "Samsung",             "lcsc_part": "C45783",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "C8":   {"mpn": "CL21A226MAQNNNE",      "manufacturer": "Samsung",             "lcsc_part": "C45783",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "C9":   {"mpn": "CL21A226MAQNNNE",      "manufacturer": "Samsung",             "lcsc_part": "C45783",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "C6":   {"mpn": "CC0603KRX7R9BB104",    "manufacturer": "YAGEO",               "lcsc_part": "C14663",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "C7":   {"mpn": "CC0603KRX7R9BB104",    "manufacturer": "YAGEO",               "lcsc_part": "C14663",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "C11":  {"mpn": "CC0603KRX7R9BB104",    "manufacturer": "YAGEO",               "lcsc_part": "C14663",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R1":   {"mpn": "0603WAF4702T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25819",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R4":   {"mpn": "0603WAF4701T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C23162",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R6":   {"mpn": "0603WAF4701T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C23162",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R2":   {"mpn": "0603WAF1003T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25803",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R3":   {"mpn": "0603WAF1003T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25803",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R5":   {"mpn": "0603WAF1003T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25803",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R9":   {"mpn": "0603WAF1002T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25804",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R19":  {"mpn": "0603WAF1002T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25804",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R22":  {"mpn": "0603WAF1002T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25804",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R23":  {"mpn": "0603WAF3001T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C4211",     "seeed_opl_part": "", "pcba_tier": "basic"},
    "R7":   {"mpn": "0603WAF5101T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C23186",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R8":   {"mpn": "0603WAF5101T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C23186",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R10":  {"mpn": "0603WAF1001T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C21190",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R11":  {"mpn": "0603WAF1201T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C22765",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R12":  {"mpn": "0603WAF1001T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C21190",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R14":  {"mpn": "0603WAF4702T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25819",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R15":  {"mpn": "0603WAF1003T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25803",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R17":  {"mpn": "0603WAF1003T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25803",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R18":  {"mpn": "0603WAF7503T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C23240",    "seeed_opl_part": "", "pcba_tier": "extended"},
    "R20":  {"mpn": "0603WAF4702T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C25819",    "seeed_opl_part": "", "pcba_tier": "basic"},
    "R21":  {"mpn": "0603WAF7503T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C23240",    "seeed_opl_part": "", "pcba_tier": "extended"},
    "R16":  {"mpn": "0603WAF7503T5E",       "manufacturer": "UNI-ROYAL",           "lcsc_part": "C23240",    "seeed_opl_part": "", "pcba_tier": "extended"},
}


def load_master_bom(bom_csv: Path, rev: str) -> dict[str, dict]:
    """Map designator -> master BOM row for the target hardware revision."""
    by_des: dict[str, dict] = {}
    with bom_csv.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            hw = row["hw_rev"].strip().lower()
            if hw not in ("all", rev.lower()):
                continue
            raw_des = row.get("designators", "").strip()
            if not raw_des:
                continue
            for des in [d.strip() for d in raw_des.split(",") if d.strip()]:
                # Prefer revision-specific PCB component rows over offboard/generic rows
                prev = by_des.get(des)
                if prev is None or (hw == rev.lower() and prev["hw_rev"].strip().lower() != rev.lower()):
                    by_des[des] = row
                elif prev["pcba_tier"] == "offboard" and row["pcba_tier"] != "offboard":
                    by_des[des] = row
    if rev.lower() == "v4.0":
        for des, ov in V40_BOM_OVERRIDES.items():
            by_des[des] = {**by_des.get(des, {}), **ov}
    return by_des


def parse_pcb_footprints(pcb_path: Path) -> list[dict]:
    """Extract placed footprints (Reference, Value, Footprint, Layer, SMD/THT) from .kicad_pcb."""
    text = pcb_path.read_text(encoding="utf-8")
    fps = []
    for block in text.split("\n\t(footprint ")[1:]:
        fp_name = re.match(r'"([^"]+)"', block).group(1)
        ref = re.search(r'\(property "Reference" "([^"]*)"', block).group(1)
        val = re.search(r'\(property "Value" "([^"]*)"', block).group(1)
        if "exclude_from_bom" in block or "DNP" in val.upper():
            continue
        lm = re.search(r'\(layer "([^"]+)"\)', block)
        layer = lm.group(1) if lm else "F.Cu"
        # Respect footprint (attr smd) so QFN thermal vias (U3) & USB-C shield tabs (J3) stay SMD
        has_attr_smd = bool(re.search(r'\(attr\s+[^\)]*\bsmd\b', block))
        is_smd = has_attr_smd or ("(pad " in block and " smd " in block and " thru_hole " not in block)
        fps.append({
            "ref": ref,
            "bom_ref": REF_ALIASES.get(ref, ref),
            "value": val or fp_name.split(":")[-1],
            "footprint": fp_name.split(":")[-1],
            "layer": "top" if layer == "F.Cu" else "bottom",
            "mount": "SMD" if is_smd else "THT",
        })
    return sorted(fps, key=lambda x: x["ref"])


def write_boms(fps: list[dict], master_bom: dict[str, dict], fab_dir: Path, stem: str) -> None:
    # Group by (value, footprint, lcsc, mpn)
    groups: dict[tuple, list[dict]] = {}
    for fp in fps:
        mrow = master_bom.get(fp["bom_ref"], {})
        lcsc = mrow.get("lcsc_part", "").strip()
        opl = mrow.get("seeed_opl_part", "").strip()
        mpn = mrow.get("mpn", "").strip()
        mfr = mrow.get("manufacturer", "").strip()
        tier = mrow.get("pcba_tier", "tht_manual" if fp["mount"] == "THT" else "extended").strip()
        val = fp["value"]
        key = (val, fp["footprint"], lcsc, opl, mpn, mfr, fp["mount"], tier)
        groups.setdefault(key, []).append(fp)

    # 1. JLCPCB BOM format
    jlc_path = fab_dir / f"{stem}-bom-jlcpcb.csv"
    with jlc_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (val, fp_name, lcsc, _opl, _mpn, _mfr, _mount, _tier), items in groups.items():
            des = ",".join(i["ref"] for i in items)
            w.writerow([val, des, fp_name, lcsc])

    # 2. Seeed Studio Fusion PCBA BOM format
    seeed_path = fab_dir / f"{stem}-bom-seeed.csv"
    with seeed_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "MPN", "Qty", "Seeed OPL", "LCSC", "Value", "Footprint", "Mount", "PCBA Tier"])
        for (val, fp_name, lcsc, opl, mpn, _mfr, mount, tier), items in groups.items():
            des = ",".join(i["ref"] for i in items)
            w.writerow([des, mpn, len(items), opl, lcsc, val, fp_name, mount, tier])

    # 3. PCBWay BOM format
    pcbway_path = fab_dir / f"{stem}-bom-pcbway.csv"
    with pcbway_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Item #", "Designator", "Qty", "Manufacturer", "Mfg Part #", "Description / Value", "Package/Footprint", "Type"])
        for idx, ((val, fp_name, _lcsc, _opl, mpn, mfr, mount, _tier), items) in enumerate(groups.items(), 1):
            des = ",".join(i["ref"] for i in items)
            w.writerow([idx, des, len(items), mfr, mpn, val, fp_name, mount])


# Vendor-basis JLCPCB/EasyEDA CPL calibration for V4.0 (dx_mm, dy_mm in PosXY [Y-up], drot_deg)
# Applied AFTER the standard JLCPCB bottom-side (180 - angle) transform (kicad-jlcpcb-tools / KiKit convention).
# Verified against EasyEDA component footprints (C7519, C15127, C20917, C5446, C221660, C165948, C915088, C54313, C295747, C963349).
V40_CPL_CALIBRATION: dict[str, tuple[float, float, float]] = {
    "Q1":  ( 0.000,  0.000, 180.0),  # C15127 SOT-23: bottom (180-0)+180 = 0 deg
    "Q2":  ( 0.000,  0.000, 180.0),  # C20917 SOT-23: bottom (180-0)+180 = 0 deg
    "U4":  ( 0.000,  0.000, 180.0),  # C5446  SOT-23: bottom (180-0)+180 = 0 deg
    "D1":  ( 0.000,  0.000, 270.0),  # C7519  SOT-23-6: bottom (180-0)+270 = 90 deg
    "SW1": (-2.750,  0.000,   0.0),  # C221660 EasyEDA origin at middle terminal (Pin 2); bottom (180-270) = 270 deg
    "J3":  ( 0.000,  1.570,   0.0),  # C165948 EasyEDA origin 1.57 mm along terminal axis from KiCad origin
    "U2":  ( 0.175,  0.000,   0.0),  # C915088 EasyEDA symmetric pad origin (+0.175 mm X); bottom (180-0) = 180 deg
    "J5":  ( 0.000,  0.000, 270.0),  # C41376163 horizontal EasyEDA basis: bottom (180-0)+270 = 90 deg
    "J6":  ( 0.000,  0.000, 270.0),  # C41376163 horizontal EasyEDA basis: bottom (180-0)+270 = 90 deg
    "J7":  ( 0.000,  0.000,  90.0),  # C50950 horizontal EasyEDA basis: bottom (180-270)+90 = 0 deg
}


def compute_pad_midpoints(pcb_path: Path) -> dict[str, tuple[float, float]]:
    """Compute true geometric pad-bounding-box midpoints in PosXY (X, -Y) for multi-pin headers."""
    text = pcb_path.read_text(encoding="utf-8")
    mids: dict[str, tuple[float, float]] = {}
    for block in text.split("\n\t(footprint ")[1:]:
        rm = re.search(r'\(property "Reference" "([^"]*)"', block)
        am = re.search(r'\n\t\t\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?\)', block)
        if not rm or not am:
            continue
        ref = rm.group(1)
        if ref not in ("J1", "J2", "J5", "J6", "J7"):
            continue
        fx, fy = float(am.group(1)), float(am.group(2))
        rot = float(am.group(3) or 0.0)
        import math
        rad = math.radians(rot)
        xs, ys = [], []
        for pm in re.finditer(r'\(pad "[^"]+" \w+ \w+\s+\(at\s+([-\d.]+)\s+([-\d.]+)', block):
            px, py = float(pm.group(1)), float(pm.group(2))
            wx = fx + px * math.cos(rad) + py * math.sin(rad)
            wy = fy - px * math.sin(rad) + py * math.cos(rad)
            xs.append(wx)
            ys.append(-wy)  # PosXY has Y-up = -KiCad_Y
        if xs:
            mids[ref] = (round((min(xs) + max(xs)) / 2.0, 4), round((min(ys) + max(ys)) / 2.0, 4))
    return mids


def write_cpls(raw_pos_csv: Path, fab_dir: Path, stem: str, rev: str = "", pad_mids: dict[str, tuple[float, float]] | None = None) -> None:
    rows = []
    with raw_pos_csv.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            rows.append(row)

    cal_rows = []
    for r in rows:
        ref = r["Ref"]
        side_l = r["Side"].strip().lower()
        px, py, rot = float(r["PosX"]), float(r["PosY"]), float(r["Rot"])
        if rev.lower() == "v4.0":
            if side_l == "bottom":
                # Standard JLCPCB bottom-side transform (kicad-jlcpcb-tools / KiKit convention)
                rot = (180.0 - rot) % 360.0
            if pad_mids and ref in pad_mids:
                px, py = pad_mids[ref]
            if ref in V40_CPL_CALIBRATION:
                dx, dy, drot = V40_CPL_CALIBRATION[ref]
                px += dx
                py += dy
                rot = (rot + drot) % 360.0
        cal_rows.append((ref, px, py, side_l, rot))

    # JLCPCB CPL: Designator,Mid X,Mid Y,Layer,Rotation
    jlc_cpl = fab_dir / f"{stem}-cpl-jlcpcb.csv"
    with jlc_cpl.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for ref, px, py, side_l, rot in cal_rows:
            side = "Top" if side_l == "top" else "Bottom"
            w.writerow([ref, f"{px:.4f}mm", f"{py:.4f}mm", side, f"{rot:.2f}"])

    # Seeed Fusion CPL: Designator,PosX,PosY,Layer,Rot
    seeed_cpl = fab_dir / f"{stem}-cpl-seeed.csv"
    with seeed_cpl.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "PosX", "PosY", "Layer", "Rot"])
        for ref, px, py, side_l, rot in cal_rows:
            side = "T" if side_l == "top" else "B"
            w.writerow([ref, f"{px:.4f}", f"{py:.4f}", side, f"{rot:.2f}"])


def export_revision(pcb_root: Path, rev: str = "v4.0", skip_render: bool = False) -> None:
    proj_dir = pcb_root
    stem = f"diy-thermocam-{rev}"
    sch_path = proj_dir / f"{stem}.kicad_sch"
    pcb_path = proj_dir / f"{stem}.kicad_pcb"
    bom_csv = pcb_root.parent / "bom" / "bom.csv"

    if not pcb_path.exists():
        raise FileNotFoundError(f"Missing PCB file: {pcb_path}")

    fab_dir = proj_dir / "fab"
    gbr_dir = fab_dir / "gerbers"
    if gbr_dir.exists():
        shutil.rmtree(gbr_dir)
    gbr_dir.mkdir(parents=True, exist_ok=True)

    print(f"=== Exporting {rev} fabrication package -> {fab_dir} ===")

    # 1. Standard Gerbers + Excellon drills
    run_cmd([
        KICAD_CLI, "pcb", "export", "gerbers",
        "--layers", "F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,F.Paste,B.Paste,Edge.Cuts",
        "--check-zones",
        "-o", str(gbr_dir) + "/",
        str(pcb_path),
    ])
    run_cmd([
        KICAD_CLI, "pcb", "export", "drill",
        "-u", "mm",
        "--generate-map", "--map-format", "pdf",
        "-o", str(gbr_dir) + "/",
        str(pcb_path),
    ])

    zip_path = fab_dir / f"{stem}-gerbers.zip"
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for p in sorted(gbr_dir.iterdir()):
            if p.suffix.lower() in {".gtl", ".gbl", ".gts", ".gbs", ".gto", ".gbo", ".gtp", ".gbp", ".gm1", ".gbr", ".drl", ".gbrjob"}:
                zf.write(p, arcname=p.name)
    print(f"  [OK] Gerbers + drills zip: {zip_path.name}")

    # 2. Position / CPL CSVs
    raw_pos = fab_dir / f"{stem}-pos-raw.csv"
    run_cmd([
        KICAD_CLI, "pcb", "export", "pos",
        "--side", "both", "--format", "csv", "--units", "mm",
        "-o", str(raw_pos),
        str(pcb_path),
    ])
    pad_mids = compute_pad_midpoints(pcb_path) if rev.lower() == "v4.0" else None
    write_cpls(raw_pos, fab_dir, stem, rev=rev, pad_mids=pad_mids)
    raw_pos.unlink(missing_ok=True)
    print(f"  [OK] CPL CSVs (JLCPCB & Seeed Fusion)")

    # 3. PCBA BOM CSVs joined with master hardware/bom/bom.csv
    master_bom = load_master_bom(bom_csv, rev)
    fps = parse_pcb_footprints(pcb_path)
    write_boms(fps, master_bom, fab_dir, stem)
    print(f"  [OK] BOM CSVs (JLCPCB, Seeed Fusion, PCBWay) for {len(fps)} footprints")

    # 4. 3D STEP model
    step_path = fab_dir / f"{stem}.step"
    run_cmd([
        KICAD_CLI, "pcb", "export", "step",
        "--subst-models",
        "--force",
        "-o", str(step_path),
        str(pcb_path),
    ])
    print(f"  [OK] 3D STEP: {step_path.name} ({step_path.stat().st_size // 1024} KiB)")

    # 5. Schematic PDF
    if sch_path.exists():
        pdf_path = fab_dir / f"{stem}-schematic.pdf"
        run_cmd([
            KICAD_CLI, "sch", "export", "pdf",
            "-o", str(pdf_path),
            str(sch_path),
        ])
        print(f"  [OK] Schematic PDF: {pdf_path.name}")

    # 6. Board top & bottom SVG renders
    run_cmd([
        KICAD_CLI, "pcb", "export", "svg",
        "--layers", "F.Cu,F.Silkscreen,F.Mask,Edge.Cuts",
        "--page-size-mode", "2",
        "-o", str(fab_dir / f"{stem}-top.svg"),
        str(pcb_path),
    ])
    run_cmd([
        KICAD_CLI, "pcb", "export", "svg",
        "--layers", "B.Cu,B.Silkscreen,B.Mask,Edge.Cuts",
        "--mirror",
        "--page-size-mode", "2",
        "-o", str(fab_dir / f"{stem}-bottom.svg"),
        str(pcb_path),
    ])
    print(f"  [OK] Top/Bottom SVG renders")

    # 7. High-res 3D PNG previews
    if not skip_render:
        for side in ("top", "bottom"):
            out_png = fab_dir / f"{stem}-{side}.png"
            run_cmd([
                KICAD_CLI, "pcb", "render",
                "--side", side,
                "--width", "1600", "--height", "1240",
                "--quality", "high",
                "--background", "opaque",
                "--use-board-stackup-colors",
                "-o", str(out_png),
                str(pcb_path),
            ])
        print(f"  [OK] Top/Bottom 3D PNG renders")


def main() -> int:
    parser = argparse.ArgumentParser(description="Export KiCad 10 fabrication packages for DIY-Thermocam.")
    parser.add_argument("--rev", choices=["v4.0", "all"], default="v4.0", help="PCB revision to export")
    parser.add_argument("--skip-render", action="store_true", help="Skip 3D PNG raytrace rendering")
    args = parser.parse_args()

    pcb_root = Path(__file__).resolve().parent.parent
    rev = "v4.0"
    if not (pcb_root / f"diy-thermocam-{rev}.kicad_pcb").exists():
        raise FileNotFoundError(f"Missing {pcb_root / f'diy-thermocam-{rev}.kicad_pcb'}")
    export_revision(pcb_root, rev, skip_render=args.skip_render)
    return 0


if __name__ == "__main__":
    sys.exit(main())
