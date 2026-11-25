#!/usr/bin/env python3
"""Verify 1:1 fidelity of the Eagle V3.0 -> KiCad 10 V3.0 PCB & schematic port.

Checks performed:
  1. kicad-cli sch erc (0 errors)
  2. kicad-cli pcb drc (0 errors, 0 unconnected items)
  3. 100% electrical netlist parity between Eagle pcb.brd <signals>, KiCad .kicad_pcb,
     and KiCad .kicad_sch across all 21 components and 30 active electrical nets
  4. 100% Excellon drill hit parity vs eagle-v3.0/gerber.zip (<= 0.05 mm XY & diameter)
  5. 600 DPI gerbv raster XOR comparison across F_Cu, B_Cu, F_Mask, B_Mask,
     F_Silkscreen, B_Silkscreen, and Edge_Cuts
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile
import zlib
from collections import defaultdict
from pathlib import Path

KICAD_CLI = os.environ.get(
    "KICAD_CLI",
    "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli",
)
GERBV = os.environ.get("GERBV", "/opt/homebrew/bin/gerbv")

# KiCad's Eagle importer appends '0' to Eagle refdes that lack a trailing digit.
EAGLE_TO_KICAD_REF = {
    "SWITCH": "SWITCH0",
    "BUTTON": "BUTTON0",
    "CHG-LED": "CHG-LED0",
    "LCD-HEADER": "LCD-HEADER0",
    "PROGRAM": "PROGRAM0",
    "BOOSTER": "BOOSTER0",
    "BAT": "BAT0",
}


def norm_ref(ref: str) -> str:
    return EAGLE_TO_KICAD_REF.get(ref, ref)


def run_cmd(cmd: list[str], allow_fail: bool = False) -> subprocess.CompletedProcess:
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0 and not allow_fail:
        raise RuntimeError(
            f"Command failed ({proc.returncode}): {' '.join(cmd)}\n"
            f"STDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
        )
    return proc


def check_erc(sch_path: Path, tmp_dir: Path) -> dict:
    rpt = tmp_dir / "erc.json"
    run_cmd([KICAD_CLI, "sch", "erc", "--format", "json", "--severity-all", "-o", str(rpt), str(sch_path)])
    data = json.loads(rpt.read_text())
    violations = [v for s in data.get("sheets", []) for v in s.get("violations", [])]
    errors = [v for v in violations if v.get("severity") == "error"]
    warnings = [v for v in violations if v.get("severity") == "warning"]
    return {
        "errors": len(errors),
        "warnings": len(warnings),
        "error_details": [v["description"] for v in errors],
        "ok": len(errors) == 0,
    }


def check_drc(pcb_path: Path, tmp_dir: Path) -> dict:
    rpt = tmp_dir / "drc.json"
    run_cmd([KICAD_CLI, "pcb", "drc", "--format", "json", "--severity-all", "-o", str(rpt), str(pcb_path)])
    data = json.loads(rpt.read_text())
    violations = data.get("violations", [])
    unconnected = data.get("unconnected_items", [])
    errors = [v for v in violations if v.get("severity") == "error"]
    warnings = [v for v in violations if v.get("severity") == "warning"]
    return {
        "errors": len(errors),
        "warnings": len(warnings),
        "unconnected": len(unconnected),
        "error_details": [v["description"] for v in errors],
        "ok": len(errors) == 0 and len(unconnected) == 0,
    }


def parse_eagle_netlist(brd_path: Path) -> tuple[set[str], dict[str, set[tuple[str, str]]]]:
    tree = ET.parse(brd_path)
    board = tree.find(".//board")
    elements = {norm_ref(el.attrib["name"]) for el in board.findall("elements/element")}
    nets: dict[str, set[tuple[str, str]]] = {}
    for sig in board.findall("signals/signal"):
        contacts = {(norm_ref(cr.attrib["element"]), cr.attrib["pad"]) for cr in sig.findall("contactref")}
        if contacts:
            nets[sig.attrib["name"]] = contacts
    return elements, nets


def parse_kicad_pcb_netlist(pcb_path: Path) -> tuple[set[str], dict[str, set[tuple[str, str]]]]:
    text = pcb_path.read_text()
    elements: set[str] = set()
    nets: dict[str, set[tuple[str, str]]] = defaultdict(set)
    for block in text.split("\n\t(footprint ")[1:]:
        ref = re.search(r'\(property "Reference" "([^"]*)"', block).group(1)
        elements.add(ref)
        for pm in re.finditer(r'\(pad "([^"]*)" [^\n]+.*?\n\t\t\)', block, re.S):
            pnum = pm.group(1).replace("\\\\", "\\")
            nm = re.search(r'\(net "([^"]*)"\)', pm.group(0))
            if nm and nm.group(1):
                nets[nm.group(1)].add((ref, pnum))
    return elements, dict(nets)


def parse_kicad_sch_netlist(sch_path: Path, tmp_dir: Path) -> tuple[set[str], dict[str, set[tuple[str, str]]]]:
    xml_out = tmp_dir / "sch_netlist.xml"
    run_cmd([KICAD_CLI, "sch", "export", "netlist", "--format", "kicadxml", "-o", str(xml_out), str(sch_path)])
    tree = ET.parse(xml_out)
    root = tree.getroot()
    elements = {c.attrib["ref"] for c in root.findall("./components/comp")}
    nets: dict[str, set[tuple[str, str]]] = {}
    for net in root.findall("./nets/net"):
        name = net.attrib["name"]
        if name.startswith("unconnected-"):
            continue
        nodes = {(n.attrib["ref"], n.attrib["pin"]) for n in net.findall("node")}
        if nodes:
            nets[name] = nodes
    return elements, nets


def compare_netlists(
    eagle_el: set[str],
    eagle_nets: dict[str, set[tuple[str, str]]],
    other_el: set[str],
    other_nets: dict[str, set[tuple[str, str]]],
    label: str,
) -> dict:
    missing_parts = sorted(eagle_el - other_el)
    extra_parts = sorted(other_el - eagle_el)
    diffs = []
    all_net_names = sorted(set(eagle_nets) | set(other_nets))
    for n in all_net_names:
        e_set = eagle_nets.get(n, set())
        o_set = other_nets.get(n, set())
        if e_set != o_set:
            diffs.append({
                "net": n,
                "missing_pins": sorted(e_set - o_set),
                "extra_pins": sorted(o_set - e_set),
            })
    return {
        "target": label,
        "parts_matched": len(eagle_el & other_el),
        "parts_expected": len(eagle_el),
        "missing_parts": missing_parts,
        "extra_parts": extra_parts,
        "nets_matched": len(all_net_names) - len(diffs),
        "nets_expected": len(eagle_nets),
        "net_diffs": diffs,
        "ok": not missing_parts and not extra_parts and not diffs,
    }


def parse_excellon(path: Path) -> list[tuple[float, float, float]]:
    tools: dict[str, float] = {}
    hits: list[tuple[float, float, float]] = []
    cur_tool: str | None = None
    lines = path.read_text().splitlines()
    is_decimal = any("." in line for line in lines if line.startswith("X"))
    for raw in lines:
        line = raw.strip()
        m = re.match(r"^(T\d+)C([\d.]+)", line)
        if m:
            tools[m.group(1)] = float(m.group(2))
            continue
        m = re.match(r"^(T\d+)$", line)
        if m:
            cur_tool = m.group(1)
            continue
        m = re.match(r"^X([-\d.]+)Y([-\d.]+)", line)
        if m and cur_tool in tools:
            if is_decimal:
                x, y = float(m.group(1)), float(m.group(2))
            else:
                x, y = float(m.group(1)) / 1000.0, float(m.group(2)) / 1000.0
            hits.append((x, y, tools[cur_tool]))
    return hits


def compare_drills(eagle_drl: Path, kicad_drl: Path, tol_mm: float = 0.05) -> dict:
    eh = parse_excellon(eagle_drl)
    kh = parse_excellon(kicad_drl)
    used: set[int] = set()
    max_dxy = 0.0
    max_dd = 0.0
    mismatches = []
    for ex, ey, ed in eh:
        best_i = None
        best_d = 1e9
        for i, (kx, ky, kd) in enumerate(kh):
            if i in used:
                continue
            d = math.hypot(ex - kx, ey - ky)
            if d < best_d:
                best_d = d
                best_i = i
        if best_i is not None and best_d < 0.5:
            used.add(best_i)
            kx, ky, kd = kh[best_i]
            dd = abs(ed - kd)
            max_dxy = max(max_dxy, best_d)
            max_dd = max(max_dd, dd)
            if best_d > tol_mm or dd > tol_mm:
                mismatches.append({"eagle": [ex, ey, ed], "kicad": [kx, ky, kd], "dxy": best_d, "dd": dd})
        else:
            mismatches.append({"eagle": [ex, ey, ed], "kicad": None, "dxy": best_d, "dd": None})
    extra = [kh[i] for i in range(len(kh)) if i not in used]
    return {
        "eagle_hits": len(eh),
        "kicad_hits": len(kh),
        "matched": len(used),
        "extra_kicad_hits": len(extra),
        "max_xy_err_mm": round(max_dxy, 5),
        "max_dia_err_mm": round(max_dd, 5),
        "tolerance_mm": tol_mm,
        "mismatches": mismatches,
        "ok": len(used) == len(eh) and len(extra) == 0 and max_dxy <= tol_mm and max_dd <= tol_mm,
    }


def read_png_binary(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"Not a PNG file: {path}")
    pos = 8
    idat = bytearray()
    w = h = ct = 0
    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        ctype = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            w, h, _bd, ct = struct.unpack(">IIBB", chunk[:10])
        elif ctype == b"IDAT":
            idat.extend(chunk)
        elif ctype == b"IEND":
            break
    raw = zlib.decompress(bytes(idat))
    bpp = {0: 1, 2: 3, 4: 2, 6: 4}[ct]
    stride = w * bpp
    out = bytearray(w * h)
    prev = bytearray(stride)
    idx = 0
    for y in range(h):
        ftype = raw[idx]
        idx += 1
        row = bytearray(raw[idx : idx + stride])
        idx += stride
        if ftype == 1:
            for i in range(bpp, stride):
                row[i] = (row[i] + row[i - bpp]) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                row[i] = (row[i] + prev[i]) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                a = row[i - bpp] if i >= bpp else 0
                row[i] = (row[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = row[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                row[i] = (row[i] + pr) & 0xFF
        prev = row
        out[y * w : (y + 1) * w] = bytes(1 if v > 127 else 0 for v in row[::bpp])
    return w, h, bytes(out)


def compare_gerbers_xor(eagle_dir: Path, kicad_dir: Path, tmp_dir: Path, dpi: int = 600) -> list[dict]:
    layers = [
        ("F_Cu", "PCB 3.0_copper_T.gbr", "diy-thermocam-v3.0-F_Cu.gtl"),
        ("B_Cu", "PCB 3.0_copper_B.gbr", "diy-thermocam-v3.0-B_Cu.gbl"),
        ("F_Mask", "PCB 3.0_soldermask_T.gbr", "diy-thermocam-v3.0-F_Mask.gts"),
        ("B_Mask", "PCB 3.0_soldermask_B.gbr", "diy-thermocam-v3.0-B_Mask.gbs"),
        ("F_Silkscreen", "PCB 3.0_silkscreen_T.gbr", "diy-thermocam-v3.0-F_Silkscreen.gto"),
        ("B_Silkscreen", "PCB 3.0_silkscreen_B.gbr", "diy-thermocam-v3.0-B_Silkscreen.gbo"),
        ("Edge_Cuts", "PCB 3.0_profile.gbr", "diy-thermocam-v3.0-Edge_Cuts.gm1"),
    ]
    results = []
    for name, ef, kf in layers:
        ep = tmp_dir / f"{name}_eagle.png"
        kp = tmp_dir / f"{name}_kicad.png"
        for src, dst in [(eagle_dir / ef, ep), (kicad_dir / kf, kp)]:
            run_cmd([
                GERBV, "-x", "png", "-D", str(dpi),
                "-O", "-0.05x-0.05", "-W", "3.62x2.80",
                "-b", "#000000", "-f", "#FFFFFF", "-B", "0",
                "-o", str(dst), str(src),
            ])
        w1, h1, b1 = read_png_binary(ep)
        w2, h2, b2 = read_png_binary(kp)
        if (w1, h1) != (w2, h2):
            raise RuntimeError(f"Raster dimension mismatch for {name}: {(w1, h1)} vs {(w2, h2)}")
        n_tot = w1 * h1
        n1 = b1.count(1)
        n2 = b2.count(1)
        i1 = int.from_bytes(b1, "big")
        i2 = int.from_bytes(b2, "big")
        n_xor = (i1 ^ i2).to_bytes(n_tot, "big").count(1)
        n_and = (i1 & i2).to_bytes(n_tot, "big").count(1)
        n_or = n1 + n2 - n_and
        iou = (n_and / n_or * 100.0) if n_or else 100.0
        xor_pct = n_xor / n_tot * 100.0
        results.append({
            "layer": name,
            "dpi": dpi,
            "width_px": w1,
            "height_px": h1,
            "eagle_px": n1,
            "kicad_px": n2,
            "xor_px": n_xor,
            "xor_board_pct": round(xor_pct, 4),
            "iou_pct": round(iou, 2),
        })
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify 1:1 Eagle -> KiCad 10 V3.0 PCB port.")
    parser.add_argument("--dpi", type=int, default=600, help="gerbv raster DPI (default: 600)")
    parser.add_argument("--json-out", type=Path, help="Optional JSON report output path")
    args = parser.parse_args()

    pcb_root = Path(__file__).resolve().parent.parent
    eagle_dir = pcb_root / "eagle-v3.0"
    kicad_dir = pcb_root / "kicad-v3.0"
    sch_path = kicad_dir / "diy-thermocam-v3.0.kicad_sch"
    pcb_path = kicad_dir / "diy-thermocam-v3.0.kicad_pcb"

    with tempfile.TemporaryDirectory(prefix="verify-v3-port-") as td:
        tmp = Path(td)
        erc_res = check_erc(sch_path, tmp)
        drc_res = check_drc(pcb_path, tmp)

        eagle_el, eagle_nets = parse_eagle_netlist(eagle_dir / "pcb.brd")
        kpcb_el, kpcb_nets = parse_kicad_pcb_netlist(pcb_path)
        ksch_el, ksch_nets = parse_kicad_sch_netlist(sch_path, tmp)

        pcb_net_res = compare_netlists(eagle_el, eagle_nets, kpcb_el, kpcb_nets, "kicad_pcb")
        sch_net_res = compare_netlists(eagle_el, eagle_nets, ksch_el, ksch_nets, "kicad_sch")

        eagle_gbr = tmp / "eagle_gbr"
        kicad_gbr = tmp / "kicad_gbr"
        eagle_gbr.mkdir()
        kicad_gbr.mkdir()
        with zipfile.ZipFile(eagle_dir / "gerber.zip") as zf:
            zf.extractall(eagle_gbr)

        run_cmd([
            KICAD_CLI, "pcb", "export", "gerbers",
            "--layers", "F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,F.Paste,B.Paste,Edge.Cuts",
            "--check-zones",
            "-o", str(kicad_gbr) + "/",
            str(pcb_path),
        ])
        run_cmd([
            KICAD_CLI, "pcb", "export", "drill",
            "-u", "mm",
            "-o", str(kicad_gbr) + "/",
            str(pcb_path),
        ])

        drill_res = compare_drills(
            eagle_gbr / "PCB 3.0_drills.xln",
            kicad_gbr / "diy-thermocam-v3.0.drl",
            tol_mm=0.05,
        )
        xor_res = compare_gerbers_xor(eagle_gbr, kicad_gbr, tmp, dpi=args.dpi)

    all_ok = (
        erc_res["ok"]
        and drc_res["ok"]
        and pcb_net_res["ok"]
        and sch_net_res["ok"]
        and drill_res["ok"]
    )
    report = {
        "erc": erc_res,
        "drc": drc_res,
        "netlist_pcb": pcb_net_res,
        "netlist_sch": sch_net_res,
        "drills": drill_res,
        "gerber_xor": xor_res,
        "passed": all_ok,
    }
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report, indent=2) + "\n")

    print("=== DIY-Thermocam V3.0 Eagle -> KiCad 10 Port Verification ===")
    print(f"1. Schematic ERC : {erc_res['errors']} errors, {erc_res['warnings']} warnings -> {'PASS' if erc_res['ok'] else 'FAIL'}")
    print(f"2. PCB DRC       : {drc_res['errors']} errors, {drc_res['warnings']} warnings, {drc_res['unconnected']} unconnected -> {'PASS' if drc_res['ok'] else 'FAIL'}")
    print(f"3. Netlist (PCB) : {pcb_net_res['parts_matched']}/{pcb_net_res['parts_expected']} parts, {pcb_net_res['nets_matched']}/{pcb_net_res['nets_expected']} nets -> {'PASS' if pcb_net_res['ok'] else 'FAIL'}")
    print(f"   Netlist (SCH) : {sch_net_res['parts_matched']}/{sch_net_res['parts_expected']} parts, {sch_net_res['nets_matched']}/{sch_net_res['nets_expected']} nets -> {'PASS' if sch_net_res['ok'] else 'FAIL'}")
    print(f"4. Drills        : {drill_res['matched']}/{drill_res['eagle_hits']} hits, max_xy={drill_res['max_xy_err_mm']:.4f} mm, max_dia={drill_res['max_dia_err_mm']:.4f} mm (tol <= {drill_res['tolerance_mm']} mm) -> {'PASS' if drill_res['ok'] else 'FAIL'}")
    print(f"5. Gerber XOR ({args.dpi} DPI via gerbv):")
    for row in xor_res:
        print(
            f"   {row['layer']:12s}: {row['width_px']}x{row['height_px']} px  "
            f"xor={row['xor_board_pct']:6.3f}% of board  IoU={row['iou_pct']:6.2f}%"
        )
    print(f"OVERALL: {'PASS' if all_ok else 'FAIL'}")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
