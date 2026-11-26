"""Export the full V4.0 camera (enclosures, hardware, modules, PCBA) as named GLB parts for Blender.

Every part keeps the shared camera frame (mm, X right / Y up / Z toward the display), so the
Blender script only assigns materials, offsets parts for exploded views and places cameras.
The PCBA comes from KiCad (`kicad-cli pcb export glb`) with the real component models.
"""

from __future__ import annotations

import json
import shutil
import subprocess
from pathlib import Path

from build123d import Compound, Solid, Unit, export_gltf, extrude
from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform
from OCP.TopLoc import TopLoc_Location

from .accessories import TRIPOD_FLOOR_T, laser_bottom_faces
from .decals import render_decal
from .hardware import (
    FRONT_SCREW_LEN,
    LASER_WASHER_T,
    build_shell_screws,
    build_standoff_stack,
    flat_washer,
    pan_head_screw,
    tripod_hex_nut,
)
from .laser_case import build_laser_enclosure
from .params import V4_PARAMS, EnclosureParams
from .printable_case import _shell_envelope, build_printable_enclosure
from .props import build_render_props

KICAD_CLI_CANDIDATES = (
    "kicad-cli",
    "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli",
)


def _kicad_cli() -> str:
    for c in KICAD_CLI_CANDIDATES:
        if shutil.which(c) or Path(c).exists():
            return c
    raise FileNotFoundError("kicad-cli not found; install KiCad 10 or add kicad-cli to PATH")


def _bake(solid: Solid | Compound) -> Solid | Compound:
    """Move a shape's location into its geometry.

    build123d's glTF writer emits a located shape (e.g. `Location(...) * Box(...)`) as a node
    transform without converting it to glTF's Y-up axes, so such parts land in the wrong place.
    """
    loc = solid.wrapped.Location()
    if loc.IsIdentity():
        return solid
    base = solid.wrapped.Located(TopLoc_Location())
    baked = BRepBuilderAPI_Transform(base, loc.Transformation(), True).Shape()
    return Solid(baked) if isinstance(solid, Solid) else Compound(baked)


def export_pcba_glb(pcb_path: Path, out_path: Path) -> Path:
    """Export the assembled V4.0 board with KiCad's 3D models (board origin, F.Cu top at Z=0)."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            _kicad_cli(), "pcb", "export", "glb", "--user-origin", "0x0mm",
            "--include-tracks", "--include-pads", "--include-zones",
            "--include-silkscreen", "--include-soldermask",
            "-f", "-o", str(out_path), str(pcb_path),
        ],
        check=True,
        capture_output=True,
    )
    # kicad-cli drops a project-local settings file next to the board; keep the repo clean
    pcb_path.with_suffix(".kicad_prl").unlink(missing_ok=True)
    return out_path


def export_scene(out_dir: Path, pcb_path: Path, screen_image: Path, p: EnclosureParams = V4_PARAMS) -> Path:
    """Write `<name>.glb` per part plus `manifest.json`; returns the manifest path."""
    out_dir.mkdir(parents=True, exist_ok=True)
    enc = build_printable_enclosure(p)
    laser = build_laser_enclosure(p)
    e = _shell_envelope(p)
    stack = build_standoff_stack(p)

    parts: list[dict[str, object]] = []

    def add(name: str, solid: Solid | Compound, material: str, groups: list[str],
            decal: str | None = None, view: tuple[str, str] = ("+X", "-Y")) -> None:
        path = out_dir / f"{name}.glb"
        solid = _bake(solid)
        export_gltf(solid, str(path), unit=Unit.MM, binary=True, linear_deflection=0.02, angular_deflection=0.15)
        bb = solid.bounding_box()
        spec: dict[str, object] = {
            "name": name, "file": path.name, "material": material, "groups": groups,
            "center_mm": [round(v, 3) for v in (bb.center().X, bb.center().Y, bb.center().Z)],
        }
        if decal:  # Planar XY texture over the part's own bounding box (see decals.py)
            tex = render_decal(decal, (bb.min.X, bb.min.Y, bb.max.X, bb.max.Y), view, out_dir / f"{name}.png")
            spec["texture"] = tex.name
        parts.append(spec)

    # Groups run most specific first: the Blender script takes the first group it has an offset for.
    # 3D-printed shell
    add("front_shell", enc.front_shell, "print_shell", ["shell"])
    add("back_shell", enc.back_shell, "print_shell", ["shell"])
    add("lens_cap_open", enc.lens_cap_open, "print_accent", ["cap_open", "shell"])
    add("lens_cap_closed", enc.lens_cap_closed, "print_accent", ["cap_closed", "shell"])
    add("capture_button", enc.capture_button, "print_accent", ["shell"])
    add("slide_switch_cap", enc.slide_switch_cap, "print_accent", ["shell"])
    add("battery_cradle", enc.battery_cradle, "print_inner", ["battery_stack", "shell", "laser"])
    add("battery_floor", enc.battery_floor, "fr4_black", ["battery_stack", "shell", "laser"])
    add("battery_lid", enc.battery_lid, "fr4_black", ["battery_stack", "shell", "laser"])

    # Purchased hardware (shared stack) + per-enclosure screws
    for tier, solids in (("rear", stack.rear), ("mid", stack.mid)):
        for i, s in enumerate(solids):
            add(f"standoff_{tier}_{i}", s, "nickel", [f"standoff_{tier}", "standoffs", "shell", "laser"])
    for i, s in enumerate(stack.washers):
        add(f"standoff_washer_{i}", s, "nylon_white", ["rear_washers", "standoffs", "shell", "laser"])
    for i, s in enumerate(build_shell_screws(p, e["z_back_outer"], e["z_front_outer"])):
        side = "back" if i < 4 else "front"
        add(f"shell_screw_{side}_{i % 4}", s, "steel", [f"screws_{side}", "shell_screws", "shell"])
    add("tripod_nut", tripod_hex_nut(p.tripod_x, p.shell_tripod_z, e["oy0"] + p.shell_tripod_floor_t), "zinc",
        ["shell"])

    # Laser-cut acrylic case + pan-head screws on nylon washers
    for name, plate in laser.plates_3d.items():
        is_key = name.endswith("_key")
        add(f"laser_{name}", plate, "acrylic_key" if is_key else "acrylic",
            ["laser_keys" if is_key else "laser_plates", "laser"])
    z0 = p.standoff_seat_z
    z_back_seat = z0 - p.laser_t - LASER_WASHER_T
    z_front_seat = p.internal_depth + p.laser_t + LASER_WASHER_T
    for i, (x, y) in enumerate(p.mounting_holes):
        add(f"laser_screw_back_{i}", pan_head_screw(x, y, z_back_seat, 8.0, pointing=+1), "steel",
            ["screws_back", "laser_screws", "laser"])
        add(f"laser_screw_front_{i}", pan_head_screw(x, y, z_front_seat, FRONT_SCREW_LEN, pointing=-1), "steel",
            ["screws_front", "laser_screws", "laser"])
        add(f"laser_washer_back_{i}", flat_washer(x, y, z_back_seat), "nylon_white",
            ["washers_back", "laser_washers", "laser"])
        add(f"laser_washer_front_{i}", flat_washer(x, y, z_front_seat - LASER_WASHER_T), "nylon_white",
            ["washers_front", "laser_washers", "laser"])
    # Laser-only printed parts: tripod mount (+ nut), microSD chute, display spacer bars
    printed_groups = {
        "tripod_mount": "laser_tripod", "sd_chute": "laser_chute",
        "display_spacer_bottom": "laser_spacers", "display_spacer_top": "laser_spacers",
    }
    for name, solid in laser.printed.items():
        add(f"laser_{name}", solid, "print_inner", [printed_groups[name], "laser"])
    _, y_out = laser_bottom_faces(p)
    add("laser_tripod_nut", tripod_hex_nut(p.laser_tripod_x, p.laser_tripod_z, y_out + TRIPOD_FLOOR_T, True), "zinc",
        ["laser_tripod", "laser"])

    # The cut acrylic parts as they come off the sheet (for the "laser sheet" product shot)
    for i, face in enumerate(laser.sheet_2d.faces()):
        add(f"sheet_part_{i}", _solid_of(extrude(face, amount=p.laser_t)), "acrylic", ["laser_sheet"])

    # Plug-in modules (kit-only extras such as the loose battery lead stay out of the assemblies)
    for prop in build_render_props(p):
        groups = ["lipo_lead"] if prop.kit_only else _module_groups(prop.name)
        add(prop.name, prop.solid, prop.material, groups, prop.decal, prop.view)

    pcba_path = export_pcba_glb(pcb_path, out_dir / "pcba.glb")
    holes = list(p.mounting_holes)
    pcb_top = p.pcb_z_back + p.pcb_t
    bat_top = p.battery.z_start + p.battery.thickness_z
    fbb = enc.battery_floor.bounding_box()
    floor_corners = [(x, y) for x in (fbb.min.X + 2.0, fbb.max.X - 2.0) for y in (fbb.min.Y + 2.0, fbb.max.Y - 2.0)]
    manifest = {
        "unit": "m",
        "parts": parts,
        "pcba": {"file": pcba_path.name, "offset_mm": [0.0, 0.0, pcb_top]},
        "screen_image": str(screen_image.resolve()),
        "camera_frame": {
            "outer_mm": [e["ox0"], e["oy0"], e["z_back_outer"], e["ox1"], e["oy1"], e["z_front_outer"]],
            "lepton_mm": [p.lepton_x, p.lepton_y],
        },
        # Points (device mm) where a big part meets the part below it; build-step guides start here
        "anchors_mm": {
            "pcba": [[x, y, p.pcb_z_back] for x, y in holes],
            "display": [[x, y, p.display_pcb_z_back] for x, y in holes],
            "front_shell": [[x, y, p.display_pcb_z_back + p.display_pcb_t] for x, y in holes],
            "battery_cradle": [[x, y, pcb_top] for x, y in holes],
            "battery_floor": [[x, y, p.battery.z_start - p.battery_floor_t] for x, y in floor_corners],
            "battery_lid": [[x, y, bat_top + p.battery_lid_gap] for x, y in holes],
            "laser_front": [[x, y, p.internal_depth] for x, y in holes],
            "laser_back": [[x, y, p.standoff_seat_z] for x, y in holes],
            "teensy": [[sx, sy, 11.5] for sx in (59.28, 74.52) for sy in (1.42, 59.84)],
            "lepton_module": [[x, y, pcb_top - p.pcb_t - p.lepton_sleeve_len] for x, y in p.lepton_breakout_holes],
        },
    }
    manifest_path = out_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=1))
    return manifest_path


def _solid_of(shape: object) -> Solid:
    return shape if isinstance(shape, Solid) else shape.solids()[0]  # type: ignore[attr-defined]


def _module_groups(name: str) -> list[str]:
    if name.startswith("lepton_"):
        kind = name.split("_")[1]  # spacer / washer / screw / nut / breakout / header / adapter / ...
        if kind in ("spacer", "washer", "screw", "nut"):
            return [f"lepton_{kind}s", "lepton_fasteners", "lepton", "modules"]
        return ["lepton_module", "lepton", "modules"]
    if name == "usb_c":
        return ["pcba", "modules"]  # Stand-in for the missing KiCad model; travels with the board
    for prefix in ("teensy", "display", "lipo"):
        if name.startswith(prefix):
            return [prefix, "modules"]
    return ["modules"]
