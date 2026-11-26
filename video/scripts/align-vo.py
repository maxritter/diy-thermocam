# /// script
# requires-python = ">=3.11"
# dependencies = ["faster-whisper==1.2.1", "numpy"]
# ///
"""Word timestamps for narration chunks (local ASR, timing only).

    uv run scripts/align-vo.py <jobs.json> <words-dir>
"""

import difflib
import json
import re
import subprocess
import sys
from pathlib import Path

import numpy as np
from faster_whisper import WhisperModel


def pcm16k(path: str) -> np.ndarray:
    raw = subprocess.check_output(
        ["ffmpeg", "-v", "error", "-i", path, "-f", "s16le", "-ac", "1", "-ar", "16000", "-"]
    )
    return np.frombuffer(raw, np.int16).astype(np.float32) / 32768.0


def norm(s: str) -> str:
    return re.sub(r"[^a-z0-9']", "", s.lower())


def onto_script(script_words: list[str], heard: list[tuple[str, float, float]], duration: float) -> list[dict]:
    a = [norm(w) for w in script_words]
    b = [norm(w) for w, _, _ in heard]
    times: list[tuple[float, float] | None] = [None] * len(a)
    for block in difflib.SequenceMatcher(a=a, b=b, autojunk=False).get_matching_blocks():
        for k in range(block.size):
            _, s, e = heard[block.b + k]
            times[block.a + k] = (s, e)
    i = 0
    while i < len(a):
        if times[i] is not None:
            i += 1
            continue
        j = i
        while j < len(a) and times[j] is None:
            j += 1
        lo = times[i - 1][1] if i > 0 else 0.0
        hi = times[j][0] if j < len(a) else duration
        step = (hi - lo) / (j - i)
        for k in range(i, j):
            times[k] = (lo + (k - i) * step, lo + (k - i + 1) * step)
        i = j
    return [{"w": w, "s": round(t[0], 2), "e": round(t[1], 2)} for w, t in zip(script_words, times)]


jobs = json.loads(Path(sys.argv[1]).read_text())
out_dir = Path(sys.argv[2])
model = WhisperModel("small.en", device="cpu", compute_type="int8")
for job in jobs:
    duration = float(
        subprocess.check_output(
            ["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", job["audio"]]
        ).strip()
    )
    segments, _ = model.transcribe(
        pcm16k(job["audio"]),
        word_timestamps=True,
        condition_on_previous_text=False,
        hotwords="DIY-Thermocam Teensy Lepton Lanczos FFmpeg GPL Studio Firmware",
    )
    heard = [(w.word.strip(), w.start, w.end) for seg in segments for w in seg.words]
    script_words = job["text"].split()
    words = onto_script(script_words, heard, duration)
    (out_dir / f"{job['key']}.json").write_text(json.dumps({"duration": duration, "words": words}) + "\n")
    print(job["key"], round(duration, 2), " ".join(w["w"] for w in words), flush=True)
