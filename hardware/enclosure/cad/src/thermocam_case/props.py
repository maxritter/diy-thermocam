"""Render-only models of the plug-in modules, placed at their real positions in the camera frame.

They give the Blender renders the look of the real parts; the clearance tests also check them against
every printed part. Layouts are measured from the vendor photos and drawings:
  - PJRC Teensy 4.1 (61.0 x 17.8 x 1.57 mm), plugged into J5/J6 (X=59.33/74.57) on the PCB back,
    component side toward the back wall; its five inner pads stay empty (spring pins P3/P4 press on them)
  - FLIR Lepton 3.5 (10.5 x 10.5 face, 7.14 mm socketed) on the Lepton carrier board, hung on
    12 mm sleeves below the PCB; its straight 2x10 header plugs into J2
  - BuyDisplay ER-TFTM032-3: black 89.4 x 64.9 mm PCB, white backlight frame, 3.2" TFT glass
  - 505060 LiPo pouch (5.0 x 50 x 60 mm) with its kapton-wrapped protection board toward J4
  - USB-C receptacle J3 (HRO TYPE-C-31-M-12; KiCad ships no 3D model for it)
Decals (silkscreen, chip markings, battery label) come from `decals.py`.
"""

from __future__ import annotations

import math
from collections.abc import Sequence
from dataclasses import dataclass

from build123d import (
    Align,
    Axis,
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
    Polygon,
    RectangleRounded,
    RegularPolygon,
    Solid,
    Spline,
    extrude,
    fillet,
    sweep,
)

from .params import EnclosureParams

TEENSY_W, TEENSY_L, TEENSY_T = 17.78, 60.96, 1.57
TEENSY_PCB_Z_TOP = 9.00  # Header plastic (2.5 mm) sits on the 8.5 mm J5/J6 sockets (Z=11.5..20.0)
TEENSY_Y0 = 0.15         # USB end of the board (pin 1 at Y=1.47 in KiCad, 1.32 mm from the edge)
PIN_SQ, PIN_LONG = 0.64, 6.0
HEADER_H = 2.5
SOLDER_STUB = 1.45       # Header tail + solder past the component side


def _solid(shape: object) -> Solid:
    if isinstance(shape, Solid):
        return shape
    solids = shape.solids()  # type: ignore[attr-defined]
    assert len(solids) == 1, f"expected one solid, got {len(solids)}"
    return solids[0]


def _box(cx: float, cy: float, z0: float, w: float, h: float, t: float) -> Solid:
    return _solid(Location((cx, cy, z0)) * Box(w, h, t, align=(Align.CENTER, Align.CENTER, Align.MIN)))


def _rounded_slab(cx: float, cy: float, z0: float, w: float, h: float, t: float, r: float) -> Solid:
    with BuildPart() as bp:
        with BuildSketch(Plane.XY.offset(z0)):
            with Locations((cx, cy)):
                RectangleRounded(w, h, r)
        extrude(amount=t)
    return _solid(bp.part)


def _cyl(x: float, y: float, z0: float, r: float, h: float) -> Solid:
    return _solid(Location((x, y, z0)) * Cylinder(r, h, align=(Align.CENTER, Align.CENTER, Align.MIN)))


def _annulus(x: float, y: float, z0: float, r_out: float, r_in: float, h: float) -> Solid:
    with BuildPart() as bp:
        with Locations((x, y, z0)):
            Cylinder(r_out, h, align=(Align.CENTER, Align.CENTER, Align.MIN))
            Cylinder(r_in, h, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
    return _solid(bp.part)


def _group(shapes: Sequence[Solid | Compound]) -> Compound:
    return Compound(children=list(shapes))


@dataclass(frozen=True)
class RenderProp:
    name: str
    solid: Solid | Compound
    material: str
    decal: str | None = None                 # Key in decals.PAINTERS
    view: tuple[str, str] = ("+X", "-Y")     # Device axes pointing right / down on the decal canvas
    kit_only: bool = False                   # Shown in the flat lays only (e.g. the loose battery lead)


# ---------------------------------------------------------------------------------------------
# Teensy 4.1
# ---------------------------------------------------------------------------------------------

class _TeensyFrame:
    """Photo coordinates (u along the board from the USB edge, v across from the 5V/G/3V row edge)
    to the camera frame. The component side faces -Z (the back wall)."""

    def __init__(self, p: EnclosureParams) -> None:
        self.cx = p.teensy_center_x
        self.z_top = TEENSY_PCB_Z_TOP
        self.z_bot = TEENSY_PCB_Z_TOP - TEENSY_T

    def xy(self, u: float, v: float) -> tuple[float, float]:
        return self.cx + TEENSY_W / 2.0 - v, TEENSY_Y0 + u

    def part(self, u: float, v: float, su: float, sv: float, h: float, lift: float = 0.0) -> Solid:
        """A block of su (along the board) x sv (across) hanging h below the component side."""
        x, y = self.xy(u, v)
        return _box(x, y, self.z_bot - lift - h, sv, su, h)

    def rounded(self, u: float, v: float, su: float, sv: float, h: float, r: float, lift: float = 0.0) -> Solid:
        x, y = self.xy(u, v)
        return _rounded_slab(x, y, self.z_bot - lift - h, sv, su, h, r)


TEENSY_ROW_V = (1.27, 16.51)
TEENSY_INNER_U = 1.32 + 18 * 2.54             # Column of 5 pads beside the microSD socket (Y=47.19)
TEENSY_INNER_V = tuple(1.27 + 2.54 * k for k in range(1, 6))  # VBAT is the last, next to the J5 row
TEENSY_ETH_PADS = [(u, v) for u in (14.3, 16.3) for v in (3.7, 5.7, 7.7)]
TEENSY_USBH_PADS = [(7.62 + 2.54 * k, 13.5) for k in range(5)]


def _teensy_pin_uvs() -> list[tuple[float, float]]:
    return [(1.32 + 2.54 * k, v) for v in TEENSY_ROW_V for k in range(24)]


def _micro_usb(t: _TeensyFrame) -> tuple[Solid, Solid]:
    """Micro-B receptacle: chamfered shell overhanging the board edge by 0.65 mm, tongue inside."""
    x, _ = t.xy(0.0, 8.89)
    y0, length = TEENSY_Y0 - 0.65, 4.95
    plane = Plane(origin=(x, y0, t.z_bot), x_dir=(1, 0, 0), z_dir=(0, 1, 0))  # local y = -Z
    shell_pts = [(-3.75, 2.5), (3.75, 2.5), (3.75, 0.7), (3.05, 0.0), (-3.05, 0.0), (-3.75, 0.7)]
    mouth_pts = [(-3.4, 2.2), (3.4, 2.2), (3.4, 0.95), (2.85, 0.35), (-2.85, 0.35), (-3.4, 0.95)]
    with BuildPart() as bp:
        with BuildSketch(plane):
            Polygon(*shell_pts, align=None)
        extrude(amount=length)
        with BuildSketch(plane):
            Polygon(*mouth_pts, align=None)
        extrude(amount=3.6, mode=Mode.SUBTRACT)
    tongue = _box(x, y0 + 0.4 + 1.6, t.z_bot - 1.55, 3.8, 3.2, 0.55)
    return _solid(bp.part), tongue


def _sd_socket(t: _TeensyFrame) -> tuple[Solid, Solid]:
    """Push-push microSD socket at the far end with a card in it (2.5 mm proud of the board edge)."""
    u_end = TEENSY_L - 0.06
    su, sv, h = 12.0, 12.6, 1.8
    x, y = t.xy(u_end - su / 2.0, 8.89)
    with BuildPart() as bp:
        with Locations((x, y, t.z_bot - h)):
            Box(sv, su, h, align=(Align.CENTER, Align.CENTER, Align.MIN))
        # Shallow pressed panel and two spring-tab windows on the outer face
        with Locations((x, y + 0.6, t.z_bot - h)):
            Box(sv - 2.2, su - 4.5, 0.12, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        with Locations((x - 3.2, y - su / 2.0 + 1.4, t.z_bot - h), (x + 3.2, y - su / 2.0 + 1.4, t.z_bot - h)):
            Box(1.2, 1.4, 0.5, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        # Card slot at the board end
        with Locations((x, y + su / 2.0, t.z_bot - 1.45)):
            Box(11.3, 3.6, 0.95, align=(Align.CENTER, Align.MAX, Align.MIN), mode=Mode.SUBTRACT)
    cx, _ = t.xy(0, 8.89)
    card = _rounded_slab(cx, TEENSY_Y0 + u_end + 2.5 - 3.0, t.z_bot - 1.30, 11.0, 6.0, 0.8, 0.6)
    return _solid(bp.part), card


def _teensy_header(t: _TeensyFrame, uvs: list[tuple[float, float]], along_u: bool) -> tuple[list[Solid], list[Solid]]:
    """Segmented black header plastic on the +Z side and the long gold pins that go into the sockets."""
    plastic, pins = [], []
    for u, v in uvs:
        x, y = t.xy(u, v)
        w, h = (2.5, 2.42) if along_u else (2.42, 2.5)
        plastic.append(_box(x, y, t.z_top, w, h, HEADER_H))
        pins.append(_box(x, y, t.z_top + HEADER_H, PIN_SQ, PIN_SQ, PIN_LONG - 0.3))
        pins.append(_solid(Location((x, y, t.z_top + HEADER_H + PIN_LONG - 0.3))
                           * Cone(PIN_SQ / 2.0, 0.18, 0.3, align=(Align.CENTER, Align.CENTER, Align.MIN))))
    return plastic, pins


def teensy41(p: EnclosureParams) -> list[RenderProp]:
    """Teensy 4.1 with pins, as in PJRC's photos, hanging component side down from J5/J6."""
    t = _TeensyFrame(p)
    pcb_x, pcb_y = t.xy(TEENSY_L / 2.0, TEENSY_W / 2.0)
    with BuildPart() as pcb:
        with BuildSketch(Plane.XY.offset(t.z_bot)):
            with Locations((pcb_x, pcb_y)):
                RectangleRounded(TEENSY_W, TEENSY_L, 0.8)
            with Locations(*[t.xy(u, v) for u, v in TEENSY_USBH_PADS + TEENSY_ETH_PADS]):
                Circle(0.5, mode=Mode.SUBTRACT)
        extrude(amount=TEENSY_T)

    usb_shell, usb_tongue = _micro_usb(t)
    sd_shell, sd_card = _sd_socket(t)

    # (u, v, su, sv, h) from the photo; markings on the larger chips
    mcu = t.part(29.3, 8.8, 12.0, 12.0, 1.2)
    flash = t.part(41.5, 13.1, 4.0, 4.0, 0.75)
    boot = t.part(9.5, 5.6, 3.5, 3.5, 0.85)
    phy = t.part(19.6, 5.6, 3.5, 3.5, 0.85)
    small_ics = [
        t.part(8.5, 10.0, 2.9, 1.3, 1.0),    # SOT-23 regulator
        t.part(12.8, 10.4, 1.6, 1.6, 0.6),
        t.part(16.9, 10.4, 2.9, 1.6, 1.0),   # SOT-23-6
        t.part(45.1, 13.5, 1.8, 1.8, 0.6),
        t.part(42.0, 3.8, 1.0, 0.5, 0.35), t.part(44.2, 3.8, 1.0, 0.5, 0.35),  # 0402 resistors
    ]
    metal = [
        *[t.part(41.5 + s * 2.25, 13.1 + dv, 0.5, 0.4, 0.2) for s in (-1, 1) for dv in (-1.905, -0.635, 0.635, 1.905)],
        *[t.part(8.5 + du, 10.0 + dv, 0.4, 0.6, 0.25) for du, dv in ((-0.95, -0.95), (0.95, -0.95), (0.0, 0.95))],
        *[t.part(16.9 + du, 10.4 + s * 1.1, 0.4, 0.6, 0.25) for du in (-0.95, 0.0, 0.95) for s in (-1, 1)],
        t.part(38.1, 5.6, 2.2, 2.9, 0.55, lift=0.25),                                      # 24 MHz crystal lid
        t.part(21.1, 11.7 - 1.5, 2.06, 0.6, 1.32), t.part(21.1, 11.7 + 1.5, 2.06, 0.6, 1.32),  # Bulk cap terminals
        *[t.part(37.1 + s * 0.6, v, 0.35, 0.8, 0.8) for v in (9.1, 10.9) for s in (-1, 1)],  # 0603 cap ends
    ]
    tan = [
        t.part(21.1, 11.7, 2.0, 3.3, 1.3),          # Bulk ceramic capacitor
        t.part(38.1, 5.6, 2.5, 3.2, 0.25),          # Crystal base
        t.part(43.0, 5.6, 3.2, 1.7, 0.9),           # Fuse
        t.part(37.1, 9.1, 0.9, 0.8, 0.8), t.part(37.1, 10.9, 0.9, 0.8, 0.8),
    ]
    inductor = t.part(37.0, 13.4, 2.2, 2.8, 1.0)
    led = t.part(39.4, 9.1, 0.8, 1.6, 0.55)
    led_usb = t.part(5.5, 5.2, 0.8, 1.6, 0.55)
    # Pushbutton: dark body, steel frame, cream actuator
    btn_body = t.part(43.15, 8.95, 3.9, 3.5, 0.55)
    btn_frame = t.part(43.15, 8.95, 3.9, 3.5, 0.12, lift=0.55)
    btn_cap = t.rounded(43.15, 8.95, 2.7, 2.0, 0.75, 0.8, lift=0.6)

    pads, solder = [], []
    for u, v in _teensy_pin_uvs():
        x, y = t.xy(u, v)
        pads.append(_annulus(x, y, t.z_bot - 0.04, 0.82, 0.5, 0.04))
        solder.append(_solid(Location((x, y, t.z_bot - 0.5)) * Cone(0.36, 0.78, 0.5, align=(Align.CENTER, Align.CENTER, Align.MIN))))
        solder.append(_box(x, y, t.z_bot - SOLDER_STUB, PIN_SQ, PIN_SQ, SOLDER_STUB - 0.3))
    for u, v in [*TEENSY_USBH_PADS, *[(TEENSY_INNER_U, v) for v in TEENSY_INNER_V]]:
        x, y = t.xy(u, v)
        pads.append(_annulus(x, y, t.z_bot - 0.04, 0.82, 0.5, 0.04))
    for u, v in TEENSY_ETH_PADS:
        x, y = t.xy(u, v)
        pads.append(_annulus(x, y, t.z_bot - 0.04, 0.70, 0.5, 0.04))

    rows = [(1.32 + 2.54 * k, v) for v in TEENSY_ROW_V for k in range(24)]
    plastic, pins = _teensy_header(t, rows, along_u=True)

    view = ("+Y", "-X")  # Component side seen from the back wall, USB at the left as in PJRC's photo
    return [
        RenderProp("teensy_pcb", _solid(pcb.part), "pcb_green", "teensy_component_side", view),
        RenderProp("teensy_mcu", mcu, "ic_black", "chip_mcu", view),
        RenderProp("teensy_flash", flash, "ic_black", "chip_flash", view),
        RenderProp("teensy_boot", boot, "ic_black", "chip_bootloader", view),
        RenderProp("teensy_phy", phy, "ic_black", "chip_phy", view),
        RenderProp("teensy_ics", _group(small_ics), "ic_black"),
        RenderProp("teensy_metal", _group([*metal, btn_frame]), "steel"),
        RenderProp("teensy_shells", _group([usb_shell, sd_shell]), "tin_shell"),
        RenderProp("teensy_usb_tongue", usb_tongue, "plastic_black"),
        RenderProp("teensy_tan", _group(tan), "ceramic_tan"),
        RenderProp("teensy_inductor", inductor, "inductor_grey"),
        RenderProp("teensy_leds", _group([led, led_usb]), "led_white"),
        RenderProp("teensy_button", btn_body, "plastic_black"),
        RenderProp("teensy_button_cap", btn_cap, "button_cream"),
        RenderProp("teensy_sdcard", sd_card, "sd_card"),
        RenderProp("teensy_pads", _group(pads), "gold"),
        RenderProp("teensy_solder", _group(solder), "solder"),
        RenderProp("teensy_headers", _group(plastic), "plastic_black"),
        RenderProp("teensy_pins", _group(pins), "gold"),
    ]


# ---------------------------------------------------------------------------------------------
# FLIR Lepton 3.5 on the Lepton carrier
# ---------------------------------------------------------------------------------------------

# Lepton face features (mm from the lens axis, x right / y up as seen looking at the lens), from the
# GroupGets top view: (dx, dy, diameter)
LEPTON_BOSSES = [(0.8, 4.5, 1.7), (-1.4, -3.2, 2.0), (4.0, -1.2, 1.6)]
LEPTON_HOLES = [(-1.2, 4.4, 0.6), (-2.6, 1.9, 0.7), (-2.8, -0.2, 0.7), (3.9, 4.6, 0.8), (1.9, -2.2, 0.6),
                (0.9, -3.4, 0.5), (2.4, -3.4, 0.9)]


def _lepton(p: EnclosureParams, z_bo_bot: float) -> list[RenderProp]:
    lx, ly = p.lepton_x, p.lepton_y
    z_front = z_bo_bot - p.lepton_socketed_h   # Lens bezel face, the outermost point
    z_face = z_front + 0.30                    # Shutter face around the bezel
    z_housing_back = z_front + 3.15
    a = 10.5 / 2.0

    def at(dx: float, dy: float) -> tuple[float, float]:
        return lx + dx, ly - dy  # Seen from -Z (looking at the lens) device X runs right, device Y runs down

    with BuildPart() as housing:
        with Locations((lx, ly, z_face)):
            Box(10.5, 10.5, z_housing_back - z_face, align=(Align.CENTER, Align.CENTER, Align.MIN))
        # Raised lens bezel and its bore
        with BuildSketch(Plane.XY.offset(z_front)):
            with Locations((lx, ly)):
                RectangleRounded(3.0, 3.0, 0.7)
        extrude(amount=0.30)
        with Locations((lx, ly, z_front)):
            Cylinder(1.05, 0.44, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        # Screw bosses (raised rings) and small holes in the face
        for dx, dy, d in LEPTON_BOSSES:
            x, y = at(dx, dy)
            with Locations((x, y, z_face - 0.12)):
                Cylinder(d / 2.0, 0.12, align=(Align.CENTER, Align.CENTER, Align.MIN))
            with Locations((x, y, z_face - 0.12)):
                Cylinder(d / 4.0, 0.42, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        for dx, dy, d in LEPTON_HOLES:
            with Locations((*at(dx, dy), z_face)):
                Cylinder(d / 2.0, 0.30, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        # Stepped corner and the recess with the two shutter contacts
        with BuildSketch(Plane.XY.offset(z_face)):
            Polygon(at(a + 0.01, -a - 0.01), at(a - 3.0, -a - 0.01), at(a + 0.01, -a + 3.0), align=None)
        extrude(amount=0.35, mode=Mode.SUBTRACT)
        with Locations((*at(-4.55, -2.6), z_face)):
            Box(1.3, 2.9, 0.25, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
    contacts = _group([_box(*at(-4.6, dy), z_face + 0.20, 0.5, 0.6, 0.05) for dy in (-1.8, -3.4)])
    lens = _cyl(lx, ly, z_front + 0.38, 1.02, 0.06)

    # Camera core between the shutter housing and the socket, and the socket with its latches
    core = _box(lx, ly, z_housing_back, 8.5, 8.5, z_bo_bot - 2.0 - z_housing_back)
    with BuildPart() as socket:
        with Locations((lx, ly, z_bo_bot - 2.0)):
            Box(11.6, 11.6, 2.0, align=(Align.CENTER, Align.CENTER, Align.MIN))
            Box(9.0, 9.0, 2.0, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
    latches = []
    for dx, sy in ((0.0, 1), (-2.6, -1), (2.6, -1)):
        x, y = at(dx, sy * (a + 0.35))
        latches.append(_box(x, y, z_face + 0.6, 1.6, 0.6, z_bo_bot - (z_face + 0.6)))
    return [
        RenderProp("lepton_housing", _solid(housing.part), "plastic_black"),
        RenderProp("lepton_contacts", contacts, "gold"),
        RenderProp("lepton_lens", lens, "lens_ar"),
        RenderProp("lepton_core", core, "ic_black"),
        RenderProp("lepton_socket", _group([_solid(socket.part), *latches]), "plastic_black"),
    ]


J2_PIN_X0, J2_PIN_ROWS_Y = 31.40, (21.00, 23.54)  # J2 pad 1 and the two rows (KiCad pads)


def lepton_on_breakout(p: EnclosureParams) -> list[RenderProp]:
    """FLIR Breakout V2.0 (29.5 x 29.0 mm) on four 12 mm sleeves + 2x 0.3 mm washers, Lepton facing -Z.

    The breakout's right-angle 2x10 header (Harwin M20-9951045) points toward J2; a right-angle 2x10
    female adapter (Sullins PPPC102LJBN-RC) takes it and plugs into J2.
    """
    z_top = p.pcb_z_back - p.lepton_sleeve_len - p.lepton_shim_t
    z_bot = z_top - p.breakout_t
    bx, by = 0.5 * (p.breakout_x0 + p.breakout_x1), 0.5 * (p.breakout_y0 + p.breakout_y1)
    with BuildPart() as breakout:
        with BuildSketch(Plane.XY.offset(z_bot)):
            with Locations((bx, by)):
                RectangleRounded(p.breakout_x1 - p.breakout_x0, p.breakout_y1 - p.breakout_y0, 1.0)
        extrude(amount=p.breakout_t)
        with Locations(*[(x, y, z_bot) for x, y in p.lepton_breakout_holes]):
            Cylinder(1.1, p.breakout_t, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

    # Right-angle 2x10 male header (Harwin M20-9951045) on the +Z side, pins running -Y into the adapter
    xs = [J2_PIN_X0 + 2.54 * k for k in range(10)]
    adapter_back_y = J2_PIN_ROWS_Y[1] + 1.60    # 25.14 mm (+0.28 mm clear of J2 housing at 24.86)
    adapter_face_y = adapter_back_y + 8.50      # 33.64 mm (tucks 1.64 mm under breakout edge against Harwin)
    header_y0, header_y1 = adapter_face_y, adapter_face_y + 2.54
    plastic = [_box(x, (header_y0 + header_y1) / 2.0, z_top, 2.42, header_y1 - header_y0, 5.08) for x in xs]
    pins, joints = [], []
    for x in xs:
        for zc, y_hole in ((z_top + 1.27, 37.6), (z_top + 3.81, 40.14)):
            pins.append(_box(x, (adapter_face_y - 2.0 + header_y0) / 2.0, zc - PIN_SQ / 2.0, PIN_SQ, header_y0 - adapter_face_y + 2.0, PIN_SQ))
            joints.append(_solid(Location((x, y_hole, z_bot - 0.45)) * Cone(0.34, 0.75, 0.45, align=(Align.CENTER, Align.CENTER, Align.MIN))))
            joints.append(_box(x, y_hole, z_bot - 1.1, PIN_SQ, PIN_SQ, 0.7))
    # Sullins PPPC102LJBN-RC right-angle female header: 8.5 mm body (Y 25.14..33.64), bent tails into J2
    with BuildPart() as adapter:
        with Locations((0.5 * (xs[0] + xs[-1]), 0.5 * (adapter_back_y + adapter_face_y), z_top)):
            Box(25.88, 8.50, 5.08, align=(Align.CENTER, Align.CENTER, Align.MIN))
    adapter_legs = []
    for x in xs:
        for y_leg, z_bend in ((J2_PIN_ROWS_Y[1], z_top + 3.81), (J2_PIN_ROWS_Y[0], z_top + 1.27)):
            adapter_legs.append(_box(x, 0.5 * (y_leg + adapter_back_y), z_bend - 0.2, 0.6, adapter_back_y - y_leg + 0.4, 0.4))
            adapter_legs.append(_box(x, y_leg, z_bend, 0.6, 0.4, (z_top + 5.08 + 3.15) - z_bend))
    # Jumpers J5-J9 and SMD regulators on the +Z side of Breakout V2.0
    smd = [
        _box(36.0, 52.0, z_top, 2.9, 1.6, 1.1),
        _box(36.0, 56.0, z_top, 2.9, 1.6, 1.1),
        _box(50.5, 55.0, z_top, 2.0, 2.5, 0.8),
        _box(35.0, 46.0, z_top, 2.5, 5.0, 8.5),  # Jumper J5
        _box(42.5, 42.5, z_top, 5.0, 2.5, 8.5),  # Jumper J8
        _box(48.0, 48.0, z_top, 5.0, 2.5, 8.5),  # Jumper J9
        _box(48.0, 52.0, z_top, 5.0, 2.5, 8.5),  # Jumper J6
    ]

    props = [
        RenderProp("lepton_breakout", _solid(breakout.part), "pcb_green", "breakout_lepton_side", ("-Y", "+X")),
        RenderProp("lepton_header", _group(plastic), "plastic_black"),
        RenderProp("lepton_header_pins", _group(pins), "gold"),
        RenderProp("lepton_joints", _group(joints), "solder"),
        RenderProp("lepton_adapter", _solid(adapter.part), "plastic_black"),
        RenderProp("lepton_adapter_pins", _group(adapter_legs), "gold"),
        RenderProp("lepton_smd", _group(smd), "ic_black"),
        *_lepton(p, z_bot),
    ]
    # 4x M2x18 ISO 1207 screws on Ø2.2/5.0 x 0.3 nylon washers, 4x 2.2/4.0 x 12 sleeves + 2x 0.3 mm shims, 4x M2 nuts
    pcb_top = p.pcb_z_back + p.pcb_t
    for i, (x, y) in enumerate(p.lepton_breakout_holes):
        props += [
            RenderProp(f"lepton_spacer_{i}", _tube(x, y, z_top, p.lepton_sleeve_len, 2.0, 1.1), "nylon_black"),
            RenderProp(f"lepton_shim_{i}", _tube(x, y, p.pcb_z_back - p.lepton_shim_t, p.lepton_shim_t, 2.5, 1.1), "nylon_black"),
            RenderProp(f"lepton_washer_{i}", _tube(x, y, pcb_top, 0.3, 2.5, 1.1), "nylon_black"),
            RenderProp(f"lepton_screw_{i}", _cheese_head_m2(x, y, pcb_top + 0.3, 18.0), "zinc"),
            RenderProp(f"lepton_nut_{i}", _hex_nut_m2(x, y, z_bot - 1.2), "zinc"),
        ]
    return props


def _tube(x: float, y: float, z0: float, length: float, r_out: float, r_in: float) -> Solid:
    with BuildPart() as bp:
        with Locations((x, y, z0)):
            Cylinder(r_out, length, align=(Align.CENTER, Align.CENTER, Align.MIN))
            Cylinder(r_in, length, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
    return _solid(bp.part)


def _cheese_head_m2(x: float, y: float, z_seat: float, length: float) -> Solid:
    """ISO 1207 M2 slotted cheese head (Ø3.8 x 1.3) seated at z_seat, shank pointing -Z."""
    with BuildPart() as bp:
        with Locations((x, y, z_seat)):
            Cylinder(1.9, 1.3, align=(Align.CENTER, Align.CENTER, Align.MIN))
            Cylinder(0.95, length, align=(Align.CENTER, Align.CENTER, Align.MAX))
        with Locations((x, y, z_seat + 1.3)):
            Box(3.9, 0.5, 0.6, align=(Align.CENTER, Align.CENTER, Align.MAX), mode=Mode.SUBTRACT)
    return _solid(bp.part)


def _hex_nut_m2(x: float, y: float, z0: float) -> Solid:
    """ISO 4035 M2 thin hex nut (SW 4, 1.2 mm)."""
    with BuildPart() as bp:
        with BuildSketch(Plane.XY.offset(z0)):
            with Locations((x, y)):
                RegularPolygon(radius=4.0 / 3**0.5, side_count=6)
                Circle(1.0, mode=Mode.SUBTRACT)
        extrude(amount=1.2)
    return _solid(bp.part)


# ---------------------------------------------------------------------------------------------
# Display, LiPo, USB-C
# ---------------------------------------------------------------------------------------------

def display_module(p: EnclosureParams) -> list[RenderProp]:
    """ER-TFTM032-3: black PCB, white backlight frame, black glass with the screen image on top."""
    z_pcb = p.display_pcb_z_back
    z_frame = z_pcb + p.display_pcb_t
    glass_w, glass_h = p.display_win_w + 6.0, p.display_win_h + 6.0
    with BuildPart() as pcb:  # Display PCB with the same four M2.5 mounting holes as the main board
        with Locations((p.display_pcb_center_x, p.display_pcb_center_y, z_pcb)):
            Box(p.display_pcb_w, p.display_pcb_h, p.display_pcb_t, align=(Align.CENTER, Align.CENTER, Align.MIN))
        with Locations(*[(x, y, z_pcb) for x, y in p.mounting_holes]):
            Cylinder(p.mounting_hole_d / 2.0, p.display_pcb_t, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
    # The TFT's orange flex wraps round the glass edge opposite the header
    flex = _box(p.display_win_cx + glass_w / 2.0 + 0.08, p.display_win_cy, z_frame, 0.16, 26.0, 3.25 + 1.2)
    return [
        RenderProp("display_pcb", _solid(pcb.part), "pcb_black", "display_pcb_front", ("+X", "-Y")),
        RenderProp("display_frame", _box(p.display_win_cx, p.display_win_cy, z_frame, glass_w, glass_h, 3.25), "backlight_white"),
        RenderProp("display_glass", _box(p.display_win_cx, p.display_win_cy, z_frame + 3.25, glass_w, glass_h, 1.55), "glass_black"),
        # Thin screen plane exactly on the active area; Blender maps the firmware UI onto it
        RenderProp("display_screen", _box(p.display_win_cx, p.display_win_cy, z_frame + 4.80, p.display_win_w, p.display_win_h, 0.02), "screen"),
        RenderProp("display_flex", flex, "kapton"),
        # Male 2x20 header into J1 (X=2.6, along Y centred at 34.45)
        RenderProp("display_header", _box(2.6, 34.45, z_pcb - 2.5, 5.08, 50.8, 2.5), "plastic_black"),
    ]


# Adafruit 258 (PKCELL LP503562), 34 x 62 x 5 mm: a 56 mm pouch and its protection board in kapton.
# It lies between the two spacer bars, the board end toward the wire gap.
LIPO_CELL_X = 56.0
LIPO_PCM_X = 6.0


def lipo_pouch(p: EnclosureParams) -> list[RenderProp]:
    """Adafruit 258 pouch with seal flanges, its protection board in kapton at the J4 end, and (kit
    only) the red/black lead with a JST-PH plug lying beside it."""
    bat = p.battery
    z0, t = bat.z_start, 5.0
    x0 = bat.center_x - (LIPO_CELL_X + LIPO_PCM_X) / 2.0
    w_y = 33.4  # 34.0 over the seal flanges
    with BuildPart() as pouch:
        with BuildSketch(Plane.XY.offset(z0)):
            with Locations((x0 + LIPO_CELL_X / 2.0, bat.center_y)):
                RectangleRounded(LIPO_CELL_X, w_y, 2.0)
        extrude(amount=t)
        fillet(pouch.edges().filter_by(Axis.Z, reverse=True).group_by(Axis.Z)[-1], radius=1.4)
        fillet(pouch.edges().filter_by(Axis.Z, reverse=True).group_by(Axis.Z)[0], radius=1.0)
    flanges = [_box(x0 + LIPO_CELL_X / 2.0, bat.center_y + s * (w_y / 2.0 + 0.14), z0 + 0.9, LIPO_CELL_X - 3.0, 0.28, 3.0)
               for s in (-1, 1)]
    pcm_x = x0 + LIPO_CELL_X + LIPO_PCM_X / 2.0
    pcm = _rounded_slab(pcm_x, bat.center_y, z0 + 0.35, LIPO_PCM_X, w_y - 1.0, 4.3, 0.8)
    props = [
        RenderProp("lipo", _group([_solid(pouch.part), *flanges]), "lipo_foil", "lipo_label", ("+X", "-Y")),
        RenderProp("lipo_pcm", pcm, "kapton"),
    ]
    # Lead: two wires leave the kapton at the corner by the wire exit (Y=50) and arc round to a
    # JST-PH plug, about 7 cm in all (the Adafruit 258's is 8-10 cm)
    x_exit, r = x0 + LIPO_CELL_X + LIPO_PCM_X, 0.55
    centre = [(-0.5, 50.0), (4.0, 50.4), (12.0, 49.0), (20.0, 44.0), (24.0, 35.0), (24.5, 24.0), (22.0, 14.0), (21.0, 7.0)]
    zs = [z0 + 2.4, z0 + r + 0.3, *[z0 + r] * 5, z0 + 1.7]
    wires = []
    for side, mat in ((1.0, "wire_red"), (-1.0, "wire_black")):
        pts = []
        for i, (u, y) in enumerate(centre):
            (ua, ya), (ub, yb) = centre[max(i - 1, 0)], centre[min(i + 1, len(centre) - 1)]
            n = math.hypot(ub - ua, yb - ya)
            nx, ny = -(yb - ya) / n, (ub - ua) / n  # In-plane normal: the two wires lie side by side
            pts.append((x_exit + u + 0.6 * side * nx, y + 0.6 * side * ny, zs[i]))
        path = Spline(*pts, tangents=[(1, 0, -0.2), (0, -1, 0)])
        section = Plane(origin=path @ 0, z_dir=path % 0) * Circle(r)
        wires.append((sweep(section, path=path), mat))
    end = (x_exit + centre[-1][0], centre[-1][1])
    plug = _box(end[0], end[1] - 3.4, z0, 6.0, 6.8, 3.4)  # JST PHR-2 housing lying on its side
    for solid, mat in wires:
        props.append(RenderProp(f"lipo_lead_{mat}", solid, mat, kit_only=True))
    props.append(RenderProp("lipo_lead_plug", plug, "nylon_white", kit_only=True))
    return props


def build_render_props(p: EnclosureParams) -> list[RenderProp]:
    """Modules the KiCad board doesn't carry (the board's own parts, J3 included, come from KiCad)."""
    return [
        *teensy41(p),
        *lepton_on_breakout(p),
        *display_module(p),
        *lipo_pouch(p),
    ]
