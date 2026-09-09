"""The temporal structure: syllables into phrases, phrases into a soundscape.

    CHIRPPARADE_REFS=/path/to/wavs python3 phrases.py [--per-file]

ChirpParade has to do two things the plugin's parameters keep separate, and both
of them are measured here.

  one shot   A note is played and one *phrase* comes out: a run of syllables at
             a rate of its own, then silence. So the plugin needs the number of
             syllables in a phrase and the gap between them inside it.

  drone      Nothing is played and birds carry on by themselves. That is a
             Poisson process with a rate, and the rate is what the multi-bird
             references measure.

The two are separated by the same statistic: the gap between syllables is
strongly bimodal. Short gaps are inside a phrase; long gaps are between them.
The split is found per file rather than fixed, as a threshold in log time,
because a budgie's phrase gap is a nightingale's syllable gap.
"""
import os
import sys

import numpy as np

import syllables as S

REFS = os.environ.get("CHIRPPARADE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
SECONDS = float(os.environ.get("CHIRPPARADE_SECONDS", "60"))

# A gap longer than this is between phrases whatever the per-file split says:
# no bird holds a phrase together across a second and a half of silence.
MAX_INPHRASE_GAP = 1.5

# Two syllables closer than this are *joined*: the envelope dips between them
# but never reaches silence, which is what a legato trill is. They are inside a
# phrase by construction, and they are kept out of the threshold search --
# there are thousands of them at a gap of exactly zero, and Otsu handed back a
# split between "zero" and "everything else" for the whole library.
JOINED_GAP = 0.012


def split_gaps(gaps):
    """The threshold that separates within-phrase from between-phrase gaps.

    Otsu's method on log gap, which is a one-dimensional two-class split and
    needs no assumption about either class. Returns None when the gaps are not
    bimodal at all -- an isolated call has no phrase structure, and inventing
    one for it would put a phrase length in the statistics that no bird sang.
    """
    g = np.asarray([x for x in gaps if x > JOINED_GAP], float)
    if len(g) < 8:
        return None
    lg = np.log10(g)
    lo, hi = lg.min(), lg.max()
    if hi - lo < 0.5:  # everything inside a factor of three: one class
        return None
    edges = np.linspace(lo, hi, 41)
    hist, _ = np.histogram(lg, bins=edges)
    total = hist.sum()
    centres = 0.5 * (edges[1:] + edges[:-1])
    best, bestvar = None, -1.0
    for i in range(1, len(hist)):
        w0 = hist[:i].sum() / total
        w1 = 1.0 - w0
        if w0 <= 0.0 or w1 <= 0.0:
            continue
        m0 = (hist[:i] * centres[:i]).sum() / hist[:i].sum()
        m1 = (hist[i:] * centres[i:]).sum() / hist[i:].sum()
        var = w0 * w1 * (m0 - m1) ** 2
        if var > bestvar:
            bestvar, best = var, edges[i]
    if best is None:
        return None
    # Otsu will split anything, including a unimodal spread. The split only
    # counts as phrase structure if the two classes really are apart.
    thresh = 10.0 ** best
    a = g[g <= thresh]
    b = g[g > thresh]
    if len(a) < 3 or len(b) < 2:
        return None
    if np.median(b) / max(np.median(a), 1e-6) < 3.0:
        return None
    return min(thresh, MAX_INPHRASE_GAP)


def phrases(sylls, thresh):
    out = []
    cur = [sylls[0]]
    for prev, nxt in zip(sylls, sylls[1:]):
        if nxt.t0 - prev.t1 > thresh:
            out.append(cur)
            cur = [nxt]
        else:
            cur.append(nxt)
    out.append(cur)
    return out


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

    all_gaps, in_gaps, out_gaps = [], [], []
    lengths, rates, durations, densities, shares = [], [], [], [], []
    joined_total, gap_total = 0, 0
    structured = 0
    rows = []

    for f in files:
        r = S.analyse(os.path.join(REFS, f), SECONDS)
        sy = r["syllables"] if r else []
        if len(sy) < 3:
            rows.append((f, len(sy), None))
            continue
        gaps = S.gaps(sy)
        all_gaps.extend(gaps)
        joined_total += sum(1 for g in gaps if g <= JOINED_GAP)
        gap_total += len(gaps)
        density = 60.0 * len(sy) / r["seconds"]
        densities.append(density)
        shares.append(r["voiced_share"])

        thresh = split_gaps(gaps)
        if thresh is None:
            rows.append((f, len(sy), (density, r["voiced_share"], None, None, None, None)))
            continue
        structured += 1
        ph = phrases(sy, thresh)
        plen = [len(p) for p in ph]
        pdur = [p[-1].t1 - p[0].t0 for p in ph]
        # Rate inside a phrase: syllables per second over the phrase's own span.
        prate = [(len(p) - 1) / max(p[-1].t0 - p[0].t0, 1e-6) for p in ph if len(p) > 1]
        ingap = [g for g in gaps if JOINED_GAP < g <= thresh]
        outgap = [g for g in gaps if g > thresh]
        in_gaps.extend(ingap)
        out_gaps.extend(outgap)
        lengths.extend(plen)
        durations.extend(pdur)
        rates.extend(prate)
        rows.append((f, len(sy), (density, r["voiced_share"], thresh,
                                  np.median(plen), np.median(pdur),
                                  np.median(prate) if prate else 0.0)))

    if per_file:
        print("%-42s%5s%8s%7s%8s%7s%8s%8s" %
              ("reference", "n", "syl/min", "voiced", "split s", "syl/ph", "ph s",
               "syl/s"))
        for f, n, got in rows:
            if not got:
                print("%-42s%5d   (too few syllables for a structure)" % (f[:42], n))
                continue
            density, share, thresh, plen, pdur, prate = got
            if thresh is None:
                print("%-42s%5d%8.0f%7.2f    (no phrase structure)" %
                      (f[:42], n, density, share))
                continue
            print("%-42s%5d%8.0f%7.2f%8.2f%7.0f%8.2f%8.1f" %
                  (f[:42], n, density, share, thresh, plen, pdur, prate))
        print()

    print("%d references, %d with a bimodal gap distribution" % (len(files), structured))
    print()
    print("syllables per minute       : %s" % spread(densities, "%.0f"))
    print("voiced share of the file   : %s" % spread(shares))
    print("syllables joined, no silence: %d of %d gaps (%.0f %%)" %
          (joined_total, gap_total, 100.0 * joined_total / max(gap_total, 1)))
    print("every gap                s : %s" % spread(all_gaps, "%.2f"))
    print("gap inside a phrase      s : %s" % spread(in_gaps, "%.3f"))
    print("gap between phrases      s : %s" % spread(out_gaps, "%.2f"))
    print("syllables per phrase       : %s" % spread(lengths, "%.0f"))
    print("phrase duration          s : %s" % spread(durations, "%.2f"))
    print("syllable rate in a phrase /s: %s" % spread(rates, "%.1f"))
    if in_gaps and out_gaps:
        print()
        print("the two gap classes are a factor of %.1f apart in the median" %
              (np.median(out_gaps) / max(np.median(in_gaps), 1e-6)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
