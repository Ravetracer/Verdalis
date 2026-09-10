"""How many of a signal's events are broadband clicks rather than tones.

    python3 striation.py <file>...

This exists because a spectrogram showed the fault that four aggregate
statistics missed: RiverFlow's events were drawing dense vertical striations
across the whole spectrum, with only a few tonal blobs among them. A vertical
line in a spectrogram is an impulse -- energy appearing in every band at the
same instant -- and a pocket of air ringing at its Minnaert pitch is the
opposite of that.

So: detect onsets independently in eight narrow bands, and ask how often they
coincide. A broadband click fires all eight at once; a bubble fires one or two.
The fraction of onsets that are part of a wide coincidence is the striation
rate, and the mean number of bands per coincidence is how wide they are.

Printed beside a Gaussian control, which has no events at all and whose figure
is therefore what chance coincidence alone produces.
"""
import sys, os, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

BANDS = [(200, 320), (320, 500), (500, 800), (800, 1250),
         (1250, 2000), (2000, 3150), (3150, 5000), (5000, 8000)]
COINCIDENCE_MS = 3.0
WIDE = 4  # bands firing together before it counts as broadband


def band_onsets(m, sr, lo, hi, k=3.5):
    n = 1 << int(math.ceil(math.log2(len(m))))
    X = np.fft.rfft(m, n)
    f = np.fft.rfftfreq(n, 1.0 / sr)
    X[(f < lo) | (f > hi)] = 0.0
    y = np.fft.irfft(X)[:len(m)]
    hop = max(1, int(sr * 0.001))
    kf = len(y) // hop
    e = np.sqrt(np.mean(y[:kf * hop].reshape(kf, hop) ** 2, axis=1) + 1e-30)
    d = np.diff(e, prepend=e[0])
    med = np.median(d)
    mad = np.median(np.abs(d - med)) * 1.4826 + 1e-30
    thr = med + k * mad
    out = []
    last = -100
    for i in range(1, len(d) - 1):
        if d[i] > thr and d[i] >= d[i - 1] and d[i] > d[i + 1] and i - last >= 3:
            out.append(i)
            last = i
    return np.array(out, int)  # in milliseconds


def measure(m, sr):
    per = [band_onsets(m, sr, lo, min(hi, sr * 0.45)) for lo, hi in BANDS]
    total = sum(len(p) for p in per)
    if total == 0:
        return 0.0, 0.0, 0.0
    # Bin every onset onto a millisecond grid and count how many distinct bands
    # fire inside each coincidence window.
    dur_ms = int(len(m) / sr * 1000)
    grid = np.zeros((len(BANDS), dur_ms + 8), dtype=bool)
    for b, p in enumerate(per):
        p = p[p < dur_ms]
        grid[b, p] = True
    w = int(COINCIDENCE_MS)
    width = np.zeros(dur_ms, int)
    for t in range(dur_ms - w):
        width[t] = int(grid[:, t:t + w].any(axis=1).sum())
    # Non-overlapping peaks of the coincidence count.
    wide_n, wide_sum, i = 0, 0, 0
    while i < dur_ms - w:
        if width[i] >= WIDE:
            wide_n += 1
            wide_sum += width[i]
            i += w
        else:
            i += 1
    secs = len(m) / sr
    return (wide_n / secs, (wide_sum / wide_n) if wide_n else 0.0, total / secs)


def main():
    rng = np.random.default_rng(11)
    c = measure(rng.standard_normal(48000 * 8), 48000)
    print("%-42s %11s %11s %11s" % ("", "wide/s", "bands wide", "onsets/s"))
    print("%-42s %11.1f %11.2f %11.1f" % ("-- CONTROL: gaussian noise --", *c))
    for p in sys.argv[1:]:
        x, sr = wavio.read_wav(p)
        m = wavio.to_mono(x)
        n = int(min(10.0 * sr, len(m)))
        st = (len(m) - n) // 2
        r = measure(m[st:st + n], sr)
        print("%-42s %11.1f %11.2f %11.1f" % (os.path.basename(p)[:42], *r))
    return 0


if __name__ == "__main__":
    sys.exit(main())
