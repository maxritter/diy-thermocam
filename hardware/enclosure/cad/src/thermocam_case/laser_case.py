"""Six-plate laser-cut acrylic case (3 mm) for the DIY-Thermocam V4.0.

Front and back plates carry slots; the four side plates carry tabs and interlock at the corners.
The capture key and switch key are cut from the same sheet. Slots are 0.2 mm wider than the
stock, so 2.8..3.2 mm acrylic fits. The back plate's inner face sits at Z=-0.5, the depth of the
standoff seats in the printed shell, so both enclosures share one board stack.
The back plate has a separate engrave layer (#0066FF) with the lens mark and wordmark.
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
    mirror,
    offset,
)

from .accessories import (
    PCB_TOP_EDGE_Y,
    SW2_TIP_Y,
    TRIPOD_PLUG_X,
    TRIPOD_PLUG_Z,
    build_laser_display_spacer,
    build_laser_sd_chute,
    build_laser_tripod_mount,
)
from .params import EnclosureParams

CAPTURE_KEY_Z0 = 18.20      # 3 mm key spans the SW2 plunger (Z 18.8..19.8) and the PCB edge (20..21.6)
SWITCH_KEY_Z0 = 16.80       # 3 mm key spans the SW1 actuator (Z 16.6..18.35)
SWITCH_FORK_TIP_X = 89.50   # 0.5 mm clear of the SW1 body (X<=89.0)
SWITCH_POCKET_W = 1.90


@dataclass(frozen=True)
class LaserEnclosure:
    """Assembled 3D solids and 2D flat cutting/engraving sheets for the 6-plate acrylic case."""

    params: EnclosureParams
    plates_3d: dict[str, Solid]
    plates_2d: dict[str, Face]
    sheet_2d: Compound
    engrave_2d: Compound
    sheet_labels: list[tuple[str, float, float]]
    printed: dict[str, Solid]  # Laser-only printed parts in their assembled position


# Printed parts for the laser case -> copies per camera (the cradle comes from the shell set; its
# FR4 floor and lid are the same boards in both cases)
LASER_PRINT_PARTS: dict[str, int] = {
    "battery_cradle": 1,
    "tripod_mount": 1,
    "sd_chute": 1,
    "display_spacer": 2,
}


def _plate_box_bounds(p: EnclosureParams) -> dict[str, float]:
    t = p.laser_t
    x0 = -p.laser_inner_margin_x_neg
    x1 = p.pcb_w + p.laser_inner_margin_x_pos
    y0 = -p.laser_inner_margin_y_neg
    y1 = p.pcb_h + p.laser_inner_margin_y_pos
    z0 = p.standoff_seat_z
    d_in = p.laser_internal_depth
    assert abs(z0 + d_in - p.internal_depth) < 1e-6, "laser depth must close the board stack"
    over = p.laser_plate_overhang
    return {
        "t": t, "x0": x0, "x1": x1, "y0": y0, "y1": y1,
        "z0": z0, "z1": z0 + d_in, "w_in": x1 - x0, "h_in": y1 - y0, "d_in": d_in,
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
    """Top or bottom plate in local (X, Z - z0) coordinates."""
    t, tab_l = b["t"], p.laser_tab_len
    tb_xs, _ = _fb_tab_centers(b)
    x0, x1, d_in, z0 = b["x0"], b["x1"], b["d_in"], b["z0"]
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
            # Capture-key slot (5.0 mm neck + 0.6) and the printed microSD chute's neck
            with Locations((p.button_x, CAPTURE_KEY_Z0 + t / 2.0 - z0)):
                RectangleRounded(5.60, t + 0.40, 0.30, mode=Mode.SUBTRACT)
            with Locations((p.microsd_x, p.microsd_z - z0)):
                RectangleRounded(p.microsd_w + 2.6, p.microsd_h + 2.6, 0.9, mode=Mode.SUBTRACT)
        else:
            # USB-C plug opening (fits a 13.8 x 7.8 boot) and the printed tripod mount's plug
            with Locations((p.usb.center_x, p.usb.center_z - z0)):
                RectangleRounded(p.usb.laser_width, p.usb.laser_height, 1.00, mode=Mode.SUBTRACT)
            with Locations((p.laser_tripod_x, p.laser_tripod_z - z0)):
                RectangleRounded(TRIPOD_PLUG_X + 0.2, TRIPOD_PLUG_Z + 0.2, 1.6, mode=Mode.SUBTRACT)

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
            # 10.2 mm slot for the switch key's 6.0 mm neck: stops at +-2.1 mm
            with Locations((p.switch_y, SWITCH_KEY_Z0 + t / 2.0 - b["z0"])):
                RectangleRounded(p.switch_h_y, t + 0.40, 0.30, mode=Mode.SUBTRACT)

    return sk.sketch.faces()[0]


def _build_acrylic_keys_2d(p: EnclosureParams, b: dict[str, float]) -> dict[str, Face]:
    """Capture key and switch key, cut from the case sheet (world XY coordinates).

    capture_key: a 10 mm hook bar inside the top plate and a 5 mm neck through its slot, pushed in
    from inside before the top plate goes on. At rest its stem is 0.15 mm above the SW2 plunger;
    pressed 0.7 mm, the stem lands on the PCB edge (the key spans Z 18.2..21.2, across the board),
    which pushes SW2 by the plunger's 0.55 mm overhang.
    switch_key: a fork around the SW1 actuator, a 14 mm shoulder inside the right plate and a
    6 mm neck through its 10.2 mm slot.
    """
    t = b["t"]
    y_plate_in = b["y1"]
    y_plate_out = y_plate_in + t
    y_stem_tip = SW2_TIP_Y + p.button_rest_gap
    y_bar_bot = y_stem_tip + 0.20
    with BuildSketch() as sk_cap:
        with Locations((p.button_x, 0.5 * (y_bar_bot + y_plate_in))):
            RectangleRounded(10.00, y_plate_in - y_bar_bot, 0.30)
        with Locations((p.button_x, 0.5 * (y_stem_tip + y_bar_bot + 0.2))):
            Rectangle(1.60, y_bar_bot + 0.2 - y_stem_tip)
        with Locations((p.button_x, 0.5 * (y_plate_in - 0.2 + y_plate_out + 2.0))):
            RectangleRounded(5.00, y_plate_out + 2.0 - (y_plate_in - 0.2), 0.9)

    x_plate_in = b["x1"]
    x_plate_out = x_plate_in + t
    x_shoulder_in = p.pcb_w + 0.70
    with BuildSketch() as sk_sw:
        with Locations((0.5 * (x_shoulder_in + (x_plate_in - 0.10)), p.switch_y)):
            RectangleRounded((x_plate_in - 0.10) - x_shoulder_in, 14.00, 0.5)
        with Locations((0.5 * (SWITCH_FORK_TIP_X + x_shoulder_in + 0.3), p.switch_y)):
            RectangleRounded(x_shoulder_in + 0.3 - SWITCH_FORK_TIP_X, 7.20, 0.35)
        with Locations((0.5 * (x_plate_in - 0.4 + (x_plate_out + 2.00)), p.switch_y)):
            RectangleRounded((x_plate_out + 2.00) - (x_plate_in - 0.4), 6.00, 0.8)
        # Fork pocket, cut last, ending inside the neck
        with Locations((0.5 * (SWITCH_FORK_TIP_X - 0.1 + x_plate_in - 0.3), p.switch_y)):
            Rectangle(x_plate_in - 0.3 - (SWITCH_FORK_TIP_X - 0.1), SWITCH_POCKET_W, mode=Mode.SUBTRACT)

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
        **_build_acrylic_keys_2d(p, b),
    }

    def _to_solid(obj: object) -> Solid:
        return obj if isinstance(obj, Solid) else obj.solids()[0]  # type: ignore[union-attr]

    z0 = b["z0"]
    left_plane = Plane(origin=(b["x0"] - t, 0, z0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))
    right_plane = Plane(origin=(b["x1"], 0, z0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))
    plates_3d = {
        "front": _to_solid(Location((0, 0, z0 + d_in)) * _extrude_plate_on_plane(front_3d_f, t)),
        "back": _to_solid(Location((0, 0, z0 - t)) * _extrude_plate_on_plane(back_3d_f, t)),
        "bottom": _to_solid(Location((0, b["y0"], z0)) * Rotation(90, 0, 0) * _extrude_plate_on_plane(bottom_2d, t)),
        "top": _to_solid(Location((0, b["y1"] + t, z0)) * Rotation(90, 0, 0) * _extrude_plate_on_plane(top_2d, t)),
        "left": _to_solid(Location(left_plane) * _extrude_plate_on_plane(left_2d, t)),
        "right": _to_solid(Location(right_plane) * _extrude_plate_on_plane(right_2d, t)),
        "capture_key": _to_solid(Location((0, 0, CAPTURE_KEY_Z0)) * _extrude_plate_on_plane(plates_2d["capture_key"], t)),
        "switch_key": _to_solid(Location((0, 0, SWITCH_KEY_Z0)) * _extrude_plate_on_plane(plates_2d["switch_key"], t)),
    }

    gap, margin = 6.0, 5.0
    placed_faces: list[Face] = []
    sheet_labels: list[tuple[str, float, float]] = []
    engrave_placed = engrave_local

    kerfed = {name: _apply_kerf(f, eff_kerf) for name, f in plates_2d.items()}
    # Flip the back plate outside-face UP on the flat cutting sheet so laser ENGRAVE text
    # lands directly on the exterior face and reads forward from the outside when assembled!
    back_mir = mirror(kerfed["back"], about=Plane.YZ.offset(b["fb_cx"]))
    kerfed["back"] = back_mir if isinstance(back_mir, Face) else back_mir.faces()[0]

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
    _place_row(("left", "right", "capture_key", "switch_key"),margin + h0 + gap + 4.0 + h1 + gap + 4.0)

    return LaserEnclosure(
        params=p,
        plates_3d=plates_3d,
        plates_2d=plates_2d,
        sheet_2d=Compound(children=placed_faces),
        engrave_2d=engrave_placed,
        sheet_labels=sheet_labels,
        printed={
            "tripod_mount": build_laser_tripod_mount(p),
            "sd_chute": build_laser_sd_chute(p),
            "display_spacer_bottom": build_laser_display_spacer(p, top=False),
            "display_spacer_top": build_laser_display_spacer(p, top=True),
        },
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
