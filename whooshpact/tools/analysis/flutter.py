"""Measures the flutter in the reference library: how fast, how deep, and whether it moves.

    python3 flutter.py [dir]

A downshifter is not just a falling pitch, it is a falling pitch being chopped,
and the chopping speeds up or slows down across the gesture. That is what the
plugin's Flutter Start / Flutter End pair has to be fitted against, so it is
measured here rather than guessed.

Two things have to be got right or the number is meaningless:

  * The envelope hop has to be slow enough not to track the waveform. These
    sounds end around 30 Hz -- `refs.py` measures a median end centroid of 29 Hz
    for the downshifters -- so a 4 ms RMS envelope is partly following the
    carrier, and reports "flutter at 34 Hz" for a sound with none. A 12 ms hop
    puts Nyquist at 42 Hz and a 25 Hz lowpass on the envelope puts the carrier
    out of reach.
  * The gesture's own shape has to be divided out, or a sound that simply
    swells reads as fluttering at its own duration.

Then the rate is taken separately in the first and last third, which is what
gives the start-speed / end-speed pair.
"""
import sys, os, glob, math

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

DEFAULT_DIR = os.path.join(os.path.dirname(__file__), "../../!dev/references")
HOP_MS = 12.0
LO, HI = 2.0, 25.0


def env(mono, sr, hop_ms=HOP_MS):
    hop = max(1, int(round(sr * hop_ms * 1e-3)))
    n = len(mono) // hop
    frames = mono[:n * hop].reshape(n, hop)
    return np.sqrt((frames ** 2).mean(axis=1) + 1e-20), hop / sr


def rate_depth(e, dt, lo=LO, hi=HI):
    """Strongest modulation in [lo, hi] Hz of the envelope with its trend divided out."""
    if len(e) < 24:
        return 0.0, 0.0, 0.0
    w = max(3, int(round(0.20 / dt)) | 1)
    slow = np.convolve(e, np.ones(w) / w, mode="same")
    slow = np.maximum(slow, e.max() * 1e-3)
    r = (e / slow - 1.0) * np.hanning(len(e))
    n = 1 << int(math.ceil(math.log2(len(r)))) + 2
    P = np.abs(np.fft.rfft(r, n)) ** 2
    f = np.fft.rfftfreq(n, dt)
    band = (f >= lo) & (f <= hi)
    if not band.any() or P[band].sum() <= 0:
        return 0.0, 0.0, 0.0
    k = np.nonzero(band)[0][np.argmax(P[band])]
    # How much of the in-band energy sits at that rate: 1.0 is a metronome,
    # 0.1 is noise with no rate at all.
    peakiness = float(P[k] / P[band].sum())
    return float(f[k]), float(np.sqrt((r ** 2).mean()) * math.sqrt(2.0)), peakiness


def main(argv):
    root = argv[1] if len(argv) > 1 else DEFAULT_DIR
    cats = sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d)))
    for cat in cats:
        files = sorted(glob.glob(os.path.join(root, cat, "*.wav")))
        rows = []
        print("\n== %s" % cat)
        print("%-44s %7s %7s %7s   %6s %6s   %6s" %
              ("file", "rate0", "rate1", "ratio", "dep0", "dep1", "peaky"))
        for p in files:
            x, sr = wavio.read_wav(p)
            m = wavio.to_mono(x)
            e, dt = env(m, sr)
            if len(e) < 60:
                continue
            thr = e.max() * 10 ** (-45.0 / 20.0)
            idx = np.nonzero(e >= thr)[0]
            if len(idx) < 60:
                continue
            e = e[idx[0]:idx[-1] + 1]
            third = len(e) // 3
            r0, d0, k0 = rate_depth(e[:third], dt)
            r1, d1, k1 = rate_depth(e[-third:], dt)
            if r0 <= 0 or r1 <= 0:
                continue
            rows.append((r0, r1, d0, d1, 0.5 * (k0 + k1)))
            print("%-44s %7.2f %7.2f %7.2f   %6.2f %6.2f   %6.3f" %
                  (os.path.basename(p)[:44], r0, r1, r1 / r0, d0, d1, 0.5 * (k0 + k1)))
        if rows:
            a = np.array(rows)
            print("   %-41s %7.2f %7.2f %7.2f   %6.2f %6.2f   %6.3f" %
                  ("MEDIAN", *[np.median(a[:, i]) for i in range(2)],
                   np.median(a[:, 1] / a[:, 0]), np.median(a[:, 2]), np.median(a[:, 3]),
                   np.median(a[:, 4])))
            print("   %-41s %7.2f %7.2f %7.2f" %
                  ("10th pct", *[np.percentile(a[:, i], 10) for i in range(2)],
                   np.percentile(a[:, 1] / a[:, 0], 10)))
            print("   %-41s %7.2f %7.2f %7.2f" %
                  ("90th pct", *[np.percentile(a[:, i], 90) for i in range(2)],
                   np.percentile(a[:, 1] / a[:, 0], 90)))


if __name__ == "__main__":
    main(sys.argv)
