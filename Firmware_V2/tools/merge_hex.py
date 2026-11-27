#!/usr/bin/env python3
"""Merge Teensy 3.1/3.2 main firmware Intel HEX with MassStorage.hex (at 0x38880).

Validates strictly that:
  1. Main firmware occupies [0x000000, <0x038880) with zero overlap with MassStorage.hex.
  2. MassStorage.hex vector table at 0x038880..0x038887 is present and intact.
  3. Total image fits within Teensy 3.1/3.2 256 KB flash (0x040000).
"""
import sys
from pathlib import Path

MSD_BASE = 0x038880
FLASH_LIMIT = 0x040000


def parse_ihex(path: Path) -> dict[int, int]:
    mem: dict[int, int] = {}
    base = 0
    for lineno, raw in enumerate(path.read_text(encoding="ascii").splitlines(), 1):
        line = raw.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise ValueError(f"{path}:{lineno}: missing ':'")
        data = bytes.fromhex(line[1:])
        if (sum(data) & 0xFF) != 0:
            raise ValueError(f"{path}:{lineno}: bad checksum")
        cnt = data[0]
        addr = (data[1] << 8) | data[2]
        rtype = data[3]
        payload = data[4 : 4 + cnt]
        if rtype == 0x00:
            for i, b in enumerate(payload):
                a = base + addr + i
                if a in mem:
                    raise ValueError(f"{path}:{lineno}: duplicate address 0x{a:06X}")
                mem[a] = b
        elif rtype == 0x01:
            break
        elif rtype == 0x02:
            base = ((payload[0] << 8) | payload[1]) << 4
        elif rtype == 0x04:
            base = ((payload[0] << 8) | payload[1]) << 16
        elif rtype in (0x03, 0x05):
            pass
        else:
            raise ValueError(f"{path}:{lineno}: unsupported record type 0x{rtype:02X}")
    return mem


def ihex_record(addr16: int, rtype: int, payload: bytes) -> str:
    hdr = bytes([len(payload), (addr16 >> 8) & 0xFF, addr16 & 0xFF, rtype]) + payload
    ck = (-sum(hdr)) & 0xFF
    return ":" + hdr.hex().upper() + f"{ck:02X}"


def write_ihex(path: Path, mem: dict[int, int]) -> None:
    lines: list[str] = []
    addrs = sorted(mem.keys())
    cur_hi = None
    i = 0
    n = len(addrs)
    while i < n:
        start = addrs[i]
        hi = start >> 16
        if hi != cur_hi:
            lines.append(ihex_record(0, 0x04, bytes([(hi >> 8) & 0xFF, hi & 0xFF])))
            cur_hi = hi
        chunk = bytearray([mem[start]])
        prev = start
        i += 1
        while i < n and len(chunk) < 16:
            nxt = addrs[i]
            if nxt != prev + 1 or (nxt >> 16) != hi:
                break
            chunk.append(mem[nxt])
            prev = nxt
            i += 1
        lines.append(ihex_record(start & 0xFFFF, 0x00, bytes(chunk)))
    lines.append(":00000001FF")
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def main() -> int:
    if len(sys.argv) != 4:
        print("Usage: merge_hex.py <main.hex> <MassStorage.hex> <out.hex>", file=sys.stderr)
        return 2
    main_path, msd_path, out_path = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    main_mem = parse_ihex(main_path)
    msd_mem = parse_ihex(msd_path)

    main_max = max(main_mem.keys()) + 1
    msd_min = min(msd_mem.keys())
    msd_max = max(msd_mem.keys()) + 1

    if main_max > MSD_BASE:
        print(
            f"ERROR: Main V1 firmware ends at 0x{main_max:06X} ({main_max:,} B), "
            f"overlapping MassStorage base 0x{MSD_BASE:06X} ({MSD_BASE:,} B)!",
            file=sys.stderr,
        )
        return 1
    if msd_min != MSD_BASE:
        print(
            f"ERROR: MassStorage.hex starts at 0x{msd_min:06X}, expected 0x{MSD_BASE:06X}",
            file=sys.stderr,
        )
        return 1
    if msd_max > FLASH_LIMIT:
        print(f"ERROR: Combined image exceeds 256 KB flash (0x{msd_max:06X})", file=sys.stderr)
        return 1

    overlap = set(main_mem.keys()) & set(msd_mem.keys())
    if overlap:
        print(f"ERROR: {len(overlap)} overlapping bytes!", file=sys.stderr)
        return 1

    # Verify vector table at 0x38880 (initial SP in SRAM 0x1FFF8000..0x20008000, reset PC in 0x38880..0x40000)
    sp = int.from_bytes(bytes(msd_mem[MSD_BASE + k] for k in range(4)), "little")
    pc = int.from_bytes(bytes(msd_mem[MSD_BASE + 4 + k] for k in range(4)), "little")
    if not (0x1FFF8000 <= sp <= 0x20008000 and MSD_BASE <= pc < FLASH_LIMIT and (pc & 1) == 1):
        print(f"ERROR: Invalid MassStorage vector table SP=0x{sp:08X} PC=0x{pc:08X}", file=sys.stderr)
        return 1

    merged = dict(main_mem)
    merged.update(msd_mem)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    write_ihex(out_path, merged)
    print(
        f"Merged V1 HEX OK: main={len(main_mem):,} B (0x000000..0x{main_max:06X}, "
        f"{MSD_BASE - main_max:,} B headroom), MSD={len(msd_mem):,} B "
        f"(0x{msd_min:06X}..0x{msd_max:06X}, SP=0x{sp:08X}, PC=0x{pc:08X}) -> {out_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
