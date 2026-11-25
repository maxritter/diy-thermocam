"""Parametric 2-piece 3D-printable / CNC-ready enclosure + mechanical accessories.

Incorporates ALL Stage 1 Carbon (#90/#169), Fable 5.2 (#176), GPT-6.2 Astra (#184),
and Max #186 ($0-extra-cost industrial design) requirements:
  - ENC-3D-P0-1: Tripod mount cleanly on back_shell (X=44.70, Z=7.20, Z=0..14.4) with 3.0 mm
    blind roof (stops 10 mm stud!), 0.000 mm^3 collision w/ real 89.4x64.9 ER-TFTM032-3 PCB!
  - ENC-3D-P0-2: Flat-printing battery_cradle (1.8 mm pin vault > 1.55 mm real THT tails,
    Ø6.2 mm standoff bores) + flat battery_lid roof shield over swollen 5.5 mm 505060 cell!
  - ENC-3D-P0-3: slide_switch_cap thumb pad 3.4 mm < 4.0 mm slot (0.000 mm^3 assembly drop-in!),
    12.2 mm slot (+-3.0 mm throw), deep fork engaging SW1 across X=89.55..91.1 mm.
  - ENC-3D-P1-1: Full-depth 13.8x7.8x2.8 mm USB-C boot counterbore across BOTH back & front shells.
  - ENC-3D-P1-2: Zero Z-datum drift (flat Z=0 & Z=39.20 standoff seats), 45 deg support-free
    capture_button aligned w/ SW2 at Z=19.15 + hard travel stop.
  - ENC-3D-P1-3: V3 NKK rocker 19.3x6.9 mm cutout + internal 1.0 mm wall relief (1.6 mm latch seat).
  - ENC-3D-P1-4: True one-end cantilever lens_cap detent actively engaging floor notches + top stop.
  - ENC-3D-P1-5: Support-free 45 deg ramped microSD chute reaching Y=61.5 + deep 22 mm nail scoop.
  - #186 Design: 0.40 mm debossed Version 1 concentric thermal-lens mark + chamfered side grip ribs.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from build123d import (
    Align,
    BuildPart,
    BuildSketch,
    Circle,
    Compound,
    Cone,
    Cylinder,
    FontStyle,
    Location,
    Locations,
    Mesher,
    Mode,
    Plane,
    Rectangle,
    RectangleRounded,
    RegularPolygon,
    Rotation,
    SlotOverall,
    Solid,
    Text,
    export_step,
    export_stl,
    extrude,
    loft,
)

from .params import EnclosureParams
from .v4_parts import (
    _as_solid,
    _xz_plane,
    _yz_plane,
    build_battery_cradle,
    build_battery_lid,
    build_capture_button_plunger,
    build_lens_cap,
    build_slide_switch_cap,
)


@dataclass(frozen=True)
class PrintableEnclosure:
    params: EnclosureParams
    front_shell: Solid
    back_shell: Solid
    lens_cap_closed: Solid
    lens_cap_open: Solid
    capture_button: Solid | None = None
    slide_switch_cap: Solid | None = None
    battery_cradle: Solid | None = None
    battery_lid: Solid | None = None


def _shell_envelope(p: EnclosureParams) -> dict[str, float]:
    ix0, ix1 = -p.shell_clearance_x_neg, p.pcb_w + p.shell_clearance_x_pos
    iy0, iy1 = -p.shell_clearance_y_neg, p.pcb_h + p.shell_clearance_y_pos
    w = p.shell_wall
    ox0, ox1 = ix0 - w, ix1 + w
    oy0, oy1 = iy0 - w, iy1 + w
    z_back_outer = -p.shell_back_face_t
    z_back_seam = p.shell_split_z
    z_front_seam = z_back_seam + 0.20

    return {
        "w": w,
        "back_face_t": p.shell_back_face_t,
        "front_face_t": p.shell_face_t,
        "ix0": ix0, "ix1": ix1, "iy0": iy0, "iy1": iy1,
        "iw": ix1 - ix0, "ih": iy1 - iy0,
        "icx": 0.5 * (ix0 + ix1), "icy": 0.5 * (iy0 + iy1),
        "ox0": ox0, "ox1": ox1, "oy0": oy0, "oy1": oy1,
        "ow": ox1 - ox0, "oh": oy1 - oy0,
        "ocx": 0.5 * (ox0 + ox1), "ocy": 0.5 * (oy0 + oy1),
        "z_back_outer": z_back_outer,
        "z_back_inner": 0.0,
        "z_front_inner": p.internal_depth,
        "z_front_outer": p.internal_depth + p.shell_face_t,
        "z_back_seam": z_back_seam,
        "z_front_seam": z_front_seam,
        "track_y_min": 15.20,
        "track_y_max": oy1 + 0.50,
        "track_w_outer": 25.00,  # Clears full Lepton 3.1R 95 deg HFOV / 119 deg DFOV cone at Z=-3.60!
        "track_w_inner": 28.00,
        "track_depth": 1.50,
        "z_track_floor": z_back_outer + 1.50,
        "cap_open_y": 26.80,     # Top edge Y=37.80 clears 119 deg DFOV cone (13.45 mm > 12.38 mm)!
        "cap_closed_y": p.lepton_y,
    }


def _cut_slot_or_unotch(
    cx: float, cz: float, width: float, height: float, corner_r: float,
    z_seam: float, plane: Plane, extrude_amt: float,
) -> None:
    top_z, bot_z = cz + height / 2.0, cz - height / 2.0
    with BuildSketch(plane) as sk:
        if top_z >= z_seam - 1.5:
            # Keep top fillet completely above z_seam so U-notch walls stay straight for +Z drop-in!
            r = min(corner_r, width / 2.2)
            slot_h = (z_seam + r + 1.0) - bot_z
            slot_cz = bot_z + slot_h / 2.0
            with Locations((cx, slot_cz)):
                RectangleRounded(width, slot_h, min(r, slot_h / 2.2))
        else:
            with Locations((cx, cz)):
                SlotOverall(width, height)
    extrude(sk.sketch, amount=extrude_amt, mode=Mode.SUBTRACT)


def _build_back_shell(p: EnclosureParams, e: dict[str, float]) -> Solid:
    """Build Lepton / Teensy rear shell half (prints Lepton-face down, zero Z-datum drift)."""
    z0, z_in, z_seam = e["z_back_outer"], e["z_back_inner"], e["z_back_seam"]
    ch = p.shell_edge_chamfer

    with BuildPart() as bp:
        # 1. Outer rounded shell with 45 deg perimeter grip chamfer
        with BuildSketch(Plane.XY.offset(z0)):
            with Locations((e["ocx"], e["ocy"])):
                RectangleRounded(e["ow"] - 2 * ch, e["oh"] - 2 * ch, max(p.shell_corner_r - ch, 0.8))
        with BuildSketch(Plane.XY.offset(z0 + ch)):
            with Locations((e["ocx"], e["ocy"])):
                RectangleRounded(e["ow"], e["oh"], p.shell_corner_r)
        loft()
        with BuildSketch(Plane.XY.offset(z0 + ch)):
            with Locations((e["ocx"], e["ocy"])):
                RectangleRounded(e["ow"], e["oh"], p.shell_corner_r)
        extrude(amount=z_seam - (z0 + ch))

        # 2. Hollow interior cavity (wall = 2.60 mm; flat Z=0.00 standoff seating -> 0.00 mm Z drift!)
        with BuildSketch(Plane.XY.offset(z_in)):
            with Locations((e["icx"], e["icy"])):
                RectangleRounded(e["iw"], e["ih"], 1.40)
        extrude(amount=z_seam - z_in + 1.0, mode=Mode.SUBTRACT)

        # 3. Flush DIN 965 / ISO 10642 M2.5 countersinks (Ø5.00 mm -> exact flush seat!)
        cs_depth = (p.countersink_head_d - p.mounting_hole_d) / 2.0
        for mx, my in p.mounting_holes:
            with Locations((mx, my, z0 - 0.5)):
                Cylinder(p.mounting_hole_d / 2.0, e["back_face_t"] + 2.0,
                         align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
            with Locations((mx, my, z0 - 0.05)):
                Cone(p.countersink_head_d / 2.0 + 0.05, p.mounting_hole_d / 2.0, cs_depth + 0.05,
                     align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

        # 4. ENC-3D-P0-1: Support-Free Captive Metal 1/4"-20 UNC Tripod Boss on back_shell!
        # Rises directly from flat Z=0.0 inner back wall up to Z=14.40 (0 deg overhangs = support-free!).
        # Zero collision w/ Display PCB (Z=32.6), Main PCB (Z=20.0), USB-C (Z=16.8..20.0), or LiPo!
        # Floor Y=-3.8..-1.6 (2.2 mm), Nut Y=-1.6..+4.2 (5.8 mm), Solid Blind Roof Y=+4.2..+7.2 (3.0 mm!)
        y_nut_bot = e["oy0"] + p.shell_tripod_floor_t       # -1.60 mm
        y_nut_top = y_nut_bot + p.shell_tripod_nut_h        # +4.20 mm
        y_boss_top = y_nut_top + p.shell_tripod_roof_t      # +7.20 mm (11.8 mm below Lepton!)
        tz = p.shell_tripod_z                               # 7.20 mm
        boss_z_end = p.shell_tripod_boss_z_end              # 14.40 mm

        with BuildSketch(Plane.XY.offset(z_in - 0.10)):
            with Locations((p.tripod_x, 0.5 * (e["iy0"] - 0.4 + y_boss_top))):
                RectangleRounded(p.shell_tripod_boss_w, y_boss_top - (e["iy0"] - 0.4), 1.6)
        extrude(amount=boss_z_end - (z_in - 0.10))

        # +Z top-loading slide-in channel for metal 1/4"-20 hex nut (prints vertically, 0 overhangs!)
        nut_af = p.shell_tripod_hex_af
        hex_r = nut_af / (3.0**0.5)
        with BuildSketch(Plane.XY.offset(tz - hex_r)):
            with Locations((p.tripod_x, 0.5 * (y_nut_bot + y_nut_top))):
                Rectangle(nut_af, p.shell_tripod_nut_h)
        extrude(amount=(boss_z_end + 0.5) - (tz - hex_r), mode=Mode.SUBTRACT)

        with BuildSketch(_xz_plane(y_nut_top)):
            with Locations((p.tripod_x, tz)):
                RegularPolygon(radius=hex_r, side_count=6, rotation=30.0)
        extrude(amount=p.shell_tripod_nut_h, mode=Mode.SUBTRACT)

        # Bottom 6.80 mm tripod stud bore ONLY through floor into nut cavity (NEVER pierces blind roof!)
        with BuildSketch(_xz_plane(e["oy0"] - 0.5)):
            with Locations((p.tripod_x, tz)):
                Circle(p.shell_tripod_screw_d / 2.0)
        extrude(amount=-(p.shell_tripod_floor_t + p.shell_tripod_nut_h + 0.20), mode=Mode.SUBTRACT)

        # 5. ENC-3D-P1-5: Support-Free microSD Guide Chute pedestal reaching Y=61.20 (0.09 mm from Eagle SD!)
        # Rests directly on flat Z=0.0 inner back face -> ZERO overhangs when printing back_shell face-down!
        y_chute_end = 61.20
        chute_len = (e["iy1"] + 0.40) - y_chute_end
        chute_top_z = p.microsd_z + p.microsd_h / 2.0 + 1.40  # 9.60 mm
        with BuildSketch(Plane.XY.offset(z_in - 0.10)):
            with Locations((p.microsd_x, 0.5 * (y_chute_end + e["iy1"] + 0.40))):
                RectangleRounded(p.microsd_w + 3.0, chute_len, 1.0)
        extrude(amount=chute_top_z - (z_in - 0.10))

        # 6. ENC-3D-P1-2: Internal U-cradle capture plunger guide pocket w/ positive Y=68.95 hard-stop!
        # Flange bottom (Y=69.50) stops firmly on the Y=68.95 shelf after 0.55 mm travel (0.40 mm SW2 stroke);
        # center stem slot opens down to Y=68.50 so stem travels freely to Y=68.55 (0.15 mm clear of PCB!).
        if p.is_zero_solder:
            with BuildSketch(_xz_plane(e["iy1"] + 0.40)):
                with Locations((p.button_x, 18.20)):
                    RectangleRounded(12.20, 7.20, 1.4)
            extrude(amount=(e["iy1"] + 0.40) - 68.55)  # Solid block down to Y=68.55 (0.15 mm clear of PCB!)
            # Straight-walled +Z drop-in flange cavity stopping at Y=68.95 (housing hard-stop shelf!)
            with BuildSketch(_xz_plane(e["iy1"])):
                with Locations((p.button_x, 20.10)):
                    RectangleRounded(9.80, 10.60, 1.4)
            extrude(amount=e["iy1"] - 68.95, mode=Mode.SUBTRACT)
            # Center stem through-slot below Y=68.95 (width 4.0 mm > 3.0 mm stem)
            with BuildSketch(_xz_plane(69.10)):
                with Locations((p.button_x, 20.10)):
                    RectangleRounded(4.00, 10.60, 0.8)
            extrude(amount=0.80, mode=Mode.SUBTRACT)

        # 7. ENC-3D-P1-4: Top-open 45 deg dovetail slide track + deep floor cantilever detent notches!
        track_h = e["track_y_max"] - e["track_y_min"]
        track_cy = 0.5 * (e["track_y_min"] + e["track_y_max"])
        with BuildSketch(Plane.XY.offset(z0 - 0.05)):
            with Locations((p.lepton_x, track_cy)):
                Rectangle(e["track_w_outer"] - 0.10, track_h)
        with BuildSketch(Plane.XY.offset(e["z_track_floor"])):
            with Locations((p.lepton_x, track_cy)):
                Rectangle(e["track_w_inner"], track_h)
        loft(mode=Mode.SUBTRACT)

        # Planar floor detent notches (Z=-2.10..-1.65) actively engaged by lens_cap inboard cantilever bumps!
        finger_x_off = 8.40
        for dy in (e["cap_open_y"] + 1.50, e["cap_closed_y"] + 1.50, 61.50 + 1.50):
            for sx in (-1.0, 1.0):
                with BuildSketch(Plane.XY.offset(e["z_track_floor"] - 0.05)):
                    with Locations((p.lepton_x + sx * finger_x_off, dy)):
                        RectangleRounded(2.00, 1.80, 0.40)
                extrude(amount=0.50, mode=Mode.SUBTRACT)

        # 8. Widened 13.4 -> 20.2 mm chamfered Lepton optical cone (clears Lepton 3.5 71 deg AND 3.1R 119 deg DFOV!)
        r_in = p.shell_lepton_inner_d / 2.0
        r_out = p.shell_lepton_outer_d / 2.0
        cone_h = z_in - e["z_track_floor"] + 0.20
        with Locations((p.lepton_x, p.lepton_y, e["z_track_floor"] - 0.10)):
            Cone(r_out, r_in, cone_h,
                 align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        with Locations((p.lepton_x, p.lepton_y, -0.10)):
            Cylinder(r_in, 0.80, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

        # 9. Teensy / SW3 Prog button pinhole (V3: 66.90, 43.08 exact PJRC; V4 SW3: 85.30, 4.00)
        with Locations((p.teensy_prog_x, p.teensy_prog_y, z0 - 0.5)):
            Cylinder(p.teensy_prog_hole_d / 2.0, e["back_face_t"] + 1.5,
                     align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        with Locations((p.teensy_prog_x, p.teensy_prog_y, z0 - 0.05)):
            Cone(p.teensy_prog_hole_d / 2.0 + 0.80, p.teensy_prog_hole_d / 2.0, 0.80,
                 align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

        # 10. ENC-3D-P1-1: Bottom-wall USB tongue slot + full-depth 2.8 mm boot counterbore!
        _cut_slot_or_unotch(
            p.usb.center_x, p.usb.center_z, p.usb.width, p.usb.height,
            p.usb.corner_radius, z_seam, _xz_plane(e["oy0"] - 1.0), -(e["w"] + 2.5),
        )
        _cut_slot_or_unotch(
            p.usb.center_x, p.usb.center_z, p.usb.overmold_width, p.usb.overmold_height,
            1.00, z_seam, _xz_plane(e["oy0"] - 0.05), -(p.usb.overmold_depth + 0.05),
        )

        # 11. Top-wall microSD card guide tunnel + deep 22 mm fingertip/nail scoop (P1-5)
        with BuildSketch(_xz_plane(e["oy1"] + 1.0)):
            with Locations((p.microsd_x, p.microsd_z)):
                SlotOverall(p.microsd_w, p.microsd_h)
        extrude(amount=e["w"] + chute_len + 2.0, mode=Mode.SUBTRACT)
        with BuildSketch(_xz_plane(e["oy1"] + 0.05)):
            with Locations((p.microsd_x, p.microsd_z)):
                RectangleRounded(22.00, 8.20, 2.4)
        extrude(amount=1.80, mode=Mode.SUBTRACT)

        # 12. V4.0 onboard controls on B.Cu Lepton side
        if p.is_zero_solder:
            _cut_slot_or_unotch(
                p.button_x, p.button_z, p.button_plunger_bore_d, p.button_plunger_bore_d,
                p.button_plunger_bore_d / 2.0 - 0.1, z_seam, _xz_plane(e["oy1"] + 1.0), e["w"] + 2.5,
            )
            _cut_slot_or_unotch(
                p.switch_y, p.switch_z, p.switch_h_y, p.switch_w_z,
                1.4, z_seam, _yz_plane(e["ox1"] + 1.0), -(e["w"] + 2.5),
            )
            with BuildSketch(_yz_plane(e["ox1"] + 0.05)):
                with Locations((p.switch_y, min(p.switch_z, z_seam - 2.2))):
                    RectangleRounded(p.switch_recess_y, 7.0, 2.0)
            extrude(amount=-0.90, mode=Mode.SUBTRACT)

            for lx, ly in p.back_face_led_holes:
                with Locations((lx, ly, z0 - 0.5)):
                    Cylinder(p.back_face_led_d / 2.0, e["back_face_t"] + 1.5,
                             align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
                with Locations((lx, ly, z0 - 0.05)):
                    Cone(p.back_face_led_d / 2.0 + 0.65, p.back_face_led_d / 2.0, 0.65,
                         align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

        # 13. Max #186 $0-extra-cost stylish industrial design:
        #     (a) Shallow 0.40 mm debossed Version 1 concentric thermal-lens mark + wordmark
        #         Cleanly centered below track_y_min=15.20 (Y in [4.2, 12.8], clear of mounting holes!)
        brand_plane = Plane(origin=(e["ocx"], 0.0, z0), x_dir=(-1, 0, 0), z_dir=(0, 0, -1))
        with BuildSketch(brand_plane):
            for r_out_b, r_in_b in ((3.6, 2.8), (2.1, 1.4), (0.8, 0.0)):
                with Locations((-24.0, 8.6)):
                    Circle(r_out_b)
                    if r_in_b > 0:
                        Circle(r_in_b, mode=Mode.SUBTRACT)
            with Locations((4.0, 10.6)):
                Text("DIY-THERMOCAM", font_size=4.2, font_style=FontStyle.BOLD)
            with Locations((4.0, 5.6)):
                Text(p.version_tag, font_size=2.8, font_style=FontStyle.REGULAR)
        extrude(amount=-0.40, mode=Mode.SUBTRACT)

        #     (b) Subtle 0.45 mm ergonomic tactile grip grooves along Left (-X) flank
        for gy in (24.0, 29.0, 34.0, 39.0, 44.0):
            with BuildSketch(_yz_plane(e["ox0"] - 0.05)):
                with Locations((gy, 10.5)):
                    RectangleRounded(1.80, 14.0, 0.6)
            extrude(amount=0.45, mode=Mode.SUBTRACT)

    return _as_solid(bp.part)


def _build_front_shell(p: EnclosureParams, e: dict[str, float]) -> Solid:
    """Build Display front shell half (prints Display-face down, 100% support-free, ZERO Display crush!)."""
    z_seam, z_in, z_out = e["z_front_seam"], e["z_front_inner"], e["z_front_outer"]
    ch = p.shell_edge_chamfer

    with BuildPart() as bp:
        # 1. Outer rounded shell with 45 deg perimeter grip chamfer
        with BuildSketch(Plane.XY.offset(z_seam)):
            with Locations((e["ocx"], e["ocy"])):
                RectangleRounded(e["ow"], e["oh"], p.shell_corner_r)
        extrude(amount=(z_out - ch) - z_seam)
        with BuildSketch(Plane.XY.offset(z_out - ch)):
            with Locations((e["ocx"], e["ocy"])):
                RectangleRounded(e["ow"], e["oh"], p.shell_corner_r)
        with BuildSketch(Plane.XY.offset(z_out)):
            with Locations((e["ocx"], e["ocy"])):
                RectangleRounded(e["ow"] - 2 * ch, e["oh"] - 2 * ch, max(p.shell_corner_r - ch, 0.8))
        loft()

        # 2. Hollow interior cavity (wall = 2.60 mm; flat Z=39.20 standoff seat -> 0.00 mm Z drift!)
        with BuildSketch(Plane.XY.offset(z_seam - 1.0)):
            with Locations((e["icx"], e["icy"])):
                RectangleRounded(e["iw"], e["ih"], 1.40)
        extrude(amount=(z_in - z_seam) + 1.0, mode=Mode.SUBTRACT)

        # 3. Fused seam alignment tabs (clear of PCB, Display PCB, SW1, and J4 wires!)
        tab_t, tab_gap, foot_h, tongue_h = 0.90, 0.25, 2.00, 1.40
        foot_w = 0.5 + tab_gap + tab_t
        for ty in (20.0, 50.0):
            with BuildSketch(Plane.XY.offset(z_seam)):
                with Locations((e["ix0"] - 0.5 + foot_w / 2.0, ty)):
                    Rectangle(foot_w, 12.0)
            extrude(amount=foot_h)
            with BuildSketch(Plane.XY.offset(z_seam)):
                with Locations((e["ix0"] + tab_gap + tab_t / 2.0, ty)):
                    Rectangle(tab_t, 12.0)
            extrude(amount=-tongue_h)

        right_tabs = (38.0,) if p.is_zero_solder else (16.0, 54.0)
        for ty in right_tabs:
            with BuildSketch(Plane.XY.offset(z_seam)):
                with Locations((e["ix1"] + 0.5 - foot_w / 2.0, ty)):
                    Rectangle(foot_w, 9.0)
            extrude(amount=foot_h)
            with BuildSketch(Plane.XY.offset(z_seam)):
                with Locations((e["ix1"] - tab_gap - tab_t / 2.0, ty)):
                    Rectangle(tab_t, 9.0)
            extrude(amount=-tongue_h)

        # 4. ENC-3D-P1-1: V4 USB-C full-depth boot counterbore relief notch on front_shell bottom rim (Z>=22.6)!
        if p.is_zero_solder:
            with BuildSketch(_xz_plane(e["oy0"] - 0.05)):
                with Locations((p.usb.center_x, z_seam + 0.10)):
                    RectangleRounded(p.usb.overmold_width, 1.40, 0.6)
            extrude(amount=-(p.usb.overmold_depth + 0.05), mode=Mode.SUBTRACT)

        # 5. Flush DIN 965 / ISO 10642 M2.5 countersinks on front face
        cs_depth = (p.countersink_head_d - p.mounting_hole_d) / 2.0
        for mx, my in p.mounting_holes:
            with Locations((mx, my, z_in - 1.0)):
                Cylinder(p.mounting_hole_d / 2.0, e["front_face_t"] + 2.0,
                         align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
            with Locations((mx, my, z_out - cs_depth)):
                Cone(p.mounting_hole_d / 2.0, p.countersink_head_d / 2.0 + 0.05, cs_depth + 0.05,
                     align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

        # 6. Display active viewing window (64.80 x 48.60 mm) with 45 deg touch-friendly bezel
        bevel = 1.60
        with BuildSketch(Plane.XY.offset(z_in - 0.5)):
            with Locations((p.display_win_cx, p.display_win_cy)):
                RectangleRounded(p.display_win_w, p.display_win_h, 0.6)
        extrude(amount=e["front_face_t"] + 1.0, mode=Mode.SUBTRACT)
        with BuildSketch(Plane.XY.offset(z_out - bevel)):
            with Locations((p.display_win_cx, p.display_win_cy)):
                RectangleRounded(p.display_win_w, p.display_win_h, 0.6)
        with BuildSketch(Plane.XY.offset(z_out + 0.1)):
            with Locations((p.display_win_cx, p.display_win_cy)):
                RectangleRounded(p.display_win_w + 2 * (bevel + 0.1),
                                 p.display_win_h + 2 * (bevel + 0.1), 1.2)
        loft(mode=Mode.SUBTRACT)

        # 7. V3.0 panel-mount controls on front_shell (ENC-3D-P1-3: 19.3x6.9 + 1.6 mm latch seat!)
        if not p.is_zero_solder:
            with BuildSketch(_yz_plane(e["ox1"] + 1.0)):
                with Locations((p.switch_y, p.switch_z)):
                    Rectangle(p.switch_h_y, p.switch_w_z)
            extrude(amount=-(e["w"] + 2.5), mode=Mode.SUBTRACT)
            # Internal 1.0 mm latch relief -> local panel thickness = 1.60 mm (inside NKK 0.75..2.00 mm!)
            with BuildSketch(_yz_plane(e["ix1"] - 0.10)):
                with Locations((p.switch_y, p.switch_z)):
                    Rectangle(p.switch_h_y + 4.0, p.switch_w_z + 2.4)
            extrude(amount=1.10, mode=Mode.SUBTRACT)

            with BuildSketch(_xz_plane(e["oy1"] + 1.0)):
                with Locations((p.button_x, p.button_z)):
                    Circle(p.button_hole_d / 2.0)
                with Locations((p.led_x, p.led_z)):
                    Circle(p.shell_led_d / 2.0)
            extrude(amount=e["w"] + 2.5, mode=Mode.SUBTRACT)

        # 8. Matching subtle ergonomic grip grooves on Left (-X) flank (#186)
        for gy in (24.0, 29.0, 34.0, 39.0, 44.0):
            with BuildSketch(_yz_plane(e["ox0"] - 0.05)):
                with Locations((gy, 31.0)):
                    RectangleRounded(1.80, 12.0, 0.6)
            extrude(amount=0.45, mode=Mode.SUBTRACT)

    return _as_solid(bp.part)


def build_printable_enclosure(p: EnclosureParams) -> PrintableEnclosure:
    e = _shell_envelope(p)
    return PrintableEnclosure(
        params=p,
        front_shell=_build_front_shell(p, e),
        back_shell=_build_back_shell(p, e),
        lens_cap_closed=build_lens_cap(p, e, y_center=e["cap_closed_y"]),
        lens_cap_open=build_lens_cap(p, e, y_center=e["cap_open_y"]),
        capture_button=build_capture_button_plunger(p, e) if p.is_zero_solder else None,
        slide_switch_cap=build_slide_switch_cap(p, e) if p.is_zero_solder else None,
        battery_cradle=build_battery_cradle(p) if p.is_zero_solder else None,
        battery_lid=build_battery_lid(p) if p.is_zero_solder else None,
    )


def orient_for_printing(enc: PrintableEnclosure) -> dict[str, Solid]:
    """Return print-ready solids oriented flat on Z=0 (100% support-free, incl. floor-down cradle!)."""
    def _ground(s: Solid) -> Solid:
        bb = s.bounding_box()
        return _as_solid(Location((-bb.min.X, -bb.min.Y, -bb.min.Z)) * s)

    parts: dict[str, Solid] = {
        "front_shell": _ground(Rotation(180, 0, 0) * enc.front_shell),
        "back_shell": _ground(enc.back_shell),
        # Outer face (Z=z_out) flat on build plate -> 45 deg outward dovetail flare & upward detents = support-free!
        "lens_cap": _ground(enc.lens_cap_closed),
    }
    if enc.capture_button is not None:
        parts["capture_button"] = _ground(Rotation(-90, 0, 0) * enc.capture_button)
    if enc.slide_switch_cap is not None:
        parts["slide_switch_cap"] = _ground(enc.slide_switch_cap)
    if enc.battery_cradle is not None:
        # Fable #5 / Astra P1-7 fix: FLOOR-DOWN upright on Z=0 -> ZERO bridges, ZERO supports!
        parts["battery_cradle"] = _ground(enc.battery_cradle)
    if enc.battery_lid is not None:
        parts["battery_lid"] = _ground(enc.battery_lid)
    return parts


def export_printable_enclosure(enc: PrintableEnclosure, out_dir: Path) -> list[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    exported: list[Path] = []
    prefix = f"diy-thermocam-{enc.params.variant.replace('_', '.')}"
    print_parts = orient_for_printing(enc)

    for part_name, solid in print_parts.items():
        step_path = out_dir / f"{prefix}-{part_name}.step"
        stl_path = out_dir / f"{prefix}-{part_name}.stl"
        threemf_path = out_dir / f"{prefix}-{part_name}.3mf"
        export_step(solid, str(step_path))
        export_stl(solid, str(stl_path), tolerance=0.02, angular_tolerance=0.1)
        m = Mesher()
        m.add_shape(solid)
        m.write(str(threemf_path))
        exported.extend([step_path, stl_path, threemf_path])

    asm_children = [enc.front_shell, enc.back_shell, enc.lens_cap_open]
    for opt in (enc.capture_button, enc.slide_switch_cap, enc.battery_cradle, enc.battery_lid):
        if opt is not None:
            asm_children.append(opt)
    asm_step = out_dir / f"{prefix}-assembly.step"
    export_step(Compound(children=asm_children, label=f"{prefix}-assembly"), str(asm_step))
    exported.append(asm_step)

    plate_mesher = Mesher()
    cursor_x, cursor_y, row_h = 0.0, 0.0, 0.0
    for idx, solid in enumerate(print_parts.values()):
        bb = solid.bounding_box()
        if idx == 3:
            cursor_x, cursor_y, row_h = 0.0, row_h + 10.0, 0.0
        plate_mesher.add_shape(_as_solid(Location((cursor_x, cursor_y, 0.0)) * solid))
        cursor_x += bb.size.X + 8.0
        row_h = max(row_h, bb.size.Y)
    plate_3mf = out_dir / f"{prefix}-print-plate.3mf"
    plate_mesher.write(str(plate_3mf))
    exported.append(plate_3mf)
    return exported
