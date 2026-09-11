"""What fire colours the library actually contains.

    python3 shapes.py [--k 4]

Clusters the usable references by the octave-band colour of their *bed* --
125 Hz-16 kHz, crackles gated out by bed.py, normalised to the bed's own total
-- with k-means, and prints the centroids. Those centroids are what the `Fire`
shapes ship as.

Gating first is not optional. The library's median crest factor is 31.7 dB, so
the ungated spectrum of a fire is substantially the spectrum of its crackles,
and clustering on it sorts the references by how hard the recordist was sitting
on top of the fire rather than by what kind of fire it was.

Two exclusions, both measured rather than chosen:

  * everything below 60 Hz, because `lowend.py` finds its envelope uncorrelated
    with the fire above it in all 25 references (|r| < 0.2 in 24 of them). It is
    the room and the microphone.
  * eight references whose energy above 1 kHz is a flat dither floor 40 dB down
    -- `ambiance_campfire_loop_stereo`, `big-fire-loop`, `sauna-fireplace-loop`,
    `fireplace_01`, `fireplace_02`, `fireplace_08`, `fire-crackle-and-flames`
    and `fireplace_03`. They carry a rumble, not a fire, and averaging them in
    drags every centroid towards a low-pass that no listener would call a fire.
    `--all` includes them anyway, to show what they do.
"""
import sys, os, math, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import refs
import bed

RUMBLE_ONLY = {
    "ambiance_campfire_loop_stereo.wav", "big-fire-loop.wav",
    "sauna-fireplace-loop.wav", "fireplace_01.wav", "fireplace_02.wav",
    "fireplace_08.wav", "fire-crackle-and-flames.wav", "fireplace_03.wav",
}


def main():
    k = 4
    if "--k" in sys.argv:
        k = int(sys.argv[sys.argv.index("--k") + 1])
    keep_all = "--all" in sys.argv
    files = sorted(glob.glob(os.path.join(refs.DEFAULT_DIR, "*.wav")))
    names, B = [], []
    for p in files:
        if not keep_all and os.path.basename(p) in RUMBLE_ONLY:
            continue
        r = bed.measure(p)
        if r is None:
            continue
        names.append(os.path.basename(p))
        B.append(r["bands"])
    B = np.array(B)
    rng = np.random.default_rng(7)
    C = B[rng.choice(len(B), k, replace=False)].copy()
    for _ in range(200):
        d = ((B[:, None, :] - C[None, :, :]) ** 2).sum(axis=2)
        a = d.argmin(axis=1)
        for j in range(k):
            if (a == j).any():
                C[j] = B[a == j].mean(axis=0)
    order = np.argsort([(bed.OCT * 10 ** (c / 10)).sum() / (10 ** (c / 10)).sum() for c in C])
    print("%-42s %s" % ("", " ".join("%6.0f" % f for f in bed.OCT)))
    for rank, j in enumerate(order):
        print()
        print("cluster %d  (%d members)                %s" % (
            rank, int((a == j).sum()), " ".join("%6.1f" % v for v in C[j])))
        for n, ai in zip(names, a):
            if ai == j:
                print("    %-38s %s" % (n[:38], " ".join("%6.1f" % v for v in B[names.index(n)])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
