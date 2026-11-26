"""Animated 3D shots for the launch film, rendered from the enclosure's own CAD scene.

    blender -b --factory-startup -P video/blender/film_shots.py -- --shot assemble [--samples 48] \
            [--frames 0:120] [--scale 1.0]

Reuses hardware/enclosure/cad/blender/render_scene.py (the same GLBs, materials, studio lights and light
backdrop as the still renders) and animates on top of it: camera moves, parts flying into place, the
lens cap sliding open, and the display showing frames from the real Firmware 4.0 UI (video/public/sim).
Every frame is posed and rendered explicitly (no keyframes), so a shot can be rendered in parts.
Frames go to video/work/blender/<shot>/NNNN.png; scripts/encode-shots.sh turns them into clips.

Shot clocks are in seconds and were laid out against the narration's word timings (src/vo/film.json).
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "hardware/enclosure/cad/blender"))
import render_scene as rs  # noqa: E402

MANIFEST = ROOT / "hardware/enclosure/cad/build/scene/manifest.json"
SIM = ROOT / "video/public/sim"
WORK = ROOT / "video/work/blender"
FPS = 30
SENSOR_FPS = 9.0
MM = rs.MM


# ---------------------------------------------------------------------------------------------
# Easing
# ---------------------------------------------------------------------------------------------

def clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))


def ease_io(x: float) -> float:  # cubic in-out
    x = clamp01(x)
    return 4 * x * x * x if x < 0.5 else 1 - (-2 * x + 2) ** 3 / 2


def ease_out(x: float) -> float:  # expo-ish settle
    x = clamp01(x)
    return 1 - (1 - x) ** 4


def span(t: float, a: float, b: float, f=ease_io) -> float:
    return f((t - a) / (b - a)) if b > a else float(t >= b)


def lerp(a, b, k):
    return a + (b - a) * k


# ---------------------------------------------------------------------------------------------
# Screen content
# ---------------------------------------------------------------------------------------------

_images: dict[str, bpy.types.Image] = {}


def _image(path: Path) -> bpy.types.Image:
    key = str(path)
    if key not in _images:
        _images[key] = bpy.data.images.load(key)
    return _images[key]


def sim_frame(seq: str, t: float, loop: bool = True) -> Path:
    frames = sorted((SIM / seq).glob("*.png"))
    i = int(max(t, 0.0) * SENSOR_FPS)
    i = i % len(frames) if loop else min(i, len(frames) - 1)
    return frames[i]


def black_screen() -> Path:
    path = WORK / "screen-off.png"
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        img = bpy.data.images.new("screen-off", 32, 24)
        img.pixels = [0.004, 0.004, 0.005, 1.0] * (32 * 24)
        img.filepath_raw = str(path)
        img.file_format = "PNG"
        img.save()
    return path


def set_screen(path: Path) -> None:
    mat = bpy.data.materials["screen"]
    node = next(n for n in mat.node_tree.nodes if n.type == "TEX_IMAGE")
    node.image = _image(path)
    node.interpolation = "Closest"  # The panel's own pixels, not a blur


# ---------------------------------------------------------------------------------------------
# Posing
# ---------------------------------------------------------------------------------------------

@dataclass
class Pose:
    groups: list[str]
    build: str = "3d"
    pose: str = "upright"
    hide: list[str] = None  # type: ignore[assignment]


def pose_parts(parts: dict[str, rs.Part], p: Pose, offsets: Callable[[rs.Part], Vector]) -> list[rs.Part]:
    """Show the build's parts, each displaced by offsets(part) in mm (device frame)."""
    shot = rs.Shot("film", p.groups, build=p.build, pose=p.pose, hide=p.hide or [])
    pose = rs.POSES[p.pose]
    visible = []
    for part in parts.values():
        show = rs._matches(part, shot.groups) and not rs._matches(part, shot.hide) and rs._belongs_to_build(part, p.build)
        rs._set_visible(part, show)
        if show:
            part.obj.matrix_world = pose @ Matrix.Translation(offsets(part) * MM) @ rs._base(part)
            visible.append(part)
    bpy.context.view_layer.update()
    return visible


def bbox(parts: list[rs.Part], keys: list[str] | None = None) -> tuple[Vector, Vector]:
    chosen = [p for p in parts if keys is None or rs._matches(p, keys)]
    return rs._world_bbox(chosen)


# ---------------------------------------------------------------------------------------------
# Camera
# ---------------------------------------------------------------------------------------------

def view_dir(az: float, el: float) -> Vector:
    a, e = math.radians(az), math.radians(el)
    return Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)))


def fit_distance(lo: Vector, hi: Vector, az: float, el: float, lens: float, aspect: float,
                 margin: float) -> tuple[Vector, float]:
    """Target and distance that fit the box in frame (same maths as render_scene.place_camera)."""
    d = view_dir(az, el)
    rot = (-d).to_track_quat("-Z", "Y").to_matrix()
    right, up, fwd = rot.col[0], rot.col[1], -rot.col[2]
    sensor = 36.0
    tan_x = math.tan(math.atan(sensor / 2 / lens)) * (1 - margin)
    tan_y = tan_x / aspect
    center = (lo + hi) / 2
    corners = [Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
    xs = [(c - center).dot(right) for c in corners]
    ys = [(c - center).dot(up) for c in corners]
    center = center + right * (max(xs) + min(xs)) / 2 + up * (max(ys) + min(ys)) / 2
    dist = 0.0
    for c in corners:
        v = c - center
        depth = v.dot(fwd)
        dist = max(dist, depth + abs(v.dot(right)) / tan_x, depth + abs(v.dot(up)) / tan_y)
    return center, dist


@dataclass
class Cam:
    target: Vector
    az: float
    el: float
    dist: float
    lens: float = 70.0
    shift_y: float = 0.0


def mix_cam(a: Cam, b: Cam, k: float) -> Cam:
    return Cam(a.target.lerp(b.target, k), lerp(a.az, b.az, k), lerp(a.el, b.el, k),
               math.exp(lerp(math.log(a.dist), math.log(b.dist), k)), lerp(a.lens, b.lens, k),
               lerp(a.shift_y, b.shift_y, k))


def fit_cam(lo: Vector, hi: Vector, az: float, el: float, lens: float = 70.0, margin: float = 0.1,
            shift_y: float = 0.0) -> Cam:
    r = bpy.context.scene.render
    target, dist = fit_distance(lo, hi, az, el, lens, r.resolution_x / r.resolution_y, margin)
    return Cam(target, az, el, dist, lens, shift_y)


def apply_cam(c: Cam) -> None:
    scene = bpy.context.scene
    cam = scene.camera
    cam.data.lens = c.lens
    cam.data.shift_y = c.shift_y
    d = view_dir(c.az, c.el)
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam.location = c.target + d * c.dist


# ---------------------------------------------------------------------------------------------
# Scene setup
# ---------------------------------------------------------------------------------------------

def setup(parts: dict[str, rs.Part], studio_lo: Vector, studio_hi: Vector, res: tuple[int, int], samples: int,
          scale: float, floor: bool) -> None:
    for o in list(bpy.data.objects):
        if o.type in {"CAMERA", "LIGHT"} or o.name.startswith(("floor", "guides")):
            bpy.data.objects.remove(o, do_unlink=True)
    shot = rs.Shot("film", [], res=res)
    rs.setup_world("photo", False)
    rs.setup_studio((studio_lo + studio_hi) / 2, (studio_hi - studio_lo).length / 2, studio_lo.z)
    for o in bpy.data.objects:  # The floor only catches shadows; the film draws its own stage behind the device
        if o.name.startswith("floor"):
            o.hide_render = not floor
            o.is_shadow_catcher = True
    cam_data = bpy.data.cameras.new("cam")
    cam_data.sensor_fit = "HORIZONTAL"
    cam_data.sensor_width = 36.0
    cam_data.clip_start = 0.001
    cam_data.clip_end = 50
    cam = bpy.data.objects.new("cam", cam_data)
    bpy.context.scene.collection.objects.link(cam)
    bpy.context.scene.camera = cam
    rs.configure_render(shot, samples, scale)
    r = bpy.context.scene.render
    r.image_settings.file_format = "PNG"
    r.image_settings.color_mode = "RGBA"
    r.image_settings.compression = 15
    r.film_transparent = True
    bpy.context.scene.cycles.use_adaptive_sampling = True
    bpy.context.scene.cycles.adaptive_threshold = 0.02
    try:
        bpy.context.scene.cycles.denoiser = "OPENIMAGEDENOISE"
        bpy.context.scene.cycles.denoising_use_gpu = True
    except Exception:
        pass


def film_materials() -> None:
    """Close-up looks the stills never need: the Lepton window as coated germanium, not a flat colour."""
    mat = bpy.data.materials.get("lens_ar")
    if mat is None or not mat.use_nodes:
        return
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is None:
        return
    bsdf.inputs["Base Color"].default_value = (0.045, 0.045, 0.05, 1.0)
    bsdf.inputs["Metallic"].default_value = 1.0
    bsdf.inputs["Roughness"].default_value = 0.035
    for name, value in (("Thin Film Thickness", 420.0), ("Thin Film IOR", 1.7)):
        if name in bsdf.inputs:
            bsdf.inputs[name].default_value = value


SHELL = ["shell", "modules", "pcba"]
LASER = ["laser", "modules", "pcba"]


def everything(parts, groups, build="3d", pose="upright", hide=None):
    return pose_parts(parts, Pose(groups, build, pose, hide), lambda p: Vector((0, 0, 0)))


# ---------------------------------------------------------------------------------------------
# Shots
# ---------------------------------------------------------------------------------------------

class Shot:
    name = ""
    seconds = 1.0
    res = (1920, 1080)
    floor = True

    def __init__(self, parts):
        self.parts = parts

    def studio_box(self):
        return bbox(everything(self.parts, SHELL, hide=["cap_closed"]))

    def frame(self, t: float) -> None:
        raise NotImplementedError


class Open(Shot):
    """From the glowing screen, filling the frame, back and round to the three-quarter hero."""
    name, seconds = "open", 9.5

    def frame(self, t):
        vis = everything(self.parts, SHELL, hide=["cap_closed"])
        set_screen(sim_frame("live-mug", t + 2.0))
        slo, shi = bbox(vis, ["display_screen"])
        lo, hi = bbox(vis)
        a = fit_cam(slo, shi, -2, 2, lens=70, margin=0.02)
        b = fit_cam(lo, hi, -34, 13, lens=70, margin=0.31, shift_y=0.03)
        apply_cam(mix_cam(a, b, span(t, 0.4, 8.6)))


class Sensor(Shot):
    """Round to the back, the lens cap slides open, and in to the Lepton's lens."""
    name, seconds = "sensor", 11.2

    def frame(self, t):
        open_k = span(t, 4.6, 6.0)

        def off(p):
            return Vector((0, -24.2 * open_k, 0)) if rs._matches(p, ["cap_closed"]) else Vector((0, 0, 0))

        vis = pose_parts(self.parts, Pose(SHELL, hide=["cap_open"]), off)
        set_screen(sim_frame("live-mug", t + 4.0))
        lo, hi = bbox(vis)
        llo, lhi = bbox(vis, ["lepton_lens"])
        a = fit_cam(lo, hi, -34, 13, lens=70, margin=0.31, shift_y=0.03)
        b = fit_cam(lo, hi, 158, 16, lens=70, margin=0.31, shift_y=0.03)
        c = fit_cam(llo, lhi, 176, 6, lens=85, margin=0.0)
        c.dist *= 1.0 / 0.32  # leave the lens window and its surround in frame
        cam = mix_cam(a, b, span(t, 0.0, 5.0))
        cam = mix_cam(cam, c, span(t, 6.0, 11.2))
        apply_cam(cam)


# Where each group waits before it moves into place (mm along device Z, toward the display)
AWAY = {
    "lepton_fasteners": -110, "lepton_module": -110, "standoff_rear": -100, "rear_washers": -100, "teensy": -90,
    "screws_back": -200, "shell": -160,
    "standoff_mid": 120, "battery_stack": 140, "battery_spacers": 140, "lipo": 146, "display": 170,
    "front_shell": 210, "screws_front": 250,
}
# When each group starts to move (s); narration: Lepton 7.3, Teensy 8.3, battery 10.0, touchscreen 11.0,
# close the shell 13.3, eight screws 14.4 (stack-1 begins 6.8 s into the shot)
ARRIVE = [
    (["lepton_module", "lepton_fasteners", "standoff_rear", "rear_washers"], 7.0),
    (["teensy"], 8.0),
    (["standoff_mid", "battery_stack", "battery_spacers", "lipo"], 9.7),
    (["display"], 10.7),
    (["screws_back", "screws_front"], 14.1),  # before "shell": screws belong to that group too
    (["front_shell"], 12.9),
    (["shell"], 12.6),                        # back shell, buttons, lens cap and tripod nut rise from below
]
MOVE = 1.1


class Assemble(Shot):
    """The finished board turns in the light; then everything else stacks onto it."""
    name, seconds, floor = "assemble", 16.6, False

    def studio_box(self):
        return bbox(everything(self.parts, SHELL, pose="flat", hide=["cap_closed"]))

    def frame(self, t):
        def away(p):
            for g in p.groups:
                if g in AWAY:
                    return g, AWAY[g]
            return None, 0.0

        def start(p):
            for keys, s in ARRIVE:
                if rs._matches(p, keys):
                    return s
            return 0.0

        def off(p):
            g, z = away(p)
            if g is None:
                return Vector((0, 0, 0))
            k = 1.0 - span(t, start(p), start(p) + MOVE, ease_out)
            return Vector((0, 0, z * k))

        vis = pose_parts(self.parts, Pose(SHELL, pose="flat", hide=["cap_closed"]), off)
        set_screen(black_screen())
        assembled = everything(self.parts, SHELL, pose="flat", hide=["cap_closed"])
        lo, hi = bbox(assembled)
        blo, bhi = bbox(assembled, ["pcba"])
        vis = pose_parts(self.parts, Pose(SHELL, pose="flat", hide=["cap_closed"]), off)
        for p in vis:  # Parts wait out of sight until their cue, then fly in from off-frame
            if start(p) > t:
                rs._set_visible(p, False)
        board = fit_cam(blo, bhi, lerp(-78, -40, span(t, 0, 7.5)), lerp(46, 30, span(t, 0, 7.5)), lens=60, margin=0.24,
                        shift_y=0.07)
        wide_lo, wide_hi = lo - Vector((0, 0, 0.045)), hi + Vector((0, 0, 0.07))
        wide = fit_cam(wide_lo, wide_hi, -30, 16, lens=60, margin=0.16, shift_y=0.04)
        final = fit_cam(lo, hi, -24, 20, lens=60, margin=0.32, shift_y=0.02)
        cam = mix_cam(board, wide, span(t, 6.4, 9.0))
        cam = mix_cam(cam, final, span(t, 12.6, 16.4))
        apply_cam(cam)


class Turn(Shot):
    """A slow half-turn of one build, for the two cases side by side."""
    seconds, res = 7.6, (960, 1080)
    build = "3d"

    def studio_box(self):
        groups = SHELL if self.build == "3d" else LASER
        return bbox(everything(self.parts, groups, build=self.build, hide=["cap_closed"]))

    def frame(self, t):
        groups = SHELL if self.build == "3d" else LASER
        vis = everything(self.parts, groups, build=self.build, hide=["cap_closed"])
        set_screen(sim_frame("live-pcb" if self.build == "3d" else "use-window", t, loop=True))
        lo, hi = bbox(vis)
        az = lerp(-62, -8, span(t, 0, self.seconds))
        apply_cam(fit_cam(lo, hi, az, 14, lens=70, margin=0.18, shift_y=-0.02))


class TurnShell(Turn):
    name, build = "case-shell", "3d"


class TurnLaser(Turn):
    name, build = "case-laser", "laser"


class Boot(Shot):
    """Switched on: black, the boot splash, then the live image, while we push in to the screen."""
    name, seconds = "boot", 6.0

    def frame(self, t):
        vis = everything(self.parts, SHELL, hide=["cap_closed"])
        if t < 0.9:
            set_screen(black_screen())
        elif t < 3.0:
            set_screen(SIM / "boot/0000.png")
        else:
            set_screen(sim_frame("live-mug", t - 3.0))
        lo, hi = bbox(vis)
        slo, shi = bbox(vis, ["display_screen"])
        a = fit_cam(lo, hi, -20, 10, lens=70, margin=0.18, shift_y=0.04)
        b = fit_cam(slo, shi, 0, 0, lens=70, margin=0.0)
        apply_cam(mix_cam(a, b, span(t, 0.2, 6.0, lambda x: ease_io(x) ** 1.15)))


class End(Shot):
    """The end card's slow orbit."""
    name, seconds = "end", 10.0

    def frame(self, t):
        vis = everything(self.parts, SHELL, hide=["cap_closed"])
        set_screen(sim_frame("live-pcb", t + 2.0))
        lo, hi = bbox(vis)
        az = lerp(28, -36, span(t, 0, self.seconds, lambda x: x))
        apply_cam(fit_cam(lo, hi, az, 12, lens=70, margin=0.34, shift_y=0.02))


# How far each group stands off in the exploded view (mm along device Z, like AWAY)
SPREAD = {
    "screws_back": -122, "shell": -92, "lepton_fasteners": -48, "lepton_module": -48, "standoff_rear": -40,
    "rear_washers": -40, "teensy": -40,
    "standoff_mid": 30, "battery_stack": 46, "battery_spacers": 46, "lipo": 48, "display": 82,
    "front_shell": 116, "screws_front": 142,
}


class Explode(Shot):
    """The end: the finished camera opens into its layers, the screen still live, then closes again."""
    name, seconds, floor = "explode", 10.0, False

    def studio_box(self):
        return bbox(everything(self.parts, SHELL, pose="flat", hide=["cap_closed"]))

    def frame(self, t):
        k = span(t, 0.6, 2.8) * (1 - span(t, 6.0, 8.0))

        def off(p):
            for g in p.groups:
                if g in SPREAD:
                    return Vector((0, 0, SPREAD[g] * k))
            return Vector((0, 0, 0))

        flat = Pose(SHELL, pose="flat", hide=["cap_closed"])
        set_screen(sim_frame("live-mug", t + 1.0))
        lo, hi = bbox(pose_parts(self.parts, flat, lambda p: Vector((0, 0, 0))))
        elo, ehi = bbox(pose_parts(self.parts, flat, lambda p: Vector((0, 0, SPREAD.get(next((g for g in p.groups if g in SPREAD), ""), 0.0)))))
        pose_parts(self.parts, flat, off)
        closed = fit_cam(lo, hi, lerp(-26, -18, span(t, 6.0, 10.0)), 20, lens=60, margin=0.36, shift_y=-0.05)
        opened = fit_cam(elo, ehi, lerp(-36, -22, span(t, 1.0, 7.0)), 13, lens=60, margin=0.12, shift_y=0.0)
        apply_cam(mix_cam(closed, opened, k))


SHOTS = {s.name: s for s in (Open, Sensor, Assemble, TurnShell, TurnLaser, Boot, End, Explode)}


def main() -> None:
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--shot", required=True, choices=sorted(SHOTS))
    ap.add_argument("--samples", type=int, default=48)
    ap.add_argument("--scale", type=float, default=1.0)
    ap.add_argument("--frames", default="", help="start:end (end exclusive), default all")
    ap.add_argument("--step", type=int, default=1, help="Render every n-th frame (previews)")
    ap.add_argument("--out", type=Path, default=WORK)
    ap.add_argument("--project", default="", help="Comma-separated part groups: write their screen positions per "
                    "frame to <out>/<shot>/points.json instead of rendering (labels that follow the parts)")
    args = ap.parse_args(argv)

    for o in list(bpy.data.objects):  # The factory start-up scene's cube, light and camera
        bpy.data.objects.remove(o, do_unlink=True)
    parts, _ = rs.load_scene(MANIFEST)
    shot = SHOTS[args.shot](parts)
    slo, shi = shot.studio_box()
    setup(parts, slo, shi, shot.res, args.samples, args.scale, shot.floor)
    rs.apply_style(list(parts.values()), rs.Shot("film", []), {})
    film_materials()

    total = round(shot.seconds * FPS)
    a, b = 0, total
    if args.frames:
        s, e = args.frames.split(":")
        a, b = int(s or 0), min(int(e or total), total)
    out = args.out / shot.name
    out.mkdir(parents=True, exist_ok=True)
    scene = bpy.context.scene
    if args.project:
        from bpy_extras.object_utils import world_to_camera_view
        keys = args.project.split(",")
        w, h = shot.res
        points = []
        for f in range(a, b):
            shot.frame(f / FPS)
            bpy.context.view_layer.update()
            row = {}
            for key in keys:
                chosen = [p for p in parts.values() if rs._matches(p, [key]) and not p.obj.hide_render]
                if chosen:
                    lo, hi = rs._world_bbox(chosen)
                    v = world_to_camera_view(scene, scene.camera, (lo + hi) / 2)
                    row[key] = [round(v.x * w, 1), round((1 - v.y) * h, 1)]
            points.append(row)
        (out / "points.json").write_text(json.dumps(points))
        print(f"[film] {shot.name} points for {len(points)} frames", flush=True)
        return
    for f in range(a, b, args.step):
        target = out / f"{f:04d}.png"
        if target.exists():
            continue
        shot.frame(f / FPS)
        scene.render.filepath = str(target)
        bpy.ops.render.render(write_still=True)
        print(f"[film] {shot.name} {f + 1}/{total}", flush=True)


if __name__ == "__main__":
    main()
