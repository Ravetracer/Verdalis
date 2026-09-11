"""Surveys the CrackleBlaze reference library: spectral shape, slope, envelope and stereo.

    python3 refs.py [dir] [--csv out.csv]

Reads a window from the middle of every recording (the ends are where the
recordist walks up to the water and away from it again) and prints one row per
file. The numbers this produces are what the parameter ranges and the factory
presets are fitted to; see README.md for the conclusions drawn from them.
"""
import sys, os, glob, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

DEFAULT_DIR = os.path.join(os.path.dirname(__file__), "../../!dev/references")
WINDOW_SEC = 20.0
OCT = np.array([63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000], float)
HP_HZ = 60.0  # lowend.py: sub-60 Hz is uncorrelated with the fire in every
              # reference. It is the room and the microphone, not the flame,
              # and eight of the 25 files are more than a quarter sub-60 Hz.


def highpass(m, sr, f0=HP_HZ):
    """Brick-wall in the frequency domain -- offline, so no filter design."""
    n = 1 << int(math.ceil(math.log2(len(m))))
    X = np.fft.rfft(m, n)
    X[np.fft.rfftfreq(n, 1.0 / sr) < f0] = 0.0
    return np.fft.irfft(X)[:len(m)]


def band_levels(m, sr, centres, width=2.0):
    """Energy per band, in dB relative to the broadband total."""
    n = 1 << 14
    if len(m) < n:
        return np.full(len(centres), -120.0)
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    acc = np.zeros(len(centres))
    hop = n // 2
    frames = 0
    for i in range(0, len(m) - n, hop):
        mag = np.abs(np.fft.rfft(m[i:i + n] * win)) ** 2
        for j, fc in enumerate(centres):
            lo, hi = fc / math.sqrt(width), fc * math.sqrt(width)
            sel = (freqs >= lo) & (freqs < hi)
            if sel.any():
                acc[j] += mag[sel].sum()
        frames += 1
    if frames == 0:
        return np.full(len(centres), -120.0)
    acc /= frames
    total = acc.sum()
    return 10.0 * np.log10(np.maximum(acc, 1e-30) / max(total, 1e-30))


def spectrum(m, sr, n=1 << 14):
    """Averaged power spectrum, for slope fits."""
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    acc = np.zeros(len(freqs))
    frames = 0
    for i in range(0, max(1, len(m) - n), n // 2):
        acc += np.abs(np.fft.rfft(m[i:i + n] * win)) ** 2
        frames += 1
    return freqs, acc / max(frames, 1)


def slope(freqs, psd, f0, f1):
    """dB per octave over [f0, f1], least squares in log-log."""
    sel = (freqs >= f0) & (freqs <= f1) & (psd > 0)
    if sel.sum() < 8:
        return float("nan")
    x = np.log2(freqs[sel])
    y = 10.0 * np.log10(psd[sel])
    a = np.polyfit(x, y, 1)
    return a[0]


def envelope(m, sr, ms):
    hop = max(1, int(sr * ms / 1000.0))
    k = len(m) // hop
    if k < 4:
        return np.array([1.0])
    return np.sqrt(np.mean(m[:k * hop].reshape(k, hop) ** 2, axis=1) + 1e-20)


def mod_peak(env, rate_hz, lo=0.2, hi=25.0):
    """Strongest modulation frequency of the envelope, and its depth.

    A river's bed is steady; a creek gurgles. The modulation spectrum is where
    that difference shows up as a number rather than an impression.
    """
    e = env - env.mean()
    if len(e) < 64 or env.mean() <= 0:
        return float("nan"), 0.0
    n = 1 << int(math.floor(math.log2(len(e))))
    e = e[:n] * np.hanning(n)
    mag = np.abs(np.fft.rfft(e))
    f = np.fft.rfftfreq(n, 1.0 / rate_hz)
    sel = (f >= lo) & (f <= hi)
    if not sel.any():
        return float("nan"), 0.0
    idx = np.argmax(mag[sel])
    depth = mag[sel][idx] / (n / 4) / max(env.mean(), 1e-12)
    return f[sel][idx], depth


def centroid_rolloff(freqs, psd):
    tot = psd.sum()
    if tot <= 0:
        return float("nan"), float("nan")
    c = (freqs * psd).sum() / tot
    cum = np.cumsum(psd) / tot
    r = freqs[np.searchsorted(cum, 0.9)] if cum[-1] >= 0.9 else freqs[-1]
    return c, r


def measure(path):
    x, sr = wavio.read_wav(path)
    if x.shape[0] < sr:
        return None
    # A window from the middle: the ends are approach and departure.
    n = int(min(WINDOW_SEC * sr, x.shape[0]))
    start = (x.shape[0] - n) // 2
    x = x[start:start + n]
    m = highpass(wavio.to_mono(x), sr)
    corr = 1.0
    if x.shape[1] >= 2:
        a, b = x[:, 0], x[:, 1]
        d = math.sqrt(np.sum(a * a) * np.sum(b * b))
        corr = float(np.sum(a * b) / d) if d > 0 else 1.0

    rms = math.sqrt(float(np.mean(m ** 2)))
    peak = float(np.max(np.abs(m)))
    crest = 20.0 * math.log10(peak / max(rms, 1e-12))

    freqs, psd = spectrum(m, sr)
    cent, roll = centroid_rolloff(freqs, psd)
    bands = band_levels(m, sr, OCT)

    e50 = envelope(m, sr, 50.0)
    e4 = envelope(m, sr, 4.0)
    cv50 = float(e50.std() / max(e50.mean(), 1e-12))
    cv4 = float(e4.std() / max(e4.mean(), 1e-12))
    mf, md = mod_peak(e50, sr / max(1, int(sr * 0.05)))

    return dict(
        name=os.path.basename(path), sr=sr, ch=x.shape[1], secs=n / sr,
        rms_db=20.0 * math.log10(max(rms, 1e-12)), crest=crest,
        cent=cent, roll=roll,
        s_low=slope(freqs, psd, 80.0, 400.0),
        s_mid=slope(freqs, psd, 400.0, 2000.0),
        s_hi=slope(freqs, psd, 2000.0, min(16000.0, sr * 0.45)),
        cv50=cv50, cv4=cv4, modf=mf, modd=md, corr=corr,
        bands=bands,
    )


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    d = args[0] if args else DEFAULT_DIR
    files = sorted(glob.glob(os.path.join(d, "*.wav")) + glob.glob(os.path.join(d, "*.mp3")))
    if not files:
        print("no references in", d)
        return 1
    rows = []
    for p in files:
        try:
            r = measure(p)
        except Exception as exc:
            print("%-58s ERR %s" % (os.path.basename(p), exc))
            continue
        if r is None:
            continue
        rows.append(r)
        print("%-56s %5.0f %5.1f %6.0f %6.0f %6.2f %6.2f %6.2f %5.2f %5.2f %5.2f %5.2f %5.2f" % (
            r["name"][:56], r["cent"], r["crest"], r["roll"], r["modf"] * 1000 if r["modf"] == r["modf"] else 0,
            r["s_low"], r["s_mid"], r["s_hi"], r["cv50"], r["cv4"], r["modf"], r["modd"], r["corr"]))

    print()
    print("columns: centroid  crest  rolloff90  --  slope dB/oct low(60-400) mid(400-2k) hi(2k-16k)"
          "  cv50  cv4  modHz  modDepth  L/Rcorr")
    print()
    keys = ["cent", "roll", "crest", "s_low", "s_mid", "s_hi", "cv50", "cv4", "modf", "modd", "corr"]
    print("%-10s %9s %9s %9s %9s" % ("quantity", "min", "median", "max", "mean"))
    for k in keys:
        v = np.array([r[k] for r in rows], float)
        v = v[np.isfinite(v)]
        print("%-10s %9.2f %9.2f %9.2f %9.2f" % (k, v.min(), np.median(v), v.max(), v.mean()))
    print()
    print("octave bands, dB relative to broadband total")
    print("%-40s %s" % ("", " ".join("%6.0f" % f for f in OCT)))
    B = np.array([r["bands"] for r in rows])
    for label, v in (("min", B.min(axis=0)), ("median", np.median(B, axis=0)), ("max", B.max(axis=0))):
        print("%-40s %s" % (label, " ".join("%6.1f" % b for b in v)))
    print()
    for r in rows:
        print("%-56s %s" % (r["name"][:56], " ".join("%6.1f" % b for b in r["bands"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
