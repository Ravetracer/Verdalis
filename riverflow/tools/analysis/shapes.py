"""Clusters the library's octave-band shapes into water classes.

    python3 shapes.py [dir] [--k 6] [--emit]

The bed of a river is broadband, but it is not white and it is not one shape.
This clusters every reference by its octave-band shape -- normalised, so that
loudness plays no part and only the colour does -- and prints the centroid of
each cluster together with the recordings in it.

With --emit it prints the centroids as a C table, which is what the engine
ships: the bed is rendered by driving one noise source through a filterbank at
these measured gains, rather than by a filter someone tuned by ear. A table of
numbers is not a sample; see the suite's note on pure synthesis.
"""
import sys, os, glob, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import refs

# The engine renders from 125 Hz up. Below that the references carry wind and
# handling noise on the microphone rather than water: lowend.py measures the
# 20-60 Hz envelope as uncorrelated with the 1-4 kHz envelope (|r| < 0.15) in
# every recording, including the four where it is a fifth of the total energy.
# Clustering on those bands would fit the wind, not the river.
OCT = refs.OCT[refs.OCT >= 125.0]


def shape_of(path, secs=20.0):
    x, sr = wavio.read_wav(path)
    n = int(min(secs * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = wavio.to_mono(x[st:st + n])
    b = refs.band_levels(m, sr, OCT)
    # Above Nyquist there is nothing to cluster on; mark it.
    valid = OCT < sr * 0.45
    return b, valid


def kmeans(X, k, iters=200, seed=3):
    rng = np.random.default_rng(seed)
    C = X[rng.choice(len(X), k, replace=False)].copy()
    lab = np.zeros(len(X), int)
    for _ in range(iters):
        d = ((X[:, None, :] - C[None, :, :]) ** 2).sum(axis=2)
        new = np.argmin(d, axis=1)
        if (new == lab).all():
            break
        lab = new
        for j in range(k):
            if (lab == j).any():
                C[j] = X[lab == j].mean(axis=0)
    return C, lab


def main():
    argv = sys.argv[1:]
    k = 6
    emit = False
    args = []
    i = 0
    while i < len(argv):
        if argv[i] == "--k":
            k = int(argv[i + 1]); i += 2
        elif argv[i] == "--emit":
            emit = True; i += 1
        else:
            args.append(argv[i]); i += 1
    d = args[0] if args else os.path.join(os.path.dirname(__file__), "../../!dev/references")
    files = sorted(glob.glob(os.path.join(d, "*.wav")))
    rows, names = [], []
    for p in files:
        try:
            b, valid = shape_of(p)
        except Exception:
            continue
        if not valid.all():
            # 44.1 kHz files have no 16 k octave; fill it by extrapolating the
            # last two bands rather than dropping the file.
            b = b.copy()
            b[~valid] = b[valid][-1] + (b[valid][-1] - b[valid][-2])
        rows.append(b)
        names.append(os.path.basename(p))
    X = np.array(rows)
    # Normalise each shape to its own peak: colour only, no loudness.
    X = X - X.max(axis=1, keepdims=True)
    C, lab = kmeans(X, k)
    order = np.argsort([C[j].argmax() * 100 - C[j][-1] for j in range(k)])

    print("%-14s %s" % ("", " ".join("%6.0f" % f for f in OCT)))
    for rank, j in enumerate(order):
        members = [names[i] for i in range(len(names)) if lab[i] == j]
        print()
        print("class %d  (%d recordings)  %s" % (rank, len(members),
              " ".join("%6.1f" % v for v in C[j])))
        for mname in members:
            print("     %s" % mname)
    if emit:
        print()
        print("// Octave-band shapes measured across the reference library, dB relative")
        print("// to each shape's own loudest band. Centres: %s Hz." %
              ", ".join("%g" % f for f in OCT))
        print("constexpr float kBedShapes[%d][kNumBedBands] = {" % k)
        for rank, j in enumerate(order):
            print("   {%s}," % ", ".join("%6.1ff" % v for v in C[j]))
        print("};")
    return 0


if __name__ == "__main__":
    sys.exit(main())
