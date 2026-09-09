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
MAX_DUR = 0.400
MIN_TONALITY_DB = 6.0
MAX_FIT_CENTS = 90.0


def usable(freq, amp, dt, syl, err_cents):
    dur = len(freq) * dt
    if dur < MIN_DUR or dur > MAX_DUR:
        return False
    if err_cents > MAX_FIT_CENTS:
        return False
    if syl.hnr < MIN_TONALITY_DB:
        return False
    # A tracked contour that leaves the band it started in, or that moves faster
    # than any bird, is the tracker losing the bird rather than the bird moving.
    lg = np.log2(np.maximum(freq, 20.0))
    if lg.max() - lg.min() > 3.0:
        return False
    return True


def shape(coef, n=64):
    """The archetype's pitch shape: mean removed, resampled to a fixed length."""
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
    D = np.sqrt(((X[:, None, :] - X[None, :, :]) ** 2).mean(axis=2))
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

PITCH_TERMS = 40
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
                if not usable(freq, amp, dt, syl, ec):
                    continue
                centre = float(2.0 ** np.median(np.log2(np.maximum(freq, 20.0))))
                got.append({
                    "pitch": fc, "level": acl, "centre": centre,
                    "dur": len(freq) * dt, "err": ec, "file": name,
                    "shape": shape(fc),
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
                 "Crane", "Goose", "Crow", "Raven", "Screech"]


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
// pitch[] is in octaves about the syllable's own centre, term 0 removed, so the
// table carries shape only and the engine transposes it.
// level[] is in dB with the peak at 0.
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

struct Contour {
   float durationSec;
   float centreHz;
   float pitch[kPitchTerms];
   float level[kLevelTerms];
};

struct ContourRange {
   int first;
   int count;
};

''' % (PITCH_TERMS, LEVEL_TERMS))
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
            f.write("   {%.6ff, %.1ff,\n    {\n%s\n    },\n    {\n%s\n    }},\n"
                    % (c["dur"], c["centre"], arr(pc), arr(c["level"])))
        f.write("};\n\n")
        f.write("constexpr int kNumContours = %d;\n\n" % len(table))
        f.write("// One range per SpeciesKind, in enum order.\n")
        f.write("constexpr ContourRange kContourRange[] = {\n")
        for name, (first, count) in zip(SPECIES_ORDER, ranges):
            f.write("   {%3d, %2d},  // %s\n" % (first, count, name))
        f.write("};\n\n} // namespace chirpparade\n")
    print("\nwrote %s: %d archetypes, %d floats" %
          (path, len(table), len(table) * (PITCH_TERMS + LEVEL_TERMS)))
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
