"""Surveys the WhooshPact reference library: gesture shape, spectrum, sweep and flutter.

    python3 refs.py [dir] [--csv out.csv]

Every reference is one *gesture*: a sound with a beginning, a peak and an end,
unlike the continuous textures the rest of the suite models. So the survey is
per file rather than per window, and the quantities it takes are the ones a
gesture has -- how long it is, where in it the peak sits, how the spectrum
moves from the start to the peak to the end, and how much the amplitude is
being chopped up on the way.

The numbers this produces are what the parameter ranges, the six type profiles
and the factory presets are fitted to; see README.md for the conclusions.
"""
import sys, os, glob, math, csv

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

DEFAULT_DIR = os.path.join(os.path.dirname(__file__), "../../!dev/references")

# Octave centres. 31.5 Hz is in because these sounds live down there in a way
# nothing else in the suite does -- the whole point of a boom.
OCT = np.array([31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000], float)

ENV_HOP_MS = 4.0      # envelope frame hop
GATE_DB = -45.0       # what counts as the sound having started / stopped


# ----------------------------------------------------------------- envelope

def envelope(mono, sr, hop_ms=ENV_HOP_MS):
    """RMS envelope on a fixed hop, plus the hop in seconds."""
    hop = max(1, int(round(sr * hop_ms * 1e-3)))
    n = len(mono) // hop
    if n < 4:
        return np.zeros(1), hop / sr
    frames = mono[:n * hop].reshape(n, hop)
    return np.sqrt((frames ** 2).mean(axis=1) + 1e-20), hop / sr


def active_span(env, gate_db=GATE_DB):
    """First and last frame above `gate_db` under the envelope's own peak."""
    thr = env.max() * (10.0 ** (gate_db / 20.0))
    above = np.nonzero(env >= thr)[0]
    if len(above) == 0:
        return 0, len(env) - 1
    return int(above[0]), int(above[-1])


# ------------------------------------------------------------------ spectra

def band_levels(mono, sr, centres=OCT, width=2.0):
    """Energy per octave band, dB relative to the broadband total."""
    n = 1 << 14
    if len(mono) < n:
        n = 1 << max(10, int(math.floor(math.log2(max(len(mono), 1024)))))
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    acc = np.zeros(len(centres))
    total = 0.0
    hop = n // 2
    frames = 0
    for i in range(0, max(len(mono) - n, 0) + 1, hop):
        seg = mono[i:i + n]
        if len(seg) < n:
            break
        P = np.abs(np.fft.rfft(seg * win)) ** 2
        total += P.sum()
        for b, fc in enumerate(centres):
            lo, hi = fc / math.sqrt(width), fc * math.sqrt(width)
            acc[b] += P[(freqs >= lo) & (freqs < hi)].sum()
        frames += 1
    if frames == 0 or total <= 0:
        return np.full(len(centres), -120.0)
    return 10.0 * np.log10(acc / total + 1e-12)


def centroid(mono, sr):
    """Spectral centroid in Hz, power weighted."""
    n = 1 << 12
    if len(mono) < n:
        n = 1 << max(8, int(math.floor(math.log2(max(len(mono), 256)))))
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    num = den = 0.0
    hop = n // 2
    for i in range(0, max(len(mono) - n, 0) + 1, hop):
        seg = mono[i:i + n]
        if len(seg) < n:
            break
        P = np.abs(np.fft.rfft(seg * win)) ** 2
        num += (P * freqs).sum()
        den += P.sum()
    return num / den if den > 0 else 0.0


def low_fraction(mono, sr, f0):
    """Fraction of the total energy below f0."""
    n = 1 << 14
    if len(mono) < n:
        n = 1 << max(10, int(math.floor(math.log2(max(len(mono), 1024)))))
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    lo = tot = 0.0
    hop = n // 2
    for i in range(0, max(len(mono) - n, 0) + 1, hop):
        seg = mono[i:i + n]
        if len(seg) < n:
            break
        P = np.abs(np.fft.rfft(seg * win)) ** 2
        lo += P[freqs < f0].sum()
        tot += P.sum()
    return lo / tot if tot > 0 else 0.0


# ------------------------------------------------------------------ flutter
#
# The amplitude being chopped. Taken on the envelope with its slow shape
# divided out, so what is left is the chopping alone and a gesture that merely
# swells does not read as fluttering at 0.3 Hz.

def flutter(env, dt, lo=2.0, hi=60.0):
    """(rate Hz, depth 0..1) of the strongest amplitude modulation in [lo, hi]."""
    if len(env) < 16:
        return 0.0, 0.0
    # Divide out the gesture shape: a 150 ms moving average is slower than any
    # flutter worth the name and faster than any gesture.
    w = max(3, int(round(0.150 / dt)) | 1)
    kernel = np.ones(w) / w
    slow = np.convolve(env, kernel, mode="same")
    slow = np.maximum(slow, env.max() * 1e-3)
    r = env / slow - 1.0
    r *= np.hanning(len(r))
    n = 1 << int(math.ceil(math.log2(len(r))))
    P = np.abs(np.fft.rfft(r, n)) ** 2
    f = np.fft.rfftfreq(n, dt)
    band = (f >= lo) & (f <= hi)
    if not band.any() or P[band].sum() <= 0:
        return 0.0, 0.0
    k = np.nonzero(band)[0][np.argmax(P[band])]
    # Depth as the RMS of the residual, which is what a tremolo depth means.
    return float(f[k]), float(np.sqrt((r ** 2).mean()) * math.sqrt(2.0))


# -------------------------------------------------------------------- survey

def survey(path):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    if len(m) < sr // 10:
        return None

    env, dt = envelope(m, sr)
    a, b = active_span(env)
    span = max((b - a + 1) * dt, dt)
    seg = m[int(a * dt * sr):int((b + 1) * dt * sr)]
    if len(seg) < 512:
        return None
    e = env[a:b + 1]

    peak_i = int(np.argmax(e))
    peak_pos = peak_i / max(len(e) - 1, 1)

    # Where the energy actually is, which for a gesture is a better summary of
    # its shape than where the single loudest frame landed.
    cum = np.cumsum(e ** 2)
    cum /= cum[-1]
    t50 = float(np.searchsorted(cum, 0.5) / max(len(e) - 1, 1))

    # Rise and fall, as the 10-90 % times of the envelope either side of peak.
    def cross(seq, frac, rising):
        thr = frac * seq.max()
        idx = np.nonzero(seq >= thr)[0]
        if len(idx) == 0:
            return 0
        return int(idx[0] if rising else idx[-1])
    rise = (peak_i - cross(e[:peak_i + 1], 0.1, True)) * dt
    tail = e[peak_i:]
    fall = (cross(tail, 0.1, False)) * dt

    # The sweep: centroid over the first, middle and last fifth of the gesture.
    fifth = max(len(seg) // 5, 256)
    c_start = centroid(seg[:fifth], sr)
    c_mid = centroid(seg[2 * fifth:3 * fifth], sr)
    c_end = centroid(seg[-fifth:], sr)
    sweep = math.log2(max(c_end, 20.0) / max(c_start, 20.0))

    bands = band_levels(seg, sr)
    fl_rate_a, fl_depth_a = flutter(e[:max(len(e) // 2, 8)], dt)
    fl_rate_b, fl_depth_b = flutter(e[max(len(e) // 2, 8):], dt)

    crest = 20.0 * math.log10(np.abs(seg).max() / (np.sqrt((seg ** 2).mean()) + 1e-12) + 1e-12)

    if x.shape[1] >= 2:
        l, r = x[:, 0], x[:, 1]
        denom = math.sqrt((l ** 2).sum() * (r ** 2).sum()) + 1e-20
        corr = float((l * r).sum() / denom)
    else:
        corr = 1.0

    return {
        "file": os.path.basename(path),
        "span": span,
        "peak_pos": peak_pos,
        "t50": t50,
        "rise": rise,
        "fall": fall,
        "c_start": c_start,
        "c_mid": c_mid,
        "c_end": c_end,
        "sweep_oct": sweep,
        "sub100": low_fraction(seg, sr, 100.0),
        "sub200": low_fraction(seg, sr, 200.0),
        "crest": crest,
        "corr": corr,
        "fl_rate_a": fl_rate_a,
        "fl_depth_a": fl_depth_a,
        "fl_rate_b": fl_rate_b,
        "fl_depth_b": fl_depth_b,
        "bands": bands,
    }


def main(argv):
    root = DEFAULT_DIR
    out_csv = None
    args = [a for a in argv[1:]]
    if "--csv" in args:
        i = args.index("--csv")
        out_csv = args[i + 1]
        del args[i:i + 2]
    if args:
        root = args[0]

    cats = sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d)))
    rows = []
    for cat in cats:
        files = sorted(glob.glob(os.path.join(root, cat, "*.wav")))
        print("\n== %s (%d files)" % (cat, len(files)))
        print("%-44s %6s %5s %5s %6s %6s %7s %7s %6s %6s %5s %5s  %5s/%4s %5s/%4s" %
              ("file", "span", "peak", "t50", "rise", "fall", "c0", "c1",
               "sweep", "sub100", "crest", "corr", "flA", "dA", "flB", "dB"))
        for p in files:
            s = survey(p)
            if s is None:
                continue
            s["cat"] = cat
            rows.append(s)
            print("%-44s %6.2f %5.2f %5.2f %6.3f %6.3f %7.0f %7.0f %6.2f %6.2f %5.1f %5.2f "
                  " %5.1f/%4.2f %5.1f/%4.2f" %
                  (s["file"][:44], s["span"], s["peak_pos"], s["t50"], s["rise"], s["fall"],
                   s["c_start"], s["c_end"], s["sweep_oct"], s["sub100"], s["crest"], s["corr"],
                   s["fl_rate_a"], s["fl_depth_a"], s["fl_rate_b"], s["fl_depth_b"]))

    print("\n\n================ medians by category ================")
    keys = ["span", "peak_pos", "t50", "rise", "fall", "c_start", "c_mid", "c_end",
            "sweep_oct", "sub100", "sub200", "crest", "corr",
            "fl_rate_a", "fl_depth_a", "fl_rate_b", "fl_depth_b"]
    print("%-14s %s" % ("category", " ".join("%9s" % k for k in keys)))
    for cat in cats:
        sel = [r for r in rows if r["cat"] == cat]
        if not sel:
            continue
        print("%-14s %s" % (cat, " ".join("%9.3f" % np.median([r[k] for r in sel]) for k in keys)))

    print("\n================ octave-band shape by category (dB rel. total) ================")
    print("%-14s %s" % ("category", " ".join("%7.0f" % f for f in OCT)))
    for cat in cats:
        sel = [r for r in rows if r["cat"] == cat]
        if not sel:
            continue
        med = np.median(np.array([r["bands"] for r in sel]), axis=0)
        print("%-14s %s" % (cat, " ".join("%7.1f" % v for v in med)))

    print("\n================ spread (10th / 90th percentile) ================")
    for cat in cats:
        sel = [r for r in rows if r["cat"] == cat]
        if not sel:
            continue
        print("-- %s" % cat)
        for k in ["span", "peak_pos", "rise", "fall", "sweep_oct", "sub100", "crest",
                  "fl_rate_b", "fl_depth_b"]:
            v = np.array([r[k] for r in sel], float)
            print("   %-12s %8.3f  ..%8.3f   (median %8.3f)" %
                  (k, np.percentile(v, 10), np.percentile(v, 90), np.median(v)))

    if out_csv:
        with open(out_csv, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["cat", "file"] + keys + ["b%g" % f for f in OCT])
            for r in rows:
                w.writerow([r["cat"], r["file"]] + ["%.4f" % r[k] for k in keys] +
                           ["%.2f" % v for v in r["bands"]])
        print("\nwrote %s (%d rows)" % (out_csv, len(rows)))


if __name__ == "__main__":
    main(sys.argv)
