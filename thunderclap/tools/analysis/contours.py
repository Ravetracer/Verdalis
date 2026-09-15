#!/usr/bin/env python3
"""Extracts the shapes ThunderClap currently computes from a formula.

    python3 tools/analysis/contours.py front <wav or directory>...
    python3 tools/analysis/contours.py decay <wav or directory>...
    python3 tools/analysis/contours.py fit   <wav or directory>...

`measure.py` answers how much energy sits where. This answers what the curves
look like, so that a measured contour can replace a parametric one -- the same
move ChirpParade made when fitted statistics turned out not to describe a
syllable.

`front` prints two shapes of the attack: the RMS envelope from the onset at
1 ms resolution, and the pressure waveform around the steepest rise, with a
coherence figure saying how much of it survives averaging across a library.
Both turned out not to be contours -- see *What the contours say* in the README
-- and the commands are kept because that is a result worth being able to
reproduce rather than take on trust.

`decay` is atmospheric dispersion: the spectral centroid of the flash over
time, in windows that grow with the log of the time since onset, with the
pre-onset bed subtracted so that rain does not set the floor, and any window
that fails to stand over that bed dropped. It fits `centroid ~ t^-a` and prints
the exponent, which is the number a swept lowpass would have to follow. This is
the one that measures, and it measures flat.

`fit` prints the centroid contour over a library as C++ array initialisers,
close and distant separately.

Only numpy is needed. Any sample rate, 16/24/32-bit PCM or 32-bit float.

The bed subtraction matters for the city recordings: several of these have
rain through the whole file, and rain is broadband, so an unsubtracted
centroid measures the rain once the thunder has decayed into it. Where the
file gives no usable pre-onset bed the row is marked and left out of the
medians.
"""
import glob
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', '..', 'shared', 'tools', 'analysis'))
from wavio import read_wav, to_mono  # noqa: E402

# The N-wave of a close strike peaks in the tens of hertz, so its own shape
# lives under a few hundred. Above that is the channel roughness burst, which
# is a separate feature of the engine and would only blur the front here.
FRONT_LP_HZ = 400.0

# The centroid is measured over this band. Below 30 Hz is wind and handling on
# these recordings; above 8 kHz there is nothing left of a thunder but the bed.
CENTROID_LO, CENTROID_HI = 30.0, 8000.0

# Bed: this much of the file before the onset, when there is that much.
BED_SECONDS = 0.5
BED_MIN_SECONDS = 0.15


def files_in(args):
    for a in args:
        if os.path.isdir(a):
            yield from sorted(glob.glob(os.path.join(a, '*.wav')))
        else:
            yield a


def envelope(x, sr, ms):
    """RMS envelope at a given frame length, and the frame length in samples."""
    n = max(1, int(sr * ms / 1000.0))
    frames = len(x) // n
    env = np.sqrt((x[:frames * n].reshape(frames, n) ** 2).mean(axis=1) + 1e-30)
    return env, n


def find_onset(x, sr):
    """Sample index where the flash starts.

    From the loudest 5 ms frame, walk back while the envelope is above the
    quietest tenth of the file by 9 dB. That floor is the bed -- rain, traffic,
    tape hiss -- rather than a fixed threshold below the peak, because a
    recording with rain in it never gets within 30 dB of silence.
    """
    env, n = envelope(x, sr, 5.0)
    if len(env) < 4:
        return 0
    floor = np.percentile(env, 10)
    peak = int(np.argmax(env))
    thresh = max(floor * 2.82, env[peak] * 1e-3)      # 9 dB over the bed
    j = peak
    while j > 0 and env[j - 1] > thresh:
        j -= 1
    return j * n


def lowpass(x, sr, fc):
    """Zero-phase brick wall. Offline analysis, so no filter design needed --
    and zero phase matters here: a causal filter would smear the front, which
    is the thing being measured."""
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1.0 / sr)
    X[f > fc] = 0.0
    return np.fft.irfft(X, len(x))


ATTACK_MS = 60.0
ATTACK_STEP_MS = 1.0


def attack_contour(x, sr, onset, ms=ATTACK_MS, step_ms=ATTACK_STEP_MS):
    """The attack: RMS envelope from the onset, in dB relative to the loudest
    point of the first 200 ms. This is a shape that survives averaging, because
    it is an envelope and not a waveform."""
    n = max(1, int(sr * step_ms / 1000.0))
    want = int(ms / step_ms)
    ref = x[onset:onset + int(sr * 0.2)]
    if len(ref) < n * 4:
        return None
    peak = np.sqrt((ref ** 2).mean()) if len(ref) else 0.0
    peak = max(peak, np.max(np.abs(ref)) / 4.0, 1e-9)
    out = []
    for i in range(want):
        a = onset + i * n
        seg = x[a:a + n]
        if len(seg) < n:
            return None
        out.append(20 * np.log10(np.sqrt((seg ** 2).mean()) / peak + 1e-9))
    return np.array(out)


def front_window(x, sr, onset, half_ms=8.0, step_ms=0.25):
    """The pressure waveform around the steepest rise near the peak, oriented
    so that the rise is upward and normalised to its own largest excursion.

    Read the result with the bias in mind: aligning on the steepest rise and
    then orienting it guarantees a step at the centre of the window whatever
    the recording contains. Only what happens either side of that step is a
    measurement, which is what `front` reports a coherence figure for.
    """
    env, n = envelope(x, sr, 5.0)
    if len(env) < 2:
        return None
    pk = int(np.argmax(env)) * n
    a = max(0, min(onset, pk - int(sr * 0.05)))
    b = min(len(x), pk + int(sr * 0.05))
    if b - a < 64:
        return None
    seg = lowpass(x[a:b], sr, FRONT_LP_HZ)
    d = np.diff(seg)
    if len(d) < 2:
        return None
    k = int(np.argmax(np.abs(d)))
    if d[k] < 0:
        seg = -seg
    h = int(sr * half_ms / 1000.0)
    if k - h < 0 or k + h >= len(seg):
        return None
    w = seg[k - h:k + h]
    big = np.max(np.abs(w))
    if big <= 0:
        return None
    w = w / big
    idx = np.minimum((np.arange(int(2 * half_ms / step_ms)) * step_ms / 1000.0 * sr)
                     .astype(int), len(w) - 1)
    return w[idx]


def spectrum(seg, sr):
    win = np.hanning(len(seg))
    X = np.abs(np.fft.rfft(seg * win)) ** 2
    f = np.fft.rfftfreq(len(seg), 1.0 / sr)
    return X, f


def centroid_of(power, f, bed=None):
    m = (f >= CENTROID_LO) & (f <= CENTROID_HI)
    p = power[m].copy()
    if bed is not None:
        p = np.maximum(p - bed[m], 0.0)
    total = p.sum()
    if total <= 0:
        return float('nan')
    return float((f[m] * p).sum() / total)


def bed_spectrum(x, sr, onset, nfft):
    """Mean power spectrum of the file before the onset, at nfft resolution."""
    have = onset / float(sr)
    if have < BED_MIN_SECONDS:
        return None
    a = max(0, onset - int(sr * BED_SECONDS))
    seg = x[a:onset]
    if len(seg) < nfft:
        return None
    acc = np.zeros(nfft // 2 + 1)
    count = 0
    win = np.hanning(nfft)
    for start in range(0, len(seg) - nfft + 1, nfft // 2):
        acc += np.abs(np.fft.rfft(seg[start:start + nfft] * win)) ** 2
        count += 1
    return acc / max(count, 1) if count else None


# Windows the centroid is measured in: short while the clap is happening,
# longer as the rumble takes over. Start, in seconds from the onset.
DECAY_TIMES = [0.00, 0.02, 0.05, 0.10, 0.20, 0.40, 0.80, 1.6, 3.2, 6.4, 12.8]


# A window whose signal does not stand this far over the bed is not measuring
# the thunder any more -- it is measuring the rain, and its centroid is the
# rain's. Half of this library is a city recording in rain, so without the gate
# every contour flattens out at the bed's own centroid and looks like a law.
SNR_GATE_DB = 6.0


def decay_contour(x, sr, onset):
    """Spectral centroid per window, bed subtracted and SNR gated.

    Returns (times, centroids, snr_db). A window under the gate keeps its
    place in the arrays as nan, so the callers can say how far a recording
    stayed above its own bed.
    """
    out_t, out_c, out_s = [], [], []
    for i, t0 in enumerate(DECAY_TIMES):
        t1 = DECAY_TIMES[i + 1] if i + 1 < len(DECAY_TIMES) else t0 * 2
        a = onset + int(sr * t0)
        b = min(len(x), onset + int(sr * t1))
        if b - a < 512:
            continue
        nfft = int(2 ** np.floor(np.log2(b - a)))
        seg = x[a:a + nfft]
        bed = bed_spectrum(x, sr, onset, nfft)
        power, f = spectrum(seg, sr)
        m = (f >= CENTROID_LO) & (f <= CENTROID_HI)
        if bed is None:
            snr = float('nan')
        else:
            snr = 10 * np.log10((power[m].sum() + 1e-30) / (bed[m].sum() + 1e-30))
        c = centroid_of(power, f, bed)
        if not np.isnan(snr) and snr < SNR_GATE_DB:
            c = float('nan')
        out_t.append(0.5 * (t0 + t1))
        out_c.append(c)
        out_s.append(snr)
    return np.array(out_t), np.array(out_c), np.array(out_s)


def clap_tilt_db(x, sr, onset):
    """How far 1-2 kHz sits under the loudest octave over the 400 ms from the
    onset. The distance proxy: absorption takes the top end off first, so a
    distant strike has nothing up there and a close one has plenty."""
    seg = x[onset:onset + int(sr * 0.4)]
    if len(seg) < 2048:
        return float('nan')
    nfft = int(2 ** np.floor(np.log2(len(seg))))
    power, f = spectrum(seg[:nfft], sr)
    bands = []
    for lo, hi in [(20, 40), (40, 80), (80, 160), (160, 320), (320, 640),
                   (640, 1280), (1280, 2560)]:
        m = (f >= lo) & (f < hi)
        bands.append(10 * np.log10(power[m].mean() + 1e-30))
    return bands[-1] - max(bands)


def fit_exponent(t, c):
    """centroid ~ t^-a over the windows after the clap. Returns a, or nan."""
    m = (t > 0.05) & (c > 0)
    if m.sum() < 3:
        return float('nan')
    p = np.polyfit(np.log(t[m]), np.log(c[m]), 1)
    return -float(p[0])


def cmd_front(args):
    """Two things: the attack envelope, which is a contour, and the shock front
    waveform, which turns out not to be one."""
    paths = list(files_in(args))
    attacks, fronts, names = [], [], []
    print("%-38s %7s %7s %7s" % ("file", "t-20dB", "t-6dB", "peak"))
    print("%-38s %7s %7s %7s" % ("", "ms", "ms", "ms"))
    for path in paths:
        x, sr = read_wav(path)
        x = to_mono(x)
        onset = find_onset(x, sr)
        a = attack_contour(x, sr, onset)
        if a is None:
            continue
        attacks.append((a, clap_tilt_db(x, sr, onset)))
        names.append(os.path.basename(path))
        w = front_window(x, sr, onset)
        if w is not None:
            fronts.append(w)

        def first_over(db):
            idx = np.nonzero(a >= db)[0]
            return idx[0] * ATTACK_STEP_MS if len(idx) else float('nan')

        print("%-38s %7.0f %7.0f %7.0f" %
              (os.path.basename(path)[:38], first_over(-20), first_over(-6),
               float(np.argmax(a)) * ATTACK_STEP_MS))

    for label, sel in (("close", [a for a, tilt in attacks if tilt > -25]),
                       ("distant", [a for a, tilt in attacks if tilt <= -25])):
        if len(sel) < 2:
            continue
        arr = np.array(sel)
        med = np.median(arr, axis=0)
        lo = np.percentile(arr, 10, axis=0)
        hi = np.percentile(arr, 90, axis=0)
        print("\n%s attack envelope, %d files, dB relative to the first 200 ms, "
              "%g ms steps:" % (label, len(arr), ATTACK_STEP_MS))
        for i in range(0, len(med), 20):
            print("   %3d ms  %s" % (i * ATTACK_STEP_MS,
                                     " ".join("%6.1f" % v for v in med[i:i + 20])))
        print("   spread  %s" % " ".join("%6.1f" % v for v in (hi - lo)[:20]))

    if len(fronts) > 1:
        arr = np.array(fronts)
        mean = arr.mean(axis=0)
        coherence = float((mean ** 2).sum() / (arr ** 2).mean(axis=0).sum())
        half = len(mean) // 2
        t = (np.arange(len(mean)) - half) * 0.25
        print("\nshock front waveform, %d files aligned on their steepest rise:"
              % len(arr))
        print("   t/ms  %s" % " ".join("%6.1f" % v for v in t[::4]))
        print("   mean  %s" % " ".join("%6.2f" % v for v in mean[::4]))
        print("   sd    %s" % " ".join("%6.2f" % v for v in arr.std(axis=0)[::4]))
        print("   coherence %.2f -- 1.00 would be every recording the same "
              "shape.\n   The step at t=0 is the alignment, not a finding; what "
              "matters is\n   how little survives either side of it." % coherence)


def cmd_decay(args):
    mids = [0.5 * (DECAY_TIMES[i] + DECAY_TIMES[i + 1])
            for i in range(len(DECAY_TIMES) - 1)]
    print("%-38s %5s %6s  %s" % ("file", "tilt", "expo",
                                 " ".join("%6.2f" % t for t in mids)))
    rows = []
    for path in files_in(args):
        x, sr = read_wav(path)
        x = to_mono(x)
        onset = find_onset(x, sr)
        t, c, snr = decay_contour(x, sr, onset)
        if len(c) < 3:
            continue
        tilt = clap_tilt_db(x, sr, onset)
        a = fit_exponent(t, c)
        has_bed = bed_spectrum(x, sr, onset, 4096) is not None
        rows.append((os.path.basename(path), t, c, a, has_bed, tilt))
        print("%-38s %5.0f %6.2f  %s%s" %
              (os.path.basename(path)[:38], tilt, a,
               " ".join("     ." if np.isnan(v) else "%6.0f" % v for v in c),
               "" if has_bed else "  (no bed)"))

    good = [r for r in rows if r[4]]
    if not good:
        return
    print("\n%d of %d files have a pre-onset bed; a dot is a window that did "
          "not stand %g dB\nover it, and is the rain rather than the thunder."
          % (len(good), len(rows), SNR_GATE_DB))
    for label, sel in (("close  (1-2 kHz within 25 dB of the loudest octave)",
                        [r for r in good if r[5] > -25]),
                       ("distant(1-2 kHz further down than that)",
                        [r for r in good if r[5] <= -25])):
        if not sel:
            continue
        n = min(len(r[2]) for r in sel)
        arr = np.array([r[2][:n] for r in sel])
        count = np.sum(~np.isnan(arr), axis=0)
        med = np.full(arr.shape[1], np.nan)
        for i in np.nonzero(count)[0]:
            med[i] = np.nanmedian(arr[:, i])
        t = sel[0][1][:n]
        expo = [r[3] for r in sel if not np.isnan(r[3])]
        print("\n%s -- %d files" % (label, len(sel)))
        print("   t/s   %s" % " ".join("%6.2f" % v for v in t))
        print("   f/Hz  %s" % " ".join("     ." if np.isnan(v) else "%6.0f" % v
                                       for v in med))
        print("   files %s" % " ".join("%6d" % v for v in count))
        if expo:
            print("   median exponent a in centroid ~ t^-a: %.2f  (10th-90th %.2f to %.2f)"
                  % (np.median(expo), np.percentile(expo, 10),
                     np.percentile(expo, 90)))


def cmd_fit(args):
    """The centroid contour over a library, as a C++ initialiser."""
    decays = []
    for path in files_in(args):
        x, sr = read_wav(path)
        x = to_mono(x)
        onset = find_onset(x, sr)
        if bed_spectrum(x, sr, onset, 4096) is None:
            continue
        t, c, snr = decay_contour(x, sr, onset)
        if np.sum(~np.isnan(c)) >= 4:
            decays.append((t, c, clap_tilt_db(x, sr, onset)))
    if not decays:
        print("nothing measurable: no file had both a pre-onset bed and four "
              "windows over it")
        return
    n = min(len(d[1]) for d in decays)
    for label, sel in (("close", [d for d in decays if d[2] > -25]),
                       ("distant", [d for d in decays if d[2] <= -25])):
        if not sel:
            continue
        arr = np.array([d[1][:n] for d in sel])
        count = np.sum(~np.isnan(arr), axis=0)
        med = np.full(n, np.nan)
        for i in np.nonzero(count)[0]:
            med[i] = np.nanmedian(arr[:, i])
        t = sel[0][0][:n]
        keep = ~np.isnan(med)
        print("\n// Spectral centroid of a %s flash: %d recordings, median, Hz,\n"
              "// bed subtracted, windows that stood over their own bed only."
              % (label, len(sel)))
        print("constexpr float kCentroidTimes%s[%d] = {%s};"
              % (label.capitalize(), int(keep.sum()),
                 " ".join("%.3ff," % v for v in t[keep])))
        print("constexpr float kCentroidHz%s[%d]    = {%s};"
              % (label.capitalize(), int(keep.sum()),
                 " ".join("%.1ff," % v for v in med[keep])))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == 'front':
        paths = []
        for a in args:
            if os.path.isdir(a):
                paths.extend(sorted(glob.glob(os.path.join(a, '*.wav'))))
            else:
                paths.append(a)
        cmd_front(paths)
    elif cmd == 'decay':
        cmd_decay(args)
    elif cmd == 'fit':
        cmd_fit(args)
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
