"""Per-caller measurements, which is where the Caller table in the engine comes from.

    NIGHTLIFE_REFS=/path/to/wavs python3 callers.py

The reference library is named by what is in it, so it can be grouped by caller
and each group measured on its own. The engine's Caller enum is that table:
every entry's pitch, call length, harmonic richness, roughness and call rate is
the median of the group below, not a guess about what a wolf sounds like.

The grouping is by file name and is written out in full, so it can be checked
against the library rather than trusted. As in ChirpParade a group is an
*acoustic* class and not a taxonomic one -- the engine biases pitch, length,
harmonic richness, roughness and rate together, so a recording belongs where
those five put it. That is why the two screech owls are not filed with the
other owls: a screech owl's descending whinny shares no column with a tawny
owl's hoot.
"""
import os
import sys

import numpy as np

import calls as S

REFS = os.environ.get("NIGHTLIFE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
SECONDS = float(os.environ.get("NIGHTLIFE_SECONDS", "45"))

# Which reference belongs to which caller. Prefix match, longest first.
GROUPS = [
    # The long howl: one pitch held, with a slow vibrato and a fall at the end.
    # Dogs and the "creature vocalization" timber wolf are in here because they
    # measure as howls -- see the census this prints.
    ("Wolf", ["howling-wolf", "howling-wolves", "wolf-howl", "wolves-howling",
              "howling-in-the-distance",
              "canine-howl", "dog-howling"]),
    # The hoot: low, near-sinusoidal, one or two harmonics, in phrases.
    ("Owl", ["barred-owl", "owl-hoot", "owl-hooting", "owl_hooting", "owl-sound-effect",
             "owl-in-calm", "owl-in-the-forest", "tawny-owl"]),
    # The whinny: a fast descending trill, which no hoot resembles.
    ("Screech", ["screech_owl"]),
    # The pip: one short pure tone, repeated on a metronome for minutes.
    ("Scops", ["scops-owl"]),
    # The scream: harsh, harmonic-rich, an octave above the howl.
    ("Fox", ["fox-calling", "fox-mating-call", "fox-scream", "red-fox-screeching"]),
    # The wail: a loon is the one *bird* in the library, and it is here because
    # a night soundscape without one is missing its most recognisable voice.
    ("Loon", ["loon-call"]),
    # The chorus. Measured here for the census only: a croak is a pulse train
    # through a body resonance rather than a pitch contour, so the chorus layer
    # is built by frogs.py and takes nothing from this table.
    ("(frogs)", ["frog", "croaking-frogs", "many-croaking", "multiple-frog",
                 "leopard-frogs"]),
    # Performed rather than recorded in the field. Measured and reported so that
    # nothing is silently left out, and deliberately given to no caller: the
    # suite fits to the world, and a werewolf is not in it.
    ("(performed)", ["female-werewolf", "female-werewolves"]),
]


def group_of(name):
    best = (None, -1)
    for caller, prefixes in GROUPS:
        for p in prefixes:
            if name.startswith(p) and len(p) > best[1]:
                best = (caller, len(p))
    return best[0]


def main():
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    if not files:
        print("no .wav files in %s -- set NIGHTLIFE_REFS" % REFS)
        return 1

    members, syls, rates = {}, {}, {}
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

    print("%-12s%6s%6s%8s%7s%8s%7s%7s%6s%7s%8s%8s%9s" %
          ("caller", "files", "n", "f0 Hz", "sweep", "len ms", "rise", "fall",
           "nh", "rough", "call/min", "turns", "shape"))
    for caller, _ in GROUPS:
        sy = syls.get(caller)
        if not sy:
            continue
        shapes = {}
        for s in sy:
            shapes[s.shape] = shapes.get(s.shape, 0) + 1
        top = max(shapes.items(), key=lambda kv: kv[1])
        print("%-12s%6d%6d%8.0f%7.2f%8.0f%7.0f%7.0f%6d%7.0f%8.0f%8.2f%9s" % (
            caller, len(members.get(caller, [])), len(sy),
            np.median([s.f0_med for s in sy]),
            np.median([s.sweep_oct for s in sy]),
            1000.0 * np.median([s.dur for s in sy]),
            1000.0 * np.median([s.rise for s in sy]),
            1000.0 * np.median([s.fall for s in sy]),
            int(np.median([s.harmonics for s in sy])),
            10.0 * np.log10(np.median([s.flatness for s in sy])),
            np.median(rates.get(caller, [0])),
            np.median([s.turns for s in sy]),
            "%s %.0f%%" % (top[0], 100.0 * top[1] / len(sy))))

    pooled = [s for c, _ in GROUPS if c in ("Wolf", "Owl", "Screech", "Scops", "Fox", "Loon")
              for s in syls.get(c, [])]
    if pooled:
        print("%-12s%6s%6d%8.0f%7.2f%8.0f%7.0f%7.0f%6d%7.0f%8s%8.2f" % (
            "LIBRARY", "", len(pooled), np.median([s.f0_med for s in pooled]),
            np.median([s.sweep_oct for s in pooled]),
            1000.0 * np.median([s.dur for s in pooled]),
            1000.0 * np.median([s.rise for s in pooled]),
            1000.0 * np.median([s.fall for s in pooled]),
            int(np.median([s.harmonics for s in pooled])),
            10.0 * np.log10(np.median([s.flatness for s in pooled])),
            "%.0f" % np.median([r for c, _ in GROUPS if c in ("Wolf", "Owl", "Screech",
                                                              "Scops", "Fox", "Loon")
                                for r in rates.get(c, [])]),
            np.median([s.turns for s in pooled])))
        print("   (the six callers pooled -- what the engine's library-wide medians are)")

    print("\nthe vibrato and the tremolo, which is what a howl is made of:")
    print("%-12s%10s%10s%10s%10s" % ("caller", "fm Hz", "fm cents", "am Hz", "am depth"))
    for caller, _ in GROUPS:
        sy = syls.get(caller)
        if not sy:
            continue
        fm = [s.fm_rate for s in sy if s.fm_rate > 0.0]
        am = [s.am_rate for s in sy if s.am_rate > 0.0]
        print("%-12s%10s%10.0f%10s%10.2f" % (
            caller, "%.1f" % np.median(fm) if fm else "-",
            1200.0 * np.median([s.fm_depth_oct for s in sy]),
            "%.1f" % np.median(am) if am else "-",
            np.median([s.am_depth for s in sy])))

    print()
    for caller, _ in GROUPS:
        if caller in members:
            print("%-12s %s" % (caller, ", ".join(members[caller])))
    if unmatched:
        print("\nNOT GROUPED: %s" % ", ".join(unmatched))
    return 0


if __name__ == "__main__":
    sys.exit(main())
