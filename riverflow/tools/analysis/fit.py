"""Compares every factory preset against the reference it was fitted to.

    python3 fit.py <dir-of-rendered-presets> [--refs DIR]

Renders are produced by riverflow-render with the seed pinned; see README.md
for the recipe. Prints, per preset, the whole-signal statistics and the
octave-band shape beside its reference's, and flags what is more than 6 dB out.

The pairing comes from fitpresets.PRESETS, so there is one place that says
which recording a preset is fitted to and it cannot drift from the fit.
"""
import sys, os, math, argparse
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import refs as R
import grain as G
from fitpresets import PRESETS, OCT

BAND_TOL = 6.0


def metrics(path):
    x, sr = wavio.read_wav(path)
    n = int(min(20.0 * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    x = x[st:st + n]
    m = wavio.to_mono(x)
    rms = math.sqrt(float(np.mean(m ** 2)))
    crest = 20.0 * math.log10(float(np.max(np.abs(m))) / max(rms, 1e-12))
    e50 = R.envelope(m, sr, 50.0)
    cv50 = float(e50.std() / max(e50.mean(), 1e-12))
    freqs, psd = R.spectrum(m, sr)
    cent, _ = R.centroid_rolloff(freqs, psd)
    bands = R.band_levels(m, sr, OCT)
    if OCT[-1] > sr * 0.45:
        bands[-1] = bands[-2] + (bands[-2] - bands[-3])
    cvs = []
    for _, lo, hi in G.BANDS:
        if hi > sr * 0.45:
            cvs.append(float("nan")); continue
        e = G.band_envelope(m, sr, lo, hi)
        cvs.append(G.stats(e)[0] if e is not None else float("nan"))
    corr = 1.0
    if x.shape[1] >= 2:
        a, b = x[:, 0], x[:, 1]
        d = math.sqrt(float(np.sum(a * a) * np.sum(b * b)))
        corr = float(np.sum(a * b) / d) if d > 0 else 1.0
    return dict(crest=crest, cv50=cv50, cent=cent, bands=bands, cvs=cvs, corr=corr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--refs", default=os.path.join(os.path.dirname(__file__),
                                                   "../../!dev/references"))
    a = ap.parse_args()

    print("%-22s %-28s %s" % ("preset", "", " ".join("%6.0f" % f for f in OCT)))
    worst_all, flagged, n = 0.0, 0, 0
    rows = []
    for spec in PRESETS:
        name, ref = spec[0], spec[1]
        rpath = os.path.join(a.refs, ref)
        gpath = os.path.join(a.dir, name.replace("_", " ").title().replace(" ", "_") + ".wav")
        if not os.path.exists(gpath):
            alt = [f for f in os.listdir(a.dir)
                   if f.lower().replace("_", "").startswith(name.replace("_", "")[:12])]
            if alt:
                gpath = os.path.join(a.dir, alt[0])
        if not (os.path.exists(rpath) and os.path.exists(gpath)):
            print("%-22s missing (%s / %s)" % (name, os.path.basename(rpath),
                                               os.path.basename(gpath)))
            continue
        rm, gm = metrics(rpath), metrics(gpath)
        rb = rm["bands"] - rm["bands"].max()
        gb = gm["bands"] - gm["bands"].max()
        err = gb - rb
        worst = float(np.max(np.abs(err)))
        worst_all = max(worst_all, worst)
        n += 1
        bad = int(np.sum(np.abs(err) > BAND_TOL))
        flagged += 1 if bad else 0
        rows.append((name, worst, bad, rm, gm))
        print("%-22s %-28s %s" % (name, "reference", " ".join("%6.1f" % v for v in rb)))
        print("%-22s %-28s %s" % ("", "render", " ".join("%6.1f" % v for v in gb)))
        print("%-22s %-28s %s   worst %.1f dB%s" % (
            "", "error", " ".join("%6.1f" % v for v in err), worst,
            "   <-- %d band(s) over %.0f dB" % (bad, BAND_TOL) if bad else ""))
        print()

    print("%-22s %11s %11s %11s %11s %11s" % ("preset", "crest r/g", "cv50 r/g",
                                              "centroid r/g", "corr r/g", "band cv 2-6k"))
    for name, worst, bad, rm, gm in rows:
        print("%-22s %5.1f/%5.1f %5.2f/%5.2f %5.0f/%5.0f %5.2f/%5.2f %5.2f/%5.2f" % (
            name, rm["crest"], gm["crest"], rm["cv50"], gm["cv50"], rm["cent"], gm["cent"],
            rm["corr"], gm["corr"], rm["cvs"][2], gm["cvs"][2]))
    print()
    print("%d presets compared; worst single band %.1f dB; %d preset(s) with a band over %.0f dB"
          % (n, worst_all, flagged, BAND_TOL))
    return 0


if __name__ == "__main__":
    sys.exit(main())
