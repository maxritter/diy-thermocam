"""Clearance, mechanism, FOV and export tests for the V4.0 enclosures.

    cd hardware/enclosure/cad && uv run pytest

Every printed part and acrylic plate is checked against datasheet models of the real parts
(`keepout.build_real_parts`), and every moving part is moved through its travel.
"""

from __future__ import annotations

import math
import re
import xml.etree.ElementTree as ET
import zipfile

import pytest
from build123d import Location, Plane, RectangleRounded, Solid, extrude

from thermocam_case.accessories import (
    CAP_BUMP_DY,
    CAP_FINGER_X,
    PCB_TOP_EDGE_Y,
    SW2_TIP_Y,
    TRIPOD_FLOOR_T,
    battery_stack_z,
    laser_bottom_faces,
)
from thermocam_case.export import (
    SHELL_SERVICE_PARTS,
    default_enclosure_root,
    export_all,
    expected_exports,
    service_files,
)
from thermocam_case.hardware import (
    build_shell_screws,
    build_standoff_stack,
    screw_joints,
    teensy_socket_solder_joints,
    tripod_hex_nut,
)
from thermocam_case.keepout import J2_TAILS, J5J6_TAILS, build_keepouts, sw1_actuator, sw2_plunger
from thermocam_case.laser_case import build_laser_enclosure
from thermocam_case.params import V4_PARAMS as P, parse_kicad_pcb_metrics
from thermocam_case.printable_case import (
    PRINT_PARTS,
    _shell_envelope,
    build_printable_enclosure,
    orient_for_printing,
)
from thermocam_case.props import build_render_props

ENCLOSURE_ROOT = default_enclosure_root()
KICAD_PCB_PATH = ENCLOSURE_ROOT.parent / "pcb" / "diy-thermocam-v4.0.kicad_pcb"

# Measured on the KiCad board (pcbnew): nearest exposed pads to the rear standoffs, on B.Cu
SW2_PAD_FROM_H4, SW3_PAD_FROM_H2 = 3.11, 3.18
ISO7046_M25_THEORETICAL_HEAD_D = 5.5
TRIPOD_SCREW_LEN = 4.5  # Common camera-plate screw protrusion


@pytest.fixture(scope="module")
def enc():
    return build_printable_enclosure(P)


@pytest.fixture(scope="module")
def laser():
    return build_laser_enclosure(P)


@pytest.fixture(scope="module")
def k():
    return build_keepouts(P)


def _inter_vol(a: Solid, b: Solid) -> float:
    ba, bb = a.bounding_box(), b.bounding_box()
    if (ba.max.X < bb.min.X or bb.max.X < ba.min.X or ba.max.Y < bb.min.Y or bb.max.Y < ba.min.Y
            or ba.max.Z < bb.min.Z or bb.max.Z < ba.min.Z):
        return 0.0
    inter = a & b
    return 0.0 if inter is None else float(sum(s.volume for s in inter.solids()))


def _move(s: Solid, dx: float, dy: float, dz: float) -> Solid:
    m = Location((dx, dy, dz)) * s
    return m if isinstance(m, Solid) else m.solids()[0]


def _assert_watertight(solid: Solid, label: str, min_vol: float = 40.0) -> None:
    assert isinstance(solid, Solid), f"{label} is not a Solid"
    assert solid.is_valid, f"{label} failed OCCT is_valid"
    assert solid.volume > min_vol, f"{label} volume too small: {solid.volume:.2f} mm^3"


def _assert_clear(pairs: list[tuple[str, Solid, Solid]], tol: float = 1e-3) -> None:
    hits = [(label, v) for label, a, b in pairs if (v := _inter_vol(a, b)) >= tol]
    assert not hits, "collisions: " + ", ".join(f"{label} {v:.3f} mm^3" for label, v in hits)


def _usb_c_boot() -> Solid:
    """Sharp R=1.0 mm rectangular 13.8x7.8 USB-C plug boot entering from below."""
    boot_plane = Plane(origin=(45.80, -4.50, 18.40), x_dir=(1, 0, 0), z_dir=(0, -1, 0))
    boot = extrude(boot_plane * RectangleRounded(13.80, 7.80, 1.00), amount=-4.20)
    return boot if isinstance(boot, Solid) else boot.solids()[0]


def _shell_parts(enc) -> dict[str, Solid]:
    parts = {
        "front_shell": enc.front_shell, "back_shell": enc.back_shell,
        "lens_cap_open": enc.lens_cap_open, "lens_cap_closed": enc.lens_cap_closed,
        "capture_button": enc.capture_button, "slide_switch_cap": enc.slide_switch_cap,
        "battery_cradle": enc.battery_cradle, "battery_floor": enc.battery_floor, "battery_lid": enc.battery_lid,
    }
    return parts


def _laser_parts(enc, laser) -> dict[str, Solid]:
    parts = {f"laser_{n}": s for n, s in laser.plates_3d.items()}
    parts.update({f"laser_{n}": s for n, s in laser.printed.items()})
    parts.update({"battery_cradle": enc.battery_cradle, "battery_floor": enc.battery_floor,
                  "battery_lid": enc.battery_lid})
    return parts


def _printed_parts(enc, laser) -> dict[str, Solid]:
    """Every part a print service makes, once (FR4 boards and acrylic plates excluded)."""
    parts = {n: s for n, s in _shell_parts(enc).items() if n not in ("lens_cap_closed", "battery_floor", "battery_lid")}
    parts.update({f"laser_{n}": s for n, s in laser.printed.items() if n != "display_spacer_top"})
    return parts


def _wall_samples(solid: Solid, min_area: float = 0.2) -> list[tuple[float, tuple[float, float, float]]]:
    """Wall thickness: from points on each face, inward along the normal to the next surface, where
    that surface faces back (within ~37 deg). Chamfer and dovetail edges, whose far face meets the
    ray at an angle, are wedges rather than walls and are skipped."""
    from build123d import Axis

    out = []
    for face in solid.faces():
        if face.area < min_area:
            continue
        for u, v in ((0.5, 0.5), (0.25, 0.5), (0.75, 0.5), (0.5, 0.25), (0.5, 0.75)):
            pt, n = face.position_at(u, v), face.normal_at(u, v)
            if not solid.is_inside(pt - n * 0.01) or solid.is_inside(pt + n * 0.01):
                continue  # Sample point off a trimmed face
            hits = sorted((((h[0] - pt).dot(-n), h[1]) for h in solid.find_intersection_points(Axis(pt, -n))),
                          key=lambda r: r[0])
            hits = [(t, hn) for t, hn in hits if t > 1e-3]
            if hits and hits[0][1].dot(n) < -0.8:
                out.append((hits[0][0], (round(pt.X, 2), round(pt.Y, 2), round(pt.Z, 2))))
    return out


# --- Board data -----------------------------------------------------------------------------------

def test_params_match_real_kicad_pcb() -> None:
    metrics = parse_kicad_pcb_metrics(KICAD_PCB_PATH)
    assert abs(float(metrics["pcb_w"]) - P.pcb_w) <= 0.01
    assert abs(float(metrics["pcb_h"]) - P.pcb_h) <= 0.01
    assert sorted(P.mounting_holes) == metrics["mounting_holes"]
    assert sorted(P.lepton_breakout_holes) == metrics["lepton_holes"]
    usb_x, _ = metrics["usb_pos"]  # type: ignore[misc]
    _, sw_y = metrics["switch_pos"]  # type: ignore[misc]
    btn_x, _ = metrics["button_pos"]  # type: ignore[misc]
    prog_x, prog_y = metrics["prog_pos"]  # type: ignore[misc]
    led_x, led_y = metrics["chg_led_pos"]  # type: ignore[misc]
    assert abs(usb_x - P.usb.center_x) <= 0.01
    assert abs(sw_y - P.switch_y) <= 0.01
    assert abs(btn_x - P.button_x) <= 0.01
    assert (prog_x, prog_y) == (P.teensy_prog_x, P.teensy_prog_y)
    assert (led_x, led_y) == P.back_face_led_holes[0]
    # Board stack: 20 standoff (seated 0.5 deep) + 0.5 washer, PCB, 11 standoff, display PCB, 5 boss
    assert P.standoff_seat_z + 20.0 + P.rear_washer_t == pytest.approx(P.pcb_z_back)
    assert P.pcb_z_back + P.pcb_t + 11.0 == pytest.approx(P.display_pcb_z_back)
    assert P.display_pcb_z_back + P.display_pcb_t + 5.0 == pytest.approx(P.internal_depth)
    assert P.standoff_seat_z + P.laser_internal_depth == pytest.approx(P.internal_depth)


def test_pin_tails_match_kicad_pads() -> None:
    """The battery cradle steps round the J2/J5/J6 tails: their model must sit on the real pads."""
    text = KICAD_PCB_PATH.read_text()
    pads: set[tuple[float, float]] = set()
    for ref in ("J2", "J5", "J6"):
        m = re.search(r'\(footprint "[^"]+"\s*\(layer "B\.Cu"\)(?:(?!\n\t\(footprint ).)*?\(property "Reference" "'
                      + ref + r'"(?:(?!\n\t\(footprint ).)*', text, re.S)
        assert m, f"{ref} not found on B.Cu"
        block = m.group(0)
        fx, fy = (float(v) for v in re.search(r"\n\t\t\(at ([-\d.]+) ([-\d.]+)", block).groups())  # type: ignore[union-attr]
        for px, py in re.findall(r'\(pad "\d+" thru_hole \w+\s*\(at ([-\d.]+) ([-\d.]+)', block):
            pads.add((round(fx + float(px), 2), round(-(fy + float(py)), 2)))
    assert pads == {(round(x, 2), round(y, 2)) for x, y in (*J2_TAILS, *J5J6_TAILS)}


# --- Printed parts ---------------------------------------------------------------------------------

def test_printable_solids_valid_and_watertight(enc, laser) -> None:
    for name, s in _shell_parts(enc).items():
        _assert_watertight(s, name, min_vol=15.0)
    for name, s in laser.printed.items():
        _assert_watertight(s, name, min_vol=200.0)


def test_printable_minimum_wall_thickness(enc) -> None:
    e = _shell_envelope(P)
    assert P.shell_wall >= 2.0 and P.shell_face_t >= 2.0
    assert (e["z_back_inner"] - e["z_track_floor"]) >= 2.0
    for label, solid, probe_z in [("back_shell_left", enc.back_shell, 10.0), ("front_shell_left", enc.front_shell, 32.0)]:
        from build123d import Box

        probe = Location((e["ox0"] - 1.0, 35.0, probe_z)) * Box(10.0, 2.0, 2.0)
        bb = (solid & probe).bounding_box()
        assert bb.size.X >= 2.0 - 1e-3, f"{label} wall {bb.size.X:.2f} mm < 2.0 mm"


MJF_MIN_WALL = 1.0  # JLC3DP MJF design guideline: minimum wall


def test_every_printed_wall_meets_mjf_minimum(enc, laser) -> None:
    """Every printed part has walls of at least 1.0 mm, measured inward from points on each face."""
    thin = {}
    for name, solid in _printed_parts(enc, laser).items():
        samples = [(t, pt) for t, pt in _wall_samples(solid) if t < MJF_MIN_WALL - 1e-3]
        if samples:
            thin[name] = sorted(samples)[:3]
    assert not thin, "walls under 1.0 mm: " + "; ".join(
        f"{n}: " + ", ".join(f"{t:.2f} at {pt}" for t, pt in s) for n, s in thin.items())


def test_printed_parts_clear_every_real_part(enc, laser, k) -> None:
    """No printed part or plate touches a datasheet part, in either enclosure."""
    pairs = []
    for parts in (_shell_parts(enc), _laser_parts(enc, laser)):
        for pname, part in parts.items():
            for rname, real in k.real.items():
                pairs.append((f"{pname} x {rname}", part, real))
            pairs.append((f"{pname} x pcb", part, k.pcb))
    _assert_clear(pairs)


def test_printed_parts_clear_each_other(enc, laser) -> None:
    shell = _shell_parts(enc)
    del shell["lens_cap_closed"]  # Same part as lens_cap_open, other position
    lp = _laser_parts(enc, laser)
    pairs = []
    for group in (shell, lp):
        names = list(group)
        for i, a in enumerate(names):
            for b in names[i + 1:]:
                pairs.append((f"{a} x {b}", group[a], group[b]))
    _assert_clear(pairs)


def test_hardware_clears_everything(enc, laser, k) -> None:
    """Standoffs, washers, screws and the tripod nuts clear the printed parts, plates and real parts."""
    e = _shell_envelope(P)
    stack = build_standoff_stack(P)
    screws = build_shell_screws(P, e["z_back_outer"], e["z_front_outer"])
    joints = teensy_socket_solder_joints(P)
    pairs = []
    for i, s in enumerate(stack.all):
        for pname, part in {**_shell_parts(enc), **_laser_parts(enc, laser)}.items():
            pairs.append((f"stack{i} x {pname}", s, part))
        for rname, real in k.real.items():
            pairs.append((f"stack{i} x {rname}", s, real))
        if i < len(stack.rear):
            pairs.append((f"rear standoff {i} x pcb", s, k.pcb))
    pairs.append(("battery_cradle x J5/J6 joints", enc.battery_cradle, joints))
    for i, sc in enumerate(screws):
        pairs += [(f"screw{i} x pcb", sc, k.pcb), (f"screw{i} x display_pcb", sc, k.display_pcb)]
        pairs.append((f"screw{i} x display_module", sc, k.real["display_module"]))
    _, y_out = laser_bottom_faces(P)
    shell_nut = tripod_hex_nut(P.tripod_x, P.shell_tripod_z, e["oy0"] + P.shell_tripod_floor_t)
    laser_nut = tripod_hex_nut(P.laser_tripod_x, P.laser_tripod_z, y_out + TRIPOD_FLOOR_T, corners_along_x=True)
    pairs.append(("shell tripod nut x back_shell", shell_nut, enc.back_shell))
    # The laser nut touches its holder only at the two 1.0 mm crush ribs (test_tripod_threads_engage)
    squeeze = laser_nut & laser.printed["tripod_mount"]
    for s in (squeeze.solids() if squeeze is not None else []):
        bb = s.bounding_box()
        assert abs(0.5 * (bb.min.X + bb.max.X) - P.laser_tripod_x) < 0.05 and bb.size.X <= 1.0 + 1e-3, bb
    for pname, plate in laser.plates_3d.items():
        pairs.append((f"laser tripod nut x {pname}", laser_nut, plate))
    _assert_clear(pairs)
    # Rear standoff corners end 0.5 mm below the PCB; the washer under them stays off the pads
    for s in stack.rear:
        assert s.bounding_box().max.Z <= P.pcb_z_back - P.rear_washer_t + 1e-6
    assert P.rear_washer_d / 2.0 < min(SW2_PAD_FROM_H4, SW3_PAD_FROM_H2)


def test_render_props_stay_inside_their_space(enc, laser) -> None:
    stack = build_standoff_stack(P)
    solid_parts = {**_shell_parts(enc), **_laser_parts(enc, laser),
                   **{f"standoff_{i}": s for i, s in enumerate(stack.all)}}
    props = [prop for prop in build_render_props(P) if not prop.kit_only]  # The loose battery lead is a flat-lay prop
    pairs = [(f"{prop.name} x {n}", prop.solid, part) for prop in props for n, part in solid_parts.items()]
    _assert_clear(pairs, tol=1e-2)


# --- Fasteners -------------------------------------------------------------------------------------

def test_screw_engagement_fits_standoff_threads() -> None:
    """Every screw and male stud bites >= 2.0 mm and stays >= 0.5 mm short of the thread bottom."""
    for j in screw_joints(P):
        assert 2.0 <= j.engagement <= j.thread_depth - 0.5, (
            f"{j.name}: engagement {j.engagement:.2f} mm vs thread depth {j.thread_depth:.2f} mm"
        )
    assert ISO7046_M25_THEORETICAL_HEAD_D <= P.countersink_head_d  # Head sits flush or below


def test_tripod_threads_engage(enc, laser) -> None:
    e = _shell_envelope(P)
    assert TRIPOD_SCREW_LEN - P.shell_tripod_floor_t >= 3.0
    y_in, y_out = laser_bottom_faces(P)
    mount = laser.printed["tripod_mount"]
    assert y_out - 0.01 <= mount.bounding_box().min.Y <= y_out + 0.1  # Flush with the outer face
    assert TRIPOD_SCREW_LEN - (mount.bounding_box().min.Y - y_out) - TRIPOD_FLOOR_T >= 3.0
    # The crush ribs squeeze the laser nut (0.3 mm each) and the holder can't lift (fin under the PCB edge)
    laser_nut = tripod_hex_nut(P.laser_tripod_x, P.laser_tripod_z, y_out + TRIPOD_FLOOR_T, corners_along_x=True)
    assert _inter_vol(laser_nut, mount) > 1.0
    k_pcb = build_keepouts(P).pcb
    assert _inter_vol(_move(mount, 0, 0.15, 0), k_pcb) > 0.05
    assert _inter_vol(_move(mount, 0, -0.1, 0), laser.plates_3d["bottom"]) > 0.05
    # Long tripod screws in the shell stop in the nut cavity, below the blind roof
    k = build_keepouts(P)
    _assert_clear([
        ("tripod stud x back_shell", k.tripod_stud, enc.back_shell),
        ("tripod stud x pcb", k.tripod_stud, k.pcb),
        ("tripod stud x usb", k.tripod_stud, k.real["J3_usb_c"]),
    ])
    assert e["oy0"] < 0


# --- Mechanisms -----------------------------------------------------------------------------------

@pytest.mark.parametrize("which", ["shell", "laser"])
def test_capture_button_actuates_and_stops_on_pcb_edge(enc, laser, k, which) -> None:
    if which == "shell":
        button, guides = enc.capture_button, [enc.back_shell, enc.front_shell]
    else:
        button, guides = laser.plates_3d["capture_key"], [laser.plates_3d["top"]]
    rest_gap = button.bounding_box().min.Y - SW2_TIP_Y
    assert rest_gap == pytest.approx(P.button_rest_gap, abs=1e-3)
    if which == "shell":  # The flange lands on the PCB edge
        travel = P.button_rest_gap + P.button_stop_stroke
    else:  # The acrylic stem itself lands on the PCB edge
        travel = SW2_TIP_Y + P.button_rest_gap - PCB_TOP_EDGE_Y
    stroke = travel - rest_gap
    # Pretravel is 0.1..0.4 mm; SW2 sits within +-0.1 mm of its footprint
    assert 0.5 <= stroke <= 0.7, f"SW2 stroke at the stop {stroke:.2f} mm"
    pressed = _move(button, 0, -travel, 0)
    over = _move(button, 0, -(travel + 0.05), 0)
    _assert_clear([
        ("rest x SW2 plunger", button, k.real["SW2_plunger"]),
        ("pressed x pcb", pressed, k.pcb),
        ("pressed x SW2 body", pressed, k.real["SW2_body"]),
        *[(f"pressed x guide{i}", pressed, g) for i, g in enumerate(guides)],
        *[(f"rest x guide{i}", button, g) for i, g in enumerate(guides)],
    ])
    assert _inter_vol(over, k.pcb) > 0.05, "the button must stop on the PCB edge"
    assert _inter_vol(pressed, sw2_plunger()) > 0.05, "the button must push the SW2 plunger"
    assert _inter_vol(_move(button, 0, 0.6, 0), guides[0]) > 0.05, "the button must be retained outward"
    if which == "shell":
        assert _inter_vol(_move(button, 0, 0, 3.0), enc.back_shell) < 1e-3, "drops in from +Z"


@pytest.mark.parametrize("which", ["shell", "laser"])
def test_slide_switch_cap_drives_sw1(enc, laser, k, which) -> None:
    if which == "shell":
        cap, housing = enc.slide_switch_cap, enc.back_shell
    else:
        cap, housing = laser.plates_3d["switch_key"], laser.plates_3d["right"]
    half = P.switch_travel / 2.0
    for dy in (-1.0, 1.0):
        moved = _move(cap, 0, dy * (half + 0.20), 0)
        _assert_clear([
            ("cap x SW1 body", moved, k.real["SW1_body"]),
            ("cap x housing", moved, housing),
            ("cap x pcb", moved, k.pcb),
            ("cap x actuator at its end", moved, sw1_actuator(dy * half)),
        ])
        assert _inter_vol(moved, sw1_actuator(dy * (half - 0.1))) > 0.01, "the fork must push the actuator"
        assert _inter_vol(_move(cap, 0, dy * 2.3, 0), housing) > 0.01, "the housing stops the cap"
    # 0.5 mm from the PCB edge and the SW1 body
    assert _inter_vol(_move(cap, -0.45, 0, 0), k.pcb) < 1e-3
    assert _inter_vol(_move(cap, -0.45, 0, 0), k.real["SW1_body"]) < 1e-3
    if which == "shell":
        # The cap sits in the shell first; the actuator then drops into the open fork from +Z
        from thermocam_case.keepout import box

        sweep = box(89.0, 91.0, 12.7 - 0.75, 12.7 + 0.75, 16.6, 30.0)
        assert _inter_vol(cap, sweep) < 1e-3
        assert _inter_vol(_move(cap, 0, 0, 3.0), enc.back_shell) < 1e-3


def test_lens_cap_detents_hold_open_and_closed(enc) -> None:
    e = _shell_envelope(P)
    r_open = P.shell_lepton_outer_d / 2.0
    # Bumps never cross the Lepton opening between the open and closed positions
    y0, y1 = e["cap_open_y"], e["cap_closed_y"]
    for i in range(41):
        yc = y0 + (y1 - y0) * i / 40.0 + CAP_BUMP_DY
        assert math.hypot(CAP_FINGER_X, yc - P.lepton_y) >= r_open + 1.0
    for cap in (enc.lens_cap_open, enc.lens_cap_closed):
        assert _inter_vol(cap, enc.back_shell) < 1e-3, "bumps rest in their notches"
        for dy in (-1.5, 1.5):
            assert _inter_vol(_move(cap, 0, dy, 0), enc.back_shell) > 0.01, "a detent holds the cap"
    # The closed cap covers the opening; the open cap clears every FOV cone
    assert _inter_vol(enc.lens_cap_closed, build_keepouts(P).lepton_fov_cone) > 50.0


def test_battery_stack_heights(enc, k) -> None:
    """FR4 floor over the pin tails and Lepton screw heads, FR4 lid under the display's back parts."""
    z = battery_stack_z(P)
    lid_top = enc.battery_lid.bounding_box().max.Z
    assert lid_top == pytest.approx(z["lid_top"])
    assert lid_top <= P.display_pcb_z_back - P.display_pcb_smd_keepout_h - 0.15
    head_top = P.pcb_z_back + P.pcb_t + 0.3 + 1.3
    floor_bot = enc.battery_floor.bounding_box().min.Z
    assert floor_bot >= head_top + 0.15 and floor_bot >= P.pcb_z_back + P.pcb_t + 1.6 + 0.15
    assert enc.battery_floor.bounding_box().max.Z == pytest.approx(P.battery.z_start)
    # 0.6 / 0.4 mm are standard FR4 thicknesses at JLCPCB and PCBWay
    assert (P.battery_floor_t, P.battery_lid_t) == (0.6, 0.4)
    heads = [s for n, s in k.real.items() if n.startswith(("lepton_head_", "lepton_washer_"))]
    pairs = [
        ("battery x cradle", k.battery, enc.battery_cradle),
        ("battery x floor", k.battery, enc.battery_floor),
        ("battery x lid", k.battery, enc.battery_lid),
        ("cradle x floor", enc.battery_cradle, enc.battery_floor),
        ("cradle x lid", enc.battery_cradle, enc.battery_lid),
        ("lid x display back parts", enc.battery_lid, k.display_back_smds),
    ]
    for part in ("battery_cradle", "battery_floor"):
        pairs += [(f"{part} x tail {i}", getattr(enc, part), t) for i, t in enumerate(k.pcb_tht_pins)]
        pairs += [(f"{part} x Lepton head {i}", getattr(enc, part), h) for i, h in enumerate(heads)]
    _assert_clear(pairs)
    # The floor rests on the cradle's lips and rib: dropped 0.05 mm, it lands on them
    assert _inter_vol(_move(enc.battery_floor, 0, 0, -0.05), enc.battery_cradle) > 5.0


def test_laser_chute_spacers_and_margins(laser, k) -> None:
    chute = laser.printed["sd_chute"]
    top = laser.plates_3d["top"]
    assert _inter_vol(chute, top) < 1e-3
    assert _inter_vol(_move(chute, 0, 0.2, 0), top) > 0.01, "barbs hold the chute"
    assert _inter_vol(chute, k.real["T41_sd_card"]) < 1e-3, "the card runs through the chute"
    assert chute.bounding_box().min.Y >= 61.11 + 0.5
    for name in ("display_spacer_bottom", "display_spacer_top"):
        bb = laser.printed[name].bounding_box()
        assert bb.min.Z == pytest.approx(P.display_pcb_z_back + P.display_pcb_t)
        assert bb.max.Z == pytest.approx(P.internal_depth)
    # Plates keep >= 0.5 mm from the board edges (display PCB width +-0.3 mm) and the Teensy USB
    assert P.laser_inner_margin_x_neg >= 0.5 and P.laser_inner_margin_y_neg >= 1.0
    assert _inter_vol(_usb_c_boot(), laser.plates_3d["bottom"]) < 1e-4


# --- Optics ---------------------------------------------------------------------------------------

def test_lepton_fov_and_body_clearance(enc, laser, k) -> None:
    """71 deg (3.5) and 119 deg (3.1R) diagonal FOV clear every opening; the Lepton body clears the shell."""
    pairs = []
    for cname, cone in (*k.lepton_fov_cones_all, *k.lepton_31r_fov_cones_all):
        pairs += [
            (f"back_shell x {cname}", enc.back_shell, cone),
            (f"lens_cap_open x {cname}", enc.lens_cap_open, cone),
            (f"laser back x {cname}", laser.plates_3d["back"], cone),
        ]
    thick_back = extrude(laser.plates_2d["back"], amount=-3.20)
    thick_back = thick_back if isinstance(thick_back, Solid) else thick_back.solids()[0]
    thick_back = _move(thick_back, 0, 0, P.standoff_seat_z)
    for cname, cone in (*k.lepton_fov_cones_all, *k.lepton_31r_fov_cones_all):
        pairs.append((f"3.2 mm laser back x {cname}", thick_back, cone))
    _assert_clear(pairs)
    lep = k.real["lepton_socketed"]
    assert lep.bounding_box().min.Z == pytest.approx(-0.74, abs=0.01)
    # Tolerance stack (+-0.3 mm in Z, +-0.5 mm in XY) still clears the shell
    for dx, dy, dz in ((0.5, 0.5, -0.3), (-0.5, -0.5, -0.3), (0.5, -0.5, -0.3), (-0.5, 0.5, -0.3)):
        assert _inter_vol(_move(lep, dx, dy, dz), enc.back_shell) < 1e-3


# --- Laser plates ---------------------------------------------------------------------------------

def test_laser_plates_valid_and_zero_interference(laser, k) -> None:
    assert set(laser.plates_3d.keys()) == {"front", "back", "top", "bottom", "left", "right", "capture_key", "switch_key"}
    for name, solid in laser.plates_3d.items():
        _assert_watertight(solid, f"laser.{name}", min_vol=80.0 if name.endswith("_key") else 3000.0)
    names = list(laser.plates_3d.keys())
    _assert_clear([
        (f"{a} x {b}", laser.plates_3d[a], laser.plates_3d[b])
        for i, a in enumerate(names) for b in names[i + 1:]
    ])


def test_printable_flat_support_free_orientation(enc) -> None:
    parts = orient_for_printing(enc)
    assert set(parts) == set(PRINT_PARTS)
    for name, solid in parts.items():
        _assert_watertight(solid, f"print.{name}", min_vol=15.0)
        assert abs(solid.bounding_box().min.Z) <= 1e-3


def test_exported_artifacts_exist_and_non_empty() -> None:
    expected = expected_exports(ENCLOSURE_ROOT)
    if not all(f.exists() for f in expected):
        export_all(enclosure_root=ENCLOSURE_ROOT)
    for path in expected:
        assert path.exists(), f"Missing exported file: {path}"
        assert path.stat().st_size > 500, f"File too small: {path}"
        if path.suffix == ".step":
            assert "ISO-10303-21" in path.read_text(encoding="utf-8", errors="ignore")[:100]
        elif path.suffix == ".svg":
            svg_txt = path.read_text(encoding="utf-8", errors="ignore")
            assert ET.fromstring(svg_txt).tag.endswith("svg")
            assert 'id="cut"' in svg_txt and 'id="engrave"' in svg_txt
        elif path.suffix == ".dxf":
            txt = path.read_text(encoding="utf-8", errors="ignore")
            assert "ENTITIES" in txt and "EOF" in txt
            if "laser-sheet" in path.name:
                assert "CUT" in txt and "ENGRAVE" in txt
        elif path.suffix == ".3mf":
            assert path.read_bytes()[:4] == b"PK\x03\x04"


def test_service_files_hold_each_part_once_and_named() -> None:
    """JLC3DP takes at most 10 objects per upload: the service plates and zips list every unique part once."""
    files = service_files(ENCLOSURE_ROOT)
    if not all(f.exists() for f in files.values()):
        export_all(enclosure_root=ENCLOSURE_ROOT)
    for key, expected in (("shell_plate", list(SHELL_SERVICE_PARTS)),
                          ("laser_plate", ["battery_cradle", "tripod_mount", "sd_chute",
                                           "display_spacer_1", "display_spacer_2"])):
        with zipfile.ZipFile(files[key]) as zf:
            model = zf.read("3D/3dmodel.model").decode()
        names = re.findall(r'<object [^>]*name="([^"]+)"', model)
        assert names == expected and len(names) <= 10, f"{key}: {names}"
    for key in ("shell_zip", "laser_zip"):
        with zipfile.ZipFile(files[key]) as zf:
            listing = zf.namelist()
        steps = [n for n in listing if n.startswith("step/")]
        stls = [n for n in listing if n.startswith("stl/")]
        assert "ORDER.txt" in listing and len(steps) == len(stls) <= 10, f"{key}: {listing}"


def test_fr4_boards_have_gerbers_with_outline() -> None:
    """Each FR4 board ships as a Gerber zip whose Edge.Cuts layer holds its outline."""
    expected = [f for f in expected_exports(ENCLOSURE_ROOT) if f.name.endswith("-gerbers.zip")]
    assert {f.name for f in expected} == {f"{P.file_prefix}-battery_floor-gerbers.zip",
                                          f"{P.file_prefix}-battery_lid-gerbers.zip"}
    if not all(f.exists() for f in expected):
        export_all(enclosure_root=ENCLOSURE_ROOT)
    for path in expected:
        with zipfile.ZipFile(path) as zf:
            names = zf.namelist()
            edge = [n for n in names if n.endswith("Edge_Cuts.gbr")]
            assert edge and len(zf.read(edge[0])) > 500, f"{path.name}: {names}"


def test_pcb_edge_constants() -> None:
    assert PCB_TOP_EDGE_Y == pytest.approx(P.pcb_h)
