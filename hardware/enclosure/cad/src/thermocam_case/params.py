"""Single source of truth for DIY-Thermocam V3.0 / V4.0 enclosure dimensions.

All XY coordinates are anchored directly to `hardware/pcb/diy-thermocam-v4.0.kicad_pcb`
in the canonical camera frame (viewed from the Display / Front side):
  - X in [0, 89.4] mm: left edge (display 2x20 header) -> right edge (power switch)
  - Y in [0, 68.4] mm: bottom edge (USB / tripod) -> top edge (button / LED / microSD)
  - Z in [0, 40.2] mm: inner face of Back (Lepton) wall -> inner face of Front (Display) wall

Exact zero-drift Z-stackup (verified against Fable 5.2 #176 & Stage 1 #169):
  - Z =  0.0 .. 20.0 mm : Rear compartment (4x M2.5x20 brass standoffs directly on flat Z=0!)
      - Back-shell bottom tripod boss sits cleanly at Z=0..14.4, X=44.7, Y=-3.8..+7.2
        (zero Display/PCB/LiPo/USB-C collision, 3.0 mm solid blind roof stops 10 mm stud!)
  - Z = 20.0 .. 21.6 mm : Main PCB (89.4 x 68.4 x 1.6 mm)
  - Z = 21.6 .. 32.6 mm : Mid compartment (4x M2.5x11 brass standoffs = exact 2x20 LCD mate):
      - Z = 21.6 .. 23.4 mm : 1.80 mm THT pin relief vault (clears real 1.51 mm PCB STEP tails!)
      - Z = 23.4 .. 24.2 mm : V4 flat-printing smooth LiPo cradle floor (0.80 mm)
      - Z = 24.2 .. 29.7 mm : Slim UL/IEC 62133 protected pouch (505060 2000 mAh, 5.0 mm + 10% swell = 5.5 mm)
      - Z = 29.7 .. 30.3 mm : V4 smooth insulating LiPo top lid (0.60 mm -> shields Display SMDs!)
      - Z = 30.3 .. 32.6 mm : 2.30 mm Display PCB backside SMD/FPC/SD socket clearance
  - Z = 32.6 .. 34.2 mm : Real BuyDisplay ER-TFTM032-3 PCB (89.4 x 64.9 x 1.6 mm, Y=2.0..66.9!)
  - Z = 34.2 .. 40.2 mm : Display bezel + 4x M2.5x5 standoffs directly on flat Z=40.2!
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import re
import xml.etree.ElementTree as ET


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
    """Complete parametric dimensions for one DIY-Thermocam hardware revision."""

    variant: str
    version_tag: str
    description: str
    is_zero_solder: bool = False

    # --- Verified PCB geometry (from hardware/pcb/diy-thermocam-v4.0.kicad_pcb) ---
    pcb_w: float = 89.40
    pcb_h: float = 68.40
    pcb_t: float = 1.60
    pcb_z_back: float = 20.00  # Exact zero-drift Z datum (standoffs seat on flat Z=0.0!)

    mounting_holes: tuple[tuple[float, float], ...] = (
        (12.50, 4.00),
        (79.00, 4.00),
        (12.50, 64.90),
        (79.00, 64.90),
    )
    mounting_hole_d: float = 2.80
    countersink_head_d: float = 5.00  # Exact DIN 965 / ISO 10642 M2.5 CSK flush seat (Astra P2-1)
    countersink_angle_deg: float = 90.0

    # --- Real BuyDisplay ER-TFTM032-3 PCB (Fable #176 P0-1: 89.4 x 64.9 mm, Y=2.0..66.9!) ---
    # Exact standoff stackup (Astra ENC-2D-P1-2 / ENC-3D-P2-1): 20 + 1.6 + 11 + 1.6 + 5 = 39.20 mm!
    internal_depth: float = 39.20
    display_pcb_z_back: float = 32.60
    display_pcb_t: float = 1.60
    display_pcb_smd_keepout_h: float = 2.20
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
    lepton_x: float = 43.60
    lepton_y: float = 51.25
    lepton_optical_centers_check: tuple[tuple[float, float], ...] = (
        (42.75, 51.00),
        (44.50, 51.50),
    )
    lepton_pupil_z: float = 2.20
    lepton_pupil_clear_d: float = 3.20
    lepton_hfov_deg: float = 56.0
    lepton_vfov_deg: float = 42.0
    lepton_dfov_deg: float = 71.0
    lepton_31r_hfov_deg: float = 95.0
    lepton_31r_dfov_deg: float = 119.0

    # Clears BOTH Lepton 3.5 (71 deg DFOV) AND Lepton 3.1R (95 deg HFOV / 119 deg DFOV) at BOTH centers!
    laser_lepton_aperture_d: float = 23.80  # Clears 119 deg DFOV even on 3.20 mm acrylic (>=5.6 mm web)!
    shell_lepton_inner_d: float = 13.40     # Clears 119 deg DFOV at Z=0.0 across both optical centers
    shell_lepton_outer_d: float = 20.20     # Clears 119 deg DFOV at Z_track_floor=-2.10 w/ >=2.1 mm wall!

    # --- Teensy 4.1 features ---
    teensy_x: float = 66.90
    teensy_z: float = 7.50
    teensy_prog_x: float = 66.90   # Exact PJRC Teensy 4.1 Prog button XY (Astra P1-6)
    teensy_prog_y: float = 43.08
    teensy_prog_hole_d: float = 2.60

    microsd_x: float = 66.90
    microsd_z: float = 6.80
    microsd_w: float = 12.60       # Guides 11x1 mm microSD cleanly without rattle (Astra P1-5)
    microsd_h: float = 2.80

    # --- Controls ---
    switch_y: float = 34.45
    switch_z: float = 28.10
    switch_h_y: float = 19.30      # Exact NKK CWT12AAS1 19.20x6.80 + 0.1 mm fit (Astra P1-3)
    switch_w_z: float = 6.90
    switch_recess_y: float = 0.0

    button_x: float = 71.40
    button_z: float = 28.10
    button_hole_d: float = 9.20
    button_plunger_bore_d: float = 6.80
    button_plunger_cap_d: float = 6.20
    button_plunger_flange_d: float = 9.60
    button_stem_z: float = 19.30  # Dead-center on SW2 B3U-3000P tip (Z=18.815..19.815), < PCB 20.0!

    led_x: float = 5.50
    led_z: float = 24.40
    laser_led_d: float = 5.20      # Clears standard 5 mm V3 LED (Astra P2-2)
    shell_led_d: float = 5.20
    back_face_led_holes: tuple[tuple[float, float], ...] = ()
    back_face_led_d: float = 2.40

    # --- Tripod Mount (1/4"-20 UNC) ---
    # Fable #176 P0 fix: 3D shell tripod boss sits on back_shell at (X=44.70, Z=6.80, Z=0..13.60)
    # -> 0.000 mm^3 collision w/ 89.4x64.9 Display PCB, Main PCB, USB-C (0.80 mm clear!), or LiPo;
    #    100% support-free; 3.0 mm solid blind roof stops even a 10.0 mm tripod screw!
    tripod_x: float = 44.70
    tripod_z: float = 29.80           # Laser bottom plate (V3 centered; V4 overridden to clear USB-C)
    shell_tripod_z: float = 6.80      # 3D back_shell empty bottom-center bay (Z in [0.0, 13.60])
    laser_tripod_x: float = 44.70     # Overridden on V4 to X=27.00 so >=9.6 mm web clears full USB-C!
    laser_tripod_z: float = 29.80
    laser_tripod_center_d: float = 9.00
    laser_tripod_bolt_r: float = 8.20  # >=2.6 mm acrylic web between center bore & M2 bolts (Astra P1-3)
    laser_tripod_bolt_d: float = 2.20
    shell_tripod_screw_d: float = 6.80
    shell_tripod_hex_af: float = 11.50
    shell_tripod_nut_h: float = 5.80
    shell_tripod_floor_t: float = 2.20  # Y = -3.80 .. -1.60
    shell_tripod_roof_t: float = 3.00   # Y = +4.20 .. +7.20 (stops up to 10 mm stud!)
    shell_tripod_boss_w: float = 19.00
    shell_tripod_boss_z_end: float = 13.60  # Leaves 0.80 mm solid separation below V4 USB-C boot (Z=14.4)!

    # --- Laser-cut acrylic parameters (3.0 mm Acryl GS) ---
    laser_t: float = 3.00
    laser_internal_depth: float = 39.20  # Exact 20+1.6+11+1.6+5 standoff stack = 0 mm bow (Astra P1-2)
    laser_slot_clearance: float = 0.20   # Fits 2.8..3.2 mm cast acrylic cleanly (Astra P1-1)
    laser_kerf: float = 0.10
    laser_inner_margin_x_neg: float = 0.25
    laser_inner_margin_x_pos: float = 0.25
    laser_inner_margin_y_neg: float = 0.25
    laser_inner_margin_y_pos: float = 0.50
    laser_plate_overhang: float = 2.50
    laser_corner_r: float = 3.50
    laser_tab_len: float = 12.00

    # --- 2-piece 3D-printable / CNC shell parameters ---
    shell_wall: float = 2.60
    shell_face_t: float = 2.60
    shell_back_face_t: float = 3.60
    shell_corner_r: float = 3.20
    shell_edge_chamfer: float = 1.00
    shell_clearance_x_neg: float = 1.60
    shell_clearance_x_pos: float = 2.20
    shell_clearance_y_neg: float = 1.20  # Compact oy0=-3.80 so USB-C plug boots seat fully!
    shell_clearance_y_pos: float = 2.00  # iy1=70.40 gives proper SW2 (Y=68.95) travel + hard stop!
    shell_split_z: float = 21.80
    lens_cap_clearance: float = 0.25

    usb: UsbCutoutSpec = field(
        default_factory=lambda: UsbCutoutSpec(
            label="Teensy 4.1 micro-USB",
            center_x=66.90,
            center_z=7.50,
            width=10.40,
            height=5.60,
            corner_radius=1.50,
            overmold_width=13.20,
            overmold_height=8.20,
            overmold_depth=2.40,
            laser_width=11.00,
            laser_height=6.00,
        )
    )
    battery: BatterySpec = field(
        default_factory=lambda: BatterySpec(
            name="Samsung EB-BG355BBE",
            width_x=53.00,
            height_y=50.00,
            thickness_z=5.80,
            center_x=44.70,
            center_y=34.45,
            z_start=22.00,
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


PRESETS: dict[str, EnclosureParams] = {
    "v3_0": EnclosureParams(
        variant="v3_0",
        version_tag="V3.0",
        description="DIY-Thermocam V3.0 (Teensy 4.1 micro-USB, wired switch/button, EB-BG355BBE LiPo)",
        is_zero_solder=False,
        usb=UsbCutoutSpec(
            label="Teensy 4.1 micro-USB",
            center_x=66.90,
            center_z=7.50,
            width=10.40,
            height=5.60,
            corner_radius=1.50,
            overmold_width=13.20,
            overmold_height=8.20,
            overmold_depth=2.80,  # Full-depth through-wall boot opening (0.0 mm thin web, seats to Y=-0.55!)
            laser_width=13.20,    # Clears full micro-USB boot through 3.0 mm acrylic plate!
            laser_height=8.20,
        ),
        battery=BatterySpec(
            name="Samsung EB-BG355BBE (w/ 1.6 mm adhesive insulating protector)",
            width_x=53.00,
            height_y=50.00,
            thickness_z=5.80,
            center_x=44.70,
            center_y=34.45,
            z_start=23.20,  # Sits on 1.6 mm insulating protector above 1.55 mm THT tails!
        ),
    ),
    "v4_0": EnclosureParams(
        variant="v4_0",
        version_tag="V4.0 PROTO",
        description=(
            "DIY-Thermocam V4.0 Zero-Solder PCBA (USB-C full-depth counterbore, support-free "
            "capture plunger & switch shuttle, enclosed LiPo cradle+lid, back-shell 1/4\"-20 blind nut)"
        ),
        is_zero_solder=True,
        usb=UsbCutoutSpec(
            label="Board-edge USB-C (TYPE-C-31-M-12)",
            center_x=45.80,
            center_z=18.40,
            width=10.40,
            height=4.60,
            corner_radius=1.60,
            overmold_width=14.20,
            overmold_height=8.00,
            overmold_depth=2.80,
            laser_width=14.20,   # Clears full 13.8x7.8 USB-C boot right to J3 mouth (ENC-3D-P1-1)!
            laser_height=8.00,
        ),
        laser_tripod_x=27.00,    # >=9.6 mm acrylic web clear of full 14.2x8.0 USB-C opening (ENC-2D-P1-3)!
        laser_tripod_z=28.00,
        switch_y=12.70,
        switch_z=18.50,
        switch_h_y=10.20,        # Positive housing hard-stop at +-2.0 mm (clears +-1.65 mm w/ 0.000 mm^3)!
        switch_w_z=4.00,
        switch_recess_y=16.80,
        button_x=84.00,
        button_z=18.20,
        button_hole_d=6.80,
        button_plunger_bore_d=6.80,
        button_plunger_cap_d=6.20,
        button_plunger_flange_d=9.60,
        button_stem_z=19.30,     # Centered on SW2 B3U-3000P tip (Z=18.815..19.815), < PCB 20.0!
        back_face_led_holes=((5.75, 66.40),),
        back_face_led_d=2.40,
        teensy_prog_x=85.30,
        teensy_prog_y=4.00,
        teensy_prog_hole_d=2.40,
        # ENC-2D-P0-1 & P1-3: Room for pure 3 mm laser-cut acrylic capture_key & switch_key + >=4 mm webs!
        laser_inner_margin_x_pos=2.20,
        laser_inner_margin_y_pos=2.40,  # iy1=70.80 -> proper rest gap + 0.55 mm travel over SW2 (Y=68.95)!
        # Fable #176 P0 fix: 505060 2000mAh UL/IEC 62133 slim protected cell (5.0 mm nominal ->
        # tested at 5.5 mm swollen thickness!) between 0.8 mm cradle floor (Z=24.2) & 0.6 mm lid (Z=29.7)
        battery=BatterySpec(
            name="UL/IEC 62133 Slim Protected LiPo 2000mAh (505060, 5.0mm nominal / 5.5mm swell, JST-PH)",
            width_x=60.00,
            height_y=50.00,
            thickness_z=5.50,
            center_x=42.50,
            center_y=34.50,
            z_start=24.20,
        ),
    ),
}


def get_preset(variant: str) -> EnclosureParams:
    if variant == "v3_1":
        variant = "v4_0"
    if variant not in PRESETS:
        raise KeyError(f"Unknown variant {variant!r}; expected one of {sorted(PRESETS)}")
    return PRESETS[variant]


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


def parse_eagle_brd_metrics(brd_path: Path) -> dict[str, object]:
    tree = ET.parse(brd_path)
    root = tree.getroot()
    dim_xs: list[float] = []
    dim_ys: list[float] = []
    plain = root.find(".//board/plain")
    assert plain is not None
    for wire in plain.findall("wire"):
        if wire.get("layer") == "20":
            dim_xs.extend([float(wire.get("x1", 0)), float(wire.get("x2", 0))])
            dim_ys.extend([float(wire.get("y1", 0)), float(wire.get("y2", 0))])
    mounting_holes: list[tuple[float, float]] = []
    lepton_holes: list[tuple[float, float]] = []
    for sig in root.findall(".//board/signals/signal"):
        for via in sig.findall("via"):
            drill = float(via.get("drill", 0))
            pt = (float(via.get("x", 0)), float(via.get("y", 0)))
            if 2.6 <= drill <= 3.0:
                mounting_holes.append(pt)
            elif 2.0 <= drill <= 2.2:
                lepton_holes.append(pt)
    elements = {
        el.get("name", ""): (float(el.get("x", 0)), float(el.get("y", 0)))
        for el in root.findall(".//board/elements/element")
    }
    return {
        "pcb_w": round(max(dim_xs) - min(dim_xs), 4),
        "pcb_h": round(max(dim_ys) - min(dim_ys), 4),
        "mounting_holes": sorted(mounting_holes),
        "lepton_holes": sorted(lepton_holes),
        "teensy_pos": elements["U$2"],
        "chg_led_pos": elements["CHG-LED"],
        "switch_pos": elements["SWITCH"],
        "button_pos": elements["BUTTON"],
    }
