"""Compares every factory preset against the reference it was fitted to.

    SKYHOWL_REFS=/path/to/refs python3 fit.py <dir-of-rendered-presets>

Prints, per preset, the figures the presets were actually fitted on -- the
third-octave shape relative to broadband, the crest factor, the L/R
correlation, the turbulence intensity and the gust factor -- beside the same
figures for its reference, and flags the bands more than 6 dB out.

**Bands above the reference's own bandwidth are not compared.** Much of the
library is lossy-encoded, and the encoders put a hard lowpass in: one reference
holds nothing above 3 kHz, several stop between 4 and 7 kHz, and only the .wav
material runs to 24. Fitting the synthesis to a band the reference does not
contain would be fitting it to an encoder, so the usable bandwidth is measured
per reference (the highest frequency still within 60 dB of the in-band peak)
and the comparison stops there. The cut-off is printed beside each pair.

Render the presets with the seed pinned, or the comparison moves between runs:

    skyhowl-render --plugin ./SkyHowl.clap --all --outdir /tmp/wav \\
        --seconds 100 --tail 3 --rate 48000 --param randomseed=7
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np
import wavio

REFS = os.environ.get("SKYHOWL_REFS", os.path.expanduser("~/refs"))
CENTRES = np.array([31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000], float)

# Which reference each preset was fitted against. The recordings are not part
# of this repository and are not ours to redistribute; they live outside it and
# are read, never copied.
PAIRS = [
    ("Open_Plain",      "wind_plain"),
    ("Meadow_Breeze",   "wind_meadow"),
    ("Soft_Breath",     "very_soft_wind"),
    ("Gentle_Gusts",    "gentle_wind_gusts"),
    ("Howling_Wind",    "howling_wind"),
    ("Howling_Storm",   "strong_howling_storm"),
    ("Winter_Gale",     "freezing_winter_wind"),
    ("Arctic_Cold",     "cold_arctic_wind"),
    ("Snow_Storm",      "heavy_snow_storm"),
    ("Cave_Mouth",      "cave_wind"),
    ("Between_Houses",  "storm_between_houses"),
    ("Desert_Dune",     "wind_dune_medium_desert"),
    ("Mountain_Ridge",  "wind_mountain"),
    ("Wind_In_Birches", "wind_in_birch_trees"),
    ("Bare_Trees",      "wind_in_city_bare_trees"),
    ("Rustling_Leaves", "rustling_leafs"),
    ("Autumn_Leaves",   "rustling_leafs_light"),
    ("Conifer_Sough",   "wind_through_forest"),
    ("River_Reeds",     "wind_in_river_reeds"),
    ("Beach_Grass",     "wind_european_beachgrass"),
    ("Far_Away",        "wind_far_away"),
    ("Spooky_Wind",     "spooky_wind"),
    ("Underground",     "wind_underground"),
]


def bandwidth(path):
    """Highest frequency the recording still holds, within 60 dB of its peak.

    A lossy encoder's lowpass shows up here as a cliff; a real spectrum does
    not reach one.
    """
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    n = 1 << 14
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    acc = np.zeros(len(freqs))
    frames = 0
    for i in range(0, max(1, len(m) - n), n * 4):
        acc += np.abs(np.fft.rfft(m[i:i + n] * win)) ** 2
        frames += 1
    db = 10 * np.log10(np.maximum(acc / max(1, frames), 1e-30))
    ref = db[(freqs > 200) & (freqs < 2000)].max()
    ok = np.where((db > ref - 60.0) & (freqs > 1000))[0]
    return float(freqs[ok[-1]]) if len(ok) else 1000.0


def metrics(path):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    rms = np.sqrt(np.mean(m ** 2))
    crest = 20 * np.log10(np.max(np.abs(m)) / max(1e-12, rms))
    corr = float(np.corrcoef(x[:, 0], x[:, 1])[0, 1]) if x.shape[1] >= 2 else 1.0

    hop = int(sr * 0.05)
    k = len(m) // hop
    e = np.sqrt(np.mean(m[:k * hop].reshape(k, hop) ** 2, axis=1) + 1e-20)
    turb = (e.std() / max(1e-12, e.mean())) / 3.0
    gustf = (np.percentile(e, 95) / max(1e-12, np.median(e))) ** (1.0 / 3.0)

    n = 1 << 14
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    acc = np.zeros(len(CENTRES))
    frames = 0
    for i in range(0, max(1, len(m) - n), n):
        mag = np.abs(np.fft.rfft(m[i:i + n] * win)) ** 2
        for j, fc in enumerate(CENTRES):
            sel = (freqs >= fc / 2 ** (1 / 6)) & (freqs < fc * 2 ** (1 / 6))
            if sel.any():
                acc[j] += mag[sel].mean()
        frames += 1
    acc /= max(1, frames)
    band = 10 * np.log10(np.maximum(acc / acc.sum(), 1e-12))
    return crest, corr, turb, gustf, band, 20 * np.log10(max(rms, 1e-12))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    src = sys.argv[1]
    hdr = "".join("%7d" % int(c) for c in CENTRES)
    print("%-18s%7s%7s%7s%7s%7s  %s" %
          ("preset", "rms", "crest", "L/R", "I", "gustF", hdr))
    worst = []
    missing = 0
    for mine, ref in PAIRS:
        mp = os.path.join(src, mine + ".wav")
        rp = os.path.join(REFS, ref + ".wav")
        if not os.path.exists(mp) or not os.path.exists(rp):
            print("%-18s  -- missing (%s)" % (mine, mp if not os.path.exists(mp) else rp))
            missing += 1
            continue
        c1, k1, t1, g1, b1, r1 = metrics(mp)
        c2, k2, t2, g2, b2, r2 = metrics(rp)
        bw = bandwidth(rp)
        print("%-18s%7.1f%7.1f%7.2f%7.2f%7.2f  %s" %
              (mine[:18], r1, c1, k1, t1, g1, "".join("%7.1f" % v for v in b1)))
        print("%-18s%7.1f%7.1f%7.2f%7.2f%7.2f  %s   (to %.1f kHz)" %
              ("  ref", r2, c2, k2, t2, g2, "".join("%7.1f" % v for v in b2), bw * 0.001))
        d = b1 - b2
        bad = [(abs(d[j]), int(CENTRES[j]), d[j]) for j in range(len(d))
               if abs(d[j]) > 6.0 and CENTRES[j] <= bw]
        if bad:
            bad.sort(reverse=True)
            print("%-18s%42s  %s" % ("  out", "",
                  " ".join("%dHz %+.0f" % (b[1], b[2]) for b in bad[:4])))
            worst.append((bad[0][0], mine, bad[0][1], bad[0][2]))
    worst.sort(reverse=True)
    print()
    print("presets with a band more than 6 dB out: %d of %d" % (len(worst), len(PAIRS) - missing))
    for w in worst[:10]:
        print("  %-18s %5d Hz  %+.1f dB" % (w[1], w[2], w[3]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
