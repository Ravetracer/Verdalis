"""Surveys the InsectSwarm reference library: format, level, spectrum and class.

    python3 refs.py [dir] [--csv out.csv]

Prints one row per recording: duration, sample rate, channels, the broadband
crest factor, the spectral centroid and the band levels. The class column is
taken from the filename and is the grouping every other script here uses.
"""
import sys, os, glob, math, re
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

DEFAULT_DIR = os.path.join(os.path.dirname(__file__), "../../!dev/references")
OCT = np.array([63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000], float)
HP_HZ = 40.0

# The filenames carry the species; nothing else in the library does. Order
# matters -- "bumble" must be tested before "bee".
CLASSES = [
    ("mosquito", ("mosquito", "mosquitos")),
    ("cicada",   ("cicada", "cicadas", "ciacdas", "heat-bug")),
    ("cricket",  ("cricket", "crickets", "grille", "grill-llarg", "bocegi", "night-ambience", "night-atmosphere")),
    ("bumblebee",("bumble", "bumblebee")),
    ("hornet",   ("hornet",)),
    ("wasp",     ("wasp",)),
    ("dragonfly",("dragonfly",)),
    ("fly",      ("flies", "mouche", "fly-is", "fly_", "fly-buzz", "buzz-zzz", "of-a-fly")),
    ("bee",      ("bee", "bees", "honey-bee", "hive", "pollinat", "buzz-polli")),
]


def classify(name):
    low = name.lower()
    for cls, keys in CLASSES:
        for k in keys:
            if k in low:
                return cls
    return "other"


def highpass(m, sr, f0=HP_HZ):
    n = 1 << int(math.ceil(math.log2(len(m))))
    X = np.fft.rfft(m, n)
    X[np.fft.rfftfreq(n, 1.0 / sr) < f0] = 0.0
    return np.fft.irfft(X)[:len(m)]


def band_levels(m, sr, centres=OCT, width=2.0, n=1 << 14):
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
    acc /= max(frames, 1)
    total = acc.sum()
    return 10.0 * np.log10(np.maximum(acc, 1e-30) / max(total, 1e-30))


def centroid(m, sr, n=1 << 14):
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    num = den = 0.0
    for i in range(0, max(len(m) - n, 1), n // 2):
        seg = m[i:i + n]
        if len(seg) < n:
            break
        mag = np.abs(np.fft.rfft(seg * win)) ** 2
        num += (freqs * mag).sum(); den += mag.sum()
    return num / max(den, 1e-30)


def crest(m):
    r = math.sqrt(float(np.mean(m * m)) + 1e-30)
    return 20.0 * math.log10(float(np.max(np.abs(m)) + 1e-30) / r)


def load(path, limit_sec=60.0):
    x, sr = wavio.read_wav(path)
    ch = x.shape[1]
    m = wavio.to_mono(x)
    if len(m) > int(limit_sec * sr):
        # a window from the middle -- the ends are approach and handling
        s = (len(m) - int(limit_sec * sr)) // 2
        m = m[s:s + int(limit_sec * sr)]
    return m, sr, ch, len(x) / float(sr)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    d = args[0] if args else DEFAULT_DIR
    files = sorted(glob.glob(os.path.join(d, "*.wav")))
    print("%-58s %-9s %6s %2s %7s %7s %7s  %s" %
          ("file", "class", "dur", "ch", "sr", "crest", "centr", "  ".join("%5d" % f for f in OCT)))
    rows = []
    for p in files:
        name = os.path.basename(p)
        try:
            m, sr, ch, dur = load(p)
        except Exception as e:
            print("%-58s  !! %s" % (name[:58], e)); continue
        if len(m) < sr // 4:
            print("%-58s  !! too short" % name[:58]); continue
        mh = highpass(m, sr)
        bl = band_levels(mh, sr)
        cls = classify(name)
        print("%-58s %-9s %6.1f %2d %7d %7.1f %7.0f  %s" %
              (name[:58], cls, dur, ch, sr, crest(mh), centroid(mh, sr),
               "  ".join("%5.1f" % v for v in bl)))
        rows.append((name, cls, dur, ch, sr, crest(mh), centroid(mh, sr), bl))

    print()
    print("%-10s %3s %7s %7s %7s" % ("class", "n", "dur", "crest", "centroid"))
    for cls, _ in CLASSES + [("other", ())]:
        sel = [r for r in rows if r[1] == cls]
        if not sel:
            continue
        print("%-10s %3d %7.1f %7.1f %7.0f" % (
            cls, len(sel), sum(r[2] for r in sel),
            float(np.median([r[5] for r in sel])),
            float(np.median([r[6] for r in sel]))))
    print("\ntotal %d files, %.1f min" % (len(rows), sum(r[2] for r in rows) / 60.0))


if __name__ == "__main__":
    main()
