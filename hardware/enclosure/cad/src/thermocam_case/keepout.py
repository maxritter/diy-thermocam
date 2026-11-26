"""Datasheet models of everything inside the camera, for the clearance tests.

Positions are in the camera frame (see params.py). Sources:
  - KiCad board `hardware/pcb/diy-thermocam-v4.0.kicad_pcb` (footprint positions, pad clearances)
  - FLIR Lepton datasheet 500-0771-01: 11.8 x 12.7 x 7.14 mm in the Molex 105028 socket
  - FLIR Lepton Breakout V2.0 drawing: 29.5 x 29.0 mm board, holes 2 mm from the edges
  - PJRC Teensy 4.1 drawing: 61.0 x 17.8 x 1.57 mm, micro-USB overhang 0.7 mm
  - BuyDisplay ER-TFTM032-3 drawing: module 77.7 x 55.04 x 3.6 (+0.3), back-side parts <= 2.5 mm
  - Omron B3U-3000P, C&K JS102011SAQN, XKB TS-1187A, HRO TYPE-C-31-M-12, JST S2B-PH-SM4-TB
KiCad ships no 3D models for SW1, SW3, J3, J4 and U2, so those come from the datasheets here.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

from build123d import Align, Box, Cone, Cylinder, Location, Part, Rotation, Solid

from .params import EnclosureParams


def _as_solid(shape: Part | Solid) -> Solid:
    if isinstance(shape, Solid):
        return shape
    solids = shape.solids()
    assert len(solids) == 1, f"Expected 1 solid, got {len(solids)}"
    return solids[0]


def box(x0: float, x1: float, y0: float, y1: float, z0: float, z1: float) -> Solid:
    return _as_solid(Location((0.5 * (x0 + x1), 0.5 * (y0 + y1), 0.5 * (z0 + z1))) * Box(x1 - x0, y1 - y0, z1 - z0))


def cyl(x: float, y: float, z0: float, z1: float, r: float) -> Solid:
    return _as_solid(Location((x, y, z0)) * Cylinder(r, z1 - z0, align=(Align.CENTER, Align.CENTER, Align.MIN)))


def board(p: EnclosureParams, x0: float, x1: float, y0: float, y1: float, z0: float, z1: float,
          lepton_holes: bool) -> Solid:
    """A PCB slab with its M2.5 mounting holes (and the M2 Lepton holes on the main board)."""
    holes = [(x, y, p.mounting_hole_d / 2.0) for x, y in p.mounting_holes]
    if lepton_holes:
        holes += [(x, y, 1.05) for x, y in p.lepton_breakout_holes]
    s = box(x0, x1, y0, y1, z0, z1)
    for x, y, r in holes:
        s = _as_solid(s - cyl(x, y, z0 - 0.1, z1 + 0.1, r))
    return s


def hex_prism(x: float, y: float, z0: float, z1: float, af: float) -> Solid:
    from build123d import BuildPart, BuildSketch, Locations, Plane, RegularPolygon, extrude

    with BuildPart() as bp:
        with BuildSketch(Plane.XY.offset(z0)):
            with Locations((x, y)):
                RegularPolygon(radius=af / math.sqrt(3.0), side_count=6, rotation=30.0)
        extrude(amount=z1 - z0)
    return _as_solid(bp.part)


def build_fov_cone(
    p: EnclosureParams,
    cx: float,
    cy: float,
    far_z: float = -20.0,
    dfov_deg: float | None = None,
) -> Solid:
    cone_len = p.lepton_pupil_z - far_z
    r_pupil = p.lepton_pupil_clear_d / 2.0
    angle = p.lepton_dfov_deg if dfov_deg is None else dfov_deg
    r_far = r_pupil + cone_len * math.tan(math.radians(angle / 2.0))
    return _as_solid(
        Location((cx, cy, far_z))
        * Cone(bottom_radius=r_far, top_radius=r_pupil, height=cone_len, align=(Align.CENTER, Align.CENTER, Align.MIN))
    )


# Pin tails of the back-side THT sockets on the PCB front (KiCad pad centres, checked in the tests):
# J2 2x10 at X 31.40 + 2.54 k, Y 21.00 / 23.54; J5 / J6 1x24 at X 59.28 / 74.52, Y 1.42 + 2.54 k.
# Each tail is a Ø1.7 mm solder fillet (the pad) 1.6 mm tall (KiCad STEP: 1.51 mm).
J2_TAILS = tuple((31.40 + 2.54 * k, y) for k in range(10) for y in (21.00, 23.54))
J5J6_TAILS = tuple((x, 1.42 + 2.54 * k) for x in (59.28, 74.52) for k in range(24))
THT_TAIL_R, THT_TAIL_H = 0.85, 1.60


def front_tht_tails(p: EnclosureParams) -> tuple[Solid, ...]:
    z0 = p.pcb_z_back + p.pcb_t
    return tuple(cyl(x, y, z0, z0 + THT_TAIL_H, THT_TAIL_R) for x, y in (*J2_TAILS, *J5J6_TAILS))


def sw1_actuator(dy: float) -> Solid:
    """C&K JS102011SAQN actuator, 1.5 mm wide, X 89.0..91.0, at `dy` from its centre (+-1.0 mm)."""
    return box(89.0, 91.0, 12.7 + dy - 0.75, 12.7 + dy + 0.75, 16.6, 18.35)


def sw2_plunger(depress: float = 0.0) -> Solid:
    """B3U-3000P plunger tip, 1.0 mm wide, 0.55 mm past the PCB edge at rest."""
    return box(83.5, 84.5, 68.25, 68.95 - depress, 18.8, 19.8)


@dataclass(frozen=True)
class KeepoutAssembly:
    pcb: Solid
    pcb_tht_pins: tuple[Solid, ...]
    display_pcb: Solid
    display_back_smds: Solid
    display_panel: Solid
    battery: Solid
    tripod_stud: Solid
    lepton_fov_cones_all: tuple[tuple[str, Solid], ...]
    lepton_31r_fov_cones_all: tuple[tuple[str, Solid], ...]
    lepton_fov_cone: Solid
    # Every real part by name; "approx_" parts are outlines where no exact drawing exists
    real: dict[str, Solid] = field(default_factory=dict)


def build_real_parts(p: EnclosureParams) -> dict[str, Solid]:
    pcb_back, pcb_top = p.pcb_z_back, p.pcb_z_back + p.pcb_t
    m: dict[str, Solid] = {}
    # Switches and connectors on the PCB back (B.Cu)
    m["SW1_body"] = box(85.4, 89.0, 8.2, 17.2, 16.5, pcb_back)
    m["SW1_actuator"] = sw1_actuator(0.0)
    m["SW2_body"] = box(82.5, 85.5, 65.75, 68.25, 18.8, pcb_back)
    m["SW2_plunger"] = sw2_plunger()
    m["SW3"] = box(85.3 - 2.55, 85.3 + 2.55, 4.0 - 2.55, 4.0 + 2.55, 18.5, pcb_back)
    m["D2_led"] = box(5.75 - 0.8, 5.75 + 0.8, 66.4 - 0.4, 66.4 + 0.4, 19.2, pcb_back)
    m["J3_usb_c"] = box(45.8 - 4.47, 45.8 + 4.47, -0.3, 7.0, pcb_back - 3.26, pcb_back)
    m["J4_battery"] = box(80.55, 88.45, 47.7, 55.3, 15.0, pcb_back)
    m["U4_C11"] = box(24.55, 28.45, 3.44, 9.72, 18.6, pcb_back)
    # Teensy 4.1 on 8.5 mm J5/J6 sockets + 2.54 mm header plastic, component side toward the back
    tz1 = pcb_back - 8.5 - 2.54
    tz0 = tz1 - 1.57
    m["T41_pcb"] = box(66.9 - 8.89, 66.9 + 8.89, 0.15, 61.11, tz0, tz1)
    m["T41_usb"] = box(66.9 - 4.03, 66.9 + 4.03, 0.15 - 0.7, 0.15 + 4.3, tz0 - 2.5, tz0)
    m["T41_sd_socket"] = box(66.9 - 6.35, 66.9 + 6.35, 61.11 - 12.7, 61.11, tz0 - 1.9, tz0)
    m["T41_sd_card"] = box(66.9 - 5.5, 66.9 + 5.5, 61.11 - 12.5, 61.11 + 2.5, tz0 - 1.3, tz0 - 0.3)
    # Header tails + solder poke 1.45 mm out of the component side. The five inner pads stay empty:
    # spring pins P3/P4 press on them from the PCB, inside the J5-J6 span
    for sx in (59.28, 74.52):
        m[f"T41_header_{int(sx)}"] = box(sx - 1.27, sx + 1.27, 0.15, 61.11, tz1, pcb_back - 8.5)
        m[f"T41_tails_{int(sx)}"] = box(sx - 1.27, sx + 1.27, 0.15, 61.11, tz0 - 1.45, tz0)
        m[f"J5J6_socket_{int(sx)}"] = box(sx - 1.27, sx + 1.27, 0.15, 61.11, pcb_back - 8.5, pcb_back)
    # Lepton Breakout V2.0 on 12 mm sleeves; Lepton 3.5 in its Molex socket faces the back wall
    z_bo_top = pcb_back - p.lepton_sleeve_len
    z_bo_bot = z_bo_top - p.breakout_t
    m["breakout_pcb"] = box(28.0, 57.5, 32.0, 61.0, z_bo_bot, z_bo_top)
    m["lepton_socketed"] = box(
        p.lepton_x - p.lepton_body_w / 2.0, p.lepton_x + p.lepton_body_w / 2.0,
        p.lepton_y - p.lepton_body_l / 2.0, p.lepton_y + p.lepton_body_l / 2.0,
        z_bo_bot - p.lepton_socketed_h, z_bo_bot,
    )
    for i, (hx, hy) in enumerate(p.lepton_breakout_holes):
        m[f"lepton_sleeve_{i}"] = cyl(hx, hy, z_bo_top, pcb_back, 2.0)
        m[f"lepton_washer_{i}"] = cyl(hx, hy, pcb_top, pcb_top + 0.3, 2.5)
        m[f"lepton_head_{i}"] = cyl(hx, hy, pcb_top + 0.3, pcb_top + 0.3 + 1.3, 1.9)
        m[f"lepton_nut_{i}"] = hex_prism(hx, hy, z_bo_bot - 1.6, z_bo_bot, 4.0)
        m[f"lepton_screw_tip_{i}"] = cyl(hx, hy, pcb_top + 0.3 - 18.0, z_bo_bot - 1.6, 1.0)
    # Right-angle 2x10 female adapter between the breakout header and J2 (outline only)
    m["approx_lepton_adapter"] = box(30.0, 55.7, 20.0, 32.0, z_bo_top, pcb_back - 8.5)
    # HC-PM254-8.5H-2x10PZ: body = KiCad courtyard (29.69..56.07 x 19.52..25.11) less its 0.25 mm margin
    m["J2_socket"] = box(29.94, 55.82, 19.77, 24.86, pcb_back - 8.5, pcb_back)
    # Display: J1 socket + display header, PCB, module (max height), back-side parts
    m["J1_display_socket"] = box(0.06, 5.14, 9.05, 59.85, pcb_top, p.display_pcb_z_back)
    m["display_pcb"] = board(p, 0.0, 89.4, 2.0, 66.9, p.display_pcb_z_back, p.display_pcb_z_back + p.display_pcb_t,
                             lepton_holes=False)
    z_disp_top = p.display_pcb_z_back + p.display_pcb_t
    m["display_module"] = box(8.34, 8.34 + 77.7, 34.45 - 27.52, 34.45 + 27.52, z_disp_top, z_disp_top + 3.9)
    # LiPo at its worst-case size
    bat = p.battery
    m["battery"] = box(bat.center_x - bat.width_x / 2.0, bat.center_x + bat.width_x / 2.0,
                       bat.center_y - bat.height_y / 2.0, bat.center_y + bat.height_y / 2.0,
                       bat.z_start, bat.z_start + bat.thickness_z)
    return m


def build_keepouts(p: EnclosureParams) -> KeepoutAssembly:
    pcb = board(p, 0.0, p.pcb_w, 0.0, p.pcb_h, p.pcb_z_back, p.pcb_z_back + p.pcb_t, lepton_holes=True)
    pcb_tht_pins = front_tht_tails(p)
    real = build_real_parts(p)
    smd_h = p.display_pcb_smd_keepout_h
    display_back_smds = box(6.0, 87.0, 4.5, 64.5, p.display_pcb_z_back - smd_h, p.display_pcb_z_back)
    display_panel = _as_solid(
        Location((p.display_win_cx, p.display_win_cy, p.display_pcb_z_back + p.display_pcb_t + 2.425))
        * Box(p.display_win_w, p.display_win_h, 4.85)
    )
    # A long 7.8 mm tripod screw in the shell's tripod nut
    y_bottom_outer = -(p.shell_clearance_y_neg + p.shell_wall)
    stud_len = 7.80
    tripod_stud = _as_solid(
        Location((p.tripod_x, y_bottom_outer + stud_len / 2.0, p.shell_tripod_z))
        * Rotation(90, 0, 0)
        * Cylinder(radius=3.175, height=stud_len)
    )
    # Optical axis with +-0.5 mm placement tolerance
    centers = [(p.lepton_x + dx, p.lepton_y + dy) for dx, dy in ((0, 0), (0.5, 0.5), (-0.5, -0.5), (0.5, -0.5), (-0.5, 0.5))]
    fov = tuple((f"3.5 at ({cx:.2f},{cy:.2f})", build_fov_cone(p, cx, cy)) for cx, cy in centers)
    fov_31r = tuple(
        (f"3.1R at ({cx:.2f},{cy:.2f})", build_fov_cone(p, cx, cy, dfov_deg=p.lepton_31r_dfov_deg)) for cx, cy in centers
    )
    return KeepoutAssembly(
        pcb=pcb,
        pcb_tht_pins=pcb_tht_pins,
        display_pcb=real["display_pcb"],
        display_back_smds=display_back_smds,
        display_panel=display_panel,
        battery=real["battery"],
        tripod_stud=tripod_stud,
        lepton_fov_cones_all=fov,
        lepton_31r_fov_cones_all=fov_31r,
        lepton_fov_cone=fov[0][1],
        real=real,
    )
