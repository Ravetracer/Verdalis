"""Event-triggered analysis: what one water event actually is.

    python3 events.py <file> [<file>...] [--band low|high] [--secs 12]

grain.py establishes *that* the grainy references carry discrete events. This
measures *what* they are. For each detected event it takes the spectrum of a
short window at the onset, subtracts the local bed spectrum measured just
before it, and averages the excess over all events. What is left is the
spectrum of the event alone, with the river it sits in removed.

From that:

  peak       the event's pitch. By Minnaert, r = 3.26 / f0 (r in mm, f0 in kHz),
             so a peak is a bubble radius as directly as it is a frequency.
  Q          from the -3 dB width of the peak, which gives the ring time as
             Q / (pi f0).
  decay      measured independently from the event-triggered band envelope, as
             the time to fall 10 dB from the onset peak. The two should agree,
             and where they do not the peak is a cluster rather than one bubble.
"""
import sys, os, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

NFFT = 2048


def band_env(m, sr, lo, hi, env_ms=1.0):
    n = 1 << int(math.ceil(math.log2(len(m))))
    X = np.fft.rfft(m, n)
    f = np.fft.rfftfreq(n, 1.0 / sr)
    X[(f < lo) | (f > hi)] = 0.0
    y = np.fft.irfft(X)[:len(m)]
    hop = max(1, int(sr * env_ms / 1000.0))
    k = len(y) // hop
    return np.sqrt(np.mean(y[:k * hop].reshape(k, hop) ** 2, axis=1) + 1e-30), hop


def find_events(m, sr, lo, hi, k=4.0, min_gap_ms=25.0):
    """Onsets by envelope jump, thresholded well above the noise floor.

    A high threshold on purpose: grain.py shows the bed is near-Gaussian, so a
    2-sigma detector would return mostly bed. Only events that stand 4 MAD
    above the local median are counted, which is the population that makes the
    reference audibly grainy in the first place.
    """
    env, hop = band_env(m, sr, lo, hi)
    d = np.diff(env, prepend=env[0])
    w = 201
    pad = np.pad(d, w // 2, mode="edge")
    med = np.median(pad)
    mad = np.median(np.abs(pad - med)) * 1.4826 + 1e-30
    thr = med + k * mad
    gap = max(1, int(min_gap_ms))
    idx = []
    last = -10 ** 9
    for i in range(2, len(d) - 2):
        if d[i] > thr and d[i] >= d[i - 1] and d[i] > d[i + 1]:
            if i - last < gap:
                continue
            last = i
            idx.append(i)
    return np.array(idx, int) * hop, env, hop


def measure(path, band, secs):
    # The low band starts at 400 Hz, not 150. Below that the references carry
    # wind and handling noise -- lowend.py measures it as uncorrelated with the
    # water -- and a detector given 150 Hz locks onto it: creek-ambience.wav
    # returned a "pocket" at 328 Hz with a Q of 1.4 and a spectral flatness of
    # 0.63, which is not a resonance, and the fitter turned that into a 9.9 mm
    # bubble. The measured population is 562-1406 Hz, so 400 Hz clears the
    # contamination with margin to spare.
    lo, hi = (400.0, 1600.0) if band == "low" else (1800.0, 16000.0)
    x, sr = wavio.read_wav(path)
    n = int(min(secs * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = wavio.to_mono(x[st:st + n])
    hi = min(hi, sr * 0.45)
    pos, env, hop = find_events(m, sr, lo, hi)
    # A run of onsets too close to a window edge is unusable.
    pre = int(sr * 0.010)
    post = NFFT
    pos = pos[(pos > pre + NFFT) & (pos < len(m) - post - NFFT)]
    if len(pos) < 12:
        return None

    win = np.hanning(NFFT)
    freqs = np.fft.rfftfreq(NFFT, 1.0 / sr)
    ev = np.zeros(len(freqs))
    bed = np.zeros(len(freqs))
    for p in pos:
        ev += np.abs(np.fft.rfft(m[p:p + NFFT] * win)) ** 2
        bed += np.abs(np.fft.rfft(m[p - pre - NFFT:p - pre] * win)) ** 2
    ev /= len(pos)
    bed /= len(pos)
    excess = np.maximum(ev - bed, 0.0)

    sel = (freqs >= lo) & (freqs <= hi)
    if not sel.any() or excess[sel].max() <= 0:
        return None

    # Smooth before measuring a width. The excess spectrum is an average of a
    # few hundred noisy periodograms, and its bin-to-bin scatter is easily as
    # large as the peak it sits on -- so the half-power crossing found by
    # walking raw bins lands wherever the noise happens to dip. Unsmoothed and
    # at a 512-point transform this returned a Q of 18 for a signal that
    # measures 7 at a different window length, which is not a measurement.
    k = 9
    kern = np.hanning(k + 2)[1:-1]
    kern /= kern.sum()
    sm = np.convolve(excess, kern, mode="same")

    fi = np.where(sel)[0][int(np.argmax(sm[sel]))]
    f0 = freqs[fi]
    half = sm[fi] * 0.5
    a = fi
    while a > 1 and sm[a] > half:
        a -= 1
    b = fi
    while b < len(sm) - 2 and sm[b] > half:
        b += 1
    # Only trust a width that is actually resolved: a peak whose half-power
    # points are within a couple of bins of each other is at the resolution
    # limit and its Q is a property of the transform, not of the water.
    if (b - a) < 3:
        return None
    bw = max(freqs[b] - freqs[a], freqs[1])
    q = f0 / bw

    # Event-triggered envelope, for the decay measured directly.
    L = int(sr * 0.150 / hop)
    stack = np.zeros(L)
    cnt = 0
    for p in pos:
        i = p // hop
        if i + L < len(env):
            stack += env[i:i + L]
            cnt += 1
    if cnt:
        stack /= cnt
        pk = stack.max()
        pi = int(np.argmax(stack))
        tail = stack[pi:]
        below = np.where(tail < pk * 10 ** (-10.0 / 20.0))[0]
        t10 = (below[0] * hop / sr * 1000.0) if len(below) else float("nan")
    else:
        t10 = float("nan")

    # Spectral flatness of the event's own spectrum, which is the statistic that
    # actually separates a tuned tick from a tack off a surface -- and, unlike a
    # -3 dB width, one that is stable.
    #
    # A width has to find a peak first, and an event population whose members
    # are each tuned to a *different* random pitch has no consistent peak to
    # find: the average spectrum is a smear, and the width measured off it
    # depends on which pitch happened to dominate the window. That is why this
    # replaced a Q estimate that returned 21 and 8 for the same file at two
    # window lengths, while giving the same reference 5.5 both times.
    #
    # Flatness is the geometric mean over the arithmetic mean across the band:
    # 1.0 is white, and the peakier the spectrum the lower it goes. A drop that
    # takes its colour from the stone it hit is broad and lands high; a drop
    # modelled as a tuned resonance lands low however its width is estimated.
    band = excess[sel]
    band = band[band > 0]
    flat = float("nan")
    if len(band) > 16:
        flat = float(np.exp(np.mean(np.log(band))) / max(np.mean(band), 1e-30))

    # Excess above the bed, in dB, at the peak: how far an event stands out.
    prom = 10.0 * math.log10(max(ev[fi], 1e-30) / max(bed[fi], 1e-30))
    return dict(n=len(pos), rate=len(pos) / (n / sr), f0=f0, q=q, t10=t10, flat=flat,
                radius_mm=3.26e3 / f0 if f0 > 0 else float("nan"),
                ring_ms=q / (math.pi * f0) * 1000.0 if f0 > 0 else float("nan"),
                prom=prom)


def main():
    argv = sys.argv[1:]
    band = "low"
    secs = 12.0
    files = []
    i = 0
    while i < len(argv):
        if argv[i] == "--band":
            band = argv[i + 1]; i += 2
        elif argv[i] == "--secs":
            secs = float(argv[i + 1]); i += 2
        else:
            files.append(argv[i]); i += 1
    print("band=%s" % band)
    print("%-46s %6s %6s %8s %6s %6s %8s %8s %6s" % (
        "reference", "n", "ev/s", "peak Hz", "Q", "flat", "radius", "t-10 ms", "prom"))
    rows = []
    for p in files:
        try:
            r = measure(p, band, secs)
        except Exception as exc:
            print("%-46s ERR %s" % (os.path.basename(p)[:46], exc)); continue
        if r is None:
            print("%-46s (too few events)" % os.path.basename(p)[:46]); continue
        rows.append(r)
        print("%-46s %6d %6.1f %8.0f %6.1f %6.3f %8.2f %8.1f %6.1f" % (
            os.path.basename(p)[:46], r["n"], r["rate"], r["f0"], r["q"], r["flat"],
            r["radius_mm"], r["t10"], r["prom"]))
    if rows:
        print()
        for k in ("rate", "f0", "q", "flat", "radius_mm", "ring_ms", "t10", "prom"):
            v = np.array([r[k] for r in rows], float)
            v = v[np.isfinite(v)]
            print("%-10s min %8.2f  median %8.2f  max %8.2f" % (k, v.min(), np.median(v), v.max()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
