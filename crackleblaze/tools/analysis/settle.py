"""Do logs shift? Low-frequency thumps, as a population of their own.

    python3 settle.py [file...]

A fire's fourth sound, after the roar, the ticks and the hiss, is the log that
collapses into the embers: a dull thump with no top end. It is rare, so it has
to be looked for on purpose -- an onset detector run on 80-300 Hz, with events
that also jump in the 2-12 kHz band thrown away, because a crackle loud enough
leaks into every band and would otherwise be counted twice.
"""
import sys, os, math, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import crackles, refs
import shapes as sh

SECS = 60.0


def measure(path):
    x, sr = wavio.read_wav(path)
    n = int(min(SECS * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = refs.highpass(wavio.to_mono(x[st:st + n]), sr)
    lo, hoplo, _ = crackles.band_env(m, sr, 80.0, 300.0, env_ms=2.0)
    hi, hophi, _ = crackles.band_env(m, sr, 2000.0, min(12000.0, sr * 0.45), env_ms=2.0)
    k = min(len(lo), len(hi))
    lo, hi = lo[:k], hi[:k]
    rate = sr / hoplo
    w = int(1.0 * rate) | 1
    pad = np.pad(lo, w // 2, mode="edge")
    med = np.array([np.median(pad[i:i + w]) for i in range(0, k, 16)])
    med = np.interp(np.arange(k), np.arange(len(med)) * 16, med)
    mad = np.median(np.abs(lo - med)) * 1.4826 + 1e-30
    hmed = np.median(hi)
    hmad = np.median(np.abs(hi - hmed)) * 1.4826 + 1e-30

    idx, i, gap = [], 1, max(1, int(0.05 * rate))
    while i < k - 1:
        if lo[i] > med[i] + 8.0 * mad and lo[i] >= lo[i - 1] and lo[i] > lo[i + 1]:
            # Not a crackle bleeding downwards: the top has to stay put.
            if hi[i] < hmed + 4.0 * hmad:
                idx.append(i)
            i += gap
        else:
            i += 1
    if not idx:
        return dict(name=os.path.basename(path), rate=0.0, n=0, prom=float("nan"),
                    dec=float("nan"), dur=n / sr)
    idx = np.array(idx, int)
    prom = 20 * np.log10(lo[idx] / np.maximum(med[idx], 1e-30))
    dec = []
    for i in idx:
        p, j = lo[i], i
        lim = min(k, i + int(1.0 * rate))
        while j < lim - 1 and lo[j] > p * 0.3162:
            j += 1
        dec.append((j - i) / rate * 1000.0)
    return dict(name=os.path.basename(path), rate=len(idx) / (n / sr), n=len(idx),
                prom=float(np.median(prom)), dec=float(np.median(dec)), dur=n / sr)


files = sys.argv[1:] or [p for p in sorted(glob.glob(os.path.join(refs.DEFAULT_DIR, "*.wav")))
                         if os.path.basename(p) not in sh.RUMBLE_ONLY]
rows = [measure(p) for p in files]
print("%-44s %6s %8s %8s %8s" % ("reference", "n", "per min", "prom dB", "dec ms"))
for r in rows:
    print("%-44s %6d %8.1f %8.1f %8.1f" % (r["name"][:44], r["n"], 60 * r["rate"], r["prom"], r["dec"]))
for k in ("rate", "prom", "dec"):
    v = np.array([r[k] for r in rows], float); v = v[np.isfinite(v)]
    if k == "rate":
        v = v * 60
    print("%-8s min %8.2f  median %8.2f  max %8.2f" % (k, v.min(), np.median(v), v.max()))
