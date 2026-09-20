"""NightLife's own output, measured back against the reference library.

    python3 fit.py                  # everything
    python3 fit.py --callers        # the caller layer: pitch, length, harmonics
    python3 fit.py --chorus         # the frog chorus: pulse rate and Fano factor
    python3 fit.py --beds           # the bed's spectrum and the insect band

Every other script here measures recordings. This one measures the plugin, with
the same estimators, and prints the two side by side. It is the only check that
the engine does what the library says -- and ChirpParade is the reason it
exists: that plugin was validated against twenty measured quantities, agreed
with all of them, and did not sound like a bird, because the statistics had been
taken through an analysis window that could not see what a syllable does. So
this file answers one question only -- *does the engine reproduce what was
measured* -- and the ear answers the other one.

It needs the built renderer:

    cmake --build build && cd build && python3 ../tools/analysis/fit.py
"""
import os
import subprocess
import sys
import tempfile

import numpy as np

import calls as S
import bed as B
from callers import GROUPS
from contours import CALLER_ORDER

REFS = os.environ.get("NIGHTLIFE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
RENDER = os.environ.get("NIGHTLIFE_RENDER", "./nightlife-render")
PLUGIN = os.environ.get("NIGHTLIFE_PLUGIN", "./NightLife.clap")

# What the library measures, per caller. From callers.py (the census) and
# contours.py (the archetype durations, which is what the engine plays).
REF_CALLER = {
    # caller:    (f0 Hz, archetype length s, harmonics, roughness dB)
    "Wolf": (484.0, 1.290, 2.0, -33.0),
    "Owl": (497.0, 0.181, 1.0, -34.0),
    "Screech": (1256.0, 0.456, 1.0, -33.0),
    "Scops": (1272.0, 0.907, 1.0, -32.0),
    "Fox": (1053.0, 0.949, 3.0, -23.0),
    "Loon": (1132.0, 0.268, 1.0, -34.0),
}

# From frogs.py.
REF_CHORUS = {"pulseHz": 27.0, "croakSec": 0.325, "f1": 2063.0,
              "fano": [0.90, 0.81, 0.59, 0.30]}


def render(path, seconds, extra, seed=7, tail=2.0):
    args = [RENDER, "--plugin", PLUGIN, "--out", path, "--seconds", "%g" % seconds,
            "--tail", "%g" % tail, "--param", "randomseed=%d" % seed]
    for kv in extra:
        args += ["--param", kv]
    r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        print(r.stdout.decode(errors="replace"))
        raise SystemExit("render failed: %s" % " ".join(args))
    return path


SILENT = ["choruslevel=-60", "insectlevel=-60", "bedlevel=-60", "packlevel=-60",
          "spaceamount=0"]


def fit_callers(tmp):
    """One call at a time, which is the only way to measure a call's own length.

    Rendering the pack and measuring the result does not work and the numbers it
    gives are worth recording: with three animals answering each other, two
    overlapping hoots segment as one 320 ms call against a measured 181, and a
    howl longer than the analysis's own patience segments as its loud middle
    alone. So each archetype is fired on its own here, with one animal, one call
    per phrase and nothing else sounding.
    """
    print("the caller layer, one archetype at a time against what that caller measures:")
    print("%-9s%8s%8s%7s%9s%9s%7s%8s%8s" %
          ("caller", "f0 Hz", "ref", "err", "len ms", "ref", "err", "harm", "ref"))
    rows = []
    for ci, name in enumerate(CALLER_ORDER):
        f0s, lens, harms = [], [], []
        for k in range(8):
            # Contour is a percentage, so the value is 0..100 and not 0..1 --
            # the trap the suite's CLAUDE.md records for randomseed.
            contour = 100.0 * (k + 0.5) / 8.0
            wav = os.path.join(tmp, "call_%d_%d.wav" % (ci, k))
            render(wav, 8.0, SILENT + ["caller=%d" % ci, "contour=%g" % contour,
                                       "shotlevel=0", "calls=1", "repeats=1", "animals=1",
                                       "variation=0", "jitter=0", "distance=0",
                                       "depth=0"])
            r = S.analyse(wav, 10.0)
            if not r or not r["syllables"]:
                continue
            # The loudest call in the render is the one that was fired; anything
            # else is a tail or an artefact.
            sy = max(r["syllables"], key=lambda s: s.level)
            f0s.append(sy.f0_med)
            lens.append(sy.dur)
            harms.append(sy.harmonics)
        if not f0s:
            print("%-9s  nothing rendered" % name)
            continue
        ref = REF_CALLER[name]
        f0 = float(np.median(f0s))
        ln = float(np.median(lens))
        hm = float(np.median(harms))
        rows.append((name, f0, ln))
        print("%-9s%8.0f%8.0f%6.0f%%%9.0f%9.0f%6.0f%%%8.1f%8.1f" %
              (name, f0, ref[0], 100.0 * (f0 - ref[0]) / ref[0], 1000.0 * ln,
               1000.0 * ref[1], 100.0 * (ln - ref[1]) / ref[1], hm, ref[2]))
    return rows


def fit_chorus(tmp):
    print("\nthe frog chorus, against what the thirteen frog recordings measure:")
    wav = os.path.join(tmp, "chorus.wav")
    render(wav, 60.0, ["packlevel=-60", "shotlevel=-60", "insectlevel=-60",
                       "bedlevel=-60", "choruslevel=0", "spaceamount=0", "distance=0.1",
                       "frogs=8"], tail=1.0)
    r = S.analyse(wav, 62.0)
    sy = r["syllables"] if r else []
    if not sy:
        print("   nothing rendered")
        return
    import frogs as F
    m, sr, _ = S.load_mono(wav, 62.0)
    rates, f1s = [], []
    for s in sy[:120]:
        a, b = int(s.t0 * sr), min(len(m), int(s.t1 * sr))
        if b - a < 0.04 * sr:
            continue
        rate, depth, count = F.pulse_train(m[a:b], sr)
        if rate > 0.0:
            rates.append(rate)
        f1, f2, q = F.spectrum_peaks(m[a:b], sr, rate)
        if f1 > 0.0:
            f1s.append(f1)
    print("   %-22s %8.0f Hz   ref %6.0f" %
          ("pulse rate", np.median(rates) if rates else 0.0, REF_CHORUS["pulseHz"]))
    print("   %-22s %8.0f ms   ref %6.0f" %
          ("croak length", 1000.0 * np.median([s.dur for s in sy]),
           1000.0 * REF_CHORUS["croakSec"]))
    print("   %-22s %8.0f Hz   ref %6.0f" %
          ("first resonance", np.median(f1s) if f1s else 0.0, REF_CHORUS["f1"]))
    fano = F.fano([s.t0 for s in sy], r["seconds"])
    print("   Fano factor, which is the measurement a Poisson spawner fails:")
    print("      %-19s %s" % ("window", "  ".join("%7.2fs" % w for w in (0.05, 0.25, 1.0, 4.0))))
    print("      %-19s %s" % ("rendered", "  ".join("%8.2f" % f for f in fano)))
    print("      %-19s %s" % ("reference", "  ".join("%8.2f" % f for f in REF_CHORUS["fano"])))
    print("      %-19s %s" % ("a Poisson process", "  ".join("%8.2f" % 1.0 for _ in fano)))


def fit_beds(tmp):
    print("\nthe bed's own third-octave curve against the measured night, dB:")
    wav = os.path.join(tmp, "bed.wav")
    render(wav, 30.0, ["packlevel=-60", "shotlevel=-60", "insectlevel=-60",
                       "choruslevel=-60", "bedlevel=0", "spaceamount=0", "bedmotion=0"],
           tail=0.5)
    p, fr, sr = B.spectrum(wav, 30.0, quiet=1.0)
    got = B.third_octave(p, fr, sr)
    got = got - np.nanmax(got)

    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    curves = []
    for name in files:
        rp, rfr, rsr = B.spectrum(os.path.join(REFS, name))
        if rp is not None:
            curves.append(B.third_octave(rp, rfr, rsr))
    ref = np.nanmedian(np.array(curves), axis=0)
    ref = ref - np.nanmax(ref)
    err = []
    print("   %8s %8s %8s %7s" % ("Hz", "rendered", "measured", "err"))
    for i, c in enumerate(B.THIRD):
        if not np.isfinite(got[i]) or not np.isfinite(ref[i]):
            continue
        # Below 63 Hz and above 10 kHz the bank has no band of its own, so the
        # comparison there is of two skirts and means little.
        if c < 50.0 or c > 10000.0:
            continue
        err.append(got[i] - ref[i])
        print("   %8.0f %8.1f %8.1f %7.1f" % (c, got[i], ref[i], got[i] - ref[i]))
    if err:
        print("   rms error over the bank's own range: %.1f dB" %
              float(np.sqrt(np.mean(np.array(err) ** 2))))

    print("\nthe insect band:")
    wav = os.path.join(tmp, "insects.wav")
    render(wav, 20.0, ["packlevel=-60", "shotlevel=-60", "choruslevel=-60",
                       "bedlevel=-60", "insectlevel=0", "spaceamount=0"], tail=0.5)
    got = B.insect_band(wav, 20.0)
    if got:
        print("   %-14s %8.0f Hz   ref %6.0f" % ("carrier", got["peak"], 2982.0))
        print("   %-14s %8.1f      ref %6.1f" % ("Q", got["q"], 20.7))
        print("   %-14s %8.1f Hz   ref %6.1f" % ("trill rate", got["trill"], 49.2))
        print("   %-14s %8.2f      ref %6.2f" % ("steadiness", got["steady"], 0.54))


def main():
    want = [a for a in sys.argv[1:] if a.startswith("--")]
    if not os.path.exists(RENDER):
        print("no renderer at %s -- run this from the build directory" % RENDER)
        return 1
    with tempfile.TemporaryDirectory(prefix="nightlife-fit-") as tmp:
        if not want or "--callers" in want:
            fit_callers(tmp)
        if not want or "--chorus" in want:
            fit_chorus(tmp)
        if not want or "--beds" in want:
            fit_beds(tmp)
    return 0


if __name__ == "__main__":
    sys.exit(main())
