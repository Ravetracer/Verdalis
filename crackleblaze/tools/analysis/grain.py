"""How grainy a reference is, band by band, measured against a white-noise control.

    python3 grain.py [dir-or-file] [--secs 12]

The trap this script exists to avoid: a flux-based onset detector finds
"events" in band-limited white noise at a steady ten to twenty a second,
because noise has amplitude fluctuations of its own. Any event rate quoted for
a river is meaningless without knowing what the same detector reports for noise
with the same spectrum. So every statistic here is printed beside the figure a
Gaussian-noise control gives, and what matters is the excess.

Per band, three statistics of the 4 ms envelope:

  cv        standard deviation over mean. Rayleigh noise gives 0.523.
  p99/med   how far the loudest hundredth stands above the middle. Noise: ~2.6.
  kurt      excess kurtosis of the envelope. Noise: ~0.25.

A river that is genuinely "just noise" lands on the control row. A creek does
not, and the band in which it does not is the band its bubbles live in.
"""
import sys, os, glob, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

BANDS = [("200-800", 200.0, 800.0), ("0.8-2k", 800.0, 2000.0),
         ("2-6k", 2000.0, 6000.0), ("6-14k", 6000.0, 14000.0)]


def band_envelope(m, sr, lo, hi, env_ms=4.0):
    """Envelope of one band, via an FFT brick-wall filter and RMS blocks."""
    n = 1 << int(math.ceil(math.log2(len(m))))
    X = np.fft.rfft(m, n)
    f = np.fft.rfftfreq(n, 1.0 / sr)
    X[(f < lo) | (f > hi)] = 0.0
    y = np.fft.irfft(X)[:len(m)]
    hop = max(1, int(sr * env_ms / 1000.0))
    k = len(y) // hop
    if k < 32:
        return None
    return np.sqrt(np.mean(y[:k * hop].reshape(k, hop) ** 2, axis=1) + 1e-30)


def stats(e):
    mu = e.mean()
    if mu <= 0:
        return float("nan"), float("nan"), float("nan")
    cv = e.std() / mu
    ratio = np.percentile(e, 99) / max(np.median(e), 1e-30)
    z = (e - mu) / max(e.std(), 1e-30)
    kurt = float(np.mean(z ** 4) - 3.0)
    return float(cv), float(ratio), kurt


def row(m, sr):
    out = []
    for _, lo, hi in BANDS:
        if hi > sr * 0.45:
            out.append((float("nan"),) * 3)
            continue
        e = band_envelope(m, sr, lo, hi)
        out.append(stats(e) if e is not None else (float("nan"),) * 3)
    return out


def main():
    argv = sys.argv[1:]
    secs = 12.0
    args = []
    i = 0
    while i < len(argv):
        if argv[i] == "--secs":
            secs = float(argv[i + 1]); i += 2
        else:
            args.append(argv[i]); i += 1
    target = args[0] if args else os.path.join(os.path.dirname(__file__), "../../!dev/references")
    files = sorted(glob.glob(os.path.join(target, "*.wav"))) if os.path.isdir(target) else [target]

    hdr = "%-52s" % "reference"
    for name, _, _ in BANDS:
        hdr += " | %s" % ("%-6s cv  p99 kurt" % name)
    print(hdr)

    # The control: Gaussian noise, same length, same sample rate.
    rng = np.random.default_rng(7)
    ctl = row(rng.standard_normal(int(48000 * secs)), 48000)
    line = "%-52s" % "-- CONTROL: gaussian white noise --"
    for c, p, k in ctl:
        line += " | %11.2f %4.1f %4.1f" % (c, p, k)
    print(line)

    acc = []
    names = []
    for p in files:
        try:
            x, sr = wavio.read_wav(p)
            n = int(min(secs * sr, x.shape[0]))
            st = (x.shape[0] - n) // 2
            r = row(wavio.to_mono(x[st:st + n]), sr)
        except Exception as exc:
            print("%-52s ERR %s" % (os.path.basename(p)[:52], exc))
            continue
        acc.append(r)
        names.append(os.path.basename(p))
        line = "%-52s" % os.path.basename(p)[:52]
        for c, pp, k in r:
            line += " | %11.2f %4.1f %4.1f" % (c, pp, k)
        print(line)
    if not acc:
        return 1
    A = np.array(acc, float)
    print()
    for j_label, j_idx in (("cv", 0), ("p99/median", 1), ("kurtosis", 2)):
        print("%-24s | %s" % ("summary " + j_label, "  ".join("%-8s" % b[0] for b in BANDS)))
        for label, fn in (("min", np.nanmin), ("median", np.nanmedian), ("max", np.nanmax)):
            print("%-24s | %s" % (label, "  ".join("%8.2f" % fn(A[:, j, j_idx]) for j in range(len(BANDS)))))
        print("%-24s | %s" % ("control", "  ".join("%8.2f" % ctl[j][j_idx] for j in range(len(BANDS)))))
        print()
    # The grainiest and the smoothest in each band, which is what names the presets.
    for j, (name, _, _) in enumerate(BANDS):
        order = np.argsort(-A[:, j, 0])
        good = [k for k in order if np.isfinite(A[k, j, 0])]
        print("%-8s grainiest: %s" % (name, ", ".join("%s (%.2f)" % (names[k][:34], A[k, j, 0]) for k in good[:4])))
        print("%-8s smoothest: %s" % (name, ", ".join("%s (%.2f)" % (names[k][:34], A[k, j, 0]) for k in good[-4:])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
