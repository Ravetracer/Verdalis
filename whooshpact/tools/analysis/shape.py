"""The shape of a gesture: its envelope, its spectral sweep and its low-band pitch.

    python3 shape.py [dir]

`refs.py` reduces each reference to scalars; this keeps the *curves*. Every
gesture is put on a common normalised time axis 0..1 across its active span and
the median across each category is printed as a contour of 16 points.

Three contours are taken:

  env      the amplitude envelope, normalised to its own peak. This is the
           rise / mid / fall the plugin's Span, Peak and Shape parameters have
           to reproduce.
  centroid the spectral centroid in octaves relative to the gesture's own start,
           which is the sweep: a whoosh opening up and closing again, a
           downshifter falling off the bottom.
  pitch    the strongest partial below 300 Hz, in octaves relative to the start.
           Only meaningful where there is one -- the column of how often a peak
           was found at all says whether to believe it.

A contour of sixteen numbers is a formula, not a sample; see the suite's note
on what pure synthesis does and does not forbid.
"""
import sys, os, glob, math

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

DEFAULT_DIR = os.path.join(os.path.dirname(__file__), "../../!dev/references")
N = 16  # points on the normalised time axis


def active(m, sr):
    hop = max(1, int(round(sr * 0.004)))
    n = len(m) // hop
    e = np.sqrt((m[:n * hop].reshape(n, hop) ** 2).mean(axis=1) + 1e-20)
    thr = e.max() * 10 ** (-45.0 / 20.0)
    idx = np.nonzero(e >= thr)[0]
    if len(idx) < 8:
        return m, e
    return m[idx[0] * hop:(idx[-1] + 1) * hop], e[idx[0]:idx[-1] + 1]


def contours(path):
    x, sr = wavio.read_wav(path)
    seg, e = active(wavio.to_mono(x), sr)
    if len(seg) < sr // 10:
        return None

    # envelope, resampled onto the normalised axis
    env = np.interp(np.linspace(0, 1, N), np.linspace(0, 1, len(e)), e)
    env /= env.max() + 1e-20

    # centroid and low-band pitch, one measurement per slice
    win = 1 << 12
    cen = np.zeros(N)
    pit = np.zeros(N)
    ok = np.zeros(N)
    freqs = np.fft.rfftfreq(win, 1.0 / sr)
    w = np.hanning(win)
    low = (freqs >= 25.0) & (freqs <= 300.0)
    for i in range(N):
        c = int((i + 0.5) / N * len(seg))
        a = max(0, min(c - win // 2, len(seg) - win))
        s = seg[a:a + win]
        if len(s) < win:
            s = np.pad(s, (0, win - len(s)))
        P = np.abs(np.fft.rfft(s * w)) ** 2
        tot = P.sum()
        cen[i] = (P * freqs).sum() / tot if tot > 0 else 0.0
        Pl = P[low]
        if tot > 0 and Pl.sum() / tot > 0.05:
            pit[i] = freqs[low][int(np.argmax(Pl))]
            ok[i] = 1.0
    return env, cen, pit, ok


def main(argv):
    root = argv[1] if len(argv) > 1 else DEFAULT_DIR
    cats = sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d)))
    axis = " ".join("%6.2f" % t for t in np.linspace(0, 1, N))
    for cat in cats:
        envs, cens, pits, oks = [], [], [], []
        for p in sorted(glob.glob(os.path.join(root, cat, "*.wav"))):
            c = contours(p)
            if c is None:
                continue
            env, cen, pit, ok = c
            envs.append(env)
            if cen[0] > 20:
                cens.append(np.log2(np.maximum(cen, 20.0) / cen[0]))
            if ok.all() and pit[0] > 20:
                pits.append(np.log2(np.maximum(pit, 20.0) / pit[0]))
            oks.append(ok.mean())
        if not envs:
            continue
        print("\n== %s  (%d files, low partial found in %.0f%% of slices)" %
              (cat, len(envs), 100.0 * float(np.mean(oks))))
        print("   t        %s" % axis)
        print("   env      %s" % " ".join("%6.3f" % v for v in np.median(np.array(envs), axis=0)))
        if cens:
            print("   cent oct %s" % " ".join("%6.2f" % v for v in np.median(np.array(cens), axis=0)))
        if pits:
            print("   pitch oct%s   (%d files)" %
                  (" ".join("%6.2f" % v for v in np.median(np.array(pits), axis=0)), len(pits)))


if __name__ == "__main__":
    main(sys.argv)
