"""Syllable segmentation and measurement, shared by the ChirpParade scripts.

A bird recording is not a texture: it is a sequence of discrete syllables with
silence between them, and nearly every number ChirpParade is fitted to is a
property of one syllable. So the segmenter comes first and everything else
reads its output.

Most of the reference library was recorded outdoors, which means every file has
a bed of wind, traffic or tape hiss under the birds. Two things keep that out of
the measurements:

  * a per-bin noise floor taken as the 15th percentile of magnitude over the
    whole file. A bird is present in a small fraction of the frames, so a low
    percentile is the background and nothing else. It is subtracted before
    anything is measured.

  * a tonality test. Bird syllables are narrow-band: the loudest bin of a
    syllable frame stands far above the mean of its band. Broadband noise, however
    loud, does not. A frame counts as voiced only if it passes both the level and
    the peakiness test, which is what lets a chirp be found under a lawnmower.

Frequencies are read off the tallest denoised peak with a parabolic fit over the
three bins around it, so the pitch resolution is far better than the 47 Hz the
bin spacing suggests.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np
import wavio

# Analysis geometry. 1024 points at 48 kHz is a 21 ms window, which is short
# enough to sit inside a 50 ms syllable and long enough to resolve a 400 Hz
# crow fundamental. The hop is a quarter of it.
NFFT = 1024
HOP = 256

# The band birds actually occupy. Below 250 Hz is wind and traffic; above
# 12 kHz there is nothing but hiss, and half the library is 24 kHz material
# that has no content there at all.
BAND_LO = 250.0
BAND_HI = 12000.0

# A frame is voiced if it is this far above the file's own quiet level and its
# spectrum is this peaky. Both thresholds were set by running the segmenter over
# the four files whose names promise exactly one bird -- single_bird_chirp,
# whistling_single_robin, crow_call_single, green_woodpecker_chirp -- and
# choosing the pair that finds their syllables and nothing else.
LEVEL_ABOVE_FLOOR_DB = 12.0
PEAKINESS_DB = 12.0

# A frame also has to be within this much of the loudest frame in the file. The
# floor test alone is relative to the quietest fifth of the file, which works
# for a field recording and fails for a clean synthetic one: a render holding a
# single chirp in four seconds of digital silence has a "quiet level" near
# nothing, so the whole 60 dB decay tail passed the floor test and one 200 ms
# chirp was measured as a 603 ms syllable. Nothing 45 dB below the loudest thing
# in a file is a syllable.
DYNAMIC_RANGE_DB = 45.0

MIN_SYLLABLE_SEC = 0.012
MIN_GAP_SEC = 0.010

# The pitch range of a syllable is measured only over the frames within this
# much of its own peak. A syllable's onset and offset ramps are voiced but
# nearly silent, and the tracked partial wanders freely there -- on a clean
# synthetic render, where no background masks the ramps, a syllable with no
# sweep at all measured 1.5 octaves of it. The loud part is also the only part
# a listener hears a pitch in.
PITCH_WINDOW_DB = 12.0


def load_mono(path, max_seconds=120.0):
    """Mono, and at most the first two minutes: some references run ten."""
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    n = int(max_seconds * sr)
    if len(m) > n:
        m = m[:n]
    return m, sr, x[:n]


def stft(m, sr):
    win = np.hanning(NFFT)
    frames = 1 + (len(m) - NFFT) // HOP if len(m) >= NFFT else 0
    if frames <= 0:
        return None, None, None
    idx = np.arange(NFFT)[None, :] + HOP * np.arange(frames)[:, None]
    mag = np.abs(np.fft.rfft(m[idx] * win, axis=1))
    freqs = np.fft.rfftfreq(NFFT, 1.0 / sr)
    times = HOP * np.arange(frames) / float(sr)
    return mag, freqs, times


def denoise(mag):
    """Subtract the per-bin background, estimated as a low percentile in time."""
    floor = np.percentile(mag, 15, axis=0)
    return np.maximum(mag - 1.5 * floor[None, :], 0.0), floor


def frame_features(mag, freqs):
    """Per-frame band level, peakiness, dominant partial and centroid.

    Two pitches are measured, not one, because the library holds two families.
    The fundamental is not measured here: it is a property of a syllable rather
    than of a frame, and finding it needs the whole syllable's loudest spectra.
    See fundamental() below.
    """
    sel = (freqs >= BAND_LO) & (freqs <= BAND_HI)
    band = mag[:, sel]
    bfreqs = freqs[sel]
    power = band ** 2
    total = power.sum(axis=1)
    level = 10.0 * np.log10(np.maximum(total, 1e-20))

    peak = np.argmax(band, axis=1)
    peakv = band[np.arange(len(peak)), peak]
    mean = np.maximum(band.mean(axis=1), 1e-20)
    peakiness = 20.0 * np.log10(np.maximum(peakv, 1e-20) / mean)

    # Parabolic interpolation of the peak over its two neighbours, in log
    # magnitude -- the usual sub-bin estimator for a windowed sinusoid.
    df = bfreqs[1] - bfreqs[0]
    fpeak = bfreqs[peak] + interp_shift(band, peak) * df

    centroid = (power * bfreqs[None, :]).sum(axis=1) / np.maximum(total, 1e-20)
    return level, peakiness, fpeak, centroid, total


def interp_shift(band, peak):
    lo = np.clip(peak - 1, 0, band.shape[1] - 1)
    hi = np.clip(peak + 1, 0, band.shape[1] - 1)
    i = np.arange(len(peak))
    a = np.log(np.maximum(band[i, lo], 1e-20))
    b = np.log(np.maximum(band[i, peak], 1e-20))
    c = np.log(np.maximum(band[i, hi], 1e-20))
    denom = a - 2.0 * b + c
    shift = np.where(np.abs(denom) > 1e-12, 0.5 * (a - c) / np.where(denom == 0, 1.0, denom), 0.0)
    return np.clip(shift, -0.5, 0.5)


# Candidate fundamentals are the dominant partial divided by 1..6. Nothing in
# the library sings below 200 Hz, so a divisor that lands under that is not
# considered.
F0_LO = 200.0
MAX_DIVISOR = 6
COMB_HALFWIDTH = 0.06  # +-6 % around each harmonic
HARM_FLOOR_DB = -24.0


def fundamental(spec, freqs, fpeak):
    """The fundamental of one frame, and how harmonic that frame is.

    A harmonic product spectrum was tried first and is the wrong tool here.
    It always names some fundamental, including for a pure whistle that has
    none -- the robin's 8 kHz whistle came out at 2.8 kHz, a third of it being
    a bin like any other -- and it has no way to say that a raven is not
    harmonic at all.

    So instead: the dominant partial is divided by 1 through 6, each candidate
    is scored by the share of band energy that falls inside a comb of its
    harmonics, and the lower candidates carry a penalty, since the comb of a
    subharmonic always contains the comb above it. Beside the fundamental this
    returns the comb's own share of the energy, which is the number that
    separates the three voices the plugin has to make: a whistle keeps almost
    everything in one harmonic, a harmonic stack spreads it over a dozen, and a
    corvid keeps less than half of it in the comb at all.
    """
    total = float((spec ** 2).sum())
    if total <= 0.0 or fpeak <= 0.0:
        return 0.0, 0.0, 0
    best = (None, -1e9)
    for n in range(1, MAX_DIVISOR + 1):
        f0 = fpeak / n
        if f0 < F0_LO:
            break
        comb = 0.0
        count = 0
        peaks = []
        for k in range(1, 17):
            f = f0 * k
            if f > freqs[-1]:
                break
            sel = (freqs > f * (1.0 - COMB_HALFWIDTH)) & (freqs < f * (1.0 + COMB_HALFWIDTH))
            if not sel.any():
                continue
            e = float((spec[sel] ** 2).sum())
            comb += e
            peaks.append(spec[sel].max())
        if not peaks:
            continue
        ref = max(peaks)
        db = 20.0 * np.log10(np.maximum(np.array(peaks), 1e-20) / max(ref, 1e-20))
        count = int(np.sum(db > HARM_FLOOR_DB))
        frac = comb / total
        score = frac - 0.07 * (n - 1)
        if score > best[1]:
            best = ((f0, frac, count), score)
    if best[0] is None:
        return fpeak, 0.0, 1
    return best[0]


def voiced_mask(level, peakiness):
    quiet = np.percentile(level, 10)
    loud = np.percentile(level, 99.5)
    strong = ((level > quiet + LEVEL_ABOVE_FLOOR_DB) & (level > loud - DYNAMIC_RANGE_DB) &
              (peakiness > PEAKINESS_DB))
    weak = ((level > quiet + LEVEL_ABOVE_FLOOR_DB * 0.55) &
            (level > loud - DYNAMIC_RANGE_DB - 10.0) & (peakiness > PEAKINESS_DB * 0.7))
    # Hysteresis: a syllable is grown out from every strong frame through the
    # weak ones around it, so a chirp that dips mid-way stays one syllable.
    mask = strong.copy()
    n = len(mask)
    i = 0
    while i < n:
        if strong[i]:
            j = i
            while j + 1 < n and weak[j + 1]:
                j += 1
            k = i
            while k - 1 >= 0 and weak[k - 1]:
                k -= 1
            mask[k:j + 1] = True
            i = j + 1
        else:
            i += 1
    return mask


def runs(mask, sr):
    """Voiced runs, merged across gaps shorter than MIN_GAP_SEC."""
    frame_sec = HOP / float(sr)
    out = []
    i = 0
    n = len(mask)
    while i < n:
        if not mask[i]:
            i += 1
            continue
        j = i
        while j + 1 < n and mask[j + 1]:
            j += 1
        out.append([i, j])
        i = j + 1
    merged = []
    for r in out:
        if merged and (r[0] - merged[-1][1] - 1) * frame_sec < MIN_GAP_SEC:
            merged[-1][1] = r[1]
        else:
            merged.append(r)
    return [(a, b) for a, b in merged if (b - a + 1) * frame_sec >= MIN_SYLLABLE_SEC]


# A run is cut at a valley this far below the quieter of the two peaks it sits
# between. A green woodpecker's laugh is a dozen notes with no silence between
# them, so a level threshold alone finds one syllable where there are twelve;
# only the shape of the envelope separates them.
VALLEY_RATIO = 0.45


def split_run(a, b, energy, frame_sec):
    """Cut one voiced run into syllables at the valleys of its envelope."""
    seg = energy[a:b + 1]
    n = len(seg)
    minframes = max(2, int(MIN_SYLLABLE_SEC / frame_sec))
    if n < 2 * minframes + 1:
        return [(a, b)]
    # Peaks first, then the deepest valley between each neighbouring pair.
    peaks = [i for i in range(1, n - 1) if seg[i] >= seg[i - 1] and seg[i] > seg[i + 1]]
    peaks = [p for p in peaks if seg[p] > 0.15 * seg.max()]
    if len(peaks) < 2:
        return [(a, b)]
    cuts = []
    for p, q in zip(peaks, peaks[1:]):
        if q - p < minframes:
            continue
        v = p + int(np.argmin(seg[p:q + 1]))
        if seg[v] < VALLEY_RATIO * min(seg[p], seg[q]):
            cuts.append(v)
    if not cuts:
        return [(a, b)]
    out = []
    start = a
    for c in cuts:
        if c + a - start + 1 >= minframes:
            out.append((start, a + c))
            start = a + c + 1
    if b - start + 1 >= minframes:
        out.append((start, b))
    elif out:
        out[-1] = (out[-1][0], b)
    return out or [(a, b)]


def modulation_rate(env, sr, lo_hz, hi_hz, min_prominence=4.0):
    """Periodic modulation of a time-domain envelope, by its own spectrum.

    Autocorrelation over the STFT frame series was tried first and does not
    work here: a 112 ms syllable is twenty frames, and the autocorrelation of
    twenty noisy samples peaks at its shortest allowed lag almost every time.
    The first version of this reported a 37-43 Hz trill for a fifth of the
    library, which is exactly the frame rate divided by five.

    So the envelope is taken in the time domain instead, detrended, and its
    spectrum searched. A rate is only returned when its peak stands
    `min_prominence` times above the median of the search band, which is what
    stops a syllable that merely swells being called a trill.
    """
    n = len(env)
    if n < 64:
        return 0.0, 0.0
    v = env - np.polyval(np.polyfit(np.arange(n), env, 2), np.arange(n))
    v = v * np.hanning(n)
    spec = np.abs(np.fft.rfft(v))
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    sel = (freqs >= lo_hz) & (freqs <= hi_hz)
    if sel.sum() < 4:
        return 0.0, 0.0
    inband = spec[sel]
    k = int(np.argmax(inband))
    med = float(np.median(inband))
    prom = float(inband[k] / max(med, 1e-20))
    if prom < min_prominence:
        return 0.0, prom
    return float(freqs[sel][k]), prom


def dominant_period(v, frame_sec, lo_hz, hi_hz):
    """Modulation rate of a short series, by autocorrelation. None if flat."""
    v = np.asarray(v, float)
    if len(v) < 6:
        return None, 0.0
    v = v - v.mean()
    if np.allclose(v, 0.0):
        return None, 0.0
    ac = np.correlate(v, v, mode="full")[len(v) - 1:]
    ac /= ac[0] if ac[0] != 0 else 1.0
    lags = np.arange(len(ac))
    with np.errstate(divide="ignore"):
        rate = np.where(lags > 0, 1.0 / np.maximum(lags * frame_sec, 1e-9), 0.0)
    # Lags below three frames are not a modulation rate, they are the
    # analysis frame rate: at a 5.3 ms hop, lag 2 is 94 Hz and every noisy
    # contour "peaks" there. The first version of this reported exactly that
    # for two thirds of the library.
    sel = (rate >= lo_hz) & (rate <= hi_hz) & (lags >= 5) & (lags <= len(v) // 2)
    if not sel.any():
        return None, 0.0
    k = np.argmax(np.where(sel, ac, -2.0))
    return float(rate[k]), float(ac[k])


class Syllable(object):
    __slots__ = ("t0", "t1", "dur", "f_start", "f_end", "f_min", "f_max", "f_med",
                 "f0_med", "f_peak", "harm_fit", "sweep_oct", "sweep_rate", "shape", "fm_rate",
                 "fm_depth_oct", "am_rate", "am_depth", "attack", "harmonics",
                 "hnr", "flatness", "centroid", "peakiness", "level", "voiced",
                 "rise", "fall", "turns")


def track_partial(band, bfreqs, a, b):
    """Follow one partial across a syllable instead of taking the tallest bin.

    Argmax per frame jumps between harmonics: a green woodpecker's call came
    out sweeping from 1.2 kHz to 7.1 kHz because two frames in the middle were
    loudest at the third harmonic. Tracking from the loudest frame outwards,
    and refusing a step of more than half an octave, is what makes the sweep
    measurement mean anything.
    """
    seg = band[a:b + 1]
    n = seg.shape[0]
    df = bfreqs[1] - bfreqs[0]
    energy = (seg ** 2).sum(axis=1)
    anchor = int(np.argmax(energy))

    idx = np.zeros(n, dtype=int)
    idx[anchor] = int(np.argmax(seg[anchor]))
    max_step = 0.5  # octaves per frame

    def step(order):
        prev = idx[anchor]
        for i in order:
            row = seg[i]
            fprev = max(bfreqs[prev], 1.0)
            lo = np.searchsorted(bfreqs, fprev / 2.0 ** max_step)
            hi = np.searchsorted(bfreqs, fprev * 2.0 ** max_step)
            hi = max(hi, lo + 1)
            local = int(lo + np.argmax(row[lo:hi]))
            # A frame with nothing in the window keeps the previous partial
            # rather than inventing a jump.
            if row[local] < 0.05 * max(row.max(), 1e-20):
                local = prev
            idx[i] = local
            prev = local

    step(range(anchor + 1, n))
    step(range(anchor - 1, -1, -1))
    return bfreqs[idx] + interp_shift(seg, idx) * df


def contour_turns(pitch):
    """How much of a gesture cycle the contour traces, from its inflections.

    The tension gesture is one sinusoid sin(2*pi*T*s + phase) over the
    syllable, so T is fixed by how many times the contour turns: a monotone
    sweep is a quarter turn, one turning point is a half, and every further
    turning point is another half. Counting turning points therefore measures
    the engine's `Turns` parameter directly rather than leaving it to be
    chosen. Smoothed over five frames first, or the jitter is counted as twenty
    turns.
    """
    p = np.asarray(pitch, float)
    if len(p) < 6:
        return 0.25
    k = 5
    sm = np.convolve(p, np.ones(k) / k, mode="valid")
    if len(sm) < 4:
        return 0.25
    d = np.diff(sm)
    span = np.log2(max(sm.max(), 1.0) / max(sm.min(), 1.0))
    if span < 0.05:
        return 0.25
    # Turning points, ignoring ones that only move a fiftieth of the span.
    floor = 0.02 * span * np.log(2.0) * sm.mean()
    sig = np.where(np.abs(d) > floor, np.sign(d), 0.0)
    sig = sig[sig != 0.0]
    if len(sig) < 2:
        return 0.25
    turns = int(np.sum(sig[1:] != sig[:-1]))
    return max(0.25, 0.5 * turns)


def classify(pitch):
    """The contour shape, the way a sonogram reader would name it.

    The Mindlin model makes this a consequence of one number: the phase
    between the pressure gesture and the tension gesture. In phase gives a
    hump that rises and falls with the level; a quarter turn out of phase
    gives the U and the inverted U. So these six names are what the phase
    parameter has to be able to reach.
    """
    if len(pitch) < 4:
        return "click"
    lg = np.log2(np.maximum(pitch, 1.0))
    n = len(lg)
    a, b = lg[:max(1, n // 4)].mean(), lg[-max(1, n // 4):].mean()
    mid = lg[n // 4:max(n // 4 + 1, 3 * n // 4)]
    m = mid.mean()
    span = lg.max() - lg.min()
    if span < 0.08:
        return "flat"
    net = b - a
    if m > max(a, b) + 0.5 * span * 0.35:
        return "arch"
    if m < min(a, b) - 0.5 * span * 0.35:
        return "valley"
    if net > 0.10:
        return "up"
    if net < -0.10:
        return "down"
    return "wobble"


def envelope(m, sr, smooth_sec=0.0008):
    """Rectified and smoothed to 0.8 ms, for attack and decay times.

    The STFT hop is 5.3 ms, which is the same order as the attack of a chirp:
    read off the spectrogram, every syllable in the library would appear to
    start in one or two frames. So the envelope is taken in the time domain.
    """
    k = max(1, int(smooth_sec * sr))
    kernel = np.ones(k) / k
    return np.convolve(np.abs(m), kernel, mode="same")


def edge_times(env, sr, lo=0.1, hi=0.9):
    """Rise and fall of one syllable's envelope, between lo and hi of its peak."""
    if len(env) < 4:
        return 0.0, 0.0
    peak = env.max()
    if peak <= 0.0:
        return 0.0, 0.0
    k = int(np.argmax(env))
    up = env[:k + 1]
    dn = env[k:]

    def span(v, forward):
        a = np.nonzero(v > lo * peak)[0]
        b = np.nonzero(v > hi * peak)[0]
        if not len(a) or not len(b):
            return 0.0
        i, j = (a[0], b[0]) if forward else (a[-1], b[-1])
        return abs(j - i) / float(sr)

    return span(up, True), span(dn, False)


def analyse(path, max_seconds=120.0):
    """Every syllable in one reference, measured."""
    m, sr, _ = load_mono(path, max_seconds)
    mag, freqs, times = stft(m, sr)
    if mag is None:
        return None
    dn, _ = denoise(mag)
    level, peakiness, fpeak, centroid, total = frame_features(dn, freqs)
    mask = voiced_mask(level, peakiness)
    frame_sec = HOP / float(sr)

    sel = (freqs >= BAND_LO) & (freqs <= BAND_HI)
    band = dn[:, sel]
    bfreqs = freqs[sel]
    env = envelope(m, sr)

    segments = []
    for a, b in runs(mask, sr):
        segments.extend(split_run(a, b, total, frame_sec))

    out = []
    for a, b in segments:
        p = track_partial(band, bfreqs, a, b)
        amp = np.sqrt(np.maximum(total[a:b + 1], 0.0))
        s = Syllable()
        # A frame is placed at the centre of its window and given the hop as
        # its extent, so that two runs one silent frame apart have a gap of one
        # hop rather than a negative one. Taking t1 as the end of the last
        # window instead made adjacent syllables overlap by 16 ms, and the
        # median within-phrase gap came out at -17 ms.
        half = 0.5 * NFFT / float(sr)
        s.t0 = times[a] + half - 0.5 * HOP / float(sr)
        s.t1 = times[b] + half + 0.5 * HOP / float(sr)
        s.dur = (b - a + 1) * HOP / float(sr)
        # Only the audible part of the syllable carries a pitch; see
        # PITCH_WINDOW_DB.
        loud = amp >= amp.max() * 10.0 ** (-PITCH_WINDOW_DB / 20.0)
        if loud.sum() < 3:
            loud = np.ones(len(amp), dtype=bool)
        pl = p[loud]
        head = max(1, len(pl) // 5)
        s.f_start = float(np.median(pl[:head]))
        s.f_end = float(np.median(pl[-head:]))
        s.f_min = float(pl.min())
        s.f_max = float(pl.max())
        s.f_med = float(np.median(pl))
        # The tracked partial at the loudest frame. This is the statistic the
        # engine's `Pitch` parameter is defined against -- the pressure gesture
        # peaks at 0.37 of the syllable, so the pitch of a swept syllable at its
        # loudest is not the median over the whole of it, and comparing the two
        # made the engine look 10 % flat when it was exact.
        s.f_peak = float(p[int(np.argmax(amp))])
        s.sweep_oct = float(np.log2(max(s.f_max, 1.0) / max(s.f_min, 1.0)))
        s.sweep_rate = s.sweep_oct / max(s.dur, 1e-3)
        s.shape = classify(pl)
        s.turns = contour_turns(pl)
        s.level = float(level[a:b + 1].max())
        s.peakiness = float(np.median(peakiness[a:b + 1]))
        s.voiced = b - a + 1

        lg = np.log2(np.maximum(pl, 1.0))
        # The trill: a periodic wobble of the contour once its overall sweep is
        # taken out. Capped at 45 Hz, above which a "modulation" is really the
        # analysis frame rate rather than anything the bird did.
        trend = np.polyval(np.polyfit(np.arange(len(lg)), lg, 1), np.arange(len(lg))) \
            if len(lg) >= 3 else lg
        resid = lg - trend
        fm, conf = dominant_period(resid, frame_sec, 5.0, 30.0)
        s.fm_rate = fm if (fm and conf > 0.5) else 0.0
        s.fm_depth_oct = float(0.5 * (resid.max() - resid.min())) if len(resid) else 0.0

        s.am_rate, _ = modulation_rate(env[int(s.t0 * sr):min(len(env), int(s.t1 * sr))],
                                       sr, 8.0, 150.0)
        # Depth over the sustained middle only: taken across the whole
        # syllable it is the attack and the release, and comes out at 1.0 for
        # everything.
        lo = int(0.25 * len(amp))
        hi = max(lo + 1, int(0.75 * len(amp)))
        mid = amp[lo:hi]
        s.am_depth = float((mid.max() - mid.min()) / max(mid.max(), 1e-12))

        k = int(np.argmax(amp))
        s.attack = max(frame_sec, k * frame_sec)
        i0 = int(s.t0 * sr)
        i1 = min(len(env), int(s.t1 * sr))
        s.rise, s.fall = edge_times(env[i0:i1], sr)
        spec = band[a + k]
        # Measured on the three loudest frames and taken as their median: one
        # frame can lose a harmonic to a null, and a raven came out with one
        # harmonic in one frame and nine in the next.
        order = np.argsort(amp)[::-1][:3]
        got = [fundamental(band[a + int(j)], bfreqs, p[int(j)]) for j in order]
        s.f0_med = float(np.median([g[0] for g in got]))
        s.harm_fit = float(np.median([g[1] for g in got]))
        s.harmonics = int(np.median([g[2] for g in got]))
        s.centroid = float(np.median(centroid[a:b + 1]))
        # Harmonic-to-noise: the tracked partial's own third-octave against the
        # rest of the band, which is what "tonal" means for a syllable.
        fk = p[k]
        pk = (bfreqs > fk / 2 ** (1 / 6.0)) & (bfreqs < fk * 2 ** (1 / 6.0))
        inb = float((spec[pk] ** 2).sum()) if pk.any() else 0.0
        rest = float((spec[~pk] ** 2).sum())
        s.hnr = 10.0 * np.log10(max(inb, 1e-20) / max(rest, 1e-20))
        # Spectral flatness of the syllable's own band: the geometric mean of
        # the power over its arithmetic mean. A whistle is near zero, a rattle
        # or a rough corvid call approaches one. This is the measurement that
        # says a voice needs noise in it rather than more harmonics.
        # Floored 60 dB below the frame's own peak: the spectrum has been
        # noise-subtracted and holds exact zeros, and a geometric mean over
        # those is zero for everything.
        pw = np.maximum(spec ** 2, (1.0e-3 * max(spec.max(), 1e-20)) ** 2)
        s.flatness = float(np.exp(np.log(pw).mean()) / pw.mean())
        out.append(s)

    return {
        "sr": sr,
        "seconds": len(m) / float(sr),
        "syllables": out,
        "voiced_share": float(mask.mean()),
        "frame_sec": frame_sec,
    }


def gaps(sylls):
    return [sylls[i + 1].t0 - sylls[i].t1 for i in range(len(sylls) - 1)]


def summarise(vals, fmt="%.2f"):
    if not len(vals):
        return "-"
    v = np.asarray(vals, float)
    return (fmt + " .. " + fmt + ", med " + fmt) % (v.min(), v.max(), np.median(v))
