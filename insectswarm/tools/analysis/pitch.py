"""Periodicity estimation, shared by wingbeat.py and pulse.py.

The normalised square difference function (McLeod's NSDF) rather than a
harmonic sum, because it comes with a clarity value in [0, 1] that is a usable
voicing gate. A harmonic sum's score is smooth in f0 and its runner-up is
almost always the octave, so nothing useful can be thresholded on it -- the
first attempt at this called 55 of the 67 references unvoiced.
"""
import math
import numpy as np


def bandpass(x, sr, lo, hi):
    n = 1 << int(math.ceil(math.log2(len(x))))
    X = np.fft.rfft(x, n)
    f = np.fft.rfftfreq(n, 1.0 / sr)
    X[(f < lo) | (f > hi)] = 0.0
    return np.fft.irfft(X)[:len(x)]


def nsdf(x):
    """McLeod's normalised square difference function, via FFT."""
    n = len(x)
    nf = 1 << int(math.ceil(math.log2(2 * n)))
    X = np.fft.rfft(x, nf)
    r = np.fft.irfft(X * np.conj(X))[:n]
    cum = np.concatenate(([0.0], np.cumsum(x * x)))
    tau = np.arange(n)
    m = cum[n - tau] + (cum[n] - cum[tau])
    return 2.0 * r / np.maximum(m, 1e-30)


def pick_period(d, lo, hi, k=0.88):
    """First NSDF peak reaching k of the highest, inside [lo, hi] lags.

    Taking the first rather than the highest is what keeps a buzz on its
    fundamental: the octave above frequently scores a hair higher, and taking
    the maximum halves the reported wingbeat rate in about a third of frames.
    """
    lo = max(int(lo), 2); hi = min(int(hi), len(d) - 2)
    if hi <= lo + 2:
        return None, 0.0
    seg = d[lo:hi]
    rise = np.where((seg[1:-1] >= seg[:-2]) & (seg[1:-1] > seg[2:]))[0] + 1
    if len(rise) == 0:
        return None, 0.0
    vals = seg[rise]
    best = vals.max()
    if best <= 0.0:
        return None, 0.0
    cand = rise[vals >= k * best]
    i = int(cand[0]) + lo
    # parabolic refinement on the NSDF peak
    a, b, c = d[i - 1], d[i], d[i + 1]
    denom = a - 2.0 * b + c
    delta = 0.5 * (a - c) / denom if abs(denom) > 1e-20 else 0.0
    return i + float(np.clip(delta, -1.0, 1.0)), float(b)
