#!/usr/bin/env python3
"""What a drop does when it lands on a hard wet surface, measured.

    python3 tools/analysis/smack.py compare ref.wav render.wav [more.wav ...]
    python3 tools/analysis/smack.py shape   ref.wav [--top 200]

`compare` prints one column per file: the same numbers, computed the same way,
for a recording and for a render. `shape` prints the median strike of one file
band by band.

This is not part of the fit in fit.py, and that is deliberate. The objective
there describes a texture -- band energies, flatness, impulsiveness -- and a
texture can be right while every event in it is wrong. That is how the cascade
this replaced survived two re-fits: it measured close in spectrum and in crest,
and it was a rattle. What is here looks at the events themselves.

It needs numpy and scipy. Only the first channel of a file is read: summing a
stereo pair to mono is not safe on processed material, where the two channels
can carry the same envelope and unrelated phase.

Four things worth knowing before trusting a number from it:

- A strike is a peak in the envelope above 1.5 kHz that stands 6 dB clear of
  what surrounds it and 10 dB over a running floor. Both thresholds are part of
  the definition, so rates and level percentiles only mean anything between
  files measured here.
- The median strike is the median *across strikes* at each time, not the mean.
  Other drops land in the window of any one strike; they pull a mean up and
  leave a median alone as long as fewer than half the windows have one.
- Its time windows widen with time -- 0.3 ms at the click, a quarter of the
  delay later on -- so the click is resolved and the tail is not noise.
- Recordings that have been through a spectral gate or a separation model carry
  the processing's own smear around every loud event, about 27 dB under it and
  as long as its analysis frame, before the event as well as after. Below that
  level such a file says nothing about a drop. The pre-strike rows of `shape`
  are there to show where that floor is.
"""
import argparse
import os
import sys

import numpy as np
from scipy import signal
from scipy.ndimage import percentile_filter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', '..', 'shared', 'tools', 'analysis'))
from wavio import read_wav  # noqa: E402

BANDS = [(800, 1600), (1600, 3200), (3200, 6300), (6300, 12500), (12500, 20000)]
SHAPE_BANDS = [(1000, 16000), (1000, 2000), (2000, 4000), (4000, 8000), (8000, 16000)]
# Lags at which the clustering of strikes is read, in seconds.
LAGS = np.array([0.002, 0.004, 0.006, 0.009, 0.013, 0.018, 0.025, 0.035, 0.05])
# Times around a strike at which its median shape is read.
SHAPE_T = np.array([-3, -1.5, 0, 0.3, 0.6, 1, 1.5, 2, 3, 4, 5, 7, 10, 14, 20, 28, 40, 56, 80]) * 1e-3


def db(v):
    return 10.0 * np.log10(np.maximum(v, 1e-15))


def load(path):
    x, sr = read_wav(path)
    return x[:, 0], sr


def hp_envelope(x, sr, hp=1500.0, smooth_ms=0.3):
    """Envelope of everything above `hp`, smoothed over `smooth_ms`."""
    b, a = signal.butter(4, hp / (sr / 2), 'high')
    e = np.abs(signal.hilbert(signal.filtfilt(b, a, x)))
    n = max(1, int(round(smooth_ms * 1e-3 * sr)))
    return np.convolve(e, np.ones(n) / n, mode='same')


def find_strikes(e, sr, prominence_db=6.0, above_floor_db=10.0, min_gap_ms=2.0):
    """Sample index and level in dB of every strike, and the running floor."""
    level = 20 * np.log10(np.maximum(e, 1e-9))
    hop = int(0.002 * sr)
    coarse = percentile_filter(level[::hop], 30, size=251)
    floor = np.interp(np.arange(len(level)), np.arange(len(coarse)) * hop, coarse)
    peaks, _ = signal.find_peaks(level, prominence=prominence_db,
                                 distance=max(1, int(min_gap_ms * 1e-3 * sr)),
                                 wlen=int(0.02 * sr))
    keep = level[peaks] > floor[peaks] + above_floor_db
    return peaks[keep], level[peaks[keep]], floor


def isolated(peaks, level, sr, before=0.02, after=0.04, margin_db=10.0):
    """Strikes with nothing within `margin_db` of them close by."""
    out = []
    for i, p in enumerate(peaks):
        lo = np.searchsorted(peaks, p - before * sr)
        hi = np.searchsorted(peaks, p + after * sr)
        others = np.r_[level[lo:i], level[i + 1:hi]]
        if len(others) == 0 or others.max() < level[i] - margin_db:
            out.append(i)
    return np.array(out, dtype=int)


def texture(x, sr):
    """The numbers that describe the whole file."""
    out = {}
    f, p = signal.welch(x, sr, nperseg=8192)
    energy = np.array([p[(f >= lo) & (f < hi)].sum() for lo, hi in BANDS])
    out['band_pct'] = 100 * energy / max(p[f >= 100].sum(), 1e-30)
    above = f > 150
    cumulative = np.cumsum(p[above])
    out['median_hz'] = float(f[above][np.searchsorted(cumulative, cumulative[-1] / 2)])
    out['crest_db'] = float(20 * np.log10(np.abs(x).max() / np.sqrt(np.mean(x ** 2))))

    # How much of the time the 2-8 kHz band is anywhere near its loudest. A wash
    # spends most of its time there and separate smacks spend almost none.
    sos = signal.butter(4, [2000 / (sr / 2), 8000 / (sr / 2)], 'band', output='sos')
    e = np.abs(signal.hilbert(signal.sosfiltfilt(sos, x)))
    k = int(0.001 * sr)
    e = np.convolve(e, np.ones(k) / k, mode='same')
    top = np.percentile(e, 99.9)
    out['duty20'] = float(np.mean(e > top * 0.1))
    out['median_under_top_db'] = float(20 * np.log10(top / np.median(e)))

    env = hp_envelope(x, sr)
    peaks, level, floor = find_strikes(env, sr)
    seconds = len(x) / sr
    out['strike_rate'] = len(peaks) / seconds
    over = level - floor[peaks]
    out['over_floor_db'] = np.percentile(over, [50, 90, 99]) if len(over) else np.zeros(3)

    # Pair correlation: the density of strikes at a lag after a strike, over the
    # mean rate. One for a Poisson process at every lag.
    t = peaks / sr
    rate = len(t) / seconds
    counts = np.zeros(len(LAGS) - 1)
    for i in range(len(t)):
        counts += np.histogram(t[i + 1:i + 300] - t[i], bins=LAGS)[0]
    out['clustering'] = counts / np.maximum(len(t) * rate * np.diff(LAGS), 1e-12)
    return out


def anatomy(x, sr, strong_db=26.0):
    """The click and the body of strong, isolated strikes."""
    env = hp_envelope(x, sr, hp=1000.0, smooth_ms=0.15)
    peaks, level, floor = find_strikes(env, sr)
    over = level - floor[peaks]
    chosen = [i for i in isolated(peaks, level, sr)
              if over[i] >= strong_db and 0.05 * sr < peaks[i] < len(x) - 0.2 * sr]
    out = {'n': len(chosen)}
    if not chosen:
        return out
    n_fft = 256
    f = np.fft.rfftfreq(n_fft, 1 / sr)
    b, a = signal.butter(2, 1000 / (sr / 2), 'high')
    width6, width20, click_over_body, t60, centroid = [], [], [], [], []
    for i in chosen:
        p = peaks[i]
        seg = x[p - int(0.004 * sr):p + int(0.06 * sr)]
        e = np.abs(signal.hilbert(signal.lfilter(b, a, seg)))
        i0 = int(np.argmax(e[:int(0.008 * sr)]))
        peak = e[i0]

        def width(fraction):
            lo = i0
            while lo > 0 and e[lo] > peak * fraction:
                lo -= 1
            hi = i0
            while hi < len(e) - 1 and e[hi] > peak * fraction:
                hi += 1
            return (hi - lo) / sr * 1e3

        width6.append(width(0.5))
        width20.append(width(0.1))
        k = max(1, int(0.001 * sr))
        energy = np.convolve(e ** 2, np.ones(k) / k, mode='same')
        click_over_body.append(db(peak ** 2 / energy[i0 + int(0.002 * sr)]))
        # The body's decay, from a line through 1.5 to 12 ms after the click.
        t0, t1 = i0 + int(0.0015 * sr), i0 + int(0.012 * sr)
        slope = np.polyfit(np.arange(t1 - t0) / sr, db(energy[t0:t1]), 1)[0]
        t60.append(-60.0 / slope * 1e3 if slope < -1e-6 else np.nan)
        n = int(0.001 * sr) & ~1
        c = p - int(0.004 * sr) + i0
        spectrum = np.abs(np.fft.rfft(x[c - n // 2:c + n // 2] * np.hanning(n), n_fft)) ** 2
        sel = f > 800
        centroid.append(np.sum(f[sel] * spectrum[sel]) / np.sum(spectrum[sel]))
    centroid = np.array(centroid)
    out['width6_ms'] = float(np.median(width6))
    out['width20_ms'] = float(np.median(width20))
    out['click_over_body_db'] = float(np.median(click_over_body))
    out['body_t60_ms'] = float(np.nanmedian(t60)) if np.isfinite(t60).any() else float('nan')
    out['centroid_hz'] = float(np.median(centroid))
    out['centroid_sd_oct'] = float(np.std(np.log2(centroid)))
    return out


def median_shape(x, sr, top=200):
    """Energy around the `top` loudest strikes, in dB under each strike's own
    peak, median across strikes, for each of SHAPE_BANDS."""
    env = hp_envelope(x, sr, hp=1000.0, smooth_ms=0.15)
    peaks, level, floor = find_strikes(env, sr)
    over = level - floor[peaks]
    order = [i for i in np.argsort(-over) if 0.01 * sr < peaks[i] < len(x) - 0.1 * sr][:top]
    shape = {}
    for lo, hi in SHAPE_BANDS:
        sos = signal.butter(4, [lo / (sr / 2), min(hi / (sr / 2), 0.99)], 'band', output='sos')
        e2 = np.abs(signal.hilbert(signal.sosfiltfilt(sos, x))) ** 2
        rows = []
        for i in order:
            p = peaks[i]
            ref = 10 ** (level[i] / 10)
            row = []
            for t in SHAPE_T:
                w = max(0.3e-3, 0.25 * abs(t))
                a = int(p + (t - w / 2) * sr)
                row.append(e2[a:max(a + 1, int(p + (t + w / 2) * sr))].mean() / ref)
            rows.append(row)
        shape[(lo, hi)] = db(np.median(np.array(rows), axis=0)) if rows else np.full(len(SHAPE_T), np.nan)
    return shape, len(order)


def cmd_compare(args):
    cols = []
    for path in args.files:
        x, sr = load(path)
        cols.append((os.path.splitext(os.path.basename(path))[0], texture(x, sr),
                     anatomy(x, sr), median_shape(x, sr, args.top)[0]))
    width = max(13, max(len(c[0]) for c in cols) + 2)

    def line(label, values):
        print('%-26s' % label + ''.join(('%' + str(width) + 's') % v for v in values))

    def of(part, key, fmt):
        return [fmt % c[part][key] if key in c[part] else '-' for c in cols]

    line('', [c[0] for c in cols])
    line('spectral median, Hz', of(1, 'median_hz', '%.0f'))
    for i, (lo, hi) in enumerate(BANDS):
        line('%d-%d Hz, %%' % (lo, hi), ['%.1f' % c[1]['band_pct'][i] for c in cols])
    line('crest, dB', of(1, 'crest_db', '%.1f'))
    line('time within 20 dB of top', of(1, 'duty20', '%.3f'))
    line('median under top, dB', of(1, 'median_under_top_db', '%.1f'))
    line('strikes per second', of(1, 'strike_rate', '%.0f'))
    line('over floor p50/90/99, dB', ['%.0f/%.0f/%.0f' % tuple(c[1]['over_floor_db']) for c in cols])
    for j in range(len(LAGS) - 1):
        line('clustering at %.0f ms' % (1e3 * np.sqrt(LAGS[j] * LAGS[j + 1])),
             ['%.2f' % c[1]['clustering'][j] for c in cols])
    line('strong isolated strikes', of(2, 'n', '%d'))
    line('click width -6 dB, ms', of(2, 'width6_ms', '%.2f'))
    line('click width -20 dB, ms', of(2, 'width20_ms', '%.2f'))
    line('click over +2 ms, dB', of(2, 'click_over_body_db', '%.1f'))
    line('body T60, ms', of(2, 'body_t60_ms', '%.0f'))
    line('click centroid, Hz', of(2, 'centroid_hz', '%.0f'))
    line('  its spread, octaves', of(2, 'centroid_sd_oct', '%.2f'))
    print('median strike, 1-16 kHz, dB under its own peak:')
    for k, t in enumerate(SHAPE_T):
        line('  %+.1f ms' % (1e3 * t), ['%.1f' % c[3][SHAPE_BANDS[0]][k] for c in cols])


def cmd_shape(args):
    for path in args.files:
        x, sr = load(path)
        shape, n = median_shape(x, sr, args.top)
        print('%s: median of the %d loudest strikes' % (os.path.basename(path), n))
        print('   ms          ' + ' '.join('%6.1f' % (1e3 * t) for t in SHAPE_T))
        for (lo, hi), v in shape.items():
            print('   %5d-%5d ' % (lo, hi) + ' '.join('%6.1f' % q for q in v))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name, func in (('compare', cmd_compare), ('shape', cmd_shape)):
        p = sub.add_parser(name)
        p.add_argument('files', nargs='+')
        p.add_argument('--top', type=int, default=200,
                       help='how many of the loudest strikes make the median (default %(default)s)')
        p.set_defaults(func=func)
    args = ap.parse_args()
    args.func(args)


if __name__ == '__main__':
    main()
