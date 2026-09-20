"""The temporal structure of the night: gaps, phrases, and how calls sit in them.

    python3 phrases.py

Everything above one call is measured here. A call is the unit, but nothing in
the library is one call: an owl hoots twice and waits, a barred owl runs eight
notes together, a wolf holds one note for two seconds and the pack answers it
half a second later, a scops owl repeats the same pip every three seconds for
minutes. Those four behaviours are the PHRASE and PACK panels, and the numbers
below are where their defaults come from.

The one that matters most is the **gap between calls inside a phrase**. In
ChirpParade 70 % of consecutive syllable pairs have no silence between them at
all, which is why its Legato defaults to 0.70 -- a bird's phrase runs together.
Here it does not, and a Legato inherited from that plugin stretches a 181 ms
hoot to fill an 833 ms slot. The share below is what sets it.
"""
import os
import sys

import numpy as np

import calls as S
from callers import GROUPS, group_of

REFS = os.environ.get("NIGHTLIFE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
SECONDS = float(os.environ.get("NIGHTLIFE_SECONDS", "60"))

# A gap longer than this ends a phrase and starts the next one. Chosen from the
# gap histogram the census prints: every caller in the library has a clear
# bimodal gap distribution, and 2.5 s sits in the trough for all six.
PHRASE_GAP_SEC = 2.5
# Two calls closer than this are touching. The segmenter's own merge threshold
# is 45 ms, so anything below that is already one call.
TOUCHING_SEC = 0.060


def main():
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    per = {}
    for name in files:
        g = group_of(name)
        if g is None or g.startswith("("):
            continue
        r = S.analyse(os.path.join(REFS, name), SECONDS)
        if not r or len(r["syllables"]) < 2:
            continue
        sy = r["syllables"]
        gaps = np.array([sy[i + 1].t0 - sy[i].t1 for i in range(len(sy) - 1)], float)
        gaps = gaps[gaps > -0.05]
        # Onset to onset as well as gap to gap. The engine's Call Rate is
        # starts per second -- a phrase advances by the slot a call takes -- so
        # a rate read off the *gaps* would be a different quantity and would set
        # the default about four times too fast.
        onsets = np.array([sy[i + 1].t0 - sy[i].t0 for i in range(len(sy) - 1)], float)
        d = per.setdefault(g, {"gaps": [], "onsets": [], "phrase": [], "calls": [],
                               "files": 0, "n": 0, "seconds": 0.0})
        d["gaps"].extend(gaps.tolist())
        d["onsets"].extend(onsets[(onsets > 0.0) & (onsets < PHRASE_GAP_SEC + 2.0)].tolist())
        d["files"] += 1
        d["n"] += len(sy)
        d["seconds"] += r["seconds"]
        # Phrases: runs of calls separated by less than PHRASE_GAP_SEC.
        run = 1
        for gap in gaps:
            if gap < PHRASE_GAP_SEC:
                run += 1
            else:
                d["calls"].append(run)
                d["phrase"].append(gap)
                run = 1
        d["calls"].append(run)

    print("%-10s%6s%6s%9s%9s%9s%9s%9s%9s" %
          ("caller", "files", "n", "gap ms", "touching", "in-phr s", "rate Hz",
           "calls", "phr gap"))
    # rate Hz is onset to onset, which is what the engine's Call Rate means.
    for caller, _ in GROUPS:
        d = per.get(caller)
        if not d:
            continue
        gaps = np.array(d["gaps"], float)
        inside = gaps[gaps < PHRASE_GAP_SEC]
        touch = float(np.mean(gaps < TOUCHING_SEC)) if len(gaps) else 0.0
        ons = np.array(d["onsets"], float)
        print("%-10s%6d%6d%9.0f%8.0f%%%9.2f%9.2f%9.1f%9.2f" % (
            caller, d["files"], d["n"],
            1000.0 * np.median(gaps) if len(gaps) else 0.0,
            100.0 * touch,
            np.median(inside) if len(inside) else 0.0,
            1.0 / max(np.median(ons), 1e-3) if len(ons) else 0.0,
            np.median(d["calls"]), np.median(d["phrase"]) if d["phrase"] else 0.0))

    allgaps = np.array([g for d in per.values() for g in d["gaps"]], float)
    allin = allgaps[allgaps < PHRASE_GAP_SEC]
    allons = np.array([o for d in per.values() for o in d["onsets"]], float)
    allcalls = [c for d in per.values() for c in d["calls"]]
    allphr = [p for d in per.values() for p in d["phrase"]]
    print("%-10s%6s%6d%9.0f%8.0f%%%9.2f%9.2f%9.1f%9.2f" % (
        "LIBRARY", "", len(allgaps) + 1, 1000.0 * np.median(allgaps),
        100.0 * np.mean(allgaps < TOUCHING_SEC), np.median(allin),
        1.0 / max(np.median(allons), 1e-3), np.median(allcalls), np.median(allphr)))

    print("\nthe gap histogram, which is what PHRASE_GAP_SEC is read off:")
    edges = [0.0, 0.06, 0.12, 0.25, 0.5, 1.0, 1.5, 2.5, 4.0, 8.0, 1e9]
    hist, _ = np.histogram(allgaps, bins=edges)
    for i in range(len(hist)):
        lo, hi = edges[i], edges[i + 1]
        bar = "#" * int(round(60.0 * hist[i] / max(hist.max(), 1)))
        print("   %5.2f .. %5.2f s  %4d  %s" % (lo, min(hi, 99.0), hist[i], bar))

    print("\nWhat this sets:")
    print("   Legato   -- %.0f %% of consecutive pairs touch. ChirpParade measures 70 %%"
          % (100.0 * np.mean(allgaps < TOUCHING_SEC)))
    print("               and defaults its Legato to 0.70; a night is not a dawn chorus.")
    print("   Calls    -- median %.0f per phrase" % np.median(allcalls))
    print("   Call Rate-- %.2f Hz inside a phrase, onset to onset (%.2f s apart)"
          % (1.0 / max(np.median(allons), 1e-3), np.median(allons)))
    print("   Phrase Gap %.1f s between phrases" % np.median(allphr))
    return 0


if __name__ == "__main__":
    sys.exit(main())
