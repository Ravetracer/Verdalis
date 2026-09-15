"""Measures a rendered WAV the way refs.py measures a reference, so a preset can
be held against the family it was fitted to.

    python3 verify.py rendered.wav [rendered2.wav ...]

Prints the same row refs.py prints, plus the envelope and centroid contours
shape.py prints. Nothing here is new; it is the survey applied to one file so
that "does this preset actually measure like a transition" has an answer.
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np
import refs, shape

def main(argv):
    print("%-34s %6s %5s %5s %6s %6s %7s %7s %6s %6s %5s %5s" %
          ("file", "span", "peak", "t50", "rise", "fall", "c0", "c1",
           "sweep", "sub100", "crest", "corr"))
    for p in argv[1:]:
        s = refs.survey(p)
        if s is None:
            print("%-34s  (too short to measure)" % os.path.basename(p)[:34])
            continue
        print("%-34s %6.2f %5.2f %5.2f %6.3f %6.3f %7.0f %7.0f %6.2f %6.2f %5.1f %5.2f" %
              (os.path.basename(p)[:34], s["span"], s["peak_pos"], s["t50"], s["rise"],
               s["fall"], s["c_start"], s["c_end"], s["sweep_oct"], s["sub100"],
               s["crest"], s["corr"]))
        print("   bands  %s" % " ".join("%6.1f" % v for v in s["bands"]))
        c = shape.contours(p)
        if c:
            env, cen, pit, ok = c
            print("   env    %s" % " ".join("%6.3f" % v for v in env))
            if cen[0] > 20:
                print("   cent   %s" % " ".join("%6.2f" % v
                                                for v in np.log2(np.maximum(cen, 20.0) / cen[0])))

if __name__ == "__main__":
    main(sys.argv)
