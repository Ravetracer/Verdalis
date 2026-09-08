"""Onset rate and pitch of the rustling in a wind recording.

    SKYHOWL_REFS=/path/to/wavs python3 rustle.py

Leaves do not hiss, they click: each one is a separate collision, and a rustle
is a Poisson stream of them. What can be measured is how fast that stream runs
and where in the spectrum the clicks sit -- which is a leaf size, since a leaf
radiates around the frequency its own dimension gives it.

Detection is by spectral flux in the 1.5-12 kHz band, which is where a leaf
click lives and where the airflow bed has already fallen away. Reported per
recording:

  onsets/s      how many clicks a second stand out of the bed
  centroid      the median spectral centroid of a click, in Hz
  10-90%        the spread of those centroids
  flux CV       variation of the flux itself: high means clatter (discrete,
                dry leaves), low means hiss (merged, conifer needles)
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np
import wavio

REFS = os.environ.get("SKYHOWL_REFS", os.path.expanduser("~/refs"))
N = 1024
HOP = 256
LO, HI = 1500.0, 12000.0


def measure(path):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    if len(m) < N * 8:
        return None
    win = np.hanning(N)
    freqs = np.fft.rfftfreq(N, 1.0 / sr)
    sel = (freqs >= LO) & (freqs <= HI)
    frames = np.array([np.abs(np.fft.rfft(m[i:i + N] * win))
                       for i in range(0, len(m) - N, HOP)])
    band = frames[:, sel]
    # Half-wave rectified spectral flux: only energy appearing counts.
    flux = np.maximum(np.diff(band, axis=0), 0.0).sum(axis=1)
    if flux.std() <= 0:
        return None
    cv = float(flux.std() / max(1e-20, flux.mean()))

    # A click is a local maximum three standard deviations above the median
    # flux, with a 20 ms refractory window so one click is not counted twice.
    thresh = np.median(flux) + 3.0 * np.std(flux)
    refractory = max(1, int(0.020 * sr / HOP))
    peaks = []
    i = 1
    while i < len(flux) - 1:
        if flux[i] > thresh and flux[i] >= flux[i - 1] and flux[i] >= flux[i + 1]:
            peaks.append(i)
            i += refractory
        else:
            i += 1
    seconds = (len(m) - N) / sr
    rate = len(peaks) / max(1e-6, seconds)

    if len(peaks) < 4:
        return rate, 0.0, 0.0, 0.0, cv
    f = freqs[sel]
    cents = []
    for p in peaks:
        w = band[p + 1] ** 2
        s = w.sum()
        if s > 0:
            cents.append(float((w * f).sum() / s))
    cents = np.array(cents)
    return rate, float(np.median(cents)), float(np.percentile(cents, 10)), \
        float(np.percentile(cents, 90)), cv


def main():
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    if not files:
        print("no .wav files in %s -- set SKYHOWL_REFS" % REFS)
        return 1
    print("%-42s%10s%10s%9s%9s%9s" %
          ("reference", "onsets/s", "centroid", "10%", "90%", "flux CV"))
    rows = []
    for f in files:
        got = measure(os.path.join(REFS, f))
        if not got:
            continue
        print("%-42s%10.1f%10.0f%9.0f%9.0f%9.2f" % ((f[:42],) + got))
        rows.append(got)
    a = np.array(rows)
    print()
    for i, nm in enumerate(["onsets/s", "centroid Hz", "10% Hz", "90% Hz", "flux CV"]):
        print("%-12s: %8.1f .. %8.1f, median %8.1f" %
              (nm, a[:, i].min(), a[:, i].max(), np.median(a[:, i])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
