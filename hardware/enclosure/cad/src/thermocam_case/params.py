"""Single source of truth for the DIY-Thermocam V4.0 enclosure dimensions.

All XY coordinates come from `hardware/pcb/diy-thermocam-v4.0.kicad_pcb`, in the camera frame
seen from the display side:
  - X 0..89.4 mm: left board edge (display 2x20 header) -> right edge (power switch)
  - Y 0..68.4 mm: bottom edge (USB-C, tripod) -> top edge (capture button, microSD)
  - Z: 0 = inner face of the back (Lepton) wall, increasing toward the display

Z stack (mm):
  -0.5 .. 19.5  4x M2.5x20 hex standoffs, seated 0.5 mm into the back wall
  19.5 .. 20.0  4x nylon M2.5 washers (keep the standoff corners off the SW2/SW3 pads)
  20.0 .. 21.6  main PCB
  21.6 .. 32.6  4x M2.5x11 hex standoffs; printed battery cradle, FR4 floor, 505060 LiPo, FR4 lid
  32.6 .. 34.2  display PCB (BuyDisplay ER-TFTM032-3, Y 2.0..66.9)
  34.2 .. 39.2  display module; front screws reach the 11 mm standoffs through 5 mm bosses
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import re


@dataclass(frozen=True)
class UsbCutoutSpec:
    """Bottom-wall USB connector cutout & plug overmold counterbore specification."""

    label: str
    center_x: float
    center_z: float
    width: float
    height: float
    corner_radius: float
    overmold_width: float
    overmold_height: float
    overmold_depth: float
    laser_width: float
    laser_height: float


@dataclass(frozen=True)
class BatterySpec:
    """Internal LiPo keepout volume (including 10% swelling margin on V4)."""

    name: str
    width_x: float
    height_y: float
    thickness_z: float
    center_x: float
    center_y: float
    z_start: float


@dataclass(frozen=True)
class EnclosureParams:
    """Complete parametric dimensions for the DIY-Thermocam V4.0 enclosures."""

    version_tag: str = "V4.0"  # Debossed / engraved under the wordmark on the back face
    file_prefix: str = "diy-thermocam-v4.0"

    # --- Verified PCB geometry (from hardware/pcb/diy-thermocam-v4.0.kicad_pcb) ---
    pcb_w: float = 89.40
    pcb_h: float = 68.40
    pcb_t: float = 1.60
    pcb_z_back: float = 20.00

    mounting_holes: tuple[tuple[float, float], ...] = (
        (12.50, 4.00),
        (79.00, 4.00),
        (12.50, 64.90),
        (79.00, 64.90),
    )
    # Battery-cradle foot around each M2.5x11 standoff: 90 deg opening toward the nearer board edge
    # (deg, 0 = +X, 90 = +Y), where the clipped ring would be thinner than 1.0 mm; (79.0, 4.0) is
    # also open toward the J6 pin tails below the lip
    cradle_foot_edge_gap_deg: tuple[float, ...] = (270.0, 270.0, 90.0, 90.0)
    cradle_foot_tail_gap_deg: tuple[float | None, ...] = (None, 180.0, None, None)
    mounting_hole_d: float = 2.80
    # ISO 7046 M2.5: theoretical head Ø5.5 sits flush in a Ø5.6 x 90 deg countersink
    countersink_head_d: float = 5.60
    countersink_angle_deg: float = 90.0
    # Rear M2.5x20 standoffs stand 0.5 mm deep in the back wall; a 0.5 mm nylon washer on each
    # lifts their hex corners off the SW2/SW3 pads (3.11 / 3.18 mm from H4 / H2) on the PCB back
    standoff_seat_z: float = -0.50
    rear_washer_t: float = 0.50
    rear_washer_d: float = 6.00
    standoff_seat_d: float = 6.40   # Seat pocket for the SW5 hex (5.77 mm across corners)
    # Front screws reach the 11 mm standoffs through 5 mm bosses (shell) or printed spacer bars (laser)
    front_boss_d: float = 4.80      # 1.0 mm wall round the Ø2.8 hole; 0.53 mm clear of the display module frame at Y=6.93 / 61.97

    # --- BuyDisplay ER-TFTM032-3 (PCB 89.4 x 64.9 mm at Y=2.0..66.9) ---
    internal_depth: float = 39.20   # Back wall to front wall: 20 + 1.6 + 11 + 1.6 + 5
    display_pcb_z_back: float = 32.60
    display_pcb_t: float = 1.60
    display_pcb_smd_keepout_h: float = 2.50  # Back-side parts up to 2.5 mm per the module drawing
    display_pcb_w: float = 89.40
    display_pcb_h: float = 64.90
    display_pcb_center_x: float = 44.70
    display_pcb_center_y: float = 34.45

    display_win_x_min: float = 11.95
    display_win_x_max: float = 76.75
    display_win_y_min: float = 10.15
    display_win_y_max: float = 58.75

    # --- FLIR Lepton 2.5 / 3.5 optical geometry ---
    lepton_breakout_holes: tuple[tuple[float, float], ...] = (
        (30.00, 43.00),
        (55.50, 43.00),
        (55.50, 59.00),
        (30.00, 59.00),
    )
    # Lepton socket centre = optical axis: FLIR Breakout V2.0 drawing (U1 16.5 mm from the hole-side
    # edge, 10.0 mm from the far edge) and the V3 back-plate window both put it at (44.45, 51.00)
    lepton_x: float = 44.45
    lepton_y: float = 51.00
    lepton_pupil_z: float = 2.20
    lepton_pupil_clear_d: float = 3.20
    lepton_dfov_deg: float = 71.0      # Lepton 3.5
    lepton_31r_dfov_deg: float = 119.0  # Lepton 3.1R wide-angle (95 deg HFOV)
    # 12 mm sleeves + 1.6 mm breakout + 7.14 mm Lepton in its Molex socket = shutter face at Z=-0.74:
    # the Lepton pokes 0.74 mm into the back wall, so the opening there must swallow its body
    lepton_sleeve_len: float = 12.00
    breakout_t: float = 1.60
    lepton_socketed_h: float = 7.14
    lepton_body_w: float = 11.80        # With socket (FLIR datasheet: 11.8 x 12.7 x 7.14 mm)
    lepton_body_l: float = 12.70

    laser_lepton_aperture_d: float = 25.20  # Clears 119 deg DFOV on 3.2 mm acrylic, axis +-0.5 mm
    shell_lepton_inner_d: float = 18.80     # Lepton body (17.34 mm diagonal) + 0.7 mm all round
    shell_lepton_outer_d: float = 20.20     # Clears 119 deg DFOV at the lens-cap track floor

    # --- Teensy 4.1 (plugs into the J5/J6 1x24 sockets on B.Cu, X=59.28 / 74.52) ---
    teensy_center_x: float = 66.90
    microsd_x: float = 66.90
    microsd_z: float = 6.80
    microsd_w: float = 12.60       # 11 x 1 mm card with 0.8 mm side play
    microsd_h: float = 2.80

    # --- Onboard controls (all on V4.0 PCB B.Cu, reached through the back shell) ---
    # SW3 Prog tact switch pinhole on the back face
    teensy_prog_x: float = 85.30
    teensy_prog_y: float = 4.00
    teensy_prog_hole_d: float = 2.40

    # SW1 C&K JS102011SAQN right-angle slide switch on the right (+X) wall: body X<=89.0,
    # 1.5 mm actuator X 89.0..91.0, Z 16.6..18.35, 2.0 mm travel (+-1.0 mm from centre)
    switch_y: float = 12.70
    switch_z: float = 18.50
    switch_h_y: float = 10.20      # Wall slot: the 6.0 mm neck stops at +-2.1 mm
    switch_w_z: float = 4.00
    switch_recess_y: float = 16.80
    switch_travel: float = 2.00

    # SW2 Omron B3U-3000P capture switch: plunger tip 0.55 mm past the top PCB edge (Y=68.95),
    # Z 18.8..19.8, body front at Y=68.25; pretravel 0.2 +0.2/-0.1 mm. Buttons stop on the PCB edge
    # itself (Y=68.40): shell button 0.15 mm rest gap + 0.60 mm stroke into the switch; the acrylic
    # key's stem lands on the edge, so its stroke is the plunger's 0.55 mm overhang.
    button_x: float = 84.00
    button_z: float = 18.20
    button_plunger_bore_d: float = 6.80
    button_plunger_cap_d: float = 6.20
    button_stem_z: float = 19.30
    button_rest_gap: float = 0.15
    button_stop_stroke: float = 0.60

    # D2 charge LED (0603 on B.Cu) seen through a hole in the back wall
    back_face_led_holes: tuple[tuple[float, float], ...] = ((5.75, 66.40),)
    back_face_led_d: float = 2.40
    guide_tube_od: float = 4.40     # Printed light / pin guide tubes from the back wall toward D2 and SW3

    # --- Tripod mount: the same 1/4"-20 UNC hex nut (7/16" AF x 5.5 mm) in both enclosures ---
    tripod_x: float = 44.70
    shell_tripod_z: float = 6.80
    shell_tripod_screw_d: float = 6.80
    shell_tripod_hex_af: float = 11.50
    shell_tripod_nut_h: float = 5.80
    shell_tripod_floor_t: float = 1.20  # A 4.5 mm tripod screw gets 3.3 mm (2.6 turns) of thread
    shell_tripod_roof_t: float = 3.00
    shell_tripod_boss_w: float = 19.00
    shell_tripod_boss_z_end: float = 13.60
    # Laser case: printed tripod_mount snapped onto the rear standoff at (12.5, 4.0); its plug fills
    # a hole in the bottom plate so the nut sits 1.2 mm above the outer face
    laser_tripod_x: float = 26.00     # 3.6 mm acrylic web to the USB-C opening
    laser_tripod_z: float = 10.00

    # --- Laser-cut acrylic parameters (3.0 mm cast acrylic) ---
    laser_t: float = 3.00
    laser_internal_depth: float = 39.70  # Back plate inner face at Z=-0.5 (standoff seat) to Z=39.2
    laser_slot_clearance: float = 0.20   # Slots 3.2 mm wide: fits 2.8..3.2 mm stock
    laser_kerf: float = 0.10
    laser_inner_margin_x_neg: float = 0.60  # Display PCB width tolerance is +-0.3 mm
    laser_inner_margin_x_pos: float = 2.20  # Room for the switch key shoulder
    laser_inner_margin_y_neg: float = 1.05  # Teensy micro-USB overhangs the board edge to Y=-0.55;
                                            # leaves the tripod mount's fin its 1.0 mm (MJF minimum)
    laser_inner_margin_y_pos: float = 2.40  # Room for the capture key hook
    laser_plate_overhang: float = 2.50
    laser_corner_r: float = 3.50
    laser_tab_len: float = 12.00

    # --- Battery stack between the main PCB (21.6) and the display back parts (30.1) ---
    # 0.6 mm FR4 floor at Z 23.4..24.0, 0.2 mm over the 1.6 mm THT tails and Lepton screw heads;
    # 505060 cell up to 5.45 mm (5.0 mm + 9 % swell); 0.05 mm gap; 0.4 mm FR4 lid -> lid top at
    # Z=29.90, 0.2 mm under the display's back-side parts. The floor and lid are too thin to print
    # (MJF minimum wall 1.0 mm), so they are bare FR4 boards from the PCB maker. The printed cradle
    # frame carries the floor on 1.0 mm lips and stands on its own feet around the standoffs.
    battery_floor_t: float = 0.60
    battery_lid_gap: float = 0.05
    battery_lid_t: float = 0.40
    cradle_lip_t: float = 1.00
    cradle_lip_w: float = 2.00
    cradle_wall_t: float = 1.20
    cradle_foot_od: float = 8.40     # 1.1 mm wall around the Ø6.2 bore for the SW5 hex standoff
    cradle_foot_top_z: float = 25.40
    fr4_clearance: float = 0.20      # FR4 floor to cradle wall, per side

    # --- 2-piece 3D-printable shell parameters ---
    shell_wall: float = 2.60
    shell_face_t: float = 2.60
    shell_back_face_t: float = 3.60
    shell_corner_r: float = 3.20
    shell_edge_chamfer: float = 1.00
    shell_clearance_x_neg: float = 1.60
    shell_clearance_x_pos: float = 2.20
    shell_clearance_y_neg: float = 1.20  # oy0=-3.80 so USB-C plug boots seat fully
    shell_clearance_y_pos: float = 2.00  # iy1=70.40: room for the capture button flange
    shell_split_z: float = 21.80
    lens_cap_clearance: float = 0.25

    usb: UsbCutoutSpec = field(
        default_factory=lambda: UsbCutoutSpec(
            label="Board-edge USB-C (TYPE-C-31-M-12)",
            center_x=45.80,
            center_z=18.40,
            width=10.40,
            height=4.60,
            corner_radius=1.60,
            overmold_width=14.20,
            overmold_height=8.00,
            overmold_depth=2.80,
            laser_width=14.20,   # Clears a 13.8 x 7.8 USB-C plug boot right up to the J3 mouth
            laser_height=8.00,
        )
    )
    # 505060 protected LiPo, worst case: 62 x 50.5 mm with its protection board, 5.45 mm swollen
    battery: BatterySpec = field(
        default_factory=lambda: BatterySpec(
            name="505060 2000 mAh protected LiPo (JST-PH), 62 x 50.5 x 5.45 mm worst case",
            width_x=62.00,
            height_y=50.50,
            thickness_z=5.45,
            center_x=41.70,
            center_y=34.50,
            z_start=24.00,
        )
    )

    @property
    def display_win_w(self) -> float:
        return self.display_win_x_max - self.display_win_x_min

    @property
    def display_win_h(self) -> float:
        return self.display_win_y_max - self.display_win_y_min

    @property
    def display_win_cx(self) -> float:
        return 0.5 * (self.display_win_x_min + self.display_win_x_max)

    @property
    def display_win_cy(self) -> float:
        return 0.5 * (self.display_win_y_min + self.display_win_y_max)


V4_PARAMS = EnclosureParams()


def parse_kicad_pcb_metrics(pcb_path: Path) -> dict[str, object]:
    """Parse board outline, mounting/Lepton holes, and V4.0 mechanical datums from KiCad PCB."""
    txt = pcb_path.read_text(encoding="utf-8")
    dim_xs: list[float] = []
    dim_ys: list[float] = []
    for m in re.finditer(
        r'\(gr_line\s+\(start\s+([-\d.]+)\s+([-\d.]+)\)\s+\(end\s+([-\d.]+)\s+([-\d.]+)\)[\s\S]*?\(layer\s+"Edge\.Cuts"\)',
        txt,
    ):
        x1, y1, x2, y2 = (float(g) for g in m.groups())
        dim_xs.extend([x1, x2])
        dim_ys.extend([-y1, -y2])

    mounting_holes: list[tuple[float, float]] = []
    lepton_holes: list[tuple[float, float]] = []
    refs: dict[str, tuple[float, float]] = {}
    for m in re.finditer(
        r'\(footprint\s+"([^"]+)"[\s\S]*?\(at\s+([-\d.]+)\s+([-\d.]+)[\s\S]*?\(property\s+"Reference"\s+"([^"]+)"',
        txt,
    ):
        fp, xs, ys, ref = m.groups()
        pt = (round(float(xs), 4), round(-float(ys), 4))
        refs[ref] = pt
        if "MountingHole_2.8mm_M2.5" in fp:
            mounting_holes.append(pt)
        elif "MountingHole_2.1mm_M2" in fp:
            lepton_holes.append(pt)

    return {
        "pcb_w": round(max(dim_xs) - min(dim_xs), 4),
        "pcb_h": round(max(dim_ys) - min(dim_ys), 4),
        "mounting_holes": sorted(mounting_holes),
        "lepton_holes": sorted(lepton_holes),
        "usb_pos": refs["J3"],
        "switch_pos": refs["SW1"],
        "button_pos": refs["SW2"],
        "prog_pos": refs["SW3"],
        "chg_led_pos": refs["D2"],
    }

