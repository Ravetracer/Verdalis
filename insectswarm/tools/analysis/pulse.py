"""Measures the stridulators: carrier band, pulse rate, echeme rate and duty cycle.

    python3 pulse.py [--class cicada] [dir]

A cicada's tymbal and a cricket's file-and-scraper do not beat the air the way a
wing does. Both produce a train of clicks, each click ringing a resonant body,
so the model is a carrier at the resonance, amplitude-modulated by a pulse train
-- not a harmonic stack on a fundamental. `wingbeat.py` measures these two
classes as ~950 Hz with the energy 25 dB up at the fourth harmonic, which is
that estimator failing on the wrong mechanism rather than a result.

What is measured here, on the amplitude envelope rather than the waveform:

- the carrier: the spectral peak and the -6 dB bandwidth around it, which is
  the resonator's Q;
- the pulse rate: envelope periodicity in the 20-400 Hz band, the clicks
  inside one chirp;
- the echeme rate: envelope periodicity in the 0.3-20 Hz band, the chirps
  themselves, and the fraction of the time they are sounding.
"""
import sys, os, glob, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
from refs import classify
from cache import cached_dir
import pitch

ENV_SR = 2000.0             # the envelope is decimated to this before any ACF
PULSE_BAND = (20.0, 400.0)
ECHEME_BAND = (0.3, 20.0)
MAX_SEC = 120.0
STRIDULATORS = ("cicada", "cricket")


def envelope(m, sr, env_sr=ENV_SR):
    """Rectified, smoothed and decimated amplitude envelope."""
    step = max(int(round(sr / env_sr)), 1)
    a = np.abs(m)
    n = (len(a) // step) * step
    e = a[:n].reshape(-1, step).mean(axis=1)
    return e, sr / float(step)


def env_rate(e, esr, band, clarity_floor=0.15):
    """Dominant periodicity of the envelope inside a band, by NSDF."""
    x = e - e.mean()
    if np.allclose(x, 0.0):
        return None, 0.0
    d = pitch.nsdf(x)
    p, c = pitch.pick_period(d, esr / band[1], esr / band[0], k=0.92)
    if p is None or c < clarity_floor:
        return None, float(c)
    return esr / p, float(c)


def carrier(m, sr, n=1 << 14):
    """Peak frequency of the averaged spectrum, and the -6 dB width around it."""
    if len(m) < n:
        n = 1 << int(math.floor(math.log2(len(m))))
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    acc = np.zeros(len(freqs)); frames = 0
    for i in range(0, len(m) - n, n // 2):
        acc += np.abs(np.fft.rfft(m[i:i + n] * win)) ** 2
        frames += 1
    if frames == 0:
        return 0.0, 0.0, 0.0
    acc /= frames
    acc[freqs < 300.0] = 0.0            # below 300 Hz is wind and traffic
    k = int(np.argmax(acc))
    peak = acc[k]
    half = peak * 10 ** (-0.6)          # -6 dB
    lo = k
    while lo > 0 and acc[lo] > half:
        lo -= 1
    hi = k
    while hi < len(acc) - 1 and acc[hi] > half:
        hi += 1
    f0 = freqs[k]
    bw = max(freqs[hi] - freqs[lo], freqs[1])
    return f0, bw, f0 / bw


def duty(e, thresh_db=-12.0):
    """Share of the envelope within `thresh_db` of its 95th percentile."""
    p95 = np.percentile(e, 95)
    if p95 <= 0:
        return 0.0
    return float(np.mean(e > p95 * 10 ** (thresh_db / 20.0)))


def measure(path, max_sec=MAX_SEC):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    if len(m) > int(max_sec * sr):
        s = (len(m) - int(max_sec * sr)) // 2
        m = m[s:s + int(max_sec * sr)]
    if len(m) < sr:
        return None
    fc, bw, q = carrier(m, sr)
    # the envelope is taken from the carrier band alone, so the pulse rate is
    # the insect's and not whatever else is in the recording
    band = pitch.bandpass(m, sr, max(fc - bw, 200.0), fc + bw)
    e, esr = envelope(band, sr)
    pr, pc = env_rate(e, esr, PULSE_BAND)
    er, ec = env_rate(e, esr, ECHEME_BAND)
    return dict(fc=fc, bw=bw, q=q, pulse=pr, pulse_c=pc, echeme=er, echeme_c=ec,
                duty=duty(e), crest=20.0 * math.log10(
                    float(np.max(np.abs(m))) / (math.sqrt(float(np.mean(m * m))) + 1e-30) + 1e-30))


def main():
    args = sys.argv[1:]
    want = args[args.index("--class") + 1] if "--class" in args else None
    d = next((a for a in args if os.path.isdir(a)), cached_dir())
    print("%-52s %-9s %7s %7s %6s %8s %5s %8s %5s %6s" %
          ("file", "class", "carr", "bw", "Q", "pulse", "clar", "echeme", "clar", "duty"), flush=True)
    rows = []
    for p in sorted(glob.glob(os.path.join(d, "*.wav"))):
        name = os.path.basename(p)
        cls = classify(name)
        if (want and cls != want) or (not want and cls not in STRIDULATORS):
            continue
        r = measure(p)
        if r is None:
            print("%-52s %-9s  (too short)" % (name[:52], cls), flush=True); continue
        print("%-52s %-9s %7.0f %7.0f %6.1f %8s %5.2f %8s %5.2f %6.2f" %
              (name[:52], cls, r["fc"], r["bw"], r["q"],
               "%.1f" % r["pulse"] if r["pulse"] else "-", r["pulse_c"],
               "%.2f" % r["echeme"] if r["echeme"] else "-", r["echeme_c"], r["duty"]), flush=True)
        rows.append((cls, r))

    print("\n%-10s %3s %8s %8s %6s %8s %8s %6s" %
          ("class", "n", "carr", "bw", "Q", "pulse", "echeme", "duty"))
    for cls in sorted(set(r[0] for r in rows)):
        sel = [r[1] for r in rows if r[0] == cls]
        pu = [s["pulse"] for s in sel if s["pulse"]]
        ec = [s["echeme"] for s in sel if s["echeme"]]
        print("%-10s %3d %8.0f %8.0f %6.1f %8s %8s %6.2f" % (
            cls, len(sel), np.median([s["fc"] for s in sel]), np.median([s["bw"] for s in sel]),
            np.median([s["q"] for s in sel]),
            "%.1f" % np.median(pu) if pu else "-", "%.2f" % np.median(ec) if ec else "-",
            np.median([s["duty"] for s in sel])))


if __name__ == "__main__":
    main()
