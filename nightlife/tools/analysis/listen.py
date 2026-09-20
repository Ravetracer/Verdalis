"""Writes A/B pairs: a factory preset beside the recording its numbers came from.

    cd build && python3 ../tools/analysis/listen.py

The suite's rule is validate by ear *then* by measurement, and a plugin whose
numbers all agree has still not been checked. This puts the two files side by
side at the same sample rate and the same loudness so that the only difference
left is the one that matters.

Output goes to !dev/listen-v1/, which is gitignored: ref_<name>.wav is an
excerpt of the reference, new_<name>.wav is the preset.
"""
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import wavio

HERE = os.path.dirname(os.path.abspath(__file__))
REFS = os.path.join(HERE, "../../!dev/references")
OUT = os.path.join(HERE, "../../!dev/listen-v1")
RENDER = os.environ.get("NIGHTLIFE_RENDER", "./nightlife-render")
SECONDS = 16.0

# preset, reference, where in the reference to start
PAIRS = [
    ("wolf_valley", "howling-wolf.wav", 0.0),
    ("pack_answering", "wolves-howling-1.wav", 0.0),
    ("tawny_wood", "tawny-owl-2.wav", 0.0),
    ("barred_phrase", "barred-owl-hooting.wav", 0.0),
    ("scops_metronome", "scops-owl.wav", 10.0),
    ("screech_whinny", "screech_owl.wav", 0.0),
    ("vixen_scream", "fox-scream.wav", 0.0),
    ("fox_across_fields", "fox-calling.wav", 0.0),
    ("loon_lake", "loon-call.wav", 0.0),
    ("pond_chorus", "frogs-6.wav", 10.0),
    ("bullfrog_bank", "croaking-frogs.wav", 0.0),
    ("spring_peepers", "leopard-frogs-croaking.wav", 0.0),
    ("summer_night", "frogs-summer-night-ambient.wav", 5.0),
    ("midnight_marsh", "frogs-croaking-in-pond.wav", 20.0),
    ("cricket_field", "scops-owl.wav", 60.0),
]


def resample(x, src, dst):
    if src == dst:
        return x
    n = int(round(len(x) * dst / float(src)))
    t = np.linspace(0.0, len(x) - 1.0, n)
    i = np.clip(t.astype(int), 0, len(x) - 2)
    f = t - i
    return x[i] * (1.0 - f) + x[i + 1] * f


def normalise(x, target_rms=0.06):
    rms = float(np.sqrt(np.mean(x ** 2)))
    if rms <= 0.0:
        return x
    y = x * (target_rms / rms)
    peak = float(np.max(np.abs(y)))
    return y / peak * 0.95 if peak > 0.95 else y


def write(path, x, sr):
    import struct
    x = np.clip(np.asarray(x, float), -1.0, 1.0)
    d = (x * 32767.0).astype("<i2").tobytes()
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(d)) + b"WAVEfmt ")
        f.write(struct.pack("<IHHIIHH", 16, 1, 1, sr, sr * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(d)) + d)


def main():
    os.makedirs(OUT, exist_ok=True)
    for preset, ref, start in PAIRS:
        path = os.path.join(REFS, ref)
        if not os.path.exists(path):
            print("   missing reference %s" % ref)
            continue
        x, sr = wavio.read_wav(path)
        m = wavio.to_mono(x)
        a = int(start * sr)
        m = m[a:a + int(SECONDS * sr)]
        write(os.path.join(OUT, "ref_%s.wav" % preset), normalise(resample(m, sr, 48000)), 48000)

        out = os.path.join(OUT, "new_%s.wav" % preset)
        r = subprocess.run([RENDER, "--preset", preset, "--out", out,
                            "--seconds", "%g" % SECONDS, "--tail", "2", "--rate", "48000",
                            "--param", "randomseed=3"],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if r.returncode != 0:
            print(r.stdout.decode(errors="replace"))
            continue
        y, ysr = wavio.read_wav(out)
        write(out, normalise(wavio.to_mono(y)), ysr)
        print("   %-20s vs %s" % (preset, ref))
    print("\nwrote %d pairs to %s" % (len(PAIRS), os.path.abspath(OUT)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
