"""Printed accessory parts for the DIY-Thermocam V4.0 enclosures.

Shell parts: lens cap, capture button, slide-switch cap.
Shared parts (shell and laser case): printed battery cradle; FR4 battery floor and lid (PCB maker).
Laser-case parts: tripod mount, microSD chute, display spacer bars.
"""

from __future__ import annotations

import math

from build123d import (
    Align,
    Box,
    BuildPart,
    BuildSketch,
    Circle,
    Cylinder,
    Locations,
    Mode,
    Plane,
    Polygon,
    Rectangle,
    RectangleRounded,
    RegularPolygon,
    Solid,
    add,
    extrude,
    loft,
)

from .params import EnclosureParams

PCB_TOP_EDGE_Y = 68.40   # Top board edge: the capture buttons stop on it
SW2_TIP_Y = 68.95        # B3U-3000P plunger tip at rest


def _as_solid(shape: object) -> Solid:
    if isinstance(shape, Solid):
        return shape
    solids = shape.solids()  # type: ignore[union-attr]
    assert len(solids) == 1, f"Expected 1 solid, got {len(solids)}"
    return solids[0]


def _clip_to_pcb_outline(p: EnclosureParams) -> None:
    """Inside a BuildPart: trim the part to the PCB outline so it fits both the shell and the laser case."""
    with Locations((p.pcb_w / 2.0, p.pcb_h / 2.0, p.pcb_z_back + p.internal_depth / 2.0)):
        Box(p.pcb_w, p.pcb_h, p.internal_depth * 2.0, mode=Mode.INTERSECT)


def _xz_plane(y: float) -> Plane:
    """Right-handed XZ plane at Y=y (local_X=+X, local_Y=+Z, local_Z=-Y)."""
    return Plane(origin=(0.0, y, 0.0), x_dir=(1, 0, 0), z_dir=(0, -1, 0))


def _yz_plane(x: float) -> Plane:
    """Right-handed YZ plane at X=x (local_X=+Y, local_Y=+Z, local_Z=+X)."""
    return Plane(origin=(x, 0.0, 0.0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))


# --- Lens cap -------------------------------------------------------------------------------------

LENS_CAP_LEN = 22.00
CAP_FINGER_X = 8.40      # Detent fingers at lepton_x +- 8.4
CAP_BUMP_DY = -9.50      # Bumps sit 9.5 mm below the cap centre: always outside the Lepton opening


def build_lens_cap(p: EnclosureParams, e: dict[str, float], y_center: float) -> Solid:
    """Dovetail lens cap with two spring fingers whose bumps click into floor notches (open / closed)."""
    clr = p.lens_cap_clearance
    z_out = e["z_back_outer"]
    z_floor = e["z_track_floor"]
    w_outer = e["track_w_outer"] - 2.0 * clr
    w_inner = e["track_w_inner"] - 2.0 * clr - 0.30
    z_inner = z_floor - 0.15
    y_bot = y_center - LENS_CAP_LEN / 2.0

    with BuildPart() as bp:
        with BuildSketch(Plane.XY.offset(z_out)):
            with Locations((p.lepton_x, y_center)):
                RectangleRounded(w_outer, LENS_CAP_LEN, 1.2)
        with BuildSketch(Plane.XY.offset(z_inner)):
            with Locations((p.lepton_x, y_center)):
                RectangleRounded(w_inner, LENS_CAP_LEN, 1.2)
        loft()

        # Spring fingers open at the lower cap edge, anchored 7.6 mm up
        finger_top = y_center - 3.40
        for sx in (-1.0, 1.0):
            fx = p.lepton_x + sx * CAP_FINGER_X
            with BuildSketch(Plane.XY.offset(z_out - 0.1)):
                for dx in (-1.15, 1.15):
                    with Locations((fx + dx, 0.5 * (y_bot - 0.2 + finger_top))):
                        Rectangle(0.60, finger_top - (y_bot - 0.2))
            extrude(amount=1.60, mode=Mode.SUBTRACT)
            # Bump with 45 deg lead-ins front and back (rides over the floor and out of the notches)
            with BuildSketch(Plane.XY.offset(z_inner)):
                with Locations((fx, y_center + CAP_BUMP_DY)):
                    RectangleRounded(1.40, 1.20, 0.30)
            with BuildSketch(Plane.XY.offset(z_inner + 0.42)):
                with Locations((fx, y_center + CAP_BUMP_DY)):
                    RectangleRounded(1.40, 0.40, 0.15)
            loft()

        # Thumb-grip flutes at the upper end, away from the fingers; 1.0 mm ridges between them
        grip_cy = y_center + LENS_CAP_LEN / 2.0 - 4.1
        for dy in (-1.8, 0.0, 1.8):
            with BuildSketch(Plane.XY.offset(z_out - 0.05)):
                with Locations((p.lepton_x, grip_cy + dy)):
                    RectangleRounded(w_outer - 5.2, 0.80, 0.25)
            extrude(amount=0.40, mode=Mode.SUBTRACT)  # 0.35 deep: 1.0 mm of the 1.35 mm cap stays

    return _as_solid(bp.part)


# --- Capture button (shell) -----------------------------------------------------------------------

# Flange: 8.2 mm wide (0.7 mm past each side of the 6.8 mm wall slot), 6.4 mm tall; its lower face
# stops on the PCB edge. A straight plate, 1.15 mm thick.
BUTTON_FLANGE_X, BUTTON_FLANGE_Z = 8.20, 6.40
BUTTON_STEM_W, BUTTON_STEM_H = 1.60, 1.20


def build_capture_button_plunger(p: EnclosureParams, e: dict[str, float]) -> Solid:
    """Capture button for the shell: Ø6.2 shaft, square flange, short stem onto the SW2 plunger.

    Rest: stem tip 0.15 mm above the SW2 tip. Pressing moves it 0.75 mm until the flange face lands
    on the PCB edge (Y=68.40), which pushes SW2 0.6 mm: past its 0.1..0.4 mm pretravel in every
    tolerance case.
    """
    y_flange_top = e["iy1"] - 0.10
    y_flange_bot = PCB_TOP_EDGE_Y + p.button_rest_gap + p.button_stop_stroke
    y_stem_tip = SW2_TIP_Y + p.button_rest_gap
    y_cap_top = e["oy1"] + 2.20
    cap_r = p.button_plunger_cap_d / 2.0
    chamf = 0.55

    with BuildPart() as bp:
        # Flange: a straight 1.15 mm plate (a flared loft left knife edges at its corners)
        with BuildSketch(_xz_plane(y_flange_bot)):
            with Locations((p.button_x, p.button_z)):
                RectangleRounded(BUTTON_FLANGE_X, BUTTON_FLANGE_Z, 1.2)
        extrude(amount=-(y_flange_top - y_flange_bot))

        with BuildSketch(_xz_plane(y_flange_bot)):
            with Locations((p.button_x, p.button_stem_z)):
                RectangleRounded(BUTTON_STEM_W, BUTTON_STEM_H, 0.30)
        extrude(amount=y_flange_bot - y_stem_tip)

        shaft_h = (y_cap_top - chamf) - y_flange_top
        with BuildSketch(_xz_plane(y_flange_top)):
            with Locations((p.button_x, p.button_z)):
                Circle(cap_r)
        extrude(amount=-shaft_h)

        with BuildSketch(_xz_plane(y_cap_top - chamf)):
            with Locations((p.button_x, p.button_z)):
                Circle(cap_r)
        with BuildSketch(_xz_plane(y_cap_top)):
            with Locations((p.button_x, p.button_z)):
                Circle(cap_r - chamf)
        loft()

    return _as_solid(bp.part)


# --- Slide-switch cap (shell) ---------------------------------------------------------------------

SLIDE_FORK_TIP_X = 89.50   # 0.5 mm clear of the SW1 body (X<=89.0); grips 1.5 mm of the actuator
SLIDE_POCKET_W = 1.90      # 1.5 mm actuator + 0.2 mm per side


def build_slide_switch_cap(p: EnclosureParams, e: dict[str, float]) -> Solid:
    """Slide-switch cap: inner shoulder, fork around the SW1 actuator, neck and thumb pad.

    The fork pocket is open at the top, so the cap sits in the shell first and the actuator drops
    into it as the board goes in. 0.5 mm clear of the PCB edge, the wall and the SW1 body.
    """
    sy = p.switch_y
    z_bot = 16.70
    x_shoulder_in = p.pcb_w + 0.50
    x_shoulder_out = e["ix1"] - 0.50
    x_thumb_tip = e["ox1"] + 0.15

    with BuildPart() as bp:
        with BuildSketch(_yz_plane(x_shoulder_in)):
            with Locations((sy, z_bot + 2.20)):
                RectangleRounded(12.20, 4.40, 0.70)
        extrude(amount=x_shoulder_out - x_shoulder_in)

        with BuildSketch(_yz_plane(SLIDE_FORK_TIP_X)):
            with Locations((sy, z_bot + 1.50)):
                RectangleRounded(7.60, 3.00, 0.50)
        extrude(amount=(x_shoulder_in + 0.10) - SLIDE_FORK_TIP_X)

        with BuildSketch(_yz_plane(x_shoulder_out)):
            with Locations((sy, z_bot + 1.70)):
                RectangleRounded(6.00, 3.40, 0.65)
        extrude(amount=(x_thumb_tip - 0.20) - x_shoulder_out)

        for dy in (-2.0, 0.0, 2.0):  # Grip ribs, 1.0 mm wide (MJF minimum)
            with BuildSketch(_yz_plane(x_thumb_tip - 0.20)):
                with Locations((sy + dy, z_bot + 1.60)):
                    RectangleRounded(1.00, 3.20, 0.30)
            extrude(amount=0.35)

        # Fork pocket, open at the top and bottom, ending inside the neck
        with BuildSketch(_yz_plane(SLIDE_FORK_TIP_X - 0.10)):
            with Locations((sy, z_bot + 2.50)):
                Rectangle(SLIDE_POCKET_W, 8.0)
        extrude(amount=(x_shoulder_out + 0.10) - (SLIDE_FORK_TIP_X - 0.10), mode=Mode.SUBTRACT)

    return _as_solid(bp.part)


# --- Battery cradle, C-spacers and lid (shell and laser case) ------------------------------------

def _cradle_footprint(p: EnclosureParams) -> tuple[float, float, float, float]:
    bat = p.battery
    in_w, in_h = bat.width_x + 1.0, bat.height_y + 1.0
    return bat.center_x, bat.center_y, in_w + 2.4, in_h + 2.4


def _cradle_plate_with_ears(p: EnclosureParams, z0: float, t: float) -> Solid:
    """Flat plate over the battery with an ear to each mounting hole (Ø6.2 bores for the standoffs)."""
    cx, cy, out_w, out_h = _cradle_footprint(p)
    with BuildPart() as bp:
        with BuildSketch(Plane.XY.offset(z0)):
            with Locations((cx, cy)):
                RectangleRounded(out_w, out_h, 2.20)
            for mx, my in p.mounting_holes:
                arm_x = 0.5 * (mx + max(cx - out_w / 2.0 + 3.0, min(cx + out_w / 2.0 - 3.0, mx)))
                arm_y = 0.5 * (my + max(cy - out_h / 2.0 + 3.0, min(cy + out_h / 2.0 - 3.0, my)))
                with Locations((mx, my)):
                    Circle(4.40)
                with Locations((arm_x, arm_y)):
                    Rectangle(abs(mx - arm_x) * 2.0 + 5.0, abs(my - arm_y) * 2.0 + 5.0)
        extrude(amount=t)
        for mx, my in p.mounting_holes:
            with BuildSketch(Plane.XY.offset(z0 - 0.5)):
                with Locations((mx, my)):
                    Circle(3.10)
            extrude(amount=t + 1.0, mode=Mode.SUBTRACT)
    return _as_solid(bp.part)


CRADLE_BORE_D = 6.20          # Clears the M2.5x11 hex standoff corners (SW 5.0 -> 5.77 mm)
CRADLE_GAP_DEG = 90.0
CRADLE_RIB_X = 66.50          # Floor rib between the J5 and J6 tail rows
TAIL_CLEAR = 0.35             # Around each pin tail (Ø1.7 fillet) and Lepton screw washer
THT_TAIL_R_CLEAR = 0.85 + TAIL_CLEAR


def battery_stack_z(p: EnclosureParams) -> dict[str, float]:
    """Z levels of the battery stack, PCB front (21.6) to the lid top (29.9)."""
    bat = p.battery
    floor_bot = bat.z_start - p.battery_floor_t
    lid_bot = bat.z_start + bat.thickness_z + p.battery_lid_gap
    return {
        "pcb_top": p.pcb_z_back + p.pcb_t,
        "lip_bot": floor_bot - p.cradle_lip_t,
        "floor_bot": floor_bot,
        "floor_top": bat.z_start,
        "wall_top": bat.z_start + bat.thickness_z,
        "lid_bot": lid_bot,
        "lid_top": lid_bot + p.battery_lid_t,
    }


def _wedge(x: float, y: float, gap_deg: float, reach: float) -> list[tuple[float, float]]:
    half, g = math.radians(CRADLE_GAP_DEG / 2.0), math.radians(gap_deg)
    r = reach / math.cos(half)
    return [(x, y), (x + r * math.cos(g - half), y + r * math.sin(g - half)),
            (x + r * math.cos(g + half), y + r * math.sin(g + half))]


def _arm_rect(p: EnclosureParams, mx: float, my: float) -> tuple[float, float, float, float]:
    """Centre and size of the arm from a mounting hole to the nearest cradle corner."""
    cx, cy, out_w, out_h = _cradle_footprint(p)
    arm_x = 0.5 * (mx + max(cx - out_w / 2.0 + 3.0, min(cx + out_w / 2.0 - 3.0, mx)))
    arm_y = 0.5 * (my + max(cy - out_h / 2.0 + 3.0, min(cy + out_h / 2.0 - 3.0, my)))
    return arm_x, arm_y, abs(mx - arm_x) * 2.0 + 5.0, abs(my - arm_y) * 2.0 + 5.0


def build_battery_cradle(p: EnclosureParams) -> Solid:
    """Printed LiPo cradle: 1.2 mm walls, 1.0 mm lips and a rib that carry the FR4 floor, and four
    feet around the M2.5x11 standoffs that stand it on the PCB.

    Below the floor the lips stop short of every pin tail and Lepton screw head; along the J6 tail
    row the right wall starts at the floor, so the floor's right edge rests on the rib.
    """
    z = battery_stack_z(p)
    cx, cy, out_w, out_h = _cradle_footprint(p)
    in_w, in_h = out_w - 2.0 * p.cradle_wall_t, out_h - 2.0 * p.cradle_wall_t
    lip_h = z["floor_bot"] - z["lip_bot"]

    with BuildPart() as bp:
        # Walls from the lip bottom to the cell's worst-case top, lips around the inside
        with BuildSketch(Plane.XY.offset(z["lip_bot"])):
            with Locations((cx, cy)):
                RectangleRounded(out_w, out_h, 2.20)
                RectangleRounded(in_w, in_h, 0.60, mode=Mode.SUBTRACT)
        extrude(amount=z["wall_top"] - z["lip_bot"])
        with BuildSketch(Plane.XY.offset(z["lip_bot"])):
            with Locations((cx, cy)):
                RectangleRounded(in_w + 0.2, in_h + 0.2, 0.60)
                Rectangle(in_w - 2.0 * p.cradle_lip_w, in_h - 2.0 * p.cradle_lip_w, mode=Mode.SUBTRACT)
            with Locations((CRADLE_RIB_X, cy)):
                Rectangle(p.cradle_wall_t, in_h)
        extrude(amount=lip_h)
        # Below the floor: no wall or lip along the J6 row (the rib carries the floor's right side),
        # and a straight cut across the band at each J5 tail and Lepton screw washer, so no sliver
        # is left beside a round notch
        x_cut = cx + in_w / 2.0 - p.cradle_lip_w - 0.2
        with Locations((x_cut + 10.0, cy, z["lip_bot"] - 0.5)):
            Box(20.0, out_h + 10.0, lip_h + 0.5, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        for x, half in ((59.28, THT_TAIL_R_CLEAR), *[(hx, 2.5 + TAIL_CLEAR) for hx, hy in p.lepton_breakout_holes if hy > 50.0]):
            for band_y in (cy - out_h / 2.0, cy + out_h / 2.0):
                with Locations((x, band_y, z["lip_bot"] - 0.5)):
                    Box(2.0 * half, 2.0 * (p.cradle_wall_t + p.cradle_lip_w) + 2.0, lip_h + 0.5,
                        align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        # JST-PH wire exit in the right wall over a 1.0 mm sill (the wall has no lip below it here);
        # the wires run round the right board edge to J4
        sill_top = z["floor_bot"] + p.cradle_lip_t
        with BuildSketch(Plane.XY.offset(sill_top)):
            with Locations((cx + out_w / 2.0, 50.0)):
                Rectangle(3.6, 14.0)
        extrude(amount=z["wall_top"] - sill_top + 0.5, mode=Mode.SUBTRACT)

        # Feet around the standoffs, joined to the walls by arms above the pin tails
        for (mx, my), edge_gap, tail_gap in zip(p.mounting_holes, p.cradle_foot_edge_gap_deg,
                                                p.cradle_foot_tail_gap_deg, strict=True):
            with BuildPart() as foot:
                with BuildSketch(Plane.XY.offset(z["pcb_top"])):
                    with Locations((mx, my)):
                        Circle(p.cradle_foot_od / 2.0)
                extrude(amount=p.cradle_foot_top_z - z["pcb_top"])
                ax, ay, aw, ah = _arm_rect(p, mx, my)
                with BuildSketch(Plane.XY.offset(z["floor_bot"])):
                    with Locations((ax, ay)):
                        Rectangle(aw, ah)
                extrude(amount=p.cradle_foot_top_z - z["floor_bot"])
                with BuildSketch(Plane.XY.offset(z["pcb_top"] - 0.1)):
                    Polygon(*_wedge(mx, my, edge_gap, p.cradle_foot_od), align=None)
                extrude(amount=p.cradle_foot_top_z - z["pcb_top"] + 0.2, mode=Mode.SUBTRACT)
                if tail_gap is not None:
                    with BuildSketch(Plane.XY.offset(z["pcb_top"] - 0.1)):
                        Polygon(*_wedge(mx, my, tail_gap, p.cradle_foot_od), align=None)
                    extrude(amount=z["floor_bot"] - z["pcb_top"] + 0.1, mode=Mode.SUBTRACT)
                # Arms stop at the inside of the walls, clear of the floor and the cell
                with Locations((cx, cy, z["lip_bot"])):
                    Box(in_w, in_h, p.cradle_foot_top_z - z["lip_bot"] + 0.1,
                        align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
            add(foot.part)
        for mx, my in p.mounting_holes:
            with Locations((mx, my, z["pcb_top"] - 0.5)):
                Cylinder(CRADLE_BORE_D / 2.0, p.cradle_foot_top_z - z["pcb_top"] + 1.0,
                         align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        _clip_to_pcb_outline(p)

    return _as_solid(bp.part)


def build_battery_floor(p: EnclosureParams) -> Solid:
    """0.6 mm FR4 floor, dropped into the cradle onto its lips and rib."""
    z = battery_stack_z(p)
    cx, cy, out_w, out_h = _cradle_footprint(p)
    w = out_w - 2.0 * (p.cradle_wall_t + p.fr4_clearance)
    h = out_h - 2.0 * (p.cradle_wall_t + p.fr4_clearance)
    with BuildPart() as bp:
        with BuildSketch(Plane.XY.offset(z["floor_bot"])):
            with Locations((cx, cy)):
                RectangleRounded(w, h, 0.80)
        extrude(amount=p.battery_floor_t)
    return _as_solid(bp.part)


def build_battery_lid(p: EnclosureParams) -> Solid:
    """0.4 mm FR4 lid over the LiPo; top at Z=29.9, under the display's 2.5 mm back-side parts.

    Each ear hugs the inner half of its standoff and ends square at the standoff's centre line, so
    the board has no thin web toward the edge; the four half-rings hold it in X and Y."""
    z = battery_stack_z(p)
    with BuildPart() as bp:
        add(_cradle_plate_with_ears(p, z["lid_bot"], p.battery_lid_t))
        for my in sorted({my for _, my in p.mounting_holes}):
            edge_y = 0.0 if my < p.pcb_h / 2.0 else p.pcb_h
            with Locations((p.pcb_w / 2.0, 0.5 * (my + edge_y), z["lid_bot"] - 0.5)):
                Box(p.pcb_w + 10.0, abs(edge_y - my) + 2.0, p.battery_lid_t + 1.0,
                    align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        _clip_to_pcb_outline(p)
    return _as_solid(bp.part)


# --- Laser-case parts ----------------------------------------------------------------------------

TRIPOD_NUT_AF, TRIPOD_NUT_H = 11.11, 5.50   # 1/4"-20 UNC hex nut, 7/16" AF
TRIPOD_PLUG_X, TRIPOD_PLUG_Z = 18.00, 13.40  # Fills the bottom-plate hole (18.2 x 13.6)
TRIPOD_FLOOR_T = 1.20


def laser_bottom_faces(p: EnclosureParams) -> tuple[float, float]:
    """Inner and outer Y of the laser bottom plate."""
    y_in = -p.laser_inner_margin_y_neg
    return y_in, y_in - p.laser_t


def build_laser_tripod_mount(p: EnclosureParams) -> Solid:
    """Printed 1/4"-20 tripod nut holder for the laser case.

    Its plug fills the bottom-plate hole flush with the outside, so a 4.5 mm tripod screw gets
    3.3 mm of thread. The nut presses in between two crush ribs. A 1 mm fin under the main board's
    bottom edge stops the holder from lifting; the plug stops it from turning. Prints on its -X
    face without supports (the nut pocket's corners point along X, so its roof is a 60 deg arch).
    """
    tx, tz = p.laser_tripod_x, p.laser_tripod_z
    y_in, y_out = laser_bottom_faces(p)
    y_plug_bot = y_out + 0.05
    y_nut_bot = y_out + TRIPOD_FLOOR_T
    y_top = y_nut_bot + TRIPOD_NUT_H + 0.45
    body_x, body_z = 18.0, 16.0
    hex_r = TRIPOD_NUT_AF / math.sqrt(3.0) + 0.06

    with BuildPart() as bp:
        with BuildSketch(_xz_plane(y_plug_bot)):
            with Locations((tx, tz)):
                RectangleRounded(TRIPOD_PLUG_X, TRIPOD_PLUG_Z, 1.5)
        extrude(amount=-(y_in - y_plug_bot))
        with BuildSketch(_xz_plane(y_in)):
            with Locations((tx, tz)):
                RectangleRounded(body_x, body_z, 2.0)
        extrude(amount=-(y_top - y_in))
        # Fin under the main board's bottom edge (Y=0), up past the board (Z=20..21.6)
        fin_z0 = tz + body_z / 2.0 - 1.0
        with BuildSketch(_xz_plane(y_in)):
            with Locations((tx, 0.5 * (fin_z0 + p.pcb_z_back + p.pcb_t))):
                RectangleRounded(body_x, p.pcb_z_back + p.pcb_t - fin_z0, 0.8)
        extrude(amount=-(-0.05 - y_in))
        # Nut pocket (corners along X), screw hole through the floor
        with BuildSketch(_xz_plane(y_nut_bot)):
            with Locations((tx, tz)):
                RegularPolygon(radius=hex_r, side_count=6, rotation=0.0)
        extrude(amount=-(y_top - y_nut_bot + 0.1), mode=Mode.SUBTRACT)
        with BuildSketch(_xz_plane(y_plug_bot - 0.1)):
            with Locations((tx, tz)):
                Circle(p.shell_tripod_screw_d / 2.0)
        extrude(amount=-(TRIPOD_FLOOR_T + 0.3), mode=Mode.SUBTRACT)
        # Two 1.0 mm crush ribs on the pocket's Z flats, 0.3 mm into the pocket: the pressed-in nut
        # squeezes them and stays put. They stop 0.8 mm below the top so the nut starts by hand.
        pocket_flat = hex_r * math.sqrt(3.0) / 2.0
        rib_in = pocket_flat - 0.30
        for sz in (-1.0, 1.0):
            with BuildSketch(_xz_plane(y_nut_bot)):
                with Locations((tx, tz + sz * (rib_in + 0.5))):
                    Rectangle(1.0, 1.0)
            extrude(amount=-(y_top - 0.8 - y_nut_bot))

    return _as_solid(bp.part)


SD_CHUTE_LEN_IN = 0.80   # Chute ends 0.8 mm before the Teensy's end (Y=61.11)


def build_laser_sd_chute(p: EnclosureParams) -> Solid:
    """Printed microSD chute: pushed into the top plate from outside; two side barbs click behind it."""
    y_plate_in = p.pcb_h + p.laser_inner_margin_y_pos
    y_plate_out = y_plate_in + p.laser_t
    y_end = 61.11 + SD_CHUTE_LEN_IN
    neck_w, neck_h = p.microsd_w + 2.4, p.microsd_h + 2.4
    x, z = p.microsd_x, p.microsd_z

    with BuildPart() as bp:
        with BuildSketch(_xz_plane(y_end)):
            with Locations((x, z)):
                RectangleRounded(neck_w, neck_h, 0.8)
        extrude(amount=-(y_plate_out - y_end))
        with BuildSketch(_xz_plane(y_plate_out)):  # Outer bezel
            with Locations((x, z)):
                RectangleRounded(neck_w + 3.0, neck_h + 2.0, 1.2)
        extrude(amount=-0.8)
        for sx in (-1.0, 1.0):  # Barbs: ramp 0 -> 0.35 mm, back face 0.1 mm inside the plate
            bx = x + sx * neck_w / 2.0
            with BuildSketch(Plane.XY.offset(z - 1.5)):
                Polygon((bx, y_plate_in - 1.4), (bx + sx * 0.35, y_plate_in - 0.1), (bx, y_plate_in - 0.1),
                        align=None)
            extrude(amount=3.0)
        with BuildSketch(_xz_plane(y_end - 0.1)):
            with Locations((x, z)):
                RectangleRounded(p.microsd_w, p.microsd_h, 0.6)
        extrude(amount=-(y_plate_out - y_end + 1.0), mode=Mode.SUBTRACT)
        with BuildSketch(_xz_plane(y_plate_out + 0.81)):  # Lead-in at the outer opening
            with Locations((x, z)):
                RectangleRounded(p.microsd_w + 1.6, p.microsd_h + 1.2, 0.8)
        extrude(amount=0.5, mode=Mode.SUBTRACT)

    return _as_solid(bp.part)


def build_laser_display_spacer(p: EnclosureParams, top: bool) -> Solid:
    """Printed 5 mm spacer bar between display PCB and front plate, one bar per hole pair."""
    y = p.mounting_holes[2][1] if top else p.mounting_holes[0][1]
    x0, x1 = p.mounting_holes[0][0], p.mounting_holes[1][0]
    z0 = p.display_pcb_z_back + p.display_pcb_t
    h = p.internal_depth - z0
    r = p.front_boss_d / 2.0
    with BuildPart() as bp:
        with BuildSketch(Plane.XY.offset(z0)):
            with Locations((0.5 * (x0 + x1), y)):
                Rectangle(x1 - x0, 3.2)
            with Locations((x0, y), (x1, y)):
                Circle(r)
            with Locations((x0, y), (x1, y)):
                Circle(p.mounting_hole_d / 2.0, mode=Mode.SUBTRACT)
        extrude(amount=h)
        # Lighten the bar between the bosses: 2 mm tall rib only
        with Locations((0.5 * (x0 + x1), y, z0 + 2.0)):
            Box(x1 - x0 - 2.0 * r - 2.0, 3.4, h, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
    return _as_solid(bp.part)


__all__ = [
    "battery_stack_z",
    "build_battery_cradle",
    "build_battery_floor",
    "build_battery_lid",
    "build_capture_button_plunger",
    "build_laser_display_spacer",
    "build_laser_sd_chute",
    "build_laser_tripod_mount",
    "build_lens_cap",
    "build_slide_switch_cap",
    "laser_bottom_faces",
]
