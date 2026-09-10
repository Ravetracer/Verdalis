"""Does the slow surge move the whole spectrum together, or the top more?

    python3 bandsurge.py <file>...

The bed's level wanders (see surge.py). The engine has to decide whether that
wander is a plain gain change or a change of colour -- more energetic
turbulence entrains smaller bubbles, so there is a physical reason to expect
the top to move further. Cheap to implement either way, so it is worth
measuring rather than assuming.

Prints, per band, the modulation depth of its 100 ms envelope, and the
correlation of that envelope with the 1-2 kHz band's. Equal depths and
correlations near 1 mean one gain; depths rising with frequency mean a tilt.
"""
import sys, os, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import grain

B = [("125-250", 125., 250.), ("250-500", 250., 500.), ("0.5-1k", 500., 1000.),
     ("1-2k", 1000., 2000.), ("2-4k", 2000., 4000.), ("4-8k", 4000., 8000.),
     ("8-16k", 8000., 16000.)]
REF = 3

print("%-42s %s" % ("reference", "  ".join("%14s" % b[0] for b in B)))
print("%-42s %s" % ("", "  ".join("%7s%7s" % ("depth%", "corr") for _ in B)))
D, C = [], []
for p in sys.argv[1:]:
    x, sr = wavio.read_wav(p)
    n = int(min(30.0 * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = wavio.to_mono(x[st:st + n])
    envs = [grain.band_envelope(m, sr, lo, hi, env_ms=100.0) for _, lo, hi in B]
    k = min(len(e) for e in envs)
    envs = [e[:k] for e in envs]
    ref = envs[REF] - envs[REF].mean()
    d, c = [], []
    for e in envs:
        d.append(100.0 * e.std() / max(e.mean(), 1e-12))
        a = e - e.mean()
        den = math.sqrt(float(np.sum(a * a) * np.sum(ref * ref)))
        c.append(float(np.sum(a * ref) / den) if den > 0 else float("nan"))
    D.append(d); C.append(c)
    print("%-42s %s" % (os.path.basename(p)[:42],
          "  ".join("%7.1f%7.2f" % (d[i], c[i]) for i in range(len(B)))))
D, C = np.array(D), np.array(C)
print()
print("%-42s %s" % ("median depth % / median corr",
      "  ".join("%7.1f%7.2f" % (np.median(D[:, i]), np.median(C[:, i])) for i in range(len(B)))))
print()
print("depth relative to the 1-2 kHz band, median: %s" %
      "  ".join("%s %.2fx" % (B[i][0], np.median(D[:, i] / D[:, REF])) for i in range(len(B))))
