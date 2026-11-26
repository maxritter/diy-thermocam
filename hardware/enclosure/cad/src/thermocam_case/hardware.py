"""Purchased hardware for the V4.0 enclosures, modelled at catalogue dimensions.

Used by the clearance tests (standoffs, screw tips, solder joints) and by the Blender renders.
The board stack is the same in the 3D-printed shell and the laser-cut case:

    back wall | 4x M2.5x20 F-F | 4x nylon washer | main PCB | 4x M2.5x11 M-F | display PCB | front
    Z = -0.5 ......... 19.5 ...... 20.0 ........ 21.6 ....... 32.6 ............ 34.2 .... 39.2

The front screws (M2.5x14) pass through 5 mm bosses (shell) or printed spacer bars (laser) and the
display PCB into the female thread of the 11 mm standoffs.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from build123d import (
    Align,
    Box,
    BuildPart,
    BuildSketch,
    Circle,
    Compound,
    Cone,
    Cylinder,
    Location,
    Locations,
    Mode,
    Plane,
    Pos,
    RegularPolygon,
    Solid,
    extrude,
)

from .params import EnclosureParams

M25_HEX_AF = 5.0          # Standoff across-flats (SW 5)
M25_TAP_D = 2.5
M25_MALE_LEN = 6.0        # Male thread length of the M-F standoffs
DIN965_M25_HEAD_D = 4.7   # ISO 7046 M2.5 actual head diameter (max)
DIN965_M25_HEAD_D_MIN = 4.4  # ... and (min): the 90 deg cone is cut off this far below its Ø5.5 edge
DIN965_M25_HEAD_K = 1.5   # 90 deg countersunk head height (from the theoretical Ø5.5 edge)
PAN_M25_HEAD_D, PAN_M25_HEAD_K = 5.0, 1.75  # ISO 7045 M2.5 pan head
FRONT_SCREW_LEN = 14.0    # M2.5x14 through the front face, 5 mm boss / spacer and display PCB
PH1_RECESS_L, PH1_RECESS_W, PH1_RECESS_DEPTH = 2.6, 0.6, 0.9  # ISO 4757 Phillips PH1 cross (M2.5 heads)


@dataclass(frozen=True)
class ScrewJoint:
    """One screw into a standoff: how far it protrudes past the clamped material vs. the thread depth."""

    name: str
    screw_len: float      # Nominal length (countersunk: overall incl. head; pan head: under head)
    clamped: float        # Material between head seat and standoff face
    countersunk: bool
    thread_depth: float   # Female thread depth of the standoff it goes into
    head_sink: float = 0.0  # Countersunk: how much deeper a real head reaches than one flush at the face

    @property
    def engagement(self) -> float:
        return self.screw_len - self.clamped

    @property
    def engagement_max(self) -> float:
        return self.engagement + self.head_sink


# Female thread depths of the Ettinger standoffs: 005.02.205 (M2.5x20 F-F) 6 mm both ends;
# 005.12.115 (M2.5x11 M-F, brass) 6 mm, 005.12.119 (stainless) 7 mm -> use 6. Male studs 6 mm.
REAR_STANDOFF_THREAD, MID_STANDOFF_THREAD = 6.0, 6.0
# Shell back screws: M2.5x6 (Ettinger 001.12.228) bite 2.9..3.5 mm; an M2.5x8 reached 4.9..5.5 mm into
# the 6 mm thread and could bottom out with its length tolerance and a thin printed back face
SHELL_BACK_SCREW_LEN = 6.0
LASER_WASHER_T = 0.5  # ISO 7089 M2.5 polyamide washer under each pan head on acrylic
M2_WASHER_T = 0.3     # Ø2.2/5.0 x 0.3 polyamide washer under each Lepton screw head


def screw_joints(p: EnclosureParams) -> tuple[ScrewJoint, ...]:
    seat_depth = -p.standoff_seat_z  # Standoffs stand in 0.5 mm seats in the back wall
    front_stack = p.internal_depth - (p.display_pcb_z_back + p.display_pcb_t) + p.display_pcb_t
    # A real ISO 7046 head is cut off below its Ø5.5 cone edge and its length counts from that top,
    # so in the Ø5.6 countersink its tip reaches up to (5.6 - 4.4) / 2 = 0.6 mm deeper than flush
    sink = (p.countersink_head_d - DIN965_M25_HEAD_D_MIN) / 2.0
    return (
        ScrewJoint("3D back: M2.5x6 ISO 7046", SHELL_BACK_SCREW_LEN, p.shell_back_face_t - seat_depth, True, REAR_STANDOFF_THREAD, sink),
        ScrewJoint("3D front: M2.5x14 ISO 7046", FRONT_SCREW_LEN, p.shell_face_t + front_stack, True,
                   MID_STANDOFF_THREAD, sink),
        ScrewJoint("Laser back: M2.5x8 ISO 7045 + washer", 8.0, p.laser_t + LASER_WASHER_T, False, REAR_STANDOFF_THREAD),
        ScrewJoint("Laser front: M2.5x14 ISO 7045 + washer", FRONT_SCREW_LEN,
                   LASER_WASHER_T + p.laser_t + front_stack, False, MID_STANDOFF_THREAD),
        ScrewJoint("Mid standoff male stud", M25_MALE_LEN, p.pcb_t + p.rear_washer_t, False, REAR_STANDOFF_THREAD),
    )


# J5 / J6 Teensy 1x24 socket pads (B.Cu socket, solder joints protrude on the F.Cu side)
TEENSY_SOCKET_XS = (59.28, 74.52)
TEENSY_SOCKET_Y0, TEENSY_PITCH, TEENSY_PINS = 1.42, 2.54, 24


def _solid(shape: object) -> Solid:
    """Return the single fused Solid of a build result (all hardware parts are one connected body)."""
    if isinstance(shape, Solid):
        return shape
    solids = shape.solids()  # type: ignore[attr-defined]
    assert len(solids) == 1, f"expected one solid, got {len(solids)}"
    return solids[0]


def hex_standoff(x: float, y: float, z0: float, length: float, male: bool, af: float = M25_HEX_AF) -> Solid:
    """M2.5 hex standoff from z0 upward; optional male stud pointing down (-Z)."""
    with BuildPart() as bp:
        with BuildSketch(Plane.XY.offset(z0)):
            with Locations((x, y)):
                RegularPolygon(radius=af / math.sqrt(3.0), side_count=6, rotation=30.0)
                Circle(1.05, mode=Mode.SUBTRACT)
        extrude(amount=length)
        if male:
            with Locations((x, y, z0)):
                Cylinder(M25_TAP_D / 2.0 - 0.05, M25_MALE_LEN, align=(Align.CENTER, Align.CENTER, Align.MAX))
    return _solid(bp.part)


def _cross_recess(x: float, y: float, z_top: float, into: int) -> Solid:
    """Phillips cross cut from a head top at `z_top` into the head (`into` = +1 along +Z, -1 along -Z).

    Returned as a solid to subtract: build123d does not apply a helper's builder calls to its caller's part.
    """
    align = (Align.CENTER, Align.CENTER, Align.MIN if into > 0 else Align.MAX)
    cross = Box(PH1_RECESS_L, PH1_RECESS_W, PH1_RECESS_DEPTH, align=align) + Box(
        PH1_RECESS_W, PH1_RECESS_L, PH1_RECESS_DEPTH, align=align)
    return _solid(Pos(x, y, z_top) * cross)


def countersunk_screw(x: float, y: float, z_head: float, length: float, pointing: int) -> Solid:
    """ISO 7046 M2.5 countersunk screw; `z_head` is the flush head top, `pointing` = +1 (into +Z) or -1."""
    k = DIN965_M25_HEAD_K
    with BuildPart() as bp:
        if pointing > 0:
            with Locations((x, y, z_head)):
                Cone(DIN965_M25_HEAD_D / 2.0, M25_TAP_D / 2.0, k, align=(Align.CENTER, Align.CENTER, Align.MIN))
                Cylinder(M25_TAP_D / 2.0 - 0.05, length, align=(Align.CENTER, Align.CENTER, Align.MIN))
        else:
            with Locations((x, y, z_head)):
                Cone(M25_TAP_D / 2.0, DIN965_M25_HEAD_D / 2.0, k, align=(Align.CENTER, Align.CENTER, Align.MAX))
                Cylinder(M25_TAP_D / 2.0 - 0.05, length, align=(Align.CENTER, Align.CENTER, Align.MAX))
    return _solid(_solid(bp.part).cut(_cross_recess(x, y, z_head, pointing)))


def pan_head_screw(x: float, y: float, z_seat: float, length: float, pointing: int) -> Solid:
    """ISO 7045 M2.5 pan head screw seated on a plate face at `z_seat`."""
    k = PAN_M25_HEAD_K
    with BuildPart() as bp:
        if pointing > 0:
            with Locations((x, y, z_seat)):
                Cylinder(PAN_M25_HEAD_D / 2.0, k, align=(Align.CENTER, Align.CENTER, Align.MAX))
                Cylinder(M25_TAP_D / 2.0 - 0.05, length, align=(Align.CENTER, Align.CENTER, Align.MIN))
        else:
            with Locations((x, y, z_seat)):
                Cylinder(PAN_M25_HEAD_D / 2.0, k, align=(Align.CENTER, Align.CENTER, Align.MIN))
                Cylinder(M25_TAP_D / 2.0 - 0.05, length, align=(Align.CENTER, Align.CENTER, Align.MAX))
    return _solid(_solid(bp.part).cut(_cross_recess(x, y, z_seat - pointing * k, pointing)))


def flat_washer(x: float, y: float, z0: float, d_out: float = 6.0, d_in: float = 2.7, t: float = 0.5) -> Solid:
    """ISO 7089 washer lying on z0 (default: M2.5 PA6.6, 2.7/6.0 x 0.5)."""
    with BuildPart() as bp:
        with Locations((x, y, z0)):
            Cylinder(d_out / 2.0, t, align=(Align.CENTER, Align.CENTER, Align.MIN))
            Cylinder(d_in / 2.0, t, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
    return _solid(bp.part)


def tripod_hex_nut(x: float, z: float, y_bottom: float, corners_along_x: bool = False) -> Solid:
    """1/4"-20 UNC hex nut (7/16" = 11.1 mm AF, 5.5 mm thick) with its axis along Y."""
    af, h = 11.11, 5.50
    with BuildPart() as bp:
        with BuildSketch(Plane(origin=(x, y_bottom, z), x_dir=(1, 0, 0), z_dir=(0, 1, 0))):
            RegularPolygon(radius=af / math.sqrt(3.0), side_count=6, rotation=0.0 if corners_along_x else 30.0)
            Circle(3.25, mode=Mode.SUBTRACT)
        extrude(amount=h)
    return _solid(bp.part)


@dataclass(frozen=True)
class StandoffStack:
    rear: tuple[Solid, ...]     # 4x M2.5x20 F-F, brass
    washers: tuple[Solid, ...]  # 4x M2.5 nylon washer between rear standoff and PCB
    mid: tuple[Solid, ...]      # 4x M2.5x11 M-F

    @property
    def all(self) -> tuple[Solid, ...]:
        return (*self.rear, *self.washers, *self.mid)


def build_standoff_stack(p: EnclosureParams) -> StandoffStack:
    pcb_top = p.pcb_z_back + p.pcb_t
    rear_len = p.pcb_z_back - p.rear_washer_t - p.standoff_seat_z
    return StandoffStack(
        rear=tuple(hex_standoff(x, y, p.standoff_seat_z, rear_len, male=False) for x, y in p.mounting_holes),
        washers=tuple(
            flat_washer(x, y, p.pcb_z_back - p.rear_washer_t, d_out=p.rear_washer_d, t=p.rear_washer_t)
            for x, y in p.mounting_holes
        ),
        mid=tuple(hex_standoff(x, y, pcb_top, p.display_pcb_z_back - pcb_top, male=True) for x, y in p.mounting_holes),
    )


def build_shell_screws(p: EnclosureParams, z_back_outer: float, z_front_outer: float) -> tuple[Solid, ...]:
    """4x M2.5x6 ISO 7046 through the back face + 4x M2.5x14 ISO 7046 through the front face (flush)."""
    back = [countersunk_screw(x, y, z_back_outer, SHELL_BACK_SCREW_LEN, pointing=+1) for x, y in p.mounting_holes]
    front = [countersunk_screw(x, y, z_front_outer, FRONT_SCREW_LEN, pointing=-1) for x, y in p.mounting_holes]
    return (*back, *front)


def teensy_socket_solder_joints(p: EnclosureParams) -> Compound:
    """J5/J6 THT solder joints on the F.Cu face: Ø2.3 mm (1.7 mm pad + fillet margin), 1.6 mm tall."""
    pcb_top = p.pcb_z_back + p.pcb_t
    joints = [
        _solid(Location((sx, TEENSY_SOCKET_Y0 + i * TEENSY_PITCH, pcb_top))
               * Cylinder(1.15, 1.6, align=(Align.CENTER, Align.CENTER, Align.MIN)))
        for sx in TEENSY_SOCKET_XS
        for i in range(TEENSY_PINS)
    ]
    return Compound(children=joints)


__all__ = [
    "StandoffStack",
    "build_shell_screws",
    "build_standoff_stack",
    "countersunk_screw",
    "flat_washer",
    "hex_standoff",
    "pan_head_screw",
    "screw_joints",
    "teensy_socket_solder_joints",
    "tripod_hex_nut",
]
