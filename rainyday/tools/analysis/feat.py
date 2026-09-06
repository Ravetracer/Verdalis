"""Acoustic features for matching RainyDay against real rain recordings.

Everything here is computed identically for a reference recording and for a
RainyDay render, so the two are directly comparable. Features are chosen to map
onto parameters the synth actually has: spectral shape -> Drop Pitch / Bed Tone
/ Air, onset statistics -> Density / Clumping, decay -> Drop Decay, band
flatness -> Tonality / Bed Level.
"""
import math

import numpy as np

BAND_EDGES = np.array([50, 100, 200, 400, 800, 1600, 3150, 6300, 12500, 20000.0])
BAND_NAMES = ['50-100', '100-200', '200-400', '400-800', '.8-1.6k',
              '1.6-3.2k', '3.2-6.3k', '6.3-12k', '12-20k']


def _window(sr, at48k):
    """A window of constant *duration*, not of a constant number of samples.

    A fixed 512-sample window is 10.7 ms at 48 kHz but only 5.3 ms at 96 kHz,
    and its bins are then 187 Hz apart, so the 50-100 Hz band contains no bin at
    all. Its temporal flatness then comes out as exactly 1.0 -- a value no
    48 kHz render can ever produce, because a real band never is perfectly flat.
    rain_on_metal is a 96 kHz file, and that phantom band was two thirds of Tin
    Roof's whole temporal-flatness error and half its total distance: the fit
    was spending its effort chasing a number that was not reachable.

    Scaling the window with the sample rate puts a reference and a render on the
    same time grid and the same frequency grid, which is what the rest of this
    file already assumes. Files at 44.1 and 48 kHz keep the window they had.
    """
    return 1 << int(round(math.log2(max(64.0, sr * at48k / 48000.0))))


def _stft(x, sr, win=1024, hop=256):
    n = 1 + max(0, (len(x) - win) // hop)
    if n < 2:
        return np.zeros((1, win // 2 + 1)), np.fft.rfftfreq(win, 1 / sr)
    idx = np.arange(win)[None, :] + hop * np.arange(n)[:, None]
    frames = x[idx] * np.hanning(win)[None, :]
    return np.abs(np.fft.rfft(frames, axis=1)), np.fft.rfftfreq(win, 1 / sr)


def band_spectrum(x, sr, denoise=False):
    """Average power per log band, normalised to 0 dB total, plus centroid.

    `denoise` subtracts a per-band noise floor, estimated as the 10th percentile
    of the short-time energy. Field recordings carry room tone, traffic and
    preamp hiss that RainyDay is not trying to reproduce; without this the fit
    chases the noise floor instead of the rain, worst of all on the sparse drip
    recordings where most frames contain nothing else.
    """
    S, f = _stft(x, sr)
    p2 = S ** 2
    out = []
    for lo, hi in zip(BAND_EDGES[:-1], BAND_EDGES[1:]):
        if lo >= sr / 2:
            out.append(0.0)
            continue
        sel = (f >= lo) & (f < min(hi, sr / 2))
        if not sel.any():
            out.append(0.0)
            continue
        series = p2[:, sel].sum(axis=1)
        if denoise and len(series) > 20:
            floor = np.percentile(series, 10)
            out.append(max(0.0, float((series - floor).clip(0).mean())))
        else:
            out.append(float(series.mean()))
    out = np.array(out)
    total = out.sum() + 1e-30
    frac = out / total
    db = 10 * np.log10(frac + 1e-12)
    # Centroid over the same band grid, in log-frequency, expressed in Hz.
    centres = np.sqrt(BAND_EDGES[:-1] * BAND_EDGES[1:])
    centroid = float(np.exp((frac * np.log(centres)).sum() / (frac.sum() + 1e-30)))
    return db, centroid


def spectral_flatness(x, sr):
    S, f = _stft(x, sr)
    p = (S ** 2).mean(axis=0)
    sel = (f > 200) & (f < min(12000, sr / 2))
    p = p[sel] + 1e-20
    return float(np.exp(np.log(p).mean()) / p.mean())


def frame_flatness(x, sr):
    """Spectral flatness per frame, then averaged -- not flatness of the average.

    This is the difference between noise and a cloud of tones. A few hundred
    randomly pitched droplets per second average out to a perfectly smooth
    spectrum, so long-term flatness cannot tell them from a noise band of the
    same shape: `multiple_water_drops` measures 0.03 flat over its whole length
    but is full of peaks in any single frame. Getting this wrong is what let the
    fit drive Tonality up until the rain buzzed.

    Two guards keep it honest. Only bins within 30 dB of the strongest one are
    counted, so a heavily lowpassed recording is measured where it has content
    rather than in the noise floor above it; and frames below the 30th
    percentile of energy are skipped, so gaps between drops do not count either.
    """
    S, f = _stft(x, sr, win=_window(sr, 1024), hop=int(sr * 512 / 48000))
    band = (f > 200) & (f < min(16000, sr / 2))
    if S.shape[0] < 4 or not band.any():
        return 0.0
    longterm = (S ** 2).mean(axis=0)
    peak = longterm[band].max()
    sel = band & (longterm >= peak * 1.0e-3)
    if sel.sum() < 8:
        return 0.0
    p = S[:, sel] ** 2 + 1e-20
    energy = p.sum(axis=1)
    p = p[energy >= np.percentile(energy, 30)]
    if p.shape[0] < 2:
        return 0.0
    flat = np.exp(np.log(p).mean(axis=1)) / p.mean(axis=1)
    return float(np.median(flat))


def band_envelopes(x, sr, hop_ms=5.0):
    """Short-time energy per octave band -- the basis of the texture features."""
    S, f = _stft(x, sr, win=_window(sr, 512), hop=int(sr * hop_ms / 1000))
    p = S ** 2
    envs = []
    for lo, hi in zip(BAND_EDGES[:-1], BAND_EDGES[1:]):
        sel = (f >= lo) & (f < min(hi, sr / 2))
        if not sel.any() and lo < sr / 2:
            # A band inside the audio bandwidth that catches no bin is a bug in
            # the window choice, not a silent band, and it reads downstream as a
            # perfectly flat one. Never let that pass quietly again.
            raise RuntimeError(
                f'no FFT bin in the {lo}-{hi} Hz band at {sr} Hz with a window of '
                f'{(len(f) - 1) * 2} samples')
        envs.append(p[:, sel].sum(axis=1) if sel.any() else np.zeros(p.shape[0]))
    return np.array(envs)


def temporal_flatness(x, sr):
    """Per band: geometric/arithmetic mean of short-time energy.

    Near 1 means a continuous wash, near 0 means isolated events. This is what
    separates a downpour from a dripping tap without needing onset detection to
    work at either extreme.
    """
    envs = band_envelopes(x, sr)
    out = []
    for e in envs:
        e = e + 1e-20
        out.append(float(np.exp(np.log(e).mean()) / e.mean()))
    return np.array(out)


def onsets(x, sr):
    """Spectral-flux onsets with an adaptive threshold. Returns times in s."""
    hop = int(sr * 0.002)
    S, f = _stft(x, sr, win=_window(sr, 512), hop=hop)
    w = np.clip(f / 1000.0, 0.2, None)          # weight towards the high end
    flux = np.maximum(0.0, np.diff(S * w[None, :], axis=0)).sum(axis=1)
    if len(flux) < 10:
        return np.array([])
    flux /= flux.max() + 1e-20
    k = max(3, int(0.05 / 0.002))
    pad = np.pad(flux, k, mode='edge')
    local = np.array([np.median(pad[i:i + 2 * k + 1]) for i in range(len(flux))])
    thr = local * 2.2 + 0.02
    peaks = []
    for i in range(1, len(flux) - 1):
        if flux[i] > thr[i] and flux[i] >= flux[i - 1] and flux[i] > flux[i + 1]:
            if not peaks or (i - peaks[-1]) * hop / sr > 0.008:
                peaks.append(i)
    return np.array(peaks) * hop / sr


def onset_stats(x, sr, t=None):
    t = onsets(x, sr) if t is None else t
    dur = len(x) / sr
    rate = len(t) / dur if dur > 0 else 0.0
    if len(t) < 8:
        return rate, float('nan')
    ioi = np.diff(t)
    cv = float(ioi.std() / (ioi.mean() + 1e-20))
    return rate, cv


def decay_ms(x, sr, max_events=300, t=None):
    """Median -60 dB decay time of isolated impacts, in ms.

    The onset time from spectral flux is only approximate, so each candidate is
    snapped to the nearest envelope peak first; without that the measurement
    starts on the way up and reports nonsense.
    """
    t = onsets(x, sr) if t is None else t
    if len(t) < 3:
        return float('nan')
    k = max(1, int(0.0005 * sr))
    env = np.convolve(np.abs(x), np.ones(k) / k, mode='same')
    floor = np.median(env) + 1e-9
    out = []
    for i, ti in enumerate(t):
        nxt = (t[i + 1] - ti) if i + 1 < len(t) else 1e9
        if nxt < 0.05:
            continue
        s0 = max(0, int((ti - 0.003) * sr))
        s1 = min(len(env), int((ti + 0.005) * sr))
        if s1 - s0 < 4:
            continue
        pk = s0 + int(np.argmax(env[s0:s1]))
        if env[pk] < floor * 6.0:            # not clearly above the background
            continue
        seg = env[pk:min(len(env), pk + int(0.4 * sr))]
        if len(seg) < int(0.005 * sr):
            continue
        db = 20 * np.log10(np.maximum.accumulate(seg[::-1])[::-1] / env[pk] + 1e-12)
        below = np.where(db < -20)[0]
        if len(below) == 0 or below[0] < 2:
            continue
        out.append(below[0] / sr * 3000.0)
        if len(out) >= max_events:
            break
    return float(np.median(out)) if out else float('nan')


def impulsiveness(x, sr):
    """Per band: 95th percentile minus median of short-time level, in dB.

    A continuous wash sits near 3-6 dB; isolated impacts against a quiet
    background run to 20 dB and beyond. Unlike onset counting this stays
    meaningful when the impacts are too dense to separate.
    """
    envs = band_envelopes(x, sr)
    out = []
    for e in envs:
        db = 10 * np.log10(e + 1e-20)
        out.append(float(np.percentile(db, 95) - np.percentile(db, 50)))
    return np.array(out)


def modulation_depth(x, sr):
    """Standard deviation, in dB, of the slow broadband envelope (0.2-5 Hz).

    This is how much the rain surges and lulls: Clumping and Bed Drift.
    """
    hop = int(sr * 0.01)
    if hop < 1 or len(x) < hop * 20:
        return float('nan')
    n = len(x) // hop
    e = (x[:n * hop].reshape(n, hop) ** 2).mean(axis=1)
    db = 10 * np.log10(e + 1e-12)
    db = db - db.mean()
    fs = 1.0 / 0.01
    F = np.abs(np.fft.rfft(db * np.hanning(len(db))))
    fr = np.fft.rfftfreq(len(db), 1 / fs)
    sel = (fr > 0.2) & (fr < 5.0)
    if not sel.any():
        return float('nan')
    return float(np.sqrt((F[sel] ** 2).sum()) / len(db) * 2)


def crest_db(x, sr=48000, window_s=4.0):
    """Median peak-to-RMS over fixed windows, in dB.

    Taken over the whole signal this would be useless here: the peak of a
    100-second recording is drawn from far more impacts than the peak of a
    7-second render, so the recording always wins regardless of texture. Fixed
    windows make the number depend on the sound rather than on its length.
    """
    n = int(window_s * sr)
    if n < 1 or len(x) < n:
        pk = np.abs(x).max()
        rms = np.sqrt((x ** 2).mean())
        return float(20 * np.log10(pk / (rms + 1e-20) + 1e-20))
    out = []
    for i in range(0, len(x) - n + 1, n):
        seg = x[i:i + n]
        rms = np.sqrt((seg ** 2).mean())
        if rms < 1e-9:
            continue
        out.append(20 * np.log10(np.abs(seg).max() / rms))
    return float(np.median(out)) if out else 0.0


def sparse_events(x, sr, holdoff=0.06, ratio=5.0, floor_mult=8.0):
    """Onset times (in samples) of isolated events, plus the envelope and floor.

    The spectral-flux detector above is tuned for rain, where onsets are dense
    and an 8 ms hold-off is right. On a cave drip it fires two or three times
    per drop -- once on the drop and again on each slap of the room -- and put
    0.93 events a second on a recording that has 0.34. This one works on the
    broadband envelope instead: an event is a rise of `ratio` over the loudest
    thing in the preceding 30 ms, clearly above the floor, and nothing counts for
    `holdoff` after it, which is longer than any early reflection and shorter
    than any plausible drip. Dense rain has no such rises and yields nothing,
    which is the right answer for it.
    """
    k = max(1, int(sr * 0.001))
    e = np.convolve(np.abs(x), np.ones(k) / k, mode='same')
    floor = np.percentile(e, 10) + 1e-9
    hop = max(1, int(sr * 0.0005))
    n = len(e) // hop
    if n < 200:
        return np.array([], dtype=int), e, floor
    E = e[:n * hop].reshape(n, hop).max(axis=1)
    pre, gap = 60, 8                      # 30 ms window, ending 4 ms before now
    win = np.lib.stride_tricks.sliding_window_view(E, pre)
    premax = win.max(axis=1)              # premax[i] covers E[i .. i+pre-1]
    # Candidate at hop i compares against the window ending at i - gap.
    idx = np.arange(pre + gap, n)
    ref = premax[idx - gap - pre]
    cand = idx[(E[idx] > ratio * ref) & (E[idx] > floor_mult * floor)]
    out = []
    last = -1e9
    hold = int(holdoff * sr)
    look = int(0.008 * sr)
    for h in cand:
        i = h * hop
        if i - last < hold:
            continue
        pk = i + int(np.argmax(e[i:i + look]))
        out.append(pk)
        last = pk
    return np.array(out, dtype=int), e, floor


MIN_ISOLATED_EVENTS = 8
ISOLATION_DB = 12.0
SPARSE_DROP_DB = 12.0
SPARSE_QUIET_FRACTION = 0.5


def room_stats(x, sr, min_gap=0.5, max_events=80):
    """Event rate, late reverberation time and direct-to-late ratio.

    These are what separate a drop from the room it falls in, which none of the
    features above can do: a long droplet ring and a long reverb tail have the
    same spectrum and the same temporal flatness. Fitting Cave Drips without
    them put the cavern inside the droplet.

    Measured only on events with at least `min_gap` of clear air after them, so
    that what follows the direct sound is the room and nothing else. The late
    RT60 is a Schroeder backward integration of the energy from 50 ms after the
    peak, noise floor subtracted, fitted between -5 and -25 dB. The direct-to-
    late ratio is the first 20 ms against everything after 50 ms. Both are NaN
    where a recording has no isolated events, and the objective then skips them.
    """
    t, _, _ = sparse_events(x, sr)
    dur = len(x) / sr
    rate = len(t) / dur if dur > 0 else 0.0
    hop = int(0.02 * sr)
    n = len(x) // hop
    fe = (x[:n * hop].reshape(n, hop) ** 2).mean(axis=1) if n > 10 else np.zeros(1)
    nf = float(np.percentile(fe, 10))
    # Sparse means most of the time nothing much is happening: at least half
    # of the 20 ms frames sit more than 12 dB under the loudest twentieth. A
    # cave drip passes with room to spare, a dripping tap passes, and rain of
    # any kind fails, gusts and lulls included -- which is the point, because
    # rain also throws up the odd event this detector accepts, and the rhythm
    # and room measured on those would be measured on the rain.
    quiet = float(np.mean(fe < np.percentile(fe, 95) * 10 ** (-SPARSE_DROP_DB / 10)))
    sparse = quiet >= SPARSE_QUIET_FRACTION and rate >= 0.05
    if not sparse or len(t) < 3:
        return rate, float('nan'), float('nan'), sparse
    d20, d50 = int(0.02 * sr), int(0.05 * sr)
    a300, a500 = int(0.3 * sr), int(0.5 * sr)
    rts, dls = [], []
    for j, pk in enumerate(t):
        nxt = t[j + 1] if j + 1 < len(t) else len(x)
        if (nxt - pk) / sr < min_gap:
            continue
        end = min(nxt - int(0.02 * sr), pk + int(2.0 * sr), len(x))
        seg = x[pk:end] ** 2
        if len(seg) < a500 + int(0.05 * sr):
            continue
        # Clear air, not merely no other detected event: what is left 300 to
        # 500 ms after the peak has to be well below the peak itself. A cave
        # tail is 20 dB down by then; steady rain is 3 dB down, because it is
        # still raining, and a "room" measured on it would be the rain.
        peak = seg[:int(0.005 * sr)].max()
        later = seg[a300:a500].mean()
        if later > peak * 10 ** (-ISOLATION_DB / 10):
            continue
        direct = seg[:d20].sum()
        late = (seg[d50:] - nf).clip(0)
        dls.append(10 * np.log10((direct + 1e-20) / (late.sum() + 1e-20)))
        edc = np.cumsum(late[::-1])[::-1]
        edc = 10 * np.log10(edc / (edc[0] + 1e-30) + 1e-30)
        i0 = np.where(edc <= -5)[0]
        i1 = np.where(edc <= -25)[0]
        if len(i0) and len(i1) and i1[0] > i0[0] + int(0.01 * sr):
            tt = np.arange(i0[0], i1[0]) / sr
            slope = np.polyfit(tt, edc[i0[0]:i1[0]], 1)[0]
            if slope < 0:
                rts.append(-60.0 / slope)
        if len(dls) >= max_events:
            break
    # Dense rain throws up a handful of spurious events -- a gust, a cluster --
    # and a room measured on three of those is nonsense that then scores as if
    # it meant something. Fewer than eight isolated events is not a sparse
    # recording, and the room is left unmeasured.
    if len(dls) < MIN_ISOLATED_EVENTS:
        return rate, float('nan'), float('nan'), sparse
    late_rt = float(np.median(rts)) if rts else float('nan')
    direct_late = float(np.median(dls))
    return rate, late_rt, direct_late, sparse


def extract(x, sr, label='', denoise=False):
    x = x - x.mean()
    db, centroid = band_spectrum(x, sr, denoise=denoise)
    # The spectral-flux onsets are the most expensive thing here and two
    # features want them, so they are found once.
    t = onsets(x, sr)
    rate, cv = onset_stats(x, sr, t)
    event_rate, late_rt, direct_late, sparse = room_stats(x, sr)
    return {
        'label': label,
        'sr': sr,
        'bands': db,
        'centroid': centroid,
        'flatness': spectral_flatness(x, sr),
        'fflat': frame_flatness(x, sr),
        'tflat': temporal_flatness(x, sr),
        'imp': impulsiveness(x, sr),
        'onset_rate': rate,
        'ioi_cv': cv,
        'decay_ms': decay_ms(x, sr, t=t),
        'mod_db': modulation_depth(x, sr),
        'crest': crest_db(x, sr),
        'event_rate': event_rate,
        'late_rt': late_rt,
        'direct_late': direct_late,
        # 1.0 or 0.0 rather than a bool, so a composite of several recordings
        # averages to the fraction of them that are sparse.
        'sparse': 1.0 if sparse else 0.0,
    }


def report(fs):
    hdr = f"{'file':30s} {'cent':>6s} {'flat':>5s} {'ons/s':>6s} {'cv':>5s} {'dec':>6s} {'mod':>5s} {'crest':>6s}  " + ' '.join(f'{n:>7s}' for n in BAND_NAMES)
    print(hdr)
    print('-' * len(hdr))
    for f in fs:
        b = ' '.join(f'{v:7.1f}' for v in f['bands'])
        print(f"{f['label']:30s} {f['centroid']:6.0f} {f['flatness']:5.3f} "
              f"{f['onset_rate']:6.1f} {f['ioi_cv']:5.2f} {f['decay_ms']:6.0f} "
              f"{f['mod_db']:5.2f} {f['crest']:6.1f}  {b}")
