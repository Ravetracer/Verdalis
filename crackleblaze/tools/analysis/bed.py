"""The roar under the crackles: its colour, and how it moves.

    python3 bed.py [file...]

A fire's crest factor is 31.7 dB against a river's 19.6, so measuring its
spectrum straight measures the crackles. This gates them out first -- the
loudest 25% of 4 ms frames go, which is generous, and what is left is the bed --
and then asks the bed three things:

  colour   octave-band levels 125 Hz-16 kHz, which is what the `Fire` shapes
           ship as. Sub-60 Hz is excluded throughout: lowend.py finds it
           uncorrelated with the fire above it.
  surge    the coefficient of variation of each band's 100 ms envelope, and the
           correlation between neighbouring bands'. A fire flares; the question
           is whether it flares as one gain or band by band.
  rhythm   the slope of the envelope's own spectrum over 0.1-10 Hz, and whether
           any peak in it survives between recordings. If none does, the surge
           is a random walk and not an LFO.
"""
import sys, os, math, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import refs
import shapes as sh

OCT = np.array([125, 250, 500, 1000, 2000, 4000, 8000, 16000], float)
SECS = 30.0
GATE = 0.75  # keep the quietest 75% of 4 ms frames


def gated(m, sr):
    hop = max(1, int(sr * 0.004))
    k = len(m) // hop
    fr = m[:k * hop].reshape(k, hop)
    e = np.sqrt((fr ** 2).mean(axis=1) + 1e-30)
    keep = e <= np.quantile(e, GATE)
    return fr[keep].reshape(-1), keep.mean()


def band_env(m, sr, lo, hi, ms):
    n = 1 << int(math.ceil(math.log2(len(m))))
    X = np.fft.rfft(m, n)
    f = np.fft.rfftfreq(n, 1.0 / sr)
    X[(f < lo) | (f > hi)] = 0.0
    y = np.fft.irfft(X)[:len(m)]
    hop = max(1, int(sr * ms / 1000.0))
    k = len(y) // hop
    return np.sqrt(np.mean(y[:k * hop].reshape(k, hop) ** 2, axis=1) + 1e-30)


def quantile_env(m, sr, lo, hi, block_ms=100.0, frame_ms=4.0, q=0.25):
    """The bed's level, with whatever sits on top of it ignored."""
    e = band_env(m, sr, lo, hi, frame_ms)
    per = max(1, int(round(block_ms / frame_ms)))
    k = len(e) // per
    if k < 4:
        return e
    return np.quantile(e[:k * per].reshape(k, per), q, axis=1)


def measure(path):
    x, sr = wavio.read_wav(path)
    n = int(min(SECS * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = refs.highpass(wavio.to_mono(x[st:st + n]), sr)
    g, frac = gated(m, sr)

    bands = refs.band_levels(g, sr, OCT)
    full = refs.band_levels(m, sr, OCT)

    # Surge, on a *robust* envelope. Taking the RMS of a 100 ms window measures
    # the crackles in it -- they are 13 dB over the bed and the band they live
    # in is the band whose "surge" then comes out deepest, which is circular.
    # The 25th percentile of the 4 ms frames inside each 100 ms block ignores
    # them: a crackle occupies a handful of frames out of twenty-five.
    envs = [quantile_env(m, sr, fc / math.sqrt(2), min(fc * math.sqrt(2), sr * 0.45))
            for fc in OCT]
    cv = np.array([e.std() / max(e.mean(), 1e-30) for e in envs])
    cor = []
    for i in range(len(OCT) - 1):
        a, b = envs[i] - envs[i].mean(), envs[i + 1] - envs[i + 1].mean()
        d = math.sqrt(float(np.sum(a * a) * np.sum(b * b)))
        cor.append(float(np.sum(a * b) / d) if d > 0 else 0.0)

    # The envelope's own spectrum, 0.1-10 Hz, on the 1-4 kHz band.
    e = band_env(m, sr, 1000.0, 4000.0, 25.0)
    e = e - e.mean()
    nn = 1 << int(math.floor(math.log2(len(e))))
    mag = np.abs(np.fft.rfft(e[:nn] * np.hanning(nn))) ** 2
    f = np.fft.rfftfreq(nn, 0.025)
    sel = (f >= 0.1) & (f <= 10.0) & (mag > 0)
    slope = np.polyfit(np.log10(f[sel]), 10 * np.log10(mag[sel]), 1)[0] if sel.sum() > 8 else float("nan")
    pk = f[sel][np.argmax(mag[sel])] if sel.any() else float("nan")
    return dict(name=os.path.basename(path), bands=bands, full=full, cv=cv,
                cor=np.array(cor), slope=slope, peak=pk, frac=frac)


def main():
    files = sys.argv[1:] or [p for p in sorted(glob.glob(os.path.join(refs.DEFAULT_DIR, "*.wav")))
                             if os.path.basename(p) not in sh.RUMBLE_ONLY]
    rows = [measure(p) for p in files]
    print("bed colour, dB relative to the bed's own total (crackles gated out)")
    print("%-42s %s" % ("", " ".join("%6.0f" % f for f in OCT)))
    for r in rows:
        print("%-42s %s" % (r["name"][:42], " ".join("%6.1f" % v for v in r["bands"])))
    B = np.array([r["bands"] for r in rows])
    F = np.array([r["full"] for r in rows])
    print("%-42s %s" % ("MEDIAN bed", " ".join("%6.1f" % v for v in np.median(B, axis=0))))
    print("%-42s %s" % ("MEDIAN ungated", " ".join("%6.1f" % v for v in np.median(F, axis=0))))
    print("%-42s %s" % ("crackles add", " ".join("%+6.1f" % v for v in
          np.median(F, axis=0) - np.median(B, axis=0))))
    print()
    C = np.array([r["cv"] for r in rows])
    print("surge: CV of the 100 ms envelope, per band")
    print("%-42s %s" % ("min", " ".join("%6.2f" % v for v in C.min(axis=0))))
    print("%-42s %s" % ("median", " ".join("%6.2f" % v for v in np.median(C, axis=0))))
    print("%-42s %s" % ("max", " ".join("%6.2f" % v for v in C.max(axis=0))))
    print("%-42s %s" % ("median, rel. 1-2 kHz", " ".join("%6.2f" % v for v in
          np.median(C, axis=0) / np.median(C, axis=0)[3])))
    print()
    R = np.array([r["cor"] for r in rows])
    print("neighbouring-band envelope correlation")
    print("%-42s %s" % ("median", " ".join("%6.2f" % v for v in np.median(R, axis=0))))
    print()
    s = np.array([r["slope"] for r in rows]); s = s[np.isfinite(s)]
    p = np.array([r["peak"] for r in rows]); p = p[np.isfinite(p)]
    print("envelope spectrum slope 0.1-10 Hz: min %.2f  median %.2f  max %.2f dB/decade"
          % (s.min(), np.median(s), s.max()))
    print("strongest envelope frequency:      min %.2f  median %.2f  max %.2f Hz"
          % (p.min(), np.median(p), p.max()))
    print("gate kept %.0f%% of frames" % (100 * np.mean([r["frac"] for r in rows])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
