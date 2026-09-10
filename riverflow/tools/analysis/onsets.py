"""Counts and characterises the discrete water events in a reference.

    python3 onsets.py <file-or-dir> [--secs 20] [--band low|high|both]

A river's bed is noise, but the part that makes it a *creek* is not: it is a
population of discrete events sitting on top of the bed. There are two
populations, and they are physically different things:

  low  (150 Hz - 1.2 kHz)  The glug. Water folding over a stone entrains a
       large air pocket, which rings at the Minnaert frequency for its radius
       -- 3.26/r kHz with r in millimetres, so 300 Hz is a 10 mm pocket. This
       is the "dabbling between the stones" sound.

  high (2 - 14 kHz)  The tick. A single drop striking rock or a standing pool
       makes a short, bright impact plus a tiny entrained bubble. This is the
       "tickling" of a trickle and of the edge of a small waterfall.

Detection is spectral flux inside the band, thresholded against a moving
median, which is the same method ShoreBreak's bubbles.py uses -- reused so the
two plugins' figures are comparable.
"""
import sys, os, glob, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

BANDS = {"low": (150.0, 1200.0), "high": (2000.0, 14000.0)}
NFFT = 1024


def stft(m, sr, hop):
    win = np.hanning(NFFT)
    frames = (len(m) - NFFT) // hop
    if frames < 8:
        return None, None
    S = np.empty((frames, NFFT // 2 + 1))
    for i in range(frames):
        S[i] = np.abs(np.fft.rfft(m[i * hop:i * hop + NFFT] * win))
    return S, np.fft.rfftfreq(NFFT, 1.0 / sr)


def detect(m, sr, lo, hi, min_gap_ms=12.0, k=2.2):
    hop = max(1, int(sr * 0.004))
    S, freqs = stft(m, sr, hop)
    if S is None:
        return [], 0.0
    sel = (freqs >= lo) & (freqs <= hi)
    if sel.sum() < 3:
        return [], 0.0
    band = S[:, sel]
    bfreq = freqs[sel]
    d = np.diff(band, axis=0)
    flux = np.maximum(d, 0.0).sum(axis=1)
    # Moving median threshold: the bed drifts, the events do not.
    w = max(9, int(0.25 / 0.004) | 1)
    pad = np.pad(flux, w // 2, mode="edge")
    med = np.array([np.median(pad[i:i + w]) for i in range(len(flux))])
    mad = np.array([np.median(np.abs(pad[i:i + w] - med[i])) for i in range(len(flux))])
    thr = med + k * (mad * 1.4826 + 1e-12)

    min_gap = max(1, int(min_gap_ms / 4.0))
    onsets = []
    last = -10 ** 9
    for i in range(1, len(flux) - 1):
        if flux[i] > thr[i] and flux[i] >= flux[i - 1] and flux[i] > flux[i + 1]:
            if i - last < min_gap:
                continue
            last = i
            # The bin that grew most is the event's pitch.
            j = int(np.argmax(d[i]))
            onsets.append((( i + 1) * hop / sr, float(bfreq[j]), float(flux[i])))
    dur = len(m) / sr
    return onsets, dur


def summarise(path, secs, which):
    x, sr = wavio.read_wav(path)
    n = int(min(secs * sr, x.shape[0]))
    start = (x.shape[0] - n) // 2
    m = wavio.to_mono(x[start:start + n])
    out = {}
    for name in which:
        lo, hi = BANDS[name]
        onsets, dur = detect(m, sr, lo, min(hi, sr * 0.45))
        if not onsets:
            out[name] = (0.0, float("nan"), float("nan"), float("nan"), float("nan"))
            continue
        f = np.array([o[1] for o in onsets])
        t = np.array([o[0] for o in onsets])
        gaps = np.diff(t)
        out[name] = (len(onsets) / dur, float(np.median(f)),
                     float(np.percentile(f, 10)), float(np.percentile(f, 90)),
                     float(np.median(gaps)) if len(gaps) else float("nan"))
    return out


def main():
    argv = sys.argv[1:]
    secs = 20.0
    which = ["low", "high"]
    args = []
    i = 0
    while i < len(argv):
        if argv[i] == "--secs":
            secs = float(argv[i + 1]); i += 2
        elif argv[i] == "--band":
            if argv[i + 1] != "both":
                which = [argv[i + 1]]
            i += 2
        else:
            args.append(argv[i]); i += 1
    target = args[0] if args else os.path.join(os.path.dirname(__file__), "../../!dev/references")
    files = sorted(glob.glob(os.path.join(target, "*.wav"))) if os.path.isdir(target) else [target]

    hdr = "%-56s" % "reference"
    for w in which:
        hdr += " | %6s %7s %7s %7s %6s" % (w + "/s", "med Hz", "p10", "p90", "gap ms")
    print(hdr)
    acc = {w: [] for w in which}
    for p in files:
        try:
            r = summarise(p, secs, which)
        except Exception as exc:
            print("%-56s ERR %s" % (os.path.basename(p)[:56], exc))
            continue
        line = "%-56s" % os.path.basename(p)[:56]
        for w in which:
            rate, med, p10, p90, gap = r[w]
            line += " | %6.1f %7.0f %7.0f %7.0f %6.0f" % (rate, med, p10, p90,
                                                          gap * 1000 if gap == gap else 0)
            acc[w].append(r[w])
        print(line)
    print()
    for w in which:
        a = np.array(acc[w], float)
        if not len(a):
            continue
        print("%-6s rate/s  min %5.1f  median %5.1f  max %5.1f" % (
            w, np.nanmin(a[:, 0]), np.nanmedian(a[:, 0]), np.nanmax(a[:, 0])))
        print("%-6s med Hz  min %5.0f  median %5.0f  max %5.0f   (p10 median %5.0f, p90 median %5.0f)" % (
            w, np.nanmin(a[:, 1]), np.nanmedian(a[:, 1]), np.nanmax(a[:, 1]),
            np.nanmedian(a[:, 2]), np.nanmedian(a[:, 3])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
