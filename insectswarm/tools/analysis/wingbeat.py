"""Measures the wingbeat fundamental and its harmonic stack, per recording and per species.

    python3 wingbeat.py [--class bee] [--clarity 0.45] [dir]

A flying insect's wing is a driven oscillator: the tone is the wingbeat rate and
everything above it is a harmonic of that rate. So the flight model is a small
set of numbers -- the fundamental, how many harmonics carry energy, how fast the
fundamental wanders, and how much of the time the insect is audible at all --
and this measures them.

Each 85 ms frame is band-limited to 60-4000 Hz and pitched with the NSDF in
`pitch.py`. A frame counts as a wingbeat only when its clarity clears
`MIN_CLARITY`; `voiced` is the share of frames that did, which is itself a
measurement -- a hive is voiced almost continuously, a flyby is not.

Harmonic levels are read off the frame's own spectrum at multiples of the
estimated fundamental, relative to the fundamental, and the reported stack is
the median over the voiced frames.
"""
import sys, os, glob, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
from refs import classify
from cache import cached_dir
import pitch

F0_LO, F0_HI = 40.0, 1200.0
BAND = (60.0, 4000.0)       # the buzz band; above it is cicada, not wingbeat
N_HARM = 16
MIN_CLARITY = 0.45
FRAME_SEC = 0.085
MAX_SEC = 120.0
FLYERS = ("bee", "bumblebee", "wasp", "hornet", "fly", "mosquito", "dragonfly")


def measure(path, min_clarity=MIN_CLARITY, max_sec=MAX_SEC):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    if len(m) > int(max_sec * sr):
        s = (len(m) - int(max_sec * sr)) // 2   # the middle; the ends are the walk-up
        m = m[s:s + int(max_sec * sr)]
    n = 1 << int(round(math.log2(FRAME_SEC * sr)))
    if len(m) < n * 4:
        return None
    hop, win = n // 2, np.hanning(n)
    df = sr / float(n)
    lo_lag, hi_lag = sr / F0_HI, sr / F0_LO
    rms_all = math.sqrt(float(np.mean(m * m)) + 1e-30)
    f0s, clar, parts, times, lev = [], [], [], [], []
    nframes = 0
    for i in range(0, len(m) - n, hop):
        seg = m[i:i + n]
        nframes += 1
        if math.sqrt(float(np.mean(seg * seg))) < 0.05 * rms_all:
            continue
        b = pitch.bandpass(seg, sr, *BAND)
        p, c = pitch.pick_period(pitch.nsdf(b), lo_lag, hi_lag)
        if p is None or c < min_clarity:
            continue
        f0 = sr / p
        mag = np.abs(np.fft.rfft(seg * win))
        mag = np.maximum(mag, np.maximum(np.roll(mag, 1), np.roll(mag, -1)))
        idx = np.clip(np.round(np.arange(1, N_HARM + 1) * f0 / df).astype(int), 0, len(mag) - 1)
        pl = 20.0 * np.log10(np.maximum(mag[idx], 1e-30))
        f0s.append(f0); clar.append(c); parts.append(pl - pl[0]); times.append(i / float(sr))
        lev.append(20.0 * math.log10(math.sqrt(float(np.mean(seg * seg))) + 1e-30))
    if len(f0s) < 4:
        return None
    f0s = np.array(f0s); parts = np.array(parts); times = np.array(times)
    med = float(np.median(f0s))
    keep = np.abs(np.log2(f0s / med)) < 0.585   # a fifth out is a second source
    f0k, pk, tk = f0s[keep], parts[keep], times[keep]
    if len(f0k) < 4:
        return None
    med = float(np.median(f0k))
    q1, q3 = np.percentile(f0k, [25, 75])
    steps = np.abs(np.diff(1200.0 * np.log2(f0k)))
    return dict(f0=med, spread=1200.0 * math.log2(q3 / max(q1, 1e-9)),
                jitter=float(np.median(steps)) if len(steps) else 0.0,
                clarity=float(np.median(np.array(clar)[keep])),
                n=len(f0k), voiced=len(f0k) / float(max(nframes, 1)),
                parts=np.median(pk, axis=0), track=f0k, times=tk,
                level=np.array(lev)[keep])


def main():
    args = sys.argv[1:]
    want = args[args.index("--class") + 1] if "--class" in args else None
    mc = float(args[args.index("--clarity") + 1]) if "--clarity" in args else MIN_CLARITY
    d = next((a for a in args if os.path.isdir(a)), cached_dir())
    print("%-52s %-9s %7s %7s %7s %6s %6s %5s  %s" %
          ("file", "class", "f0", "iqr_ct", "jit_ct", "clar", "voiced", "n",
           "harmonics 2..9 re f0 (dB)"), flush=True)
    rows = []
    for p in sorted(glob.glob(os.path.join(d, "*.wav"))):
        name = os.path.basename(p)
        cls = classify(name)
        if want and cls != want:
            continue
        r = measure(p, mc)
        if r is None:
            print("%-52s %-9s   (unvoiced)" % (name[:52], cls), flush=True); continue
        print("%-52s %-9s %7.1f %7.0f %7.0f %6.2f %6.2f %5d  %s" %
              (name[:52], cls, r["f0"], r["spread"], r["jitter"], r["clarity"],
               r["voiced"], r["n"], " ".join("%5.1f" % v for v in r["parts"][1:9])), flush=True)
        rows.append((cls, r))

    print("\n%-10s %3s %8s %8s %8s %7s %7s %7s  %s" %
          ("class", "n", "f0_med", "f0_lo", "f0_hi", "iqr_ct", "jit_ct", "voiced",
           "harmonics 2..9 re f0 (dB)"))
    for cls in [c for c in FLYERS if any(r[0] == c for r in rows)] + \
               sorted(set(r[0] for r in rows) - set(FLYERS)):
        sel = [r[1] for r in rows if r[0] == cls]
        f = np.array([s["f0"] for s in sel])
        P = np.median(np.array([s["parts"] for s in sel]), axis=0)
        print("%-10s %3d %8.1f %8.1f %8.1f %7.0f %7.0f %7.2f  %s" %
              (cls, len(sel), np.median(f), f.min(), f.max(),
               np.median([s["spread"] for s in sel]), np.median([s["jitter"] for s in sel]),
               np.median([s["voiced"] for s in sel]),
               " ".join("%5.1f" % v for v in P[1:9])))


if __name__ == "__main__":
    main()
