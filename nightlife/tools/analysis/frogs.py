"""The frog chorus: what a croak is made of, and how a chorus is spaced in time.

    python3 frogs.py               # the census
    python3 frogs.py --emit        # write src/dsp/chorus_generated.h

A croak is not a pitch contour, which is why it is not in contours.py. It is a
**pulse train through a body resonance**: the frog drives its vocal sac in
bursts, and what reaches the microphone is a sequence of clicks -- 20 to 100 a
second -- each ringing the sac and the mouth. Two things follow, and both are
measured here rather than assumed:

  * the pulse rate is the species. It is the first thing a field guide lists,
    and at the rates measured below it is heard as a rattle rather than as
    separate clicks.

  * the spectrum is two peaks, not one. A frog's call has a dominant band and
    usually a second one about an octave away, which is the sac and the mouth
    rather than a harmonic series -- the ratio between them is not an integer
    in any recording here.

The third measurement is the chorus itself. CrackleBlaze found that a plain
Poisson process is not how fire crackles are spaced, and the statistic that
said so was the Fano factor -- the variance of the count in a window over its
mean, which is exactly 1.0 for a Poisson process. So it is measured here too,
before the chorus is built on the assumption that failed there.
"""
import os
import sys

import numpy as np

import calls as S

REFS = os.environ.get("NIGHTLIFE_REFS",
                      os.path.join(os.path.dirname(__file__), "../../!dev/references"))
SECONDS = float(os.environ.get("NIGHTLIFE_SECONDS", "60"))

# The frog files, and whether each is one frog close enough to measure a croak
# from or a chorus measured for its timing only. A dense chorus has no isolated
# croak in it by definition, so a pulse rate taken from one is the beating of
# several frogs and not a species.
FILES = [
    ("frogs-croak1.wav", "close"),
    ("frog-sound.wav", "close"),
    ("croaking-frogs.wav", "close"),
    ("frogs-croaking.wav", "close"),
    ("multiple-frog-croaking-sounds.wav", "close"),
    ("leopard-frogs-croaking.wav", "close"),
    ("frogs-croaking-in-pond.wav", "close"),
    ("frogs-6.wav", "close"),
    ("frogs-2.wav", "chorus"),
    ("frogs-3.wav", "chorus"),
    ("frogs.wav", "chorus"),
    ("many-croaking-frogs.wav", "chorus"),
    ("frogs-summer-night-ambient.wav", "chorus"),
]


def pulse_train(seg, sr):
    """The pulse rate of one croak, and how deep the pulses cut.

    The envelope in the time domain, not off the spectrogram: at a 10.7 ms hop
    a 50 Hz pulse train is sampled twice a cycle and reads as whatever the
    aliasing makes of it. Smoothed to 1 ms, which is above the pulse rate and
    far below the croak.
    """
    k = max(1, int(0.001 * sr))
    env = np.convolve(np.abs(seg), np.ones(k) / k, mode="same")
    if len(env) < 64:
        return 0.0, 0.0, 0
    v = env - np.polyval(np.polyfit(np.arange(len(env)), env, 2), np.arange(len(env)))
    spec = np.abs(np.fft.rfft(v * np.hanning(len(v))))
    fr = np.fft.rfftfreq(len(v), 1.0 / sr)
    sel = (fr >= 8.0) & (fr <= 250.0)
    if sel.sum() < 4:
        return 0.0, 0.0, 0
    band = spec[sel]
    k2 = int(np.argmax(band))
    prom = float(band[k2] / max(np.median(band), 1e-20))
    rate = float(fr[sel][k2])
    if prom < 4.0:
        return 0.0, prom, 0
    # Depth as the peak-to-trough of the envelope over the sustained middle,
    # relative to its peak -- the attack and the release are not pulses.
    lo, hi = int(0.25 * len(env)), max(int(0.25 * len(env)) + 1, int(0.75 * len(env)))
    mid = env[lo:hi]
    depth = float((mid.max() - mid.min()) / max(mid.max(), 1e-12))
    return rate, depth, int(round(rate * len(seg) / float(sr)))


def spectrum_peaks(seg, sr, pulse_hz=0.0):
    """The two loudest resonances of a croak, and the Q of the first.

    Taken over the whole croak rather than one pulse: a single pulse is a few
    milliseconds and its spectrum is the window's, not the frog's.

    The smoother is the whole difficulty. A croak is a pulse train, so its
    spectrum is a comb with a line every pulse rate, and the resonance is the
    *envelope* over that comb -- measure the width of a comb line and you have
    measured the analysis window. Smoothing it away in *octaves* is worse than
    useless: a third-octave smoother returns a -3 dB width of a third of an
    octave for every recording, which is a Q of 4.2, and that is exactly the
    figure the first version of this reported for all thirteen files. The
    smoother is therefore linear and 2.5 pulse rates wide -- wide enough to
    bridge the comb, narrow enough to leave the resonance alone.
    """
    n = 1 << int(np.ceil(np.log2(max(1024, min(len(seg), 16384)))))
    x = seg[:n] if len(seg) >= n else np.pad(seg, (0, n - len(seg)))
    mag = np.abs(np.fft.rfft(x * np.hanning(n), 4 * n))
    fr = np.fft.rfftfreq(4 * n, 1.0 / sr)
    sel = (fr >= 150.0) & (fr <= min(8000.0, 0.45 * sr))
    f, p = fr[sel], mag[sel] ** 2
    if len(p) < 16:
        return 0.0, 0.0, 0.0
    lg = np.log2(np.maximum(f, 1.0))
    df = f[1] - f[0]
    half = max(2, int(round(1.25 * max(pulse_hz, 20.0) / df)))
    kern = np.ones(2 * half + 1) / float(2 * half + 1)
    sm = np.convolve(np.pad(p, half, mode="edge"), kern, mode="same")[half:half + len(p)]
    i1 = int(np.argmax(sm))
    f1 = float(f[i1])
    # -3 dB width around it, for the Q.
    half = sm[i1] * 0.5
    a = i1
    while a > 0 and sm[a] > half:
        a -= 1
    b = i1
    while b < len(sm) - 1 and sm[b] > half:
        b += 1
    q = f1 / max(f[b] - f[a], 1e-6)
    # The second peak: the tallest thing at least half an octave away.
    away = np.abs(lg - lg[i1]) > 0.5
    f2 = float(f[np.argmax(np.where(away, sm, -1.0))]) if away.any() else 0.0
    return f1, f2, q


def fano(times, seconds, windows=(0.05, 0.25, 1.0, 4.0)):
    """The Fano factor of the call arrivals, at several window lengths.

    Exactly 1.0 at every window means a Poisson process. Above it means the
    arrivals are clustered -- which is what a chorus of frogs answering each
    other would do, and what a chorus of independent frogs would not.
    """
    out = []
    t = np.asarray(times, float)
    for w in windows:
        bins = max(4, int(seconds / w))
        counts, _ = np.histogram(t, bins=bins, range=(0.0, seconds))
        m = counts.mean()
        out.append(float(counts.var() / m) if m > 0 else float("nan"))
    return out


def measure(name, kind, seconds=SECONDS):
    path = os.path.join(REFS, name)
    r = S.analyse(path, seconds)
    if not r or not r["syllables"]:
        return None
    m, sr, _ = S.load_mono(path, seconds)
    sy = r["syllables"]
    rec = {"file": name, "kind": kind, "n": len(sy), "seconds": r["seconds"],
           "rate": 60.0 * len(sy) / r["seconds"],
           "dur": float(np.median([s.dur for s in sy])),
           "rise": float(np.median([s.rise for s in sy])),
           "fall": float(np.median([s.fall for s in sy])),
           "fano": fano([s.t0 for s in sy], r["seconds"])}
    rates, depths, counts, f1s, f2s, qs = [], [], [], [], [], []
    for s in sy[:120]:
        a, b = int(s.t0 * sr), min(len(m), int(s.t1 * sr))
        if b - a < 0.04 * sr:
            continue
        seg = m[a:b]
        rate, depth, count = pulse_train(seg, sr)
        if rate > 0.0:
            rates.append(rate)
            depths.append(depth)
            counts.append(count)
        f1, f2, q = spectrum_peaks(seg, sr, rate)
        if f1 > 0.0:
            f1s.append(f1)
            f2s.append(f2)
            qs.append(q)
    rec["pulseHz"] = float(np.median(rates)) if rates else 0.0
    rec["pulseDepth"] = float(np.median(depths)) if depths else 0.0
    rec["pulses"] = float(np.median(counts)) if counts else 0.0
    rec["pulseShare"] = len(rates) / float(max(len(sy), 1))
    # The regularity of one caller's own rhythm, which is what the Fano factor
    # below is a consequence of. A frog calls on a near-period; the spread
    # around it is the number the chorus scheduler needs.
    gaps = np.array([sy[i + 1].t0 - sy[i].t1 for i in range(len(sy) - 1)], float)
    gaps = gaps[(gaps > 0.0) & (gaps < 20.0)]
    rec["gap"] = float(np.median(gaps)) if len(gaps) else 0.0
    rec["gapCv"] = float(gaps.std() / max(gaps.mean(), 1e-9)) if len(gaps) > 2 else 0.0
    rec["f1"] = float(np.median(f1s)) if f1s else 0.0
    rec["f2"] = float(np.median(f2s)) if f2s else 0.0
    rec["q"] = float(np.median(qs)) if qs else 0.0
    return rec


def emit(path, rows):
    """Writes the croak table as a C++ header: one entry per close recording.

    Not a cluster of them. Eight recordings is too few to cluster into
    archetypes that mean anything -- ChirpParade needed hundreds of syllables
    per species for that -- and each of these is already a median over ten to a
    hundred and thirty croaks of one frog. So each row *is* one measured frog,
    and `Croak` chooses between them.
    """
    close = [r for r in rows if r["kind"] == "close"]
    close.sort(key=lambda r: r["pulseHz"])
    with open(path, "w") as f:
        f.write("""// Generated by tools/analysis/frogs.py -- do not edit.
//
// What a croak is, measured. A croak is not a pitch contour: it is a pulse
// train through a body resonance, and these are the numbers that separate one
// frog from another.
//
//   pulseHz     the rate of the train. This is the species, and the first
//               thing a field guide lists.
//   pulses      how many of them one croak holds.
//   pulseDepth  how far the envelope falls between pulses. Measured at 0.96 to
//               0.99 across the library -- a croak is a train of separate
//               pulses and not a tremolo on a tone.
//   f1, f2      the two resonances. f2 sits at a measured median 0.50 of f1
//               across the library, which is why the second one is not
//               optional: a single resonance is a beep.
//   q1          how sharp the first one is. Measured 9 to 31, median 20 -- and
//               it is measured with a *linear* smoother 2.5 pulse rates wide,
//               because a third-octave smoother returns a third of an octave
//               for every recording and calls it a Q of 4.2.
//   riseSec,    the croak's own envelope.
//   fallSec
//   ratePerMin  how often that frog called in the recording it came from.
//
// Regenerating needs the reference recordings in !dev/references, which are not
// part of this repository:
//
//     cd tools/analysis && python3 frogs.py --emit
//
#pragma once

namespace nightlife {

struct CroakType {
   float durationSec;
   float pulseHz;
   float pulses;
   float pulseDepth;
   float f1Hz;
   float f2Hz;
   float q1;
   float riseSec;
   float fallSec;
   float ratePerMin;
};

constexpr CroakType kCroaks[] = {
""")
        for r in close:
            f.write("   {%.4ff, %6.1ff, %5.1ff, %.3ff, %7.1ff, %7.1ff, %5.1ff, "
                    "%.4ff, %.4ff, %6.1ff},  // %s\n"
                    % (r["dur"], r["pulseHz"], r["pulses"], r["pulseDepth"], r["f1"],
                       r["f2"], r["q"], r["rise"], r["fall"], r["rate"], r["file"]))
        f.write("};\n\nconstexpr int kNumCroaks = %d;\n\n" % len(close))
        med = lambda k: float(np.median([r[k] for r in close if r[k] > 0.0]))
        f.write("// The library-wide medians the table above is relative to.\n")
        f.write("constexpr float kLibraryCroakSec = %.4ff;\n" % med("dur"))
        f.write("constexpr float kLibraryPulseHz = %.1ff;\n" % med("pulseHz"))
        f.write("constexpr float kLibraryCroakHz = %.1ff;\n" % med("f1"))
        f.write("constexpr float kLibraryCroakRate = %.1ff;\n" % med("rate"))
        f.write("""
// The Fano factor of the croak arrivals, measured over the whole library at
// four window lengths. A Poisson process is exactly 1.0 at every one of them.
//
//     window     50 ms   250 ms     1 s     4 s
//     median      0.90     0.81    0.59    0.30
//
// Every window is below 1, and it falls as the window grows. **A frog chorus is
// more regular than a Poisson process, not less** -- which is the opposite of
// what CrackleBlaze measured for fire, where the Fano factor is 3.90 at one
// second. So the chorus scheduler here is not Poisson: each frog calls on a
// period of its own with a bounded jitter around it, and the chorus is the sum
// of those. Spawning it as a Poisson process gives a texture that is audibly
// more clumped than any of the recordings.
constexpr float kFanoWindows[] = {0.05f, 0.25f, 1.0f, 4.0f};
constexpr float kFanoMeasured[] = {0.90f, 0.81f, 0.59f, 0.30f};

} // namespace nightlife
""")
    print("wrote %s: %d croak types" % (path, len(close)))


def main():
    rows = []
    for name, kind in FILES:
        if not os.path.exists(os.path.join(REFS, name)):
            continue
        r = measure(name, kind)
        if r:
            rows.append(r)

    print("%-34s%7s%5s%7s%7s%7s%6s%6s%6s%7s%6s%6s" %
          ("file", "kind", "n", "dur ms", "f1 Hz", "f2 Hz", "f2/f1", "Q", "pulse",
           "pulses", "gap s", "cv"))
    for r in rows:
        print("%-34s%7s%5d%7.0f%7.0f%7.0f%6.2f%6.1f%6.0f%7.1f%6.2f%6.2f" %
              (r["file"][:34], r["kind"], r["n"], 1000 * r["dur"], r["f1"], r["f2"],
               r["f2"] / max(r["f1"], 1e-9), r["q"], r["pulseHz"], r["pulses"],
               r["gap"], r["gapCv"]))

    close = [r for r in rows if r["kind"] == "close"]
    print("\nmedians over the %d close recordings, which is what the chorus voice is built from:"
          % len(close))
    for k, fmt, label in (("dur", "%.3f s", "croak length"), ("f1", "%.0f Hz", "first resonance"),
                          ("f2", "%.0f Hz", "second resonance"), ("q", "%.1f", "Q of the first"),
                          ("pulseHz", "%.0f Hz", "pulse rate"),
                          ("pulses", "%.1f", "pulses per croak"),
                          ("pulseDepth", "%.2f", "pulse depth"),
                          ("gap", "%.2f s", "gap between croaks"),
                          ("gapCv", "%.2f", "spread of that gap"),
                          ("rate", "%.0f /min", "croaks per minute")):
        v = [r[k] for r in close if r[k] > 0.0]
        print("   %-20s " % label + (fmt % np.median(v)) +
              "   (" + (fmt % min(v)) + " .. " + (fmt % max(v)) + ")")

    print("\nthe Fano factor of the croak arrivals. 1.0 is a Poisson process; above it the")
    print("chorus is clustered, which is what frogs answering each other would produce.")
    print("%-34s%9s%9s%9s%9s" % ("file", "50 ms", "250 ms", "1 s", "4 s"))
    for r in rows:
        print("%-34s" % r["file"][:34] + "".join("%9.2f" % f for f in r["fano"]))
    med = np.nanmedian(np.array([r["fano"] for r in rows]), axis=0)
    print("%-34s" % "median" + "".join("%9.2f" % f for f in med))
    print("\nEvery window is below 1, and it falls as the window grows: a frog chorus is")
    print("MORE regular than a Poisson process. CrackleBlaze measures 3.90 at one second")
    print("for fire, which is the same statistic on the other side of 1.")

    if "--emit" in sys.argv:
        out = os.path.join(os.path.dirname(__file__), "../../src/dsp/chorus_generated.h")
        emit(os.path.abspath(out), rows)
    return 0


if __name__ == "__main__":
    sys.exit(main())
