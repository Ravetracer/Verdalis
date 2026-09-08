"""Third-octave shape and high-frequency slope of every wind reference.

    SKYHOWL_REFS=/path/to/wavs python3 spectra.py

Prints, per recording, the crest factor, the L/R correlation, the third-octave
levels relative to the broadband total, and the slope of the spectrum above
500 Hz in dB per octave.

The slope is the number the airflow bed is fitted to. Kolmogorov's inertial
subrange gives a velocity spectrum going as f^-5/3, which is -5.0 dB/octave in
power; aerodynamic noise radiated from a rigid surface (Curle's dipole) steepens
that further. Anything much shallower than -3 dB/oct is not turbulence, it is a
recording with something else in it.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np
import wavio

REFS = os.environ.get("SKYHOWL_REFS", os.path.expanduser("~/refs"))
CENTRES = np.array([31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000], float)


def measure(path):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    if len(m) < sr:
        return None
    rms = np.sqrt(np.mean(m ** 2))
    crest = 20 * np.log10(np.max(np.abs(m)) / max(1e-12, rms))
    if x.shape[1] >= 2:
        l, r = x[:, 0], x[:, 1]
        corr = float(np.corrcoef(l, r)[0, 1])
    else:
        corr = 1.0

    n = 1 << 14
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    acc = np.zeros(len(CENTRES))
    frames = 0
    for i in range(0, max(1, len(m) - n), n):
        mag = np.abs(np.fft.rfft(m[i:i + n] * win)) ** 2
        for j, fc in enumerate(CENTRES):
            sel = (freqs >= fc / 2 ** (1 / 6)) & (freqs < fc * 2 ** (1 / 6))
            if sel.any():
                acc[j] += mag[sel].mean()
        frames += 1
    acc /= max(1, frames)
    band = 10 * np.log10(np.maximum(acc / acc.sum(), 1e-12))

    # Slope above 500 Hz, least squares in log-frequency.
    sel = CENTRES >= 500
    oct_axis = np.log2(CENTRES[sel] / 500.0)
    slope = np.polyfit(oct_axis, band[sel], 1)[0]
    return crest, corr, band, slope


def main():
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    if not files:
        print("no .wav files in %s -- set SKYHOWL_REFS" % REFS)
        return 1
    hdr = "".join("%7d" % int(c) for c in CENTRES)
    print("%-42s%7s%7s%8s  %s" % ("reference", "crest", "L/R", "dB/oct", hdr))
    slopes, crests, corrs = [], [], []
    for f in files:
        got = measure(os.path.join(REFS, f))
        if not got:
            continue
        crest, corr, band, slope = got
        print("%-42s%7.1f%7.2f%8.1f  %s" %
              (f[:42], crest, corr, slope, "".join("%7.1f" % v for v in band)))
        slopes.append(slope)
        crests.append(crest)
        corrs.append(corr)
    print()
    print("slope  dB/oct above 500 Hz : %.1f .. %.1f, median %.1f" %
          (min(slopes), max(slopes), float(np.median(slopes))))
    print("crest  dB                  : %.1f .. %.1f, median %.1f" %
          (min(crests), max(crests), float(np.median(crests))))
    print("L/R    correlation         : %.2f .. %.2f, median %.2f" %
          (min(corrs), max(corrs), float(np.median(corrs))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
