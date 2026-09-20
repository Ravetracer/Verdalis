"""The night bed: the noise everything else sits on, and the insects in it.

    python3 bed.py                 # the census
    python3 bed.py --emit          # write src/dsp/bed_generated.h

Two measurements, both taken from the *quiet* frames of the library -- the
background of a field recording is what is there when the animal being recorded
is not calling.

**The bed.** Every reference has one: wind in leaves, distant water, the
recorder's own hiss, and a chorus far enough away to have stopped being
individual callers. Its third-octave curve is measured over the quietest third
of the frames of each file and the median taken across the library, then
collapsed to the octave bank the engine actually runs. The suite has done this
four times now -- ShoreBreak, SkyHowl, RiverFlow and CrackleBlaze all carry a
measured bed with a gain vector solved against it -- and those four solve the
bank at run time because their own controls reshape the target. Here the target
is fixed, so the solve is done once, here, and what ships is the answer.

**The insects.** A cricket or a katydid is a narrow band of noise, pulsed. It is
not a frog and not a caller: there is no pitch contour and no body resonance,
just a carrier a few hundred Hz wide with a trill rate on it. Three of the
references carry one loudly enough to measure, and the numbers below are what
the insect layer is built from. Nothing in the library is a single insect in
isolation, so what is measured is a band rather than an individual -- which is
also what a listener hears at midnight.
"""
import os
import sys

import numpy as np

import calls as S

REFS = os.environ.get("NIGHTLIFE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
SECONDS = float(os.environ.get("NIGHTLIFE_SECONDS", "45"))

# Third-octave centres, ISO. The bed is measured on these and collapsed to
# octaves for the engine, which runs eight bandpasses and not twenty-eight.
THIRD = np.array([31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500,
                  630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000,
                  6300, 8000, 10000])
# The engine's bank: one bandpass per octave, 63 Hz to 8 kHz. Eight of them,
# the same count the other four plugins in the suite settled on.
OCTAVES = np.array([63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0])

NFFT = 4096
HOP = 2048
QUIET_SHARE = 0.30
# How steady a high band has to be to count as an insect bed rather than as the
# top of a chorus. The tenth percentile of its envelope over the median: the
# three insect beds in the library score 0.5 and up, every frog chorus below it.
STEADY_MIN = 0.45


def spectrum(path, seconds=SECONDS, quiet=QUIET_SHARE):
    """The average spectrum of the quietest frames of one file, and its rate."""
    m, sr, _ = S.load_mono(path, seconds)
    n = max(1, (len(m) - NFFT) // HOP)
    if n < 4:
        return None, None, None
    idx = np.arange(NFFT)[None, :] + HOP * np.arange(n)[:, None]
    mag = np.abs(np.fft.rfft(m[idx] * np.hanning(NFFT), axis=1))
    fr = np.fft.rfftfreq(NFFT, 1.0 / sr)
    e = (mag ** 2).sum(axis=1)
    keep = np.argsort(e)[:max(1, int(quiet * n))]
    return (mag[keep] ** 2).mean(axis=0), fr, sr


def third_octave(p, fr, sr):
    """The third-octave curve of one spectrum, in dB, normalised to its own peak."""
    out = np.full(len(THIRD), np.nan)
    for i, c in enumerate(THIRD):
        if c > 0.45 * sr:
            continue
        sel = (fr >= c / 2 ** (1 / 6.0)) & (fr < c * 2 ** (1 / 6.0))
        if sel.any():
            out[i] = p[sel].sum()
    with np.errstate(divide="ignore", invalid="ignore"):
        return 10.0 * np.log10(out / np.nanmax(out))


def insect_band(path, seconds=SECONDS):
    """The narrow high band an insect chorus makes, if the file holds one.

    A cricket band is narrow, high and *steady*. Two of those three are needed
    to find it, and the first version of this used only the first two: searched
    above 2.2 kHz for a band standing clear of the octave around it, it returned
    2203 to 2520 Hz for nine of the thirteen frog recordings, which is the
    distant part of the frog chorus itself and not an insect at all.

    So the floor moves up to 2.8 kHz -- above every frog resonance measured in
    this library, whose highest is a leopard frog at 3.3 kHz close-miked and
    whose chorus energy is all below 2.6 -- and steadiness is measured and
    reported beside it: the tenth percentile of the band's envelope over its
    median, across every frame of the file rather than the quiet ones. A cricket
    bed runs all night and scores near 1; a chorus of croaks scores low, because
    most of its frames have no croak in them.
    """
    p, fr, sr = spectrum(path, seconds)
    if p is None:
        return None
    sel = (fr >= 2800.0) & (fr <= min(9000.0, 0.45 * sr))
    if sel.sum() < 32:
        return None
    f, pv = fr[sel], p[sel]
    # Smooth over 50 Hz: the band is a few hundred wide and the raw spectrum of
    # a chorus of insects is not smooth.
    k = max(3, int(round(50.0 / (fr[1] - fr[0]))))
    sm = np.convolve(np.pad(pv, k, mode="edge"), np.ones(2 * k + 1) / (2 * k + 1),
                     mode="same")[k:k + len(pv)]
    i = int(np.argmax(sm))
    peak = float(f[i])
    # How far it stands above the octave around it, and how wide it is at -6 dB.
    around = (f > peak / 2 ** 0.5) & (f < peak * 2 ** 0.5) & (np.abs(f - peak) > 0.25 * peak)
    stand = 10.0 * np.log10(sm[i] / max(sm[around].mean(), 1e-30)) if around.any() else 0.0
    half = sm[i] / 4.0
    a = i
    while a > 0 and sm[a] > half:
        a -= 1
    b = i
    while b < len(sm) - 1 and sm[b] > half:
        b += 1
    width = float(f[b] - f[a])
    share = float(pv.sum() / max(p.sum(), 1e-30))

    # The trill rate: the band's own envelope, taken with a short window so that
    # a 25 Hz pulse train is sampled twenty times a cycle.
    m, sr2, _ = S.load_mono(path, seconds)
    n2, hop2 = 512, 64
    cnt = max(4, (len(m) - n2) // hop2)
    idx = np.arange(n2)[None, :] + hop2 * np.arange(cnt)[:, None]
    mag2 = np.abs(np.fft.rfft(m[idx] * np.hanning(n2), axis=1))
    fr2 = np.fft.rfftfreq(n2, 1.0 / sr2)
    band = (fr2 > peak - 0.5 * width - 100.0) & (fr2 < peak + 0.5 * width + 100.0)
    env = np.sqrt((mag2[:, band] ** 2).sum(axis=1))
    env = env - env.mean()
    spec = np.abs(np.fft.rfft(env * np.hanning(len(env))))
    mf = np.fft.rfftfreq(len(env), hop2 / float(sr2))
    ms = (mf >= 4.0) & (mf <= 120.0)
    trill = float(mf[ms][int(np.argmax(spec[ms]))]) if ms.any() else 0.0
    prom = float(spec[ms].max() / max(np.median(spec[ms]), 1e-30)) if ms.any() else 0.0
    full = np.sqrt((mag2[:, band] ** 2).sum(axis=1))
    steady = float(np.percentile(full, 10) / max(np.median(full), 1e-30))
    return {"peak": peak, "width": width, "q": peak / max(width, 1e-6), "stand": stand,
            "share": share, "trill": trill, "prom": prom, "steady": steady}


def main():
    files = sorted(f for f in os.listdir(REFS) if f.lower().endswith(".wav"))
    curves = []
    for name in files:
        p, fr, sr = spectrum(os.path.join(REFS, name))
        if p is None:
            continue
        curves.append(third_octave(p, fr, sr))
    curves = np.array(curves)
    med = np.nanmedian(curves, axis=0)
    med = med - np.nanmax(med)

    print("the night bed, third-octave, median over %d references, dB below its own peak:"
          % len(curves))
    for i, c in enumerate(THIRD):
        n = int(np.sum(np.isfinite(curves[:, i])))
        bar = "#" * max(0, int(round(40 + med[i])))
        print("   %6.0f Hz  %6.1f  (%2d files) %s" % (c, med[i], n, bar))

    oct_db = octave_curve(med)
    print("\ncollapsed to the engine's eight octave bands:")
    for c, v in zip(OCTAVES, oct_db):
        print("   %6.0f Hz  %6.1f dB" % (c, v))

    print("\nthe insect band, where a reference holds one that stands clear:")
    print("%-34s%9s%8s%7s%8s%8s%8s%8s" %
          ("file", "peak Hz", "width", "Q", "stands", "share", "steady", "trill"))
    got = []
    for name in files:
        r = insect_band(os.path.join(REFS, name))
        if not r or r["stand"] < 4.0 or r["share"] < 0.01:
            continue
        keep = r["steady"] >= STEADY_MIN
        if keep:
            got.append(r)
        print("%-34s%9.0f%8.0f%7.1f%8.1f%8.3f%8.2f%8.1f%s" %
              (name[:34], r["peak"], r["width"], r["q"], r["stand"], r["share"],
               r["steady"], r["trill"] if r["prom"] > 5.0 else 0.0,
               "" if keep else "   (not steady -- a chorus, not an insect bed)"))
    if got:
        print("%-34s%9.0f%8.0f%7.1f%8.1f%8.3f%8.2f%8.1f" %
              ("median of the steady ones", np.median([r["peak"] for r in got]),
               np.median([r["width"] for r in got]), np.median([r["q"] for r in got]),
               np.median([r["stand"] for r in got]), np.median([r["share"] for r in got]),
               np.median([r["steady"] for r in got]),
               np.median([r["trill"] for r in got if r["prom"] > 5.0] or [0.0])))

    if "--sweep" in sys.argv:
        sweep_ends(oct_db)
    if "--calibrate" in sys.argv:
        wav = sys.argv[sys.argv.index("--calibrate") + 1]
        gains, _rms = solve_bank(oct_db)
        # The gains currently in the header are what that render was made with,
        # so the correction is applied to those rather than to a fresh solve.
        import re as _re
        hdr = os.path.join(os.path.dirname(__file__), "../../src/dsp/bed_generated.h")
        if os.path.exists(hdr):
            m = _re.search(r"kBedBandGain\[kNumBedBands\] = \{([^}]*)\}", open(hdr).read())
            if m:
                gains = np.array([float(v.strip().rstrip("f"))
                                  for v in m.group(1).split(",")]) ** 2
        fixed, err = calibrate(wav, oct_db, gains)
        print("\nthe bank corrected against the engine's own rendered bed:")
        print("%10s%10s%12s%12s" % ("Hz", "err dB", "gain was", "gain now"))
        amp_was = np.sqrt(np.array(gains) / max(np.max(gains), 1e-20))
        amp_now = np.sqrt(fixed / max(fixed.max(), 1e-20))
        for j, c in enumerate(OCTAVES):
            print("%10.0f%10.1f%12.4f%12.4f" % (c, err[j], amp_was[j], amp_now[j]))
        print("   paste into kBedBandGain: {%s}"
              % ", ".join("%.4ff" % v for v in amp_now))
    if "--emit" in sys.argv:
        out = os.path.join(os.path.dirname(__file__), "../../src/dsp/bed_generated.h")
        emit(os.path.abspath(out), med, oct_db, got, len(curves))
    return 0


def octave_curve(third_db):
    """The third-octave curve summed into the engine's octave bands, in dB."""
    lin = 10.0 ** (np.where(np.isfinite(third_db), third_db, -120.0) / 10.0)
    out = []
    for c in OCTAVES:
        sel = (THIRD >= c / 2 ** 0.5) & (THIRD < c * 2 ** 0.5)
        out.append(10.0 * np.log10(max(lin[sel].sum(), 1e-12)))
    out = np.array(out)
    return out - out.max()


# The two filters on the ends of the bank, and why they are there.
#
# A bank of octave-wide bandpasses cannot make a steep end. The measured night
# falls 20 dB between 2.5 and 5 kHz, and an Svf bandpass's skirts fall at only
# 6 dB per octave -- so with the top two bands solved to *zero* gain the bank
# still rendered within 2 dB of flat all the way to 10 kHz, against a target
# 20 dB down. The first version of this did exactly that and measured 9.6 dB
# rms against the night it was solved for.
#
# So the bed gets a second-order lowpass and highpass, the same answer
# ShoreBreak, RiverFlow and CrackleBlaze reached for the same reason, and the
# bank is solved for what is left. The corners are swept below rather than
# chosen.
BED_LP_HZ = 2600.0
BED_HP_HZ = 40.0
# How many one-poles each end is. The engine's Lp2 and Hp2 are two cascaded
# one-poles, not a Butterworth pair, and the difference is not cosmetic: a
# Butterworth model fitted the bank to a roll-off the engine does not have and
# the rendered bed came out 7 to 10 dB bright above 2.5 kHz. The model below is
# the engine's filter, and the order is swept with the corners.
BED_LP_POLES = 4
BED_HP_POLES = 2


def end_response(grid, lp_hz=None, hp_hz=None, lp_poles=None, hp_poles=None):
    """Power response of the two end filters, as the engine actually builds them.

    A cascade of N one-poles, which is what Lp2/Hp2 are (twice over when the
    engine runs two of them in series), so this is 1/(1+(f/fc)^2)^N and not a
    Butterworth of order N.
    """
    lp_hz = BED_LP_HZ if lp_hz is None else lp_hz
    hp_hz = BED_HP_HZ if hp_hz is None else hp_hz
    lp_poles = BED_LP_POLES if lp_poles is None else lp_poles
    hp_poles = BED_HP_POLES if hp_poles is None else hp_poles
    lp = 1.0 / (1.0 + (grid / lp_hz) ** 2) ** lp_poles
    hp = ((grid / hp_hz) ** 2 / (1.0 + (grid / hp_hz) ** 2)) ** hp_poles
    return lp * hp


def solve_bank(target_db, lp_hz=None, hp_hz=None, lp_poles=None, hp_poles=None):
    """Gains for the octave bank whose summed power matches the measured curve.

    The bank's bands overlap: a bandpass an octave wide still passes a quarter
    of the power an octave away, so setting each band's gain to its own measured
    level overshoots wherever the curve has a peak. The gains are therefore
    solved -- in power, non-negative, least squares on a log-frequency grid --
    which is the same reasoning as the four run-time solvers in the suite, done
    once because nothing here reshapes the target afterwards.
    """
    grid = np.exp2(np.linspace(np.log2(40.0), np.log2(12000.0), 240))
    # An octave-wide bandpass, as a power response. Q = 1.414 is one octave
    # between the -3 dB points, which is what the engine's Svf runs.
    q = 1.414
    ends = end_response(grid, lp_hz, hp_hz, lp_poles, hp_poles)
    resp = np.zeros((len(grid), len(OCTAVES)))
    for j, c in enumerate(OCTAVES):
        w = grid / c
        resp[:, j] = ends / (1.0 + (q * (w - 1.0 / w)) ** 2)
    tgt = np.interp(np.log2(grid), np.log2(OCTAVES), target_db)
    tgt = 10.0 ** (tgt / 10.0)
    # Non-negative least squares by projected gradient: a handful of bands and a
    # convex problem, so a hundred iterations is far more than it needs and it
    # avoids a scipy dependency the rest of the suite does not have.
    g = np.full(len(OCTAVES), tgt.mean())
    step = 1.0 / (np.linalg.norm(resp, 2) ** 2)
    for _ in range(4000):
        g = np.maximum(g - step * (resp.T @ (resp @ g - tgt)), 0.0)
    fit = resp @ g
    err = 10.0 * np.log10(np.maximum(fit, 1e-12)) - 10.0 * np.log10(np.maximum(tgt, 1e-12))
    return g, float(np.sqrt(np.mean(err ** 2)))


def sweep_ends(oct_db):
    """The two corners, swept rather than chosen."""
    print("\nthe bank's end filters, swept against the measured curve:")
    print("%8s%8s%8s%8s%10s" % ("lp Hz", "poles", "hp Hz", "poles", "rms dB"))
    best = (None, 1e9)
    for poles in (2, 4, 6):
        for lp in (1800.0, 2200.0, 2600.0, 3200.0, 4000.0):
            for hp in (40.0, 60.0):
                _g, rms = solve_bank(oct_db, lp, hp, poles, 2)
                print("%8.0f%8d%8.0f%8d%10.2f" % (lp, poles, hp, 2, rms))
                if rms < best[1]:
                    best = ((lp, hp, poles), rms)
    print("   best: lp %.0f Hz x%d poles, hp %.0f Hz, %.2f dB rms"
          % (best[0][0], best[0][2], best[0][1], best[1]))
    return best[0]


def calibrate(render_wav, oct_db, gains):
    """Corrects the solved gains against what the engine actually renders.

    The analytic solve gets the bank into the right shape and no further. Its
    model of an octave bandpass is 1/(1+(Q(w-1/w))^2), which is *a* bandpass and
    not the one the engine builds, and the two differ enough at the ends that
    the first rendered bed came out 6 dB rms from the curve it had been solved
    for -- most of it above 2.5 kHz, where the difference between a model skirt
    and a real one accumulates over three octaves.

    So the last step is measured rather than modelled: render the bed alone,
    take its third-octave curve with the same estimator the night was measured
    with, and push each band by the error in its own octave. Damped, because the
    bands overlap and a full correction oscillates.
    """
    p, fr, sr = spectrum(render_wav, 30.0, quiet=1.0)
    got = third_octave(p, fr, sr)
    got = got - np.nanmax(got)
    tgt = np.interp(np.log2(THIRD), np.log2(OCTAVES), oct_db)
    out = np.array(gains, float)
    err = []
    for j, c in enumerate(OCTAVES):
        sel = (THIRD >= c / 2 ** 0.5) & (THIRD < c * 2 ** 0.5) & np.isfinite(got)
        if not sel.any():
            err.append(0.0)
            continue
        e = float(np.mean(got[sel] - tgt[sel]))
        err.append(e)
        out[j] = max(out[j] * 10.0 ** (-0.6 * e / 10.0), 0.0)
    return out, np.array(err)


def emit(path, third_db, oct_db, insects, nfiles):
    gains, rms = solve_bank(oct_db)
    amp = np.sqrt(gains / max(gains.max(), 1e-20))
    # Steady *and* narrow. A steady wide band above 2.8 kHz is as likely to be
    # the recorder's own hiss as an insect -- fox-scream's is -- and a narrow one
    # that is not steady is a croak. Only the intersection is a cricket.
    clean = [r for r in insects if r["q"] >= 10.0]
    ins_peak = np.median([r["peak"] for r in clean]) if clean else 3164.0
    ins_q = np.median([r["q"] for r in clean]) if clean else 18.6
    # The trill is taken from every band with a prominent one, narrow or wide:
    # a katydid's band is wide and its rate is the measurement, and there are
    # only two of either in the library.
    trills = [r["trill"] for r in insects if r["prom"] > 5.0 and r["trill"] > 20.0]
    ins_trill = np.median(trills) if trills else 41.0
    ins_share = np.median([r["share"] for r in clean]) if clean else 0.02
    with open(path, "w") as f:
        f.write("""// Generated by tools/analysis/bed.py -- do not edit.
//
// The night bed, and the insects in it.
//
// The bed is the third-octave curve of the quietest third of the frames of
// every reference in the library, median across %d files -- wind, distant
// water, recorder hiss and a chorus too far away to be individual callers. It
// is collapsed to the eight octave bands the engine runs and the bank's gains
// are solved against it in power, non-negatively, on a log-frequency grid.
//
// The solve is done here rather than at run time, which is the one place this
// differs from ShoreBreak, SkyHowl, RiverFlow and CrackleBlaze: their controls
// reshape the target, so they have to solve it live. Nothing here does, so what
// ships is the answer. Residual after the solve: %.1f dB RMS across the grid.
//
// The insect band is measured the same way -- from the quiet frames, because a
// cricket band is *steady* and that is what separates it from the top end of a
// croak. %d references carry one that is both steady and clear of the octave
// around it.
//
// **This is the weakest measurement in the plugin, and it is worth knowing
// why.** The library has no recording of insects on their own: what is measured
// here is the cricket bed *behind* a scops owl and behind a chorus of frogs,
// isolated by being narrow, high and steady where everything else in those
// files is none of the three. Two recordings carry a prominent trill rate and
// they disagree by an octave, 33 and 49 Hz, which may well be a katydid and a
// cricket rather than an error. Three or four clean recordings of a summer
// night would replace all of this with something worth the name.
//
// Regenerating needs the reference recordings in !dev/references, which are not
// part of this repository:
//
//     cd tools/analysis && python3 bed.py --emit
//
#pragma once

namespace nightlife {

constexpr int kNumBedBands = %d;

// Octave centres, Hz.
constexpr float kBedBandHz[kNumBedBands] = {%s};

// The measured bed's own level in each band, dB below the loudest -- what the
// bank is solved against, kept for the tools that check the solve.
constexpr float kBedBandDb[kNumBedBands] = {%s};

// The solved gains, as amplitudes with the loudest at 1.0. These are what the
// engine multiplies its bandpasses by.
constexpr float kBedBandGain[kNumBedBands] = {%s};

// The insect band: a narrow, steady, pulsed band of noise, which is what a
// chorus of crickets and katydids is. Not a caller and not a croak -- there is
// no pitch contour and no body resonance in it at all.
constexpr float kInsectHz = %.1ff;      // measured carrier
constexpr float kInsectQ = %.2ff;       // carrier over its own -6 dB width
constexpr float kInsectTrillHz = %.1ff; // the pulse rate on the band
constexpr float kInsectShare = %.3ff;   // its share of the quiet-frame energy

// The two filters on the ends of the bank. A bank of octave-wide bandpasses
// cannot make a steep end -- an Svf bandpass's skirts fall at 6 dB per octave,
// and the measured night falls 20 dB between 2.5 and 5 kHz -- so these make the
// ends and the bank is solved for what is left.
constexpr float kBedLpHz = %.1ff;
constexpr float kBedHpHz = %.1ff;

} // namespace nightlife
""" % (nfiles, rms, len(insects), len(OCTAVES),
       ", ".join("%.1ff" % c for c in OCTAVES),
       ", ".join("%.1ff" % v for v in oct_db),
       ", ".join("%.4ff" % v for v in amp),
       ins_peak, ins_q, ins_trill, ins_share, BED_LP_HZ, BED_HP_HZ))
    print("\nwrote %s (bank residual %.1f dB RMS)" % (path, rms))


if __name__ == "__main__":
    sys.exit(main())
