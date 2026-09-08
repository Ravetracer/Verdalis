"""Gust statistics of the wind references: how the flow varies, not what it sounds like.

    SKYHOWL_REFS=/path/to/wavs python3 gusts.py

Wind is not a level, it is a process. What this measures is that process, from
the 50 ms RMS envelope of each recording:

  turbulence intensity   The standard deviation of the wind speed over its mean,
                         which is what meteorology calls I = sigma_u / U. It is
                         not measured directly here: aerodynamic sound power
                         goes as U^6 (Curle's dipole for flow over a rigid
                         surface), so amplitude goes as U^3 and the envelope's
                         coefficient of variation is about 3 I for small
                         fluctuations. I is therefore CV / 3.

  gust factor           U_max / U_mean, the number a weather station reports.
                        Taken as (p95 / median of the envelope) ^ (1/3), by the
                        same cube law.

  gust rate             The peak of the envelope's own spectrum, in gusts per
                        minute. This is the integral length scale seen from the
                        other end: a gust arrives every L / U seconds.

  rise / fall           Mean upward against mean downward slope of the envelope.
                        A gust arrives faster than it leaves, and by how much is
                        what Gust Shape sets.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np
import wavio

REFS = os.environ.get("SKYHOWL_REFS", os.path.expanduser("~/refs"))
FRAME = 0.05  # seconds


def envelope(m, sr):
    hop = int(sr * FRAME)
    k = len(m) // hop
    if k < 8:
        return None, 0.0
    e = np.sqrt(np.mean(m[:k * hop].reshape(k, hop) ** 2, axis=1) + 1e-20)
    return e, 1.0 / FRAME


def measure(path):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    e, erate = envelope(m, sr)
    if e is None:
        return None

    cv = e.std() / max(1e-12, e.mean())
    turb = cv / 3.0
    gust_amp = np.percentile(e, 95) / max(1e-12, np.median(e))
    gust_speed = gust_amp ** (1.0 / 3.0)

    # The envelope's own spectrum. Detrended and windowed, then the peak below
    # 2 Hz -- above that it is texture, not gusting.
    d = e - e.mean()
    n = 1 << int(np.ceil(np.log2(len(d))))
    spec = np.abs(np.fft.rfft(d * np.hanning(len(d)), n)) ** 2
    freqs = np.fft.rfftfreq(n, 1.0 / erate)
    band = (freqs > 0.008) & (freqs < 2.0)
    if band.any():
        peak_hz = float(freqs[band][np.argmax(spec[band])])
    else:
        peak_hz = 0.0
    gusts_per_min = peak_hz * 60.0

    # Rise against fall. Only slopes above a tenth of the envelope's own
    # standard deviation count, so that the ripple between gusts does not
    # dominate the average.
    dif = np.diff(e)
    thresh = 0.1 * e.std()
    up = dif[dif > thresh]
    dn = -dif[dif < -thresh]
    asym = float(up.mean() / dn.mean()) if len(up) and len(dn) else 1.0

    # How much of the total variance sits below 0.1 Hz: the squall scale, sets
    # of gusts rather than gusts.
    slow = float(spec[(freqs > 0.008) & (freqs < 0.1)].sum() / max(1e-20, spec[band].sum()))
    return turb, gust_speed, gusts_per_min, asym, slow


def main():
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    if not files:
        print("no .wav files in %s -- set SKYHOWL_REFS" % REFS)
        return 1
    print("%-42s%7s%8s%9s%8s%8s" %
          ("reference", "I", "gustF", "gusts/min", "rise/fall", "slow"))
    rows = []
    for f in files:
        got = measure(os.path.join(REFS, f))
        if not got:
            continue
        print("%-42s%7.2f%8.2f%9.1f%8.2f%8.2f" % ((f[:42],) + got))
        rows.append(got)
    a = np.array(rows)
    names = ["turbulence intensity I", "gust factor", "gusts per minute",
             "rise/fall ratio", "share below 0.1 Hz"]
    print()
    for i, nm in enumerate(names):
        print("%-24s: %6.2f .. %6.2f, median %6.2f" %
              (nm, a[:, i].min(), a[:, i].max(), np.median(a[:, i])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
