"""What one crackle is made of, and whether there is more than one kind.

    python3 crackshape.py [file...]

crackles.py counts them and times them. This asks what they sound like.

For every detected onset it takes 8 ms from the onset, subtracts the spectrum
of the 8 ms before it, and averages what is left over all the events in a
recording -- the crackle alone, with the roar it sat in removed. Then:

  shape     the bed-subtracted excess in octave bands, normalised to its own
            peak. This is what the crackle's noise burst gets filtered to.
  flat      the spectral flatness of that excess: a struck piece of wood rings
            and comes out peaky, a bursting gas pocket is a click and comes out
            flat. 1.0 is white.
  split     the population is bimodal in decay (crackles.py: median 3 ms, p90
            out to 52 ms). Events are split at 8 ms and each half measured
            separately, because a 3 ms tick and a 40 ms hiss are not the same
            physical event and cannot share a generator.
"""
import sys, os, math, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import crackles, refs
import shapes as sh

OCT = np.array([250, 500, 1000, 2000, 4000, 8000, 16000], float)
SPLIT_MS = 8.0
SECS = 40.0


def measure(path):
    x, sr = wavio.read_wav(path)
    n = int(min(SECS * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = refs.highpass(wavio.to_mono(x[st:st + n]), sr)
    hi = min(crackles.BAND[1], sr * 0.45)
    env, hop, _ = crackles.band_env(m, sr, crackles.BAND[0], hi)
    rate = sr / hop
    w = int(0.2 * rate) | 1
    pad = np.pad(env, w // 2, mode="edge")
    med = np.array([np.median(pad[i:i + w]) for i in range(0, len(env), 8)])
    med = np.interp(np.arange(len(env)), np.arange(len(med)) * 8, med)
    mad = np.median(np.abs(env - med)) * 1.4826 + 1e-30
    thr = med + 6.0 * mad
    idx, i, gap = [], 1, max(1, int(0.004 * rate))
    while i < len(env) - 1:
        if env[i] > thr[i] and env[i] >= env[i - 1] and env[i] > env[i + 1]:
            idx.append(i); i += gap
        else:
            i += 1
    idx = np.array(idx, int)
    if len(idx) < 30:
        return None

    dec = []
    for i in idx:
        p, j = env[i], i
        lim = min(len(env), i + int(0.3 * rate))
        while j < lim - 1 and env[j] > p * 0.3162:
            j += 1
        dec.append((j - i) / rate * 1000.0)
    dec = np.array(dec)

    NF = int(0.008 * sr) // 2 * 2
    f = np.fft.rfftfreq(NF, 1.0 / sr)
    win = np.hanning(NF)
    out = {}
    for label, sel in (("tick", dec <= SPLIT_MS), ("hiss", dec > SPLIT_MS)):
        acc, k = np.zeros(len(f)), 0
        for i in idx[sel]:
            a = i * hop
            if a - NF < 0 or a + NF >= len(m):
                continue
            acc += np.maximum(np.abs(np.fft.rfft(m[a:a + NF] * win)) ** 2 -
                              np.abs(np.fft.rfft(m[a - NF:a] * win)) ** 2, 0.0)
            k += 1
        if k < 10:
            out[label] = None
            continue
        acc /= k
        b = []
        for fc in OCT:
            s = (f >= fc / math.sqrt(2)) & (f < min(fc * math.sqrt(2), sr * 0.45))
            b.append(acc[s].mean() if s.any() else 0.0)
        b = np.array(b)
        db = 10 * np.log10(np.maximum(b, 1e-30) / max(b.max(), 1e-30))
        cent = float((f * acc).sum() / max(acc.sum(), 1e-30))
        sl = (f > 500) & (f < min(14000, sr * 0.45))
        flat = float(math.exp(np.log(np.maximum(acc[sl], 1e-30)).mean()) /
                     max(acc[sl].mean(), 1e-30))
        out[label] = dict(db=db, cent=cent, flat=flat, frac=float(sel.mean()),
                          n=int(sel.sum()), dec=float(np.median(dec[sel])))
    return dict(name=os.path.basename(path), out=out, n=len(idx), dur=n / sr)


def main():
    files = sys.argv[1:] or [p for p in sorted(glob.glob(os.path.join(refs.DEFAULT_DIR, "*.wav")))
                             if os.path.basename(p) not in sh.RUMBLE_ONLY]
    rows = [r for r in (measure(p) for p in files) if r]
    for label in ("tick", "hiss"):
        print()
        print("=== %s ===  (decay %s %g ms)" % (
            label, "<=" if label == "tick" else ">", SPLIT_MS))
        print("%-42s %5s %6s %6s %6s  %s" % ("", "share", "rate/s", "cent", "flat",
              " ".join("%6.0f" % f for f in OCT)))
        D = []
        for r in rows:
            o = r["out"].get(label)
            if not o:
                continue
            D.append(o["db"])
            print("%-42s %5.2f %6.2f %6.0f %6.2f  %s" % (
                r["name"][:42], o["frac"], o["n"] / r["dur"], o["cent"], o["flat"],
                " ".join("%6.1f" % v for v in o["db"])))
        D = np.array(D)
        print("%-42s %5s %6s %6s %6s  %s" % ("MEDIAN", "", "", "", "",
              " ".join("%6.1f" % v for v in np.median(D, axis=0))))
        for k in ("frac", "cent", "flat", "dec"):
            v = np.array([r["out"][label][k] for r in rows if r["out"].get(label)])
            print("   %-8s min %8.2f  median %8.2f  max %8.2f" % (k, v.min(), np.median(v), v.max()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
