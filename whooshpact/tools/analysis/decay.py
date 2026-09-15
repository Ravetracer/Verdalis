"""How long each octave band takes to die, per category, and what the pitched
layers are actually pitched at.

    python3 decay.py [dir]

Two things the scalar survey cannot say.

*Per-band decay.* An impact is not one decay: its sub rings for seconds while
its top is gone in a tenth of that, and the ratio between them is most of what
separates a boom from an accent. Measured as the time from the band's own peak
to 20 dB below it.

*Fundamental.* The braams and the downshifters are the only pitched material in
the library. This finds the strongest partial below 200 Hz in the loudest
half-second and reports it in Hz and as a note, which is what the Tone layer's
default pitch is set from.
"""
import sys, os, glob, math

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

DEFAULT_DIR = os.path.join(os.path.dirname(__file__), "../../!dev/references")
OCT = np.array([31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000], float)
NOTES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def note_name(hz):
    if hz <= 0:
        return "-"
    n = int(round(12 * math.log2(hz / 440.0) + 69))
    return "%s%d" % (NOTES[n % 12], n // 12 - 1)


def band_decays(m, sr):
    """Time from each octave band's peak to 20 dB below it, in seconds."""
    win, hop = 2048, 512
    n = (len(m) - win) // hop
    if n < 8:
        return np.full(len(OCT), float("nan"))
    freqs = np.fft.rfftfreq(win, 1.0 / sr)
    w = np.hanning(win)
    e = np.zeros((len(OCT), n))
    for i in range(n):
        P = np.abs(np.fft.rfft(m[i * hop:i * hop + win] * w)) ** 2
        for b, fc in enumerate(OCT):
            sel = (freqs >= fc / math.sqrt(2)) & (freqs < fc * math.sqrt(2))
            e[b, i] = P[sel].sum()
    out = np.full(len(OCT), float("nan"))
    dt = hop / sr
    for b in range(len(OCT)):
        k = int(np.argmax(e[b]))
        thr = e[b, k] * 1e-2  # -20 dB in power
        rest = np.nonzero(e[b, k:] < thr)[0]
        if len(rest):
            out[b] = rest[0] * dt
    return out


def fundamental(m, sr):
    """Strongest partial below 200 Hz in the loudest half second."""
    win = 1 << 15
    if len(m) < win:
        win = 1 << int(math.floor(math.log2(len(m))))
    hop = win // 4
    best, besti = -1.0, 0
    for i in range(0, len(m) - win + 1, hop):
        p = (m[i:i + win] ** 2).sum()
        if p > best:
            best, besti = p, i
    s = m[besti:besti + win] * np.hanning(win)
    P = np.abs(np.fft.rfft(s)) ** 2
    f = np.fft.rfftfreq(win, 1.0 / sr)
    sel = (f >= 25.0) & (f <= 200.0)
    if not sel.any() or P[sel].sum() <= 0:
        return 0.0
    return float(f[sel][int(np.argmax(P[sel]))])


def main(argv):
    root = argv[1] if len(argv) > 1 else DEFAULT_DIR
    cats = sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d)))
    print("decay to -20 dB per octave band, seconds (median per category)")
    print("%-14s %s" % ("category", " ".join("%7.0f" % f for f in OCT)))
    funds = {}
    for cat in cats:
        rows = []
        fs = []
        for p in sorted(glob.glob(os.path.join(root, cat, "*.wav"))):
            x, sr = wavio.read_wav(p)
            m = wavio.to_mono(x)
            rows.append(band_decays(m, sr))
            fs.append(fundamental(m, sr))
        if not rows:
            continue
        a = np.array(rows)
        print("%-14s %s" % (cat, " ".join(
            "%7.2f" % v if v == v else "      -" for v in np.nanmedian(a, axis=0))))
        funds[cat] = np.array(fs)

    print("\nstrongest partial below 200 Hz, in the loudest half second")
    print("%-14s %8s %8s %8s   %s" % ("category", "10th", "median", "90th", "median note"))
    for cat, f in funds.items():
        f = f[f > 0]
        if not len(f):
            continue
        med = float(np.median(f))
        print("%-14s %8.1f %8.1f %8.1f   %s" %
              (cat, np.percentile(f, 10), med, np.percentile(f, 90), note_name(med)))


if __name__ == "__main__":
    main(sys.argv)
