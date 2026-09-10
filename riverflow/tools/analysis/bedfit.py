"""Does the bed actually produce the octave shape it was given?

    python3 bedfit.py <dir-of-bed-only-renders>

The six shapes the engine ships are cluster centroids measured from the
library, and the bed is a bank of octave-wide bandpasses driven at gains solved
so that the *sum* matches them. This checks that it does, which is not a
foregone conclusion: setting each band to its target instead leaves the two
edge bands 3 to 9 dB low, because they have a neighbour on one side only.

Renders must be bed-only -- every event layer off -- or this measures the
events as well. See the recipe in README.md.
"""
import sys, os, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import refs

OCT = refs.OCT[refs.OCT >= 125.0]

SHAPES = {
    "DeepRush":      [-6.2, -3.2, -1.3, -1.5, -5.6, -10.5, -14.8, -24.5],
    "Rapids":        [-8.5, -4.7, -1.5, -0.7, -2.6, -5.5, -8.4, -14.2],
    "MountainRiver": [-22.2, -9.2, -2.6, -0.8, -1.7, -6.5, -11.8, -19.0],
    "Stream":        [-15.2, -12.1, -6.5, -1.5, -1.2, -2.2, -4.6, -10.3],
    "Creek":         [-31.2, -21.0, -7.8, -2.8, -2.7, -1.1, -2.7, -17.6],
    "Trickle":       [-14.4, -15.8, -12.3, -9.8, -7.7, -6.2, -5.9, -0.5],
}


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "."
    print("%-16s %s" % ("", " ".join("%7.0f" % f for f in OCT)))
    worst_all = 0.0
    for name, target in SHAPES.items():
        matches = glob.glob(os.path.join(d, "*%s*.wav" % name))
        if not matches:
            continue
        x, sr = wavio.read_wav(matches[0])
        m = wavio.to_mono(x)
        got = refs.band_levels(m, sr, OCT)
        t = np.array(target, float)
        # Both normalised to their own loudest band: only the colour is being
        # compared, never the level.
        got = got - got.max()
        t = t - t.max()
        err = got - t
        worst = float(np.max(np.abs(err)))
        worst_all = max(worst_all, worst)
        print("%-16s %s" % (name + " want", " ".join("%7.1f" % v for v in t)))
        print("%-16s %s" % ("got", " ".join("%7.1f" % v for v in got)))
        print("%-16s %s   worst %.1f dB" % ("err", " ".join("%7.1f" % v for v in err), worst))
        print()
    print("worst band error across every shape: %.1f dB" % worst_all)
    return 0


if __name__ == "__main__":
    sys.exit(main())
