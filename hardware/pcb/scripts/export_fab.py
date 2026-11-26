#!/usr/bin/env python3
"""Export the DIY-Thermocam V4.0 manufacturing files.

    python3 hardware/pcb/scripts/export_fab.py [--skip-render]

Needs KiCad 10 (set KICAD_CLI if kicad-cli is not in the default macOS location).
Part numbers come only from hardware/bom/bom.csv.

Writes hardware/pcb/fab/:
    jlcpcb/   gerbers.zip, bom.csv (LCSC numbers), cpl.csv (rotations and offsets for JLCPCB's part library)
    seeed/    gerbers.zip, bom.csv (Seeed Fusion template), cpl.csv (KiCad centroids)
    pcbway/   gerbers.zip, bom.csv (PCBWay template), cpl.csv (KiCad centroids)
    assembly-top.pdf, assembly-bottom.pdf, schematic.pdf, .step, top/bottom renders (.png)
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

KICAD_CLI = os.environ.get("KICAD_CLI", "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli")
STEM = "diy-thermocam-v4.0"
VENDORS = ("jlcpcb", "seeed", "pcbway")
GERBER_LAYERS = "F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,F.Paste,B.Paste,Edge.Cuts"

# JLCPCB places parts from its own (EasyEDA) footprint library. Where that footprint's origin or
# zero orientation differs from ours, this table corrects the JLCPCB CPL only:
# (dx_mm, dy_mm in CPL coordinates, extra rotation in degrees), applied after the bottom-side
# transform rot' = 180 - rot. Checked against the EasyEDA footprints of the listed LCSC parts.
JLC_CPL_CORRECTION: dict[str, tuple[float, float, float]] = {
    "Q1": (0.0, 0.0, 180.0),     # C15127 SOT-23: EasyEDA pin 3 on the left
    "Q2": (0.0, 0.0, 180.0),     # C20917 SOT-23
    "U4": (0.0, 0.0, 180.0),     # C5446 SOT-23
    "D1": (0.0, 0.0, 270.0),     # C7519 SOT-23-6: EasyEDA pin 1 bottom-left
    "SW1": (-2.75, 0.0, 0.0),    # C221660: EasyEDA origin on the middle terminal
    "J3": (0.0, 1.57, 0.0),      # C165948: EasyEDA origin 1.57 mm towards the contacts
    "U2": (0.175, 0.0, 0.0),     # C915088: EasyEDA origin offset 0.175 mm
    "J1": (0.0, 0.0, 90.0),      # 2x20 socket: EasyEDA HDR-TH_40P drawn with its rows horizontal
    "J5": (0.0, 0.0, 270.0),     # C41376163: EasyEDA 1x24 drawn horizontally
    "J6": (0.0, 0.0, 270.0),     # C41376163
}

# Multi-pin headers whose footprint origin sits on pin 1: report the pad centre instead.
PAD_CENTRE_REFS = ("J1", "J2", "J5", "J6")


Row = dict[str, str]
Part = dict[str, str]


def need(pattern: str, text: str) -> re.Match[str]:
    m = re.search(pattern, text)
    if m is None:
        raise SystemExit(f"Unexpected .kicad_pcb content: no match for {pattern!r}")
    return m


def run(cmd: list[str]) -> None:
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"Command failed: {' '.join(cmd)}\n{proc.stdout}\n{proc.stderr}")


def load_bom(bom_csv: Path) -> dict[str, Row]:
    """Designator -> bom.csv row. Board parts (v4.0 rows) win over shared off-board module rows."""
    by_des: dict[str, Row] = {}
    with bom_csv.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if row["hw_rev"].strip() not in ("all", "v4.0"):
                continue
            for des in (d.strip() for d in row["designators"].split(",")):
                if not des:
                    continue
                prev = by_des.get(des)
                if prev is None or (prev["pcba_tier"] == "offboard" and row["pcba_tier"] != "offboard"):
                    by_des[des] = row
    return by_des


def placed_footprints(pcb_path: Path) -> list[Part]:
    """Footprints that get assembled: reference, value, footprint name, side, SMD or THT."""
    text = pcb_path.read_text(encoding="utf-8")
    fps: list[Part] = []
    for block in text.split("\n\t(footprint ")[1:]:
        name = need(r'^"([^"]+)"', block).group(1).split(":")[-1]
        ref = need(r'\(property "Reference" "([^"]*)"', block).group(1)
        val = need(r'\(property "Value" "([^"]*)"', block).group(1)
        attr = re.search(r'\n\t\t\(attr ([^)]*)\)', block)
        attrs = attr.group(1).split() if attr else []
        if "exclude_from_bom" in attrs or "dnp" in attrs:
            continue
        layer = need(r'\(layer "([^"]+)"\)', block).group(1)
        fps.append({
            "ref": ref,
            "value": val,
            "footprint": name,
            "side": "Top" if layer == "F.Cu" else "Bottom",
            "mount": "SMD" if "smd" in attrs else "THT",
        })
    return sorted(fps, key=lambda f: (re.sub(r"\d", "", f["ref"]), int(re.sub(r"\D", "", f["ref"]) or 0)))


def pad_centres(pcb_path: Path) -> dict[str, tuple[float, float]]:
    """Centre of the pad bounding box (CPL coordinates, Y up) for multi-pin headers."""
    text = pcb_path.read_text(encoding="utf-8")
    mids: dict[str, tuple[float, float]] = {}
    for block in text.split("\n\t(footprint ")[1:]:
        ref = need(r'\(property "Reference" "([^"]*)"', block).group(1)
        if ref not in PAD_CENTRE_REFS:
            continue
        at = need(r'\n\t\t\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?\)', block)
        fx, fy, rot = float(at.group(1)), float(at.group(2)), float(at.group(3) or 0.0)
        rad = math.radians(rot)
        xs: list[float] = []
        ys: list[float] = []
        for pm in re.finditer(r'\(pad "[^"]+" \w+ \w+\s+\(at\s+([-\d.]+)\s+([-\d.]+)', block):
            px, py = float(pm.group(1)), float(pm.group(2))
            xs.append(fx + px * math.cos(rad) + py * math.sin(rad))
            ys.append(-(fy - px * math.sin(rad) + py * math.cos(rad)))
        mids[ref] = ((min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0)
    return mids


def bom_groups(fps: list[Part], bom: dict[str, Row]) -> list[tuple[Row, list[Part]]]:
    """Group placed footprints by part; fail if any part lacks an MPN or LCSC number."""
    missing = [f["ref"] for f in fps if not bom.get(f["ref"], {}).get("mpn") or not bom.get(f["ref"], {}).get("lcsc_part")]
    if missing:
        raise SystemExit(f"hardware/bom/bom.csv has no MPN/LCSC number for: {', '.join(missing)}")
    groups: dict[tuple[str, str], tuple[Row, list[Part]]] = {}
    for f in fps:
        row = bom[f["ref"]]
        key = (row["lcsc_part"], f["footprint"])
        groups.setdefault(key, (row, []))[1].append(f)
    return list(groups.values())


def write_csv(path: Path, header: list[str], rows: list[list[object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def write_boms(fab: Path, groups: list[tuple[Row, list[Part]]]) -> None:
    jlc: list[list[object]] = []
    seeed: list[list[object]] = []
    pcbway: list[list[object]] = []
    for i, (row, items) in enumerate(groups, 1):
        refs = ",".join(f["ref"] for f in items)
        lcsc = row["lcsc_part"]
        link = f"https://www.lcsc.com/product-detail/{lcsc}.html"
        jlc.append([row["mpn"], refs, items[0]["footprint"], lcsc])
        seeed.append([refs, row["mpn"], len(items), link])
        pcbway.append([i, refs, len(items), row["manufacturer"], row["mpn"], f"{row['name']} ({row['value_spec']})",
                       items[0]["footprint"], items[0]["mount"], f"LCSC {lcsc}: {link}"])
    write_csv(fab / "jlcpcb" / f"{STEM}-bom.csv", ["Comment", "Designator", "Footprint", "LCSC Part #"], jlc)
    write_csv(fab / "seeed" / f"{STEM}-bom.csv", ["Designator", "MPN", "Qty", "Part Link"], seeed)
    write_csv(fab / "pcbway" / f"{STEM}-bom.csv",
              ["Item #", "Designator", "Qty", "Manufacturer", "Mfg Part #", "Description / Value",
               "Package/Footprint", "Type", "Your Instructions / Notes"], pcbway)


def write_cpls(fab: Path, pos_csv: Path, fps: list[Part], centres: dict[str, tuple[float, float]]) -> None:
    placed = {f["ref"] for f in fps}
    jlc: list[list[object]] = []
    generic: list[list[object]] = []
    with pos_csv.open(newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            ref = r["Ref"]
            if ref not in placed:
                continue
            side = "Top" if r["Side"].strip().lower() == "top" else "Bottom"
            x, y, rot = float(r["PosX"]), float(r["PosY"]), float(r["Rot"])
            if ref in centres:
                x, y = centres[ref]
            generic.append([ref, f"{x:.4f}mm", f"{y:.4f}mm", side, f"{rot % 360:.2f}"])
            jrot = (180.0 - rot) % 360.0 if side == "Bottom" else rot
            dx, dy, drot = JLC_CPL_CORRECTION.get(ref, (0.0, 0.0, 0.0))
            jlc.append([ref, f"{x + dx:.4f}mm", f"{y + dy:.4f}mm", side, f"{(jrot + drot) % 360:.2f}"])
    if len(generic) != len(placed):
        raise SystemExit(f"Position file is missing: {sorted(placed - {g[0] for g in generic})}")
    header = ["Designator", "Mid X", "Mid Y", "Layer", "Rotation"]
    write_csv(fab / "jlcpcb" / f"{STEM}-cpl.csv", header, jlc)
    for vendor in ("seeed", "pcbway"):
        write_csv(fab / vendor / f"{STEM}-cpl.csv", header, generic)


def main() -> int:
    parser = argparse.ArgumentParser(description="Export DIY-Thermocam V4.0 manufacturing files.")
    parser.add_argument("--skip-render", action="store_true", help="skip the 3D PNG renders")
    args = parser.parse_args()

    pcb_dir = Path(__file__).resolve().parent.parent
    pcb = pcb_dir / f"{STEM}.kicad_pcb"
    sch = pcb_dir / f"{STEM}.kicad_sch"
    fab = pcb_dir / "fab"
    bom = load_bom(pcb_dir.parent / "bom" / "bom.csv")

    if fab.exists():
        shutil.rmtree(fab)
    fab.mkdir()

    with tempfile.TemporaryDirectory() as tmp:
        tmpd = Path(tmp)
        gbr = tmpd / "gerbers"
        gbr.mkdir()
        run([KICAD_CLI, "pcb", "export", "gerbers", "--layers", GERBER_LAYERS, "--check-zones", "-o", f"{gbr}/", str(pcb)])
        run([KICAD_CLI, "pcb", "export", "drill", "-u", "mm", "--excellon-separate-th", "-o", f"{gbr}/", str(pcb)])
        zip_path = tmpd / f"{STEM}-gerbers.zip"
        with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
            for p in sorted(gbr.iterdir()):
                zf.write(p, arcname=p.name)
        for vendor in VENDORS:
            (fab / vendor).mkdir()
            shutil.copy2(zip_path, fab / vendor / zip_path.name)

        pos = tmpd / "pos.csv"
        run([KICAD_CLI, "pcb", "export", "pos", "--side", "both", "--format", "csv", "--units", "mm", "-o", str(pos), str(pcb)])
        fps = placed_footprints(pcb)
        write_cpls(fab, pos, fps, pad_centres(pcb))
        write_boms(fab, bom_groups(fps, bom))

    # Assembly drawings (bottom one mirrored, as seen from the bottom) show references and pin-1 marks.
    for side, layers, extra in (("top", "F.Fab,F.Silkscreen,F.Mask,Edge.Cuts", []),
                                ("bottom", "B.Fab,B.Silkscreen,B.Mask,Edge.Cuts", ["--mirror"])):
        run([KICAD_CLI, "pcb", "export", "pdf", "--mode-single", "--scale", "0", "--black-and-white",
             "--sketch-pads-on-fab-layers", *extra, "--layers", layers,
             "-o", str(fab / f"{STEM}-assembly-{side}.pdf"), str(pcb)])
    run([KICAD_CLI, "sch", "export", "pdf", "-o", str(fab / f"{STEM}-schematic.pdf"), str(sch)])
    run([KICAD_CLI, "pcb", "export", "step", "--subst-models", "--force", "-o", str(fab / f"{STEM}.step"), str(pcb)])
    if not args.skip_render:
        for side in ("top", "bottom"):
            run([KICAD_CLI, "pcb", "render", "--side", side, "--width", "1600", "--height", "1240", "--quality", "high",
                 "--background", "opaque", "--use-board-stackup-colors", "-o", str(fab / f"{STEM}-{side}.png"), str(pcb)])

    smd = sum(f["mount"] == "SMD" for f in fps)
    print(f"Wrote {fab}: {len(fps)} parts ({smd} SMD, {len(fps) - smd} THT) for {', '.join(VENDORS)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
