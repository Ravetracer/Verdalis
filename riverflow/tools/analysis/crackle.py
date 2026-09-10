"""How impulsive a signal is above 5 kHz -- which is what crackle is.

    python3 crackle.py <file>...

Crackle is not a line spectrum and not a raised noise floor: it is isolated,
very short, broadband transients. Neither the octave-band statistics nor a
peak-prominence search sees it, and both were tried first.

What does see it: high-pass at 5 kHz, take a 0.5 ms envelope, and ask how far
into the tail its loudest moments sit. A smooth noise bed has a Rayleigh
envelope with a p99.9-to-median ratio near 4; every sharp transient in the
signal pushes that ratio up, and a crackling signal runs to 15 and beyond.
Printed beside a Gaussian control, as everything in this directory is.
"""
import sys, os, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio


def crackle(m, sr):
    n = 1 << int(math.ceil(math.log2(len(m))))
    X = np.fft.rfft(m, n)
    f = np.fft.rfftfreq(n, 1.0 / sr)
    X[f < 5000.0] = 0.0
    y = np.fft.irfft(X)[:len(m)]
    hop = max(1, int(sr * 0.0005))
    k = len(y) // hop
    e = np.sqrt(np.mean(y[:k * hop].reshape(k, hop) ** 2, axis=1) + 1e-30)
    med = max(float(np.median(e)), 1e-30)
    return (float(np.percentile(e, 99.9) / med), float(np.percentile(e, 99) / med),
            float(np.mean(((e - e.mean()) / max(e.std(), 1e-30)) ** 4) - 3.0))


def main():
    rng = np.random.default_rng(5)
    c = crackle(rng.standard_normal(48000 * 8), 48000)
    print("%-34s %9s %9s %9s" % ("", "p99.9/med", "p99/med", "kurtosis"))
    print("%-34s %9.2f %9.2f %9.1f" % ("-- CONTROL: gaussian noise --", *c))
    for p in sys.argv[1:]:
        x, sr = wavio.read_wav(p)
        r = crackle(wavio.to_mono(x), sr)
        print("%-34s %9.2f %9.2f %9.1f" % (os.path.basename(p)[:34], *r))
    return 0


if __name__ == "__main__":
    sys.exit(main())
