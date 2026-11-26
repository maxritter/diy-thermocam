"""Clearance, mechanism, FOV and export tests for the V4.0 enclosures.

    cd hardware/enclosure/cad && uv run pytest

Every printed part and acrylic plate is checked against datasheet models of the real parts
(`keepout.build_real_parts`), and every moving part is moved through its travel.
"""

from __future__ import annotations

import itertools
import math
import re
import xml.etree.ElementTree as ET
import zipfile

import pytest
from build123d import Location, Plane, RectangleRounded, Solid, extrude

from thermocam_case.accessories import (
    CAP_BUMP_DY,
    CAP_BUMP_H,
    CAP_BUMP_TOP,
    CAP_BUMP_W,
    CAP_FINGER_X,
    CAP_GRIP_H,
    CAP_NOTCH_DEPTH,
    CRADLE_WIRE_GAP_W,
    CRADLE_WIRE_GAP_Y,
    PCB_TOP_EDGE_Y,
    SPACER_BEAM_T,
    SPACER_BUMP_H,
    SPACER_CELL_W,
    SPACER_CELL_W_RANGE,
    SPACER_END_L,
    SPACER_FREE_W,
    SPACER_H,
    SPACER_LEN,
    SPACER_SOLID_W,
    SW2_TIP_Y,
    TRIPOD_FLOOR_T,
    TRIPOD_NUT_AF,
    TRIPOD_RIB_SQUEEZE,
    battery_stack_z,
    build_battery_spacer,
    laser_bottom_faces,
    spacer_squeeze,
)
from thermocam_case.export import (
    LASER_SERVICE_PARTS,
    SHELL_SERVICE_PARTS,
    default_enclosure_root,
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
from thermocam_case.keepout import (
    J2_TAILS,
    J5J6_TAILS,
    build_keepouts,
    j3_leg_tails,
    sw1_actuator,
    sw2_plunger,
)
from thermocam_case.laser_case import build_laser_enclosure
from thermocam_case.params import MJF_MIN_DETAIL, MJF_MIN_WALL, parse_kicad_pcb_metrics
from thermocam_case.params import V4_PARAMS as P
from thermocam_case.printable_case import (
    BRAND_RINGS,
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
    boot_plane = Plane(origin=(P.usb.center_x, -4.50, P.usb.center_z), x_dir=(1, 0, 0), z_dir=(0, -1, 0))
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
    """Every part a print service makes, once, as printed (FR4 boards and acrylic plates excluded)."""
    parts = {n: s for n, s in _shell_parts(enc).items() if n not in ("lens_cap_closed", "battery_floor", "battery_lid")}
    parts["battery_spacer"] = build_battery_spacer()
    parts.update({f"laser_{n}": s for n, s in laser.printed.items() if n != "display_spacer_top"})
    return parts


WALL_GRID = 7  # Samples per face: a 7 x 7 grid, so a face with a hole in its middle is still sampled


def _wall_samples(solid: Solid, gaps: bool = False,
                  min_area: float = 0.05) -> list[tuple[float, tuple[float, float, float]]]:
    """Wall thickness (or, with `gaps`, the width of engraved slots and grooves): from points on each
    face, along the inward (outward) normal to the part's next surface, where that surface faces back
    (within ~37 deg). Chamfer and dovetail edges, whose far face meets the ray at an angle, are wedges
    rather than walls and are skipped."""
    from build123d import Axis

    grid = [((i + 0.5) / WALL_GRID, (j + 0.5) / WALL_GRID) for i in range(WALL_GRID) for j in range(WALL_GRID)]
    out = []
    for face in solid.faces():
        if face.area < min_area:
            continue
        for u, v in grid:
            pt, n = face.position_at(u, v), face.normal_at(u, v)
            if not solid.is_inside(pt - n * 0.01) or solid.is_inside(pt + n * 0.01):
                continue  # Sample point off a trimmed face
            d = n if gaps else -n
            hits = sorted((((h[0] - pt).dot(d), h[1]) for h in solid.find_intersection_points(Axis(pt, d))),
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
    assert sorted(P.lepton_carrier_holes) == metrics["lepton_holes"]
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
                      + ref + r'"(?:(?!\n\t\(footprint ).)*', text, re.DOTALL)
        assert m, f"{ref} not found on B.Cu"
        block = m.group(0)
        fx, fy = (float(v) for v in re.search(r"\n\t\t\(at ([-\d.]+) ([-\d.]+)", block).groups())  # type: ignore[union-attr]
        for px, py in re.findall(r'\(pad "\d+" thru_hole \w+\s*\(at ([-\d.]+) ([-\d.]+)', block):
            pads.add((round(fx + float(px), 2), round(-(fy + float(py)), 2)))
    assert pads == {(round(x, 2), round(y, 2)) for x, y in (*J2_TAILS, *J5J6_TAILS)}
    # USB-C J3 (180 deg on B.Cu): its four shell legs ("SH" oval pads) are the tails j3_leg_tails models
    m = re.search(r'\(footprint "[^"]+"\s*\(layer "B\.Cu"\)(?:(?!\n\t\(footprint ).)*?\(property "Reference" "J3"'
                  r'(?:(?!\n\t\(footprint ).)*', text, re.DOTALL)
    assert m, "J3 not found on B.Cu"
    fx, fy, rot = re.search(r"\n\t\t\(at ([-\d.]+) ([-\d.]+) ([-\d.]+)\)", m.group(0)).groups()  # type: ignore[union-attr]
    assert float(rot) == 180.0
    legs = {(round(float(fx) - float(px), 2), round(-(float(fy) - float(py)), 2), float(w), float(ln))
            for px, py, w, ln in re.findall(r'\(pad "SH" thru_hole oval\s*\(at ([-\d.]+) ([-\d.]+)(?: [-\d.]+)?\)\s*\(size ([-\d.]+) ([-\d.]+)\)',
                                            m.group(0))}
    tails = {(round(b.center().X, 2), round(b.center().Y, 2), round(b.size.X, 2), round(b.size.Y, 2))
             for b in (t.bounding_box() for t in j3_leg_tails(P))}
    assert legs == tails, (legs, tails)


# --- Lepton carrier (its own KiCad board) ------------------------------------------------------------

CARRIER_PCB_PATH = KICAD_PCB_PATH.with_name("diy-thermocam-v4.0-lepton.kicad_pcb")
J2_SOCKET_H = 8.5  # HC-PM254-8.5H-2x10PZ: entry face 8.5 mm below the main board


def _footprints(path) -> list[tuple[str, str, str, float, float, float, str]]:
    """(footprint, reference, layer, x, y, rotation, text) of every footprint, KiCad coordinates."""
    out = []
    for block in path.read_text().split("\n\t(footprint ")[1:]:
        name = re.match(r'"([^"]+)"', block)
        layer = re.search(r'\(layer "([^"]+)"\)', block)
        at = re.search(r"\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)", block)
        ref = re.search(r'\(property "Reference" "([^"]+)"', block)
        assert name and layer and at and ref, block[:200]
        out.append((name[1], ref[1], layer[1], float(at[1]), float(at[2]), float(at[3] or 0.0), block))
    return out


def _tht_pads(block: str, x: float, y: float) -> dict[int, tuple[float, float]]:
    """Pad number -> device XY of a footprint's through-hole pads (footprint not rotated)."""
    return {int(n): (round(x + float(px), 2), round(-(y + float(py)), 2))
            for n, px, py in re.findall(r'\(pad "(\d+)" thru_hole \w+\s*\(at ([-\d.]+) ([-\d.]+)', block)}


def _step_solids(path) -> list[Solid]:
    """Solids of a KiCad STEP export. Some vendor models (the Molex socket) are bare shells, not
    solids; each such shell counts as its bounding box, which is never smaller than the part."""
    from build123d import import_step

    shape = import_step(str(path))
    solids = list(shape.solids())
    in_solids = {hash(sh) for s in solids for sh in s.shells()}
    loose = [sh for sh in shape.shells() if hash(sh) not in in_solids]
    return solids + [Solid.from_bounding_box(sh.bounding_box()) for sh in loose]


def _only(fps, pattern: str):
    hits = [fp for fp in fps if re.search(pattern, fp[0])]
    assert len(hits) == 1, f"{pattern}: {[fp[1] for fp in hits]}"
    return hits[0]


def test_carrier_board_matches_the_enclosure_envelope() -> None:
    """The carrier keeps the outline, holes, header and socket position both enclosures are built
    around, and each header pin lands on the J2 pad of the same number."""
    text = CARRIER_PCB_PATH.read_text()
    pts = []
    for block in re.findall(r"\n\t\(gr_\w+(.*?)\n\t\)", text, re.DOTALL):
        if '(layer "Edge.Cuts")' in block:
            pts += [(float(x), -float(y)) for x, y in re.findall(r"\((?:start|mid|end) ([-\d.]+) ([-\d.]+)\)", block)]
    xs, ys = [x for x, _ in pts], [y for _, y in pts]
    assert (min(xs), max(xs), min(ys), max(ys)) == pytest.approx(
        (P.carrier_x0, P.carrier_x1, P.carrier_y0, P.carrier_y1), abs=0.01)

    fps = _footprints(CARRIER_PCB_PATH)
    holes = sorted((round(x, 2), round(-y, 2)) for name, _, _, x, y, _, _ in fps if "MountingHole" in name)
    assert holes == sorted(P.lepton_carrier_holes)

    _, _, h_layer, hx, hy, h_rot, h_block = _only(fps, r"PinHeader_2x10")
    _, _, s_layer, sx, sy, _, _ = _only(fps, r"LeptonSocket")
    assert h_layer == "F.Cu" and s_layer == "B.Cu"  # Header toward the main board, Lepton toward the back wall
    assert (sx, -sy) == pytest.approx((P.lepton_x, P.lepton_y), abs=0.01)
    _, _, j2_layer, jx, jy, j2_rot, j2_block = _only(
        [fp for fp in _footprints(KICAD_PCB_PATH) if fp[1] == "J2"], r".")
    assert h_rot == 0.0 and j2_rot == 0.0 and j2_layer == "B.Cu"
    header, j2 = _tht_pads(h_block, hx, hy), _tht_pads(j2_block, jx, jy)
    assert header and all(j2.get(n) == xy for n, xy in header.items()), (header, j2)


@pytest.fixture(scope="module")
def carrier_step(tmp_path_factory):
    """The carrier's KiCad STEP in the device frame, split into the whole board, the header and
    the back-side parts other than the Lepton socket."""
    import subprocess

    from thermocam_case.scene import _kicad_cli

    try:
        cli = _kicad_cli()
    except FileNotFoundError:
        pytest.skip("kicad-cli (KiCad 10) not installed")
    fps = _footprints(CARRIER_PCB_PATH)
    header = _only(fps, r"PinHeader_2x10")[1]
    back = ",".join(ref for name, ref, layer, *_ in fps
                    if layer == "B.Cu" and "LeptonSocket" not in name and "MountingHole" not in name)
    out = tmp_path_factory.mktemp("carrier")
    prl = CARRIER_PCB_PATH.with_suffix(".kicad_prl")
    had_prl = prl.exists()
    z_back_face = P.pcb_z_back - P.lepton_sleeve_len - P.carrier_t  # STEP: board back face at Z=0
    solids = {}
    for key, args in (("all", []), ("header", ["--no-board-body", "--component-filter", header]),
                      ("back_parts", ["--no-board-body", "--component-filter", back])):
        path = out / f"{key}.step"
        subprocess.run([cli, "pcb", "export", "step", "--user-origin", "0x0mm", "-f", *args, "-o", str(path),
                        str(CARRIER_PCB_PATH)], check=True, capture_output=True)
        solids[key] = [_move(s, 0.0, 0.0, z_back_face) for s in _step_solids(path)]
    if not had_prl:
        prl.unlink(missing_ok=True)
    return solids


def test_carrier_parts_fit_their_height_budget(carrier_step) -> None:
    """Back-side parts stay within carrier_parts_h; the header reaches far enough into J2 without
    bottoming out on the main board."""
    z_back_face = P.pcb_z_back - P.lepton_sleeve_len - P.carrier_t
    back = min(s.bounding_box().min.Z for s in carrier_step["back_parts"])
    assert back >= z_back_face - P.carrier_parts_h, f"back-side parts reach Z {back:.2f}"
    hb = [s.bounding_box() for s in carrier_step["header"]]
    hx0, hx1 = min(b.min.X for b in hb), max(b.max.X for b in hb)
    hy0, hy1 = min(b.min.Y for b in hb), max(b.max.Y for b in hb)
    pins_x, pins_y = [x for x, _ in J2_TAILS], [y for _, y in J2_TAILS]
    # The header's 3D model sits over its pads (a wrong model rotation shows in the renders and STEP)
    assert hx0 == pytest.approx(min(pins_x) - 1.27, abs=0.1) and hx1 == pytest.approx(max(pins_x) + 1.27, abs=0.1), \
        f"header model spans X {hx0:.2f}..{hx1:.2f}"
    assert hy0 == pytest.approx(min(pins_y) - 1.27, abs=0.1) and hy1 == pytest.approx(max(pins_y) + 1.27, abs=0.1), \
        f"header model spans Y {hy0:.2f}..{hy1:.2f}"
    tip = max(b.max.Z for b in hb)
    entry = P.pcb_z_back - J2_SOCKET_H
    assert tip - entry >= 4.0, f"header pins enter J2 only {tip - entry:.2f} mm"
    assert tip <= P.pcb_z_back - 1.0, f"header pin tips at Z {tip:.2f}, J2 floor at {P.pcb_z_back}"


def test_carrier_from_kicad_clears_both_enclosures(enc, laser, k, carrier_step) -> None:
    """The real carrier (board, socket, header, every part) touches no printed part, plate,
    standoff or the main board."""
    stack = build_standoff_stack(P)
    others = {**_shell_parts(enc), **_laser_parts(enc, laser), **{f"standoff_{i}": s for i, s in enumerate(stack.all)}}
    others["main_pcb"] = k.pcb
    others.update({n: s for n, s in k.real.items() if n.startswith(("T41_", "J5J6_"))})  # The Teensy beside it
    pairs = [(f"carrier[{i}] x {n}", s, part) for i, s in enumerate(carrier_step["all"]) for n, part in others.items()]
    _assert_clear(pairs)
    edge_x = max(s.bounding_box().max.X for s in carrier_step["all"])
    assert edge_x <= P.carrier_x1 + 0.01, f"a carrier part overhangs its +X edge to {edge_x:.2f}"


SW1_MODEL_ACTUATOR_DY = 1.0  # SW1's KiCad model: actuator in position a, +1.0 mm from pin 2


@pytest.fixture(scope="module")
def main_board_step(tmp_path_factory) -> list[Solid]:
    """The main board's KiCad STEP (board and every part model) in the device frame."""
    import subprocess

    from thermocam_case.scene import _kicad_cli

    try:
        cli = _kicad_cli()
    except FileNotFoundError:
        pytest.skip("kicad-cli (KiCad 10) not installed")
    path = tmp_path_factory.mktemp("main_board") / "main.step"
    prl = KICAD_PCB_PATH.with_suffix(".kicad_prl")
    had_prl = prl.exists()
    subprocess.run([cli, "pcb", "export", "step", "--user-origin", "0x0mm", "-f", "-o", str(path),
                    str(KICAD_PCB_PATH)], check=True, capture_output=True)
    if not had_prl:
        prl.unlink(missing_ok=True)
    return [_move(s, 0.0, 0.0, P.pcb_z_back) for s in _step_solids(path)]  # STEP: back face at Z=0


def test_main_board_from_kicad_clears_both_enclosures(enc, laser, main_board_step) -> None:
    """The real main board, with the vendor model of every part (USB-C, switches, battery connector,
    pogo pins), touches no printed part, FR4 board, acrylic plate or standoff of either build."""
    stack = build_standoff_stack(P)
    others = {**_shell_parts(enc), **_laser_parts(enc, laser), **{f"standoff_{i}": s for i, s in enumerate(stack.all)}}
    # The CAD draws the slider and key at mid-travel; SW1's model shows its actuator at end position a,
    # 1.0 mm toward +Y (C&K: positions 1.0 mm either side of pin 2), so they follow it there
    for name in ("slide_switch_cap", "laser_switch_key"):
        others[name] = _move(others[name], 0.0, SW1_MODEL_ACTUATOR_DY, 0.0)
    # KiCad exports the 1.6 mm board's body as its 1.51 mm dielectric, back face at Z=0
    board = (0.0, P.pcb_w, 0.0, P.pcb_h, P.pcb_z_back, P.pcb_z_back + P.pcb_t)
    boxes = [(b.min.X, b.max.X, b.min.Y, b.max.Y, b.min.Z, b.max.Z) for b in (s.bounding_box() for s in main_board_step)]
    assert any(box[:5] == pytest.approx(board[:5], abs=0.01) and box[5] == pytest.approx(board[5], abs=0.1)
               for box in boxes), "no board body in the KiCad STEP at the enclosure's PCB position"
    pairs = [(f"main[{i}] x {n}", s, part) for i, s in enumerate(main_board_step) for n, part in others.items()]
    _assert_clear(pairs)


def test_usb_c_plug_boot_reaches_j3(enc, laser) -> None:
    """A 13.8 x 7.8 mm USB-C plug boot passes every wall and plate on its way to the J3 mouth."""
    boot = _usb_c_boot()
    walls = {"front_shell": enc.front_shell, "back_shell": enc.back_shell,
             **{f"laser_{n}": s for n, s in laser.plates_3d.items()},
             **{f"laser_{n}": s for n, s in laser.printed.items()}}
    _assert_clear([(f"USB-C boot x {n}", boot, s) for n, s in walls.items()], tol=1e-4)


def test_teensy_plugs_in_past_the_fitted_carrier(k, carrier_step) -> None:
    """Build order: the carrier goes on first (its H102/H103 nuts need a nut driver where the Teensy
    would be), then the Teensy slides straight into J5/J6 beside it."""
    teensy = [s for n, s in k.real.items() if n.startswith("T41_") and n != "T41_sd_card"]
    carrier = {**{f"carrier[{i}]": s for i, s in enumerate(carrier_step["all"])},
               **{n: s for n, s in k.real.items() if n.startswith(("lepton_", "carrier_"))}}
    for i in range(25):  # 12 mm: well past the 6 mm header engagement
        moved = [_move(s, 0.0, 0.0, -0.5 * i) for s in teensy]
        hits = [n for n, c in carrier.items() for m in moved if _inter_vol(m, c) >= 1e-3]
        assert not hits, f"Teensy {0.5 * i:.1f} mm out of J5/J6 hits {sorted(set(hits))}"


def _box(x0: float, x1: float, y0: float, y1: float, z0: float, z1: float) -> Solid:
    from build123d import Align, Box

    return _move(Box(x1 - x0, y1 - y0, z1 - z0, align=(Align.MIN, Align.MIN, Align.MIN)), x0, y0, z0)


def _adafruit_cell() -> Solid:
    """Adafruit 258 (PKCELL LP503562), nominal 34 x 62 x 5 mm, in the middle of the cradle."""
    bat = P.battery
    return _box(bat.center_x - 31.0, bat.center_x + 31.0, bat.center_y - SPACER_CELL_W / 2.0,
                bat.center_y + SPACER_CELL_W / 2.0, bat.z_start, bat.z_start + 5.0)


def test_battery_lead_reaches_j4_round_the_right_edge(enc, laser, k) -> None:
    """The LiPo lead, plugged into J4 (board back, mouth toward -Y) before the board goes in, leaves
    the 34 mm cell at its corner, runs straight out through the cradle's wire gap, over the board and
    round the right board edge level with J4, clear of both cases. The front shell's tab in that gap
    ends at Y 43.5. The route is under 45 mm, so any lead of 50 mm or more reaches, and the spare
    lead folds into the free space beside the cradle."""
    j4 = k.real["J4_battery"].bounding_box()
    jy0, jz1 = j4.min.Y, j4.max.Z
    pcb_top = P.pcb_z_back + P.pcb_t
    cell = _adafruit_cell().bounding_box()
    wall_out = P.battery.center_x + (P.battery.width_x + 1.0) / 2.0 + P.cradle_wall_t
    z = battery_stack_z(P)
    lead = {
        "plug": _box(82.0, 87.0, jy0 - 6.7, jy0, 15.2, jz1 - 0.1),           # PHR-2 housing in the mouth
        "under board": _box(84.0, P.pcb_w, jy0 - 7.7, jy0 - 6.7, 16.0, 19.0),  # Wires bent toward the edge
        "along edge": _box(P.pcb_w + 0.1, P.pcb_w + 1.2, jy0 - 7.7, 52.0, 16.0, 19.0),
        "up the gap": _box(P.pcb_w + 0.1, P.pcb_w + 1.2, 48.0, 52.0, 16.0, pcb_top + 1.3),
        "over the board": _box(wall_out + 0.1, P.pcb_w + 1.2, 48.0, 52.0, pcb_top + 0.1, pcb_top + 1.3),
        "down the wall": _box(wall_out + 0.1, wall_out + 1.3, 48.0, cell.max.Y - 0.5, pcb_top + 0.1, cell.max.Z - 1.5),
        "out of the cell": _box(cell.max.X, wall_out + 1.3, 48.0, cell.max.Y - 0.5, cell.min.Z + 1.5, cell.max.Z - 1.5),
        "spare lead": _box(wall_out + 1.5, P.pcb_w - 0.4, 14.0, 46.0, pcb_top + 0.1, z["lid_top"]),
    }
    stack = build_standoff_stack(P)
    others = {**_shell_parts(enc), **_laser_parts(enc, laser), **{f"standoff_{i}": s for i, s in enumerate(stack.all)},
              "spacer_0": enc.battery_spacers[0], "spacer_1": enc.battery_spacers[1],
              "pcb": k.pcb, **{n: s for n, s in k.real.items() if n not in ("J4_battery", "battery")}}
    _assert_clear([(f"lead {ln} x {n}", ls, o) for ln, ls in lead.items() for n, o in others.items()])
    # Route length along the middle of each section, from the cell to the back of the plug
    zc, ye, xe = 0.5 * (cell.min.Z + cell.max.Z), jy0 - 7.2, P.pcb_w + 0.65
    pts = [(cell.max.X, 50.0, zc), (wall_out + 0.7, 50.0, zc), (wall_out + 0.7, 50.0, pcb_top + 0.7),
           (xe, 50.0, pcb_top + 0.7), (xe, 50.0, 17.5), (xe, ye, 17.5), (84.5, ye, 17.5), (84.5, jy0 - 6.7, 17.5)]
    route = sum(math.dist(a, b) for a, b in itertools.pairwise(pts))
    assert route <= 45.0, f"lead route {route:.1f} mm"


def test_adafruit_cell_sits_between_the_spacer_bars(enc, laser, k) -> None:
    """The 34 mm Adafruit 258 lies in the 51.5 mm cradle between two printed spring bars. In place,
    each bar presses on the cradle wall and the cell and clears every other part in both builds; the
    bar takes up the cradle and bar print tolerances (MJF +-0.3 mm) for cells 33.5..36 mm wide, within
    PA12's strength; and for every width the lead's corner lines up with the wire gap."""
    bars, cell = enc.battery_spacers, _adafruit_cell()
    stack = build_standoff_stack(P)
    others = {**_shell_parts(enc), **_laser_parts(enc, laser), **{f"standoff_{i}": s for i, s in enumerate(stack.all)},
              "pcb": k.pcb, **{n: s for n, s in k.real.items() if n != "battery"}}  # The 50 x 60 cell's space
    pairs = [(f"spacer {i} x {n}", bar, o) for i, bar in enumerate(bars) for n, o in {**others, "cell": cell}.items()]
    pairs += [("spacer 0 x spacer 1", *bars), *[(f"cell x {n}", cell, o) for n, o in others.items()]]
    _assert_clear(pairs)
    for bar, out in zip(bars, (-1.0, 1.0)):
        assert _inter_vol(_move(bar, 0.0, 0.05 * out, 0.0), enc.battery_cradle) > 0.5, "bar against the wall"
        assert _inter_vol(_move(bar, 0.0, -0.05 * out, 0.0), cell) > 0.5, "bar against the cell"
    lipo = next(prop.solid for prop in build_render_props(P) if prop.name == "lipo")
    _assert_clear([(f"lipo prop x spacer {i}", lipo, bar) for i, bar in enumerate(bars)], tol=1e-2)

    # As printed: end blocks SPACER_SOLID_W wide, bumps SPACER_FREE_W; pressed flat, the beams keep
    # an MJF slot between them
    bar = build_battery_spacer()
    assert (bar & _box(0.5, 2.5, -9.0, 9.0, 0.0, SPACER_H)).bounding_box().size.Y == pytest.approx(SPACER_SOLID_W)
    assert bar.bounding_box().size.Y == pytest.approx(SPACER_FREE_W)
    full = (SPACER_FREE_W - SPACER_SOLID_W) / 2.0
    assert SPACER_SOLID_W - 2.0 * SPACER_BEAM_T - 2.0 * full >= MJF_MIN_DETAIL

    # Tolerances: the widest cell fits with both beams pressed flat; the narrowest is still pressed
    mjf, in_y, in_x = 0.3, P.battery.height_y + 1.0, P.battery.width_x + 1.0
    assert SPACER_SOLID_W + mjf <= (in_y - mjf - SPACER_CELL_W_RANGE[1]) / 2.0 + 1e-9
    assert SPACER_FREE_W - mjf >= (in_y + mjf - SPACER_CELL_W_RANGE[0]) / 2.0
    # Lengthwise the bars keep 0.1 mm of play at worst, and the 62.5 mm maximum cell fits; the FR4 lid
    # clears the bars and the 5.2 mm maximum cell
    assert (in_x - mjf) - (SPACER_LEN + mjf) >= 0.1 and 62.5 <= in_x - mjf
    z = battery_stack_z(P)
    assert SPACER_H + mjf <= z["lid_bot"] - z["floor_top"] - 0.15 and 5.2 <= z["lid_bot"] - z["floor_top"] - 0.25

    # Spring: each beam is a fixed-fixed PA12 beam loaded at its middle (E 1.7 GPa, 48 MPa tensile)
    span, i_beam = SPACER_LEN - 2.0 * SPACER_END_L, SPACER_H * SPACER_BEAM_T ** 3 / 12.0
    stress = 12.0 * 1700.0 * SPACER_BUMP_H * SPACER_BEAM_T / span ** 2
    grip = 192.0 * 1700.0 * i_beam * spacer_squeeze(P) / span ** 3
    assert stress <= 48.0 / 3.0, f"{stress:.1f} MPa pressed flat"
    assert grip >= 0.023 * 9.81 * 4.0, f"{grip:.2f} N holds the 23 g cell at 4 g"

    # The lead leaves the board end 0.5..3 mm inside the cell's top corner: in the wire gap, with
    # room for a 1.2 mm wire on each side
    gap_y0, gap_y1 = CRADLE_WIRE_GAP_Y - CRADLE_WIRE_GAP_W / 2.0, CRADLE_WIRE_GAP_Y + CRADLE_WIRE_GAP_W / 2.0
    for w in SPACER_CELL_W_RANGE:
        top = P.battery.center_y + w / 2.0
        assert gap_y0 + 1.2 <= top - 3.0 and top - 0.5 <= gap_y1 - 1.2, f"{w} mm cell"


# JLCPCB panel tabs (PCB layout, board frame): (edge, centre along the edge, width). After filing,
# a stub stands up to ~0.2 mm proud of the routed edge; checked at 0.3 mm
MAIN_TABS = [("bottom", 6.0, 5.0), ("bottom", 32.5, 5.0), *(("top", x, 5.0) for x in (25.0, 45.0, 65.0)),
             ("right", 26.0, 5.0), ("left", 4.5, 5.0)]
CARRIER_TABS = [("low", 42.6, 8.0), ("high", 46.0, 8.0)]   # Short edges at Y 14 and Y 61
TAB_STUB = 0.3


def test_panel_tab_stubs_clear_both_cases(enc, laser, k) -> None:
    """A filed tab stub on the main board or the carrier edge touches no case part, with the board
    in place or lowered in from +Z (shell: into the back shell; acrylic: onto the back plate, past
    the bottom plate and tripod mount)."""
    zb, zt = P.pcb_z_back, P.pcb_z_back + P.pcb_t
    stubs = {}
    for edge, at, w in MAIN_TABS:
        a0, a1 = at - w / 2.0, at + w / 2.0
        if edge in ("bottom", "top"):
            y0 = -TAB_STUB if edge == "bottom" else P.pcb_h
            stubs[f"main {edge} {at:g}"] = _box(a0, a1, y0, y0 + TAB_STUB, zb, zt)
        else:
            x0 = -TAB_STUB if edge == "left" else P.pcb_w
            stubs[f"main {edge} {at:g}"] = _box(x0, x0 + TAB_STUB, a0, a1, zb, zt)
    cb = k.real["carrier_pcb"].bounding_box()
    for name, at, w in CARRIER_TABS:
        y0, y1 = (cb.min.Y - TAB_STUB, cb.min.Y) if name == "low" else (cb.max.Y, cb.max.Y + TAB_STUB)
        stubs[f"carrier {name} {at:g}"] = _box(at - w / 2.0, at + w / 2.0, y0, y1, cb.min.Z, cb.max.Z)
    stack = build_standoff_stack(P)
    others = {**_shell_parts(enc), **_laser_parts(enc, laser), **{f"standoff_{i}": s for i, s in enumerate(stack.all)}}
    _assert_clear([(f"{sn} x {n}", s, o) for sn, s in stubs.items() for n, o in others.items()])
    # Lowered in: each stub swept from its place up past the case's open side
    entry = {"back_shell": enc.back_shell, "laser_back": laser.plates_3d["back"],
             "laser_bottom": laser.plates_3d["bottom"], "laser_tripod": laser.printed["tripod_mount"]}
    sweeps = {}
    for sn, s in stubs.items():
        b = s.bounding_box()
        sweeps[sn] = _box(b.min.X, b.max.X, b.min.Y, b.max.Y, b.min.Z, P.internal_depth + 5.0)
    _assert_clear([(f"{sn} lowered x {n}", s, o) for sn, s in sweeps.items() for n, o in entry.items()])


def test_carrier_and_its_sleeves_clear_the_teensy(k) -> None:
    """The carrier shares the Teensy PCB's height (Z 7.4..9.0): its edge stays 0.8 mm off the
    Teensy (V-cut and Teensy width tolerances), the nylon sleeves at H102/H103 0.5 mm (a touch
    there is nylon on a bare board edge, harmless)."""
    t41 = k.real["T41_pcb"]
    assert k.real["carrier_pcb"].distance_to(t41) >= 0.8 - 1e-6
    assert min(k.real[f"lepton_sleeve_{i}"].distance_to(t41) for i in range(4)) >= 0.5 - 1e-6


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


def test_every_printed_wall_meets_mjf_minimum(enc, laser) -> None:
    """JLC3DP's MJF guide: every wall at least 1.0 mm, every slot and groove at least 0.8 mm wide."""
    thin = {}
    for name, solid in _printed_parts(enc, laser).items():
        samples = [(t, pt, "wall") for t, pt in _wall_samples(solid) if t < MJF_MIN_WALL - 1e-3]
        samples += [(t, pt, "gap") for t, pt in _wall_samples(solid, gaps=True) if t < MJF_MIN_DETAIL - 1e-3]
        if samples:
            thin[name] = sorted(samples)[:3]
    assert not thin, "under the MJF minimum: " + "; ".join(
        f"{n}: " + ", ".join(f"{kind} {t:.2f} at {pt}" for t, pt, kind in s) for n, s in thin.items())


def test_printed_details_meet_mjf_minimum() -> None:
    """Raised and engraved details at least 0.8 mm tall / deep (JLC3DP MJF guide); the snap features
    keep enough spring for that."""
    assert CAP_BUMP_H >= MJF_MIN_DETAIL and CAP_NOTCH_DEPTH >= CAP_BUMP_H and CAP_GRIP_H >= MJF_MIN_DETAIL
    assert CAP_BUMP_TOP >= MJF_MIN_DETAIL and CAP_BUMP_W >= MJF_MIN_DETAIL
    # Rings, ridges and centre dot of the back-face lens mark
    edges = sorted(r for ring in BRAND_RINGS for r in ring if r > 0)
    assert all(b - a >= MJF_MIN_DETAIL for a, b in itertools.pairwise(edges)) and 2.0 * edges[0] >= MJF_MIN_DETAIL
    # Lens-cap finger: 0.8 mm bump - 0.15 mm gap to the track floor bends a 7.8 mm, 1.35 mm finger 0.65 mm
    e = _shell_envelope(P)
    cap_t = e["z_track_floor"] - 0.15 - e["z_back_outer"]
    strain = 1.5 * cap_t * (CAP_BUMP_H - 0.15) / 7.8**2
    assert cap_t >= MJF_MIN_WALL and strain < 0.04, f"lens-cap finger strain {strain:.1%}"


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
    laser_nut = tripod_hex_nut(P.laser_tripod_x, P.laser_tripod_z, y_out + TRIPOD_FLOOR_T)
    # Each nut touches its holder only at its two crush ribs (test_tripod_nuts_stay_on_the_screw_axis)
    _assert_rib_contacts_only(shell_nut, enc.back_shell)
    _assert_rib_contacts_only(laser_nut, laser.printed["tripod_mount"])
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

NUT_DRIVER_OD = 7.5  # The README's limit for the 5 mm nut driver that holds the rear standoffs


def test_rear_standoffs_take_the_readme_nut_driver(enc) -> None:
    """The README asks for a 5 mm nut driver at most NUT_DRIVER_OD across: that socket slides down
    each 20 mm standoff to 1 mm above its seat, past the Prog pin guide (7.6 mm left) and the
    capture-button guide (8.0 mm) beside two of them."""
    from build123d import Align, Cylinder

    readme = (ENCLOSURE_ROOT / "README.md").read_text()
    assert f"at most {NUT_DRIVER_OD:g} mm across" in readme
    for s in build_standoff_stack(P).rear:
        bb = s.bounding_box()
        socket = Location(((bb.min.X + bb.max.X) / 2.0, (bb.min.Y + bb.max.Y) / 2.0, bb.min.Z + 1.0)) * Cylinder(
            NUT_DRIVER_OD / 2.0, 30.0, align=(Align.CENTER, Align.CENTER, Align.MIN))
        assert _inter_vol(socket, enc.back_shell) < 1e-3


def test_screw_engagement_fits_standoff_threads() -> None:
    """Every screw and male stud bites >= 2.0 mm and, with the smallest real countersunk head sunk
    into its countersink, stays >= 0.5 mm short of the thread bottom."""
    for j in screw_joints(P):
        assert 2.0 <= j.engagement and j.engagement_max <= j.thread_depth - 0.5 + 1e-9, (
            f"{j.name}: engagement {j.engagement:.2f}..{j.engagement_max:.2f} mm vs thread depth {j.thread_depth:.2f} mm"
        )
    assert ISO7046_M25_THEORETICAL_HEAD_D <= P.countersink_head_d  # Head sits flush or below


def test_tripod_threads_engage(enc, laser) -> None:
    e = _shell_envelope(P)
    assert TRIPOD_SCREW_LEN - P.shell_tripod_floor_t >= 3.0
    _, y_out = laser_bottom_faces(P)
    mount = laser.printed["tripod_mount"]
    assert y_out - 0.01 <= mount.bounding_box().min.Y <= y_out + 0.1  # Flush with the outer face
    assert TRIPOD_SCREW_LEN - (mount.bounding_box().min.Y - y_out) - TRIPOD_FLOOR_T >= 3.0
    # The holder lifts at most TRIPOD_FIN_GAP before its fin meets the PCB edge (the gap covers the
    # board outline's +-0.2 mm and a 3.2 mm plate's 0.1 mm), and can't drop out (shoulder on the plate)
    from thermocam_case.accessories import TRIPOD_FIN_GAP

    k_pcb = build_keepouts(P).pcb
    assert _inter_vol(_move(mount, 0, TRIPOD_FIN_GAP - 0.02, 0), k_pcb) < 1e-3
    assert _inter_vol(_move(mount, 0, TRIPOD_FIN_GAP + 0.15, 0), k_pcb) > 0.05
    fin_y0 = mount.bounding_box().min.Y  # Plug face; the fin is MJF_MIN_WALL thick from the plate
    assert -TRIPOD_FIN_GAP - laser_bottom_faces(P)[0] >= MJF_MIN_WALL - 1e-9 and fin_y0 < 0
    assert _inter_vol(_move(mount, 0, -0.1, 0), laser.plates_3d["bottom"]) > 0.05
    # Long tripod screws in the shell stop in the nut cavity, below the blind roof
    k = build_keepouts(P)
    _assert_clear([
        ("tripod stud x back_shell", k.tripod_stud, enc.back_shell),
        ("tripod stud x pcb", k.tripod_stud, k.pcb),
        ("tripod stud x usb", k.tripod_stud, k.real["J3_usb_c"]),
    ])
    assert e["oy0"] < 0


def _assert_rib_contacts_only(nut: Solid, holder: Solid) -> None:
    """The nut overlaps its holder in exactly two crush-rib contacts, each TRIPOD_RIB_SQUEEZE deep,
    1.0 mm wide and at least 2.5 mm long."""
    squeeze = nut & holder
    ribs = squeeze.solids() if squeeze is not None else []
    assert len(ribs) == 2, f"{len(ribs)} contacts"
    for s in ribs:
        size = s.bounding_box().size
        assert size.X == pytest.approx(TRIPOD_RIB_SQUEEZE, abs=0.02) and size.Y <= 1.0 + 1e-3 and size.Z >= 2.5, size


def _assert_slides_home(nut: Solid, holder: Solid, step: tuple[float, float, float], steps: int) -> None:
    """Moving the nut from `steps` x `step` away back to its seat, it touches its holder only at
    the crush ribs: every contact is at most TRIPOD_RIB_SQUEEZE deep, on one of the nut's X flats."""
    axis_x = nut.bounding_box().center().X
    for i in range(steps, -1, -1):
        moved = _move(nut, *(i * c for c in step))
        inter = moved & holder
        for s in (inter.solids() if inter is not None else []):
            b = s.bounding_box()
            assert b.size.X <= TRIPOD_RIB_SQUEEZE + 0.02, f"{i} steps out: contact {b.size} at {b.center()}"
            assert abs(abs(b.center().X - axis_x) - (TRIPOD_NUT_AF - TRIPOD_RIB_SQUEEZE) / 2.0) < 0.05, \
                f"{i} steps out: contact off the ribs at {b.center()}"


def test_tripod_nuts_slide_in_past_the_ribs_only(enc, laser) -> None:
    """Shell: the nut drops in from the open shell (seam) to its pocket. Acrylic: it slides into the
    mount from the mount's back face. On the way only the crush ribs touch it."""
    e = _shell_envelope(P)
    shell_nut = tripod_hex_nut(P.tripod_x, P.shell_tripod_z, e["oy0"] + P.shell_tripod_floor_t)
    travel = e["z_back_seam"] + 1.0 - shell_nut.bounding_box().min.Z
    _assert_slides_home(shell_nut, enc.back_shell, (0.0, 0.0, 0.25), math.ceil(travel / 0.25))
    _, y_out = laser_bottom_faces(P)
    laser_nut = tripod_hex_nut(P.laser_tripod_x, P.laser_tripod_z, y_out + TRIPOD_FLOOR_T)
    mount = laser.printed["tripod_mount"]
    travel = laser_nut.bounding_box().max.Z - mount.bounding_box().min.Z + 1.0
    _assert_slides_home(laser_nut, mount, (0.0, 0.0, -0.25), math.ceil(travel / 0.25))


def test_tripod_nuts_stay_on_the_screw_axis(enc, laser, k) -> None:
    """A tripod screw only finds a nut that sits on the bore's axis (Ø6.8 bore, Ø6.35 screw).
    Shell: the nut slides in from the board side; crush ribs hold it over its last 2.8 mm, before
    which nothing else would. Acrylic: the nut slides in from the back between
    crush ribs; the roof, the pocket's V and the bottom plate's hole edge leave it < 0.25 mm of play."""
    e = _shell_envelope(P)
    shell_nut = tripod_hex_nut(P.tripod_x, P.shell_tripod_z, e["oy0"] + P.shell_tripod_floor_t)
    rest = _inter_vol(shell_nut, enc.back_shell)
    for dz in (0.5, 1.5, 2.5):  # Still squeezed by the ribs on its way out toward the board
        assert _inter_vol(_move(shell_nut, 0, 0, dz), enc.back_shell) > 0.25 * rest
    _, y_out = laser_bottom_faces(P)
    nut = tripod_hex_nut(P.laser_tripod_x, P.laser_tripod_z, y_out + TRIPOD_FLOOR_T)
    mount, plate = laser.printed["tripod_mount"], laser.plates_3d["bottom"]
    rest = _inter_vol(nut, mount)
    # Roof 0.3 mm above the nut, the pocket's V and the plate's hole edge under 0.25 mm away
    for d, stop in (((0, 0.35, 0), mount), ((0, 0, 0.25), mount), ((0, 0, -0.25), plate)):
        moved = _move(nut, *d)
        assert _inter_vol(moved, stop) > (rest + 0.5 if stop is mount else 0.01), f"nut free toward {d}"
    assert P.shell_tripod_screw_d - 6.35 < 2 * 0.25 + 1e-9


# --- Mechanisms -----------------------------------------------------------------------------------

@pytest.mark.parametrize("which", ["shell", "laser"])
def test_capture_button_pushes_sw2_and_never_the_board(enc, laser, k, which) -> None:
    """The pusher stays 0.4 mm under the PCB, so it never lands on the board edge (whose stop could
    end the stroke short of the 0.1..0.4 mm pretravel under tolerances). Pressed all the way, until
    it meets the SW2 body with the plunger fully in, it touches nothing but SW2."""
    from thermocam_case.accessories import SW2_PUSHER_PCB_GAP

    if which == "shell":
        button, guides = enc.capture_button, [enc.back_shell, enc.front_shell]
    else:
        button, guides = laser.plates_3d["capture_key"], [laser.plates_3d["top"]]
    rest_gap = button.bounding_box().min.Y - SW2_TIP_Y
    assert rest_gap == pytest.approx(P.button_rest_gap, abs=1e-3)
    body_front = k.real["SW2_body"].bounding_box().max.Y
    full = rest_gap + (SW2_TIP_Y - body_front)  # Plunger pushed flush with the switch body
    # Everything that passes the board edge within the full stroke stays under the PCB
    lead = button & _box(70.0, 100.0, SW2_TIP_Y, PCB_TOP_EDGE_Y + full + 0.05, 0.0, 30.0)
    assert lead.bounding_box().max.Z <= P.pcb_z_back - SW2_PUSHER_PCB_GAP + 1e-6, "pusher reaches the PCB's height"
    for i in range(int(full / 0.05) + 1):
        pressed = _move(button, 0, -0.05 * i, 0)
        _assert_clear([("pressed x pcb", pressed, k.pcb), ("pressed x SW2 body", pressed, k.real["SW2_body"]),
                       *[(f"pressed x guide{n}", pressed, g) for n, g in enumerate(guides)]])
    _assert_clear([("rest x SW2 plunger", button, k.real["SW2_plunger"]),
                   *[(f"rest x guide{i}", button, g) for i, g in enumerate(guides)]])
    for stroke in (0.1, 0.4):  # Pretravel range: the pusher has the plunger at either end
        assert _inter_vol(_move(button, 0, -(rest_gap + stroke), 0), sw2_plunger()) > 0.05
    assert _inter_vol(_move(button, 0, -(full + 0.05), 0), k.real["SW2_body"]) > 0.0, "SW2 ends the travel"
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


def test_slider_and_button_stay_engaged_when_lifted(enc, k) -> None:
    """Display-down, or dragged by a thumb, the shell's slider and capture button ride up toward
    the display until the front shell stops them. The slider's fork then still holds the SW1
    actuator by 1 mm and drives it both ways; the button's flange stays under the board, so pressed
    all the way it still passes the board edge."""
    from thermocam_case.accessories import BUTTON_LIFT, SLIDER_LIFT

    def lift(part: Solid) -> float:
        for i in range(1, 61):
            if _inter_vol(_move(part, 0, 0, 0.05 * i), enc.front_shell) >= 1e-3:
                return 0.05 * (i - 1)
        return math.inf

    slider_lift, button_lift = lift(enc.slide_switch_cap), lift(enc.capture_button)
    assert slider_lift == pytest.approx(SLIDER_LIFT, abs=0.051)
    assert button_lift == pytest.approx(BUTTON_LIFT, abs=0.051)
    assert k.real["SW1_actuator"].bounding_box().max.Z - (enc.slide_switch_cap.bounding_box().min.Z + slider_lift) >= 1.0
    slider = _move(enc.slide_switch_cap, 0, 0, slider_lift)
    half = P.switch_travel / 2.0
    for dy in (-1.0, 1.0):
        assert _inter_vol(_move(slider, 0, dy * (half + 0.2), 0), sw1_actuator(dy * (half - 0.1))) > 0.01
    button = _move(enc.capture_button, 0, 0, button_lift)
    flange = button & _box(70.0, 100.0, SW2_TIP_Y, _shell_envelope(P)["iy1"] + 0.15, 0.0, 30.0)
    assert flange.bounding_box().max.Z <= P.pcb_z_back - 0.15
    full = P.button_rest_gap + (SW2_TIP_Y - k.real["SW2_body"].bounding_box().max.Y)
    for i in range(int(full / 0.05) + 1):
        _assert_clear([("lifted, pressed x pcb", _move(button, 0, -0.05 * i, 0), k.pcb)])
    # The front shell's stop fingers keep MJF's 0.3 mm from the back shell's button guide
    fingers = enc.front_shell & _box(P.button_x - 6.0, P.button_x + 6.0, PCB_TOP_EDGE_Y, 71.5, 19.0, 22.0)
    assert fingers.distance_to(enc.back_shell) >= 0.30 - 1e-3


def test_capture_button_drops_in_after_the_board(enc, k) -> None:
    """Build order: the board goes in first (a loose button could move under the SW2 plunger and
    hold the board up), then the button drops into its pocket from +Z past the board edge, SW2
    and the slider."""
    stack = build_standoff_stack(P)
    fixed = {"back_shell": enc.back_shell, "pcb": k.pcb, "slider": enc.slide_switch_cap,
             **{n: k.real[n] for n in ("SW2_body", "SW2_plunger", "SW1_body", "SW1_actuator", "J4_battery")},
             **{f"standoff_{i}": s for i, s in enumerate(stack.all)}}
    for i in range(101):
        moved = _move(enc.capture_button, 0, 0, 0.25 * i)
        _assert_clear([(f"button {0.25 * i:.2f} mm up x {n}", moved, o) for n, o in fixed.items()])


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


def test_laser_chute_latches_on_any_sheet(laser, k) -> None:
    """The chute's barbs click past the top plate on any 2.8..3.2 mm sheet, 0.3 mm to spare on
    3.2 mm (print tolerance), and the chute clears the card and the Teensy at either end of its play
    (on 2.8 mm it can slide out by grip - 2.8; a 3.2 or 2.8 mm plate sits up to 0.2 mm off centre)."""
    from thermocam_case.accessories import SD_CHUTE_GRIP

    chute, top = laser.printed["sd_chute"], laser.plates_3d["top"]
    x, half = P.microsd_x, (P.microsd_w + 2.4) / 2.0
    y_out = top.bounding_box().max.Y
    barb = chute & _box(x + half + 0.05, x + half + 0.75, y_out - 8.0, y_out - 1.0, 0.0, 40.0)
    bezel = chute & _box(x + half + 0.95, x + half + 1.4, y_out - 8.0, y_out + 5.0, 0.0, 40.0)
    grip = bezel.bounding_box().min.Y - barb.bounding_box().max.Y
    assert grip == pytest.approx(SD_CHUTE_GRIP, abs=1e-3)
    assert grip - 3.2 >= 0.3 - 1e-9 and grip - 2.8 <= 0.75
    assert _inter_vol(chute, top) < 1e-3
    assert _inter_vol(_move(chute, 0, grip - P.laser_t + 0.05, 0), top) > 0.01, "barbs hold the chute"
    card = {n: k.real[n] for n in ("T41_sd_card", "T41_sd_socket", "T41_pcb")}
    for dy in (-0.2, 0.0, 0.2, grip - 2.8 + 0.2):
        _assert_clear([(f"chute {dy:+.2f} x {n}", _move(chute, 0, dy, 0), o) for n, o in card.items()])
    assert chute.bounding_box().min.Y - 0.2 >= 61.11 + 0.3


def test_laser_spacers_and_margins(laser) -> None:
    for name in ("display_spacer_bottom", "display_spacer_top"):
        bb = laser.printed[name].bounding_box()
        assert bb.min.Z == pytest.approx(P.display_pcb_z_back + P.display_pcb_t)
        assert bb.max.Z == pytest.approx(P.internal_depth)
    # Plates keep >= 0.5 mm from the board edges (display PCB width +-0.3 mm) and the Teensy USB
    assert P.laser_inner_margin_x_neg >= 0.5 and P.laser_inner_margin_y_neg >= 1.0
    assert _inter_vol(_usb_c_boot(), laser.plates_3d["bottom"]) < 1e-4


LASER_BUILD_ORDER = ("bottom", "board", "top", "left", "right")  # Plates drop onto the back plate (-Z)


def test_laser_plates_drop_in_one_at_a_time(enc, laser, k) -> None:
    """Build order of the acrylic case: on the back plate and its standoffs, the bottom plate with
    the tripod mount, then the whole board stack, then top (capture key), left and right (switch
    key). Each plate, with what it carries, slides straight down onto the back plate past everything
    already in place."""
    stack = build_standoff_stack(P)
    board = {"pcb": k.pcb, **k.real, **{f"standoff_{i}": s for i, s in enumerate(stack.all)},
             "battery_cradle": enc.battery_cradle, "battery_floor": enc.battery_floor, "battery_lid": enc.battery_lid}
    carried = {"bottom": ["tripod_mount"], "top": ["capture_key"], "right": ["switch_key"], "left": []}
    parts = {**laser.plates_3d, **laser.printed}
    placed = {"back": laser.plates_3d["back"]}
    rise = P.laser_internal_depth + 2.0 * P.laser_t
    for step in LASER_BUILD_ORDER:
        if step == "board":
            # The board with Teensy, carrier and battery plugged in goes down onto the rear standoffs,
            # past the tripod mount's fin; the stack above it (mid standoffs, cradle, display) follows
            on_board = [k.pcb, *(s for n, s in k.real.items() if not n.startswith(("display", "battery")))]
            fixed = {**placed, **{f"rear_{i}": s for i, s in enumerate(stack.rear)}}
            for i in range(1, math.ceil(rise / 0.5) + 1):
                for s in on_board:
                    moved = _move(s, 0.0, 0.0, 0.5 * i)
                    hits = [n for n, o in fixed.items() if _inter_vol(moved, o) >= 1e-3]
                    assert not hits, f"board {0.5 * i:.1f} mm above its seat hits {hits}"
            placed |= board
            continue
        moving = [parts[step], *(parts[n] for n in carried[step])]
        for i in range(1, math.ceil(rise / 0.5) + 1):
            for s in moving:
                moved = _move(s, 0.0, 0.0, 0.5 * i)
                hits = [n for n, o in placed.items() if _inter_vol(moved, o) >= 1e-3]
                assert not hits, f"{step} {0.5 * i:.1f} mm above its seat hits {hits}"
        placed |= {step: parts[step], **{n: parts[n] for n in carried[step]}}


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


def test_printable_parts_lie_flat_on_the_plate(enc) -> None:
    parts = orient_for_printing(enc)
    assert set(parts) == set(PRINT_PARTS)
    for name, solid in parts.items():
        _assert_watertight(solid, f"print.{name}", min_vol=15.0)
        assert abs(solid.bounding_box().min.Z) <= 1e-3


def test_readme_names_the_parts_that_need_home_print_supports(enc, laser) -> None:
    """On an FDM printer a part needs supports when most of its down-facing faces start above the
    plate (lens_cap and battery_cradle: under 10% on it). The README's home-print line names every
    such part, as it does the back shell for its lens-cap track bridge."""
    from thermocam_case.export import orient_laser_parts

    text = (ENCLOSURE_ROOT / "README.md").read_text()
    home = next(line for line in text.splitlines() if line.startswith("- **At home:**"))
    needs_supports = home.split("need supports")[0]
    assert "`back_shell`" in needs_supports
    for name, solid in {**orient_for_printing(enc), **orient_laser_parts(laser, enc)}.items():
        down = [f for f in solid.faces() if f.normal_at().Z < -0.99]
        on_plate = sum(f.area for f in down if abs(f.center().Z) < 1e-3)
        if on_plate < 0.5 * sum(f.area for f in down):
            assert f"`{name}`" in needs_supports, f"{name} needs supports for home printing"


EXPORT_HINT = "run `uv run thermocam-case-export` and commit exports/"


def test_exported_artifacts_exist_and_non_empty() -> None:
    for path in expected_exports(ENCLOSURE_ROOT):
        assert path.exists(), f"Missing exported file: {path} ({EXPORT_HINT})"
        assert path.stat().st_size > 500, f"File too small: {path}"
        if path.suffix == ".step":
            assert "ISO-10303-21" in path.read_text(encoding="utf-8", errors="ignore")[:100]
        elif path.suffix == ".svg":
            assert ET.fromstring(path.read_text(encoding="utf-8")).tag.endswith("svg")
        elif path.suffix == ".dxf":
            txt = path.read_text(encoding="utf-8", errors="ignore")
            assert "ENTITIES" in txt and "EOF" in txt
        elif path.suffix == ".3mf":
            assert path.read_bytes()[:4] == b"PK\x03\x04"


def test_exported_parts_match_the_cad(enc, laser) -> None:
    """Every printed part's exported STEP has the volume of the current CAD part, so a stale export
    fails here rather than at the print service."""
    from build123d import import_step

    from thermocam_case.export import orient_laser_parts

    pre, root = P.file_prefix, ENCLOSURE_ROOT / "exports"
    cad = {**{n: s for n, s in orient_for_printing(enc).items() if n in PRINT_PARTS},
           **{n: s for n, s in orient_laser_parts(laser, enc).items() if n != "battery_cradle"}}
    for name, solid in cad.items():
        folder = "3d-printed" if name in PRINT_PARTS else "laser-cut"
        exported = import_step(str(root / folder / f"{pre}-{name}.step"))
        assert exported.volume == pytest.approx(solid.volume, rel=1e-4), f"{name}: stale export ({EXPORT_HINT})"


def test_laser_sheet_uses_the_cutting_services_colours() -> None:
    """Formulor (and services with its rules) read colour, not layer names: cuts pure blue
    RGB(0,0,255) hairlines, engraving pure red RGB(255,0,0) hairlines."""
    import ezdxf

    svg = (ENCLOSURE_ROOT / "exports" / "laser-cut" / f"{P.file_prefix}-laser-sheet.svg").read_text()
    groups = dict(re.findall(r'<g [^>]*stroke="(rgb\([^)]*\))"[^>]*id="(\w+)"', svg))
    assert {v: k for k, v in groups.items()} == {"cut": "rgb(0,0,255)", "engrave": "rgb(255,0,0)"}, groups
    assert set(re.findall(r'stroke-width="([^"]+)"', svg)) == {"0.01"}
    doc = ezdxf.readfile(ENCLOSURE_ROOT / "exports" / "laser-cut" / f"{P.file_prefix}-laser-sheet.dxf")
    assert (doc.layers.get("CUT").dxf.color, doc.layers.get("ENGRAVE").dxf.color) == (5, 1)  # ACI blue, red
    assert {e.dxf.layer for e in doc.modelspace()} == {"CUT", "ENGRAVE"}
    assert {e.dxf.color for e in doc.modelspace()} == {256}  # BYLAYER
    rings = sorted(round(e.dxf.radius, 2) for e in doc.modelspace().query('CIRCLE[layer=="ENGRAVE"]'))
    assert rings == [1.2, 2.7, 4.2], f"lens mark rings {rings}"  # Three lines, not one fused disc


def test_small_laser_parts_hang_on_bridges(laser) -> None:
    """Formulor: parts under 15 x 15 mm need the cut contour interrupted by 0.5..0.8 mm. The sheet
    holds each key twice (a spare pair); each keeps two bridges, and every other outline is closed.
    The capture key's bridges sit on its press face, so breaking it out loads the 5 mm neck, not
    the 1.2 mm hook arms, and leaves no stub on the neck's sides, which slide in a 0.3 mm fit."""
    from thermocam_case.laser_case import BRIDGE_W

    ends = [(v.X, v.Y) for e in laser.cut_edges for v in (e.start_point(), e.end_point())]
    free = [pt for i, pt in enumerate(ends) if not any(math.dist(pt, q) < 1e-3 for j, q in enumerate(ends) if j != i)]
    assert len(free) == 4 * 2 * 2, f"{len(free)} open ends"  # 4 keys x 2 bridges x 2 ends
    for pt in free:
        gap = min(math.dist(pt, q) for q in free if q != pt)
        assert gap == pytest.approx(BRIDGE_W, abs=1e-3)
    small = [f for f in laser.sheet_2d.faces() if max(f.bounding_box().size.X, f.bounding_box().size.Y) < 15.0]
    assert len(small) == 4
    capture = [f for f in small if f.bounding_box().size.X > f.bounding_box().size.Y]  # The 10 mm wide hook bar
    assert len(capture) == 2
    for f in capture:
        bb = f.bounding_box()
        mine = [pt for pt in free if bb.min.X - 1e-3 <= pt[0] <= bb.max.X + 1e-3 and bb.min.Y - 1e-3 <= pt[1] <= bb.max.Y + 1e-3]
        assert len(mine) == 4 and all(abs(y - bb.max.Y) < 1e-3 for _, y in mine), "capture-key bridges off its press face"
    # The README and the cutting service's ORDER.txt give the sheet's real extent
    size = laser.sheet_2d.bounding_box().size
    with zipfile.ZipFile(service_files(ENCLOSURE_ROOT)["laser_zip"]) as zf:
        order = " ".join(zf.read("ORDER.txt").decode().split())
    assert f"{size.X:.1f} × {size.Y:.1f} mm" in (ENCLOSURE_ROOT / "README.md").read_text()
    assert f"{size.X:.1f} x {size.Y:.1f} mm" in order


def test_service_files_hold_each_part_once() -> None:
    """Print services print one part per file: each upload zip holds every unique part once, as STEP
    and STL, each STEP one solid, and ORDER.txt gives the quantity of each."""
    files = service_files(ENCLOSURE_ROOT)
    expected = {"shell_zip": {n: PRINT_PARTS[n] for n in SHELL_SERVICE_PARTS}, "laser_zip": LASER_SERVICE_PARTS}
    for key, parts in expected.items():
        with zipfile.ZipFile(files[key]) as zf:
            listing, order = zf.namelist(), zf.read("ORDER.txt").decode()
            for name in parts:
                step = zf.read(f"step/{P.file_prefix}-{name}.step").decode()
                assert step.count("MANIFOLD_SOLID_BREP") == 1, f"{key}: {name}.step must hold one part"
        for fmt in ("step", "stl"):
            names = {n.removeprefix(f"{fmt}/{P.file_prefix}-").removesuffix(f".{fmt}")
                     for n in listing if n.startswith(f"{fmt}/")}
            assert names == set(parts), f"{key} {fmt}: {names}"
        for name, qty in parts.items():
            assert re.search(rf"^{name}\s+{qty}\b", order, re.MULTILINE), f"{key}: ORDER.txt lacks {name} x{qty}"


def test_readme_part_tables_match_the_service_files() -> None:
    """The README's printed-part tables list the same parts and quantities as the upload zips."""
    text = (ENCLOSURE_ROOT / "README.md").read_text()

    def table_after(heading: str) -> dict[str, int]:
        section = text.split(heading, 1)[1].split("\n## ", 1)[0]
        return {name: int(qty) for name, qty in re.findall(r"`(\w+)` \| (\d+) \|", section)}

    assert table_after("## Make the 3D-printed shell") == {n: PRINT_PARTS[n] for n in SHELL_SERVICE_PARTS}
    assert table_after("## Make the laser-cut case") == LASER_SERVICE_PARTS


def _gerber_extent(text: str) -> tuple[float, float]:
    """Width and height of a Gerber layer's drawn coordinates (KiCad: %FSLAX46Y46*%, mm)."""
    digits = int(re.search(r"%FSLAX\d(\d)Y", text)[1])  # type: ignore[index]
    pts = [(int(x) / 10**digits, int(y) / 10**digits) for x, y in re.findall(r"^X(-?\d+)Y(-?\d+)", text, re.MULTILINE)]
    xs, ys = [x for x, _ in pts], [y for _, y in pts]
    return max(xs) - min(xs), max(ys) - min(ys)


def test_fr4_boards_have_gerbers_with_outline() -> None:
    """Each FR4 board ships as a Gerber zip whose Edge.Cuts outline is the CAD board's size."""
    from thermocam_case.accessories import build_battery_floor, build_battery_lid

    for name, solid in (("battery_floor", build_battery_floor(P)), ("battery_lid", build_battery_lid(P))):
        path = ENCLOSURE_ROOT / "exports" / "fr4" / f"{P.file_prefix}-{name}-gerbers.zip"
        with zipfile.ZipFile(path) as zf:
            edge = [n for n in zf.namelist() if n.endswith("Edge_Cuts.gbr")]
            assert edge, f"{path.name}: no Edge_Cuts layer"
            w, h = _gerber_extent(zf.read(edge[0]).decode())
        size = solid.bounding_box().size
        assert (w, h) == pytest.approx((size.X, size.Y), abs=0.05), f"{name}: Gerber {w:.2f} x {h:.2f} vs CAD"


def test_pcb_edge_constants() -> None:
    assert PCB_TOP_EDGE_Y == pytest.approx(P.pcb_h)
