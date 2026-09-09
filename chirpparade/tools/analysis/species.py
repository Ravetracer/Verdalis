"""Per-species measurements, which is where the Species table in the engine comes from.

    CHIRPPARADE_REFS=/path/to/wavs python3 species.py

The reference library is named by what is in it, so it can be grouped by
species and each group measured on its own. The engine's Species enum is that
table: every entry's pitch, sweep, syllable length, harmonic richness,
roughness and syllable rate is the median of the group below, not a guess about
what a crow sounds like.

The grouping is by file name and is written out in full, so it can be checked
against the library rather than trusted.
"""
import os
import sys

import numpy as np

import syllables as S

REFS = os.environ.get("CHIRPPARADE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
SECONDS = float(os.environ.get("CHIRPPARADE_SECONDS", "45"))

# Which reference belongs to which species. Prefix match, longest first, so
# that "crow_call" is not caught by "crane". Files not matched are reported.
# Anything named fss_<species>_ came from fetch-fss.py, which encodes the
# species in the name for exactly this reason -- the site's own file names say
# "carrioncrow" and "jackdaw", and a prefix table that had to list every bird
# on a 515-species site would be the wrong place to keep that knowledge.
#
# A group is an *acoustic* class, not a taxonomic one. The engine's species
# bias pitch, length, harmonic richness, roughness and rate together, so a bird
# belongs where those five put it -- which is why a great spotted woodpecker's
# sharp 5 kHz "kik" is filed under Whistler rather than under Woodpecker, whose
# table is built from the green woodpecker's laugh and looks nothing like it.
# The assignments below were made by measuring each file and taking its
# distance to every existing group's centroid in those five quantities; the
# ones added this way sat within 1.1 of the group they joined.
GROUPS = [
    ("Whistler", ["whistling_single_robin", "single_bird_chirp",
                  "fss_whistler_",
                  # blackbird 0.5/1.0, treecreeper 0.6, nuthatch 0.8,
                  # great spotted woodpecker 0.9 -- all high and single-harmonic
                  "blackbird", "short-toed-treecreeper", "eurasian-nuthatch",
                  "great-spotted-woodpecker"]),
    ("Sparrow", ["chirps_", "multiple_bird_chirps", "multiple_birds_",
                 "chirping_birds_and_woodpecker", "fss_sparrow_",
                 "winter-wren"]),
    ("Warbler", ["nightingale", "tui", "fss_warbler_"]),
    ("Budgie", ["budgies_"]),
    ("Woodpecker", ["green_woodpecker_chirp", "red_headed_woodpecker_chirping",
                    "fss_woodpecker_"]),
    ("Crow", ["crow_call_single", "crows_alarm_call", "crows_calling",
              "multiple_crows_calling", "fss_crow_"]),
    ("Raven", ["raven", "fss_raven_"]),
    ("Goose", ["goose", "fss_goose_"]),
    ("Crane", ["crane_bird_calling", "fss_crane_", "pheasant"]),
    # Piping: fast, clean and mid-pitched, which no other group is. Its nearest
    # neighbour is Woodpecker at 1.5, far enough that folding it in would have
    # moved that table rather than joined it.
    ("Piper", ["eurasian-oystercatchers", "oystercatcher"]),
    # Drumming is sonation rather than voice and is measured by drums.py; the
    # files are listed here only so that nothing is silently left out.
    ("(drumming)", ["woodpecker_hammering", "multiple_bird_chirps_and_woodpecker"]),
]


def group_of(name):
    best = (None, -1)
    for species, prefixes in GROUPS:
        for p in prefixes:
            if name.startswith(p) and len(p) > best[1]:
                best = (species, len(p))
    return best[0]


def main():
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    if not files:
        print("no .wav files in %s -- set CHIRPPARADE_REFS" % REFS)
        return 1

    members = {}
    syls = {}
    rates = {}
    unmatched = []
    for f in files:
        g = group_of(f)
        if g is None:
            unmatched.append(f)
            continue
        members.setdefault(g, []).append(f)
        r = S.analyse(os.path.join(REFS, f), SECONDS)
        if not r or not r["syllables"]:
            continue
        syls.setdefault(g, []).extend(r["syllables"])
        rates.setdefault(g, []).append(60.0 * len(r["syllables"]) / r["seconds"])

    print("%-12s%6s%7s%8s%7s%7s%7s%7s%7s%6s%7s%8s%8s%9s" %
          ("species", "files", "n", "f0 Hz", "sweep", "len ms", "rise", "fall",
           "skew", "nh", "rough", "syl/min", "turns", "shape"))
    for species, _ in GROUPS:
        sy = syls.get(species)
        if not sy:
            continue
        shapes = {}
        for s in sy:
            shapes[s.shape] = shapes.get(s.shape, 0) + 1
        rise = np.median([s.rise for s in sy])
        fall = np.median([s.fall for s in sy])
        top = max(shapes.items(), key=lambda kv: kv[1])
        print("%-12s%6d%7d%8.0f%7.2f%7.0f%7.0f%7.0f%7.2f%6d%7.0f%8.0f%8.2f%9s" % (
            species, len(members.get(species, [])), len(sy),
            np.median([s.f0_med for s in sy]),
            np.median([s.sweep_oct for s in sy]),
            1000.0 * np.median([s.dur for s in sy]),
            1000.0 * rise, 1000.0 * fall, rise / max(rise + fall, 1e-9),
            int(np.median([s.harmonics for s in sy])),
            10.0 * np.log10(np.median([s.flatness for s in sy])),
            np.median(rates.get(species, [0])),
            np.median([s.turns for s in sy]),
            "%s %.0f%%" % (top[0], 100.0 * top[1] / len(sy))))

    print()
    for species, _ in GROUPS:
        if species in members:
            print("%-12s %s" % (species, ", ".join(members[species])))
    if unmatched:
        print()
        print("NOT GROUPED: %s" % ", ".join(unmatched))
    return 0


if __name__ == "__main__":
    sys.exit(main())
