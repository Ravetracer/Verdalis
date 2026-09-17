"""Builds the analysis cache: every reference as 48 kHz mono 16-bit WAV.

    python3 cache.py [--force]

The library is 938 MB at 22 to 192 kHz, and every script here re-reads all of
it. Decoding once into `!dev/work/cache48` costs a minute and makes the rest of
the analysis interactive. 48 kHz keeps everything up to 24 kHz, which is above
the highest cicada energy in the library, so nothing measured later is lost to
it.
"""
import sys, os, glob, subprocess
from refs import DEFAULT_DIR

CACHE = os.path.join(os.path.dirname(__file__), "../../!dev/work/cache48")


def build(src=DEFAULT_DIR, force=False):
    os.makedirs(CACHE, exist_ok=True)
    for p in sorted(glob.glob(os.path.join(src, "*.wav"))):
        out = os.path.join(CACHE, os.path.basename(p))
        if os.path.exists(out) and not force and os.path.getmtime(out) >= os.path.getmtime(p):
            continue
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", p,
                        "-ac", "1", "-ar", "48000", "-c:a", "pcm_s16le", out], check=True)
    return CACHE


def cached_dir():
    return CACHE if glob.glob(os.path.join(CACHE, "*.wav")) else DEFAULT_DIR


if __name__ == "__main__":
    build(force="--force" in sys.argv)
    print("%d files in %s" % (len(glob.glob(os.path.join(CACHE, "*.wav"))), CACHE))
