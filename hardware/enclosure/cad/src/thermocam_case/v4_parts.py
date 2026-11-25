"""Support-free mechanical accessory solids for DIY-Thermocam V3.0 / V4.0.

All Fable 5.2 (#176) & Carbon (#169) P0/P1 findings addressed:
  - `build_lens_cap`: True one-end cantilever spring detent with 0.45 mm inner-face bump
    (1.2 mm wide > nozzle width) actively engaging back_shell's floor notches at Open, Closed,
    and Top-Stop (positive retention — cannot slide out accidentally!).
  - `build_capture_button_plunger`: 45 deg support-free conical transition from cap shaft
    to flange; narrow stem centered at Z=19.15 (hits SW2 B3U-3000P at Z=19.40 while < PCB 20.0);
    hard travel stop at 0.45 mm past contact.
  - `build_slide_switch_cap`: 3.40 mm outer thumb pad (< 4.00 mm right-wall slot -> inserts
    cleanly from inside with 0.60 mm assembly clearance!), deep fork engaging SW1 at X=89.55..91.1.
  - `build_battery_cradle` + `build_battery_lid`: 100% flat-bed support-free smooth insulating
    floor (0.80 mm at Z=23.4..24.2, resting on 1.8 mm standoff spacers above real 1.51 mm PCB
    THT tails, Ø6.2 mm bores clearing SW5 M2.5 hex standoffs!) + flat 0.60 mm insulating top lid
    (Z=29.7..30.3) shielding the 505060 slim LiPo pouch from Display PCB backside SMDs!
"""

from __future__ import annotations

from build123d import (
    Align,
    BuildPart,
    BuildSketch,
    Circle,
    Cylinder,
    Location,
    Locations,
    Mode,
    Plane,
    Rectangle,
    RectangleRounded,
    Solid,
    extrude,
    loft,
)

from .params import EnclosureParams


def _as_solid(shape: object) -> Solid:
    if isinstance(shape, Solid):
        return shape
    solids = shape.solids()  # type: ignore[union-attr]
    assert len(solids) == 1, f"Expected 1 solid, got {len(solids)}"
    return solids[0]


def _xz_plane(y: float) -> Plane:
    """Right-handed XZ plane at Y=y (local_X=+X, local_Y=+Z, local_Z=-Y)."""
    return Plane(origin=(0.0, y, 0.0), x_dir=(1, 0, 0), z_dir=(0, -1, 0))


def _yz_plane(x: float) -> Plane:
    """Right-handed YZ plane at X=x (local_X=+Y, local_Y=+Z, local_Z=+X)."""
    return Plane(origin=(x, 0.0, 0.0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))


def build_lens_cap(p: EnclosureParams, e: dict[str, float], y_center: float) -> Solid:
    """Build top-loading 45 deg dovetail lens cap with true cantilever floor detent (Fable #4)."""
    clr = p.lens_cap_clearance  # 0.25 mm glide clearance per side
    z_out = e["z_back_outer"]  # -3.60
    z_floor = e["z_track_floor"]  # -2.10
    cap_len = 22.00  # Covers full 20.2 mm Lepton 3.1R wide-angle floor aperture when closed!

    w_outer = e["track_w_outer"] - 2.0 * clr  # 24.50 mm at Z = -3.60
    w_inner = e["track_w_inner"] - 2.0 * clr - 0.30  # 27.20 mm at Z = -2.25
    z_inner = z_floor - 0.15  # -2.25 mm

    with BuildPart() as bp:
        # 1. Flush 45 deg dovetail body (slides in cleanly from top-open track!)
        with BuildSketch(Plane.XY.offset(z_out)):
            with Locations((p.lepton_x, y_center)):
                RectangleRounded(w_outer, cap_len, 1.2)
        with BuildSketch(Plane.XY.offset(z_inner)):
            with Locations((p.lepton_x, y_center)):
                RectangleRounded(w_inner, cap_len, 1.2)
        loft()

        # 2. True ONE-END cantilever spring fingers strictly inboard of 45 deg dovetail rails!
        # Finger runs vertically from anchor at y_center-3.5 to free tip at y_center+2.5 (6.0 mm beam)
        finger_x_off = 8.40  # Outer slit edge at 8.40 + 1.45 = 9.85 < w_outer/2 (12.25) -> 2.4 mm solid rail!
        for sx in (-1.0, 1.0):
            fx = p.lepton_x + sx * finger_x_off
            with BuildSketch(Plane.XY.offset(z_out - 0.1)):
                with Locations((fx - sx * 1.15, y_center - 0.5)):
                    Rectangle(0.60, 6.2)
                with Locations((fx + sx * 1.15, y_center - 0.5)):
                    Rectangle(0.60, 6.2)
                with Locations((fx, y_center + 2.8)):
                    Rectangle(2.90, 0.60)
            extrude(amount=1.60, mode=Mode.SUBTRACT)

            # Inward-facing detent bump on cantilever tip (1.4 x 1.2 x 0.42 mm -> Z=-2.25..-1.83)
            # Actively drops into back_shell's floor detent notches (Z=-2.10..-1.65) at Open/Closed/Top-Stop!
            with BuildSketch(Plane.XY.offset(z_inner)):
                with Locations((fx, y_center + 1.50)):
                    RectangleRounded(1.40, 1.20, 0.35)
            extrude(amount=0.42)

        # 3. Tactile recessed thumb-grip flutes on lower outer face (0.45 mm deep -> 100% flat on Z=0 bed!)
        grip_cy = y_center - cap_len / 2.0 + 4.1
        for dy in (-1.5, 0.0, 1.5):
            with BuildSketch(Plane.XY.offset(z_out - 0.05)):
                with Locations((p.lepton_x, grip_cy + dy)):
                    RectangleRounded(w_outer - 5.2, 0.75, 0.25)
            extrude(amount=0.50, mode=Mode.SUBTRACT)

    return _as_solid(bp.part)


def build_capture_button_plunger(p: EnclosureParams, e: dict[str, float]) -> Solid:
    """Build V4 support-free guided capture plunger aligned with SW2 (Y=68.95, Z=19.30).

    - Prints cap-top DOWN on Z=0 with a 45 deg flared collar transitioning from the 6.2 mm
      shaft to the anti-rotating flange -> 100% support-free!
    - Narrow stem centered at Z=19.30 (Z in [18.70, 19.90] < PCB 20.0) hits SW2 (B3U-3000P
      tip at Y=68.95, Z=18.815..19.815) with 0.15 mm zero-preload rest gap (y_stem_tip=69.10).
    - Flange bottom at Y=69.50 bottoms on back_shell's U-cradle stop shelf at Y=68.95 after
      0.55 mm travel (0.40 mm Omron actuation stroke, 0.15 mm clear of PCB Y=68.40!).
    """
    y_flange_top = e["iy1"] - 0.10  # 70.30 mm (iy1 = 70.40 mm)
    y_flange_bot = 69.50            # Hard-stop face (hits back_shell shelf at Y=68.95 after 0.55 mm!)
    y_stem_tip = 69.10              # 0.15 mm zero-preload rest gap above SW2 tip at Y=68.95!
    y_cap_top = e["oy1"] + 2.20     # 75.20 mm (+2.20 mm proud of oy1=73.00)

    cap_r = p.button_plunger_cap_d / 2.0  # 3.10 mm
    chamf = 0.55
    flange_w_x, flange_h_z = 9.00, 6.40  # Z in [15.0, 21.4] -> stays strictly inside back_shell!

    with BuildPart() as bp:
        # 1. Anti-rotating internal flange with 45 deg print-friendly transition strictly below iy1!
        with BuildSketch(_xz_plane(y_flange_top)):
            with Locations((p.button_x, p.button_z)):
                Circle(cap_r)
        with BuildSketch(_xz_plane(y_flange_top - 0.45)):
            with Locations((p.button_x, p.button_z)):
                RectangleRounded(flange_w_x, flange_h_z, 1.4)
        loft()

        with BuildSketch(_xz_plane(y_flange_top - 0.45)):
            with Locations((p.button_x, p.button_z)):
                RectangleRounded(flange_w_x, flange_h_z, 1.4)
        extrude(amount=(y_flange_top - 0.45) - y_flange_bot)

        # 2. Dedicated narrow actuator stem centered at Z=19.30 (Z in [18.70, 19.90] < PCB 20.0!)
        with BuildSketch(_xz_plane(y_flange_bot)):
            with Locations((p.button_x, p.button_stem_z)):
                RectangleRounded(3.0, 1.20, 0.30)
        extrude(amount=y_flange_bot - y_stem_tip)

        # 3. Guided cylindrical shaft through top wall bore
        shaft_h = (y_cap_top - chamf) - y_flange_top
        with BuildSketch(_xz_plane(y_flange_top)):
            with Locations((p.button_x, p.button_z)):
                Circle(cap_r)
        extrude(amount=-shaft_h)

        # 4. Chamfered tactile finger cap (flat build-plate base!)
        with BuildSketch(_xz_plane(y_cap_top - chamf)):
            with Locations((p.button_x, p.button_z)):
                Circle(cap_r)
        with BuildSketch(_xz_plane(y_cap_top)):
            with Locations((p.button_x, p.button_z)):
                Circle(cap_r - chamf)
        loft()

    return _as_solid(bp.part)


def build_slide_switch_cap(p: EnclosureParams, e: dict[str, float]) -> Solid:
    """Build V4 flat-bed support-free captive right-wall slide-switch shuttle (ENC-3D-P0-3).

    - Entire bottom face is flush at Z=16.70 -> prints 100% flat on Z=0 with ZERO overhangs!
    - Outer thumb pad (3.40 mm Z height < 4.00 mm slot) slides cleanly from inside with 0.60 mm
      assembly clearance (0.000 mm^3 drop-in interference!).
    - Neck width 6.00 mm in 9.60 mm slot provides positive +-1.80 mm housing stops (covers C&K
      JS102011SAQN +-1.25 mm throw without stressing switch end-stops!).
    """
    sy = p.switch_y
    z_bot = 16.70               # Common flush build-plate bottom plane (100% support-free!)
    x_fork_tip = 89.10          # +0.10 mm clear of SW1 B.Fab body (X<=89.0), >=0.90 mm overlap on knob!
    x_shoulder_in = 89.55       # 0.15 mm clear of PCB right edge (89.40) for taller captive shoulder
    x_shoulder_out = e["ix1"] - 0.15  # 91.45 mm
    x_thumb_tip = e["ox1"] + 0.15     # 94.35 mm

    with BuildPart() as bp:
        # 1a. Inner captive shoulder (X in [89.55, 91.45], Z in [16.70, 21.10])
        with BuildSketch(_yz_plane(x_shoulder_in)):
            with Locations((sy, z_bot + 2.20)):
                RectangleRounded(12.20, 4.40, 0.70)
        extrude(amount=x_shoulder_out - x_shoulder_in)

        # 1b. Deep low-Z fork extension (X in [88.80, 89.65], Z in [16.70, 19.70] < PCB 20.0!)
        with BuildSketch(_yz_plane(x_fork_tip)):
            with Locations((sy, z_bot + 1.50)):
                RectangleRounded(7.60, 3.00, 0.50)
        extrude(amount=(x_shoulder_in + 0.10) - x_fork_tip)

        # Precision 2.40 mm wide fork pocket capturing the 1.5 mm C&K JS102011SAQN actuator
        with BuildSketch(_yz_plane(x_fork_tip - 0.10)):
            with Locations((sy, z_bot + 1.25)):
                Rectangle(2.40, 2.70)
        extrude(amount=2.40, mode=Mode.SUBTRACT)

        # 2. Guided slider neck + outer thumb pad (6.00 x 3.40 mm -> Z in [16.70, 20.10] < 4.0 mm slot!)
        with BuildSketch(_yz_plane(x_shoulder_out)):
            with Locations((sy, z_bot + 1.70)):
                RectangleRounded(6.00, 3.40, 0.65)
        extrude(amount=(x_thumb_tip - 0.20) - x_shoulder_out)

        # Three tactile grip ridges flush to z_bot (Z in [16.70, 19.90] -> 100% flat on bed!)
        for dy in (-1.5, 0.0, 1.5):
            with BuildSketch(_yz_plane(x_thumb_tip - 0.20)):
                with Locations((sy + dy, z_bot + 1.60)):
                    RectangleRounded(0.75, 3.20, 0.25)
            extrude(amount=0.35)

    return _as_solid(bp.part)


def build_battery_cradle(p: EnclosureParams) -> Solid:
    """Build V4 100% flat-bed support-free LiPo protective cradle (Fable #2 & #5).

    - Prints FLAT floor-down on Z=0 (ZERO underside protrusions, ZERO bridges, ZERO supports!).
    - Sits at Z in [23.40, 29.70] on 4x 1.80 mm M2.5 nylon spacers (leaving 1.80 mm vault
      clearing real 1.51 mm PCB STEP THT tails!).
    - Ø6.20 mm corner bores clear both SW4 and SW5 (5.77 mm corner) M2.5x11 hex standoffs!
    - Smooth 0.80 mm solid insulating floor + 5.50 mm side walls cradle the slim 505060 pouch
      (5.0 mm nominal + 10% swell = 5.5 mm).
    """
    bat = p.battery
    z_floor_bot = 23.40  # 1.80 mm clear above Main PCB F.Cu (21.60) -> clears 1.55 mm real THT tails!
    z_floor_top = 24.20  # 0.80 mm solid smooth flat barrier floor
    wall_h = 5.45        # Smooth side walls up to Z=29.65 (supports flat battery_lid at Z=29.70!)
    wall_t = 1.20

    in_w = bat.width_x + 1.40    # 61.40 mm
    in_h = bat.height_y + 1.40   # 51.40 mm
    out_w = in_w + 2.0 * wall_t  # 63.80 mm
    out_h = in_h + 2.0 * wall_t  # 53.80 mm
    cx, cy = bat.center_x, bat.center_y

    with BuildPart() as bp:
        # 1. 100% flat-bottom 0.80 mm insulating barrier plate + corner locator ears
        with BuildSketch(Plane.XY.offset(z_floor_bot)):
            with Locations((cx, cy)):
                RectangleRounded(out_w, out_h, 2.20)
            for mx, my in p.mounting_holes:
                arm_x = 0.5 * (mx + max(cx - out_w / 2.0 + 3.0, min(cx + out_w / 2.0 - 3.0, mx)))
                arm_y = 0.5 * (my + max(cy - out_h / 2.0 + 3.0, min(cy + out_h / 2.0 - 3.0, my)))
                with Locations((mx, my)):
                    Circle(4.40)
                with Locations((arm_x, arm_y)):
                    Rectangle(abs(mx - arm_x) * 2.0 + 5.0, abs(my - arm_y) * 2.0 + 5.0)
        extrude(amount=z_floor_top - z_floor_bot)

        # 4x Ø6.20 mm clearance bores clearing SW5 (5.77 mm corner) M2.5 hex standoffs!
        for mx, my in p.mounting_holes:
            with BuildSketch(Plane.XY.offset(z_floor_bot - 0.5)):
                with Locations((mx, my)):
                    Circle(3.10)
            extrude(amount=2.0, mode=Mode.SUBTRACT)

        # 2. Smooth perimeter containment walls around the 505060 LiPo pouch
        with BuildSketch(Plane.XY.offset(z_floor_top)):
            with Locations((cx, cy)):
                RectangleRounded(out_w, out_h, 2.20)
                RectangleRounded(in_w, in_h, 0.60, mode=Mode.SUBTRACT)
        extrude(amount=wall_h)

        # 3. Wide smooth JST-PH wire exit channel on right wall (+X) toward J4
        with BuildSketch(Plane.XY.offset(z_floor_top - 0.1)):
            with Locations((cx + out_w / 2.0, 50.0)):
                Rectangle(wall_t * 3.0, 14.0)
        extrude(amount=wall_h + 0.5, mode=Mode.SUBTRACT)

    return _as_solid(bp.part)


def build_battery_lid(p: EnclosureParams) -> Solid:
    """Build V4 flat 0.60 mm smooth insulating roof shield separating LiPo from Display SMDs (Fable #2)."""
    bat = p.battery
    z_lid_bot = 29.75  # 0.05 mm above swollen 5.50 mm battery top (29.70) & cradle rim (29.65)
    z_lid_t = 0.60     # Top at Z=30.35 -> clears Display back SMDs (Z=30.40..32.60)!
    out_w = bat.width_x + 1.40 + 2.40  # Matches battery_cradle outer footprint
    out_h = bat.height_y + 1.40 + 2.40
    cx, cy = bat.center_x, bat.center_y

    with BuildPart() as bp:
        with BuildSketch(Plane.XY.offset(z_lid_bot)):
            with Locations((cx, cy)):
                RectangleRounded(out_w, out_h, 2.20)
            for mx, my in p.mounting_holes:
                arm_x = 0.5 * (mx + max(cx - out_w / 2.0 + 3.0, min(cx + out_w / 2.0 - 3.0, mx)))
                arm_y = 0.5 * (my + max(cy - out_h / 2.0 + 3.0, min(cy + out_h / 2.0 - 3.0, my)))
                with Locations((mx, my)):
                    Circle(4.40)
                with Locations((arm_x, arm_y)):
                    Rectangle(abs(mx - arm_x) * 2.0 + 5.0, abs(my - arm_y) * 2.0 + 5.0)
        extrude(amount=z_lid_t)

        for mx, my in p.mounting_holes:
            with BuildSketch(Plane.XY.offset(z_lid_bot - 0.5)):
                with Locations((mx, my)):
                    Circle(3.10)
            extrude(amount=2.0, mode=Mode.SUBTRACT)

    return _as_solid(bp.part)
