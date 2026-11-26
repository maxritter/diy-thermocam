"""`thermocam-case-render`: rebuild every enclosure render and build-step sheet with Blender (headless).

    cd hardware/enclosure/cad
    uv run thermocam-case-render                      # all shots, final quality
    uv run thermocam-case-render --shots v4.0-3d-hero-front --samples 32 --scale 0.5   # quick preview

Needs Blender >= 4.2 (`brew install --cask blender`) and KiCad 10 (`kicad-cli`) for the board model.
Intermediate files go to `cad/build/` (git-ignored); finished images go to `hardware/enclosure/renders/`.
Build-step sheets are composed here: step number, a "you need" strip of part icons with counts,
the line-art drawing, and an optional close-up inset.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from .export import default_enclosure_root
from .scene import export_scene

BLENDER_CANDIDATES = ("blender", "/Applications/Blender.app/Contents/MacOS/Blender")
FONT_CANDIDATES = (
    "/System/Library/Fonts/Helvetica.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
)
INK, MUTED, RULE, BADGE = (31, 35, 41), (105, 112, 122), (222, 225, 230), (238, 120, 40)


def _blender() -> str:
    for c in BLENDER_CANDIDATES:
        if shutil.which(c) or Path(c).exists():
            return c
    raise FileNotFoundError("Blender not found; install it (macOS: brew install --cask blender)")


def _font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    for path in FONT_CANDIDATES:
        if Path(path).exists():
            try:
                return ImageFont.truetype(path, size, index=1 if bold and path.endswith(".ttc") else 0)
            except OSError:
                continue
    return ImageFont.load_default(size)


def _fit(img: Image.Image, box: int) -> Image.Image:
    """Trim transparent margins and scale into a box x box square."""
    bbox = img.getbbox()
    if bbox:
        img = img.crop(bbox)
    scale = min(box / img.width, box / img.height)
    return img.resize((max(1, int(img.width * scale)), max(1, int(img.height * scale))), Image.LANCZOS)


def compose_sheet(spec: dict[str, object], out_path: Path) -> None:
    main = Image.open(str(spec["raw"])).convert("RGB")
    w = main.width
    s = main.height / 1300  # Layout scales with the render (previews use --scale)
    pad, header_h, icon_box, tile_w = int(48 * s), int(310 * s), int(150 * s), int(210 * s)
    sheet = Image.new("RGB", (w, header_h + main.height), "white")
    draw = ImageDraw.Draw(sheet)

    # Step number badge
    badge = int(150 * s)
    draw.rounded_rectangle((pad, pad, pad + badge, pad + badge), radius=int(28 * s), fill=INK)
    f_num = _font(int(92 * s), bold=True)
    draw.text((pad + badge / 2, pad + badge / 2), str(spec["number"]), font=f_num, fill="white", anchor="mm")

    # "You need" strip: icon, count and label per part
    parts = list(spec["parts"])  # type: ignore[arg-type]
    x0 = pad + badge + int(56 * s)
    tile = min(tile_w, (w - x0 - pad) // max(1, len(parts)))
    f_count, f_label = _font(int(38 * s), bold=True), _font(int(24 * s))
    for i, part in enumerate(parts):
        x = x0 + i * tile
        icon = _fit(Image.open(part["icon"]).convert("RGBA"), min(icon_box, tile - int(24 * s)))
        sheet.paste(icon, (int(x + (tile - icon.width) / 2), int(pad + (icon_box - icon.height) / 2)), icon)
        draw.text((x + tile / 2, pad + icon_box + int(12 * s)), f"{part['count']}×", font=f_count, fill=BADGE,
                  anchor="mt")
        draw.text((x + tile / 2, pad + icon_box + int(58 * s)), str(part["label"]), font=f_label, fill=MUTED,
                  anchor="mt")
    draw.line((pad, header_h - 2, w - pad, header_h - 2), fill=RULE, width=max(2, int(3 * s)))
    sheet.paste(main, (0, header_h))

    if spec.get("inset"):  # Close-up in the free right column (the drawing is shifted left)
        inset = Image.open(str(spec["inset"])).convert("RGB")
        iw = int(w * 0.31)
        inset = inset.resize((iw, int(inset.height * iw / inset.width)), Image.LANCZOS)
        ix = w - iw - pad
        iy = header_h + (main.height - inset.height) // 2
        r = int(8 * s)
        draw.rounded_rectangle((ix - r, iy - r, ix + iw + r, iy + inset.height + r), radius=int(18 * s),
                               outline=RULE, width=max(2, int(4 * s)), fill="white")
        sheet.paste(inset, (ix, iy))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out_path, optimize=True)
    print(f"[sheet] {out_path}")


def main() -> None:
    root = default_enclosure_root()
    cad = root / "cad"
    ap = argparse.ArgumentParser(description="Render the DIY-Thermocam V4.0 enclosures with Blender")
    ap.add_argument("--shots", default="", help="Comma-separated shot or step names (default: all)")
    ap.add_argument("--samples", type=int, default=160, help="Cycles samples for photo shots")
    ap.add_argument("--scale", type=float, default=1.0, help="Resolution scale (0.5 for previews)")
    ap.add_argument("--out", type=Path, default=root / "renders")
    ap.add_argument("--skip-export", action="store_true", help="Reuse cad/build/scene from the last run")
    ap.add_argument("--save-blend", type=Path, default=None, help="Also save the last shot's .blend for tweaking")
    args = ap.parse_args()

    scene_dir, work = cad / "build" / "scene", cad / "build" / "render"
    manifest = scene_dir / "manifest.json"
    if not args.skip_export or not manifest.exists():
        manifest = export_scene(
            scene_dir,
            pcb_path=root.parent / "pcb" / "diy-thermocam-v4.0.kicad_pcb",
            screen_image=cad / "blender" / "assets" / "screen-live-view.png",
        )
    steps_json = work / "steps.json"
    steps_json.unlink(missing_ok=True)
    cmd = [
        _blender(), "-b", "--factory-startup", "-P", str(cad / "blender" / "render_scene.py"), "--",
        "--manifest", str(manifest), "--out", str(args.out), "--work", str(work),
        "--samples", str(args.samples), "--scale", str(args.scale),
    ]
    if args.shots:
        cmd += ["--shots", args.shots]
    if args.save_blend:
        cmd += ["--save-blend", str(args.save_blend)]
    subprocess.run(cmd, check=True)
    if steps_json.exists():
        for spec in json.loads(steps_json.read_text()):
            compose_sheet(spec, args.out / spec["file"])


if __name__ == "__main__":
    main()
