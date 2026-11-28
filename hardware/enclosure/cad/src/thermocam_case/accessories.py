"""Printed accessory parts for the DIY-Thermocam V4.0 enclosures.

Shell parts: lens cap, capture button, slide-switch cap.
Shared parts (shell and laser case): printed battery cradle and cell spacer bars; FR4 battery floor
and lid (PCB maker).
Laser-case parts: tripod mount, microSD chute, display spacer bars.
"""

from __future__ import annotations

import math

from build123d import (
    Align,
    Axis,
    Box,
    BuildPart,
    BuildSketch,
    Circle,
    Cylinder,
    Location,
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
    fillet,
    loft,
)

from .keepout import J3_LEG_DX
from .params import MJF_MIN_DETAIL, MJF_MIN_WALL, EnclosureParams

PCB_TOP_EDGE_Y = 68.40   # Top board edge: the capture pushers pass under it
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
CAP_SLOT_X = 1.25        # Finger slots (0.8 mm) centred 1.25 mm either side: 1.7 mm fingers
CAP_BUMP_W, CAP_BUMP_H, CAP_BUMP_TOP = 1.60, MJF_MIN_DETAIL, MJF_MIN_DETAIL  # 45 deg ramps both ways
CAP_NOTCH_DEPTH = MJF_MIN_DETAIL   # Track-floor notches the bumps click into
CAP_GRIP_H = MJF_MIN_DETAIL


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

        # Spring fingers open at the lower cap edge, anchored 7.6 mm up; 0.8 mm slots beside them
        finger_top = y_center - 3.40
        for sx in (-1.0, 1.0):
            fx = p.lepton_x + sx * CAP_FINGER_X
            with BuildSketch(Plane.XY.offset(z_out - 0.1)):
                for dx in (-CAP_SLOT_X, CAP_SLOT_X):
                    with Locations((fx + dx, 0.5 * (y_bot - 0.2 + finger_top))):
                        Rectangle(MJF_MIN_DETAIL, finger_top - (y_bot - 0.2))
            extrude(amount=1.60, mode=Mode.SUBTRACT)
            # 0.8 mm bump with 45 deg lead-ins front and back (rides over the floor, clicks into the notches)
            with BuildSketch(Plane.XY.offset(z_inner)):
                with Locations((fx, y_center + CAP_BUMP_DY)):
                    Rectangle(CAP_BUMP_W, CAP_BUMP_TOP + 2.0 * CAP_BUMP_H)
            with BuildSketch(Plane.XY.offset(z_inner + CAP_BUMP_H)):
                with Locations((fx, y_center + CAP_BUMP_DY)):
                    Rectangle(CAP_BUMP_W, CAP_BUMP_TOP)
            loft()

        # Two thumb-grip ribs at the upper end, away from the fingers: 1.0 mm wide, 0.8 mm proud of
        # the back face (grooves cut into the 1.35 mm cap would leave it thinner than 1.0 mm)
        grip_cy = y_center + LENS_CAP_LEN / 2.0 - 4.1
        for dy in (-1.0, 1.0):
            with BuildSketch(Plane.XY.offset(z_out)):
                with Locations((p.lepton_x, grip_cy + dy)):
                    RectangleRounded(w_outer - 5.2, 1.0, 0.3)
            extrude(amount=-CAP_GRIP_H)

    return _as_solid(bp.part)


# --- Capture button (shell) -----------------------------------------------------------------------

# Flange: 8.2 mm wide (0.7 mm past each side of the 6.8 mm wall slot), from 3.2 mm below the button
# axis up to SW2_PUSHER_PCB_GAP under the PCB's back face. A straight plate, 1.2 mm thick, its back
# BUTTON_FLANGE_INTO_WALL into a recess in the top wall (the 0.45 mm rest gap leaves no room inside).
# 10 mm wide, so the front shell's stop fingers (1.55 mm, MJF_MIN_WALL+) fit over its corners beside the Ø6.2 shaft
BUTTON_FLANGE_X, BUTTON_FLANGE_Z = 10.00, 6.40
BUTTON_FLANGE_T, BUTTON_FLANGE_INTO_WALL = 1.20, 0.20
# The pushers stop this far under the PCB's back face, so neither can land on the board edge: the
# switch alone ends the travel (B3U-3000P: 30 N max operating force), and SW2 always fires whatever
# its plunger, placement and board-edge tolerances (they add up to about +-0.3 mm). The front shell's
# stop over the flange lets the button rise 0.3 mm at most (BUTTON_LIFT).
SW2_PUSHER_PCB_GAP = 0.50
BUTTON_LIFT = 0.30


def build_capture_button_plunger(p: EnclosureParams, e: dict[str, float]) -> Solid:
    """Capture button for the shell: Ø6.2 shaft and a square flange whose flat face pushes SW2.

    Rest: flange face button_rest_gap above the SW2 tip. The flange ends SW2_PUSHER_PCB_GAP under
    the PCB, so pressed it passes under the board edge and pushes the plunger until SW2 itself stops it.
    """
    y_flange_bot = SW2_TIP_Y + p.button_rest_gap
    y_flange_top = y_flange_bot + BUTTON_FLANGE_T
    assert abs(y_flange_top - (e["iy1"] + BUTTON_FLANGE_INTO_WALL)) < 1e-6, "flange back must sit in the wall recess"
    z_flange_bot = p.button_z - BUTTON_FLANGE_Z / 2.0
    z_flange_top = p.pcb_z_back - SW2_PUSHER_PCB_GAP
    y_cap_top = e["oy1"] + 2.20
    cap_r = p.button_plunger_cap_d / 2.0
    chamf = 0.55

    with BuildPart() as bp:
        # Flange: a straight 1.2 mm plate (a flared loft left knife edges at its corners)
        with BuildSketch(_xz_plane(y_flange_bot)):
            with Locations((p.button_x, 0.5 * (z_flange_bot + z_flange_top))):
                RectangleRounded(BUTTON_FLANGE_X, z_flange_top - z_flange_bot, 1.2)
        extrude(amount=-(y_flange_top - y_flange_bot))

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
SLIDER_LIFT = 0.60         # Most the cap can rise toward the display (fork keeps ~1 mm on the actuator)


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

        # Neck and thumb pad up to 0.3 mm under the seam: the front shell's rim then stops the cap
        # after SLIDER_LIFT, while the fork still holds the actuator
        pad_x = x_thumb_tip + 0.15 - MJF_MIN_DETAIL  # Thumb pad face, below the ribs' tops
        neck_top = e["z_back_seam"] - (SLIDER_LIFT - (e["z_front_seam"] - e["z_back_seam"]))
        with BuildSketch(_yz_plane(x_shoulder_out)):
            with Locations((sy, 0.5 * (z_bot + neck_top))):
                RectangleRounded(6.00, neck_top - z_bot, 0.65)
        extrude(amount=pad_x - x_shoulder_out)

        for dy in (-2.0, 0.0, 2.0):  # Grip ribs 1.0 mm wide, 0.8 mm tall, 1.0 mm apart (MJF minimum detail)
            with BuildSketch(_yz_plane(pad_x)):
                with Locations((sy + dy, z_bot + 1.60)):
                    RectangleRounded(1.00, 3.20, 0.30)
            extrude(amount=MJF_MIN_DETAIL)

        # Fork pocket, open at the top and bottom, ending inside the neck
        with BuildSketch(_yz_plane(SLIDE_FORK_TIP_X - 0.10)):
            with Locations((sy, z_bot + 2.50)):
                Rectangle(SLIDE_POCKET_W, 8.0)
        extrude(amount=(x_shoulder_out + 0.10) - (SLIDE_FORK_TIP_X - 0.10), mode=Mode.SUBTRACT)

    return _as_solid(bp.part)


# --- Battery cradle, FR4 floor and FR4 lid (shell and laser case) --------------------------------

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
                    Circle(5.00)  # 1.9 mm of FR4 round the Ø6.2 bore: the half-ring's horns don't snap off
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
CRADLE_WIRE_GAP_Y, CRADLE_WIRE_GAP_W = 50.0, 14.0   # Lead exit in the right wall, level with J4
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
        # and a straight cut across the band at each J5 tail, at the Lepton screw washers under the
        # back side and at the USB-C J3 shell legs under the front side, so no sliver is left beside
        # a round notch. Cuts closer than the MJF minimum wall merge into one.
        x_cut = cx + in_w / 2.0 - p.cradle_lip_w - 0.2
        with Locations((x_cut + 10.0, cy, z["lip_bot"] - 0.5)):
            Box(20.0, out_h + 10.0, lip_h + 0.5, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        j5 = [(59.28, THT_TAIL_R_CLEAR)]
        band_in = out_h / 2.0 - p.cradle_wall_t - p.cradle_lip_w   # Lip band's inner edge from the centre
        washers = [(hx, 2.5 + TAIL_CLEAR) for hx, hy in p.lepton_breakout_holes if hy + 2.5 + TAIL_CLEAR > cy + band_in]
        j3 = [(x, 0.5 + TAIL_CLEAR) for x in (p.usb.center_x - J3_LEG_DX, p.usb.center_x + J3_LEG_DX)]
        for band_y, cuts in ((cy - out_h / 2.0, j5 + j3), (cy + out_h / 2.0, j5 + washers)):
            merged: list[list[float]] = []
            for x0, x1 in sorted((x - half, x + half) for x, half in cuts):
                if merged and x0 - merged[-1][1] < MJF_MIN_WALL:
                    merged[-1][1] = max(merged[-1][1], x1)
                else:
                    merged.append([x0, x1])
            for x0, x1 in merged:
                with Locations((0.5 * (x0 + x1), band_y, z["lip_bot"] - 0.5)):
                    Box(x1 - x0, 2.0 * (p.cradle_wall_t + p.cradle_lip_w) + 2.0, lip_h + 0.5,
                        align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        # JST-PH wire exit in the right wall over a 1.0 mm sill (the wall has no lip below it here);
        # the wires run round the right board edge to J4
        sill_top = z["floor_bot"] + p.cradle_lip_t
        with BuildSketch(Plane.XY.offset(sill_top)):
            with Locations((cx + out_w / 2.0, CRADLE_WIRE_GAP_Y)):
                Rectangle(3.6, CRADLE_WIRE_GAP_W)
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


# --- Spacer bars for the 34 mm cell (shell and laser case) ----------------------------------------
# The cradle fits the 50 x 60 cell. Two spring bars, one along each long side, hold the 34 mm
# Adafruit 258 (PKCELL LP503562: 35.5 x 62.5 x 5.2 mm max with its protection board) in the middle,
# so its lead, which leaves the protection-board end at one corner, runs straight out through the
# wire gap. Each bar is a frame: two beams with a slot between them and a bump at the middle of each,
# so it squeezes from its free width down to the width of its end blocks.
SPACER_CELL_W = 34.0                    # Adafruit 258, nominal
SPACER_CELL_W_RANGE = (33.5, 36.0)      # Datasheet max 35.5; 36.0 covers other LP503562 makers
SPACER_LEN, SPACER_H = 62.0, 5.0        # Spans the cradle; FR4 floor to 0.5 mm under the FR4 lid
SPACER_END_L, SPACER_BEAM_T = 5.0, 1.4
SPACER_SOLID_W = 7.3                    # Across the end blocks: both beams pressed flat
SPACER_BUMP_H, SPACER_BUMP_L, SPACER_BUMP_RAMP = 1.15, 10.0, 4.0
SPACER_FREE_W = SPACER_SOLID_W + 2.0 * SPACER_BUMP_H
SPACER_EDGE_R = 1.0


def _spacer_bump(x: float) -> float:
    d = abs(x - SPACER_LEN / 2.0) - SPACER_BUMP_L / 2.0
    return SPACER_BUMP_H * min(1.0, max(0.0, 1.0 - d / SPACER_BUMP_RAMP))


def _spacer_bend(x: float, squeeze: float) -> float:
    """How far a beam has moved in at X, as a fixed-fixed beam under a load at its middle."""
    span = SPACER_LEN - 2.0 * SPACER_END_L
    u = min(max((x - SPACER_END_L) / span, 0.0), 1.0)
    u = min(u, 1.0 - u)
    return squeeze * 4.0 * u * u * (3.0 - 4.0 * u)


def spacer_squeeze(p: EnclosureParams, cell_w: float = SPACER_CELL_W) -> float:
    """How far each beam bends in at its middle with a `cell_w` cell in the cradle."""
    _, _, _, out_h = _cradle_footprint(p)
    gap = (out_h - 2.0 * p.cradle_wall_t - cell_w) / 2.0
    return (SPACER_FREE_W - gap) / 2.0


def build_battery_spacer(squeeze: float = 0.0) -> Solid:
    """One spacer bar as printed (squeeze 0), or with each beam bent in by `squeeze` mm at its
    middle. Local frame: X 0..62, Y centred, Z 0..5."""
    length, end_l = SPACER_LEN, SPACER_END_L
    slot = SPACER_SOLID_W - 2.0 * SPACER_BEAM_T
    mid, half_bump = length / 2.0, SPACER_BUMP_L / 2.0
    xs = [mid - half_bump - SPACER_BUMP_RAMP, mid - half_bump, mid + half_bump, mid + half_bump + SPACER_BUMP_RAMP]
    slot_xs = [end_l, length - end_l]
    if squeeze > 0.0:  # Sample the bent beams
        xs = sorted({*xs, *slot_xs, *(end_l + (length - 2.0 * end_l) * i / 48 for i in range(49))})
        slot_xs = [x for x in xs if end_l <= x <= length - end_l]
    # A bent bump lies flat against the wall or the cell along its top
    face = [(x, min(SPACER_SOLID_W / 2.0 + _spacer_bump(x) - _spacer_bend(x, squeeze), SPACER_FREE_W / 2.0 - squeeze))
            for x in xs]
    inner = [(x, slot / 2.0 - _spacer_bend(x, squeeze)) for x in slot_xs]
    outline = [(0.0, -SPACER_SOLID_W / 2.0), *[(x, -y) for x, y in face], (length, -SPACER_SOLID_W / 2.0),
               (length, SPACER_SOLID_W / 2.0), *reversed(face), (0.0, SPACER_SOLID_W / 2.0)]
    with BuildPart() as bp:
        with BuildSketch():
            Polygon(*outline, align=None)
            Polygon(*[(x, -y) for x, y in inner], *reversed(inner), align=None, mode=Mode.SUBTRACT)
        extrude(amount=SPACER_H)
        uprights = bp.edges().filter_by(Axis.Z)
        fillet([e for e in uprights if min(e.center().X, length - e.center().X) < 1e-6], SPACER_EDGE_R)
        uprights = bp.edges().filter_by(Axis.Z)
        fillet([e for e in uprights if min(abs(e.center().X - end_l), abs(e.center().X - length + end_l)) < 1e-6
                and abs(abs(e.center().Y) - slot / 2.0) < 1e-6], SPACER_EDGE_R)
    return _as_solid(bp.part)


def build_battery_spacers(p: EnclosureParams) -> tuple[Solid, Solid]:
    """Both bars in the cradle beside a nominal 34 mm cell, their beams bent to fit."""
    bat = p.battery
    cx, cy, _, out_h = _cradle_footprint(p)
    half_in = out_h / 2.0 - p.cradle_wall_t
    gap = half_in - SPACER_CELL_W / 2.0
    bar = build_battery_spacer(spacer_squeeze(p))
    x0 = cx - SPACER_LEN / 2.0
    return (_as_solid(Location((x0, cy - half_in + gap / 2.0, bat.z_start)) * bar),
            _as_solid(Location((x0, cy + half_in - gap / 2.0, bat.z_start)) * bar))


# --- Laser-case parts ----------------------------------------------------------------------------

TRIPOD_NUT_AF, TRIPOD_NUT_H = 11.10, 5.56   # 1/4"-20 UNC hex nut (HNSS 025 20), 7/16" AF x 7/32" H
TRIPOD_RIB_SQUEEZE = 0.25                     # Crush ribs reach this far into the nut's flats
# The plug fills the bottom-plate hole (0.1 mm play all round). It sits 0.625 mm toward +Z of the nut
# so the hole's -Z edge stops the slid-in nut's corner and the +Z side keeps >= 1.0 mm wall over the
# pocket's corner.
TRIPOD_PLUG_X, TRIPOD_PLUG_Z, TRIPOD_PLUG_DZ = 18.00, 14.15, 0.625
TRIPOD_POCKET_AF = 11.50                      # Slide-in pocket of the acrylic case's mount (matches shell)
TRIPOD_POCKET_H_CLEAR = 0.54                  # 6.10 mm pocket (+0.24 mm even at JLC3DP -0.3 mm PA12)
TRIPOD_FLOOR_T = 1.20
TRIPOD_FIN_GAP = 0.30   # Fin to board edge: PCB outline +-0.2, a 3.2 mm bottom plate sits 0.1 further in


def laser_bottom_faces(p: EnclosureParams) -> tuple[float, float]:
    """Inner and outer Y of the laser bottom plate."""
    y_in = -p.laser_inner_margin_y_neg
    return y_in, y_in - p.laser_t


def build_laser_tripod_mount(p: EnclosureParams) -> Solid:
    """Printed 1/4"-20 tripod nut holder for the laser case.

    Its plug fills the bottom-plate hole flush with the outside, so a 4.5 mm tripod screw gets
    3.3 mm of thread. The nut slides in from the back (-Z) along a channel, flats on X, until its
    corner sits in the pocket's V; two crush ribs hold it there. Once the plug is in the plate,
    the hole's edge closes the channel, so the nut is captive on every side: a 1.2 mm roof over
    it (with a screw relief) stops a tripod screw from pushing it into the camera. A 1 mm fin beside
    the main board's bottom edge stops the holder from lifting more than TRIPOD_FIN_GAP; the plug
    stops it from turning.
    """
    tx, tz = p.laser_tripod_x, p.laser_tripod_z
    y_in, y_out = laser_bottom_faces(p)
    y_plug_bot = y_out + 0.05
    y_nut_bot = y_out + TRIPOD_FLOOR_T
    y_pocket_top = y_nut_bot + TRIPOD_NUT_H + TRIPOD_POCKET_H_CLEAR
    y_top = y_pocket_top + MJF_MIN_WALL + 0.2
    body_x, body_z = 18.0, 16.0
    pocket_af = TRIPOD_POCKET_AF
    hex_r = pocket_af / math.sqrt(3.0)

    with BuildPart() as bp:
        with BuildSketch(_xz_plane(y_plug_bot)):
            with Locations((tx, tz + TRIPOD_PLUG_DZ)):
                RectangleRounded(TRIPOD_PLUG_X, TRIPOD_PLUG_Z, 1.5)
        extrude(amount=-(y_in - y_plug_bot))
        with BuildSketch(_xz_plane(y_in)):
            with Locations((tx, tz)):
                RectangleRounded(body_x, body_z, 2.0)
        extrude(amount=-(y_top - y_in))
        # Fin beside the main board's bottom edge (Y=0), up past the board (Z=20..21.6), ending
        # TRIPOD_FIN_GAP short of the edge
        fin_z0 = tz + body_z / 2.0 - 1.0
        with BuildSketch(_xz_plane(y_in)):
            with Locations((tx, 0.5 * (fin_z0 + p.pcb_z_back + p.pcb_t))):
                RectangleRounded(body_x, p.pcb_z_back + p.pcb_t - fin_z0, 0.8)
        extrude(amount=-(-TRIPOD_FIN_GAP - y_in))
        # Nut pocket (flats on X, corners on Z) and the channel it slides in by from the back face,
        # the screw hole through the floor and a relief through the roof for long screws
        z_back = tz - body_z / 2.0
        with BuildSketch(_xz_plane(y_pocket_top)):
            with Locations((tx, tz)):
                RegularPolygon(radius=hex_r, side_count=6, rotation=30.0)
            with Locations((tx, 0.5 * (z_back - 0.1 + tz))):
                Rectangle(pocket_af, tz - (z_back - 0.1))
        extrude(amount=y_pocket_top - y_nut_bot, mode=Mode.SUBTRACT)
        with BuildSketch(_xz_plane(y_plug_bot - 0.1)):
            with Locations((tx, tz)):
                Circle(p.shell_tripod_screw_d / 2.0)
        extrude(amount=-(y_top - y_plug_bot + 0.2), mode=Mode.SUBTRACT)
        # Two 1.0 mm crush ribs on the pocket's X flats reach TRIPOD_RIB_SQUEEZE into the nut, near
        # the V, so the nut slides freely until its last 3 mm. Each stands in a 0.8 mm groove with
        # 0.8 mm on either side, so it prints as a full-size MJF detail.
        pocket_flat = pocket_af / 2.0
        rib_in = TRIPOD_NUT_AF / 2.0 - TRIPOD_RIB_SQUEEZE
        groove_floor = pocket_flat + MJF_MIN_DETAIL
        y_mid = 0.5 * (y_nut_bot + y_pocket_top)
        rib_z0, rib_z1 = tz - 0.5, tz + 2.3   # Ends 1.0 mm short of the flat's corner edge
        for sx in (-1.0, 1.0):
            with BuildSketch(_xz_plane(y_mid + 0.5 + MJF_MIN_DETAIL)):
                with Locations((tx + sx * 0.5 * (pocket_flat - 0.05 + groove_floor), 0.5 * (rib_z0 + rib_z1))):
                    Rectangle(groove_floor - pocket_flat + 0.05, rib_z1 - rib_z0)
            extrude(amount=1.0 + 2.0 * MJF_MIN_DETAIL, mode=Mode.SUBTRACT)
            with BuildSketch(_xz_plane(y_mid + 0.5)):
                with Locations((tx + sx * 0.5 * (rib_in + groove_floor + 0.05), 0.5 * (rib_z0 + rib_z1))):
                    Rectangle(groove_floor + 0.05 - rib_in, rib_z1 - rib_z0)
            extrude(amount=1.0)

    return _as_solid(bp.part)


SD_CHUTE_LEN_IN = 0.80   # Chute ends 0.8 mm before the Teensy's end (Y=61.11)
# Bezel underside to barb back face: a 3.2 mm sheet plus 0.3 mm print tolerance, so the barbs click
# past the plate's inner face on any 2.8..3.2 mm sheet (on 2.8 mm the chute has 0.7 mm of play)
SD_CHUTE_GRIP = 3.50


def build_laser_sd_chute(p: EnclosureParams) -> Solid:
    """Printed microSD chute: pushed into the top plate from outside; two 0.8 mm barbs click behind it.

    Each barb sits on the free end of a side wall that is cut loose from the rest of the tube by
    0.8 mm slots: a 7.4 mm spring arm, rooted at the inner end, that bends 0.8 mm into the card
    slot while the barb passes the plate (about 2 % strain in PA12).
    """
    y_plate_in = p.pcb_h + p.laser_inner_margin_y_pos
    y_plate_out = y_plate_in + p.laser_t
    y_end = 61.11 + SD_CHUTE_LEN_IN
    neck_w, neck_h = p.microsd_w + 2.4, p.microsd_h + 2.4
    x, z = p.microsd_x, p.microsd_z
    gap = MJF_MIN_DETAIL
    y_arm_root, y_arm_tip = y_end + 1.0, y_plate_out - SD_CHUTE_GRIP

    with BuildPart() as bp:
        with BuildSketch(_xz_plane(y_end)):
            with Locations((x, z)):
                RectangleRounded(neck_w, neck_h, 0.8)
        extrude(amount=-(y_plate_out - y_end))
        with BuildSketch(_xz_plane(y_plate_out)):  # Outer bezel
            with Locations((x, z)):
                RectangleRounded(neck_w + 3.0, neck_h + 2.0, 1.2)
        extrude(amount=-MJF_MIN_WALL)
        for sx in (-1.0, 1.0):
            arm_in = x + sx * (p.microsd_w / 2.0)            # Inner face of the side wall
            # Slots through the top and bottom walls beside the arm, and a gap after its tip
            with Locations((arm_in + sx * (-gap / 2.0), 0.5 * (y_arm_root + y_arm_tip + gap), z)):
                Box(gap, y_arm_tip + gap - y_arm_root, neck_h + 1.0, mode=Mode.SUBTRACT)
            with Locations((x + sx * 0.5 * (p.microsd_w / 2.0 + neck_w / 2.0 + 2.0), y_arm_tip + gap / 2.0, z)):
                Box(neck_w / 2.0 - p.microsd_w / 2.0 + 2.0, gap, neck_h + 1.0, mode=Mode.SUBTRACT)
            # Barb: 1.6 mm ramp up to 0.8 mm, flat back face SD_CHUTE_GRIP under the bezel
            bx = x + sx * neck_w / 2.0
            with BuildSketch(Plane.XY.offset(z - 1.5)):
                Polygon((bx - sx * 0.2, y_arm_tip - 1.6), (bx + sx * gap, y_arm_tip), (bx - sx * 0.2, y_arm_tip),
                        align=None)
            extrude(amount=3.0)
        with BuildSketch(_xz_plane(y_end - 0.1)):
            with Locations((x, z)):
                RectangleRounded(p.microsd_w, p.microsd_h, 0.6)
        extrude(amount=-(y_plate_out - y_end + 1.0), mode=Mode.SUBTRACT)
        with BuildSketch(_xz_plane(y_plate_out + MJF_MIN_WALL + 0.01)):  # Lead-in at the outer opening
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
    "build_battery_spacer",
    "build_battery_spacers",
    "build_capture_button_plunger",
    "build_laser_display_spacer",
    "build_laser_sd_chute",
    "build_laser_tripod_mount",
    "build_lens_cap",
    "build_slide_switch_cap",
    "laser_bottom_faces",
]
