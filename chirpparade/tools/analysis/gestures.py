"""The one prediction the references can falsify.

    CHIRPPARADE_REFS=/path/to/wavs python3 gestures.py

ChirpParade's voice is the Gardner-Laje-Mindlin syringeal oscillator

    x'  = y
    y'  = -eps*x - C*x^2*y + B*y

driven by two gestures: B, the air sac pressure, which switches the oscillation
on and sets its amplitude, and eps, the tension of the syringeal muscle, which
sets its frequency. Zysman et al. (Phys. Rev. E 72, 051926) establish that the
pressure is proportional to the sound envelope and the tension to the pitch,
and Gardner et al. (Phys. Rev. Lett. 87, 208101) that syllables "of quite
diverse acoustic nature" follow from nothing more than the *phase* between the
two.

That is a strong claim, and it is testable. If a syllable really is one turn of
two coupled gestures, then its envelope and its pitch contour are two sinusoids
of the same period with a phase between them, and the shape a sonogram reader
names follows from that phase alone:

    in phase          the pitch rises and falls with the level  -> arch
    quarter out       the pitch sweeps through the loud part    -> up or down
    anti-phase        the pitch dips where the level peaks      -> valley

So the prediction is: **the correlation between a syllable's envelope and its
pitch contour must be strongly positive for arches, strongly negative for
valleys, and near zero for sweeps.** If the shapes came from independent
mechanisms there would be no such ordering.

This script measures that correlation for every syllable in the library and
groups it by the shape the contour was named, plus the lag at which the two
series line up best, which is the phase itself.
"""
import os
import sys

import numpy as np

import syllables as S

REFS = os.environ.get("CHIRPPARADE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
SECONDS = float(os.environ.get("CHIRPPARADE_SECONDS", "60"))
MIN_FRAMES = 8


def best_lag(a, b, maxlag):
    """The lag at which b lines up best with a, and the correlation there."""
    best = (0, 0.0)
    for lag in range(-maxlag, maxlag + 1):
        if lag < 0:
            u, v = a[-lag:], b[:len(b) + lag]
        elif lag > 0:
            u, v = a[:len(a) - lag], b[lag:]
        else:
            u, v = a, b
        if len(u) < 4:
            continue
        su, sv = u.std(), v.std()
        if su < 1e-9 or sv < 1e-9:
            continue
        r = float(((u - u.mean()) * (v - v.mean())).mean() / (su * sv))
        if abs(r) > abs(best[1]):
            best = (lag, r)
    return best


def main():
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    if not files:
        print("no .wav files in %s -- set CHIRPPARADE_REFS" % REFS)
        return 1

    byshape = {}
    allr, alllag = [], []
    for f in files:
        path = os.path.join(REFS, f)
        m, sr, _ = S.load_mono(path, SECONDS)
        mag, freqs, times = S.stft(m, sr)
        if mag is None:
            continue
        dn, _ = S.denoise(mag)
        level, peakiness, fpeak, centroid, total = S.frame_features(dn, freqs)
        mask = S.voiced_mask(level, peakiness)
        frame_sec = S.HOP / float(sr)
        sel = (freqs >= S.BAND_LO) & (freqs <= S.BAND_HI)
        band, bfreqs = dn[:, sel], freqs[sel]

        segs = []
        for a, b in S.runs(mask, sr):
            segs.extend(S.split_run(a, b, total, frame_sec))
        for a, b in segs:
            if b - a + 1 < MIN_FRAMES:
                continue
            p = S.track_partial(band, bfreqs, a, b)
            amp = np.sqrt(np.maximum(total[a:b + 1], 0.0))
            lg = np.log2(np.maximum(p, 1.0))
            if amp.std() < 1e-12 or lg.std() < 1e-12:
                continue
            r = float(np.corrcoef(amp, lg)[0, 1])
            if not np.isfinite(r):
                continue
            lag, rlag = best_lag(amp, lg, max(1, (b - a + 1) // 3))
            shape = S.classify(p)
            byshape.setdefault(shape, []).append((r, lag / float(b - a + 1), rlag))
            allr.append(r)
            alllag.append(lag / float(b - a + 1))

    print("%d syllables of at least %d frames" % (len(allr), MIN_FRAMES))
    print()
    print("%-10s%7s%12s%12s%12s%12s" %
          ("shape", "n", "r env/f0", "r > +0.5", "r < -0.5", "best lag"))
    order = ["arch", "up", "down", "flat", "wobble", "valley"]
    for sh in order:
        v = byshape.get(sh)
        if not v:
            continue
        rs = np.array([x[0] for x in v])
        lags = np.array([x[1] for x in v])
        print("%-10s%7d%12.2f%11.0f %%%11.0f %%%12.2f" % (
            sh, len(rs), np.median(rs),
            100.0 * np.mean(rs > 0.5), 100.0 * np.mean(rs < -0.5),
            np.median(lags)))
    print()
    print("whole library, r env vs pitch : median %.2f, mean %.2f" %
          (np.median(allr), np.mean(allr)))

    arch = np.array([x[0] for x in byshape.get("arch", [])])
    valley = np.array([x[0] for x in byshape.get("valley", [])])
    sweeps = np.array([x[0] for x in byshape.get("up", []) + byshape.get("down", [])])
    if len(arch) and len(valley) and len(sweeps):
        print()
        print("the prediction:")
        print("   arches   median r %+.2f  (positive as predicted: %s)" %
              (np.median(arch), "yes" if np.median(arch) > 0.2 else "NO"))
        print("   valleys  median r %+.2f  (negative as predicted: %s)" %
              (np.median(valley), "yes" if np.median(valley) < -0.2 else "NO"))
        print("   sweeps   median r %+.2f  (near zero as predicted: %s)" %
              (np.median(sweeps), "yes" if abs(np.median(sweeps)) < 0.2 else "NO"))
        print("   arch - valley separation: %.2f" %
              (np.median(arch) - np.median(valley)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
