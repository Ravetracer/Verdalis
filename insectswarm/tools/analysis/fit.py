"""Renders the plugin's own output and measures it back against the species table.

    python3 fit.py [--plugin path/to/InsectSwarm.clap] [--render path/to/insectswarm-render]
    python3 fit.py --species      # every Species, one individual, against its measured row
    python3 fit.py --swarm        # what Count does to the harmonic-to-noise ratio
    python3 fit.py --strid        # the stridulation layer against the measured carriers
    python3 fit.py --presets      # every factory preset, for level and sanity

This is the loop that closes the argument. `wingbeat.py` measures what a
honeybee is, `species.py` fits that to a shaper and ships it, and this renders a
honeybee back out of the engine and measures it with the same estimator -- so a
claim that the table was applied can be checked rather than believed.

What it cannot check is whether the result sounds like an insect. Render the
presets and listen; these numbers only catch what the ear cannot quantify.
"""
import os, subprocess, sys, tempfile, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import wingbeat as W
import swarm as S
import pulse as P

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "../.."))
PLUGIN = os.path.join(ROOT, "build/InsectSwarm.clap")
RENDER = os.path.join(ROOT, "build/insectswarm-render")

SPECIES = ["Hornet", "Bumblebee", "Wasp", "Housefly", "Honeybee", "Mosquito", "Dragonfly"]
# The shipped table, as printed by species.py --write. Kept here so a drift
# between the header and what the engine actually produces is visible in one
# place rather than needing both files open.
def shipped_rows():
    """Reads the generated header rather than repeating it, so this cannot drift."""
    import re
    src = open(os.path.join(ROOT, "src/dsp/species_generated.h")).read()
    body = src[src.index("kSpecies[] = {"):src.index("constexpr int kNumSpecies")]
    rows = {}
    for m in re.finditer(r'\{"(\w+)",([^{]*)\{([^}]*)\}\}', body):
        name = m.group(1)
        head = [float(x) for x in re.findall(r'(-?\d+\.?\d*)f', m.group(2))]
        stack = [float(x) for x in re.findall(r'(-?\d+\.?\d*)f', m.group(3))]
        rows[name] = dict(rate=head[0], hnr=head[6], stack=np.array(stack))
    return rows


ROWS = shipped_rows()


def render(out, params, seconds=6.0, preset=None, plugin=PLUGIN, tool=RENDER):
    cmd = [tool, "--plugin", plugin, "--out", out, "--seconds", str(seconds),
           "--tail", "1", "--rate", "48000"]
    if preset:
        cmd += ["--preset", preset]
    for k, v in params.items():
        cmd += ["--param", "%s=%s" % (k, v)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out):
        raise RuntimeError("render failed: %s\n%s" % (" ".join(cmd), r.stderr or r.stdout))
    return out


def one_species(tmp, name, **extra):
    """One individual, no flyby, no chorus, no bed: just the wing."""
    # velocitytoswarm is zeroed because the renderer plays at velocity 0.9 and
    # the default mapping would put the wingbeat 13 cents under the row. That is
    # the parameter working, not the table drifting, but it is not what this is
    # measuring.
    p = {"species": name, "count": 1, "spread": 0, "wander": 0, "flybylevel": -60,
         "stridulatelevel": -60, "bedlevel": -60, "distance": 0, "space": 0,
         "width": 0, "flutter": 0, "randomseed": 7, "outputgain": 0, "swarmlevel": -6,
         "velocitytoswarm": 0}
    p.update(extra)
    out = os.path.join(tmp, "sp_%s.wav" % name.lower())
    return render(out, p)


def check_species(tmp):
    print("%-11s %8s %8s %8s   %8s %8s %8s %7s" %
          ("species", "want Hz", "got Hz", "cents", "want HNR", "got HNR", "d dB", "stack"))
    bad = 0
    for name in SPECIES:
        f = one_species(tmp, name)
        r = W.measure(f, min_clarity=0.30)
        s = S.measure(f)
        want = ROWS[name]
        if r is None:
            print("%-11s %8.1f   (unvoiced)" % (name, want["rate"]))
            # Dragonfly measures 8% voiced in the library; unvoiced here is the
            # row being reproduced, not a failure.
            if name != "Dragonfly":
                bad += 1
            continue
        cents = 1200.0 * math.log2(r["f0"] / want["rate"])
        dh = (s["hnr"] - want["hnr"]) if s else float("nan")
        # How close the rendered harmonic stack is to the one the row carries,
        # with the level divided out -- the same comparison species.py fitted.
        n = len(want["stack"])
        got = np.array(r["parts"][:n])
        e = (got - got.mean()) - (want["stack"] - want["stack"].mean())
        rms = float(np.sqrt(np.mean(e * e)))
        flag = "" if abs(cents) < 60.0 else "   <-- octave"
        print("%-11s %8.1f %8.1f %8.0f   %8.2f %8.2f %8.2f %7.1f%s" %
              (name, want["rate"], r["f0"], cents, want["hnr"],
               s["hnr"] if s else float("nan"), dh, rms, flag))
        if abs(cents) >= 60.0:
            bad += 1
    print("\n%d of %d species off by more than a semitone" % (bad, len(SPECIES)))
    return bad


def check_swarm(tmp):
    """Count and Spread together are supposed to turn one tone into a crowd.

    The library's claim is that nothing else is needed: a single close insect
    measures 8-13 dB harmonic-to-noise and a hive 0-2, and many fundamentals
    scattered over a wide enough range *are* noise. This is where that is
    checked, and it is also where Spread's default came from -- the value in the
    parameter table is the one that puts a dozen individuals inside the hive
    figure.
    """
    print("\n  HNR by count and spread. The library: one insect 8-13 dB, a hive 0-2.")
    spreads = (0, 70, 150, 250)
    print("  %-7s %s" % ("count", "  ".join("%8s" % ("%d ct" % s) for s in spreads)))
    for n in (1, 2, 4, 8, 16, 32, 64):
        row = []
        for sp in spreads:
            f = one_species(tmp, "Honeybee", count=n, spread=sp, wander=18)
            s = S.measure(f)
            row.append("%8.2f" % (s["hnr"] if s else float("nan")))
        print("  %-7d %s" % (n, "  ".join(row)))


def check_strid(tmp):
    # Solo, with the scatter off, is the exactness check: the engine should
    # reproduce the settings. The chorus is the realism check: whether eight of
    # them still read as that carrier and that rhythm, which is how every
    # reference in the library was recorded.
    print("\n%-16s %9s %9s   %9s %9s   %9s %9s   %9s %9s" %
          ("", "want carr", "got carr", "want Q", "got Q", "want pulse", "got pulse",
           "want ech", "got ech"))
    # Rendered as the references were recorded -- a chorus, not one caller.
    # Measured on a single clean click train, `pulse.py` reports the width of one
    # spectral line rather than the resonator's, so a cicada at Q 13 comes back
    # as Q 71 and its pulse rate cannot be found at all. That is the estimator
    # meeting a signal no recording ever is, not the engine.
    # Percent parameters are given to --param in percent, not as a fraction:
    # "duty=0.48" asks the plugin for 0.48 %, which is where the layer's first
    # measurements went. Duty is written as 48 below for that reason.
    for name, carr, q, pulse, ech, duty in (("Cicada", 5549.0, 13.2, 268.4, 12.54, 48.0),
                                            ("Cricket", 4518.0, 25.8, 35.9, 10.51, 33.0)):
        p = {"stridulatelevel": -6, "swarmlevel": -60,
             "flybylevel": -60, "bedlevel": -60, "chorus": 8, "distance": 0, "space": 0,
             "carrier": carr, "carrierq": q, "pulserate": pulse, "echemerate": ech,
             "duty": duty, "randomseed": 7, "outputgain": -6}
        for label, extra in (("solo", {"chorus": 1, "scatter": 0}), ("chorus", {})):
            q2 = dict(p)
            q2.update(extra)
            f = render(os.path.join(tmp, "st_%s_%s.wav" % (name.lower(), label)), q2,
                       seconds=12.0)
            r = P.measure(f)
            print("%-16s %9.0f %9.0f   %9.1f %9.1f   %9.1f %9s   %9.2f %9s" %
                  (name + " " + label, carr, r["fc"], q, r["q"], pulse,
                   "%.1f" % r["pulse"] if r["pulse"] else "-", ech,
                   "%.2f" % r["echeme"] if r["echeme"] else "-"))


def check_presets(tmp):
    r = subprocess.run([RENDER, "--plugin", PLUGIN, "--all", "--outdir", tmp,
                        "--seconds", "6", "--tail", "2", "--rate", "48000",
                        "--param", "randomseed=7"], capture_output=True, text=True)
    print(r.stdout.strip() or r.stderr.strip())


def main():
    args = sys.argv[1:]
    global PLUGIN, RENDER
    if "--plugin" in args:
        PLUGIN = args[args.index("--plugin") + 1]
    if "--render" in args:
        RENDER = args[args.index("--render") + 1]
    want = [a for a in args if a in ("--species", "--swarm", "--strid", "--presets")]
    if not want:
        want = ["--species", "--swarm", "--strid"]
    with tempfile.TemporaryDirectory() as tmp:
        if "--species" in want:
            check_species(tmp)
        if "--swarm" in want:
            check_swarm(tmp)
        if "--strid" in want:
            check_strid(tmp)
        if "--presets" in want:
            check_presets(tmp)


if __name__ == "__main__":
    main()
