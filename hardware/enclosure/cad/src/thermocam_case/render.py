"""Studio-lit orthographic, isometric, and step-by-step exploded PNG renderer.

Uses build123d's exact OCCT B-Rep tessellation + NumPy adaptive triangle subdivision +
3-point Blinn-Phong studio shading + depth-sorted 2D orthographic projection in Matplotlib
to produce clean, high-DPI, KeyShot-style engineering renders on headless arm64 macOS / CI.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import textwrap
from typing import Sequence

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection
import numpy as np
from build123d import Location, Solid

from .keepout import KeepoutAssembly
from .laser_case import LaserEnclosure
from .printable_case import PrintableEnclosure


@dataclass(frozen=True)
class RenderItem:
    """One shaded solid with base RGB color (0..1) and optional alpha."""

    solid: Solid
    color: tuple[float, float, float]
    alpha: float = 1.0
    offset: tuple[float, float, float] = (0.0, 0.0, 0.0)


# Clean satin studio industrial-design color palette (readable contrast on #F6F7F9)
COL_FRONT_SHELL = (0.34, 0.39, 0.46)   # Satin slate grey
COL_BACK_SHELL = (0.25, 0.29, 0.36)    # Matte graphite
COL_ACCENT = (0.93, 0.43, 0.14)        # Thermal-orange accent (lens cap & V4 capture plunger)
COL_PCB = (0.14, 0.46, 0.28)           # Matte emerald solder mask
COL_DISPLAY = (0.10, 0.12, 0.16)       # Glossy dark TFT glass
COL_DISPLAY_PCB = (0.18, 0.36, 0.56)   # Blue BuyDisplay TFT PCB
COL_BATTERY = (0.78, 0.81, 0.86)       # Brushed silver LiPo pouch
COL_CRADLE = (0.58, 0.64, 0.72)        # Smooth printed LiPo protective cradle
COL_LEPTON = (0.17, 0.19, 0.24)        # Matte black FLIR Lepton module
COL_FOV = (0.08, 0.68, 0.90)           # Cool translucent cyan 71 deg optical FOV cone (#77 / P2-1)
COL_ACRYLIC_FB = (0.64, 0.69, 0.74)    # Acryl GS satin grey front/back
COL_ACRYLIC_SIDE = (0.52, 0.57, 0.63)  # Acryl GS medium grey side walls


def _camera_basis(azim_deg: float, elev_deg: float) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Return orthonormal (right, up, view_dir) vectors where camera looks from +view_dir -> origin."""
    az = np.radians(azim_deg)
    el = np.radians(elev_deg)
    view_dir = np.array([
        np.cos(el) * np.sin(az),
        np.sin(el),
        np.cos(el) * np.cos(az),
    ])
    view_dir /= np.linalg.norm(view_dir)
    world_up = np.array([0.0, 1.0, 0.0])
    if abs(np.dot(view_dir, world_up)) > 0.995:
        world_up = np.array([0.0, 0.0, -1.0 if view_dir[1] > 0 else 1.0])
    right = np.cross(world_up, view_dir)
    right /= np.linalg.norm(right)
    up = np.cross(view_dir, right)
    up /= np.linalg.norm(up)
    return right, up, view_dir


def _subdivide_long_triangles(tri_pts: np.ndarray, max_edge: float = 5.0, max_iters: int = 4) -> np.ndarray:
    """Adaptively split triangles with edges > max_edge for glitch-free painter's depth sorting."""
    pts = tri_pts
    for _ in range(max_iters):
        if len(pts) == 0:
            break
        l01 = np.linalg.norm(pts[:, 1] - pts[:, 0], axis=1)
        l12 = np.linalg.norm(pts[:, 2] - pts[:, 1], axis=1)
        l20 = np.linalg.norm(pts[:, 0] - pts[:, 2], axis=1)
        longest = np.maximum(l01, np.maximum(l12, l20))
        big = longest > max_edge
        if not np.any(big):
            break
        keep = pts[~big]
        sub = pts[big]
        # 1-to-4 midpoint subdivision preserves exact planar normal & seamless edges
        p0, p1, p2 = sub[:, 0], sub[:, 1], sub[:, 2]
        m01 = 0.5 * (p0 + p1)
        m12 = 0.5 * (p1 + p2)
        m20 = 0.5 * (p2 + p0)
        t1 = np.stack([p0, m01, m20], axis=1)
        t2 = np.stack([m01, p1, m12], axis=1)
        t3 = np.stack([m20, m12, p2], axis=1)
        t4 = np.stack([m01, m12, m20], axis=1)
        pts = np.concatenate([keep, t1, t2, t3, t4], axis=0)
    return pts


def _tessellate_item(item: RenderItem, tolerance: float = 0.08) -> tuple[np.ndarray, np.ndarray]:
    """Tessellate a solid with optional translation offset into (V[N,3], F[M,3])."""
    solid = item.solid
    if item.offset != (0.0, 0.0, 0.0):
        moved = Location(item.offset) * solid
        solid = moved if isinstance(moved, Solid) else moved.solids()[0]
    verts_raw, tris_raw = solid.tessellate(tolerance)
    verts = np.array([[v.X, v.Y, v.Z] for v in verts_raw], dtype=np.float64)
    tris = np.array(tris_raw, dtype=np.int32)
    return verts, tris


def render_view_on_ax(
    ax: plt.Axes,
    items: Sequence[RenderItem],
    *,
    azim_deg: float = 35.0,
    elev_deg: float = 24.0,
    title: str = "",
    subtitle: str = "",
    subtitle_wrap: int = 58,
    tolerance: float = 0.08,
    pad_frac: float = 0.07,
) -> None:
    """Render a list of RenderItems into a Matplotlib Axes with 3-point studio shading."""
    right, up, view_dir = _camera_basis(azim_deg, elev_deg)

    # Studio 3-point lighting directions in camera-relative space -> world space
    key_dir = 0.42 * right + 0.62 * up + 0.66 * view_dir
    key_dir /= np.linalg.norm(key_dir)
    fill_dir = -0.68 * right + 0.25 * up + 0.69 * view_dir
    fill_dir /= np.linalg.norm(fill_dir)
    rim_dir = -0.25 * right + 0.80 * up - 0.54 * view_dir
    rim_dir /= np.linalg.norm(rim_dir)
    half_vec = key_dir + view_dir
    half_vec /= np.linalg.norm(half_vec)

    all_polys: list[np.ndarray] = []
    all_depths: list[np.ndarray] = []
    all_rgba: list[np.ndarray] = []

    for item in items:
        verts, tris = _tessellate_item(item, tolerance=tolerance)
        if len(tris) == 0:
            continue
        tri_pts = verts[tris]
        e1 = tri_pts[:, 1] - tri_pts[:, 0]
        e2 = tri_pts[:, 2] - tri_pts[:, 0]
        normals = np.cross(e1, e2)
        norms = np.linalg.norm(normals, axis=1, keepdims=True)
        valid = norms[:, 0] > 1e-9
        tri_pts = tri_pts[valid]
        normals = normals[valid] / norms[valid]

        # Backface culling for opaque solids before subdividing (big speedup!)
        facing = normals @ view_dir
        if item.alpha >= 0.99:
            mask = facing > -0.01
            tri_pts = tri_pts[mask]
        if len(tri_pts) == 0:
            continue

        # Subdivide large planar triangles so painter's sort is exact even on thin walls
        tri_pts = _subdivide_long_triangles(tri_pts, max_edge=5.0, max_iters=4)
        e1 = tri_pts[:, 1] - tri_pts[:, 0]
        e2 = tri_pts[:, 2] - tri_pts[:, 0]
        normals = np.cross(e1, e2)
        norms = np.linalg.norm(normals, axis=1, keepdims=True)
        normals = normals / np.maximum(norms, 1e-12)
        if item.alpha < 0.99:
            facing = normals @ view_dir
            normals = np.where(facing[:, None] < 0, -normals, normals)

        # 3-point Lambertian + Blinn-Phong specular
        ndotl_key = np.clip(normals @ key_dir, 0.0, 1.0)
        ndotl_fill = np.clip(normals @ fill_dir, 0.0, 1.0)
        ndotl_rim = np.clip(normals @ rim_dir, 0.0, 1.0)
        spec = np.clip(normals @ half_vec, 0.0, 1.0) ** 24

        base = np.array(item.color, dtype=np.float64)[None, :]
        lit = (
            base * (0.30 + 0.64 * ndotl_key[:, None] + 0.24 * ndotl_fill[:, None])
            + np.array([[0.86, 0.91, 1.00]]) * (0.12 * ndotl_rim[:, None])
            + np.array([[1.0, 0.98, 0.95]]) * (0.16 * spec[:, None])
        )
        lit = np.clip(lit, 0.0, 1.0)
        alpha_col = np.full((len(lit), 1), item.alpha, dtype=np.float64)
        rgba = np.hstack([lit, alpha_col])

        px = tri_pts @ right
        py = tri_pts @ up
        pz = (tri_pts @ view_dir).mean(axis=1)
        polys_2d = np.stack([px, py], axis=-1)

        all_polys.append(polys_2d)
        all_depths.append(pz)
        all_rgba.append(rgba)

    if not all_polys:
        return

    polys = np.concatenate(all_polys, axis=0)
    depths = np.concatenate(all_depths, axis=0)
    colors = np.concatenate(all_rgba, axis=0)

    order = np.argsort(depths)
    polys = polys[order]
    colors = colors[order]

    # Matching face/edge colors eliminate internal triangulation seams on flat surfaces
    coll = PolyCollection(
        polys,
        facecolors=colors,
        edgecolors=colors,
        linewidths=0.35,
        antialiaseds=True,
    )
    ax.add_collection(coll)

    xmin, ymin = polys[:, :, 0].min(), polys[:, :, 1].min()
    xmax, ymax = polys[:, :, 0].max(), polys[:, :, 1].max()
    dx, dy = xmax - xmin, ymax - ymin
    cx, cy = 0.5 * (xmin + xmax), 0.5 * (ymin + ymax)
    ax.set_xlim(cx - dx * (0.5 + pad_frac), cx + dx * (0.5 + pad_frac))
    ax.set_ylim(cy - dy * (0.5 + pad_frac), cy + dy * (0.5 + pad_frac))
    ax.set_aspect("equal", adjustable="box")
    ax.axis("off")

    if title:
        ax.set_title(title, fontsize=11.0, fontweight="bold", color="#111827", pad=8)
    if subtitle:
        wrapped = textwrap.fill(subtitle, width=subtitle_wrap)
        ax.text(
            0.5,
            -0.04,
            wrapped,
            transform=ax.transAxes,
            ha="center",
            va="top",
            fontsize=8.2,
            color="#4B5563",
            linespacing=1.3,
        )


def _setup_figure(
    nrows: int,
    ncols: int,
    figsize: tuple[float, float],
    suptitle: str,
    footer: str = "",
) -> tuple[plt.Figure, np.ndarray]:
    fig, axes = plt.subplots(nrows, ncols, figsize=figsize, dpi=180)
    fig.patch.set_facecolor("#F6F7F9")
    axes_arr = np.atleast_1d(axes).ravel()
    for ax in axes_arr:
        ax.set_facecolor("#F6F7F9")
    fig.suptitle(suptitle, fontsize=13.5, fontweight="bold", color="#0F172A", y=0.97)
    if footer:
        wrapped_footer = textwrap.fill(footer, width=125)
        fig.text(
            0.5,
            0.015,
            wrapped_footer,
            ha="center",
            va="bottom",
            fontsize=8.2,
            color="#64748B",
        )
    return fig, axes_arr


def render_printable_multiview(
    enc: PrintableEnclosure,
    keepouts: KeepoutAssembly,
    out_path: Path,
) -> None:
    """Render a 2x2 studio hero sheet: Front Iso, Back Iso (Lens Cap), Exploded, and FOV Check."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    p = enc.params
    tag = p.version_tag

    assembled = [
        RenderItem(enc.front_shell, COL_FRONT_SHELL),
        RenderItem(enc.back_shell, COL_BACK_SHELL),
        RenderItem(enc.lens_cap_open, COL_ACCENT),
        RenderItem(keepouts.display_panel, COL_DISPLAY),
        RenderItem(keepouts.lepton_module, COL_LEPTON),
    ]
    if enc.capture_button is not None:
        assembled.append(RenderItem(enc.capture_button, COL_ACCENT))
    if enc.slide_switch_cap is not None:
        assembled.append(RenderItem(enc.slide_switch_cap, COL_ACCENT))

    assembled_closed = [
        RenderItem(enc.front_shell, COL_FRONT_SHELL),
        RenderItem(enc.back_shell, COL_BACK_SHELL),
        RenderItem(enc.lens_cap_closed, COL_ACCENT),
        RenderItem(keepouts.lepton_module, COL_LEPTON),
    ]
    if enc.capture_button is not None:
        assembled_closed.append(RenderItem(enc.capture_button, COL_ACCENT))
    if enc.slide_switch_cap is not None:
        assembled_closed.append(RenderItem(enc.slide_switch_cap, COL_ACCENT))

    exploded = [
        RenderItem(enc.lens_cap_open, COL_ACCENT, offset=(0, 0, -40)),
        RenderItem(enc.back_shell, COL_BACK_SHELL, offset=(0, 0, -24)),
        RenderItem(keepouts.lepton_module, COL_LEPTON, offset=(0, 0, -8)),
        RenderItem(keepouts.pcb, COL_PCB, offset=(0, 0, 0)),
        *(
            [RenderItem(enc.battery_cradle, COL_CRADLE, offset=(0, 0, 8))]
            if enc.battery_cradle is not None
            else []
        ),
        RenderItem(keepouts.battery, COL_BATTERY, offset=(0, 0, 14)),
        *(
            [RenderItem(enc.battery_lid, COL_CRADLE, offset=(0, 0, 19))]
            if enc.battery_lid is not None
            else []
        ),
        RenderItem(keepouts.display_pcb, COL_DISPLAY_PCB, offset=(0, 0, 27)),
        RenderItem(keepouts.display_panel, COL_DISPLAY, offset=(0, 0, 29)),
        RenderItem(enc.front_shell, COL_FRONT_SHELL, offset=(0, 0, 45)),
    ]
    if enc.capture_button is not None:
        exploded.append(RenderItem(enc.capture_button, COL_ACCENT, offset=(0, 16, -24)))
    if enc.slide_switch_cap is not None:
        exploded.append(RenderItem(enc.slide_switch_cap, COL_ACCENT, offset=(14, 0, -24)))

    fov_items = [
        RenderItem(enc.back_shell, COL_BACK_SHELL),
        RenderItem(enc.lens_cap_open, COL_ACCENT),
        RenderItem(keepouts.lepton_module, COL_LEPTON),
        RenderItem(keepouts.lepton_fov_cone, COL_FOV, alpha=0.25),
    ]

    footer_short = (
        f"USB: {p.usb.label}  |  Battery: {p.battery.name}  |  "
        "2.6 mm walls, support-free FDM / CNC, 71° (3.5) & 119° (3.1R) DFOV zero-vignette cone"
    )
    fig, axes = _setup_figure(
        2,
        2,
        figsize=(12.2, 9.6),
        suptitle=f"DIY-Thermocam {tag} — 3D-Printable / CNC Enclosure (build123d)",
        footer=footer_short,
    )

    render_view_on_ax(
        axes[0],
        assembled,
        azim_deg=34,
        elev_deg=22,
        title="Front Display Isometric",
        subtitle="3.2\" TFT chamfered bezel, flush M2.5 countersinks, switch & capture control",
    )
    render_view_on_ax(
        axes[1],
        assembled_closed,
        azim_deg=214,
        elev_deg=20,
        title="Rear Lepton Isometric (Slide Lens Cap Closed)",
        subtitle="Flush 45° dovetail shutter, Prog pinhole, microSD & USB cutouts, debossed branding",
    )
    render_view_on_ax(
        axes[2],
        exploded,
        azim_deg=42,
        elev_deg=24,
        title="Exploded Mechanical Stackup",
        subtitle="Back shell → Lepton & Main PCB → LiPo → 3.2\" TFT → Front shell",
    )
    render_view_on_ax(
        axes[3],
        fov_items,
        azim_deg=155,
        elev_deg=18,
        title="71° / 119° DFOV Optical Cone Check (Cyan Cone vs Open Cap)",
        subtitle="13.4→20.2 mm cone clears Lepton 3.5 (71°) & 3.1R (95° HFOV / 119° DFOV) w/ 0.00 mm³ clip",
    )

    fig.subplots_adjust(left=0.04, right=0.96, top=0.90, bottom=0.09, wspace=0.14, hspace=0.28)
    fig.savefig(out_path, dpi=180, facecolor=fig.get_facecolor())
    plt.close(fig)


def render_orthographic_sheet(
    enc: PrintableEnclosure,
    keepouts: KeepoutAssembly,
    out_path: Path,
) -> None:
    """Render 6-view orthographic engineering preview (Front, Back, Top, Bottom, Right, Left)."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    p = enc.params

    items = [
        RenderItem(enc.front_shell, COL_FRONT_SHELL),
        RenderItem(enc.back_shell, COL_BACK_SHELL),
        RenderItem(enc.lens_cap_open, COL_ACCENT),
        RenderItem(keepouts.display_panel, COL_DISPLAY),
        RenderItem(keepouts.lepton_module, COL_LEPTON),
    ]
    if enc.capture_button is not None:
        items.append(RenderItem(enc.capture_button, COL_ACCENT))
    if enc.slide_switch_cap is not None:
        items.append(RenderItem(enc.slide_switch_cap, COL_ACCENT))

    bb_f = enc.front_shell.bounding_box()
    bb_b = enc.back_shell.bounding_box()
    ox = max(bb_f.max.X, bb_b.max.X) - min(bb_f.min.X, bb_b.min.X)
    oy = max(bb_f.max.Y, bb_b.max.Y) - min(bb_f.min.Y, bb_b.min.Y)
    oz = max(bb_f.max.Z, bb_b.max.Z) - min(bb_f.min.Z, bb_b.min.Z)

    fig, axes = _setup_figure(
        2,
        3,
        figsize=(13.5, 8.6),
        suptitle=f"DIY-Thermocam {p.version_tag} — 6-View Orthographic CAD Inspection",
        footer=f"Outer envelope: {ox:.1f} × {oy:.1f} × {oz:.1f} mm  |  USB: {p.usb.label}  |  Battery: {p.battery.name}",
    )

    views = [
        (0, 0, "Front (Display Side)", "64.8 × 48.6 mm bevel window + 4× M2.5"),
        (180, 0, "Back (Lepton Side, Cap Open)", "13.4→20.2 mm 3.1R/3.5 cone, Prog pinhole, thermal-lens mark"),
        (0, 89.5, "Top Wall", "Capture button, microSD scoop, charge LED"),
        (0, -89.5, "Bottom Wall", f"{p.usb.label} + back-shell 1/4\"-20 blind tripod nut"),
        (90, 0, "Right Wall (+X)", "Power switch cutout"),
        (-90, 0, "Left Wall (-X)", "Chamfered grip side w/ subtle grip ribs"),
    ]
    for ax, (az, el, title, sub) in zip(axes, views):
        render_view_on_ax(ax, items, azim_deg=az, elev_deg=el, title=title, subtitle=sub, subtitle_wrap=40)

    fig.subplots_adjust(left=0.04, right=0.96, top=0.89, bottom=0.09, wspace=0.14, hspace=0.30)
    fig.savefig(out_path, dpi=180, facecolor=fig.get_facecolor())
    plt.close(fig)


def render_laser_preview(
    laser: LaserEnclosure,
    keepouts: KeepoutAssembly,
    out_path: Path,
) -> None:
    """Render 3D assembled + exploded + 2D flat cutting sheet preview for the 6-plate acrylic case."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    p = laser.params

    plates_assembled = [
        RenderItem(laser.plates_3d["front"], COL_ACRYLIC_FB),
        RenderItem(laser.plates_3d["back"], COL_ACRYLIC_FB),
        RenderItem(laser.plates_3d["top"], COL_ACRYLIC_SIDE),
        RenderItem(laser.plates_3d["bottom"], COL_ACRYLIC_SIDE),
        RenderItem(laser.plates_3d["left"], COL_ACRYLIC_SIDE),
        RenderItem(laser.plates_3d["right"], COL_ACRYLIC_SIDE),
        *(
            [
                RenderItem(laser.plates_3d["capture_key"], COL_ACCENT),
                RenderItem(laser.plates_3d["switch_key"], COL_ACCENT),
            ]
            if "capture_key" in laser.plates_3d
            else []
        ),
        RenderItem(keepouts.pcb, COL_PCB),
        RenderItem(keepouts.display_panel, COL_DISPLAY),
    ]

    plates_exploded = [
        RenderItem(laser.plates_3d["back"], COL_ACRYLIC_FB, offset=(0, 0, -18)),
        RenderItem(laser.plates_3d["front"], COL_ACRYLIC_FB, offset=(0, 0, 18)),
        RenderItem(laser.plates_3d["top"], COL_ACRYLIC_SIDE, offset=(0, 14, 0)),
        RenderItem(laser.plates_3d["bottom"], COL_ACRYLIC_SIDE, offset=(0, -14, 0)),
        RenderItem(laser.plates_3d["left"], COL_ACRYLIC_SIDE, offset=(-14, 0, 0)),
        RenderItem(laser.plates_3d["right"], COL_ACRYLIC_SIDE, offset=(14, 0, 0)),
        *(
            [
                RenderItem(laser.plates_3d["capture_key"], COL_ACCENT, offset=(0, 22, 0)),
                RenderItem(laser.plates_3d["switch_key"], COL_ACCENT, offset=(22, 0, 0)),
            ]
            if "capture_key" in laser.plates_3d
            else []
        ),
        RenderItem(keepouts.pcb, COL_PCB),
    ]

    fig, axes = _setup_figure(
        1,
        3,
        figsize=(14.6, 5.8),
        suptitle=f"DIY-Thermocam {p.version_tag} — Improved 6-Plate 3 mm Laser-Cut Acrylic Enclosure",
        footer=(
            f"23.8 mm Lepton aperture (zero 71°/119° DFOV clip at 3.2 mm acrylic), aligned {p.usb.label}, "
            f"3.20 mm cast-acrylic slots, {p.laser_kerf:.2f} mm kerf, separate blue ENGRAVE layer"
        ),
    )

    render_view_on_ax(
        axes[0],
        plates_assembled,
        azim_deg=35,
        elev_deg=22,
        title="Assembled 6-Plate Front View",
        subtitle="Interlocking finger joints, 3.0 mm Acryl GS (39.2 mm stack)",
        subtitle_wrap=42,
    )
    render_view_on_ax(
        axes[1],
        plates_exploded,
        azim_deg=218,
        elev_deg=22,
        title="Exploded Rear View (Lepton Side)",
        subtitle="23.8 mm Lepton 3.1R/3.5 aperture + Prog pinhole + rotated tripod flange",
        subtitle_wrap=42,
    )

    ax2 = axes[2]
    for edge in laser.sheet_2d.edges():
        n = 24 if str(edge.geom_type) != "GeomType.LINE" else 2
        pts = edge.positions([i / n for i in range(n + 1)])
        ax2.plot([pt.X for pt in pts], [pt.Y for pt in pts], color="#CC0000", linewidth=0.95)
    for edge in laser.engrave_2d.edges():
        n = 24 if str(edge.geom_type) != "GeomType.LINE" else 2
        pts = edge.positions([i / n for i in range(n + 1)])
        ax2.plot([pt.X for pt in pts], [pt.Y for pt in pts], color="#0066FF", linewidth=0.75)
    for label, lx, ly in laser.sheet_labels:
        ax2.text(lx, ly, label, ha="center", va="top", fontsize=7.5, fontweight="bold", color="#334155")
    ax2.set_aspect("equal", adjustable="box")
    ax2.set_title("2D Laser Cutting Sheet (SVG / DXF)", fontsize=11.0, fontweight="bold", color="#111827", pad=8)
    ax2.set_xlabel("mm", fontsize=8, color="#64748B")
    ax2.tick_params(labelsize=7.5, colors="#64748B")
    for spine in ("top", "right"):
        ax2.spines[spine].set_visible(False)

    fig.subplots_adjust(left=0.03, right=0.98, top=0.84, bottom=0.16, wspace=0.16)
    fig.savefig(out_path, dpi=180, facecolor=fig.get_facecolor())
    plt.close(fig)


def render_v4_assembly_steps(
    enc: PrintableEnclosure,
    keepouts: KeepoutAssembly,
    renders_dir: Path,
) -> list[Path]:
    """Generate crisp step-by-step 3D exploded assembly renders for V4 zero-solder build."""
    renders_dir.mkdir(parents=True, exist_ok=True)
    out_paths: list[Path] = []

    steps = [
        (
            "v4-assembly-step-1-board-modules.png",
            "Step 1 — Plug Modules into Factory-PCBA V4 Board",
            "Snap Teensy 4.1 (pre-pinned), FLIR Lepton 3.5 + Breakout V2, "
            "and slim 505060 JST-PH LiPo into the pre-soldered sockets — zero soldering.",
            [
                RenderItem(keepouts.pcb, COL_PCB, offset=(0, 0, 0)),
                RenderItem(keepouts.lepton_module, COL_LEPTON, offset=(0, 0, -12)),
                RenderItem(keepouts.battery, COL_BATTERY, offset=(0, 0, 12)),
            ],
            38,
            24,
        ),
        (
            "v4-assembly-step-2-back-shell.png",
            "Step 2 — Slide Nut, Actuators, Board & LiPo Cradle into Back Shell",
            "Slide 1/4\"-20 hex nut into back shell's blind-roofed pocket, insert lens cap, "
            "capture plunger & switch shuttle, seat V4 board on M2.5×20 standoffs & flat LiPo cradle.",
            [
                RenderItem(enc.lens_cap_open, COL_ACCENT, offset=(0, 14, -14)),
                RenderItem(enc.back_shell, COL_BACK_SHELL, offset=(0, 0, 0)),
                *(
                    [RenderItem(enc.capture_button, COL_ACCENT, offset=(0, 11, 0))]
                    if enc.capture_button is not None
                    else []
                ),
                *(
                    [RenderItem(enc.slide_switch_cap, COL_ACCENT, offset=(11, 0, 0))]
                    if enc.slide_switch_cap is not None
                    else []
                ),
                RenderItem(keepouts.lepton_module, COL_LEPTON, offset=(0, 0, 16)),
                RenderItem(keepouts.pcb, COL_PCB, offset=(0, 0, 20)),
                *(
                    [RenderItem(enc.battery_cradle, COL_CRADLE, offset=(0, 0, 25))]
                    if enc.battery_cradle is not None
                    else []
                ),
                RenderItem(keepouts.battery, COL_BATTERY, offset=(0, 0, 31)),
                *(
                    [RenderItem(enc.battery_lid, COL_CRADLE, offset=(0, 0, 36))]
                    if enc.battery_lid is not None
                    else []
                ),
            ],
            215,
            22,
        ),
        (
            "v4-assembly-step-3-display-stack.png",
            "Step 3 — Place Insulating LiPo Lid & Plug 3.2\" TFT Display",
            "Seat 0.6 mm LiPo roof lid over 4× M2.5×11 standoffs (shielding cell from TFT SMDs) "
            "and plug the 89.4×64.9 mm BuyDisplay ER-TFTM032-3 onto the 2×20 header.",
            [
                RenderItem(enc.back_shell, COL_BACK_SHELL, offset=(0, 0, 0)),
                RenderItem(enc.lens_cap_open, COL_ACCENT, offset=(0, 0, 0)),
                *(
                    [RenderItem(enc.capture_button, COL_ACCENT, offset=(0, 0, 0))]
                    if enc.capture_button is not None
                    else []
                ),
                *(
                    [RenderItem(enc.slide_switch_cap, COL_ACCENT, offset=(0, 0, 0))]
                    if enc.slide_switch_cap is not None
                    else []
                ),
                RenderItem(keepouts.lepton_module, COL_LEPTON, offset=(0, 0, 0)),
                RenderItem(keepouts.pcb, COL_PCB, offset=(0, 0, 0)),
                *(
                    [RenderItem(enc.battery_cradle, COL_CRADLE, offset=(0, 0, 0))]
                    if enc.battery_cradle is not None
                    else []
                ),
                RenderItem(keepouts.battery, COL_BATTERY, offset=(0, 0, 0)),
                *(
                    [RenderItem(enc.battery_lid, COL_CRADLE, offset=(0, 0, 6))]
                    if enc.battery_lid is not None
                    else []
                ),
                RenderItem(keepouts.display_pcb, COL_DISPLAY_PCB, offset=(0, 0, 18)),
                RenderItem(keepouts.display_panel, COL_DISPLAY, offset=(0, 0, 20)),
            ],
            36,
            22,
        ),
        (
            "v4-assembly-step-4-close-screws.png",
            "Step 4 — Close Front Shell & Tighten 8× M2.5 Countersunk Screws",
            "Align the seam tabs and tighten 4× M2.5×6 front + 4× M2.5×8 back flat-head "
            "screws into the brass standoffs — a standard screwdriver is the only tool needed.",
            [
                RenderItem(enc.back_shell, COL_BACK_SHELL, offset=(0, 0, -8)),
                RenderItem(enc.lens_cap_open, COL_ACCENT, offset=(0, 0, -8)),
                *(
                    [RenderItem(enc.capture_button, COL_ACCENT, offset=(0, 0, -8))]
                    if enc.capture_button is not None
                    else []
                ),
                *(
                    [RenderItem(enc.slide_switch_cap, COL_ACCENT, offset=(0, 0, -8))]
                    if enc.slide_switch_cap is not None
                    else []
                ),
                RenderItem(keepouts.pcb, COL_PCB, offset=(0, 0, -5)),
                RenderItem(keepouts.display_pcb, COL_DISPLAY_PCB, offset=(0, 0, 4)),
                RenderItem(keepouts.display_panel, COL_DISPLAY, offset=(0, 0, 6)),
                RenderItem(enc.front_shell, COL_FRONT_SHELL, offset=(0, 0, 22)),
            ],
            34,
            20,
        ),
    ]

    for filename, title, subtitle, items, az, el in steps:
        path = renders_dir / filename
        fig, axes = _setup_figure(1, 1, figsize=(9.2, 7.0), suptitle=title, footer=subtitle)
        render_view_on_ax(axes[0], items, azim_deg=az, elev_deg=el, pad_frac=0.08)
        fig.subplots_adjust(left=0.05, right=0.95, top=0.90, bottom=0.10)
        fig.savefig(path, dpi=180, facecolor=fig.get_facecolor())
        plt.close(fig)
        out_paths.append(path)

    overview_path = renders_dir / "v4-assembly-overview.png"
    fig, axes = _setup_figure(
        2,
        2,
        figsize=(13.6, 10.2),
        suptitle="DIY-Thermocam V4.0 — Zero-Solder, Screwdriver-Only Assembly Guide",
        footer="Factory PCBA + 3D-printed shell (Seeed Studio Fusion / JLCPCB)  |  Only tool required: M2.5 screwdriver",
    )
    for ax, (_, title, subtitle, items, az, el) in zip(axes, steps):
        render_view_on_ax(
            ax,
            items,
            azim_deg=az,
            elev_deg=el,
            title=title,
            subtitle=subtitle,
            subtitle_wrap=56,
            pad_frac=0.07,
        )
    fig.subplots_adjust(left=0.04, right=0.96, top=0.90, bottom=0.10, wspace=0.16, hspace=0.34)
    fig.savefig(overview_path, dpi=180, facecolor=fig.get_facecolor())
    plt.close(fig)
    out_paths.append(overview_path)

    return out_paths
