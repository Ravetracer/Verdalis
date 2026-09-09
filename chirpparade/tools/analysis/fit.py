"""Renders the plugin's own output and measures it against the references.

    python3 fit.py [--plugin path/to/ChirpParade.clap] [--render path/to/chirpparade-render]
    python3 fit.py --species      # every Species against its measured group
    python3 fit.py --presets      # every factory preset against its own target
    python3 fit.py --voice        # the Voice -> harmonics calibration
    python3 fit.py --breath       # the Breath -> roughness calibration
    python3 fit.py --pull         # what is left of the relaxation pitch drop

This is the loop that closes the argument. `species.py` measures what a crow
is; the engine's Species table is that measurement; and this renders a crow out
of the engine and measures it back with the same estimator, so a claim that the
table was applied can be checked rather than believed.

Two systematic offsets showed up the first time it was run and were fixed in
the engine rather than absorbed here:

  * a syllable's measured pitch was 13 % above its Pitch setting, because the
    pressure gesture peaks at 0.37 of the syllable and a measurement reads the
    loud part rather than the middle. The engine now anchors the contour so that
    the pitch at the peak *is* Pitch, and the statistic compared here is the
    pitch at the loudest frame rather than the median over the syllable -- the
    two differ by 10 % for a swept syllable, which is not an error in either.

  * a syllable's measured sweep was up to three times its Sweep setting for the
    species whose contours turn more than once, because the gesture's raw
    excursion depends on Turns. The engine now scales the contour so that the
    excursion is Sweep octaves whatever Turns is.

What remains unfitted is listed in README.md under *What does not fit*.
"""
import os
import subprocess
import sys
import tempfile

import numpy as np

import syllables as S

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "../.."))
PLUGIN = os.path.join(ROOT, "build/ChirpParade.clap")
RENDER = os.path.join(ROOT, "build/chirpparade-render")
PRESETS = os.path.join(ROOT, "presets")

# What species.py measured for each group: pitch at the loudest frame in Hz,
# sweep octaves, length ms, harmonics, roughness dB. The engine's table carries
# the same numbers.
SPECIES = {
    "Whistler": (4748, 0.21, 128, 1, -34),
    "Sparrow": (3147, 0.34, 96, 1, -29),
    "Warbler": (1128, 0.37, 87, 2, -27),
    "Budgie": (1351, 0.47, 99, 3, -28),
    "Woodpecker": (3312, 0.31, 96, 2, -25),
    "Crane": (982, 0.36, 133, 4, -26),
    "Goose": (566, 0.51, 206, 4, -32),
    "Crow": (806, 0.45, 144, 5, -21),
    "Raven": (1171, 0.38, 267, 6, -30),
    "Screech": (1800, 1.60, 260, 6, -16),
}

# Isolating one syllable: no flock, no reverb, no distance, and none of the
# per-syllable variation, so that what is measured is the syllable the
# parameters ask for rather than a sample of a distribution. Pitch Spread has
# to be in that list as well, and forgetting it cost an afternoon: a flock's
# first bird carries its own pitch offset, so a "nominal" render came out a
# third of an octave off and looked like an oscillator running sharp.
ISOLATE = [
    "--param", "flocklevel=-60",
    "--param", "syllables=1",
    "--param", "distance=0",
    "--param", "spaceamount=0",
    "--param", "variation=0",
    "--param", "jitter=0",
    "--param", "voicespread=0",
    "--param", "pitchspread=0",
    "--param", "randomseed=7",
]


def render(out, preset="garden_sparrows", extra=(), seconds=2.0, rate=48000):
    cmd = [RENDER, "--plugin", PLUGIN, "--preset", preset, "--out", out,
           "--seconds", str(seconds), "--tail", "1", "--rate", str(rate)]
    cmd += list(extra)
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out):
        sys.stderr.write(r.stdout + r.stderr)
        return False
    return True


def med(sy, f):
    return float(np.median([f(s) for s in sy])) if sy else float("nan")


def measure(path, seconds=4.0):
    r = S.analyse(path, seconds)
    return r["syllables"] if r else []


def rel(got, want):
    if want == 0.0 or not np.isfinite(got):
        return "   -"
    return "%+4.0f%%" % (100.0 * (got - want) / abs(want))


def do_species(tmp):
    print("Every Species, rendered as one isolated syllable and measured back.")
    print()
    print("%-11s %15s %6s %14s %6s %14s %6s %10s %12s" %
          ("species", "pitch Hz", "", "sweep oct", "", "length ms", "", "harmonics",
           "roughness dB"))
    for name, (f0, sw, ln, nh, ro) in SPECIES.items():
        out = os.path.join(tmp, "sp_%s.wav" % name)
        if not render(out, extra=ISOLATE + ["--param", "species=%s" % name]):
            print("%-11s render failed" % name)
            continue
        sy = measure(out)
        if not sy:
            print("%-11s nothing voiced in the render" % name)
            continue
        g = (med(sy, lambda s: s.f0_med), med(sy, lambda s: s.sweep_oct),
             1000.0 * med(sy, lambda s: s.dur), med(sy, lambda s: s.harmonics),
             10.0 * np.log10(med(sy, lambda s: s.flatness)))
        print("%-11s %7.0f /%6.0f %6s %6.2f /%5.2f %6s %6.0f /%5.0f %6s %4.0f /%3d %6.0f /%4d" %
              (name, g[0], f0, rel(g[0], f0), g[1], sw, rel(g[1], sw), g[2], ln,
               rel(g[2], ln), g[3], nh, g[4], ro))


def do_voice(tmp):
    print("Voice against harmonics: the engine's mu = B/sqrt(eps), measured.")
    print()
    print("%8s %12s %12s" % ("Voice", "harmonics", "roughness dB"))
    for v in [0, 10, 20, 30, 40, 50, 60, 70, 85, 100]:
        out = os.path.join(tmp, "v%d.wav" % v)
        if not render(out, extra=ISOLATE + ["--param", "voice=%d" % v,
                                            "--param", "pitch=900",
                                            "--param", "breath=0"]):
            continue
        sy = measure(out)
        if not sy:
            print("%7d %%  (nothing voiced)" % v)
            continue
        print("%7d %% %12.1f %12.1f" %
              (v, med(sy, lambda s: s.harmonics),
               10.0 * np.log10(med(sy, lambda s: s.flatness))))


def do_pull(tmp):
    """The residual pitch error across Voice.

    A van der Pol oscillator driven towards relaxation genuinely goes flat --
    its period lengthens -- and the engine compensates for that so that `Pitch`
    keeps meaning the pitch that comes out. This is what is left over after the
    compensation, and it is the number to look at if the coefficient behind it
    is ever changed.
    """
    print("Pitch error across Voice, at a pitch low enough not to hit the")
    print("anti-alias clamp on the drive.")
    print()
    print("%8s %12s %12s %12s" % ("Voice", "asked Hz", "got Hz", "error"))
    base = 500.0
    for v in [0, 20, 40, 55, 70, 80, 90, 100]:
        out = os.path.join(tmp, "pull%d.wav" % v)
        if not render(out, extra=ISOLATE + ["--param", "voice=%d" % v,
                                            "--param", "pitch=%d" % int(base),
                                            "--param", "sweep=0",
                                            "--param", "breath=0",
                                            "--param", "formant=0",
                                            "--param", "species=Sparrow"],
                      seconds=2.0):
            continue
        sy = measure(out)
        if not sy:
            print("%7d %%  (nothing voiced)" % v)
            continue
        want = base * 3147.0 / 2580.0
        got = med(sy, lambda s: s.f0_med)
        print("%7d %% %12.0f %12.0f %11.1f %%" %
              (v, want, got, 100.0 * (got - want) / want))


def do_breath(tmp):
    print("Breath against roughness, which is what calibrates the species table.")
    print()
    print("%8s %14s %12s" % ("Breath", "roughness dB", "tonality dB"))
    for b in [0, 2, 5, 10, 18, 30, 50, 100]:
        out = os.path.join(tmp, "b%d.wav" % b)
        if not render(out, extra=ISOLATE + ["--param", "breath=%d" % b,
                                            "--param", "species=Whistler"]):
            continue
        sy = measure(out)
        if not sy:
            continue
        print("%7d %% %14.1f %12.1f" %
              (b, 10.0 * np.log10(med(sy, lambda s: s.flatness)), med(sy, lambda s: s.hnr)))


def preset_targets():
    """Each preset's own target, read out of its `fit` header line if it has one.

    A preset says what it was fitted against in its own file, so that this
    script has no second copy of the intent to drift from. The line is

        fit = pitch=3100 length=90 harmonics=1 rough=-29

    and anything absent is simply not checked.
    """
    out = {}
    for name in sorted(os.listdir(PRESETS)):
        if not name.endswith(".chirpparade"):
            continue
        want = {}
        with open(os.path.join(PRESETS, name)) as f:
            for line in f:
                line = line.strip()
                if not line.startswith("# fit"):
                    continue
                for token in line[5:].replace(":", " ").split():
                    if "=" not in token:
                        continue
                    k, v = token.split("=", 1)
                    try:
                        want[k] = float(v)
                    except ValueError:
                        pass
        out[name[:-len(".chirpparade")]] = want
    return out


def do_presets(tmp):
    targets = preset_targets()
    print("Every factory preset, rendered and measured. A preset states its own")
    print("target in a `# fit` line; the ones with no such line are only checked")
    print("for making a sound at all.")
    print()
    print("%-24s%6s%9s%9s%8s%8s%8s" %
          ("preset", "syls", "pitch", "length", "harm", "rough", "verdict"))
    for name, want in targets.items():
        out = os.path.join(tmp, "p_%s.wav" % name)
        if not render(out, preset=name, seconds=8.0,
                      extra=["--param", "randomseed=7"]):
            print("%-24s render failed" % name)
            continue
        sy = measure(out, 10.0)
        if not sy:
            print("%-24s%6d   nothing voiced" % (name, 0))
            continue
        got = {
            "pitch": med(sy, lambda s: s.f0_med),
            "length": 1000.0 * med(sy, lambda s: s.dur),
            "harmonics": med(sy, lambda s: s.harmonics),
            "rough": 10.0 * np.log10(med(sy, lambda s: s.flatness)),
        }
        bad = []
        for k, w in want.items():
            if k not in got:
                continue
            tol = 0.30 if k in ("pitch", "length") else 0.0
            if k == "rough":
                if abs(got[k] - w) > 4.0:
                    bad.append(k)
            elif k == "harmonics":
                if abs(got[k] - w) > 2.0:
                    bad.append(k)
            elif abs(got[k] - w) > tol * abs(w):
                bad.append(k)
        verdict = "ok" if not bad else "off: " + ",".join(bad)
        print("%-24s%6d%9.0f%9.0f%8.0f%8.1f  %s" %
              (name, len(sy), got["pitch"], got["length"], got["harmonics"],
               got["rough"], verdict))


def main():
    global PLUGIN, RENDER
    args = sys.argv[1:]
    for i, a in enumerate(args):
        if a == "--plugin" and i + 1 < len(args):
            PLUGIN = args[i + 1]
        if a == "--render" and i + 1 < len(args):
            RENDER = args[i + 1]
    if not os.path.exists(RENDER) or not os.path.exists(PLUGIN):
        print("build the plugin first: cmake --build build")
        print("  looked for %s" % RENDER)
        print("         and %s" % PLUGIN)
        return 1

    want = [a for a in args if a in ("--species", "--presets", "--voice", "--breath",
                                    "--pull")]
    if not want:
        want = ["--species", "--voice", "--breath", "--presets"]

    with tempfile.TemporaryDirectory(prefix="chirpparade-fit-") as tmp:
        for i, a in enumerate(want):
            if i:
                print()
                print("-" * 78)
                print()
            if a == "--species":
                do_species(tmp)
            elif a == "--voice":
                do_voice(tmp)
            elif a == "--breath":
                do_breath(tmp)
            elif a == "--pull":
                do_pull(tmp)
            elif a == "--presets":
                do_presets(tmp)
    return 0


if __name__ == "__main__":
    sys.exit(main())
