"""The syllable census: what one bird syllable is, measured over the library.

    CHIRPPARADE_REFS=/path/to/wavs python3 chirps.py [--per-file]

Every number ChirpParade's syllable model defaults to comes from here: how long
a syllable lasts, where it sits, how far and how fast it sweeps, what shape the
sweep is, whether it trills, how many harmonics it has and how rough it is.

The shape histogram is the interesting one. The Mindlin model
(Phys. Rev. E 72, 051926) makes a syllable's contour a consequence of a single
number -- the phase between the pressure gesture and the tension gesture -- so
the six shapes below are not six oscillators. They are six settings of one
knob, and the histogram says which settings have to be reachable.
"""
import os
import sys

import numpy as np

import syllables as S

REFS = os.environ.get("CHIRPPARADE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
SECONDS = float(os.environ.get("CHIRPPARADE_SECONDS", "60"))

SHAPES = ["flat", "up", "down", "arch", "valley", "wobble", "click"]


def roughness_db(flatness):
    """Spectral flatness as a level. Raw it spans 0.0004 to 0.08, which reads
    as nothing; in dB it spans -34 to -11 and separates the families."""
    return 10.0 * np.log10(max(flatness, 1e-9))


def spread(v, fmt="%.2f"):
    v = np.asarray([x for x in v], float)
    if not len(v):
        return "-"
    return (fmt + " .. " + fmt + ", median " + fmt) % (
        np.percentile(v, 5), np.percentile(v, 95), np.median(v))


def main():
    per_file = "--per-file" in sys.argv
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    if not files:
        print("no .wav files in %s -- set CHIRPPARADE_REFS" % REFS)
        return 1

    allsyl = []
    rows = []
    for f in files:
        r = S.analyse(os.path.join(REFS, f), SECONDS)
        rows.append((f, r))
        if r:
            allsyl.extend(r["syllables"])

    if per_file:
        print("%-42s%5s%8s%8s%7s%7s%7s%5s%7s%7s%7s" %
              ("reference", "n", "dur ms", "f0 Hz", "sweep", "oct/s", "trill", "nh",
               "rough", "rise", "fall"))
        for f, r in rows:
            sy = r["syllables"] if r else []
            if not sy:
                print("%-42s%5d   (nothing voiced found)" % (f[:42], 0))
                continue
            trills = [s.fm_rate for s in sy if s.fm_rate > 0]
            print("%-42s%5d%8.0f%8.0f%7.2f%7.1f%7.1f%5d%7.0f%7.0f%7.0f" % (
                f[:42], len(sy),
                np.median([s.dur * 1000 for s in sy]),
                np.median([s.f0_med for s in sy]),
                np.median([s.sweep_oct for s in sy]),
                np.median([s.sweep_rate for s in sy]),
                np.median(trills) if trills else 0.0,
                int(np.median([s.harmonics for s in sy])),
                roughness_db(np.median([s.flatness for s in sy])),
                np.median([s.rise * 1000 for s in sy]),
                np.median([s.fall * 1000 for s in sy])))
        print()

    print("%d syllables over %d references" % (len(allsyl), len(files)))
    if not allsyl:
        return 1
    print()
    print("syllable duration      ms : %s" % spread([s.dur * 1000 for s in allsyl], "%.0f"))
    print("fundamental            Hz : %s" % spread([s.f0_med for s in allsyl], "%.0f"))
    print("dominant partial       Hz : %s" % spread([s.f_med for s in allsyl], "%.0f"))
    print("pitch at the peak      Hz : %s" % spread([s.f_peak for s in allsyl], "%.0f"))
    print("sweep extent          oct : %s" % spread([s.sweep_oct for s in allsyl]))
    print("contour turns             : %s" % spread([s.turns for s in allsyl]))
    print("sweep rate          oct/s : %s" % spread([s.sweep_rate for s in allsyl], "%.1f"))
    print("net sweep, start->end oct : %s" %
          spread([np.log2(max(s.f_end, 1) / max(s.f_start, 1)) for s in allsyl]))
    print("rise time              ms : %s" % spread([s.rise * 1000 for s in allsyl], "%.0f"))
    print("fall time              ms : %s" % spread([s.fall * 1000 for s in allsyl], "%.0f"))
    print("harmonics above -24 dB    : %s" % spread([s.harmonics for s in allsyl], "%.0f"))
    print("roughness              dB : %s" %
          spread([roughness_db(s.flatness) for s in allsyl], "%.0f"))
    print("in-band tonality       dB : %s" % spread([s.hnr for s in allsyl], "%.0f"))

    trills = [s for s in allsyl if s.fm_rate > 0]
    print()
    print("syllables that trill      : %d of %d (%.0f %%)" %
          (len(trills), len(allsyl), 100.0 * len(trills) / len(allsyl)))
    print("trill rate             Hz : %s" % spread([s.fm_rate for s in trills], "%.1f"))
    print("trill depth             oct: %s" % spread([s.fm_depth_oct for s in trills]))
    ams = [s for s in allsyl if s.am_rate > 0]
    print("syllables that pulse      : %d (%.0f %%)" %
          (len(ams), 100.0 * len(ams) / len(allsyl)))
    print("pulse rate             Hz : %s" % spread([s.am_rate for s in ams], "%.1f"))
    print("pulse depth                : %s" % spread([s.am_depth for s in ams]))

    print()
    print("contour shape             :")
    for sh in SHAPES:
        k = sum(1 for s in allsyl if s.shape == sh)
        if k:
            print("   %-8s %5d  %4.1f %%" % (sh, k, 100.0 * k / len(allsyl)))

    # The two families, split on the harmonic count alone.
    whistle = [s for s in allsyl if s.harmonics <= 1]
    stack = [s for s in allsyl if s.harmonics >= 4]
    print()
    print("whistles (<=1 harmonic)   : %d (%.0f %%), f0 %s" %
          (len(whistle), 100.0 * len(whistle) / len(allsyl),
           spread([s.f0_med for s in whistle], "%.0f")))
    print("stacks   (>=4 harmonics)  : %d (%.0f %%), f0 %s" %
          (len(stack), 100.0 * len(stack) / len(allsyl),
           spread([s.f0_med for s in stack], "%.0f")))
    if whistle and stack:
        print("roughness, whistles   dB  : median %.0f" %
              np.median([roughness_db(s.flatness) for s in whistle]))
        print("roughness, stacks     dB  : median %.0f" %
              np.median([roughness_db(s.flatness) for s in stack]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
