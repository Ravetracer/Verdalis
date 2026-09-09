"""Renders the plugin's own output and measures it against the references.

    python3 fit.py [--plugin path/to/ChirpParade.clap] [--render path/to/chirpparade-render]
    python3 fit.py --species      # every Species against its measured group
    python3 fit.py --presets      # every factory preset against its own target
    python3 fit.py --voice        # the Voice -> harmonics calibration
    python3 fit.py --breath       # the Breath -> roughness calibration

This is the loop that closes the argument. `species.py` measures what a crow
is; the engine's Species table is that measurement; and this renders a crow out
of the engine and measures it back with the same estimator, so a claim that the
table was applied can be checked rather than believed.

What it cannot check is whether the result sounds like a bird. That was the
whole failure of the first version: 61 parameters agreeing with 4268 syllables'
worth of statistics, and it sounded like nothing. Render the presets and listen;
these numbers only catch what the ear cannot quantify.

Sweep is now a percentage of the measured contour rather than a width in
octaves, so it is not compared against the library here -- the contour carries
the excursion and contours.py is what checks that.
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

# The species table no longer carries a sweep or a contour shape -- those are in
# contours_generated.h as measured curves. What is left of it is these five.
# Length is the median duration of that species' *archetypes*, which is what
# the engine's table carries -- not the median of all its measured syllables.
# The two differ where only some of a species' syllables passed the contour
# quality gate. contours.py prints these.
SPECIES = {
    "Whistler": (3728, 94, 1, -34),
    "Sparrow": (3491, 71, 1, -32),
    "Warbler": (2812, 78, 1, -30),
    "Budgie": (1353, 66, 3, -27),
    "Woodpecker": (2550, 129, 2, -30),
    "Crane": (821, 137, 5, -25),
    "Goose": (528, 178, 6, -25),
    # Screech has no reference of its own; these are the engine's own settings,
    # so the row checks that it does what it is configured to do and nothing
    # more. It is an effect, not a bird.
    "Screech": (1800, 208, 6, -16),
    "Piper": (2559, 59, 2, -33),
}


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


# Each species has ARCHETYPES contours and Contour walks across them, so one
# render at the default Contour measures one arbitrary archetype rather than the
# species. That was harmless while every syllable was stretched to the species
# median; since 0.5.0 each archetype plays at its own measured duration, so the
# length column only means something as a median over the whole set. Every
# column is taken that way now, which is also what the targets in SPECIES are.
ARCHETYPES = 8


def do_species(tmp):
    print("Every Species, rendered as one isolated syllable per archetype and")
    print("measured back. Sweep and contour shape are not here: they are measured")
    print("curves now, and contours.py is what checks them.")
    print()
    print("%-11s %15s %6s %14s %6s %10s %12s" %
          ("species", "pitch Hz", "", "length ms", "", "harmonics", "roughness dB"))
    for name, (f0, ln, nh, ro) in SPECIES.items():
        sy = []
        for i in range(ARCHETYPES):
            out = os.path.join(tmp, "sp_%s_%d.wav" % (name, i))
            # --param takes the *displayed* value, and Contour is a percentage.
            # Passing the raw 0..1 fraction here selected archetype 0 eight
            # times over and made the whole sweep meaningless.
            where = 100.0 * (i + 0.5) / float(ARCHETYPES)
            if not render(out, extra=ISOLATE + ["--param", "species=%s" % name,
                                                "--param", "contour=%.3f" % where]):
                continue
            sy.extend(measure(out) or [])
        if not sy:
            print("%-11s nothing voiced in the render" % name)
            continue
        g = (med(sy, lambda s: s.f0_med), 1000.0 * med(sy, lambda s: s.dur),
             med(sy, lambda s: s.harmonics),
             10.0 * np.log10(med(sy, lambda s: s.flatness)))
        print("%-11s %7.0f /%6.0f %6s %6.0f /%5.0f %6s %4.0f /%3d %6.0f /%4d" %
              (name, g[0], f0, rel(g[0], f0), g[1], ln, rel(g[1], ln), g[2], nh, g[3], ro))


def do_voice(tmp):
    print("Voice against harmonics: how much of each cycle the valve is shut.")
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


# Each species' Breath multiplier, from its measured roughness. Printed beside
# the sweep because the multiplier is what the first version of this calibration
# forgot: it measured the *parameter* against roughness on a Whistler, whose
# multiplier is 0.42, and the resulting default was four times too high.
BREATH_MUL = {"Sparrow": 0.863, "Whistler": 0.415}


def do_breath(tmp):
    print("Breath against roughness, which is what calibrates the species table.")
    print("Measured on a Sparrow, whose species multiplier is 0.863 -- the")
    print("effective column is what the engine actually applies.")
    print()
    print("%8s %12s %14s %12s" % ("Breath", "effective", "roughness dB", "tonality dB"))
    for b in [0, 2, 5, 10, 18, 30, 50, 100]:
        out = os.path.join(tmp, "b%d.wav" % b)
        if not render(out, extra=ISOLATE + ["--param", "breath=%d" % b,
                                            "--param", "species=Sparrow"]):
            continue
        sy = measure(out)
        if not sy:
            continue
        print("%7d %% %11.1f %% %14.1f %12.1f" %
              (b, b * BREATH_MUL["Sparrow"],
               10.0 * np.log10(med(sy, lambda s: s.flatness)), med(sy, lambda s: s.hnr)))


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
                # 6 dB, not 4, and the reason is a limitation of the statistic
                # rather than slack. Spectral flatness cannot tell noise from
                # frequency modulation, and these contours sweep at hundreds of
                # octaves a second -- a pure sine doing that smears across a
                # 21 ms analysis window and reads as rough. So the figure
                # carries the contour's own motion as well as the breath, and
                # tightening it would mean fitting Breath to an artefact.
                if abs(got[k] - w) > 6.0:
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

    want = [a for a in args if a in ("--species", "--presets", "--voice", "--breath")]
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
            elif a == "--presets":
                do_presets(tmp)
    return 0


if __name__ == "__main__":
    sys.exit(main())
