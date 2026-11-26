"""Two-piece 3D-printed V4.0 shell (back and front halves) plus its printed accessories.

Back shell: Lepton opening and lens-cap track, captive 1/4"-20 tripod nut, microSD chute, capture
button guide, slide-switch slot, LED and Prog guide tubes, 0.5 mm standoff seats.
Front shell: display window and four 5 mm screw bosses that clamp the display PCB.
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
from .accessories import (
    BUTTON_FLANGE_X,
    BUTTON_FLANGE_Z,
    CAP_BUMP_DY,
    CAP_FINGER_X,
    PCB_TOP_EDGE_Y,
    SD_CHUTE_LEN_IN,
    _as_solid,
    _xz_plane,
    _yz_plane,
    build_battery_cradle,
    build_battery_floor,
    build_battery_lid,
    build_capture_button_plunger,
    build_lens_cap,
    build_slide_switch_cap,
)


# Printed part name -> copies per camera
PRINT_PARTS: dict[str, int] = {
    "front_shell": 1,
    "back_shell": 1,
    "lens_cap": 1,
    "capture_button": 1,
    "slide_switch_cap": 1,
    "battery_cradle": 1,
}
# Bare FR4 boards from the PCB maker (too thin to print), shared by both enclosures
FR4_PARTS: dict[str, float] = {"battery_floor": 0.6, "battery_lid": 0.4}  # Name -> board thickness


@dataclass(frozen=True)
class PrintableEnclosure:
    params: EnclosureParams
    front_shell: Solid
    back_shell: Solid
    lens_cap_closed: Solid
    lens_cap_open: Solid
    capture_button: Solid
    slide_switch_cap: Solid
    battery_cradle: Solid
    battery_floor: Solid   # FR4
    battery_lid: Solid     # FR4


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

        # 2. Interior cavity
        with BuildSketch(Plane.XY.offset(z_in)):
            with Locations((e["icx"], e["icy"])):
                RectangleRounded(e["iw"], e["ih"], 1.40)
        extrude(amount=z_seam - z_in + 1.0, mode=Mode.SUBTRACT)

        # 3. Standoff seats (0.5 mm deep) and flush ISO 7046 countersinks for the M2.5x8 screws
        cs_depth = (p.countersink_head_d - p.mounting_hole_d) / 2.0
        for mx, my in p.mounting_holes:
            with Locations((mx, my, p.standoff_seat_z)):
                Cylinder(p.standoff_seat_d / 2.0, -p.standoff_seat_z + 0.1,
                         align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
            with Locations((mx, my, z0 - 0.5)):
                Cylinder(p.mounting_hole_d / 2.0, e["back_face_t"] + 2.0,
                         align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
            with Locations((mx, my, z0 - 0.05)):
                Cone(p.countersink_head_d / 2.0 + 0.05, p.mounting_hole_d / 2.0, cs_depth + 0.05,
                     align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

        # 4. Captive 1/4"-20 tripod nut in a boss on the bottom wall: 1.2 mm floor, nut slides in
        # from +Z, 3 mm blind roof above it
        y_nut_bot = e["oy0"] + p.shell_tripod_floor_t
        y_nut_top = y_nut_bot + p.shell_tripod_nut_h
        y_boss_top = y_nut_top + p.shell_tripod_roof_t
        tz = p.shell_tripod_z
        boss_z_end = p.shell_tripod_boss_z_end

        with BuildSketch(Plane.XY.offset(z_in - 0.10)):
            with Locations((p.tripod_x, 0.5 * (e["iy0"] - 0.4 + y_boss_top))):
                RectangleRounded(p.shell_tripod_boss_w, y_boss_top - (e["iy0"] - 0.4), 1.6)
        extrude(amount=boss_z_end - (z_in - 0.10))

        # +Z top-loading slide-in channel for metal 1/4"-20 hex nut (prints vertically, 0 overhangs!).
        # Inside the bottom wall it runs up into the USB-C boot recess, so no thin web is left over
        # the nut's way in
        nut_af = p.shell_tripod_hex_af
        hex_r = nut_af / (3.0**0.5)
        usb_recess_bot = p.usb.center_z - p.usb.overmold_height / 2.0
        channel_top = max(boss_z_end + 0.5, usb_recess_bot + 0.3)
        with BuildSketch(Plane.XY.offset(tz - hex_r)):
            with Locations((p.tripod_x, 0.5 * (y_nut_bot + y_nut_top))):
                Rectangle(nut_af, p.shell_tripod_nut_h)
        extrude(amount=channel_top - (tz - hex_r), mode=Mode.SUBTRACT)

        with BuildSketch(_xz_plane(y_nut_top)):
            with Locations((p.tripod_x, tz)):
                RegularPolygon(radius=hex_r, side_count=6, rotation=30.0)
        extrude(amount=p.shell_tripod_nut_h, mode=Mode.SUBTRACT)

        # Bottom 6.80 mm tripod stud bore ONLY through floor into nut cavity (NEVER pierces blind roof!)
        with BuildSketch(_xz_plane(e["oy0"] - 0.5)):
            with Locations((p.tripod_x, tz)):
                Circle(p.shell_tripod_screw_d / 2.0)
        # Through the floor and the nut cavity plus a 1 mm relief in the roof, for up to 8 mm screws
        extrude(amount=-(0.5 + p.shell_tripod_floor_t + p.shell_tripod_nut_h + 1.00), mode=Mode.SUBTRACT)

        # 5. microSD chute: a pedestal on the back wall with the card tunnel (cut in step 11) ending
        # 0.8 mm before the Teensy's end (Y=61.11)
        y_chute_end = 61.11 + SD_CHUTE_LEN_IN
        chute_len = (e["iy1"] + 0.40) - y_chute_end
        chute_top_z = p.microsd_z + p.microsd_h / 2.0 + 1.40
        with BuildSketch(Plane.XY.offset(z_in - 0.10)):
            with Locations((p.microsd_x, 0.5 * (y_chute_end + e["iy1"] + 0.40))):
                RectangleRounded(p.microsd_w + 3.0, chute_len, 1.0)
        extrude(amount=chute_top_z - (z_in - 0.10))

        # 6. Capture-button guide: a block on the top wall with a pocket for the flange, open toward
        # +Z (drop-in) and toward the PCB, so the flange can reach its stop on the PCB edge.
        # The block ends 0.5 mm above the PCB edge.
        guide_bot = PCB_TOP_EDGE_Y + 0.50
        pocket_x, pocket_z = BUTTON_FLANGE_X + 0.50, BUTTON_FLANGE_Z + 0.50
        z_block0 = p.button_z - pocket_z / 2.0 - 1.2
        with BuildSketch(_xz_plane(e["iy1"] + 0.40)):
            with Locations((p.button_x, 0.5 * (z_block0 + z_seam))):
                RectangleRounded(pocket_x + 3.2, z_seam - z_block0, 1.0)
        extrude(amount=(e["iy1"] + 0.40) - guide_bot)
        with BuildSketch(_xz_plane(e["iy1"] + 0.01)):
            with Locations((p.button_x, 0.5 * (p.button_z - pocket_z / 2.0 + z_seam + 1.0))):
                RectangleRounded(pocket_x, z_seam + 1.0 - (p.button_z - pocket_z / 2.0), 0.8)
        extrude(amount=e["iy1"] - guide_bot + 0.2, mode=Mode.SUBTRACT)

        # 7. Dovetail lens-cap track, open at the top, with detent notches for the open and closed
        # positions; notches have 40 deg lead-ins and sit outside the Lepton opening
        track_h = e["track_y_max"] - e["track_y_min"]
        track_cy = 0.5 * (e["track_y_min"] + e["track_y_max"])
        with BuildSketch(Plane.XY.offset(z0 - 0.05)):
            with Locations((p.lepton_x, track_cy)):
                Rectangle(e["track_w_outer"] - 0.10, track_h)
        with BuildSketch(Plane.XY.offset(e["z_track_floor"])):
            with Locations((p.lepton_x, track_cy)):
                Rectangle(e["track_w_inner"], track_h)
        loft(mode=Mode.SUBTRACT)

        for cap_y in (e["cap_open_y"], e["cap_closed_y"]):
            for sx in (-1.0, 1.0):
                nx, ny = p.lepton_x + sx * CAP_FINGER_X, cap_y + CAP_BUMP_DY
                with BuildSketch(Plane.XY.offset(e["z_track_floor"] - 0.05)):
                    with Locations((nx, ny)):
                        RectangleRounded(2.00, 2.60, 0.40)
                with BuildSketch(Plane.XY.offset(e["z_track_floor"] + 0.50)):
                    with Locations((nx, ny)):
                        RectangleRounded(2.00, 1.60, 0.40)
                loft(mode=Mode.SUBTRACT)

        # 8. Lepton opening: Ø18.8 at the inner face (the socketed Lepton reaches 0.74 mm into the
        # wall), widening to Ø20.2 at the track floor; clears the 71 and 119 deg diagonal FOV
        r_in = p.shell_lepton_inner_d / 2.0
        r_out = p.shell_lepton_outer_d / 2.0
        cone_h = z_in - e["z_track_floor"] + 0.20
        with Locations((p.lepton_x, p.lepton_y, e["z_track_floor"] - 0.10)):
            Cone(r_out, r_in, cone_h,
                 align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        with Locations((p.lepton_x, p.lepton_y, -0.10)):
            Cylinder(r_in, 0.80, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

        # 9. Guide tubes from the back wall toward the D2 charge LED (light tunnel) and the SW3
        # Prog button (pin guide); they stop 0.8 / 1.0 mm short of the parts
        for (gx, gy), top in ((p.back_face_led_holes[0], 18.40), ((p.teensy_prog_x, p.teensy_prog_y), 17.50)):
            with Locations((gx, gy, z_in - 0.10)):
                Cylinder(p.guide_tube_od / 2.0, top - (z_in - 0.10), align=(Align.CENTER, Align.CENTER, Align.MIN))
                Cylinder(p.back_face_led_d / 2.0, top + 0.5, align=(Align.CENTER, Align.CENTER, Align.MIN),
                         mode=Mode.SUBTRACT)

        # 10. SW3 Prog button pinhole (85.30, 4.00)
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
        extrude(amount=e["w"] - 1.0, mode=Mode.SUBTRACT)  # Leaves the 1.0 mm MJF minimum wall

        # 12. Onboard controls on B.Cu Lepton side
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
        tab_t, tab_gap, foot_h, tongue_h = 1.00, 0.20, 2.00, 1.40  # 1.0 mm: MJF minimum wall
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

        for ty in (38.0,):  # single right-hand tab clears the SW1 slide switch
            with BuildSketch(Plane.XY.offset(z_seam)):
                with Locations((e["ix1"] + 0.5 - foot_w / 2.0, ty)):
                    Rectangle(foot_w, 9.0)
            extrude(amount=foot_h)
            with BuildSketch(Plane.XY.offset(z_seam)):
                with Locations((e["ix1"] - tab_gap - tab_t / 2.0, ty)):
                    Rectangle(tab_t, 9.0)
            extrude(amount=-tongue_h)

        # 4. ENC-3D-P1-1: USB-C full-depth boot counterbore relief notch on front_shell bottom rim (Z>=22.6)!
        with BuildSketch(_xz_plane(e["oy0"] - 0.05)):
            with Locations((p.usb.center_x, z_seam + 0.10)):
                RectangleRounded(p.usb.overmold_width, 1.40, 0.6)
        extrude(amount=-(p.usb.overmold_depth + 0.05), mode=Mode.SUBTRACT)

        # 5. Four 5 mm bosses that clamp the display PCB onto the 11 mm standoffs, with flush
        # ISO 7046 countersinks for the M2.5x14 screws
        z_disp_top = p.display_pcb_z_back + p.display_pcb_t
        cs_depth = (p.countersink_head_d - p.mounting_hole_d) / 2.0
        for mx, my in p.mounting_holes:
            with Locations((mx, my, z_disp_top)):
                Cylinder(p.front_boss_d / 2.0, z_in - z_disp_top + 0.5, align=(Align.CENTER, Align.CENTER, Align.MIN))
        for mx, my in p.mounting_holes:
            with Locations((mx, my, z_disp_top - 1.0)):
                Cylinder(p.mounting_hole_d / 2.0, z_out - z_disp_top + 2.0,
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

        # 7. Matching subtle ergonomic grip grooves on Left (-X) flank (#186)
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
        capture_button=build_capture_button_plunger(p, e),
        slide_switch_cap=build_slide_switch_cap(p, e),
        battery_cradle=build_battery_cradle(p),
        battery_floor=build_battery_floor(p),
        battery_lid=build_battery_lid(p),
    )


def orient_for_printing(enc: PrintableEnclosure) -> dict[str, Solid]:
    """Return print-ready solids oriented flat on Z=0 (100% support-free, incl. floor-down cradle!)."""
    def _ground(s: Solid) -> Solid:
        bb = s.bounding_box()
        return _as_solid(Location((-bb.min.X, -bb.min.Y, -bb.min.Z)) * s)

    return {
        "front_shell": _ground(Rotation(180, 0, 0) * enc.front_shell),
        "back_shell": _ground(enc.back_shell),
        # Outer face (Z=z_out) flat on build plate -> 45 deg outward dovetail flare & upward detents = support-free!
        "lens_cap": _ground(enc.lens_cap_closed),
        "capture_button": _ground(Rotation(-90, 0, 0) * enc.capture_button),
        "slide_switch_cap": _ground(enc.slide_switch_cap),
        # Fable #5 / Astra P1-7 fix: FLOOR-DOWN upright on Z=0 -> ZERO bridges, ZERO supports!
        "battery_cradle": _ground(enc.battery_cradle),
    }


def export_printable_enclosure(enc: PrintableEnclosure, out_dir: Path) -> list[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    exported: list[Path] = []
    prefix = enc.params.file_prefix
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

    asm_children = [
        enc.front_shell, enc.back_shell, enc.lens_cap_open,
        enc.capture_button, enc.slide_switch_cap, enc.battery_cradle, enc.battery_floor, enc.battery_lid,
    ]
    asm_step = out_dir / f"{prefix}-assembly.step"
    export_step(Compound(children=asm_children, label=f"{prefix}-assembly"), str(asm_step))
    exported.append(asm_step)

    # One build plate with every printed part at its per-camera quantity
    plate_mesher = Mesher()
    cursor_x, cursor_y, row_h = 0.0, 0.0, 0.0
    plate_items = [(name, solid) for name, solid in print_parts.items() for _ in range(PRINT_PARTS[name])]
    for idx, (_, solid) in enumerate(plate_items):
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
