"""The stridulation layer in *time*: is a rendered chorus a field or a machine?

    python3 chorus.py                      # the references
    python3 chorus.py --render <wav> ...   # anything else, measured the same way
    python3 chorus.py --class cricket      # one class only

`pulse.py` measures what a caller is -- its carrier, its Q, its pulse and echeme
rates -- and 0.2.0 reproduced all of it. This file measures what a *chorus* is,
because that is what 0.2.0 got wrong and no per-file median could have said so.

Four statistics, all taken from the envelope of the stridulation band rather
than from its spectrum:

  steadiness      the tenth percentile of the band's envelope over its median.
                  How deep the holes in the chorus are. A field of callers has
                  few; a dozen callers sharing one clock rate drift in and out
                  of phase together and leave many.

  chirp width     the -6 dB width of the chirp peak in the envelope's own
                  modulation spectrum, as a fraction of its centre. A chorus of
                  metronomes is a picket fence of sharp lines; a real one is a
                  hump, because a real caller's clock wanders.

  within-chirp    the crest factor and duty of the envelope *inside* the
  crest, duty     chirps. This is where a click into a resonator differs from a
                  scraper dragged across a file: the first sounds for 6 % of
                  each pulse period, the second for most of it.

What they said, and what was changed at 0.3.0:

    statistic          references (9 cricket)     0.2.0      0.3.0
    steadiness         0.13 .. 0.68, med 0.37      0.09       0.36
    chirp width        0.07 .. 0.86, med 0.31      0.10       0.21
    within-chirp crest 1.70 .. 9.06, med 2.24      2.23       1.99
    within-chirp duty  0.08 .. 0.69, med 0.32      0.33       0.39

The spectral statistics did not move and did not need to: the band's peak,
width, Q, comb depth and flatness were inside the references' spread before the
change and are inside it after. **The layer was never wrong about what a cricket
is. It was wrong about what a field of them does.**
"""
import sys, os, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
from refs import classify

BAND = (1500.0, 14000.0)
MAX_SEC = 30.0


def band_peak(m, sr, n=1 << 15):
    """The stridulation band's centre: the peak of the long-window spectrum."""
    if len(m) < n:
        n = 1 << int(np.floor(np.log2(max(len(m), 256))))
    hop = n // 2
    cnt = max(1, (len(m) - n) // hop)
    idx = np.arange(n)[None, :] + hop * np.arange(cnt)[:, None]
    mag = np.abs(np.fft.rfft(m[idx] * np.hanning(n), axis=1))
    fr = np.fft.rfftfreq(n, 1.0 / sr)
    p = (mag ** 2).mean(axis=0)
    sel = (fr >= BAND[0]) & (fr <= min(BAND[1], 0.45 * sr))
    if sel.sum() < 16:
        return None
    return float(fr[sel][int(np.argmax(p[sel]))])


def band_envelope(m, sr, peak, nfft, hop):
    """The band's amplitude envelope, and the rate it is sampled at."""
    cnt = max(16, (len(m) - nfft) // hop)
    idx = np.arange(nfft)[None, :] + hop * np.arange(cnt)[:, None]
    mag = np.abs(np.fft.rfft(m[idx] * np.hanning(nfft), axis=1))
    fr = np.fft.rfftfreq(nfft, 1.0 / sr)
    b = (fr > peak * 0.7) & (fr < peak * 1.3)
    if b.sum() < 2:
        return None, 0.0
    return np.sqrt((mag[:, b] ** 2).sum(axis=1)), sr / float(hop)


def measure(path, seconds=MAX_SEC):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)[:int(seconds * sr)]
    if len(m) < sr:
        return None
    peak = band_peak(m, sr)
    if not peak:
        return None

    # steadiness and the chirp peak, from a 2.7 ms envelope
    e, esr = band_envelope(m, sr, peak, 1024, 128)
    if e is None:
        return None
    steady = float(np.percentile(e, 10) / max(np.median(e), 1e-30))
    v = e - e.mean()
    sp = np.abs(np.fft.rfft(v * np.hanning(len(v)))) ** 2
    mf = np.fft.rfftfreq(len(v), 1.0 / esr)
    s = (mf >= 0.5) & (mf <= 25.0)
    centre = width = 0.0
    if s.sum() >= 16:
        f, pv = mf[s], sp[s]
        w = max(2, int(round(0.25 / (f[1] - f[0]))))
        sm = np.convolve(np.pad(pv, w, mode="edge"), np.ones(2 * w + 1) / (2 * w + 1),
                         mode="same")[w:w + len(pv)]
        k = int(np.argmax(sm))
        half = sm[k] / 4.0
        a = k
        while a > 0 and sm[a] > half:
            a -= 1
        b = k
        while b < len(sm) - 1 and sm[b] > half:
            b += 1
        centre, width = float(f[k]), float(f[b] - f[a])

    # within-chirp crest and duty, from a 0.7 ms envelope
    e2, _ = band_envelope(m, sr, peak, 256, 32)
    crest = duty = 0.0
    if e2 is not None:
        inside = e2[e2 > np.percentile(e2, 60)]
        if len(inside) >= 64:
            top = np.percentile(inside, 99)
            crest = float(top / max(inside.mean(), 1e-30))
            duty = float(np.mean(inside > 0.5 * top))
    return dict(peak=peak, steady=steady, chirp=centre,
                width=width / max(centre, 1e-9), crest=crest, duty=duty)


def show(rows, label):
    if not rows:
        return
    print("%-46s %8s %8s %9s %8s %7s %6s" %
          (label, "peak Hz", "steady", "chirp Hz", "width/f", "crest", "duty"))
    for name, r in rows:
        print("%-46s %8.0f %8.2f %9.2f %8.2f %7.2f %6.2f" %
              (name[:46], r["peak"], r["steady"], r["chirp"], r["width"], r["crest"],
               r["duty"]))
    a = np.array([[r["steady"], r["width"], r["crest"], r["duty"]] for _, r in rows])
    print("%-46s %8s %8.2f %9s %8.2f %7.2f %6.2f" %
          ("median", "", np.median(a[:, 0]), "", np.median(a[:, 1]), np.median(a[:, 2]),
           np.median(a[:, 3])))
    print()


def main():
    args = sys.argv[1:]
    if "--render" in args:
        i = args.index("--render")
        rows = []
        for p in args[i + 1:]:
            for q in sorted(glob.glob(p)):
                r = measure(q)
                if r:
                    rows.append((os.path.basename(q), r))
        show(rows, "render")
        return 0

    want = None
    if "--class" in args:
        want = args[args.index("--class") + 1]
    here = os.path.dirname(os.path.abspath(__file__))
    refs = os.environ.get("INSECTSWARM_REFS", os.path.join(here, "../../!dev/references"))
    for cls in ("cricket", "cicada"):
        if want and cls != want:
            continue
        rows = []
        for p in sorted(glob.glob(os.path.join(refs, "*.wav"))):
            if classify(os.path.basename(p)) != cls:
                continue
            r = measure(p)
            if r:
                rows.append((os.path.basename(p), r))
        show(rows, cls)
    return 0


if __name__ == "__main__":
    sys.exit(main())
