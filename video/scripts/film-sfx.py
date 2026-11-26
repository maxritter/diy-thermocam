"""Sound effects for the launch film from ElevenLabs Sound Effects (one take each, kept on disk).

    python3 scripts/film-sfx.py [--force name]

Writes public/sfx/film/<name>.mp3 and a receipt next to it. Existing files are kept, so a
re-run only fills in what is missing. The API key is read from ~/.config/elevenlabs/api-key.
"""

import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "public" / "sfx" / "film"

SOUNDS = {
    "pour": ("Hot coffee poured from a pot into a ceramic mug, close up, quiet room, ends with the mug full", 5.5),
    "seat": ("A small electronic module pressed firmly into a pin header, one soft precise plastic click, close up, dry", 0.6),
    "screw": ("A small Phillips screwdriver turning a tiny screw into a plastic case: two quick soft mechanical turns and a snug final click, close up, natural, dry room", 0.8),
    "switch": ("A small slide power switch on a handheld device flipped on, one crisp soft click, dry", 0.5),
    "cable": ("A USB-C cable plugged into a device port, one soft solid click, close up, dry", 0.5),
    "tick": ("A single very soft high wooden tick, like a quiet click wheel, dry, very short", 0.5),
    "shutter": ("A soft electronic camera shutter blip, gentle, modern, dry", 0.5),
    "cap": ("A small plastic lens cap sliding open, soft smooth slide ending in a gentle click", 0.8),
    "hum": ("A warm soft low electronic power-on hum swelling for a second, then settling, very subtle", 1.6),
}


def main() -> None:
    key = (Path.home() / ".config/elevenlabs/api-key").read_text().strip()
    force = set(sys.argv[sys.argv.index("--force") + 1].split(",")) if "--force" in sys.argv else set()
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (text, seconds) in SOUNDS.items():
        out = OUT / f"{name}.mp3"
        if out.exists() and name not in force:
            continue
        body = json.dumps({"text": text, "duration_seconds": seconds, "prompt_influence": 0.6}).encode()
        req = urllib.request.Request("https://api.elevenlabs.io/v1/sound-generation?output_format=mp3_44100_128", data=body,
                                     headers={"xi-api-key": key, "Content-Type": "application/json"})
        headers: dict[str, str] = {}
        for attempt in range(1, 5):
            try:
                with urllib.request.urlopen(req, timeout=120) as r:
                    out.write_bytes(r.read())
                    headers = dict(r.headers)
                break
            except urllib.error.HTTPError as e:
                print(f"{name}: HTTP {e.code} {e.read()[:200]!r}", file=sys.stderr)
                if e.code not in (429, 500, 502, 503) or attempt == 4:
                    raise
                time.sleep(15 * attempt)
        receipt = {"text": text, "seconds": seconds, "character_cost": headers.get("character-cost") or headers.get("Character-Cost"),
                   "request_id": headers.get("request-id") or headers.get("Request-Id")}
        (OUT / f"{name}.receipt.json").write_text(json.dumps(receipt, indent=1))
        print(f"{name}: {out.stat().st_size // 1024} KB, cost {receipt['character_cost']}")


if __name__ == "__main__":
    main()
