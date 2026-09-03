"""Acoustic features for matching RainyDay against real rain recordings.

Everything here is computed identically for a reference recording and for a
RainyDay render, so the two are directly comparable. Features are chosen to map
onto parameters the synth actually has: spectral shape -> Drop Pitch / Bed Tone
/ Air, onset statistics -> Density / Clumping, decay -> Drop Decay, band
flatness -> Tonality / Bed Level.
"""
import numpy as np

BAND_EDGES = np.array([50, 100, 200, 400, 800, 1600, 3150, 6300, 12500, 20000.0])
BAND_NAMES = ['50-100', '100-200', '200-400', '400-800', '.8-1.6k',
              '1.6-3.2k', '3.2-6.3k', '6.3-12k', '12-20k']


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
    S, f = _stft(x, sr, win=1024, hop=512)
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
    S, f = _stft(x, sr, win=512, hop=int(sr * hop_ms / 1000))
    p = S ** 2
    envs = []
    for lo, hi in zip(BAND_EDGES[:-1], BAND_EDGES[1:]):
        sel = (f >= lo) & (f < min(hi, sr / 2))
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
    S, f = _stft(x, sr, win=512, hop=hop)
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


def onset_stats(x, sr):
    t = onsets(x, sr)
    dur = len(x) / sr
    rate = len(t) / dur if dur > 0 else 0.0
    if len(t) < 8:
        return rate, float('nan')
    ioi = np.diff(t)
    cv = float(ioi.std() / (ioi.mean() + 1e-20))
    return rate, cv


def decay_ms(x, sr, max_events=300):
    """Median -60 dB decay time of isolated impacts, in ms.

    The onset time from spectral flux is only approximate, so each candidate is
    snapped to the nearest envelope peak first; without that the measurement
    starts on the way up and reports nonsense.
    """
    t = onsets(x, sr)
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


def extract(x, sr, label='', denoise=False):
    x = x - x.mean()
    db, centroid = band_spectrum(x, sr, denoise=denoise)
    rate, cv = onset_stats(x, sr)
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
        'decay_ms': decay_ms(x, sr),
        'mod_db': modulation_depth(x, sr),
        'crest': crest_db(x, sr),
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
