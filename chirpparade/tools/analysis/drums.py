"""Woodpecker drumming: the one layer in the plugin that is not a voice at all.

    CHIRPPARADE_REFS=/path/to/wavs python3 drums.py

Drumming is *sonation*, not vocalisation -- a bill against wood, not a syrinx.
So it is measured with an onset detector rather than with the syllable
segmenter, which looks for tonal frames and finds a hammer roll only by
accident.

What is measured per roll: how many strikes, how fast, whether the rate drifts
across the roll, how long the roll lasts, and the spectrum and decay of one
strike. Those are exactly the parameters of the plugin's drum layer.

The drift is the interesting one. A great spotted woodpecker's roll is usually
described as decelerating towards its end, and if that is true the plugin needs
a drift parameter rather than a fixed rate.
"""
import os
import sys

import numpy as np

import syllables as S

REFS = os.environ.get("CHIRPPARADE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
SECONDS = float(os.environ.get("CHIRPPARADE_SECONDS", "60"))

# Strikes are broadband and very short, so the envelope is followed at 0.5 ms
# and peaks closer together than this are one strike seen twice.
MIN_STRIKE_GAP = 0.020
# A roll ends where the silence is longer than this. Measured rolls run at
# 10-25 strikes a second, so a fifth of a second is several missed strikes.
ROLL_GAP = 0.20
MIN_STRIKES = 4


def strikes(m, sr):
    """Strike times, from peaks of a fast envelope above an adaptive floor."""
    env = S.envelope(m, sr, 0.0005)
    if not len(env):
        return np.zeros(0), env
    # An adaptive floor: the local median over a quarter second, so a roll that
    # fades is still followed. Plain global thresholding lost the tail of every
    # decelerating roll.
    w = max(3, int(0.25 * sr))
    kernel = np.ones(w) / w
    floor = np.convolve(env, kernel, mode="same")
    thresh = np.maximum(3.0 * floor, 0.06 * env.max())

    over = env > thresh
    idx = []
    guard = int(MIN_STRIKE_GAP * sr)
    i = 0
    n = len(env)
    while i < n:
        if not over[i]:
            i += 1
            continue
        j = i
        while j + 1 < n and over[j + 1]:
            j += 1
        k = i + int(np.argmax(env[i:j + 1]))
        if not idx or k - idx[-1] >= guard:
            idx.append(k)
        elif env[k] > env[idx[-1]]:
            idx[-1] = k
        i = j + 1
    return np.asarray(idx) / float(sr), env


def rolls(times):
    out = []
    cur = [times[0]]
    for a, b in zip(times, times[1:]):
        if b - a > ROLL_GAP:
            out.append(cur)
            cur = [b]
        else:
            cur.append(b)
    out.append(cur)
    return [np.asarray(r) for r in out if len(r) >= MIN_STRIKES]


def strike_spectrum(m, sr, t):
    """One strike: its spectral centroid, its bandwidth and how long it rings."""
    i = int(t * sr)
    n = min(1024, len(m) - i)
    if n < 128:
        return None
    seg = m[i:i + n] * np.hanning(n)
    mag = np.abs(np.fft.rfft(seg))
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    sel = freqs >= 100.0
    mag, freqs = mag[sel], freqs[sel]
    p = mag ** 2
    tot = p.sum()
    if tot <= 0.0:
        return None
    centroid = float((p * freqs).sum() / tot)
    spread = float(np.sqrt((p * (freqs - centroid) ** 2).sum() / tot))
    # Decay: how long the fast envelope takes to fall 20 dB from the strike.
    env = S.envelope(m[i:i + int(0.15 * sr)], sr, 0.0005)
    if len(env) < 8 or env.max() <= 0:
        return centroid, spread, 0.0
    below = np.nonzero(env < 0.1 * env.max())[0]
    decay = float(below[0] / sr) if len(below) else float(len(env) / sr)
    return centroid, spread, decay


def spread_str(v, fmt="%.2f"):
    v = np.asarray([x for x in v], float)
    if not len(v):
        return "-"
    return (fmt + " .. " + fmt + ", median " + fmt) % (
        np.percentile(v, 5), np.percentile(v, 95), np.median(v))


def main():
    files = sorted(f for f in os.listdir(REFS)
                   if f.lower().endswith(".wav") and
                   ("woodpecker" in f.lower() or "hammer" in f.lower()))
    if not files:
        print("no woodpecker references in %s" % REFS)
        return 1

    counts, rates, durs, drifts, cents, spreads, decays, jitters = \
        [], [], [], [], [], [], [], []
    thirds = []
    print("%-42s%7s%8s%8s%8s%8s%9s%8s%8s" %
          ("reference", "rolls", "strikes", "rate /s", "roll s", "drift %",
           "jitter", "cen Hz", "ring ms"))
    for f in files:
        m, sr, _ = S.load_mono(os.path.join(REFS, f), SECONDS)
        t, _ = strikes(m, sr)
        if len(t) < MIN_STRIKES:
            print("%-42s%7d   (no roll found)" % (f[:42], 0))
            continue
        rl = rolls(t)
        if not rl:
            print("%-42s%7d   (strikes, but no roll)" % (f[:42], 0))
            continue
        frates, fdrift, fjit = [], [], []
        for r in rl:
            iv = np.diff(r)
            frates.append((len(r) - 1) / (r[-1] - r[0]))
            durs.append(r[-1] - r[0])
            counts.append(len(r))
            # Drift: the change in interval across the roll as a percentage of
            # its mean, from a straight-line fit over the intervals.
            if len(iv) >= 4:
                slope = np.polyfit(np.arange(len(iv)), iv, 1)[0]
                fdrift.append(100.0 * slope * len(iv) / max(iv.mean(), 1e-9))
                fjit.append(float(np.std(iv) / max(iv.mean(), 1e-9)))
                # The same question asked without a fit, as a check that the
                # drift is not the onset detector losing quiet late strikes:
                # a missed strike *lengthens* an interval, so it can only bias
                # this the other way.
                k = max(1, len(iv) // 3)
                thirds.append(float(np.median(iv[-k:]) / max(np.median(iv[:k]), 1e-9)))
        rates.extend(frates)
        drifts.extend(fdrift)
        jitters.extend(fjit)

        fs = [strike_spectrum(m, sr, x) for x in t[:60]]
        fs = [x for x in fs if x]
        if fs:
            cents.extend([x[0] for x in fs])
            spreads.extend([x[1] for x in fs])
            decays.extend([x[2] for x in fs])
        print("%-42s%7d%8d%8.1f%8.2f%8.0f%9.2f%8.0f%8.0f" % (
            f[:42], len(rl), int(np.median([len(r) for r in rl])),
            np.median(frates), np.median([r[-1] - r[0] for r in rl]),
            np.median(fdrift) if fdrift else 0.0,
            np.median(fjit) if fjit else 0.0,
            np.median([x[0] for x in fs]) if fs else 0.0,
            1000.0 * np.median([x[2] for x in fs]) if fs else 0.0))

    print()
    print("strikes per roll           : %s" % spread_str(counts, "%.0f"))
    print("strike rate            /s  : %s" % spread_str(rates, "%.1f"))
    print("roll duration          s   : %s" % spread_str(durs, "%.2f"))
    print("interval drift over a roll %%: %s" % spread_str(drifts, "%.0f"))
    print("interval jitter, sd/mean   : %s" % spread_str(jitters, "%.2f"))
    print("strike centroid        Hz  : %s" % spread_str(cents, "%.0f"))
    print("strike bandwidth       Hz  : %s" % spread_str(spreads, "%.0f"))
    print("strike ring to -20 dB  ms  : %s" %
          spread_str([1000.0 * d for d in decays], "%.0f"))
    if drifts:
        slower = sum(1 for d in drifts if d > 5.0)
        faster = sum(1 for d in drifts if d < -5.0)
        print("last third / first third interval: %s" % spread_str(thirds))
        print()
        print("rolls that slow down: %d of %d;  speed up: %d;  steady: %d" %
              (slower, len(drifts), faster, len(drifts) - slower - faster))
    return 0


if __name__ == "__main__":
    sys.exit(main())
