"""Reference keepout solids and Lepton FOV cones for automated geometry checks.

Models real physical internal hardware per Fable 5.2 (#176) & Carbon (#169):
  - Real BuyDisplay ER-TFTM032-3 PCB: 89.4 x 64.9 x 1.6 mm (Y=2.0..66.9) + 2.2 mm back SMDs
  - Real 1.55 mm PCB THT pin tails (Z=21.60..23.15)
  - Swollen LiPo keepout (5.5 mm = 5.0 mm cell + 10% swell)
  - 8.0 mm long camera tripod stud (verifying blind roof stops even oversized studs!)
  - BOTH Lepton optical centers ((42.75, 51.00) and (44.50, 51.50))
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from build123d import Align, Box, Cone, Cylinder, Location, Part, Rotation, Solid

from .params import EnclosureParams


@dataclass(frozen=True)
class KeepoutAssembly:
    pcb: Solid
    pcb_tht_pins: Solid
    display_pcb: Solid
    display_back_smds: Solid
    display_panel: Solid
    battery: Solid
    lepton_module: Solid
    tripod_stud: Solid
    lepton_fov_cone: Solid
    lepton_fov_cones_all: tuple[tuple[str, Solid], ...]
    lepton_31r_fov_cones_all: tuple[tuple[str, Solid], ...]


def _as_solid(shape: Part | Solid) -> Solid:
    if isinstance(shape, Solid):
        return shape
    solids = shape.solids()
    assert len(solids) == 1, f"Expected 1 solid, got {len(solids)}"
    return solids[0]


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
        * Cone(
            bottom_radius=r_far,
            top_radius=r_pupil,
            height=cone_len,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        )
    )


def build_keepouts(p: EnclosureParams) -> KeepoutAssembly:
    # Main PCB: X in [0, 89.4], Y in [0, 68.4], Z in [20.0, 21.6]
    pcb = _as_solid(
        Location((p.pcb_w / 2.0, p.pcb_h / 2.0, p.pcb_z_back + p.pcb_t / 2.0))
        * Box(p.pcb_w, p.pcb_h, p.pcb_t)
    )

    # Real PCB STEP THT pin tails (1.60 mm conservative protrusion -> Z in [21.60, 23.20], Fable #176!)
    pcb_tht_pins = _as_solid(
        Location((53.125, 33.80, p.pcb_z_back + p.pcb_t + 0.80))
        * Box(45.25, 62.20, 1.60)
    )

    # Real BuyDisplay ER-TFTM032-3 PCB: 89.4 x 64.9 x 1.6 mm (Y in [2.00, 66.90], Z in [32.6, 34.2])
    display_pcb = _as_solid(
        Location(
            (
                p.display_pcb_center_x,
                p.display_pcb_center_y,
                p.display_pcb_z_back + p.display_pcb_t / 2.0,
            )
        )
        * Box(p.display_pcb_w, p.display_pcb_h, p.display_pcb_t)
    )

    # Display PCB backside SMD/FPC/SD socket keepout: Z in [30.40, 32.60] (2.20 mm)
    smd_h = p.display_pcb_smd_keepout_h
    display_back_smds = _as_solid(
        Location(
            (
                p.display_pcb_center_x,
                p.display_pcb_center_y,
                p.display_pcb_z_back - smd_h / 2.0,
            )
        )
        * Box(p.display_pcb_w - 6.0, p.display_pcb_h - 6.0, smd_h)
    )

    # Display active LCD glass stack: Z in [34.2, 39.05] (fits cleanly in exact 39.20 mm standoff cavity!)
    display_panel = _as_solid(
        Location((p.display_win_cx, p.display_win_cy, p.display_pcb_z_back + p.display_pcb_t + 2.425))
        * Box(p.display_win_w, p.display_win_h, 4.85)
    )

    # Swollen LiPo battery keepout volume
    bat = p.battery
    battery = _as_solid(
        Location((bat.center_x, bat.center_y, bat.z_start + bat.thickness_z / 2.0))
        * Box(bat.width_x, bat.height_y, bat.thickness_z)
    )

    # FLIR Lepton 3.5 + Breakout V2 stack (Y in [19.0, 61.0], X in [28.0, 57.5], Z in [0.4, 20.0])
    lepton_module = _as_solid(Location((42.75, 46.5, 10.2)) * Box(29.5, 38.0, 19.2))

    # Long 8.0 mm camera tripod stud entering back_shell bottom tripod pocket at (44.70, Z=7.20)
    # Stops inside nut cavity (Y <= +4.20), completely blocked by 3.0 mm solid blind roof!
    y_bottom_outer = -(p.shell_clearance_y_neg + p.shell_wall)  # -3.80
    stud_len = 7.80  # Tests even long 7.8 mm tripod screw!
    tripod_stud = _as_solid(
        Location((p.tripod_x, y_bottom_outer + stud_len / 2.0, p.shell_tripod_z))
        * Rotation(90, 0, 0)
        * Cylinder(radius=3.175, height=stud_len)
    )

    fov_primary = build_fov_cone(p, p.lepton_x, p.lepton_y)
    fov_all = (
        ("breakout_center_42.75_51.00", build_fov_cone(p, 42.75, 51.00)),
        ("legacy_center_44.50_51.50", build_fov_cone(p, 44.50, 51.50)),
        ("nominal_center_43.60_51.25", fov_primary),
    )
    fov_31r_all = (
        ("3.1R_breakout_42.75_51.00", build_fov_cone(p, 42.75, 51.00, dfov_deg=p.lepton_31r_dfov_deg)),
        ("3.1R_legacy_44.50_51.50", build_fov_cone(p, 44.50, 51.50, dfov_deg=p.lepton_31r_dfov_deg)),
        ("3.1R_nominal_43.60_51.25", build_fov_cone(p, p.lepton_x, p.lepton_y, dfov_deg=p.lepton_31r_dfov_deg)),
    )

    return KeepoutAssembly(
        pcb=pcb,
        pcb_tht_pins=pcb_tht_pins,
        display_pcb=display_pcb,
        display_back_smds=display_back_smds,
        display_panel=display_panel,
        battery=battery,
        lepton_module=lepton_module,
        tripod_stud=tripod_stud,
        lepton_fov_cone=fov_primary,
        lepton_fov_cones_all=fov_all,
        lepton_31r_fov_cones_all=fov_31r_all,
    )
