#!/usr/bin/env python3
"""Validate hardware/bom/bom.csv against hardware/bom/schema.json.

Pure Python 3 standard library. Supports schema validation, a cost rollup and
optional live HTTP link verification (--check-links).
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import ssl
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Any

BOM_DIR = Path(__file__).resolve().parent
DEFAULT_BOM = BOM_DIR / "bom.csv"
DEFAULT_SCHEMA = BOM_DIR / "schema.json"

# Distributor/storefront hosts known to issue WAF 403/429 or soft-block pages to
# automated HTTP clients even though the direct product URL is valid in browsers.
WAF_PROTECTED_HOSTS = {
    "store.groupgets.com",
    "groupgets.com",
    "www.buydisplay.com",
    "buydisplay.com",
    "www.mouser.com",
    "mouser.com",
    "www.digikey.com",
    "digikey.com",
    "tinytronics.nl",
    "www.tinytronics.nl",
    "www.addicore.com",
    "addicore.com",
}

SOFT_404_PATTERNS = [
    re.compile(r"<title[^>]*>\s*page not found\b", re.I),
    re.compile(r"<title[^>]*>\s*404\b", re.I),
    re.compile(r"PRODUCTS WITHDRAWN", re.I),
]

SOFT_WAF_PATTERNS = [
    re.compile(r"<title[^>]*>\s*Access to this page has been denied", re.I),
    re.compile(r"Just a moment\.\.\.", re.I),
    re.compile(r"Attention Required!\s*\|\s*Cloudflare", re.I),
]


def load_schema(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def validate_row(
    row_num: int,
    raw_row: dict[str, str],
    schema: dict[str, Any],
    seen_ids: set[str],
) -> tuple[dict[str, Any], list[str]]:
    errors: list[str] = []
    props = schema["properties"]
    required = schema["required"]
    parsed: dict[str, Any] = {}

    for col in required:
        if col not in raw_row or raw_row[col] is None:
            errors.append(f"row {row_num}: missing column '{col}'")
            continue

        raw_val = raw_row[col].strip()
        spec = props[col]
        expected_type = spec["type"]

        if expected_type == "integer":
            try:
                val: Any = int(raw_val)
            except ValueError:
                errors.append(f"row {row_num} ({raw_row.get('id', '?')}): '{col}' must be integer, got {raw_val!r}")
                continue
            if "minimum" in spec and val < spec["minimum"]:
                errors.append(f"row {row_num} ({raw_row.get('id', '?')}): '{col}'={val} < minimum {spec['minimum']}")
            parsed[col] = val

        elif expected_type == "number":
            try:
                val = float(raw_val)
            except ValueError:
                errors.append(f"row {row_num} ({raw_row.get('id', '?')}): '{col}' must be number, got {raw_val!r}")
                continue
            if "minimum" in spec and val < spec["minimum"]:
                errors.append(f"row {row_num} ({raw_row.get('id', '?')}): '{col}'={val} < minimum {spec['minimum']}")
            parsed[col] = val

        elif expected_type == "string":
            val = raw_val
            if "minLength" in spec and len(val) < spec["minLength"]:
                errors.append(
                    f"row {row_num} ({raw_row.get('id', '?')}): '{col}' length {len(val)} < minLength {spec['minLength']}"
                )
            if "enum" in spec and val not in spec["enum"]:
                errors.append(
                    f"row {row_num} ({raw_row.get('id', '?')}): '{col}'={val!r} not in {spec['enum']}"
                )
            if "pattern" in spec and not re.match(spec["pattern"], val):
                errors.append(
                    f"row {row_num} ({raw_row.get('id', '?')}): '{col}'={val!r} does not match {spec['pattern']}"
                )
            parsed[col] = val

    # Uniqueness and cross-field integrity checks
    part_id = parsed.get("id")
    if isinstance(part_id, str) and part_id:
        if part_id in seen_ids:
            errors.append(f"row {row_num}: duplicate id '{part_id}'")
        seen_ids.add(part_id)

    if parsed.get("alt_url") and not parsed.get("alt_supplier"):
        errors.append(f"row {row_num} ({part_id}): 'alt_url' set without 'alt_supplier'")
    if parsed.get("alt_supplier") and not parsed.get("alt_url"):
        errors.append(f"row {row_num} ({part_id}): 'alt_supplier' set without 'alt_url'")

    if parsed.get("category") == "pcb_smd" and parsed.get("pcba_tier") in ("basic", "extended"):
        if not parsed.get("lcsc_part"):
            errors.append(
                f"row {row_num} ({part_id}): pcb_smd part with pcba_tier={parsed.get('pcba_tier')} requires lcsc_part"
            )

    return parsed, errors


def check_url(url: str, timeout: float = 12.0) -> dict[str, Any]:
    host = urllib.parse.urlparse(url).hostname or ""
    ctx = ssl.create_default_context()
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": (
                "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                "AppleWebKit/537.36 (KHTML, like Gecko) "
                "Chrome/131.0.0.0 Safari/537.36"
            ),
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language": "en-US,en;q=0.9",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout, context=ctx) as resp:
            status = resp.status
            body = resp.read(32768).decode("utf-8", errors="ignore")
            title_match = re.search(r"<title[^>]*>(.*?)</title>", body, re.I | re.S)
            title = re.sub(r"\s+", " ", title_match.group(1)).strip()[:80] if title_match else ""

            for pat in SOFT_WAF_PATTERNS:
                if pat.search(body):
                    return {
                        "url": url,
                        "status": status,
                        "classification": "WAF" if host in WAF_PROTECTED_HOSTS else "WARN",
                        "detail": f"soft-WAF ({title or host})",
                    }
            for pat in SOFT_404_PATTERNS:
                if pat.search(body):
                    return {
                        "url": url,
                        "status": status,
                        "classification": "BROKEN",
                        "detail": f"soft-404 ({title})",
                    }
            return {
                "url": url,
                "status": status,
                "classification": "OK",
                "detail": title,
            }
    except urllib.error.HTTPError as exc:
        if exc.code in (403, 429) and host in WAF_PROTECTED_HOSTS:
            return {
                "url": url,
                "status": exc.code,
                "classification": "WAF",
                "detail": f"distributor WAF/rate-limit ({host})",
            }
        return {
            "url": url,
            "status": exc.code,
            "classification": "BROKEN",
            "detail": f"HTTP {exc.code} {exc.reason}",
        }
    except Exception as exc:  # noqa: BLE001
        return {
            "url": url,
            "status": 0,
            "classification": "BROKEN",
            "detail": f"{type(exc).__name__}: {exc}",
        }


def run_link_checks(rows: list[dict[str, Any]], strict_waf: bool) -> int:
    url_refs: dict[str, list[str]] = defaultdict(list)
    for row in rows:
        if row.get("primary_url"):
            url_refs[row["primary_url"]].append(f"{row['id']} (primary)")
        if row.get("alt_url"):
            url_refs[row["alt_url"]].append(f"{row['id']} (alt)")

    # Group by host to avoid slamming any single distributor simultaneously
    by_host: dict[str, list[str]] = defaultdict(list)
    for url in sorted(url_refs):
        host = urllib.parse.urlparse(url).hostname or "unknown"
        by_host[host].append(url)

    def check_host_queue(urls: list[str]) -> list[dict[str, Any]]:
        out = []
        for idx, u in enumerate(urls):
            if idx > 0:
                time.sleep(0.35)
            out.append(check_url(u))
        return out

    results: list[dict[str, Any]] = []
    with ThreadPoolExecutor(max_workers=6) as pool:
        futs = [pool.submit(check_host_queue, urls) for urls in by_host.values()]
        for fut in as_completed(futs):
            results.extend(fut.result())

    results.sort(key=lambda r: (r["classification"], r["url"]))
    counts = defaultdict(int)
    print("\n=== Link Verification ===")
    for res in results:
        cls = res["classification"]
        counts[cls] += 1
        refs = ", ".join(url_refs[res["url"]])
        print(f"[{cls:6s}] HTTP {res['status']:3d} | {res['url']} | {res['detail']}  <-- {refs}")

    print(
        f"\nLink summary: {len(results)} unique URLs -> "
        f"{counts['OK']} OK, {counts['WAF']} WAF-protected, {counts['BROKEN']} BROKEN"
    )
    if counts["BROKEN"] > 0:
        return 1
    if strict_waf and counts["WAF"] > 0:
        return 1
    return 0


def summarize_revision(rows: list[dict[str, Any]], rev: str) -> None:
    included = [
        r
        for r in rows
        if r["hw_rev"] in ("all", rev)
        and r["pcba_tier"] != "optional"
        and r["status"] != "optional"
        and r["status"] != "eol_notice"
    ]
    total = sum(r["qty"] * r["unit_price_usd"] for r in included)
    by_cat: dict[str, float] = defaultdict(float)
    by_tier: dict[str, int] = defaultdict(int)
    for r in included:
        by_cat[r["category"]] += r["qty"] * r["unit_price_usd"]
        by_tier[r["pcba_tier"]] += 1

    print(f"\n=== {rev.upper()} BOM Summary ({len(included)} required line items) ===")
    print(f"Estimated total (with Lepton 3.5): ${total:,.2f} USD")
    lepton_cost = sum(r["qty"] * r["unit_price_usd"] for r in included if r["id"] == "lepton-3-5")
    print(f"Estimated total (excl. Lepton 3.5): ${total - lepton_cost:,.2f} USD")
    tier_str = ", ".join(f"{k}={v}" for k, v in sorted(by_tier.items()))
    print(f"Assembly tiers: {tier_str}")
    cat_str = ", ".join(f"{k}=${v:.2f}" for k, v in sorted(by_cat.items(), key=lambda kv: -kv[1]))
    print(f"Cost by category: {cat_str}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate DIY-Thermocam BOM CSV.")
    parser.add_argument("--bom", type=Path, default=DEFAULT_BOM, help="Path to bom.csv")
    parser.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA, help="Path to schema.json")
    parser.add_argument("--check-links", action="store_true", help="Verify all HTTP URLs live")
    parser.add_argument(
        "--strict-waf",
        action="store_true",
        help="Treat distributor WAF 403/429 responses as errors when --check-links is enabled",
    )
    args = parser.parse_args()

    schema = load_schema(args.schema)
    expected_headers = schema["required"]

    with args.bom.open("r", encoding="utf-8", newline="") as fh:
        reader = csv.DictReader(fh)
        if reader.fieldnames != expected_headers:
            print(
                f"ERROR: CSV headers mismatch.\n  expected: {expected_headers}\n  actual:   {reader.fieldnames}",
                file=sys.stderr,
            )
            return 1

        parsed_rows: list[dict[str, Any]] = []
        all_errors: list[str] = []
        seen_ids: set[str] = set()
        for row_num, raw_row in enumerate(reader, start=2):
            parsed, errs = validate_row(row_num, raw_row, schema, seen_ids)
            all_errors.extend(errs)
            if not errs:
                parsed_rows.append(parsed)

    if all_errors:
        print(f"Schema validation FAILED with {len(all_errors)} error(s):", file=sys.stderr)
        for err in all_errors:
            print(f"  - {err}", file=sys.stderr)
        return 1

    print(f"Schema validation PASSED: {len(parsed_rows)} rows in {args.bom.name} match {args.schema.name}.")
    summarize_revision(parsed_rows, "v4.0")

    if args.check_links:
        return run_link_checks(parsed_rows, strict_waf=args.strict_waf)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
