"""Extracts a call's pitch and amplitude contour, and fits a formula to it.

    python3 contours.py                    # prove it on a few references
    python3 contours.py --wav /tmp/out     # write original/resynth pairs to hear
    python3 contours.py --emit             # write src/dsp/contours_generated.h

This is ChirpParade's contour pipeline, pointed at the night. It is here
because that plugin already proved the finding the hard way: **a call is its
frequency contour**, and a physical model of the animal fitted to aggregate
statistics is not. ChirpParade shipped 11,000 lines of syrinx fitted to twenty
medians, measured correctly on all twenty, and sounded nothing like a bird.

A wolf howl is the same kind of object as a bird syllable and an easier one: it
is one pitch held for a second or more, with a slow vibrato on it and a fall at
the end, and everything that makes it a *wolf* rather than a synthesiser is in
how that line actually moves. So the same procedure applies -- pull the
instantaneous frequency and amplitude of a real call out of the WAV, fit each as
a short cosine series in normalised call time, cluster them per caller and keep
the medoids.

What is different here, and why:

  * **the analysis window.** ChirpParade tracks with a 5.3 ms window because a
    chirp's whole content is fast motion. These calls are an octave and more
    lower -- a 220 Hz tawny owl, a 138 Hz howl -- and a 5.3 ms window cannot
    resolve a 220 Hz fundamental at all. 21 ms with a 2.7 ms hop instead, which
    still samples a 5 Hz vibrato seventy times a cycle.

  * **the length.** A bird syllable is 12 to 900 ms. A howl runs to five
    seconds, so the duration gate opens to six -- and a series of a fixed term
    count over a call of that length is a *coarser* fit, which is why the term
    count below was re-measured against this library rather than inherited.

  * **the vibrato comes along for free.** It is stored nowhere and needs no
    oscillator: a 5 Hz vibrato across a 2 s howl is ten cycles, which is term 20
    of the series. The archetype already holds it, in exactly the places where
    the wolf that was recorded put it.

**No audio is stored.** What comes out is two short sets of coefficients per
archetype -- a pitch curve and a level curve -- which is a formula in the same
sense as the one van Hunter Adams reads off a cardinal's spectrogram by hand.
"""
import os
import sys

import numpy as np

import calls as S

REFS = os.environ.get("NIGHTLIFE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))

# The default term count for the demonstration mode below. The number the table
# is actually built with is PITCH_TERMS, further down, and it was measured
# against this library rather than inherited.
TERMS = 48


def smooth(v, k):
    """A short Hann smoother, for rejecting bin noise before differentiating.

    Edge-replicating, not zero-padded. numpy's mode="same" pads with zeros, and
    these series are log2 of a frequency -- around 11.6 -- so zero padding drags
    the first and last samples towards nothing and invents a three-octave
    excursion at each end. It did exactly that, on every contour measured,
    including a constant sine: everything came out with a span of about 3
    octaves and a peak slew of 8000 oct/s, which is the artefact and not the
    bird.
    """
    v = np.asarray(v, float)
    if k < 2 or len(v) < 3:
        return v
    k = min(k, len(v))
    pad = k // 2
    padded = np.concatenate([np.full(pad, v[0]), v, np.full(pad, v[-1])])
    kern = np.hanning(k)
    kern /= kern.sum()
    out = np.convolve(padded, kern, mode="same")
    return out[pad:pad + len(v)]


def track(x, sr, nfft=1024, pad=4096, hop=128):
    """The dominant partial, frame by frame, with a continuity constraint.

    A 21 ms window stepped every 2.7 ms, zero-padded to 4096 for the parabolic
    peak fit. ChirpParade uses 5.3 ms and 0.33 ms, which is the right trade for
    a chirp and the wrong one here: 5.3 ms is 188 Hz of resolution and cannot
    see a 220 Hz owl or a 138 Hz howl as anything but one smeared bin. The cost
    is modulation above about 47 Hz, and nothing in this library has any -- the
    fastest thing measured is a frog's 50 Hz pulse train, which is the chorus
    layer's business and not this one's.

    The instantaneous frequency of the analytic signal was tried first and is
    the wrong tool. A syllable is only *approximately* one component -- there is
    always a harmonic, a neighbour bird or a bit of bed inside any band wide
    enough to hold the sweep -- and the phase derivative of a two-component
    signal swings between them without meaning anything. It reported 6-octave
    spans and 15000 oct/s: the estimator hitting its own rails.
    """
    if len(x) < nfft:
        return None
    win = np.hanning(nfft)
    n = 1 + (len(x) - nfft) // hop
    if n < 4:
        return None
    idx = np.arange(nfft)[None, :] + hop * np.arange(n)[:, None]
    frames = x[idx] * win
    mag = np.abs(np.fft.rfft(frames, pad, axis=1))
    fr = np.fft.rfftfreq(pad, 1.0 / sr)
    sel = (fr >= 120.0) & (fr <= min(10000.0, 0.45 * sr))
    b, bf = mag[:, sel], fr[sel]
    energy = (b ** 2).sum(axis=1)
    if energy.max() <= 0.0:
        return None

    # Start from the loudest frame and walk outwards, refusing a step of more
    # than a third of an octave between frames -- at 0.33 ms that is still a
    # 1000 oct/s slew, so it constrains nothing real and rejects octave jumps.
    anchor = int(np.argmax(energy))
    pk = np.zeros(n, dtype=int)
    pk[anchor] = int(np.argmax(b[anchor]))
    # A third of an octave per frame. At a 2.7 ms hop that is still 123 oct/s,
    # four times the fastest slew measured in this library, so it constrains
    # nothing real and rejects octave jumps between a howl and its second
    # harmonic.
    max_step = 0.33

    def walk(order):
        prev = pk[anchor]
        for i in order:
            f_prev = max(bf[prev], 50.0)
            lo = np.searchsorted(bf, f_prev / 2.0 ** max_step)
            hi = max(lo + 1, np.searchsorted(bf, f_prev * 2.0 ** max_step))
            local = int(lo + np.argmax(b[i, lo:hi]))
            if b[i, local] < 0.02 * b[i].max():
                local = prev
            pk[i] = local
            prev = local

    walk(range(anchor + 1, n))
    walk(range(anchor - 1, -1, -1))

    # Parabolic refinement in log magnitude, the usual sub-bin estimator.
    rows = np.arange(n)
    lo = np.clip(pk - 1, 0, b.shape[1] - 1)
    hi = np.clip(pk + 1, 0, b.shape[1] - 1)
    a1 = np.log(np.maximum(b[rows, lo], 1e-20))
    a2 = np.log(np.maximum(b[rows, pk], 1e-20))
    a3 = np.log(np.maximum(b[rows, hi], 1e-20))
    den = a1 - 2.0 * a2 + a3
    sh = np.where(np.abs(den) > 1e-12, 0.5 * (a1 - a3) / np.where(den == 0, 1.0, den), 0.0)
    freq = bf[pk] + np.clip(sh, -0.5, 0.5) * (bf[1] - bf[0])
    amp = np.sqrt(np.maximum(energy, 0.0))
    return freq, amp, hop / sr


def extract(x, sr):
    """One syllable's pitch and amplitude contour, and its duration."""
    got = track(x, sr)
    if not got:
        return None
    freq, amp, dt = got
    peak = amp.max()
    if peak <= 0.0:
        return None
    # Trim to where it is sounding, or the fit spends its terms on silence.
    live = np.nonzero(amp > 0.08 * peak)[0]
    if len(live) < 6:
        return None
    a, b = live[0], live[-1]
    return freq[a:b + 1], amp[a:b + 1], dt


# How many partials to store, and how many terms each gets.
#
# Measured over 1777 syllables in 57 files before committing to it:
#
#   energy inside a 10-harmonic comb        median 0.67  (0.10 .. 0.99)
#   harmonic balance drift over a syllable  median 4.4 dB (0.8 .. 7.3)
#   harmonics above -24 dB                  median 2.2   (1.0 .. 8.8)
#
# NightLife's own library says the same thing more strongly: a fox scream
# carries a measured median of 3 harmonics above -24 dB and a howl 2, against a
# hoot's 1, and the balance between them is what separates a scream from a
# siren.
#
# The middle row is the one that justifies this. The balance *moves* by 4.4 dB
# across a single syllable, and a fixed valve through a fixed tract cannot do
# that -- so the timbral evolution of a real syllable is not reachable without
# measuring it. Six partials covers the 8.8 at the top of the range with room
# to spare; twelve terms puts the fastest basis function at six cycles across
# the syllable, which is far more than a 4.4 dB drift needs.
HARMONICS = 6
HARM_TERMS = 12


def harmonic_balance(seg, sr, nfft=2048, pad=8192, hop=256):
    """How the energy is shared between the partials, frame by frame.

    Returns HARMONICS curves in dB, normalised so that the partials of each
    frame sum to unit power -- the overall envelope is the level curve's job, so
    these carry only the balance, and the two multiply back together.

    It runs its own STFT rather than reusing the contour tracker's. Resolving
    the harmonics of a 138 Hz howl needs 43 ms, twice what the tracker uses. The balance moves slowly -- 4.4 dB
    across a whole syllable -- so the coarser window costs it nothing, and
    keeping them separate means adding this cannot disturb the contours that are
    already shipping.

    The fundamental is not assumed to be the tracked partial. The loudest
    partial of a corvid is often its second or third, so the divisor is searched
    per syllable by which one puts the most energy into its own comb.
    """
    if len(seg) < nfft * 2:
        return None
    win = np.hanning(nfft)
    n = 1 + (len(seg) - nfft) // hop
    if n < 6:
        return None
    idx = np.arange(nfft)[None, :] + hop * np.arange(n)[:, None]
    mag = np.abs(np.fft.rfft(seg[idx] * win, pad, axis=1))
    fr = np.fft.rfftfreq(pad, 1.0 / sr)
    band = (fr >= 110.0) & (fr <= min(10000.0, 0.45 * sr))
    energy = (mag[:, band] ** 2).sum(axis=1)
    if energy.max() <= 0.0:
        return None
    live = np.nonzero(energy > 0.20 * energy.max())[0]
    if len(live) < 6:
        return None

    # The loudest partial per live frame, as the starting point for the divisor.
    bf = fr[band]
    peak = bf[np.argmax(mag[:, band], axis=1)]

    def comb_share(divisor):
        tot = share = 0.0
        for i in live:
            f0 = peak[i] / divisor
            if f0 < F0_LO_HARM:
                return -1.0
            row = mag[i]
            tot += float((row ** 2).sum())
            for k in range(1, HARMONICS + 1):
                f = f0 * k
                if f > fr[-1]:
                    break
                sel = (fr > f * 0.94) & (fr < f * 1.06)
                if sel.any():
                    share += float((row[sel] ** 2).sum())
        return share / tot if tot > 0.0 else -1.0

    best, divisor = -1.0, 1
    for d in range(1, 5):
        # A lower candidate's comb contains the one above it, so it is
        # penalised -- the same reasoning as the fundamental search in
        # syllables.py.
        sc = comb_share(d) - 0.06 * (d - 1)
        if sc > best:
            best, divisor = sc, d
    if best <= 0.0:
        return None

    curves = np.zeros((HARMONICS, len(live)))
    for j, i in enumerate(live):
        f0 = peak[i] / divisor
        row = mag[i]
        a = np.zeros(HARMONICS)
        for k in range(1, HARMONICS + 1):
            f = f0 * k
            if f > fr[-1]:
                break
            sel = (fr > f * 0.94) & (fr < f * 1.06)
            if sel.any():
                a[k - 1] = float(row[sel].max())
        power = float((a ** 2).sum())
        if power <= 0.0:
            a[0] = 1.0
            power = 1.0
        a /= np.sqrt(power)
        curves[:, j] = 20.0 * np.log10(np.maximum(a, 1.0e-3))

    return ([fit_series(curves[h], HARM_TERMS) for h in range(HARMONICS)],
            divisor, float(best))


# Below this the harmonics of a candidate fundamental fall on top of each other
# at the analysis resolution, and the divisor search stops meaning anything. The
# 43 ms window buys the octave that the lowest howls in the library need.
F0_LO_HARM = 95.0


def fit_series(v, terms=TERMS):
    """Least-squares fit of a cosine series in normalised time on [0, 1].

    A cosine basis rather than a full Fourier one: the contour is not periodic,
    and the half-period cosines of the discrete cosine transform basis do not
    force the ends to meet.
    """
    n = len(v)
    t = (np.arange(n) + 0.5) / n
    basis = np.stack([np.cos(np.pi * k * t) for k in range(terms)], axis=1)
    coef, *_ = np.linalg.lstsq(basis, v, rcond=None)
    return coef


def eval_series(coef, n):
    t = (np.arange(n) + 0.5) / n
    basis = np.stack([np.cos(np.pi * k * t) for k in range(len(coef))], axis=1)
    return basis @ coef


def fit_call(freq, amp, terms=TERMS):
    """Fit both contours. Pitch in octaves, level in dB -- the perceptual axes."""
    lf = np.log2(np.maximum(freq, 20.0))
    la = 20.0 * np.log10(np.maximum(amp / max(amp.max(), 1e-12), 1e-4))
    fc = fit_series(lf, terms)
    ac = fit_series(la, terms)
    err_cents = 1200.0 * float(np.sqrt(np.mean((lf - eval_series(fc, len(lf))) ** 2)))
    err_db = float(np.sqrt(np.mean((la - eval_series(ac, len(la))) ** 2)))
    return fc, ac, err_cents, err_db


def resynth(fc, ac, dur, sr):
    """A pure sine driven by the fitted contours. This is Adams' DDS, generalised."""
    n = max(8, int(round(dur * sr)))
    f = np.exp2(eval_series(fc, n))
    lvl = 10.0 ** (eval_series(ac, n) / 20.0)
    phase = 2.0 * np.pi * np.cumsum(f) / sr
    return np.sin(phase) * np.clip(lvl, 0.0, 4.0)


def vibrato(c):
    """The dominant modulation of one archetype's pitch curve: rate and depth.

    This is the measurement the frame-domain estimator in calls.py cannot make.
    Its hop is 10.7 ms and it refuses lags under five frames, so it cannot
    report below a 18.7 Hz period at all -- and asked for a wolf's vibrato it
    returned exactly that rail, for every group in the library. Here the curve
    is already fitted and continuous, so the modulation is simply the largest
    spectral peak of the pitch series with its overall shape removed.

    Returns (rate in Hz, depth in cents). A caller with no vibrato returns 0.
    """
    n = 1024
    v = eval_series(np.asarray(c["pitch"], float), n)
    dur = max(float(c["dur"]), 1e-3)
    # Remove the call's overall arc -- a cubic, not a mean: a howl that rises and
    # falls has a huge term at its own duration, and that is the howl and not
    # the vibrato.
    t = np.linspace(0.0, 1.0, n)
    v = v - np.polyval(np.polyfit(t, v, 3), t)
    spec = np.abs(np.fft.rfft(v * np.hanning(n)))
    fr = np.fft.rfftfreq(n, dur / n)
    sel = (fr >= 2.0) & (fr <= 30.0)
    if not sel.any() or spec[sel].max() <= 0.0:
        return 0.0, 0.0
    k = int(np.argmax(spec[sel]))
    if spec[sel][k] < 4.0 * np.median(spec[sel]):
        return 0.0, 0.0
    # Depth as the peak-to-peak of the band around that rate, in cents.
    keep = np.zeros_like(spec)
    band = np.nonzero(sel)[0][max(0, k - 2):k + 3]
    keep[band] = spec[band]
    comp = np.fft.irfft(keep * np.exp(1j * np.angle(np.fft.rfft(v * np.hanning(n)))[None, :][0]), n)
    return float(fr[sel][k]), float(1200.0 * (comp.max() - comp.min()))


def describe(freq, dt, label):
    """The statistics the 21 ms window could not see."""
    lg = np.log2(np.maximum(freq, 20.0))
    sm = smooth(lg, 5)
    d = np.diff(sm) / dt
    turns = int(np.sum(np.diff(np.sign(np.diff(sm))) != 0))
    return ("%-6s dur %5.1f ms  span %.2f oct  path %.2f oct  peak slew %4.0f oct/s  turns %3d"
            % (label, 1000.0 * len(freq) * dt, sm.max() - sm.min(),
               np.abs(np.diff(sm)).sum(), np.abs(d).max(), turns))


def write_wav(path, x, sr):
    """16-bit mono, for the A/B proof pairs."""
    import struct
    x = np.clip(np.asarray(x, float), -1.0, 1.0)
    d = (x * 32767.0).astype("<i2").tobytes()
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(d)) + b"WAVEfmt ")
        f.write(struct.pack("<IHHIIHH", 16, 1, 1, sr, sr * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(d)) + d)


def calls_of(path, seconds=30.0, limit=400):
    """Coarse call bounds from the existing segmenter, as sample slices."""
    m, sr, _ = S.load_mono(path, seconds)
    r = S.analyse(path, seconds)
    out = []
    for s in (r["syllables"] if r else [])[:limit]:
        a = max(0, int((s.t0 - 0.004) * sr))
        b = min(len(m), int((s.t1 + 0.004) * sr))
        if b - a > 0.010 * sr:
            out.append((m[a:b], s))
    return out, sr


# ------------------------------------------------------------------- quality
#
# Most syllables in the library are not usable as archetypes, and shipping a
# badly tracked one would put a glitch in the instrument for ever. A syllable
# has to be long enough to fit, loud enough to track, and dominated by one
# partial -- in the dense multi-bird files the tracker is often following two
# birds at once, and the result is a contour no bird ever sang.
# A howl is not a chirp. The shortest thing here worth an archetype is a frog
# pulse or a scops owl pip at about 60 ms; the longest measured single howl in
# the library runs to 5.6 s. Past six seconds what the segmenter has found is a
# chorus of several animals it could not separate -- `wolves-howling-1.wav` is
# one 9.2 s run of three wolves over each other -- so a ceiling stays, and it
# is there to reject exactly that.
MIN_DUR = 0.055
MAX_DUR = 6.000
# How tonal a syllable has to be to become an archetype. Swept, because it
# turned out to be the single most consequential number in the pipeline: it
# selects *for tonality*, so a strict gate fills the table with each caller's
# cleanest calls and leaves the foxes sounding thinner than they are.
#
# Swept over this library, counting usable contours and what they cost in fit
# error; the Fox column is the one that matters, because a vixen scream is the
# noisiest thing here and a gate tuned on hoots would keep none of it:
#
#   gate dB   usable   err c   Wolf   Owl   Screech   Scops   Fox   Loon
#      6.0        146       5     40    31        15      37    15      8
#      3.0        166       7     42    33        16      37    30      8
#      0.0        181       8     48    33        16      37    39      8
#     -3.0        188       8     50    33        16      37    44      8
#     -6.0        191       8     50    33        16      37    47      8
#
# -3 dB nearly triples the fox candidates against +6 for no fit error at all,
# and the four quiet callers stop moving well before it -- their contours are
# already all in. Below -3 only the fox gains, three contours, and what it gains
# there is the part of a scream where the tracker is following the noise rather
# than the voice.
MIN_TONALITY_DB = -3.0
MAX_FIT_CENTS = 90.0


# How fast the *fitted* contour is allowed to move, in octaves per second of
# path travelled. The night callers are an order of magnitude slower than
# birds: this library's fastest measured slew is a fox scream, and the limit
# below is headroom over it rather than a taste judgement. It is the one gate
# that looks at the *fitted* curve, and with a long call and a high term count
# it is what catches a series ringing between the points it was fitted at.
#
# It exists because nothing else here looks at the fitted curve. Duration, fit
# error, tonality and span are all properties of the tracked contour or of how
# well the series threads it -- and fit error is evaluated *at* the tracked
# points, so a series that oscillates between them scores perfectly. With 40
# terms that was survivable, since the basis could not ring hard enough to
# matter. At 96 it is not: one sparrow archetype came through travelling 1089
# octaves in 255 ms, which is 4300 oct/s, and Screech selects for exactly this
# by construction because it takes the highest-path contours in the library.
MAX_PATH_OCT_PER_SEC = 60.0


def usable(freq, amp, dt, syl, err_cents, coef=None):
    dur = len(freq) * dt
    if dur < MIN_DUR or dur > MAX_DUR:
        return False
    if err_cents > MAX_FIT_CENTS:
        return False
    if syl.hnr < MIN_TONALITY_DB:
        return False
    if coef is not None and dur > 0:
        # Densely, not at shape()'s sample count: the point is to catch motion
        # that lives between the samples.
        v = eval_series(coef, max(512, 8 * len(coef)))
        if float(np.abs(np.diff(v)).sum()) / dur > MAX_PATH_OCT_PER_SEC:
            return False
    # A tracked contour that leaves the band it started in is the tracker losing
    # the animal rather than the animal moving. Two octaves, not ChirpParade's
    # three: the widest measured sweep in this library is a fox at 1.4, and a
    # contour spanning more than two has jumped to a harmonic or to a second
    # animal.
    lg = np.log2(np.maximum(freq, 20.0))
    if lg.max() - lg.min() > 2.0:
        return False
    return True


def shape(coef, n=None):
    """The archetype's pitch shape: mean removed, resampled to a fixed length.

    n must stay above the Nyquist rate of the series, or the clustering runs on
    an aliased curve rather than on the contour. It was a fixed 64, which is
    below Nyquist for the 40 terms fitted even before this was noticed: a
    cos(pi k t) with k up to 39 sampled at 64 points folds its top third back
    down, so two contours that differ only in their fine motion could land on
    top of each other and the medoid picked between them arbitrarily.
    """
    # A fixed length here, not one derived from the term count: with the count
    # scaled by duration the archetypes no longer share one, and a distance
    # between two curves sampled at different lengths is not a distance. 512 is
    # above the Nyquist rate of the longest series the gate admits.
    if n is None:
        n = 512
    v = eval_series(coef, n)
    return v - v.mean()


def kmedoids(shapes, k, seed=7):
    """Plain k-medoids on the shape distance.

    Medoids rather than means, deliberately. Averaging contours smears them --
    two syllables that zig-zag out of phase average to a smooth glide, which is
    exactly the mistake that produced the first version of this plugin. A medoid
    is one real measured contour.
    """
    n = len(shapes)
    if n <= k:
        return list(range(n))
    X = np.stack(shapes)
    # ||a-b||^2 = |a|^2 + |b|^2 - 2ab, rather than materialising the n x n x d
    # difference. The explicit form cost n^2 d floats, which was survivable
    # while shape() sampled at 64 points and is not now that it samples above
    # the series' Nyquist rate: Sparrow's 1699 contours at 384 points wanted
    # 8.9 GB and were killed by the OOM reaper.
    sq = np.einsum("ij,ij->i", X, X)
    d2 = sq[:, None] + sq[None, :] - 2.0 * (X @ X.T)
    D = np.sqrt(np.maximum(d2, 0.0) / X.shape[1])
    rng = np.random.RandomState(seed)
    # k-means++ style seeding on the distance matrix.
    med = [int(rng.randint(n))]
    while len(med) < k:
        d = D[:, med].min(axis=1)
        if d.sum() <= 0:
            break
        med.append(int(np.argmax(d)))
    for _ in range(40):
        lab = np.argmin(D[:, med], axis=1)
        newmed = []
        for j in range(len(med)):
            members = np.nonzero(lab == j)[0]
            if not len(members):
                newmed.append(med[j])
                continue
            sub = D[np.ix_(members, members)].sum(axis=1)
            newmed.append(int(members[int(np.argmin(sub))]))
        if newmed == med:
            break
        med = newmed
    return med


# ------------------------------------------------------------------ the table

# **The term count is a resolution, so here it is per second and not per call.**
#
# ChirpParade fits a fixed 96 terms to every syllable, and records in its own
# TODO that this makes the fit budget a hidden duration filter. Over a library
# whose calls span 12 to 900 ms that is survivable. Over this one, where a scops
# owl pip is 60 ms and a howl is five seconds, it is not -- a fixed count was
# measured against the tracked contour it is supposed to reproduce and came out
# like this (`--sweep`, and the per-caller run beside it):
#
#   ratio of fitted path to tracked path, by caller, at a fixed term count
#              dur s    48 terms   64    96   128
#     Wolf      1.19        0.66  0.80  0.97  1.15
#     Owl       0.35        1.19  1.38  1.84  2.39
#     Screech   0.47        1.67  1.90  1.94  2.42
#     Scops     0.57        1.61  2.02  2.53  3.48
#     Fox       0.45        1.25  1.53  2.05  2.57
#     Loon      0.27        1.36  1.54  2.08  2.38
#
# There is no column in that table worth having. At 96 the wolf is right and
# every short caller rings at twice the motion it was asked to draw; at 48 the
# short callers are close and the howl is over-smoothed to two thirds of its
# real motion -- and a howl smoothed like that is the sine-with-a-vibrato that
# this whole pipeline exists to avoid.
#
# Scaling the count with the call's own duration collapses the spread:
#
#   terms/s   median    Wolf   Owl  Screech  Scops   Fox   Loon
#       40      0.83    0.69  0.82     0.91   0.91  0.85   1.01
#       55      ~1.00   0.77  0.92     1.15   1.09  1.04   1.01
#       85      1.14    0.92  1.00     1.34   1.36  1.29   1.10
#
# 55 terms per second of call. The array is sized for the longest call the gate
# admits and each archetype carries its own count; the rest is zero, and a zero
# coefficient costs nothing at either end.
TERMS_PER_SEC = 55.0
PITCH_TERMS = 128
MIN_PITCH_TERMS = 12
LEVEL_TERMS = 24
PER_CALLER = 8


def pitch_terms(dur):
    """How many terms this call's own length earns. See TERMS_PER_SEC."""
    return int(np.clip(round(TERMS_PER_SEC * float(dur)), MIN_PITCH_TERMS, PITCH_TERMS))


def harvest(groups, seconds=45.0, verbose=True):
    """Every usable call contour in the library, grouped by caller."""
    out = {}
    for caller, files in groups:
        got = []
        for name in files:
            path = os.path.join(REFS, name)
            if not os.path.exists(path):
                continue
            segs, sr = calls_of(path, seconds)
            for seg, syl in segs:
                ex = extract(seg, sr)
                if not ex:
                    continue
                freq, amp, dt = ex
                if len(freq) < 12:
                    continue
                terms = pitch_terms(len(freq) * dt)
                fc, ac, ec, ed = fit_call(freq, amp, terms)
                acl = fit_series(20.0 * np.log10(np.maximum(
                    amp / max(amp.max(), 1e-12), 1e-4)), LEVEL_TERMS)
                if not usable(freq, amp, dt, syl, ec, fc):
                    continue
                centre = float(2.0 ** np.median(np.log2(np.maximum(freq, 20.0))))
                # The harmonic balance, and how much of the energy it actually
                # accounts for. A syllable whose comb captures little is either
                # inharmonic or has a second bird in it, and the engine scales
                # its use of the measurement by this rather than trusting it.
                hb = harmonic_balance(seg, sr)
                if hb is None:
                    harm = [np.zeros(HARM_TERMS) for _ in range(HARMONICS)]
                    harm[0][0] = 0.0
                    hfit = 0.0
                else:
                    harm, _divisor, hfit = hb
                got.append({
                    "pitch": fc, "terms": terms, "level": acl, "centre": centre,
                    "dur": len(freq) * dt, "err": ec, "file": name,
                    "shape": shape(fc), "harm": harm, "harmFit": hfit,
                })
        out[caller] = got
        if verbose:
            print("   %-11s %4d usable contours from %2d files"
                  % (caller, len(got), len(files)))
    return out


def pick(got, k=PER_CALLER):
    """The k archetypes of one caller: medoids of its contour shapes."""
    if not got:
        return []
    idx = kmedoids([g["shape"] for g in got], min(k, len(got)))
    # Ordered by centre pitch, so that the Contour control sweeps from the
    # lowest-sitting shape to the highest rather than in an arbitrary order.
    return sorted([got[i] for i in idx], key=lambda g: g["centre"])


# Must match the engine's CallerKind enum, in order: kContourRange is indexed by
# it. Anything new appends -- inserting a caller anywhere else shifts every
# index below it and breaks saved state.
CALLER_ORDER = ["Wolf", "Owl", "Screech", "Scops", "Fox", "Loon"]
# `(frogs)` and `(performed)` are measured by callers.py -- they are in the
# library and in its census -- but they are not callers the engine offers here,
# so no archetypes are extracted for them. A croak is a pulse train through a
# body resonance rather than a pitch contour and is the chorus layer's business;
# see frogs.py.


def emit(path, seconds=45.0):
    """Writes the archetype table as a C++ header."""
    import callers as CA
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    real = {}
    for f in files:
        g = CA.group_of(f)
        if g in CALLER_ORDER:
            real.setdefault(g, []).append(f)

    print("harvesting:")
    got = harvest(sorted(real.items()), seconds)

    table, ranges = [], []
    print("\narchetypes:")
    for name in CALLER_ORDER:
        chosen = pick(got.get(name, []))
        ranges.append((len(table), len(chosen)))
        table.extend(chosen)
        if chosen:
            print("   %-11s %d archetypes, %4.0f..%4.0f ms, path %.1f..%.1f oct, vib %s"
                  % (name, len(chosen),
                     1000 * min(c["dur"] for c in chosen),
                     1000 * max(c["dur"] for c in chosen),
                     min(np.abs(np.diff(c["shape"])).sum() for c in chosen),
                     max(np.abs(np.diff(c["shape"])).sum() for c in chosen),
                     " ".join("%.1f" % vibrato(c)[0] for c in chosen)))
        else:
            print("   %-11s none -- no reference passed the gate" % name)

    def arr(v, per=6):
        out, line = [], "     "
        for i, x in enumerate(v):
            t = "%+.6ff," % x
            if len(line) + len(t) > 96:
                out.append(line)
                line = "     "
            line += " " + t
        out.append(line)
        return "\n".join(out)

    with open(path, "w") as f:
        f.write('''// Generated by tools/analysis/contours.py -- do not edit.
//
// The measured pitch and level contours of real night calls, as cosine series
// in normalised call time. This is the table the caller voice is driven by, and
// it is the reason a howl sounds like an animal rather than like a sine with a
// vibrato on it.
//
// Each entry is one *real* call out of the reference library -- a howl, a hoot,
// a whinny, a pip, a scream, a wail -- chosen as the medoid of a cluster of
// similar contours. A medoid rather than a mean, because averaging two contours
// that move out of phase gives a smooth glide, and a smooth glide is precisely
// what a synthesised howl sounds wrong as.
//
// No audio is stored here. A contour is a formula: the same thing as
// f(x) = -260 sin(-pi x / 5200) + 1740 read off a spectrogram by hand
// (van Hunter Adams, Cornell ECE 4760), with one term per 18 ms of call
// instead of one term in total.
// The vibrato of a howl is in there as the middle terms of the series -- it
// needs no oscillator of its own, and it sits where the wolf that was recorded
// put it.
//
// pitch[] is in octaves about the call's loudest moment, so the table carries
// shape only and the engine transposes it.
// level[] is in dB with the peak at 0.
// harm[] is the balance between the first six partials across the call, in dB.
// It is what makes the timbre *evolve*: a fox scream opens and closes its
// harmonic stack within one call, and a fixed valve through a fixed tract
// cannot do that.
//
// Regenerating needs the reference recordings in !dev/references, which are not
// part of this repository:
//
//     cd tools/analysis && python3 contours.py --emit
//
#pragma once

namespace nightlife {

constexpr int kPitchTerms = %d;
constexpr int kLevelTerms = %d;
constexpr int kHarmonics = %d;
constexpr int kHarmTerms = %d;

struct Contour {
   float durationSec;
   float centreHz;
   // How many of pitch[] are non-zero. The count is the call's own duration at
   // 55 terms per second -- a fit budget is a resolution, and a fixed one is a
   // hidden duration filter. The rest of the array is zero.
   int pitchTerms;
   // How much of the call's energy the harmonic comb below accounts for. Low
   // means the call is inharmonic or has a second animal in it, and the engine
   // scales its use of the measurement by this instead of trusting it.
   float harmFit;
   float pitch[kPitchTerms];
   float level[kLevelTerms];
   // The balance between the partials across the call, in dB, normalised so
   // that each frame's partials sum to unit power -- the overall envelope is
   // level[]'s job, and the two multiply back together.
   float harm[kHarmonics][kHarmTerms];
};

struct ContourRange {
   int first;
   int count;
};

''' % (PITCH_TERMS, LEVEL_TERMS, HARMONICS, HARM_TERMS))
        f.write("constexpr Contour kContours[] = {\n")
        for c in table:
            pc = np.array(c["pitch"], float).copy()
            pc[0] = 0.0
            # Anchor the curve at the call's loudest moment rather than at its
            # mean, so that transposing it puts *the pitch you hear* where Pitch
            # says. Anchored at the mean, an arching contour reads high, because
            # the loud part of an arch sits above its average.
            n = 256
            t = (np.arange(n) + 0.5) / n
            curve = sum(pc[k] * np.cos(np.pi * k * t) for k in range(len(pc)))
            lvl = sum(c["level"][k] * np.cos(np.pi * k * t) for k in range(len(c["level"])))
            pc[0] = -float(curve[int(np.argmax(lvl))])
            harm = ",\n".join("     {\n%s\n     }" % arr(h) for h in c["harm"])
            pad = np.zeros(PITCH_TERMS)
            pad[:len(pc)] = pc
            f.write("   {%.6ff, %.1ff, %d, %.3ff,\n    {\n%s\n    },\n    {\n%s\n    },\n"
                    "    {\n%s\n    }},\n"
                    % (c["dur"], c["centre"], len(pc), c["harmFit"],
                       arr(pad), arr(c["level"]), harm))
        f.write("};\n\n")
        f.write("constexpr int kNumContours = %d;\n\n" % len(table))
        f.write("// One range per CallerKind, in enum order.\n")
        f.write("constexpr ContourRange kContourRange[] = {\n")
        for name, (first, count) in zip(CALLER_ORDER, ranges):
            f.write("   {%3d, %2d},  // %s\n" % (first, count, name))
        f.write("};\n\n} // namespace nightlife\n")
    per = PITCH_TERMS + LEVEL_TERMS + HARMONICS * HARM_TERMS
    print("\nwrote %s: %d archetypes, %d floats (%.0f kB)" %
          (path, len(table), len(table) * per, len(table) * per * 4 / 1024.0))
    fits = [c["harmFit"] for c in table]
    print("harmonic comb share across the table: %.2f .. %.2f, median %.2f"
          % (min(fits), max(fits), float(np.median(fits))))
    print("\nmedian archetype duration per caller -- this is what the engine's")
    print("CallerTraits lengthSec should be, so that Length and the contours agree:")
    for name, (first, count) in zip(CALLER_ORDER, ranges):
        if count:
            print("   %-11s %.3ff   (%d archetypes)"
                  % (name, float(np.median([table[first + i]["dur"] for i in range(count)])),
                     count))


def sweep(seconds=45.0):
    """The two numbers that were inherited rather than measured, swept here.

    The tonality gate decides what becomes an archetype, and the term count
    decides how much of a contour survives being written down. Both were set in
    ChirpParade against a library of bird syllables 12 to 900 ms long; this
    library's calls run to six seconds, so neither transfers by assumption.
    """
    import callers as CA
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    groups = {}
    for f in files:
        g = CA.group_of(f)
        if g in CALLER_ORDER:
            groups.setdefault(g, []).append(f)

    # Harvest once with every gate wide open, then count what each gate keeps.
    raw = []
    for caller, names in sorted(groups.items()):
        for name in names:
            segs, sr = calls_of(os.path.join(REFS, name), seconds)
            for seg, syl in segs:
                ex = extract(seg, sr)
                if not ex:
                    continue
                freq, amp, dt = ex
                if len(freq) < 12:
                    continue
                dur = len(freq) * dt
                if dur < MIN_DUR or dur > MAX_DUR:
                    continue
                lg = np.log2(np.maximum(freq, 20.0))
                if lg.max() - lg.min() > 2.0:
                    continue
                raw.append((caller, freq, amp, dt, syl))
    print("%d calls segmented and tracked inside the duration and span gates\n" % len(raw))

    print("term count: how much of the tracked motion the fitted curve reproduces,")
    print("and what it costs in fit error. Too few terms over-smooths; too many")
    print("ring between the points the fit was evaluated at.")
    print("%8s%14s%14s%10s%10s" % ("terms/s", "fitted path", "tracked path", "ratio", "err c"))
    for tps in (40, 55, 70, 85, 100):
        fp, tp, err = [], [], []
        for caller, freq, amp, dt, syl in raw[::3]:
            fc, ac, ec, ed = fit_call(freq, amp, int(np.clip(
                round(tps * len(freq) * dt), MIN_PITCH_TERMS, PITCH_TERMS)))
            v = eval_series(fc, 1024)
            fp.append(float(np.abs(np.diff(v)).sum()))
            lg = smooth(np.log2(np.maximum(freq, 20.0)), 5)
            tp.append(float(np.abs(np.diff(lg)).sum()))
            err.append(ec)
        print("%8d%14.2f%14.2f%10.2f%10.0f" % (tps, np.median(fp), np.median(tp),
                                               np.median(fp) / max(np.median(tp), 1e-9),
                                               np.median(err)))

    print("\ntonality gate: what each threshold keeps, per caller, and the fit error")
    print("of what it keeps. The Fox column is the one that matters -- a vixen scream")
    print("is the noisiest thing in the library and a gate tuned on hoots keeps none.")
    hdr = "%8s%8s%8s" % ("gate dB", "usable", "err c")
    for c in CALLER_ORDER:
        hdr += "%9s" % c
    print(hdr)
    for gate in (6.0, 3.0, 0.0, -3.0, -6.0, -12.0):
        keep, err, per = 0, [], {c: 0 for c in CALLER_ORDER}
        for caller, freq, amp, dt, syl in raw:
            if syl.hnr < gate:
                continue
            fc, ac, ec, ed = fit_call(freq, amp, pitch_terms(len(freq) * dt))
            if ec > MAX_FIT_CENTS:
                continue
            v = eval_series(fc, max(512, 8 * len(fc)))
            if float(np.abs(np.diff(v)).sum()) / (len(freq) * dt) > MAX_PATH_OCT_PER_SEC:
                continue
            keep += 1
            err.append(ec)
            per[caller] += 1
        row = "%8.1f%8d%8.0f" % (gate, keep, np.median(err) if err else 0.0)
        for c in CALLER_ORDER:
            row += "%9d" % per[c]
        print(row)
    return 0


def main():
    if "--sweep" in sys.argv:
        return sweep()
    if "--emit" in sys.argv:
        out = os.path.join(os.path.dirname(__file__), "../../src/dsp/contours_generated.h")
        emit(os.path.abspath(out))
        return 0

    outdir = None
    if "--wav" in sys.argv:
        outdir = sys.argv[sys.argv.index("--wav") + 1]
        os.makedirs(outdir, exist_ok=True)

    files = ["howling-wolf.wav", "wolf-howling-type-01.wav", "tawny-owl-2.wav",
             "barred-owl-hooting.wav", "fox-scream.wav", "loon-call.wav",
             "screech_owl.wav", "scops-owl.wav"]
    print("Extracting the contour of real calls, fitting %.0f cosine terms per second of\n"
          "call to each. 'orig' is measured from the recording, 'fit' from a pure sine\n"
          "driven by the fitted formula. If the two agree, the formula is the call.\n"
          % TERMS_PER_SEC)
    tot_c, tot_d, n = 0.0, 0.0, 0
    for name in files:
        path = os.path.join(REFS, name)
        if not os.path.exists(path):
            continue
        segs, sr = calls_of(path, 20.0, 6)
        print("--- %s" % name)
        for i, (seg, _syl) in enumerate(segs[:3]):
            got = extract(seg, sr)
            if not got:
                print("    (could not isolate a tone)")
                continue
            freq, amp, dt = got
            dur = len(freq) * dt
            fc, ac, ec, ed = fit_call(freq, amp, pitch_terms(dur))
            y = resynth(fc, ac, dur, sr)
            back = extract(y, sr)
            print("    " + describe(freq, dt, "orig"))
            if back:
                print("    " + describe(back[0], back[2], "fit") +
                      "   err %.0f c, %.1f dB" % (ec, ed))
                tot_c += ec
                tot_d += ed
                n += 1
            if outdir:
                write_wav(os.path.join(outdir, "%s_%d_orig.wav" % (name[:-4], i)), seg, sr)
                write_wav(os.path.join(outdir, "%s_%d_fit.wav" % (name[:-4], i)), y * 0.7, sr)
    if n:
        print("\n%d calls fitted: mean error %.0f cents of pitch, %.1f dB of level"
              % (n, tot_c / n, tot_d / n))
    if outdir:
        print("wrote original/resynth pairs to %s" % outdir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
