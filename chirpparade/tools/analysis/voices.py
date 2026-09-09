"""The voice: how many harmonics, how rough, and where the tract resonance sits.

    CHIRPPARADE_REFS=/path/to/wavs python3 voices.py

`chirps.py` measures what a syllable *does*; this measures what it is *made of*,
which is what the plugin's voice model has to reproduce.

Three things come out of it.

  the families   Sorting every syllable by harmonic count and roughness gives
                 three groups with no overlap worth speaking of: whistles,
                 harmonic stacks, and rough calls. They are the plugin's three
                 voice types, and they are found rather than chosen.

  the tract      In a harmonic stack the loudest harmonic is usually not the
                 first. Which one it is says where the resonance above the
                 syringeal source sits -- the trachea and the open beak -- and
                 that is a filter in the engine with a measured centre.

  the background For each reference, how far the birds stand above the bed the
                 recordist could not avoid. This is the number that justifies
                 the noise subtraction in `syllables.py`: at a median 19 dB, a
                 measurement that ignores the bed is measuring the bed.
"""
import os
import sys

import numpy as np

import syllables as S

REFS = os.environ.get("CHIRPPARADE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
SECONDS = float(os.environ.get("CHIRPPARADE_SECONDS", "60"))


def spread(v, fmt="%.2f"):
    v = np.asarray([x for x in v], float)
    if not len(v):
        return "-"
    return (fmt + " .. " + fmt + ", median " + fmt) % (
        np.percentile(v, 5), np.percentile(v, 95), np.median(v))


def formant(spec, freqs, f0, nharm=12):
    """Which harmonic of f0 is the loudest, and at what frequency."""
    if f0 <= 0.0:
        return None
    best = (0, 0.0, -1.0)
    for k in range(1, nharm + 1):
        f = f0 * k
        if f > freqs[-1]:
            break
        sel = (freqs > f * 0.94) & (freqs < f * 1.06)
        if not sel.any():
            continue
        v = float(spec[sel].max())
        if v > best[2]:
            best = (k, f, v)
    return best if best[0] else None


def background(path):
    """The bed under the birds, and how far the birds stand above it."""
    m, sr, x = S.load_mono(path, SECONDS)
    mag, freqs, _ = S.stft(m, sr)
    if mag is None:
        return None
    sel = (freqs >= S.BAND_LO) & (freqs <= S.BAND_HI)
    band = mag[:, sel]
    bfreqs = freqs[sel]
    power = (band ** 2).sum(axis=1)
    level = 10.0 * np.log10(np.maximum(power, 1e-20))
    quiet = float(np.percentile(level, 10))
    loud = float(np.percentile(level, 99))
    # Slope of the background itself, over octave bands, from the quiet frames.
    q = band[level <= np.percentile(level, 20)]
    if len(q) < 4:
        slope = 0.0
    else:
        avg = (q ** 2).mean(axis=0)
        centres = np.array([500.0, 1000.0, 2000.0, 4000.0, 8000.0])
        vals = []
        for fc in centres:
            s = (bfreqs >= fc / np.sqrt(2)) & (bfreqs < fc * np.sqrt(2))
            vals.append(10.0 * np.log10(max(avg[s].mean(), 1e-20)) if s.any() else np.nan)
        vals = np.array(vals)
        ok = ~np.isnan(vals)
        slope = float(np.polyfit(np.log2(centres[ok] / 500.0), vals[ok], 1)[0]) \
            if ok.sum() >= 3 else 0.0
    corr = 1.0
    if x.shape[1] >= 2:
        c = np.corrcoef(x[:, 0], x[:, 1])
        if np.isfinite(c[0, 1]):
            corr = float(c[0, 1])
    return loud - quiet, slope, corr


def main():
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    if not files:
        print("no .wav files in %s -- set CHIRPPARADE_REFS" % REFS)
        return 1

    allsyl = []
    snrs, slopes, corrs = [], [], []
    print("%-42s%8s%9s%7s%8s%8s" % ("reference", "SNR dB", "bed dB/o", "L/R",
                                    "f0 lo", "f0 hi"))
    for f in files:
        path = os.path.join(REFS, f)
        bg = background(path)
        r = S.analyse(path, SECONDS)
        sy = r["syllables"] if r else []
        allsyl.extend(sy)
        if bg:
            snrs.append(bg[0])
            slopes.append(bg[1])
            corrs.append(bg[2])
        f0s = [s.f0_med for s in sy]
        print("%-42s%8.0f%9.1f%7.2f%8.0f%8.0f" % (
            f[:42], bg[0] if bg else 0.0, bg[1] if bg else 0.0, bg[2] if bg else 1.0,
            np.percentile(f0s, 5) if f0s else 0.0,
            np.percentile(f0s, 95) if f0s else 0.0))

    print()
    print("birds above the bed    dB : %s" % spread(snrs, "%.0f"))
    print("the bed's own slope dB/oct: %s" % spread(slopes, "%.1f"))
    print("L/R correlation           : %s" % spread(corrs))

    if not allsyl:
        return 1

    # ---------------------------------------------------------- the families
    whistle = [s for s in allsyl if s.harmonics <= 1]
    stack = [s for s in allsyl if 2 <= s.harmonics <= 5]
    rich = [s for s in allsyl if s.harmonics >= 6]
    print()
    print("%-24s%7s%9s%10s%9s%9s" %
          ("family", "n", "share", "f0 Hz", "rough dB", "dur ms"))
    for name, group in (("whistle, <=1 harmonic", whistle),
                        ("stack, 2-5 harmonics", stack),
                        ("rich, >=6 harmonics", rich)):
        if not group:
            continue
        print("%-24s%7d%8.0f %%%10.0f%9.0f%9.0f" % (
            name, len(group), 100.0 * len(group) / len(allsyl),
            np.median([s.f0_med for s in group]),
            10.0 * np.log10(np.median([s.flatness for s in group])),
            1000.0 * np.median([s.dur for s in group])))

    print()
    print("whistle f0             Hz : %s" % spread([s.f0_med for s in whistle], "%.0f"))
    if rich:
        print("rich-voice f0          Hz : %s" % spread([s.f0_med for s in rich], "%.0f"))
        print("rich-voice sweep      oct : %s" % spread([s.sweep_oct for s in rich]))

    # ------------------------------------------------------------ the tract
    # Measured only where there is a harmonic series to measure it on.
    print()
    ks, fs = [], []
    for f in files:
        path = os.path.join(REFS, f)
        m, sr, _ = S.load_mono(path, SECONDS)
        mag, freqs, times = S.stft(m, sr)
        if mag is None:
            continue
        dn, _ = S.denoise(mag)
        sel = (freqs >= S.BAND_LO) & (freqs <= S.BAND_HI)
        band, bfreqs = dn[:, sel], freqs[sel]
        r = S.analyse(path, SECONDS)
        for s in (r["syllables"] if r else []):
            if s.harmonics < 3:
                continue
            frame = int(np.clip((0.5 * (s.t0 + s.t1)) * sr / S.HOP, 0, band.shape[0] - 1))
            got = formant(band[frame], bfreqs, s.f0_med)
            if got and got[0] >= 1:
                ks.append(got[0])
                fs.append(got[1])
    if ks:
        print("loudest harmonic, harmonic stacks: %s" % spread(ks, "%.0f"))
        print("its frequency                 Hz : %s" % spread(fs, "%.0f"))
        print("share where it is not the first  : %.0f %%" %
              (100.0 * sum(1 for k in ks if k > 1) / len(ks)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
