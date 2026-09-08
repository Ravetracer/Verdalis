"""Finds the tonal component in a wind recording, and asks whether it tracks the gust.

    SKYHOWL_REFS=/path/to/wavs python3 howl.py

Wind itself is silent. What is heard is air interacting with something, and when
that something is roughly cylindrical -- a wire, a twig, a blade of grass, the
edge of a gap -- the flow sheds vortices at the Strouhal frequency

    f = St * U / d,   St ~ 0.2 for a cylinder,

which is a *tone*, and one whose pitch is proportional to the wind speed. That
is what howling is, and it is the one prediction of the model a recording can
falsify: if the tone is aeolian, its pitch has to rise and fall with the level,
because both follow U.

**The prominence is measured frame by frame, not on the average spectrum.** A
howl swoops, so averaging half a minute of it smears the tone across an octave
and leaves no peak to find -- which is a property of the measurement, not of the
sound. Each frame is smoothed to a twelfth of an octave, a one-and-a-half-octave
running mean of it is taken as the bed the peak stands on, and the reported
figure is the median over frames of the tallest peak. That is what a listener
hears: a tone that is prominent at every instant even though its average is not.

Per recording:

  prom dB       median instantaneous prominence over a 1.5-octave bed. On its
                own this proves nothing: the tallest peak in any single frame of
                *noise* also stands 7-9 dB over a 1.5-octave mean, purely
                because a frame of noise is lumpy. It has to be read with the
                jitter beside it.
  jitter oct    median frame-to-frame movement of that peak, in octaves. This is
                what separates a tone from a lump: a real tone moves smoothly
                and slowly, so its jitter is a few hundredths of an octave,
                while the tallest lump in successive frames of noise jumps
                anywhere in the search band. A recording is counted as tonal
                below 0.15 octaves.
  peak Hz       median frequency of that peak
  swoop oct     how far it moves: the 10-90 % spread of the peak frequency,
                in octaves
  Q             centre frequency over -3 dB bandwidth, from the frame at the
                median prominence
  track r       correlation between the peak frequency and the frame's level.
                Positive is the Strouhal signature.
  implied d     the obstacle diameter that peak implies at 10 m/s, in mm
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np
import wavio

REFS = os.environ.get("SKYHOWL_REFS", os.path.expanduser("~/refs"))
STROUHAL = 0.2
FRAME = 1 << 13
LO, HI = 150.0, 3000.0


def octave_windows(freqs, frac):
    """Bin index bounds of a 1/frac-octave window around every bin."""
    r = 2.0 ** (1.0 / (2.0 * frac))
    lo = np.searchsorted(freqs, freqs / r, side="left")
    hi = np.searchsorted(freqs, freqs * r, side="right")
    return lo, np.maximum(hi, lo + 1)


def smooth(db, lo, hi, cum):
    """Mean of db over each precomputed window, via a cumulative sum."""
    cum[0] = 0.0
    np.cumsum(db, out=cum[1:])
    return (cum[hi] - cum[lo]) / (hi - lo)


def measure(path):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    if len(m) < FRAME * 4:
        return None
    win = np.hanning(FRAME)
    freqs = np.fft.rfftfreq(FRAME, 1.0 / sr)
    band = (freqs >= LO) & (freqs <= HI)
    if not band.any():
        return None

    lo12, hi12 = octave_windows(freqs, 12.0)
    loBed, hiBed = octave_windows(freqs, 0.66)
    cum = np.empty(len(freqs) + 1)

    proms, peaks, levels, smoothed = [], [], [], []
    for i in range(0, len(m) - FRAME, FRAME // 2):
        p = np.abs(np.fft.rfft(m[i:i + FRAME] * win)) ** 2
        db = 10.0 * np.log10(np.maximum(p, 1e-30))
        sm = smooth(db, lo12, hi12, cum)
        bed = smooth(sm, loBed, hiBed, cum)
        pr = np.where(band, sm - bed, -1e9)
        k = int(np.argmax(pr))
        proms.append(pr[k])
        peaks.append(freqs[k])
        levels.append(10.0 * np.log10(max(p.sum(), 1e-30)))
        smoothed.append(sm)

    proms = np.array(proms)
    peaks = np.array(peaks)
    levels = np.array(levels)
    prom = float(np.median(proms))
    peak = float(np.median(peaks))
    swoop = float(np.log2(max(np.percentile(peaks, 90), 1.0) /
                          max(np.percentile(peaks, 10), 1.0)))
    r = float(np.corrcoef(peaks, levels)[0, 1]) if peaks.std() > 0 else 0.0
    jitter = float(np.median(np.abs(np.diff(np.log2(np.maximum(peaks, 1.0)))))) \
        if len(peaks) > 2 else 0.0

    # Q from the frame whose prominence is the median one, so the width is
    # measured on a representative frame rather than on the smeared average.
    sm = smoothed[int(np.argsort(proms)[len(proms) // 2])]
    k = int(np.argmin(np.abs(freqs - peak)))
    top = sm[k]
    a = k
    while a > 1 and sm[a] > top - 3.0:
        a -= 1
    b = k
    while b < len(sm) - 2 and sm[b] > top - 3.0:
        b += 1
    q = peak / max(freqs[1], freqs[b] - freqs[a])

    d_mm = 1000.0 * STROUHAL * 10.0 / max(peak, 1.0)
    return prom, jitter, peak, swoop, q, r, d_mm


def main():
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    if not files:
        print("no .wav files in %s -- set SKYHOWL_REFS" % REFS)
        return 1
    print("%-42s%8s%8s%9s%10s%7s%9s%9s" %
          ("reference", "prom dB", "jitter", "peak Hz", "swoop oct", "Q", "track r", "d mm"))
    rows = []
    for f in files:
        got = measure(os.path.join(REFS, f))
        if not got:
            continue
        print("%-42s%8.1f%8.3f%9.0f%10.2f%7.1f%9.2f%9.2f" % ((f[:42],) + got))
        rows.append(got)
    a = np.array(rows)
    t = a[a[:, 1] <= 0.15]
    print()
    print("recordings with a tone (jitter <= 0.15 oct): %d of %d" % (len(t), len(a)))
    if len(t):
        for i, nm in enumerate(["prominence dB", "jitter oct", "peak Hz", "swoop oct", "Q",
                                "track r", "implied d mm"]):
            print("  %-14s %8.2f .. %8.2f, median %8.2f" %
                  (nm, t[:, i].min(), t[:, i].max(), np.median(t[:, i])))
        print("  track r positive in %d of %d" % (int((t[:, 5] > 0).sum()), len(t)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
