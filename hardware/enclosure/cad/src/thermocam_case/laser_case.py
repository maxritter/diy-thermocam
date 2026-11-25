"""Parametric 6-plate 3 mm laser-cut acrylic enclosure for DIY-Thermocam V3.0 / V4.0.

Incorporates all GPT-6.2 Astra Stage 1 2D findings (#182/#184) & Max #186 polish:
  - ENC-2D-P1-1: 3.20 mm nominal face slot width (t + 0.20 mm fit allowance matching
    legacy-v3.0/enclosure.svg) so 2.8-3.2 mm cast acrylic assembles smoothly without cracking.
  - ENC-2D-P1-2: Exact 39.20 mm standoff cavity depth (20 + 1.6 + 11 + 1.6 + 5) -> 0.00 mm bow!
  - ENC-2D-P1-3: Rotated tripod bolt circle (30/150/270 deg, R=8.2 mm, Z=29.8) -> >=2.6 mm
    bolt-to-center web and >=3.8 mm web clear of V4 USB-C!
  - ENC-2D-P1-4: Widened 13.60 mm circular Lepton aperture clearing 71 deg DFOV across BOTH
    optical centers ((42.75, 51.00) & (44.50, 51.50)) even on worst-case 3.20 mm acrylic stock!
  - Max #186: Separate `#0066FF` `engrave` / `ENGRAVE` layer with Version 1 concentric
    thermal-lens mark + `DIY-THERMOCAM` wordmark on the back plate (toggleable for $0!).
"""

from __future__ import annotations

from dataclasses import dataclass
import math
from pathlib import Path

from build123d import (
    BuildPart,
    BuildSketch,
    Circle,
    ColorIndex,
    Compound,
    ExportDXF,
    ExportSVG,
    Face,
    FontStyle,
    LineType,
    Location,
    Locations,
    Mode,
    Plane,
    Rectangle,
    RectangleRounded,
    Rotation,
    SlotOverall,
    Solid,
    Text,
    Unit,
    extrude,
    offset,
)

from .params import EnclosureParams


@dataclass(frozen=True)
class LaserEnclosure:
    """Assembled 3D solids and 2D flat cutting/engraving sheets for the 6-plate acrylic case."""

    params: EnclosureParams
    plates_3d: dict[str, Solid]
    plates_2d: dict[str, Face]
    sheet_2d: Compound
    engrave_2d: Compound
    sheet_labels: list[tuple[str, float, float]]


def _plate_box_bounds(p: EnclosureParams) -> dict[str, float]:
    t = p.laser_t
    x0 = -p.laser_inner_margin_x_neg
    x1 = p.pcb_w + p.laser_inner_margin_x_pos
    y0 = -p.laser_inner_margin_y_neg
    y1 = p.pcb_h + p.laser_inner_margin_y_pos
    d_in = p.laser_internal_depth  # 39.20 mm exact standoff stack (ENC-2D-P1-2!)
    over = p.laser_plate_overhang
    return {
        "t": t, "x0": x0, "x1": x1, "y0": y0, "y1": y1,
        "z0": 0.0, "z1": d_in, "w_in": x1 - x0, "h_in": y1 - y0, "d_in": d_in,
        "fb_w": (x1 - x0) + 2.0 * (t + over),
        "fb_h": (y1 - y0) + 2.0 * (t + over),
        "fb_cx": 0.5 * (x0 + x1),
        "fb_cy": 0.5 * (y0 + y1),
    }


def _fb_tab_centers(b: dict[str, float]) -> tuple[list[float], list[float]]:
    x0, y0, w_in, h_in = b["x0"], b["y0"], b["w_in"], b["h_in"]
    return (
        [x0 + w_in * 0.18, x0 + w_in * 0.50, x0 + w_in * 0.82],
        [y0 + h_in * 0.25, y0 + h_in * 0.75],
    )


def _build_front_or_back_2d(
    p: EnclosureParams, b: dict[str, float], is_front: bool, slot_clr: float = 0.0
) -> Face:
    t = b["t"]
    slot_t = t + slot_clr
    tab_l = p.laser_tab_len + slot_clr
    tb_xs, lr_ys = _fb_tab_centers(b)

    with BuildSketch() as sk:
        with Locations((b["fb_cx"], b["fb_cy"])):
            RectangleRounded(b["fb_w"], b["fb_h"], p.laser_corner_r)
        with Locations(p.mounting_holes):
            Circle(p.mounting_hole_d / 2.0, mode=Mode.SUBTRACT)

        slot_y_bot, slot_y_top = b["y0"] - t / 2.0, b["y1"] + t / 2.0
        for tx in tb_xs:
            with Locations((tx, slot_y_bot), (tx, slot_y_top)):
                Rectangle(tab_l, slot_t, mode=Mode.SUBTRACT)

        slot_x_left, slot_x_right = b["x0"] - t / 2.0, b["x1"] + t / 2.0
        for ty in lr_ys:
            with Locations((slot_x_left, ty), (slot_x_right, ty)):
                Rectangle(slot_t, tab_l, mode=Mode.SUBTRACT)

        if is_front:
            with Locations((p.display_win_cx, p.display_win_cy)):
                RectangleRounded(p.display_win_w, p.display_win_h, 0.5, mode=Mode.SUBTRACT)
        else:
            with Locations((p.lepton_x, p.lepton_y)):
                Circle(p.laser_lepton_aperture_d / 2.0, mode=Mode.SUBTRACT)
            with Locations((p.teensy_prog_x, p.teensy_prog_y)):
                Circle(p.teensy_prog_hole_d / 2.0, mode=Mode.SUBTRACT)
            if p.back_face_led_holes:
                with Locations(p.back_face_led_holes):
                    Circle(p.back_face_led_d / 2.0, mode=Mode.SUBTRACT)

    return sk.sketch.faces()[0]


def _build_back_engrave_2d(p: EnclosureParams, b: dict[str, float]) -> Compound:
    """Build Max #186 separate #0066FF laser-engrave layer (Version 1 concentric lens + wordmark)."""
    cx = b["fb_cx"]
    with BuildSketch() as sk:
        # 3 concentric thermal-lens rings (zero crosshairs!) cleanly placed on lower back plate
        for r in (4.2, 2.7, 1.2):
            with Locations((cx - 25.5, 16.0)):
                Circle(r)
        with Locations((cx + 5.5, 18.2)):
            Text("DIY-THERMOCAM", font_size=4.8, font_style=FontStyle.BOLD)
        with Locations((cx + 5.5, 12.2)):
            Text(p.version_tag, font_size=3.2, font_style=FontStyle.REGULAR)
    return Compound(children=list(sk.sketch.edges()))


def _build_top_or_bottom_2d(p: EnclosureParams, b: dict[str, float], is_top: bool) -> Face:
    t, tab_l = b["t"], p.laser_tab_len
    tb_xs, _ = _fb_tab_centers(b)
    x0, x1, d_in = b["x0"], b["x1"], b["d_in"]
    seg = d_in / 3.0

    with BuildSketch() as sk:
        with Locations((0.5 * (x0 + x1), d_in / 2.0)):
            Rectangle(x1 - x0, d_in)
        for corner_x in (x0 - t / 2.0, x1 + t / 2.0):
            with Locations((corner_x, seg / 2.0), (corner_x, 2.5 * seg)):
                Rectangle(t, seg)
        for tx in tb_xs:
            with Locations((tx, -t / 2.0), (tx, d_in + t / 2.0)):
                Rectangle(tab_l, t)

        if is_top:
            if p.is_zero_solder:
                # ENC-2D-P0-1: Guided 7.2 x 3.4 mm slot for pure 3 mm laser-cut acrylic capture_key!
                with Locations((p.button_x, 18.30)):
                    RectangleRounded(7.20, 3.40, 0.30, mode=Mode.SUBTRACT)
            else:
                with Locations((p.button_x, p.button_z)):
                    Circle(p.button_hole_d / 2.0, mode=Mode.SUBTRACT)
            if not p.back_face_led_holes:
                with Locations((p.led_x, p.led_z)):
                    Circle(p.laser_led_d / 2.0, mode=Mode.SUBTRACT)
            with Locations((p.microsd_x, p.microsd_z)):
                SlotOverall(p.microsd_w, p.microsd_h, mode=Mode.SUBTRACT)
        else:
            with Locations((p.usb.center_x, p.usb.center_z)):
                SlotOverall(p.usb.laser_width, p.usb.laser_height, mode=Mode.SUBTRACT)
            # ENC-2D-P1-3: Rotated bolt circle (30, 150, 270 deg; R=8.2 mm) -> >=2.6 mm & >=9.6 mm webs!
            with Locations((p.laser_tripod_x, p.laser_tripod_z)):
                Circle(p.laser_tripod_center_d / 2.0, mode=Mode.SUBTRACT)
            for deg in (30.0, 150.0, 270.0):
                ang = math.radians(deg)
                bx = p.laser_tripod_x + p.laser_tripod_bolt_r * math.cos(ang)
                bz = p.laser_tripod_z + p.laser_tripod_bolt_r * math.sin(ang)
                with Locations((bx, bz)):
                    Circle(p.laser_tripod_bolt_d / 2.0, mode=Mode.SUBTRACT)

    return sk.sketch.faces()[0]


def _build_left_or_right_2d(p: EnclosureParams, b: dict[str, float], is_right: bool) -> Face:
    t, tab_l = b["t"], p.laser_tab_len
    _, lr_ys = _fb_tab_centers(b)
    y0, y1, d_in = b["y0"], b["y1"], b["d_in"]
    seg = d_in / 3.0

    with BuildSketch() as sk:
        with Locations((0.5 * (y0 + y1), d_in / 2.0)):
            Rectangle(y1 - y0, d_in)
        for corner_y in (y0 - t / 2.0, y1 + t / 2.0):
            with Locations((corner_y, 1.5 * seg)):
                Rectangle(t, seg)
        for ty in lr_ys:
            with Locations((ty, -t / 2.0), (ty, d_in + t / 2.0)):
                Rectangle(tab_l, t)
        if is_right:
            if p.is_zero_solder:
                # ENC-2D-P0-1: Guided 10.2 x 3.4 mm slot for pure 3 mm laser-cut acrylic switch_key!
                with Locations((p.switch_y, 18.30)):
                    RectangleRounded(p.switch_h_y, 3.40, 0.30, mode=Mode.SUBTRACT)
            else:
                with Locations((p.switch_y, p.switch_z)):
                    Rectangle(p.switch_h_y, p.switch_w_z, mode=Mode.SUBTRACT)

    return sk.sketch.faces()[0]


def _build_v4_acrylic_keys_2d(p: EnclosureParams, b: dict[str, float]) -> dict[str, Face]:
    """Build pure 3 mm laser-cut V4 acrylic capture_key & switch_key (ENC-2D-P0-1 fix!).

    Both keys are cut from the exact same 3 mm Acryl GS sheet (Z in [16.80, 19.80] < PCB 20.0!):
      - `capture_key`: T-shaped plunger retained behind top plate (Y=69.50..70.65), actuator tip at
        Y=69.10 (0.15 mm clear above SW2 at Y=68.95!), tactile finger button protruding +2.0 mm!
      - `switch_key`: Forked slide actuator retained behind right plate (X=90.10..91.50), fork tines
        reaching X=88.90 (1.10 mm axial overlap on C&K JS102011SAQN!), 6.0 mm neck -> +-1.8 mm stop!
    """
    t = b["t"]
    # 1. Capture key (XY plane in world coords: centered at X=p.button_x, Y=69.10..75.80)
    y_plate_in = b["y1"]          # 70.80 mm
    y_plate_out = b["y1"] + t     # 73.80 mm
    with BuildSketch() as sk_cap:
        with Locations((p.button_x, 0.5 * (69.50 + (y_plate_in - 0.15)))):
            RectangleRounded(11.40, (y_plate_in - 0.15) - 69.50, 0.35)
        with Locations((p.button_x, 0.5 * (69.10 + 69.65))):
            Rectangle(3.20, 69.65 - 69.10)
        with Locations((p.button_x, 0.5 * (70.00 + (y_plate_out + 2.00)))):
            RectangleRounded(6.20, (y_plate_out + 2.00) - 70.00, 0.8)

    # 2. Switch key (XY plane in world coords: X=88.90..96.60, centered at Y=p.switch_y)
    x_plate_in = b["x1"]          # 91.60 mm
    x_plate_out = b["x1"] + t     # 94.60 mm
    with BuildSketch() as sk_sw:
        with Locations((0.5 * (90.10 + (x_plate_in - 0.10)), p.switch_y)):
            RectangleRounded((x_plate_in - 0.10) - 90.10, 14.00, 0.5)
        with Locations((0.5 * (88.90 + 90.40), p.switch_y)):
            RectangleRounded(90.40 - 88.90, 7.20, 0.4)
        with Locations((0.5 * (88.80 + 91.10), p.switch_y)):
            Rectangle(91.10 - 88.80, 2.40, mode=Mode.SUBTRACT)
        with Locations((0.5 * (90.90 + (x_plate_out + 2.00)), p.switch_y)):
            RectangleRounded((x_plate_out + 2.00) - 90.90, 6.00, 0.8)

    return {
        "capture_key": sk_cap.sketch.faces()[0],
        "switch_key": sk_sw.sketch.faces()[0],
    }


def _extrude_plate_on_plane(face_2d: Face, thickness: float) -> Solid:
    with BuildPart() as bp:
        extrude(face_2d, amount=thickness)
    solids = bp.part.solids()
    assert len(solids) == 1
    return solids[0]


def _apply_kerf(face_2d: Face, kerf: float) -> Face:
    if kerf <= 0.0:
        return face_2d
    comp = offset(face_2d, amount=kerf / 2.0, mode=Mode.REPLACE)
    faces = comp.faces() if hasattr(comp, "faces") else [comp]
    return faces[0]


def build_laser_enclosure(p: EnclosureParams, *, kerf: float | None = None) -> LaserEnclosure:
    b = _plate_box_bounds(p)
    t, d_in = b["t"], b["d_in"]
    eff_kerf = p.laser_kerf if kerf is None else kerf

    # 3D nominal faces (0 slot clearance for exact 3D B-Rep interference checking)
    front_3d_f = _build_front_or_back_2d(p, b, is_front=True, slot_clr=0.0)
    back_3d_f = _build_front_or_back_2d(p, b, is_front=False, slot_clr=0.0)
    # 2D cutting faces (with 0.20 mm cast-acrylic stock tolerance -> 3.20 mm slots, ENC-2D-P1-1!)
    front_2d = _build_front_or_back_2d(p, b, is_front=True, slot_clr=p.laser_slot_clearance)
    back_2d = _build_front_or_back_2d(p, b, is_front=False, slot_clr=p.laser_slot_clearance)
    top_2d = _build_top_or_bottom_2d(p, b, is_top=True)
    bottom_2d = _build_top_or_bottom_2d(p, b, is_top=False)
    right_2d = _build_left_or_right_2d(p, b, is_right=True)
    left_2d = _build_left_or_right_2d(p, b, is_right=False)
    engrave_local = _build_back_engrave_2d(p, b)

    plates_2d = {
        "front": front_2d, "back": back_2d, "top": top_2d,
        "bottom": bottom_2d, "left": left_2d, "right": right_2d,
    }
    if p.is_zero_solder:
        plates_2d.update(_build_v4_acrylic_keys_2d(p, b))

    def _to_solid(obj: object) -> Solid:
        return obj if isinstance(obj, Solid) else obj.solids()[0]  # type: ignore[union-attr]

    left_plane = Plane(origin=(b["x0"] - t, 0, 0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))
    right_plane = Plane(origin=(b["x1"], 0, 0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))
    plates_3d = {
        "front": _to_solid(Location((0, 0, d_in)) * _extrude_plate_on_plane(front_3d_f, t)),
        "back": _to_solid(Location((0, 0, -t)) * _extrude_plate_on_plane(back_3d_f, t)),
        "bottom": _to_solid(Location((0, b["y0"], 0)) * Rotation(90, 0, 0) * _extrude_plate_on_plane(bottom_2d, t)),
        "top": _to_solid(Location((0, b["y1"] + t, 0)) * Rotation(90, 0, 0) * _extrude_plate_on_plane(top_2d, t)),
        "left": _to_solid(Location(left_plane) * _extrude_plate_on_plane(left_2d, t)),
        "right": _to_solid(Location(right_plane) * _extrude_plate_on_plane(right_2d, t)),
    }
    if p.is_zero_solder:
        # Both 3 mm acrylic control keys sit at Z in [16.80, 19.80] (< PCB 20.0, centered in 3.4 mm slots!)
        plates_3d["capture_key"] = _to_solid(
            Location((0, 0, 16.80)) * _extrude_plate_on_plane(plates_2d["capture_key"], t)
        )
        plates_3d["switch_key"] = _to_solid(
            Location((0, 0, 16.80)) * _extrude_plate_on_plane(plates_2d["switch_key"], t)
        )

    gap, margin = 6.0, 5.0
    placed_faces: list[Face] = []
    sheet_labels: list[tuple[str, float, float]] = []
    engrave_placed = engrave_local

    kerfed = {name: _apply_kerf(f, eff_kerf) for name, f in plates_2d.items()}

    def _place_row(names: tuple[str, ...], y_pos: float) -> float:
        nonlocal engrave_placed
        cx, max_h = margin, 0.0
        for name in names:
            f = kerfed[name]
            bb = f.bounding_box()
            shift = Location((cx - bb.min.X, y_pos - bb.min.Y, 0))
            pf = shift * f
            placed_faces.append(pf if isinstance(pf, Face) else pf.faces()[0])
            if name == "back":
                engrave_placed = shift * engrave_local
            short_lbl = {"capture_key": "CAP", "switch_key": "SW"}.get(name, name.upper())
            sheet_labels.append((short_lbl, cx + bb.size.X / 2.0, y_pos - 2.5))
            cx += bb.size.X + (14.0 if name.endswith("_key") else gap)
            max_h = max(max_h, bb.size.Y)
        return max_h

    h0 = _place_row(("front", "back"), margin)
    h1 = _place_row(("top", "bottom"), margin + h0 + gap + 4.0)
    row3 = ("left", "right", "capture_key", "switch_key") if p.is_zero_solder else ("left", "right")
    _place_row(row3, margin + h0 + gap + 4.0 + h1 + gap + 4.0)

    return LaserEnclosure(
        params=p,
        plates_3d=plates_3d,
        plates_2d=plates_2d,
        sheet_2d=Compound(children=placed_faces),
        engrave_2d=engrave_placed,
        sheet_labels=sheet_labels,
    )


def export_laser_sheet(enc: LaserEnclosure, svg_path: Path, dxf_path: Path) -> None:
    """Export 6-plate flat cutting sheet to SVG & DXF with separate CUT & #0066FF ENGRAVE layers."""
    svg_path.parent.mkdir(parents=True, exist_ok=True)
    dxf_path.parent.mkdir(parents=True, exist_ok=True)

    # SVG: Red hairline `cut` layer (#CC0000) + separate Blue `engrave` layer (#0066FF, Max #186!)
    exporter_svg = ExportSVG(unit=Unit.MM, scale=1.0, margin=5.0)
    exporter_svg.add_layer("cut", line_color=(204 / 255, 0.0, 0.0), line_weight=0.15)
    exporter_svg.add_layer("engrave", line_color=(0.0, 102 / 255, 1.0), line_weight=0.15)
    exporter_svg.add_shape(enc.sheet_2d.edges(), layer="cut")
    exporter_svg.add_shape(enc.engrave_2d.edges(), layer="engrave")
    exporter_svg.write(str(svg_path))

    # DXF: Red `CUT` layer + separate Blue `ENGRAVE` layer (1:1 mm)
    exporter_dxf = ExportDXF(unit=Unit.MM, line_weight=0.15)
    exporter_dxf.add_layer("CUT", color=ColorIndex.RED, line_type=LineType.CONTINUOUS)
    exporter_dxf.add_layer("ENGRAVE", color=ColorIndex.BLUE, line_type=LineType.CONTINUOUS)
    exporter_dxf.add_shape(enc.sheet_2d.edges(), layer="CUT")
    exporter_dxf.add_shape(enc.engrave_2d.edges(), layer="ENGRAVE")
    exporter_dxf.write(str(dxf_path))
