#!/usr/bin/env python3
"""Export the DIY-Thermocam V4.0 manufacturing files for both boards: the main board and the Lepton carrier.

    python3 hardware/pcb/scripts/export_fab.py [--skip-render]

Needs KiCad 10 (set KICAD_CLI if kicad-cli is not in the default macOS location).
Part numbers come only from hardware/bom/bom.csv; designators are unique across both boards.

Writes hardware/pcb/fab/, once per board (file names start with the board's file stem):
    jlcpcb/   panel-gerbers.zip (the board in its own routed frame with break-off tabs, tooling holes and
              fiducials, ordered as "Panel by Customer"), bom.csv (LCSC numbers), cpl.csv (rotations and
              offsets for JLCPCB's part library; same coordinates as the panel)
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
STEMS = ("diy-thermocam-v4.0", "diy-thermocam-v4.0-lepton")
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
    "U2": (0.175, 0.0, 0.0),     # C2071784 (same EasyEDA land as C915088): origin offset 0.175 mm
    "J1": (0.0, 0.0, 90.0),      # 2x20 socket: EasyEDA HDR-TH_40P drawn with its rows horizontal
    "J5": (0.0, 0.0, 270.0),     # C41376163: EasyEDA 1x24 drawn horizontally
    "J6": (0.0, 0.0, 270.0),     # C41376163
    # Lepton carrier
    "U101": (0.0, 0.0, 180.0),   # C163219 SOT-23-5: EasyEDA pins 1-3 on the right
    "U102": (0.0, 0.0, 180.0),   # C53099 SOT-23-5, same EasyEDA footprint
    "U103": (0.0, 0.0, 270.0),   # C403651 SOT-23-5: EasyEDA drawn with its pin rows horizontal
    "L101": (0.0, 0.0, 180.0),   # C43389: EasyEDA pad 1 on the right (unpolarised, kept consistent)
}

# Multi-pin headers whose footprint origin sits on pin 1: report the pad centre instead.
PAD_CENTRE_REFS = ("J1", "J2", "J5", "J6", "J101")

# JLCPCB assembly panels. Each board sits in its own frame (JLCPCB: >= 70 x 70 mm, 5 mm or wider process edges,
# 2 mm non-plated tooling holes placed asymmetrically, 1 mm fiducials 3.85 mm from the panel edge), separated by
# a 2 mm routed slot. Routed edges hold +-0.2 mm, where V-cut holds +-0.4 mm and leaves a lip. Break-off tabs
# with mouse bites sit only where the edge is free: never at J3, the Teensy sockets, the spring pins, SW1, SW2,
# Q2, the acrylic case's tripod fin (bottom edge, X 35.7-53.7), or on the carrier's 47 mm edges (the one beside
# the Teensy has 0.8 mm to spare). Bites follow JLCPCB's guide: 0.6 mm holes, 0.35 mm webs, a third into the board.
# Tabs: (edge, centre along that edge, width); edges by KiCad coordinates: ymin = top of the drawing.
PANEL_TABS: dict[str, list[tuple[str, float, float]]] = {
    "diy-thermocam-v4.0": [("ymax", 6.0, 5.0), ("ymax", 32.5, 5.0),                        # USB edge, beside J3
                           ("ymin", 25.0, 5.0), ("ymin", 45.0, 5.0), ("ymin", 65.0, 5.0),   # far edge, clear of D2/SW2
                           ("xmax", -26.0, 5.0),                                            # right edge, between SW1 and Q2
                           ("xmin", -4.5, 5.0)],                                            # left edge, below J1
    "diy-thermocam-v4.0-lepton": [("ymin", 46.0, 8.0), ("ymax", 42.6, 8.0)],               # the two 29.2 mm edges only
}
PANEL_GAP, PANEL_FRAME, PANEL_MIN = 2.0, 7.0, 70.0
BITE_DRILL, BITE_PITCH, BITE_OUT = 0.6, 0.95, 0.1   # hole, centre spacing, hole centre outside the board edge


Row = dict[str, str]
Part = dict[str, str]
Point = tuple[float, float]
Seg = tuple[str, tuple[Point, ...]]


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


def write_boms(fab: Path, stem: str, groups: list[tuple[Row, list[Part]]]) -> None:
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
    write_csv(fab / "jlcpcb" / f"{stem}-bom.csv", ["Comment", "Designator", "Footprint", "LCSC Part #"], jlc)
    write_csv(fab / "seeed" / f"{stem}-bom.csv", ["Designator", "MPN / Seeed SKU", "Quantity", "Part Link"], seeed)
    write_csv(fab / "pcbway" / f"{stem}-bom.csv",
              ["Item #", "Designator", "Qty", "Manufacturer", "Mfg Part #", "Description / Value",
               "Package/Footprint", "Type", "Your Instructions / Notes"], pcbway)


def write_cpls(fab: Path, stem: str, pos_csv: Path, fps: list[Part], centres: dict[str, tuple[float, float]]) -> None:
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
    write_csv(fab / "jlcpcb" / f"{stem}-cpl.csv", header, jlc)
    for vendor in ("seeed", "pcbway"):
        write_csv(fab / vendor / f"{stem}-cpl.csv", header, generic)


def top_level_blocks(text: str) -> list[tuple[int, int]]:
    """(start, end) offsets of the depth-1 s-expressions in a .kicad_pcb file."""
    spans: list[tuple[int, int]] = []
    depth, start, in_str, i = 0, 0, False, 0
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            depth += 1
            if depth == 2:
                start = i
        elif c == ")":
            if depth == 2:
                spans.append((start, i + 1))
            depth -= 1
        i += 1
    return spans


def board_outline(text: str) -> tuple[float, float, float, float, float]:
    """Bounding box and corner radius of the board's rounded-rectangle outline: x0, x1, y0, y1, r."""
    xs: list[float] = []
    ys: list[float] = []
    radius = 0.0
    for a, b in top_level_blocks(text):
        block = text[a:b]
        if not re.match(r'\((gr_line|gr_arc)\b', block) or '(layer "Edge.Cuts")' not in block:
            continue
        pts = [(float(m.group(1)), float(m.group(2))) for m in re.finditer(r'\((?:start|mid|end) ([-\d.]+) ([-\d.]+)\)', block)]
        xs += [p[0] for p in pts]
        ys += [p[1] for p in pts]
        if block.startswith("(gr_arc"):
            (sx, _), _, (ex, _) = pts
            radius = max(radius, abs(ex - sx))
    return min(xs), max(xs), min(ys), max(ys), radius


def panel_text(text: str, tabs: list[tuple[str, float, float]]) -> str:
    """The board inside a routed frame: Edge.Cuts replaced by frame + slot contours, plus mouse bites,
    tooling holes and fiducials as footprints that stay out of the BOM and position files."""
    x0, x1, y0, y1, r = board_outline(text)
    g = PANEL_GAP
    # outer frame: board + gap + frame on every side, grown symmetrically to JLCPCB's 70 x 70 mm minimum
    fx0, fx1, fy0, fy1 = x0 - g - PANEL_FRAME, x1 + g + PANEL_FRAME, y0 - g - PANEL_FRAME, y1 + g + PANEL_FRAME
    if fx1 - fx0 < PANEL_MIN:
        grow = (PANEL_MIN - (fx1 - fx0)) / 2
        fx0, fx1 = fx0 - grow, fx1 + grow
    if fy1 - fy0 < PANEL_MIN:
        grow = (PANEL_MIN - (fy1 - fy0)) / 2
        fy0, fy1 = fy0 - grow, fy1 + grow

    def f(v: float) -> str:
        return f"{round(v, 4):g}"

    uid = iter(range(1, 10**6))

    def u() -> str:
        return f"00000000-0000-4000-8000-{next(uid):012d}"

    def line(a: Point, b: Point) -> str:
        return (f'\t(gr_line\n\t\t(start {f(a[0])} {f(a[1])})\n\t\t(end {f(b[0])} {f(b[1])})\n'
                f'\t\t(stroke\n\t\t\t(width 0.1)\n\t\t\t(type default)\n\t\t)\n\t\t(layer "Edge.Cuts")\n\t\t(uuid "{u()}")\n\t)\n')

    def arc(a: Point, m: Point, b: Point) -> str:
        return (f'\t(gr_arc\n\t\t(start {f(a[0])} {f(a[1])})\n\t\t(mid {f(m[0])} {f(m[1])})\n\t\t(end {f(b[0])} {f(b[1])})\n'
                f'\t\t(stroke\n\t\t\t(width 0.1)\n\t\t\t(type default)\n\t\t)\n\t\t(layer "Edge.Cuts")\n\t\t(uuid "{u()}")\n\t)\n')

    # Rounded rectangle offset by d, walked clockwise on screen: edges 0 top (ymin), 1 right, 2 bottom, 3 left.
    order = ["ymin", "xmax", "ymax", "xmin"]
    k = 1 / math.sqrt(2)

    def corner(i: int, d: float) -> tuple[Point, Point, Point]:
        cx, cy, R = [(x1 - r, y0 + r), (x1 - r, y1 - r), (x0 + r, y1 - r), (x0 + r, y0 + r)][i] + (r + d,)
        start = [(cx, cy - R), (cx + R, cy), (cx, cy + R), (cx - R, cy)][i]
        mid = [(cx + R * k, cy - R * k), (cx + R * k, cy + R * k), (cx - R * k, cy + R * k), (cx - R * k, cy - R * k)][i]
        end = [(cx + R, cy), (cx, cy + R), (cx - R, cy), (cx, cy - R)][i]
        return start, mid, end

    def point(i: int, c: float, d: float) -> Point:
        return [(c, y0 - d), (x1 + d, c), (c, y1 + d), (x0 - d, c)][i]

    def along(i: int, c: float) -> float:
        """Position along edge i in walking direction."""
        return [c, c, -c, -c][i]

    def path(ia: int, ca: float, ib: int, cb: float, d: float) -> list[Seg]:
        """Segments from point (ia, ca) clockwise to (ib, cb) on the outline offset by d."""
        segs: list[Seg] = []
        p = point(ia, ca, d)
        i = ia
        if not (ia == ib and along(ib, cb) > along(ia, ca)):
            while True:
                s, m, e = corner(i, d)
                segs.append(("line", (p, s)))
                if r + d > 0:
                    segs.append(("arc", (s, m, e)))
                p = e
                i = (i + 1) % 4
                if i == ib:
                    break
        segs.append(("line", (p, point(ib, cb, d))))
        return segs

    def emit(segs: list[Seg]) -> str:
        out = ""
        for kind, pts in segs:
            if kind == "line" and math.dist(pts[0], pts[1]) > 1e-6:
                out += line(*pts)
            elif kind == "arc":
                out += arc(*pts)
        return out

    def reverse(segs: list[Seg]) -> list[Seg]:
        return [(kind, tuple(reversed(pts))) for kind, pts in reversed(segs)]

    cuts = line((fx0, fy0), (fx1, fy0)) + line((fx1, fy0), (fx1, fy1)) + line((fx1, fy1), (fx0, fy1)) + line((fx0, fy1), (fx0, fy0))
    ends = sorted(((order.index(e), c - w / 2 * (1 if order.index(e) < 2 else -1), c + w / 2 * (1 if order.index(e) < 2 else -1))
                   for e, c, w in tabs), key=lambda t: (t[0], along(t[0], t[1])))
    for n, (ia, _, ca) in enumerate(ends):           # slot piece from the end of one tab to the start of the next
        ib, cb, _ = ends[(n + 1) % len(ends)]
        board = path(ia, ca, ib, cb, 0.0)
        inner = path(ia, ca, ib, cb, g)
        cuts += emit(board + [("line", (point(ib, cb, 0.0), point(ib, cb, g)))] + reverse(inner)
                     + [("line", (point(ia, ca, g), point(ia, ca, 0.0)))])

    def footprint(ref: str, x: float, y: float, side: str, pad: str) -> str:
        return (f'\t(footprint "Panel:{ref.rstrip("0123456789")}"\n\t\t(layer "{side}.Cu")\n\t\t(uuid "{u()}")\n'
                f'\t\t(at {f(x)} {f(y)})\n'
                f'\t\t(property "Reference" "{ref}"\n\t\t\t(at 0 0 0)\n\t\t\t(layer "{side}.Fab")\n\t\t\t(hide yes)\n'
                f'\t\t\t(uuid "{u()}")\n\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1 1)\n\t\t\t\t\t(thickness 0.15)\n'
                f'\t\t\t\t)\n\t\t\t)\n\t\t)\n'
                f'\t\t(property "Value" "panel"\n\t\t\t(at 0 0 0)\n\t\t\t(layer "{side}.Fab")\n\t\t\t(hide yes)\n'
                f'\t\t\t(uuid "{u()}")\n\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1 1)\n\t\t\t\t\t(thickness 0.15)\n'
                f'\t\t\t\t)\n\t\t\t)\n\t\t)\n'
                f'\t\t(attr exclude_from_pos_files exclude_from_bom allow_missing_courtyard)\n{pad}\t)\n')

    def npth(dia: float) -> str:
        return (f'\t\t(pad "" np_thru_hole circle\n\t\t\t(at 0 0)\n\t\t\t(size {f(dia)} {f(dia)})\n\t\t\t(drill {f(dia)})\n'
                f'\t\t\t(layers "*.Cu" "*.Mask")\n\t\t\t(uuid "{u()}")\n\t\t)\n')

    def fid(side: str) -> str:
        return (f'\t\t(pad "" smd circle\n\t\t\t(at 0 0)\n\t\t\t(size 1 1)\n\t\t\t(layers "{side}.Cu" "{side}.Mask")\n'
                f'\t\t\t(solder_mask_margin 0.5)\n\t\t\t(uuid "{u()}")\n\t\t)\n')

    parts = ""
    n = 0
    for e, c, w in tabs:                             # mouse bites along the board edge
        i = order.index(e)
        count = int(w // BITE_PITCH)
        for j in range(count):
            t = c + (j - (count - 1) / 2) * BITE_PITCH
            x, y = point(i, t, BITE_OUT)
            n += 1
            parts += footprint(f"MB{n}", x, y, "F", npth(BITE_DRILL))
    e = 3.85                                          # tooling holes (3, asymmetric) and fiducials, both sides
    for m, (x, y) in enumerate(((fx0 + e, fy0 + e), (fx1 - e, fy0 + e), (fx0 + e, fy1 - e)), 1):
        parts += footprint(f"TH{m}", x, y, "F", npth(2.0))
    for m, (x, y) in enumerate(((fx0 + e + 6, fy0 + e), (fx1 - e - 6, fy0 + e), (fx0 + e + 6, fy1 - e)), 1):
        parts += footprint(f"FID{m}", x, y, "F", fid("F"))
        parts += footprint(f"FID{m + 3}", x, y, "B", fid("B"))

    keep: list[str] = []
    last = 0
    for a, b in top_level_blocks(text):
        block = text[a:b]
        if re.match(r'\((gr_line|gr_arc|gr_rect|gr_circle|gr_poly)\b', block) and '(layer "Edge.Cuts")' in block:
            keep.append(text[last:a].rstrip("\t"))
            last = b + 1 if text[b:b + 1] == "\n" else b
    keep.append(text[last:])
    body = "".join(keep).rstrip()
    assert body.endswith(")")
    return body[:-1] + cuts + parts + ")\n"


def write_panel(pcb_dir: Path, stem: str, out_dir: Path) -> Path:
    """Panel copy of a board, next to copies of its project and rule files so zone fills use the board's rules."""
    out_dir.mkdir(parents=True, exist_ok=True)
    for ext in (".kicad_pro", ".kicad_dru"):
        if (pcb_dir / f"{stem}{ext}").exists():
            _ = shutil.copy2(pcb_dir / f"{stem}{ext}", out_dir / f"{stem}{ext}")
    panel = out_dir / f"{stem}.kicad_pcb"
    text = panel_text((pcb_dir / f"{stem}.kicad_pcb").read_text(encoding="utf-8"), PANEL_TABS[stem])
    _ = panel.write_text(text, encoding="utf-8")
    return panel


def gerber_zip(pcb: Path, tmpd: Path, name: str) -> Path:
    gbr = tmpd / f"gerbers-{name}"
    gbr.mkdir()
    run([KICAD_CLI, "pcb", "export", "gerbers", "--layers", GERBER_LAYERS, "--check-zones", "-o", f"{gbr}/", str(pcb)])
    run([KICAD_CLI, "pcb", "export", "drill", "-u", "mm", "--excellon-separate-th", "-o", f"{gbr}/", str(pcb)])
    zip_path = tmpd / name
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for p in sorted(gbr.iterdir()):
            zf.write(p, arcname=p.name)
    return zip_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Export DIY-Thermocam V4.0 manufacturing files.")
    _ = parser.add_argument("--skip-render", action="store_true", help="skip the 3D PNG renders")
    skip_render = parser.parse_args().skip_render is True

    pcb_dir = Path(__file__).resolve().parent.parent
    fab = pcb_dir / "fab"
    bom = load_bom(pcb_dir.parent / "bom" / "bom.csv")

    if fab.exists():
        shutil.rmtree(fab)
    fab.mkdir()
    for vendor in VENDORS:
        (fab / vendor).mkdir()
    for stem in STEMS:
        export_board(pcb_dir, fab, stem, bom, skip_render)
    return 0


def export_board(pcb_dir: Path, fab: Path, stem: str, bom: dict[str, Row], skip_render: bool) -> None:
    pcb = pcb_dir / f"{stem}.kicad_pcb"
    sch = pcb_dir / f"{stem}.kicad_sch"
    with tempfile.TemporaryDirectory() as tmp:
        tmpd = Path(tmp)
        single = gerber_zip(pcb, tmpd, f"{stem}-gerbers.zip")
        for vendor in ("seeed", "pcbway"):
            _ = shutil.copy2(single, fab / vendor / single.name)
        panel = gerber_zip(write_panel(pcb_dir, stem, tmpd / "panel"), tmpd, f"{stem}-panel-gerbers.zip")
        _ = shutil.copy2(panel, fab / "jlcpcb" / panel.name)

        pos = tmpd / "pos.csv"
        run([KICAD_CLI, "pcb", "export", "pos", "--side", "both", "--format", "csv", "--units", "mm", "-o", str(pos), str(pcb)])
        fps = placed_footprints(pcb)
        write_cpls(fab, stem, pos, fps, pad_centres(pcb))
        write_boms(fab, stem, bom_groups(fps, bom))

    # Assembly drawings (bottom one mirrored, as seen from the bottom) show references and pin-1 marks.
    for side, layers, extra in (("top", "F.Fab,F.Silkscreen,F.Mask,Edge.Cuts", []),
                                ("bottom", "B.Fab,B.Silkscreen,B.Mask,Edge.Cuts", ["--mirror"])):
        run([KICAD_CLI, "pcb", "export", "pdf", "--mode-single", "--scale", "0", "--black-and-white",
             "--sketch-pads-on-fab-layers", *extra, "--layers", layers,
             "-o", str(fab / f"{stem}-assembly-{side}.pdf"), str(pcb)])
    run([KICAD_CLI, "sch", "export", "pdf", "-o", str(fab / f"{stem}-schematic.pdf"), str(sch)])
    run([KICAD_CLI, "pcb", "export", "step", "--subst-models", "--force", "-o", str(fab / f"{stem}.step"), str(pcb)])
    if not skip_render:
        for side in ("top", "bottom"):
            run([KICAD_CLI, "pcb", "render", "--side", side, "--width", "1600", "--height", "1240", "--quality", "high",
                 "--background", "opaque", "--use-board-stackup-colors", "-o", str(fab / f"{stem}-{side}.png"), str(pcb)])

    smd = sum(f["mount"] == "SMD" for f in fps)
    print(f"Wrote {stem}: {len(fps)} parts ({smd} SMD, {len(fps) - smd} THT) for {', '.join(VENDORS)}")


if __name__ == "__main__":
    sys.exit(main())
