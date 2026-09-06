"""Compares every factory preset against the reference it was fitted to.

    SHOREBREAK_REFS=/path/to/refs python3 fit.py <dir-of-rendered-presets>

Prints, per preset, the crest factor, the envelope variation and the
third-octave shape relative to broadband, beside the same figures for its
reference, and flags the bands that are more than 6 dB out.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

B = os.environ.get("SHOREBREAK_REFS", os.path.expanduser("~/Dokumente/Samples/Ocean Waves"))
CENTRES = np.array([50, 100, 200, 400, 800, 1600, 3150, 6300, 12500], float)

PAIRS = [
    ("Distant_Roar",        "Northsea - Earth-Toned Roar (Rizzi)"),
    ("Open_Sea_Murmur",     "Northsea - White Noise Murmur (Rizzi)"),
    ("Big_Waves_Crashing",  "Kattegat Uproar - Big Waves Crashing"),
    ("Crashing_Tides",      "Eyrarsund - Crashing Tides"),
    ("Rolling_Tide",        "Shores of Kattegat - Rolling Tide"),
    ("Rhythmic_Tide",       "Beach of Kattegat - Rhythmic Tide"),
    ("Sand_and_Foam",       "Beach of Kattegat - Sand and Foam"),
    ("Receding_Sand",       "Forest Ocean - Receding Sand pt. 1"),
    ("Gentle_Waves",        "The City Coast - Gentle Waves"),
    ("Shallow_Clear_Water", "Forest Ocean - Shallow Clear Water"),
    ("Tidal_Swells",        "Eyrarsund - Tidal Swells"),
    ("Fast_Swells",         "Kattegat Uproar - Fast Swells"),
    ("Crystal_Bubbles",     "Shores of Kattegat - Crystal Clear Bubbles"),
    ("Harbour_Lapping",     "The City Harbor - Water Lapping on Pier"),
    ("Hollow_Shore",        "The City Coast - Hollow Shore"),
    ("Uproar_Waves",        "Eyrarsund - Uproar Waves"),
    ("Turmoil_Fjord",       "Kolding Fjord - Turmoil Fjord pt. 1 (Rizzi)"),
]

def metrics(path):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    rms = np.sqrt(np.mean(m ** 2)); pk = np.max(np.abs(m))
    crest = 20 * np.log10(pk / max(1e-12, rms))
    hop = int(sr * 0.05); k = len(m) // hop
    e = np.sqrt(np.mean(m[:k * hop].reshape(k, hop) ** 2, axis=1) + 1e-20)
    cv = e.std() / max(1e-12, e.mean())
    n = 1 << 15; win = np.hanning(n); freqs = np.fft.rfftfreq(n, 1 / sr)
    acc = np.zeros(len(CENTRES)); frames = 0
    for i in range(0, max(1, len(m) - n), n // 2 * 4):
        mag = np.abs(np.fft.rfft(m[i:i + n] * win)) ** 2
        for j, fc in enumerate(CENTRES):
            sel = (freqs >= fc / 2 ** (1 / 6)) & (freqs < fc * 2 ** (1 / 6))
            if sel.any(): acc[j] += mag[sel].sum()
        frames += 1
    acc /= max(1, frames)
    band = 10 * np.log10(np.maximum(acc / acc.sum(), 1e-12))
    return crest, cv, band

src = sys.argv[1]
hdr = "".join(f"{int(c):>7}" for c in CENTRES)
print(f"{'preset':<22}{'crest':>6}{'envCV':>7}  " + hdr)
worst = []
for mine, ref in PAIRS:
    mp = os.path.join(src, mine + ".wav")
    rp = os.path.join(B, ref + ".wav")
    if not (os.path.exists(mp) and os.path.exists(rp)):
        print(f"{mine:<22} (missing)"); continue
    a = metrics(mp); b = metrics(rp)
    print(f"{mine[:22]:<22}{a[0]:>6.1f}{a[1]:>7.2f}  " + "".join(f"{v:>7.1f}" for v in a[2]))
    print(f"{'   ref':<22}{b[0]:>6.1f}{b[1]:>7.2f}  " + "".join(f"{v:>7.1f}" for v in b[2]))
    d = a[2] - b[2]
    # Only bands that carry something. A reference can sit at -120 dB at 50 Hz,
    # and being 30 dB above nothing is still nothing: flagging it sends you
    # chasing content neither recording has.
    floor = b[2].max() - 35.0
    bad = [(CENTRES[i], d[i]) for i in range(len(d))
           if abs(d[i]) > 6.0 and max(a[2][i], b[2][i]) > floor]
    if bad:
        worst.append((mine, bad))
        print(f"{'   >6 dB out:':<22}" + "  ".join(f"{int(f)}Hz {v:+.0f}" for f, v in bad))
    else:
        print(f"{'   fit:':<22}every audible band within 6 dB")
    print()
print(f"{len(worst)} of {len(PAIRS)} presets have an audible band more than 6 dB out")
for nm, bad in worst:
    print("   " + nm.ljust(22) + "  ".join(f"{int(f)}Hz {v:+.0f}" for f, v in bad))
