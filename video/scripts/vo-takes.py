# /// script
# requires-python = ">=3.11"
# dependencies = ["faster-whisper==1.2.1", "numpy"]
# ///
"""Record several takes of each new narration line and keep the best one.

    uv run scripts/vo-takes.py film [--takes 3] [--only id,id]

For every line in vo/<video>.json whose recording is missing from public/vo/<video>/, this
records N takes with Max's voice clone (personal_voice.py), transcribes each with local Whisper,
and asks Gemini Neon (Apollo bridge, audio in) which take pronounces the names best and sounds
most natural. The winner is copied to the path scripts/vo.mjs expects, so a following
`node scripts/vo.mjs <video>` only aligns. Takes and verdicts stay in work/takes/<video>/.
"""

import argparse
import base64
import difflib
import hashlib
import json
import re
import shutil
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

import numpy as np
from faster_whisper import WhisperModel

ROOT = Path(__file__).resolve().parent.parent
HOME = Path.home()
HELPER = HOME / ".claude/skills/personal-voice/scripts/personal_voice.py"

# How the names should sound, for the judge
SAY = """Pronunciation guide (the speaker is German, narrating in English; a light accent is fine):
- FLIR: one syllable, "fleer" (rhymes with "beer"), never spelled out.
- Lepton: "LEP-ton".
- Teensy: "TEEN-zee".
- DIY-Thermocam: "D-I-Y Thermo-cam", letters spelled out.
- V4: "vee four". Firmware 4.0: "four point oh" or "four point zero".
- USB-C: "U-S-B C". diy-thermocam.net: "D-I-Y thermocam dot net".
"""


def norm(s: str) -> str:
    return re.sub(r"[^a-z0-9]", "", s.lower())


def pcm16k(path: Path) -> np.ndarray:
    raw = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(path), "-f", "s16le", "-ac", "1", "-ar", "16000", "-"])
    return np.frombuffer(raw, np.int16).astype(np.float32) / 32768.0


def wer(script: str, heard: str) -> float:
    a = [norm(w) for w in script.split() if norm(w)]
    b = [norm(w) for w in heard.split() if norm(w)]
    sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
    same = sum(bl.size for bl in sm.get_matching_blocks())
    return 1 - same / max(len(a), 1)


def gemini(parts: list, timeout: int = 300) -> str:
    key = subprocess.check_output(["apollo-bridge", "key"], text=True).strip()
    body = json.dumps({"contents": [{"role": "user", "parts": parts}],
                       "generationConfig": {"temperature": 0.2, "maxOutputTokens": 4000,
                                            "responseMimeType": "application/json"}}).encode()
    req = urllib.request.Request("http://127.0.0.1:8067/v1beta/models/gemini-4-neon-eu:generateContent", data=body,
                                 headers={"Content-Type": "application/json", "x-goog-api-key": key})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        res = json.load(r)
    return "".join(p.get("text", "") for p in res["candidates"][0]["content"]["parts"])


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("video")
    ap.add_argument("--takes", type=int, default=3)
    ap.add_argument("--only", default="")
    args = ap.parse_args()

    script = json.loads((ROOT / "vo" / f"{args.video}.json").read_text())
    profile = json.loads((HOME / ".config/personal-voice/profile.json").read_text())
    voice = f"{profile['voice_id']}|{profile['model_id']}|{profile['stability']}|{profile['similarity_boost']}|{script['language']}"
    only = set(filter(None, args.only.split(",")))
    takes_dir = ROOT / "work" / "takes" / args.video
    takes_dir.mkdir(parents=True, exist_ok=True)
    model = WhisperModel("small.en", device="cpu", compute_type="int8")

    for c in script["chunks"]:
        if only and c["id"] not in only:
            continue
        text = c["text"].strip()
        key = hashlib.sha256(f"{voice}\n{text}".encode()).hexdigest()[:10]
        final = ROOT / "public" / "vo" / args.video / f"{c['id']}.{key}.mp3"
        if final.exists():
            continue
        tf = takes_dir / f"{c['id']}.{key}.txt"
        tf.write_text(c.get("say", text).strip() + "\n")  # "say" respells a name for the voice only
        takes = []
        for n in range(1, args.takes + 1):
            out = takes_dir / f"{c['id']}.{key}.t{n}.mp3"
            for attempt in range(1, 6):
                if out.exists():
                    break
                run = subprocess.run(["python3", str(HELPER), "synthesize", "--text-file", str(tf),
                                      "--output", str(out), "--language", script["language"]], capture_output=True, text=True)
                if run.returncode == 0:
                    break
                print(run.stderr[-400:], file=sys.stderr)
                time.sleep(15 * attempt)
            if not out.exists():
                sys.exit(f"recording {c['id']} take {n} failed")
            segs, _ = model.transcribe(pcm16k(out), language="en", beam_size=5)
            heard = " ".join(s.text.strip() for s in segs)
            dur = float(subprocess.check_output(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(out)]))
            takes.append({"take": n, "file": out.name, "heard": heard, "wer": round(wer(text, heard), 3), "seconds": round(dur, 2)})
            print(f"  {c['id']} t{n}: {dur:.1f}s wer {takes[-1]['wer']:.2f}  {heard}")

        parts = [{"text": f"{SAY}\nScript line: \"{text}\" (sent to the voice as \"{c.get('say', text)}\")\nBelow are {len(takes)} takes of this line by the same voice."}]
        for t in takes:
            parts.append({"text": f"Take {t['take']}:"})
            parts.append({"inline_data": {"mime_type": "audio/mpeg", "data": base64.b64encode((takes_dir / t['file']).read_bytes()).decode()}})
        parts.append({"text": "Listen to every take. Judge: every word clearly said and none missing; names pronounced as in the guide; "
                              "natural, calm product-film delivery with even pace and no odd stress, glitches, breaths or artifacts. "
                              "Reply as JSON: {\"best\": <take number>, \"takes\": [{\"take\": n, \"score\": 0-10, "
                              "\"mispronounced\": [words], \"notes\": \"...\"}]}"})
        verdict = None
        for attempt in range(3):
            try:
                verdict = json.loads(gemini(parts))
                break
            except Exception as e:  # bridge hiccup: retry, then fall back to Whisper
                print(f"  judge failed ({e}); retrying", file=sys.stderr)
                time.sleep(10)
        if verdict and isinstance(verdict.get("best"), int) and 1 <= verdict["best"] <= len(takes):
            best = verdict["best"]
        else:
            best = min(takes, key=lambda t: t["wer"])["take"]
        (takes_dir / f"{c['id']}.{key}.verdict.json").write_text(json.dumps({"text": text, "takes": takes, "judge": verdict, "best": best}, indent=1))
        final.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(takes_dir / f"{c['id']}.{key}.t{best}.mp3", final)
        receipt = takes_dir / f"{c['id']}.{key}.t{best}.mp3.receipt.json"
        if receipt.exists():
            rdir = ROOT / "vo" / "receipts" / args.video
            rdir.mkdir(parents=True, exist_ok=True)
            shutil.copy(receipt, rdir / f"{c['id']}.{key}.receipt.json")
        print(f"  {c['id']}: kept take {best}  {json.dumps(verdict)[:600] if verdict else '(whisper fallback)'}")


if __name__ == "__main__":
    main()
