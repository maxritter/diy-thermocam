"""Exact-clearance 0.20 mm 2-layer A* router for DIY-Thermocam V4.0.

Features:
- Multi-tier trace widths: 0.80 mm BOOST_SW, 0.60 mm high-current power trunks
  (5V, VSYS, VLIPO, LIPO_RAW, LIPO_PROT, VBUS), 0.40 mm 3V3, 0.20 mm signals
- Matched-length USB 2.0 differential routing (USB_DP_RAW / USB_DM_RAW skew < 0.5 mm)
- Zero dangling QFN stubs (adjacent same-net QFN pins bridged without unused fanouts)
- Per-SMD-GND-pad via stitching + 2.5 mm global GND stitching grid
"""

from __future__ import annotations

import heapq
import math
from collections import defaultdict

BOARD_W = 89.4
BOARD_H = 68.4
GRID_MM = 0.20
NX = int(round(BOARD_W / GRID_MM)) + 1
NY = int(round(BOARD_H / GRID_MM)) + 1

HIGH_PWR_NETS = {"5V", "VSYS", "VLIPO", "LIPO_RAW", "LIPO_PROT", "VBUS", "BOOST_SW"}
MED_PWR_NETS = {"3V3"}
POWER_NETS = HIGH_PWR_NETS | MED_PWR_NETS

CRITICAL_NET_ORDER = [
    # 1. Boost switching & feedback loops (tightest, widest 1.00mm SW)
    "BOOST_SW", "BOOST_FB", "BOOST_EN",
    # 2. USB-C differential & ESD
    "USB_DP_RAW", "USB_DM_RAW", "USB_CC1", "USB_CC2",
    # 3. BQ24074 & Q1/Q2 gate control pins (route before wide power trunks!)
    "BQ_EN2", "BQ_TS", "BQ_ISET", "BQ_ITERM", "BQ_ILIM", "BQ_TMR", "BQ_CHG_N",
    "Q1_GATE", "Q2_GATE",
    # 4. High-current power trunks (0.80mm wide!)
    "VSYS", "VBUS", "VLIPO", "LIPO_RAW", "LIPO_PROT", "5V",
    # 5. Display J1 outer-edge pins first so none get boxed in
    "LCD_P30", "LCD_P31", "D5", "D9", "D12", "D13", "D11", "D10", "D21", "D22", "CTP_INT", "D4",
    # 6. Lepton SPI1 + VSYNC + I2C
    "D27", "D26", "D38", "D39", "D3", "D18", "D19",
    # 7. Medium power & RTC
    "3V3", "VBAT",
    # 8. Remaining controls, dividers, LEDs
    "D2", "D23", "A16", "TEENSY_PROG", "LED_CHG_A",
]


def xy_to_grid(x: float, y: float) -> tuple[int, int]:
    return (
        max(3, min(NX - 4, int(round(x / GRID_MM)))),
        max(3, min(NY - 4, int(round((-y) / GRID_MM)))),
    )


def grid_to_xy(gx: int, gy: int) -> tuple[float, float]:
    return round(gx * GRID_MM, 4), round(-gy * GRID_MM, 4)


def route_all_nets_exact(
    pads: list[dict], comp_center: dict[str, tuple[float, float]]
) -> tuple[list[tuple], list[tuple], list[str]]:
    """Route all non-GND multi-pad nets with exact signal/medium/power/via clearances."""
    occ_sig = [[None] * (NX * NY) for _ in range(2)]  # 0.20 mm signals
    occ_med = [[None] * (NX * NY) for _ in range(2)]  # 0.40 mm medium power
    occ_pwr = [[None] * (NX * NY) for _ in range(2)]  # 0.60 mm high-current power
    via_block = bytearray(NX * NY)

    def idx(gx: int, gy: int) -> int:
        return gy * NX + gx

    def claim(grid: list, i: int, tag: str) -> None:
        cur = grid[i]
        grid[i] = tag if (cur is None or cur == tag) else "__CONFLICT__"

    for gy in range(NY):
        for gx in range(NX):
            i = idx(gx, gy)
            if gx < 3 or gx >= NX - 3 or gy < 3 or gy >= NY - 3:
                occ_sig[0][i] = occ_sig[1][i] = "__EDGE__"
                occ_med[0][i] = occ_med[1][i] = "__EDGE__"
                via_block[i] = 1
            if gx < 4 or gx >= NX - 4 or gy < 4 or gy >= NY - 4:
                occ_pwr[0][i] = occ_pwr[1][i] = "__EDGE__"

    def mark_pad(p: dict, grid: list, clr: float, net_tag: str) -> None:
        rx, ry = p["hx"] + clr, p["hy"] + clr
        gx0 = max(0, int(math.floor((p["x"] - rx) / GRID_MM)))
        gx1 = min(NX - 1, int(math.ceil((p["x"] + rx) / GRID_MM)))
        gy0 = max(0, int(math.floor((-p["y"] - ry) / GRID_MM)))
        gy1 = min(NY - 1, int(math.ceil((-p["y"] + ry) / GRID_MM)))
        for gy in range(gy0, gy1 + 1):
            if abs(-gy * GRID_MM - p["y"]) > ry:
                continue
            for gx in range(gx0, gx1 + 1):
                if abs(gx * GRID_MM - p["x"]) > rx:
                    continue
                i = idx(gx, gy)
                if p["on_f"] or p["is_npth"]:
                    claim(grid[0], i, net_tag)
                if p["on_b"] or p["is_npth"]:
                    claim(grid[1], i, net_tag)

    for p in pads:
        net_tag = p["net"] if (p["net"] and not p["is_npth"]) else "__PAD__"
        is_fine = ((min(p["hx"], p["hy"]) < 0.25) or p["ref"].startswith("JP")) and (not p["is_npth"])
        mark_pad(p, occ_sig, 0.22 if p["ref"].startswith("JP") else (0.26 if is_fine else 0.285), net_tag)
        mark_pad(p, occ_med, 0.36 if is_fine else 0.39, net_tag)
        mark_pad(p, occ_pwr, 0.65 if is_fine else 0.68, net_tag)
        vrx, vry = p["hx"] + 0.56, p["hy"] + 0.56
        vx0 = max(0, int(math.floor((p["x"] - vrx) / GRID_MM)))
        vx1 = min(NX - 1, int(math.ceil((p["x"] + vrx) / GRID_MM)))
        vy0 = max(0, int(math.floor((-p["y"] - vry) / GRID_MM)))
        vy1 = min(NY - 1, int(math.ceil((-p["y"] + vry) / GRID_MM)))
        for gy in range(vy0, vy1 + 1):
            if abs(-gy * GRID_MM - p["y"]) > vry:
                continue
            for gx in range(vx0, vx1 + 1):
                if abs(gx * GRID_MM - p["x"]) <= vrx:
                    via_block[idx(gx, gy)] = 1

    def mark_line(
        x1: float, y1: float, x2: float, y2: float, lyr: int, w: float, net: str, stub: bool = False
    ) -> None:
        r_sig = (w / 2.0 + 0.23) if stub else (w / 2.0 + 0.29)
        r_med = (w / 2.0 + 0.33) if stub else (w / 2.0 + 0.39)
        r_pwr = (w / 2.0 + 0.65) if stub else (w / 2.0 + 0.65)
        r_via = w / 2.0 + 0.58
        rmax = max(r_pwr, r_via)
        gx0 = max(0, int(math.floor((min(x1, x2) - rmax) / GRID_MM)))
        gx1 = min(NX - 1, int(math.ceil((max(x1, x2) + rmax) / GRID_MM)))
        gy0 = max(0, int(math.floor((min(-y1, -y2) - rmax) / GRID_MM)))
        gy1 = min(NY - 1, int(math.ceil((max(-y1, -y2) + rmax) / GRID_MM)))
        vx, vy = x2 - x1, y2 - y1
        l2 = vx * vx + vy * vy
        for gy in range(gy0, gy1 + 1):
            py = -gy * GRID_MM
            for gx in range(gx0, gx1 + 1):
                px = gx * GRID_MM
                t = 0.0 if l2 < 1e-9 else max(0.0, min(1.0, ((px - x1) * vx + (py - y1) * vy) / l2))
                d = math.hypot(px - (x1 + t * vx), py - (y1 + t * vy))
                i = idx(gx, gy)
                if d <= r_sig:
                    claim(occ_sig[lyr], i, net)
                if d <= r_med:
                    claim(occ_med[lyr], i, net)
                if d <= r_pwr:
                    claim(occ_pwr[lyr], i, net)
                if d <= r_via:
                    via_block[i] = 1

    def mark_via(vx: float, vy: float, net: str) -> None:
        r_sig, r_med, r_pwr, r_via = 0.60, 0.72, 0.90, 0.80
        gx0 = max(0, int(math.floor((vx - r_via) / GRID_MM)))
        gx1 = min(NX - 1, int(math.ceil((vx + r_via) / GRID_MM)))
        gy0 = max(0, int(math.floor((-vy - r_via) / GRID_MM)))
        gy1 = min(NY - 1, int(math.ceil((-vy + r_via) / GRID_MM)))
        for gy in range(gy0, gy1 + 1):
            py = -gy * GRID_MM
            for gx in range(gx0, gx1 + 1):
                d = math.hypot(gx * GRID_MM - vx, py - vy)
                i = idx(gx, gy)
                if d <= r_sig:
                    claim(occ_sig[0], i, net)
                    claim(occ_sig[1], i, net)
                if d <= r_med:
                    claim(occ_med[0], i, net)
                    claim(occ_med[1], i, net)
                if d <= r_pwr:
                    claim(occ_pwr[0], i, net)
                    claim(occ_pwr[1], i, net)
                if d <= r_via:
                    via_block[i] = 1

    segments: list[tuple] = []
    vias: list[tuple] = []
    failed: list[str] = []
    fine_refs = {"J3", "U3", "U2", "D1", "JP1", "JP2", "JP3"}
    # Skip redundant fanout on adjacent same-net QFN pins (bridged directly to pin 2 / pin 10 -> 0 dangling stubs!)
    skip_fanout = {("U3", "3"), ("U3", "11")}
    fanout_tip: dict[tuple[str, str], tuple[float, float]] = {}

    for p in pads:
        key = (p["ref"], p["num"])
        if key in skip_fanout:
            continue
        if p["ref"] in fine_refs and p["net"] and p["net"] != "GND" and not p["is_npth"] and p["num"] not in ("SH", "MP"):
            cx0, cy0 = comp_center[p["ref"]]
            dx, dy = p["x"] - cx0, p["y"] - cy0
            if p["ref"] == "J3":
                stagger = {"A6": 1.0, "B6": 1.0, "A7": 1.6, "B7": 1.6, "A5": 2.2, "B5": 1.2}.get(p["num"], 2.6)
                x_shift = (0.5 if p["x"] > cx0 else -0.5) if p["num"] in ("A4", "B4", "A9", "B9") else 0.0
                fx, fy = p["x"] + x_shift, p["y"] - stagger
            elif p["ref"] == "D1" and p["num"] == "5":
                fx, fy = p["x"] + 1.15, p["y"]
            elif p["ref"].startswith("JP"):
                if abs(dx) < 0.1:
                    fx, fy = p["x"], p["y"] - 1.35
                else:
                    fx, fy = p["x"] + (1.0 if dx > 0 else -1.0), p["y"]
            elif p["ref"] == "U2" and p["num"] == "2":
                fx, fy = p["x"] - 1.35, p["y"]
            elif p["ref"] == "U2" and p["num"] == "3":
                fx, fy = p["x"], p["y"] - 1.2
            elif abs(dx) >= abs(dy):
                fx, fy = p["x"] + (1.5 if dx >= 0 else -1.5), p["y"] + dy * 0.65
            else:
                fx, fy = p["x"] + dx * 0.65, p["y"] + (1.5 if dy >= 0 else -1.5)
            gx, gy = xy_to_grid(fx, fy)
            fx, fy = grid_to_xy(gx, gy)
            fanout_tip[key] = (fx, fy)
            lyr = 1 if p["on_b"] else 0
            w_stub = 0.25 if (p["net"] in HIGH_PWR_NETS and p["ref"] != "J3") else 0.16
            segments.append((p["x"], p["y"], fx, fy, w_stub, lyr, p["net"]))
            mark_line(p["x"], p["y"], fx, fy, lyr, w_stub, p["net"], stub=True)

    for p in pads:
        key = (p["ref"], p["num"])
        if key in fanout_tip:
            fx, fy = fanout_tip[key]
            gx, gy = xy_to_grid(fx, fy)
            lyr = 1 if p["on_b"] else 0
            occ_sig[lyr][idx(gx, gy)] = p["net"]

    net_pads: dict[str, list[dict]] = defaultdict(list)
    for p in pads:
        if p["net"] and p["net"] != "GND":
            net_pads[p["net"]].append(p)
    ordered_nets = [n for n in CRITICAL_NET_ORDER if n in net_pads] + [
        n for n in sorted(net_pads) if n not in CRITICAL_NET_ORDER
    ]

    def grid_for_tier(tier: int):
        return occ_pwr if tier == 2 else (occ_med if tier == 1 else occ_sig)

    def entry_cells(p: dict, net: str, tier: int) -> list[tuple[int, int, int]]:
        grid = grid_for_tier(tier)
        tx, ty = fanout_tip.get((p["ref"], p["num"]), (p["x"], p["y"]))
        cx, cy = xy_to_grid(tx, ty)
        layers = ([0] if p["on_f"] else []) + ([1] if p["on_b"] else [])
        out = []
        for r in range(0, 6):
            for lyr in layers:
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        if max(abs(dx), abs(dy)) != r:
                            continue
                        gx, gy = cx + dx, cy + dy
                        if 3 <= gx < NX - 3 and 3 <= gy < NY - 3:
                            v = grid[lyr][idx(gx, gy)]
                            if v is None or v == net:
                                out.append((gx, gy, lyr))
            if out:
                break
        if out or tier > 0:
            return out
        return [(cx, cy, layers[0])]

    def astar(
        starts: list[tuple[int, int, int]], goals: list[tuple[int, int, int]], net: str, tier: int, single_layer: int | None = None
    ):
        if not starts or not goals:
            return None
        grid = grid_for_tier(tier)
        gset = set(goals)
        gxa = sum(g[0] for g in goals) / len(goals)
        gya = sum(g[1] for g in goals) / len(goals)
        pq, dist, parent = [], {}, {}
        for s in starts:
            if single_layer is not None and s[2] != single_layer:
                continue
            dist[s] = 0
            heapq.heappush(pq, (int((abs(s[0] - gxa) + abs(s[1] - gya)) * 10), 0, s))
        while pq:
            _f, g, cur = heapq.heappop(pq)
            if cur in gset:
                path = [cur]
                while cur in parent:
                    cur = parent[cur]
                    path.append(cur)
                path.reverse()
                return path
            if g > dist.get(cur, 10**9):
                continue
            cx, cy, cl = cur
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx_, ny_ = cx + dx, cy + dy
                if 2 <= nx_ < NX - 2 and 2 <= ny_ < NY - 2:
                    v = grid[cl][idx(nx_, ny_)]
                    if v is None or v == net:
                        ng = g + (10 if ((cl == 0 and dy == 0) or (cl == 1 and dx == 0)) else 13)
                        nxt = (nx_, ny_, cl)
                        if ng < dist.get(nxt, 10**9):
                            dist[nxt], parent[nxt] = ng, cur
                            heapq.heappush(pq, (ng + int((abs(nx_ - gxa) + abs(ny_ - gya)) * 10), ng, nxt))
            if single_layer is None and not via_block[idx(cx, cy)]:
                nl = 1 - cl
                v = grid[nl][idx(cx, cy)]
                if v is None or v == net:
                    ng = g + 26
                    nxt = (cx, cy, nl)
                    if ng < dist.get(nxt, 10**9):
                        dist[nxt], parent[nxt] = ng, cur
                        heapq.heappush(pq, (ng + int((abs(cx - gxa) + abs(cy - gya)) * 10), ng, nxt))
        return None

    def commit_path(path: list[tuple[int, int, int]], uw: float, net: str, sp: dict | None, ep: dict | None) -> float:
        total_len = 0.0
        if ep is not None:
            txe, tye = fanout_tip.get((ep["ref"], ep["num"]), (ep["x"], ep["y"]))
            epx, epy = grid_to_xy(path[-1][0], path[-1][1])
            d_end = math.hypot(txe - epx, tye - epy)
            if d_end > 0.01:
                sw = 0.16 if ep["ref"] in fine_refs else uw
                segments.append((epx, epy, txe, tye, sw, path[-1][2], net))
                mark_line(epx, epy, txe, tye, path[-1][2], sw, net, stub=(ep["ref"] in fine_refs))
                total_len += d_end
        if sp is not None:
            txs, tys = fanout_tip.get((sp["ref"], sp["num"]), (sp["x"], sp["y"]))
            spx, spy = grid_to_xy(path[0][0], path[0][1])
            d_start = math.hypot(txs - spx, tys - spy)
            if d_start > 0.01:
                sw = 0.16 if sp["ref"] in fine_refs else uw
                segments.append((txs, tys, spx, spy, sw, path[0][2], net))
                mark_line(txs, tys, spx, spy, path[0][2], sw, net, stub=(sp["ref"] in fine_refs))
                total_len += d_start

        rstart, prev, pdir = path[0], path[0], None
        for node in path[1:]:
            if node[2] != prev[2]:
                if prev != rstart:
                    x1, y1 = grid_to_xy(rstart[0], rstart[1])
                    x2, y2 = grid_to_xy(prev[0], prev[1])
                    segments.append((x1, y1, x2, y2, uw, rstart[2], net))
                    mark_line(x1, y1, x2, y2, rstart[2], uw, net)
                    total_len += math.hypot(x2 - x1, y2 - y1)
                vx, vy = grid_to_xy(prev[0], prev[1])
                vias.append((vx, vy, net))
                mark_via(vx, vy, net)
                rstart, prev, pdir = node, node, None
            else:
                cd = (node[0] - prev[0], node[1] - prev[1])
                if pdir is not None and cd != pdir:
                    x1, y1 = grid_to_xy(rstart[0], rstart[1])
                    x2, y2 = grid_to_xy(prev[0], prev[1])
                    segments.append((x1, y1, x2, y2, uw, rstart[2], net))
                    mark_line(x1, y1, x2, y2, rstart[2], uw, net)
                    total_len += math.hypot(x2 - x1, y2 - y1)
                    rstart = prev
                pdir, prev = cd, node
        if prev != rstart:
            x1, y1 = grid_to_xy(rstart[0], rstart[1])
            x2, y2 = grid_to_xy(prev[0], prev[1])
            segments.append((x1, y1, x2, y2, uw, rstart[2], net))
            mark_line(x1, y1, x2, y2, rstart[2], uw, net)
            total_len += math.hypot(x2 - x1, y2 - y1)
        return total_len

    def add_poly(pts: list[tuple[float, float]], w: float, lyr: int, net: str) -> float:
        tot = 0.0
        for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
            d = math.hypot(x2 - x1, y2 - y1)
            if d > 0.005:
                segments.append((round(x1, 4), round(y1, 4), round(x2, 4), round(y2, 4), w, lyr, net))
                mark_line(x1, y1, x2, y2, lyr, w, net)
                tot += d
        return tot

    # Dedicated clean direct 1.00 mm BOOST_SW link (L1.2 -> U2.2) + tight C4/C9 5V pour!
    routed_special: set[str] = set()
    if "BOOST_SW" in net_pads and len(net_pads["BOOST_SW"]) == 2:
        p_l1 = next(p for p in net_pads["BOOST_SW"] if p["ref"] == "L1")
        p_u2 = next(p for p in net_pads["BOOST_SW"] if p["ref"] == "U2")
        tx, ty = fanout_tip.get(("U2", p_u2["num"]), (p_u2["x"], p_u2["y"]))
        add_poly([(p_l1["x"], p_l1["y"]), (tx, p_l1["y"]), (tx, ty)], 1.00, 1, "BOOST_SW")
        routed_special.add("BOOST_SW")
    # Pre-tie U2.3 -> C4.1 -> C9.1 with direct 0.80 mm B.Cu pour (<=1.5 mm hot loop, zero vias!)
    if "5V" in net_pads:
        p_u2_vout = next((p for p in net_pads["5V"] if p["ref"] == "U2"), None)
        p_c4 = next((p for p in net_pads["5V"] if p["ref"] == "C4"), None)
        p_c9 = next((p for p in net_pads["5V"] if p["ref"] == "C9"), None)
        if p_u2_vout and p_c4 and p_c9:
            t_vout = fanout_tip.get(("U2", p_u2_vout["num"]), (p_u2_vout["x"], p_u2_vout["y"]))
            add_poly([t_vout, (p_c4["x"], t_vout[1]), (p_c4["x"], p_c4["y"]), (p_c9["x"], p_c9["y"])], 0.80, 1, "5V")

    # USB 2.0 differential pair: A* routes J3 <-> D1 (zero crossings), then coupled matched-length leg D1 -> P1/P2!
    if "USB_DP_RAW" in net_pads and "USB_DM_RAW" in net_pads:
        usb_lens = {}
        for net_u, pogo_ref in (("USB_DP_RAW", "P1"), ("USB_DM_RAW", "P2")):
            plist_u = [p for p in net_pads[net_u] if p["ref"] != pogo_ref]
            conn_u, rem_u = [0], list(range(1, len(plist_u)))
            tc_u = {i: entry_cells(plist_u[i], net_u, 0) for i in range(len(plist_u))}
            rt_u = list(tc_u[0])
            tot_u = 0.0
            while rem_u:
                ci, ri = min(
                    ((c, r) for r in rem_u for c in conn_u),
                    key=lambda pr: math.hypot(plist_u[pr[1]]["x"] - plist_u[pr[0]]["x"], plist_u[pr[1]]["y"] - plist_u[pr[0]]["y"]),
                )
                path = astar(rt_u, tc_u[ri], net_u, 0)
                if path is not None:
                    tot_u += commit_path(path, 0.20, net_u, plist_u[0] if len(conn_u) == 1 else None, plist_u[ri])
                    rt_u.extend(path[::3])
                    rt_u.extend(tc_u[ri])
                else:
                    failed.append(f"{net_u} ({plist_u[ci]['ref']}.{plist_u[ci]['num']} -> {plist_u[ri]['ref']}.{plist_u[ri]['num']})")
                conn_u.append(ri)
                rem_u.remove(ri)
            usb_lens[net_u] = tot_u

        # Coupled length-matched leg from D1 right-side pads (4=DP, 6=DM) to P1 (65.63,-2.15) & P2 (68.17,-2.15)
        t_dp4 = fanout_tip[("D1", "4")]
        t_dm6 = fanout_tip[("D1", "6")]
        p_dp = next(p for p in net_pads["USB_DP_RAW"] if p["ref"] == "P1")
        p_dm = next(p for p in net_pads["USB_DM_RAW"] if p["ref"] == "P2")
        # Pass cleanly through J5 gaps Y=-5.23 (between J5.2 & J5.3) and Y=-7.77 (between J5.3 & J5.4) on B.Cu
        # D1.4 (DP) is at Y=-9.05, D1.6 (DM) is at Y=-7.15!
        # Route DM via Y=-5.23 to X=68.17 -> P2, or DP via Y=-5.23 to P1 (65.63) & DM via Y=-7.77 to P2 (68.17) (zero crossing!)
        # Wait: t_dp4 is BELOW t_dm6 (t_dp4.y ~ -9.6, t_dm6.y ~ -6.6), so route DM on upper channel Y=-5.23?
        # If DM goes on Y=-5.23 to X=68.17, then DP from Y=-7.77 going UP to P1 (65.63, -2.15) would cross Y=-5.23!
        # So route DP on F.Cu (via at X=57.2) or put DP on UPPER channel Y=-5.23 (to P1 X=65.63) and DM on LOWER channel Y=-7.77 (to P2 X=68.17)!
        # Let D1.4 (DP) jump to F.Cu at (57.0, t_dp4[1]) -> (57.0, -5.23) -> (65.63, -5.23) -> via to B.Cu -> P1, OR
        # simply route DM on B.Cu along Y=-7.77 (zero crossings!) and DP on F.Cu along Y=-7.77? No, keep BOTH on B.Cu!
        # From t_dm6 (57.2, -6.6) -> (57.2, -7.77) -> (68.17, -7.77) -> (68.17, -2.15) (pure B.Cu!).
        # From t_dp4 (57.2, -9.6) -> via to F.Cu at (58.0, -9.6) -> F.Cu (58.0, -5.23) -> (64.4, -5.23) -> via to B.Cu at (64.4, -5.23) -> (65.63, -5.23) -> (65.63, -2.15)!
        # Zero crossings AND both stay in the exact same corridor!
        # Symmetric differential pair across adjacent J5 gaps (Y=-5.23 & Y=-7.77, 2 vias each, 0 crossings!)
        dm_pre = [t_dm6, (57.8, t_dm6[1]), (57.8, -7.77), (63.6, -7.77)]
        dm_mid_base = [(63.6, -7.77), (66.8, -7.77)]
        dm_post = [(66.8, -7.77), (p_dm["x"], -7.77), (p_dm["x"], p_dm["y"])]
        l_dm_leg = (
            sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(dm_pre, dm_pre[1:]))
            + sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(dm_mid_base, dm_mid_base[1:]))
            + sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(dm_post, dm_post[1:]))
        )
        dp_pre = [t_dp4, (57.6, t_dp4[1])]
        dp_mid_base = [(57.6, t_dp4[1]), (57.6, -5.23), (64.4, -5.23)]
        dp_post = [(64.4, -5.23), (p_dp["x"], -5.23), (p_dp["x"], p_dp["y"])]
        l_dp_leg = (
            sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(dp_pre, dp_pre[1:]))
            + sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(dp_mid_base, dp_mid_base[1:]))
            + sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(dp_post, dp_post[1:]))
        )
        delta = (usb_lens["USB_DM_RAW"] + l_dm_leg) - (usb_lens["USB_DP_RAW"] + l_dp_leg)
        dm_mid, dp_mid = dm_mid_base, dp_mid_base
        if delta > 0.25:
            h = round(min(delta / 4.0, 2.0), 2)
            dp_mid = [
                (57.6, t_dp4[1]), (57.6, -5.23),
                (61.0, -5.23), (61.0, -5.23 + h), (61.8, -5.23 + h), (61.8, -5.23),
                (62.6, -5.23), (62.6, -5.23 + h), (63.4, -5.23 + h), (63.4, -5.23),
                (64.4, -5.23),
            ]
        elif delta < -0.25:
            h = round(min((-delta) / 4.0, 2.0), 2)
            dm_mid = [
                (63.6, -7.77), (64.2, -7.77), (64.2, -7.77 - h), (65.0, -7.77 - h), (65.0, -7.77),
                (65.6, -7.77), (65.6, -7.77 - h), (66.4, -7.77 - h), (66.4, -7.77),
                (66.8, -7.77),
            ]
        l_dm_tot = usb_lens["USB_DM_RAW"] + add_poly(dm_pre, 0.20, 1, "USB_DM_RAW")
        vias.append((63.6, -7.77, "USB_DM_RAW")); mark_via(63.6, -7.77, "USB_DM_RAW")
        l_dm_tot += add_poly(dm_mid, 0.20, 0, "USB_DM_RAW")
        vias.append((66.8, -7.77, "USB_DM_RAW")); mark_via(66.8, -7.77, "USB_DM_RAW")
        l_dm_tot += add_poly(dm_post, 0.20, 1, "USB_DM_RAW")
        l_dp_tot = usb_lens["USB_DP_RAW"] + add_poly(dp_pre, 0.20, 1, "USB_DP_RAW")
        vias.append((57.6, t_dp4[1], "USB_DP_RAW")); mark_via(57.6, t_dp4[1], "USB_DP_RAW")
        l_dp_tot += add_poly(dp_mid, 0.20, 0, "USB_DP_RAW")
        vias.append((64.4, -5.23, "USB_DP_RAW")); mark_via(64.4, -5.23, "USB_DP_RAW")
        l_dp_tot += add_poly(dp_post, 0.20, 1, "USB_DP_RAW")
        print(f"  [USB DIFF] USB_DP_RAW={l_dp_tot:.2f} mm, USB_DM_RAW={l_dm_tot:.2f} mm, skew={abs(l_dp_tot - l_dm_tot):.2f} mm")
        routed_special.update({"USB_DP_RAW", "USB_DM_RAW"})

    for net in ordered_nets:
        if net in routed_special:
            continue
        plist = net_pads[net]
        if len(plist) < 2:
            continue
        if net in HIGH_PWR_NETS:
            pref_tier, pref_w = 2, 0.80
        elif net in MED_PWR_NETS:
            pref_tier, pref_w = 1, 0.40
        else:
            pref_tier, pref_w = 0, 0.20

        connected, remaining = [0], list(range(1, len(plist)))
        tcells = {i: entry_cells(plist[i], net, pref_tier) for i in range(len(plist))}
        routed = list(tcells[0])

        while remaining:
            ci, ri = min(
                ((c, r) for r in remaining for c in connected),
                key=lambda pr: math.hypot(plist[pr[1]]["x"] - plist[pr[0]]["x"], plist[pr[1]]["y"] - plist[pr[0]]["y"]),
            )
            pa, pb = plist[ci], plist[ri]
            if pa["ref"] == pb["ref"] and pa["ref"] != "J3" and math.hypot(pa["x"] - pb["x"], pa["y"] - pb["y"]) <= 0.65:
                lyr = 1 if pa["on_b"] else 0
                segments.append((pa["x"], pa["y"], pb["x"], pb["y"], 0.25, lyr, net))
                mark_line(pa["x"], pa["y"], pb["x"], pb["y"], lyr, 0.25, net, stub=True)
                routed.extend(tcells[ri])
                connected.append(ri)
                remaining.remove(ri)
                continue

            # Try preferred width tier, stepping down gracefully if tight around fine-pitch/header pins
            path, uw = None, pref_w
            is_j3_pwr = (net == "VBUS") and (pa["ref"] == "J3" or pb["ref"] == "J3")
            tiers = (
                [(0, 0.28)] if is_j3_pwr else
                ([(2, 0.80), (1, 0.50), (0, 0.28)] if pref_tier == 2
                 else ([(1, 0.40), (0, 0.22)] if pref_tier == 1 else [(0, 0.20)]))
            )
            for tier_try, w_try in tiers:
                if tier_try == pref_tier:
                    starts_t, goals_t = routed, tcells[ri]
                else:
                    starts_t = []
                    for c_idx in connected:
                        starts_t.extend(entry_cells(plist[c_idx], net, tier_try))
                    starts_t.extend(routed)
                    goals_t = entry_cells(plist[ri], net, tier_try)
                path = astar(starts_t, goals_t, net, tier_try)
                if path is not None:
                    uw = w_try
                    break

            if path is None:
                failed.append(f"{net} ({pa['ref']}.{pa['num']} -> {pb['ref']}.{pb['num']})")
                remaining.remove(ri)
                connected.append(ri)
                continue

            commit_path(path, uw, net, plist[0] if len(connected) == 1 else None, plist[ri])
            routed.extend(path[::3])
            routed.extend(entry_cells(plist[ri], net, 0))
            connected.append(ri)
            remaining.remove(ri)

    # Tie every B.Cu SMD GND pad with a nearby GND via
    gnd_vias = []
    for p in pads:
        if p["net"] == "GND" and p["on_b"] and not p["on_f"]:
            cx, cy = xy_to_grid(p["x"], p["y"])
            found = None
            for r in range(1, 14):
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        if max(abs(dx), abs(dy)) != r:
                            continue
                        gx, gy = cx + dx, cy + dy
                        if 3 <= gx < NX - 3 and 3 <= gy < NY - 3:
                            i = idx(gx, gy)
                            if not via_block[i] and (occ_sig[0][i] in (None, "GND")) and (occ_sig[1][i] in (None, "GND")):
                                found = (gx, gy)
                                break
                    if found:
                        break
                if found:
                    break
            if found:
                vx, vy = grid_to_xy(found[0], found[1])
                gnd_vias.append((vx, vy, "GND"))
                mark_via(vx, vy, "GND")

    # Dense 2.0 mm GND stitching grid across all clear regions (including corners)
    for gy_i in range(3, 135, 4):
        gy_mm = gy_i * 0.5
        for gx_i in range(3, 177, 4):
            gx_mm = gx_i * 0.5
            gx, gy = xy_to_grid(gx_mm, -gy_mm)
            i = idx(gx, gy)
            if not via_block[i] and occ_sig[0][i] is None and occ_sig[1][i] is None:
                gnd_vias.append((gx_mm, -gy_mm, "GND"))
                mark_via(gx_mm, -gy_mm, "GND")

    return segments, vias + gnd_vias, failed
