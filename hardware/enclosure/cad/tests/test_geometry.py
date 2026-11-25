"""Automated geometry, clearance, FOV, LiPo safety, assembly drop-in & export test suite.

Verifies ALL Carbon (#90/#169), Fable 5.2 (#176), and GPT-6.2 Astra (#182) P0/P1/P2 gates:
    cd hardware/enclosure/cad && uv run pytest
"""

from __future__ import annotations

import xml.etree.ElementTree as ET
import pytest
from build123d import Box, Location, Plane, RectangleRounded, Solid, extrude

from thermocam_case.export import default_enclosure_root, export_all
from thermocam_case.keepout import build_keepouts
from thermocam_case.laser_case import build_laser_enclosure
from thermocam_case.params import PRESETS, get_preset, parse_eagle_brd_metrics
from thermocam_case.printable_case import (
    _shell_envelope,
    build_printable_enclosure,
    orient_for_printing,
)

ENCLOSURE_ROOT = default_enclosure_root()
EAGLE_BRD_PATH = ENCLOSURE_ROOT.parent / "pcb" / "eagle-v3.0" / "pcb.brd"


def _inter_vol(a: Solid, b: Solid) -> float:
    inter = a & b
    return 0.0 if inter is None else float(sum(s.volume for s in inter.solids()))


def _assert_watertight(solid: Solid, label: str, min_vol: float = 40.0) -> None:
    assert isinstance(solid, Solid), f"{label} is not a Solid"
    assert solid.is_valid, f"{label} failed OCCT is_valid"
    assert solid.volume > min_vol, f"{label} volume too small: {solid.volume:.2f} mm^3"
    shells = solid.shells()
    assert len(shells) >= 1 and all(sh.is_valid for sh in shells)


def test_params_match_real_eagle_pcb_xml() -> None:
    metrics = parse_eagle_brd_metrics(EAGLE_BRD_PATH)
    p = get_preset("v3_0")
    assert abs(float(metrics["pcb_w"]) - p.pcb_w) <= 0.01
    assert abs(float(metrics["pcb_h"]) - p.pcb_h) <= 0.01
    assert sorted(p.mounting_holes) == metrics["mounting_holes"]
    assert sorted(p.lepton_breakout_holes) == metrics["lepton_holes"]
    teensy_x, _ = metrics["teensy_pos"]  # type: ignore[misc]
    assert abs(teensy_x - p.usb.center_x) <= 0.01
    # Fable/Astra P0-1: real ER-TFTM032-3 carrier PCB dimensions
    assert (p.display_pcb_w, p.display_pcb_h) == (89.40, 64.90)
    # Exact 39.20 mm standoff stack closure
    assert p.internal_depth == pytest.approx(39.20, abs=0.01)
    assert p.laser_internal_depth == pytest.approx(39.20, abs=0.01)


def test_v4_locked_pcb_coordinates_and_audit_specs() -> None:
    p = get_preset("v4_0")
    assert p.is_zero_solder is True
    assert p.usb.center_x == pytest.approx(45.80, abs=0.05)
    assert p.usb.overmold_width >= 14.0 and p.usb.overmold_height >= 7.8
    assert p.switch_y == pytest.approx(12.70, abs=0.05)
    assert p.button_x == pytest.approx(84.00, abs=0.05)
    assert p.back_face_led_holes == ((5.75, 66.40),)
    assert (p.teensy_prog_x, p.teensy_prog_y) == (85.30, 4.00)


@pytest.mark.parametrize("variant", sorted(PRESETS.keys()))
def test_printable_solids_valid_and_watertight(variant: str) -> None:
    p = get_preset(variant)
    enc = build_printable_enclosure(p)
    _assert_watertight(enc.front_shell, f"{variant}.front_shell", min_vol=15000.0)
    _assert_watertight(enc.back_shell, f"{variant}.back_shell", min_vol=25000.0)
    _assert_watertight(enc.lens_cap_closed, f"{variant}.lens_cap_closed", min_vol=200.0)
    _assert_watertight(enc.lens_cap_open, f"{variant}.lens_cap_open", min_vol=200.0)
    if p.is_zero_solder:
        assert enc.capture_button is not None
        assert enc.slide_switch_cap is not None
        assert enc.battery_cradle is not None
        assert enc.battery_lid is not None
        _assert_watertight(enc.capture_button, f"{variant}.capture_button", min_vol=100.0)
        _assert_watertight(enc.slide_switch_cap, f"{variant}.slide_switch_cap", min_vol=40.0)
        _assert_watertight(enc.battery_cradle, f"{variant}.battery_cradle", min_vol=1500.0)
        _assert_watertight(enc.battery_lid, f"{variant}.battery_lid", min_vol=1000.0)


@pytest.mark.parametrize("variant", sorted(PRESETS.keys()))
def test_printable_minimum_wall_thickness(variant: str) -> None:
    p = get_preset(variant)
    e = _shell_envelope(p)
    assert p.shell_wall >= 2.0 and p.shell_face_t >= 2.0
    assert (e["z_back_inner"] - e["z_track_floor"]) >= 2.0
    enc = build_printable_enclosure(p)
    for label, solid, probe_z in [
        ("back_shell_left", enc.back_shell, 10.0),
        ("front_shell_left", enc.front_shell, 32.0),
    ]:
        probe = Location((e["ox0"] - 1.0, 35.0, probe_z)) * Box(10.0, 2.0, 2.0)
        bb = (solid & probe).bounding_box()
        assert bb.size.X >= 2.0 - 1e-3, f"{label} wall {bb.size.X:.2f} mm < 2.0 mm"


@pytest.mark.parametrize("variant", sorted(PRESETS.keys()))
def test_printable_zero_interference_and_safety_gates(variant: str) -> None:
    """Verify 0.000 mm^3 interference across real 89.4x64.9 Display, 1.55 mm THT tails, swollen LiPo, 7.8 mm stud & drop-in."""
    p = get_preset(variant)
    enc = build_printable_enclosure(p)
    k = build_keepouts(p)

    checks = [
        ("front vs back", enc.front_shell, enc.back_shell),
        ("back vs cap_open", enc.back_shell, enc.lens_cap_open),
        ("back vs cap_closed", enc.back_shell, enc.lens_cap_closed),
        ("front vs pcb", enc.front_shell, k.pcb),
        ("back vs pcb", enc.back_shell, k.pcb),
        ("front vs display_pcb_89.4x64.9", enc.front_shell, k.display_pcb),
        ("back vs display_pcb_89.4x64.9", enc.back_shell, k.display_pcb),
        ("front vs display_panel", enc.front_shell, k.display_panel),
        ("front vs battery", enc.front_shell, k.battery),
        ("back vs battery", enc.back_shell, k.battery),
        ("battery vs pcb_tht_pins_1.55mm", k.battery, k.pcb_tht_pins),
        ("battery vs display_back_smds", k.battery, k.display_back_smds),
        ("back vs lepton_module", enc.back_shell, k.lepton_module),
        # Long 7.8 mm tripod stud on back_shell must NEVER touch PCB, Display, LiPo, Lepton, or roof!
        ("tripod_stud vs pcb", k.tripod_stud, k.pcb),
        ("tripod_stud vs display_pcb", k.tripod_stud, k.display_pcb),
        ("tripod_stud vs battery", k.tripod_stud, k.battery),
        ("tripod_stud vs lepton_module", k.tripod_stud, k.lepton_module),
        ("tripod_stud vs back_shell_blind_roof", k.tripod_stud, enc.back_shell),
    ]
    if p.is_zero_solder:
        assert enc.capture_button is not None
        assert enc.slide_switch_cap is not None
        assert enc.battery_cradle is not None
        assert enc.battery_lid is not None

        def _move(s: Solid, dx: float, dy: float, dz: float) -> Solid:
            m = Location((dx, dy, dz)) * s
            return m if isinstance(m, Solid) else m.solids()[0]

        checks.extend([
            ("capture_button vs back_shell", enc.capture_button, enc.back_shell),
            ("capture_button vs front_shell", enc.capture_button, enc.front_shell),
            ("capture_button vs pcb", enc.capture_button, k.pcb),
            # Astra P1-2: 0.15 mm rest gap above SW2 (Y=68.95); 0.50 mm stroke clears shelf & PCB!
            ("capture_button_pressed_0.50mm vs back_shell", _move(enc.capture_button, 0, -0.50, 0), enc.back_shell),
            ("capture_button_pressed_0.50mm vs pcb", _move(enc.capture_button, 0, -0.50, 0), k.pcb),
            # Astra P1-2: straight +Z drop-in must clear U-cradle!
            ("capture_button_dropin_dZ+3 vs back_shell", _move(enc.capture_button, 0, 0, 3.0), enc.back_shell),
            ("slide_switch_cap vs back_shell", enc.slide_switch_cap, enc.back_shell),
            ("slide_switch_cap vs front_shell", enc.slide_switch_cap, enc.front_shell),
            ("slide_switch_cap vs pcb", enc.slide_switch_cap, k.pcb),
            # Astra P0-3: straight +Z drop-in & +-1.65 mm throw (covers C&K +-1.25 mm) clear back_shell & PCB!
            ("slide_switch_cap_dropin_dZ+3 vs back_shell", _move(enc.slide_switch_cap, 0, 0, 3.0), enc.back_shell),
            ("slide_switch_cap_throw_+1.65 vs back_shell", _move(enc.slide_switch_cap, 0, 1.65, 0), enc.back_shell),
            ("slide_switch_cap_throw_-1.65 vs back_shell", _move(enc.slide_switch_cap, 0, -1.65, 0), enc.back_shell),
            ("slide_switch_cap_throw_+1.65 vs pcb", _move(enc.slide_switch_cap, 0, 1.65, 0), k.pcb),
            ("slide_switch_cap_throw_-1.65 vs pcb", _move(enc.slide_switch_cap, 0, -1.65, 0), k.pcb),
            # Fable/Astra P0-2: cradle + roof lid shield swollen LiPo from 1.60 mm tails & Display SMDs!
            ("battery_cradle vs pcb", enc.battery_cradle, k.pcb),
            ("battery_cradle vs pcb_tht_pins_1.60mm", enc.battery_cradle, k.pcb_tht_pins),
            ("battery_cradle vs battery", enc.battery_cradle, k.battery),
            ("battery_cradle vs front_shell", enc.battery_cradle, enc.front_shell),
            ("battery_cradle vs back_shell", enc.battery_cradle, enc.back_shell),
            ("battery_lid vs battery", enc.battery_lid, k.battery),
            ("battery_lid vs battery_cradle", enc.battery_lid, enc.battery_cradle),
            ("battery_lid vs display_back_smds", enc.battery_lid, k.display_back_smds),
            ("battery_lid vs front_shell", enc.battery_lid, enc.front_shell),
        ])

    for label, a, b in checks:
        vol = _inter_vol(a, b)
        assert vol < 1e-3, f"{variant} collision ({label}): {vol:.4f} mm^3"

    # Astra P1-4: Verify lens_cap cantilever detent bumps actively engage into back_shell floor notches!
    e = _shell_envelope(p)
    assert enc.lens_cap_open.bounding_box().max.Z > e["z_track_floor"] + 0.15
    assert enc.lens_cap_closed.bounding_box().max.Z > e["z_track_floor"] + 0.15
    if p.is_zero_solder and enc.slide_switch_cap is not None and enc.capture_button is not None:
        # Astra ENC-DELTA-P0-2: Fork reaches X=89.10 (>=0.90 mm knob overlap) & clears SW1 B.Fab body (X<=89.0)!
        assert 89.05 <= enc.slide_switch_cap.bounding_box().min.X <= 89.15
        sw1_body = Location((87.50, 12.70, 18.00)) * Box(3.00, 6.00, 1.70)
        assert _inter_vol(enc.slide_switch_cap, sw1_body) < 1e-4
        for dy in (-1.25, 1.25):
            assert _inter_vol(_move(enc.slide_switch_cap, 0, dy, 0), sw1_body) < 1e-4
            sw1_knob = Location((90.00, 12.70 + dy, 17.35)) * Box(2.00, 1.50, 1.00)
            assert _inter_vol(_move(enc.slide_switch_cap, 0, dy, 0), sw1_knob) < 1e-4
        # Astra ENC-DELTA-P1-1: Sharp R=1.0 mm rectangular 13.8x7.8 USB-C boot clears BOTH shells!
        boot_plane = Plane(origin=(45.80, -4.50, 18.40), x_dir=(1, 0, 0), z_dir=(0, -1, 0))
        usb_boot_r1 = extrude(boot_plane * RectangleRounded(13.80, 7.80, 1.00), amount=-4.20)
        usb_boot_r1 = usb_boot_r1 if isinstance(usb_boot_r1, Solid) else usb_boot_r1.solids()[0]
        assert _inter_vol(usb_boot_r1, enc.back_shell) < 1e-4
        assert _inter_vol(usb_boot_r1, enc.front_shell) < 1e-4
        # Astra P1-2: Rest tip sits at Y=69.10 (>=0.15 mm zero-preload gap above SW2 at Y=68.95)
        assert enc.capture_button.bounding_box().min.Y >= 69.09
        # Positive housing hard-stop engages back_shell shelf at 0.70 mm overtravel (prevents SW2 crush!)
        assert _inter_vol(_move(enc.capture_button, 0, -0.70, 0), enc.back_shell) > 0.5
        # Positive housing hard-stop engages right-wall slot at +-2.30 mm (prevents SW1 shear!)
        assert _inter_vol(_move(enc.slide_switch_cap, 0, 2.30, 0), enc.back_shell) > 0.1
    if not p.is_zero_solder:
        # Astra P1-6, P1-3, P2-2: V3 Prog PJRC coords, NKK rocker cutout & 5 mm LED bore
        assert (p.teensy_prog_x, p.teensy_prog_y) == (66.90, 43.08)
        assert p.switch_h_y >= 19.20 and p.switch_w_z >= 6.80
        assert p.shell_led_d >= 5.20 and p.laser_led_d >= 5.20


@pytest.mark.parametrize("variant", sorted(PRESETS.keys()))
def test_lepton_fov_cone_zero_clipping_both_optical_centers(variant: str) -> None:
    """71 deg DFOV (Lepton 3.5) AND 95 deg HFOV / 119 deg DFOV (Lepton 3.1R) clear BOTH centers!"""
    p = get_preset(variant)
    assert p.shell_lepton_inner_d >= 13.0
    assert p.laser_lepton_aperture_d >= 23.5
    enc = build_printable_enclosure(p)
    laser = build_laser_enclosure(p)
    k = build_keepouts(p)

    for cname, cone in (*k.lepton_fov_cones_all, *k.lepton_31r_fov_cones_all):
        assert _inter_vol(enc.back_shell, cone) < 1e-4, f"{variant} 3D back_shell clips {cname}"
        assert _inter_vol(enc.lens_cap_open, cone) < 1e-4, f"{variant} open lens_cap clips {cname}"
        assert _inter_vol(laser.plates_3d["back"], cone) < 1e-4, f"{variant} laser back clips {cname}"

    assert _inter_vol(enc.lens_cap_closed, k.lepton_fov_cone) > 50.0


@pytest.mark.parametrize("variant", sorted(PRESETS.keys()))
def test_printable_flat_support_free_orientation(variant: str) -> None:
    p = get_preset(variant)
    parts = orient_for_printing(build_printable_enclosure(p))
    for name, solid in parts.items():
        _assert_watertight(solid, f"{variant}.print.{name}", min_vol=40.0)
        assert abs(solid.bounding_box().min.Z) <= 1e-3


@pytest.mark.parametrize("variant", sorted(PRESETS.keys()))
def test_laser_plates_valid_and_zero_interference(variant: str) -> None:
    p = get_preset(variant)
    laser = build_laser_enclosure(p)
    k = build_keepouts(p)
    expected_keys = {"front", "back", "top", "bottom", "left", "right"}
    if p.is_zero_solder:
        expected_keys |= {"capture_key", "switch_key"}
    assert set(laser.plates_3d.keys()) == expected_keys
    for name, solid in laser.plates_3d.items():
        min_v = 80.0 if name.endswith("_key") else 3000.0
        _assert_watertight(solid, f"{variant}.laser.{name}", min_vol=min_v)
    names = list(laser.plates_3d.keys())
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            assert _inter_vol(laser.plates_3d[names[i]], laser.plates_3d[names[j]]) < 1e-3, (
                f"{variant} laser collision: {names[i]} vs {names[j]}"
            )
    for name, plate_solid in laser.plates_3d.items():
        for keep_solid in (k.pcb, k.display_pcb, k.display_panel, k.battery):
            assert _inter_vol(plate_solid, keep_solid) < 1e-3, f"{variant} laser {name} hits keepout"
    if p.is_zero_solder:
        # ENC-2D-P0-1 + ENC-DELTA-P0-2/P1-1: Working V4 acrylic capture_key & switch_key + hard-stop + R=1.0 USB!
        cap_k = laser.plates_3d["capture_key"]
        sw_k = laser.plates_3d["switch_key"]
        assert cap_k.bounding_box().min.Y >= 69.09  # 0.15 mm zero-preload above SW2 at Y=68.95
        assert 89.05 <= sw_k.bounding_box().min.X <= 89.15  # Clears SW1 B.Fab body (X<=89.0), >=0.9 mm knob overlap!
        # Positive outer shoulder hard-stop on top plate at -0.55 mm travel (0.000 mm^3 at -0.50, >0.5 at -0.70!)
        cap_50 = Location((0, -0.50, 0)) * cap_k
        cap_50 = cap_50 if isinstance(cap_50, Solid) else cap_50.solids()[0]
        cap_70 = Location((0, -0.70, 0)) * cap_k
        cap_70 = cap_70 if isinstance(cap_70, Solid) else cap_70.solids()[0]
        assert _inter_vol(cap_50, laser.plates_3d["top"]) < 1e-4
        assert _inter_vol(cap_70, laser.plates_3d["top"]) > 0.5
        # Zero SW1 B.Fab body & knob proxy interference across full throw!
        sw1_body = Location((87.50, 12.70, 18.00)) * Box(3.00, 6.00, 1.70)
        assert _inter_vol(sw_k, sw1_body) < 1e-4
        for dy in (-1.65, -1.0, 1.0, 1.65):
            moved_sw = Location((0, dy, 0)) * sw_k
            moved_sw = moved_sw if isinstance(moved_sw, Solid) else moved_sw.solids()[0]
            assert _inter_vol(moved_sw, laser.plates_3d["right"]) < 1e-3
            assert _inter_vol(moved_sw, k.pcb) < 1e-3
            assert _inter_vol(moved_sw, sw1_body) < 1e-4
            if abs(dy) <= 1.25:
                sw1_knob = Location((90.00, 12.70 + dy, 17.35)) * Box(2.00, 1.50, 1.00)
                assert _inter_vol(moved_sw, sw1_knob) < 1e-4
        # Sharp R=1.0 mm rectangular 13.8x7.8 USB-C boot clears laser bottom plate!
        boot_plane = Plane(origin=(45.80, -4.50, 18.40), x_dir=(1, 0, 0), z_dir=(0, -1, 0))
        usb_boot_r1 = extrude(boot_plane * RectangleRounded(13.80, 7.80, 1.00), amount=-4.20)
        usb_boot_r1 = usb_boot_r1 if isinstance(usb_boot_r1, Solid) else usb_boot_r1.solids()[0]
        assert _inter_vol(usb_boot_r1, laser.plates_3d["bottom"]) < 1e-4
    # ENC-2D-P1-4 + #221: Laser back plate clears 71 deg (3.5) AND 119 deg (3.1R) DFOV even at 3.20 mm stock!
    thick_back = extrude(laser.plates_2d["back"], amount=-3.20)
    thick_back = thick_back if isinstance(thick_back, Solid) else thick_back.solids()[0]
    for cname, cone in (*k.lepton_fov_cones_all, *k.lepton_31r_fov_cones_all):
        assert _inter_vol(thick_back, cone) < 1e-3, f"{variant} 3.2 mm laser back clips {cname} FOV!"


def test_exported_artifacts_exist_and_non_empty() -> None:
    exports_dir = ENCLOSURE_ROOT / "exports"
    renders_dir = ENCLOSURE_ROOT / "renders"
    v3_parts = ("front_shell", "back_shell", "lens_cap")
    v4_parts = (
        "front_shell", "back_shell", "lens_cap",
        "capture_button", "slide_switch_cap", "battery_cradle", "battery_lid",
    )

    expected = [
        exports_dir / "laser" / "diy-thermocam-v3.0-laser-sheet.svg",
        exports_dir / "laser" / "diy-thermocam-v3.0-laser-sheet.dxf",
        exports_dir / "laser" / "diy-thermocam-v3.0-laser-assembly.step",
        exports_dir / "laser" / "diy-thermocam-v4.0-laser-sheet.svg",
        exports_dir / "laser" / "diy-thermocam-v4.0-laser-sheet.dxf",
        exports_dir / "laser" / "diy-thermocam-v4.0-laser-assembly.step",
        exports_dir / "3d-v3.0" / "diy-thermocam-v3.0-assembly.step",
        exports_dir / "3d-v3.0" / "diy-thermocam-v3.0-print-plate.3mf",
        exports_dir / "3d-v4.0" / "diy-thermocam-v4.0-assembly.step",
        exports_dir / "3d-v4.0" / "diy-thermocam-v4.0-print-plate.3mf",
        renders_dir / "v3.0-3d-enclosure-hero.png",
        renders_dir / "v3.0-3d-enclosure-orthographic.png",
        renders_dir / "v3.0-laser-enclosure-preview.png",
        renders_dir / "v4.0-3d-enclosure-hero.png",
        renders_dir / "v4.0-3d-enclosure-orthographic.png",
        renders_dir / "v4.0-laser-enclosure-preview.png",
        renders_dir / "v4-assembly-step-1-board-modules.png",
        renders_dir / "v4-assembly-step-2-back-shell.png",
        renders_dir / "v4-assembly-step-3-display-stack.png",
        renders_dir / "v4-assembly-step-4-close-screws.png",
        renders_dir / "v4-assembly-overview.png",
    ]
    for pt in v3_parts:
        for ext in ("step", "stl", "3mf"):
            expected.append(exports_dir / "3d-v3.0" / f"diy-thermocam-v3.0-{pt}.{ext}")
    for pt in v4_parts:
        for ext in ("step", "stl", "3mf"):
            expected.append(exports_dir / "3d-v4.0" / f"diy-thermocam-v4.0-{pt}.{ext}")

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
            assert "CUT" in txt and "ENGRAVE" in txt
        elif path.suffix == ".3mf":
            assert path.read_bytes()[:4] == b"PK\x03\x04"
        elif path.suffix == ".png":
            assert path.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
