"""Extracts a syllable's pitch and amplitude contour, and fits a formula to it.

    python3 contours.py                    # prove it on a few references
    python3 contours.py --wav /tmp/out     # write original/resynth pairs to hear

This is the thing that should have been built first.

Van Hunter Adams synthesises a northern cardinal by putting a spectrogram into
PowerPoint, drawing lines along the frequency trace, and fitting

    f(x) = -260 sin(-pi x / 5200) + 1740        Hz, x in samples

then driving a sine table with it. That is one sine term, read off by hand. The
Bitwig Grid patch in !dev does the same thing with hand-drawn multi-segment
curves on a sine's pitch. Both work.

A syrinx model fitted to *aggregate statistics* does not, because the statistics
were measured through a 21 ms analysis window that cannot see what a syllable
actually does:

    through a 21 ms window     peak slew   2.7 oct/s,  0.25 turns
    at 0.7 ms resolution       peak slew  20-440 oct/s,  2-40 turns

So this module does Adams' procedure automatically and at scale: pull the
instantaneous frequency and amplitude of a real syllable out of the WAV, and fit
each as a short Fourier series in normalised syllable time. Forty terms gets within a third of a
semitone of the real contour; one term -- which is what a single-sinusoid
gesture is -- gets nowhere near it.

**No audio is stored.** What comes out is two short sets of coefficients -- a
pitch curve and a level curve -- which is a formula in exactly the sense Adams'
one is. The synthesis stays an oscillator driven by a gesture; the gesture is
measured instead of assumed to be a single sinusoid.

"""
import os
import sys

import numpy as np

import syllables as S

REFS = os.environ.get("CHIRPPARADE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))

# Terms in each fitted series, chosen by measuring the fit error against the
# real contours. It converges slowly, because a real syllable has up to forty
# direction changes in it:
#
#     terms      8    16    24    32    48    64
#     error     82c   58c   47c   37c   27c   24c
#
# 40 is past the knee at a third of a semitone. The engine that was shipped
# before this used *one* term per gesture, which is the 82-cent column and then
# some -- and that is the whole reason it did not sound like a bird.
TERMS = 40


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


def track(x, sr, nfft=256, pad=1024, hop=16):
    """The dominant partial, frame by frame, with a continuity constraint.

    A 5.3 ms window stepped every 0.33 ms, zero-padded to 1024 for the
    parabolic peak fit. The window is what limits this: at 5.3 ms it resolves
    modulation to about 190 Hz, which is past everything measured.

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
    sel = (fr >= 350.0) & (fr <= min(12000.0, 0.45 * sr))
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
# The middle row is the one that justifies this. The balance *moves* by 4.4 dB
# across a single syllable, and a fixed valve through a fixed tract cannot do
# that -- so the timbral evolution of a real syllable is not reachable without
# measuring it. Six partials covers the 8.8 at the top of the range with room
# to spare; twelve terms puts the fastest basis function at six cycles across
# the syllable, which is far more than a 4.4 dB drift needs.
HARMONICS = 6
HARM_TERMS = 12


def harmonic_balance(seg, sr, nfft=512, pad=2048, hop=32):
    """How the energy is shared between the partials, frame by frame.

    Returns HARMONICS curves in dB, normalised so that the partials of each
    frame sum to unit power -- the overall envelope is the level curve's job, so
    these carry only the balance, and the two multiply back together.

    It runs its own STFT rather than reusing the contour tracker's. The tracker
    uses a 5.3 ms window because that is what keeps the scribble; resolving the
    harmonics of a 200 Hz corvid needs 10.7 ms. The balance moves slowly -- 4.4 dB
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
    band = (fr >= 250.0) & (fr <= min(12000.0, 0.45 * sr))
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
# at the analysis resolution, and the divisor search stops meaning anything.
F0_LO_HARM = 180.0


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


def fit_syllable(freq, amp, terms=TERMS):
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


def syllables_of(path, seconds=30.0, limit=400):
    """Coarse syllable bounds from the existing segmenter, as sample slices."""
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
MIN_DUR = 0.018
# A croak is not a chirp. At 0.400 this cut every raven syllable longer than
# 300 ms, and a raven's call is mostly longer than that. Past a second a
# "syllable" is a phrase the segmenter failed to split, so a ceiling stays.
MAX_DUR = 0.900
# How tonal a syllable has to be to become an archetype. Swept, because it
# turned out to be the single most consequential number in the pipeline: it
# selects *for tonality*, so a strict gate fills the table with each species'
# cleanest syllables and leaves the corvids sounding thinner than they are.
#
#   gate dB    usable    fit err    Crow candidates    Crow harmonics
#      6.0        808      32 c            56               1.9
#      2.0        967      34 c            79               2.1
#      0.0       1026      35 c            83               2.2
#     -3.0       1100      36 c           110               2.5
#
# 0 dB buys 27 % more usable contours and 48 % more corvid candidates for three
# cents of fit error. Below that the comb share starts falling away, which means
# the harmonic measurement stops being worth much.
MIN_TONALITY_DB = 0.0
MAX_FIT_CENTS = 90.0


# How fast the *fitted* contour is allowed to move, in octaves per second of
# path travelled. The library's own census bounds the bird at a peak slew of
# 20..440 oct/s, so this is headroom over the fastest thing ever measured
# rather than a taste judgement.
#
# It exists because nothing else here looks at the fitted curve. Duration, fit
# error, tonality and span are all properties of the tracked contour or of how
# well the series threads it -- and fit error is evaluated *at* the tracked
# points, so a series that oscillates between them scores perfectly. With 40
# terms that was survivable, since the basis could not ring hard enough to
# matter. At 96 it is not: one sparrow archetype came through travelling 1089
# octaves in 255 ms, which is 4300 oct/s, and Screech selects for exactly this
# by construction because it takes the highest-path contours in the library.
MAX_PATH_OCT_PER_SEC = 500.0


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
    # A tracked contour that leaves the band it started in, or that moves faster
    # than any bird, is the tracker losing the bird rather than the bird moving.
    lg = np.log2(np.maximum(freq, 20.0))
    if lg.max() - lg.min() > 3.0:
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
    if n is None:
        n = max(256, 4 * len(coef))
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

# The fit budget is per syllable, so it is also a resolution: 40 terms over a
# 60 ms chirp is one every 1.5 ms, and over a 900 ms croak one every 22 ms.
# The fit error then carried the syllable's duration, and MAX_FIT_CENTS
# rejected long syllables for being long -- 28 of the 30 raven syllables over
# 300 ms were dropped, several with an HNR above 20 dB.
#
# The number is set by how far the fitted contour travels against how far the
# tracked one does, measured densely rather than at shape()'s sample count:
#
#   terms   fitted path   tracked path
#      40      2.3 oct      2.9 oct     over-smoothed, 79 % of the real motion
#      64      3.5          4.1         85 %
#      96      4.9          4.5         109 %
#     128      5.8          4.8         121 %, ringing between the samples
#
# 96 is the closest to reproducing what was measured. Fit error is not the
# criterion and cannot be: it is evaluated at the tracked points, so a
# high-order series threads them exactly while oscillating in between, and it
# falls monotonically with the term count all the way into nonsense.
PITCH_TERMS = 96
LEVEL_TERMS = 24
PER_SPECIES = 8


def harvest(groups, seconds=30.0, verbose=True):
    """Every usable syllable contour in the library, grouped by species."""
    out = {}
    for species, files in groups:
        got = []
        for name in files:
            path = os.path.join(REFS, name)
            if not os.path.exists(path):
                continue
            segs, sr = syllables_of(path, seconds)
            for seg, syl in segs:
                ex = extract(seg, sr)
                if not ex:
                    continue
                freq, amp, dt = ex
                if len(freq) < 12:
                    continue
                fc, ac, ec, ed = fit_syllable(freq, amp, PITCH_TERMS)
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
                    "pitch": fc, "level": acl, "centre": centre,
                    "dur": len(freq) * dt, "err": ec, "file": name,
                    "shape": shape(fc), "harm": harm, "harmFit": hfit,
                })
        out[species] = got
        if verbose:
            print("   %-11s %4d usable contours from %2d files"
                  % (species, len(got), len(files)))
    return out


def pick(got, k=PER_SPECIES):
    """The k archetypes of one species: medoids of its contour shapes."""
    if not got:
        return []
    idx = kmedoids([g["shape"] for g in got], min(k, len(got)))
    # Ordered by centre pitch, so that the Contour control sweeps from the
    # lowest-sitting shape to the highest rather than in an arbitrary order.
    return sorted([got[i] for i in idx], key=lambda g: g["centre"])


SPECIES_ORDER = ["Whistler", "Sparrow", "Warbler", "Budgie", "Woodpecker",
                 "Crane", "Goose", "Screech"]
# Crow and Raven are measured by species.py -- they are still in the reference
# library and still part of its census -- but they are not species the engine
# offers, so no archetypes are extracted for them. See TODO.md.


def emit(path, seconds=25.0):
    """Writes the archetype table as a C++ header."""
    import species as SP
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    real = {}
    for f in files:
        g = SP.group_of(f)
        if g and g != "(drumming)":
            real.setdefault(g, []).append(f)

    print("harvesting:")
    got = harvest(sorted(real.items()), seconds)

    # Screech has no references of its own. Rather than invent coefficients, it
    # gets the eight most extreme contours in the whole library -- the ones whose
    # pitch travels furthest -- which is what "every range at once" honestly
    # means when there is nothing to measure.
    every = [g for v in got.values() for g in v]
    every.sort(key=lambda g: -np.abs(np.diff(g["shape"])).sum())
    got["Screech"] = sorted(every[:PER_SPECIES], key=lambda g: g["centre"])

    table, ranges = [], []
    print("\narchetypes:")
    for name in SPECIES_ORDER:
        chosen = got.get(name, []) if name == "Screech" else pick(got.get(name, []))
        ranges.append((len(table), len(chosen)))
        table.extend(chosen)
        if chosen:
            print("   %-11s %d archetypes, %4.0f..%4.0f ms, path %.1f..%.1f oct"
                  % (name, len(chosen),
                     1000 * min(c["dur"] for c in chosen),
                     1000 * max(c["dur"] for c in chosen),
                     min(np.abs(np.diff(c["shape"])).sum() for c in chosen),
                     max(np.abs(np.diff(c["shape"])).sum() for c in chosen)))
        else:
            print("   %-11s none -- falls back to the species below it" % name)

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
// The measured pitch and level contours of real bird syllables, as cosine
// series in normalised syllable time. This is the table the syllable voice is
// driven by, and it is the reason the plugin sounds like a bird.
//
// Each entry is one *real* syllable out of the reference library, chosen as the
// medoid of a cluster of similar contours -- a medoid rather than a mean,
// because averaging two contours that zig-zag out of phase gives a smooth glide
// and smooth glides are what the first version of this plugin got wrong.
//
// No audio is stored here. A contour is a formula: the same thing as
// f(x) = -260 sin(-pi x / 5200) + 1740 read off a spectrogram by hand
// (van Hunter Adams, Cornell ECE 4760), with forty terms instead of one because
// a real syllable turns direction up to forty times and one term cannot.
//
// pitch[] is in octaves about the syllable's loudest moment, so the table
// carries shape only and the engine transposes it.
// level[] is in dB with the peak at 0.
// harm[] is the balance between the first six partials across the syllable, in
// dB. It is what makes the timbre *evolve*: measured over 1777 syllables the
// balance moves 4.4 dB across a single syllable, and a fixed valve through a
// fixed tract cannot do that.
//
// Regenerating needs the reference recordings in !dev/references, which are not
// part of this repository:
//
//     cd tools/analysis && python3 contours.py --emit
//
#pragma once

namespace chirpparade {

constexpr int kPitchTerms = %d;
constexpr int kLevelTerms = %d;
constexpr int kHarmonics = %d;
constexpr int kHarmTerms = %d;

struct Contour {
   float durationSec;
   float centreHz;
   // How much of the syllable's energy the harmonic comb below accounts for.
   // Low means the syllable is inharmonic or has a second bird in it, and the
   // engine scales its use of the measurement by this instead of trusting it.
   float harmFit;
   float pitch[kPitchTerms];
   float level[kLevelTerms];
   // The balance between the partials across the syllable, in dB, normalised so
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
            # Anchor the curve at the syllable's loudest moment rather than at
            # its mean, so that transposing it puts *the pitch you hear* where
            # Pitch says. Anchored at the mean, an arching contour read 31 %
            # high, because the loud part of an arch sits above its average.
            n = 256
            t = (np.arange(n) + 0.5) / n
            curve = sum(pc[k] * np.cos(np.pi * k * t) for k in range(len(pc)))
            lvl = sum(c["level"][k] * np.cos(np.pi * k * t) for k in range(len(c["level"])))
            pc[0] = -float(curve[int(np.argmax(lvl))])
            harm = ",\n".join("     {\n%s\n     }" % arr(h) for h in c["harm"])
            f.write("   {%.6ff, %.1ff, %.3ff,\n    {\n%s\n    },\n    {\n%s\n    },\n"
                    "    {\n%s\n    }},\n"
                    % (c["dur"], c["centre"], c["harmFit"], arr(pc), arr(c["level"]), harm))
        f.write("};\n\n")
        f.write("constexpr int kNumContours = %d;\n\n" % len(table))
        f.write("// One range per SpeciesKind, in enum order.\n")
        f.write("constexpr ContourRange kContourRange[] = {\n")
        for name, (first, count) in zip(SPECIES_ORDER, ranges):
            f.write("   {%3d, %2d},  // %s\n" % (first, count, name))
        f.write("};\n\n} // namespace chirpparade\n")
    per = PITCH_TERMS + LEVEL_TERMS + HARMONICS * HARM_TERMS
    print("\nwrote %s: %d archetypes, %d floats (%.0f kB)" %
          (path, len(table), len(table) * per, len(table) * per * 4 / 1024.0))
    fits = [c["harmFit"] for c in table]
    print("harmonic comb share across the table: %.2f .. %.2f, median %.2f"
          % (min(fits), max(fits), float(np.median(fits))))
    print("\nmedian archetype duration per species -- this is what the engine's")
    print("SpeciesTraits lengthSec should be, so that Length and the contours agree:")
    for name, (first, count) in zip(SPECIES_ORDER, ranges):
        if count:
            print("   %-11s %.3ff   (%d archetypes)"
                  % (name, float(np.median([table[first + i]["dur"] for i in range(count)])),
                     count))


def main():
    if "--emit" in sys.argv:
        out = os.path.join(os.path.dirname(__file__), "../../src/dsp/contours_generated.h")
        emit(os.path.abspath(out))
        return 0

    outdir = None
    if "--wav" in sys.argv:
        outdir = sys.argv[sys.argv.index("--wav") + 1]
        os.makedirs(outdir, exist_ok=True)

    files = ["single_bird_chirp.wav", "chirps_01.wav", "chirps_05.wav",
             "whistling_single_robin.wav", "nightingale.wav", "green_woodpecker_chirp.wav"]
    print("Extracting the contour of real syllables and fitting %d cosine terms to each.\n"
          "'orig' is measured from the recording, 'fit' from a pure sine driven by the\n"
          "fitted formula. If the two agree, the formula is the syllable.\n" % PITCH_TERMS)
    tot_c, tot_d, n = 0.0, 0.0, 0
    for name in files:
        path = os.path.join(REFS, name)
        if not os.path.exists(path):
            continue
        segs, sr = syllables_of(path, 8.0, 6)
        print("--- %s" % name)
        for i, (seg, _syl) in enumerate(segs[:3]):
            got = extract(seg, sr)
            if not got:
                print("    (could not isolate a tone)")
                continue
            freq, amp, dt = got
            dur = len(freq) * dt
            fc, ac, ec, ed = fit_syllable(freq, amp, PITCH_TERMS)
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
        print("\n%d syllables fitted: mean error %.0f cents of pitch, %.1f dB of level"
              % (n, tot_c / n, tot_d / n))
    if outdir:
        print("wrote original/resynth pairs to %s" % outdir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
