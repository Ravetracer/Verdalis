"""Reduces the library to the engine's species table, and writes it as a C++ header.

    python3 species.py [--write] [dir]

`wingbeat.py` measures a stack of harmonic levels per recording and `swarm.py`
measures how harmonic the sound is at all. Those are the measurement. This is
the step that turns them into something an oscillator can be built from, and it
is a fit, not a copy: the same four-parameter shaper is fitted to every
species' median stack, so what the engine ships is a shape rather than a curve
read back out of a table.

The shaper is the one the engine actually runs, so the fit is against the
filters' own discrete-time responses and not against an idealised version of
them:

    ex     = the stroke pulse at the shipped Bite, generated as the engine does
    tilted = lp*gLow + (x - lp)*gHigh          one-pole shelf, pivot at 300 Hz
    out    = (1-mix)*tilted + mix*k*BP(tilted) one SVF bandpass, crossfaded in

Four free numbers per species -- the shelf's tilt in dB, and the bandpass's
centre, resonance and mix -- fitted by grid search, with the overall level
divided out so only the shape is being matched.

The bandpass is crossfaded rather than added on top. Added, its gain and the
shelf's tilt both control the slope above the peak and the fit runs away to
whatever ceiling the search puts on it; crossfaded it is bounded at both ends,
and a pure bandpass -- mix 1 -- is the steepest shape the model can reach.

Every measured stack in the library has the same form: a peak somewhere between
150 and 460 Hz and then a fall of 5 to 10 dB per octave. That is one resonance
and one tilt, which is why four numbers are enough and why the residuals below
are small.
"""
import sys, os, math, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np
from refs import classify
from cache import cached_dir
import wingbeat as W
import swarm as S
import pulse as P

SR = 48000.0
TILT_PIVOT_HZ = 300.0
N_FIT = 10                 # harmonics 1..10 are fitted; above that the measurement thins out

# The flight species, in the order they appear in the window's Species chip:
# rising wingbeat rate, which is also roughly falling body size.
FLYERS = ["hornet", "bumblebee", "wasp", "fly", "bee", "mosquito", "dragonfly"]
DISPLAY = {"hornet": "Hornet", "bumblebee": "Bumblebee", "wasp": "Wasp", "fly": "Housefly",
           "bee": "Honeybee", "mosquito": "Mosquito", "dragonfly": "Dragonfly"}


# ------------------------------------------------------------------ responses
#
# The discrete-time magnitude of each filter in shared/include/verdalis/dsp,
# evaluated on the unit circle. These mirror the C++ exactly; a fit against an
# idealised response would land coefficients the engine does not reproduce.

def onepole_lp(f, cutoff, sr=SR):
    a = 1.0 - math.exp(-2.0 * math.pi * min(cutoff / sr, 0.49))
    z = np.exp(-2j * np.pi * f / sr)
    return a / (1.0 - (1.0 - a) * z)


def svf_bp(f, cutoff, res, sr=SR):
    """Svf::bandpassNormalised: k * BP, the constant-peak-gain bandpass."""
    g = math.tan(math.pi * min(max(cutoff / sr, 1e-5), 0.49))
    k = min(max(2.0 - 1.98 * res, 0.02), 2.0)
    z = np.exp(-2j * np.pi * f / sr)
    num = g * (1.0 - z * z)
    den = (1.0 + g * k + g * g) + (2.0 * g * g - 2.0) * z + (1.0 - g * k + g * g) * z * z
    return k * num / den


DEFAULT_BITE = 0.45        # the shipped Bite; the fit is referenced to it


def excitation_db(f0, n_harm, sr=SR, bite=DEFAULT_BITE, stroke=0.0):
    """The stroke pulse's own harmonic levels, generated exactly as the engine does.

    The first version of this fit assumed the excitation was spectrally flat and
    let the shelf and the resonance carry the whole measured stack. They cannot:
    a (1 - u^2)^2 pulse 7 per cent of a cycle wide is already 20 dB down by its
    tenth harmonic, so a shaper fitted as if it were flat comes out far too
    steep -- and on the hornet, whose measured second harmonic is 10.7 dB above
    its fundamental to begin with, steep enough that the rendered buzz jumped
    the octave.

    So the pulse is built here at the shipped Bite, one cycle at a time, and its
    own spectrum is part of the model. Bite then moves the stack off the
    measurement rather than being invisible to it, which is what its
    documentation says it does.
    """
    hw = max(0.45 * 0.02 ** bite, 1.25 * f0 / sr)
    hw = min(max(hw, 0.004), 0.45)
    n = 4096
    ph = np.arange(n) / float(n)
    dA = np.minimum(ph, 1.0 - ph)
    dB = np.abs(ph - 0.5)
    mean = hw * 16.0 / 15.0

    def pulse(d):
        u = d / hw
        w = np.where(u < 1.0, 1.0 - u * u, 0.0)
        return w * w

    ex = (pulse(dA) - mean) + stroke * (pulse(dB) - mean)
    X = np.abs(np.fft.rfft(ex))
    k = np.arange(1, n_harm + 1)
    return 20.0 * np.log10(np.maximum(X[k], 1e-12))


def shelf_db(f, tilt_db):
    """The one-pole shelf, in dB. Separated out because it is additive in dB and
    independent of the bandpass, which is what makes the grid below tractable."""
    lp = onepole_lp(f, TILT_PIVOT_HZ)
    gl = 10.0 ** (np.asarray(tilt_db)[..., None] / 40.0)
    gh = 10.0 ** (-np.asarray(tilt_db)[..., None] / 40.0)
    return 20.0 * np.log10(np.maximum(np.abs(gl * lp + gh * (1.0 - lp)), 1e-12))


def band_db(f, fc, res, mix):
    """(1 - mix)*x + mix*k*BP(x), in dB, broadcast over fc x res x mix."""
    bp = np.empty((len(fc), len(res), len(f)), complex)
    for i, c in enumerate(fc):
        for j, r in enumerate(res):
            bp[i, j] = svf_bp(f, c, r)
    m = np.asarray(mix)[None, None, :, None]
    h = (1.0 - m) + m * bp[:, :, None, :]
    return 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))


def shaper_db(f, tilt_db, fc, res, mix):
    """The chain the engine runs, for one parameter set."""
    return (shelf_db(f, np.array([tilt_db]))[0]
            + band_db(f, [fc], [res], [mix])[0, 0, 0])


STROKES = np.linspace(0.0, 1.0, 11)


def fit(f0, stack_db):
    """Grid search over the shaper, once per candidate stroke asymmetry.

    Stroke is fitted rather than assumed because it is where the measurement
    actually puts the second harmonic. A wing pushes air on the downstroke and
    again on the upstroke, and when the two are equal the odd harmonics cancel;
    a hornet's second harmonic measures 10.7 dB *above* its fundamental and a
    bumblebee's 11.7, which is two near-equal strokes and not a resonance.

    Leaving it at zero and letting a narrow bandpass at 159 Hz carry the
    hornet's h2 fitted the stack to 3.8 dB RMS and still rendered an insect an
    octave above the one it was fitted to, because a resonance between h1 and h2
    crushes h1 rather than reinforcing h2. Fitting it puts the dominance in the
    mechanism that produces it.

    The shelf is additive in dB and does not interact with the bandpass, so the
    error surface factorises: one (tilt x harmonic) table plus one
    (fc x res x mix x harmonic) table, broadcast against each other. That turns
    a four-deep Python loop into two array operations, and lets the grid be fine
    enough that nothing has to be refined afterwards.

    The level is removed from every candidate before the error is taken, so what
    is fitted is the shape of the stack and not how loud the recording was.
    """
    k = np.arange(1, len(stack_db) + 1)
    f = f0 * k
    target = np.asarray(stack_db, float)

    # The shelf's range stops at +36 dB on purpose. Above roughly +24 the
    # error surface is flat: a shelf that steep is already a straight
    # -6 dB/octave across every harmonic that was measured, and adding more
    # tilt changes nothing the fit can see. Left open, hornet and mosquito walk
    # to whatever ceiling the grid has and buy 0.3 dB of residual for it.
    # Harmonics are weighted 1/sqrt(k), the same weighting the pitch estimator
    # uses and for the same reason: the first few carry the perceived pitch.
    # Weighted flat, the hornet's fit traded 3.6 dB of error at its second
    # harmonic -- which is 10.7 dB above its fundamental and therefore is the
    # pitch -- for a fraction of a dB spread over harmonics nobody hears
    # separately.
    wk = 1.0 / np.sqrt(k)
    wk = wk / wk.sum()

    tilts = np.linspace(-40.0, 36.0, 153)
    fcs = np.exp(np.linspace(math.log(50.0), math.log(6000.0), 96))
    ress = np.linspace(0.0, 0.94, 25)
    mixes = np.linspace(0.0, 1.0, 41)

    bd = band_db(f, fcs, ress, mixes)             # (C, R, M, K)
    best = None
    for st in STROKES:
        ex = excitation_db(f0, len(stack_db), stroke=st)   # (K,)
        sh = shelf_db(f, tilts) + ex                       # (T, K)
        tot = sh[:, None, None, None, :] + bd[None]        # (T, C, R, M, K)
        tot -= (tot * wk).sum(axis=-1, keepdims=True) / wk.sum()
        tgt = target - (target * wk).sum() / wk.sum()
        err = np.sqrt(((tot - tgt) ** 2 * wk).sum(axis=-1) / wk.sum())
        i = int(np.argmin(err))
        t, c, r, m = np.unravel_index(i, err.shape)
        e = float(err[t, c, r, m])
        if best is None or e < best[0]:
            best = (e, tilts[t], fcs[c], ress[r], mixes[m], float(st))
    e, tilt, fc, res, mix, st = best
    return tilt, fc, res, mix, st, e


def gather(d):
    """Per-class median wingbeat rate, harmonic stack and HNR."""
    out = {}
    for p in sorted(glob.glob(os.path.join(d, "*.wav"))):
        cls = classify(os.path.basename(p))
        if cls not in FLYERS:
            continue
        r = W.measure(p)
        if r is None:
            continue
        s = S.measure(p)
        out.setdefault(cls, []).append((r, s))
    table = {}
    for cls, rows in out.items():
        f = np.array([r["f0"] for r, _ in rows])
        # a species' rate is the median of the per-file medians; files more than
        # a fifth from it are a misfiled recording or a second insect in frame
        med = float(np.median(f))
        sel = [(r, s) for (r, s) in rows if abs(math.log2(r["f0"] / med)) < 0.585]
        if not sel:
            sel = rows
        stack = np.median(np.array([r["parts"][:N_FIT] for r, _ in sel]), axis=0)
        # The 80th percentile, not the median. A species row is meant to be
        # *one* insect, and most of the library is not: of the 21 honeybee
        # references, the hives and swarms measure 0-2 dB harmonic-to-noise
        # where the close single ones measure 8-13. Taking the median mixes the
        # two and ships a hive as the definition of a bee -- after which Count
        # has nothing left to do, because the one-insect case already sounds
        # like sixty.
        #
        # The upper end is the right estimator here and not a convenience:
        # mixing sources destroys harmonicity and never creates it, so the most
        # harmonic recording of a species is the closest thing the library has
        # to one of them on its own. The 80th percentile rather than the maximum
        # only because a single clipped or filtered file should not define a row.
        hnrs = sorted(s["hnr"] for _, s in sel if s)
        hnr = float(np.percentile(hnrs, 80)) if hnrs else 0.0
        table[cls] = dict(
            n=len(sel), f0=float(np.median([r["f0"] for r, _ in sel])), stack=stack, hnr=hnr,
            voiced=float(np.median([r["voiced"] for r, _ in sel])),
            jitter=float(np.median([r["jitter"] for r, _ in sel])),
            disp=float(np.median([r["spread"] for r, _ in sel])))
    return table


def stridulators(d):
    out = {}
    for p in sorted(glob.glob(os.path.join(d, "*.wav"))):
        cls = classify(os.path.basename(p))
        if cls not in P.STRIDULATORS:
            continue
        r = P.measure(p)
        if r:
            out.setdefault(cls, []).append(r)
    t = {}
    for cls, rows in out.items():
        pu = [r["pulse"] for r in rows if r["pulse"]]
        ec = [r["echeme"] for r in rows if r["echeme"]]
        t[cls] = dict(n=len(rows), fc=float(np.median([r["fc"] for r in rows])),
                      q=float(np.median([r["q"] for r in rows])),
                      pulse=float(np.median(pu)) if pu else 0.0,
                      echeme=float(np.median(ec)) if ec else 0.0,
                      duty=float(np.median([r["duty"] for r in rows])))
    return t


HEADER = '''#pragma once

// The measured species table. Generated by tools/analysis/species.py -- do not
// edit by hand; edit the script and re-run it.
//
// Each row is one species of flying insect, reduced from the reference library
// to the six numbers the engine needs. `rateHz` and `hnrDb` are measurements:
// the median wingbeat rate across that species' recordings, and the median
// share of band energy that lies on a harmonic of it. The four shaper
// coefficients are a fit -- one one-pole shelf and one SVF bandpass, fitted
// against those filters' own discrete-time responses to the species' median
// harmonic stack, with the overall level divided out.
//
// The stack itself is carried alongside as `stack`, in dB relative to the
// fundamental, so the fit can be checked against what it was fitted to without
// going back to the recordings. It is what the manual's tables print.
//
// A table of numbers is not a sample; see the suite's note on pure synthesis.
// Nothing here reproduces recorded audio -- these say what shape to give a
// signal the plugin generates itself.

#include <cstdint>

namespace insectswarm {

constexpr int kStackHarmonics = %(nfit)d;

struct SpeciesRow {
   const char *name;
   float rateHz;      // measured median wingbeat rate
   float tiltDb;      // shelf tilt about 300 Hz, fitted
   float formantHz;   // the bandpass centre, fitted
   float formantRes;  // its resonance, 0..1, in Svf::setCutoff's units
   float formantMix;  // how much of the chain is the bandpass, 0..1, fitted
   float stroke;      // the upstroke against the downstroke, 0..1, fitted
   float hnrDb;       // measured harmonic-to-noise ratio of the buzz
   float voiced;      // measured share of frames that are a wingbeat at all
   float fitErrDb;    // RMS residual of the shaper fit against the stack
   int   refs;        // recordings the row was taken from
   float stack[kStackHarmonics]; // the measured stack, dB re the fundamental
};

constexpr SpeciesRow kSpecies[] = {
%(rows)s};

constexpr int kNumSpecies = static_cast<int>(sizeof(kSpecies) / sizeof(kSpecies[0]));

// The two stridulating mechanisms. A tymbal and a file-and-scraper are both a
// train of clicks ringing a resonant body, so they share a model and differ
// only in its numbers -- and they differ in it sharply. A cricket's resonator
// is twice as sharp as a cicada's (Q 26 against 13) and its clicks arrive
// seven times more slowly (36/s against 268), which is the whole distance
// between a pure whistling trill and a dry rattle.
struct StridulatorRow {
   const char *name;
   float carrierHz;
   float q;
   float pulseHz;
   float echemeHz;
   float duty;
   int   refs;
};

constexpr StridulatorRow kStridulators[] = {
%(strid)s};

constexpr int kNumStridulators =
   static_cast<int>(sizeof(kStridulators) / sizeof(kStridulators[0]));

} // namespace insectswarm
'''


def main():
    args = sys.argv[1:]
    d = next((a for a in args if os.path.isdir(a)), cached_dir())
    table = gather(d)
    print("%-10s %3s %8s %7s %8s %7s %7s %7s %7s %7s" %
          ("species", "n", "rate", "tilt", "formant", "res", "mix", "stroke", "err", "HNR"))
    rows = []
    for cls in FLYERS:
        if cls not in table:
            print("%-10s  (no usable references)" % cls); continue
        t = table[cls]
        tilt, fc, res, gb, st, err = fit(t["f0"], t["stack"])
        print("%-10s %3d %8.1f %7.1f %8.0f %7.2f %7.2f %7.2f %7.2f %7.1f" %
              (cls, t["n"], t["f0"], tilt, fc, res, gb, st, err, t["hnr"]))
        rows.append((cls, t, tilt, fc, res, gb, st, err))

    strid = stridulators(d)
    print("\n%-10s %3s %8s %7s %8s %8s %6s" % ("strid", "n", "carrier", "Q", "pulse", "echeme", "duty"))
    for cls in ("cicada", "cricket"):
        if cls in strid:
            s = strid[cls]
            print("%-10s %3d %8.0f %7.1f %8.1f %8.2f %6.2f" %
                  (cls, s["n"], s["fc"], s["q"], s["pulse"], s["echeme"], s["duty"]))

    print("\ndispersion and wander, across the flight species:")
    for cls in FLYERS:
        if cls in table:
            print("  %-10s dispersion %5.0f ct   frame-to-frame wander %4.0f ct   voiced %.2f" %
                  (cls, table[cls]["disp"], table[cls]["jitter"], table[cls]["voiced"]))

    if "--write" not in args:
        print("\n(--write to regenerate src/dsp/species_generated.h)")
        return

    def crow(cls, t, tilt, fc, res, gb, stroke, err):
        st = ", ".join("%.1ff" % v for v in t["stack"])
        return ('   {"%s", %.1ff, %.2ff, %.1ff, %.4ff, %.4ff, %.3ff, %.2ff, %.3ff, %.2ff, %d,\n'
                '    {%s}},\n' % (DISPLAY[cls], t["f0"], tilt, fc, res, gb, stroke, t["hnr"],
                                  t["voiced"], err, t["n"], st))

    body = "".join(crow(*r) for r in rows)
    sbody = ""
    for cls, name in (("cicada", "Cicada"), ("cricket", "Cricket")):
        if cls in strid:
            s = strid[cls]
            sbody += '   {"%s", %.1ff, %.1ff, %.1ff, %.2ff, %.3ff, %d},\n' % (
                name, s["fc"], s["q"], s["pulse"], s["echeme"], s["duty"], s["n"])

    out = os.path.join(os.path.dirname(__file__), "../../src/dsp/species_generated.h")
    with open(out, "w") as f:
        f.write(HEADER % dict(nfit=N_FIT, rows=body, strid=sbody))
    print("\nwrote %s" % os.path.normpath(out))


if __name__ == "__main__":
    main()
