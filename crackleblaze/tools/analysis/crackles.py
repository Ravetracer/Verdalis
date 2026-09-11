"""What a crackle is: rate, amplitude law, clustering, decay and spectrum.

    python3 crackles.py [file...]

A fire's transients are not water's. refs.py puts the library's crest factor at
a median of 31.7 dB against a river's 19.6, so a detector tuned for water finds
almost nothing here and one tuned for noise finds the bed. This measures the
population that carries that crest: onsets in the 1-12 kHz band, well above the
local floor, and for each one the decay, the spectrum with the bed subtracted,
and the peak height over the bed it sat on.

The interval distribution is the question that decides the engine: a Poisson
process has a coefficient of variation of 1 on its intervals. Anything much
above that is bursty -- crackles arriving in trains, which is what a log falling
apart actually does.
"""
import sys, os, math, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio

DEFAULT_DIR = os.path.join(os.path.dirname(__file__), "../../!dev/references")
SECS = 30.0
BAND = (1000.0, 12000.0)
ENV_MS = 0.5


def band_env(m, sr, lo, hi, env_ms=ENV_MS):
    n = 1 << int(math.ceil(math.log2(len(m))))
    X = np.fft.rfft(m, n)
    f = np.fft.rfftfreq(n, 1.0 / sr)
    X[(f < lo) | (f > hi)] = 0.0
    y = np.fft.irfft(X)[:len(m)]
    hop = max(1, int(round(sr * env_ms / 1000.0)))
    k = len(y) // hop
    return np.sqrt(np.mean(y[:k * hop].reshape(k, hop) ** 2, axis=1) + 1e-30), hop, y


def floor_db(m, sr):
    """Mean level of the 14-18 kHz octave, as a proxy for the dither floor."""
    n = 1 << int(math.ceil(math.log2(len(m))))
    X = np.abs(np.fft.rfft(m, n)) ** 2
    f = np.fft.rfftfreq(n, 1.0 / sr)
    sel = (f > 14000) & (f < min(18000, sr * 0.45))
    ref = (f > 200) & (f < 6000)
    if not sel.any() or not ref.any():
        return float("nan")
    return 10 * math.log10(X[sel].mean() / max(X[ref].mean(), 1e-30))


def measure(path, secs=SECS):
    x, sr = wavio.read_wav(path)
    n = int(min(secs * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = wavio.to_mono(x[st:st + n])
    dur = n / sr
    hi = min(BAND[1], sr * 0.45)
    env, hop, y = band_env(m, sr, BAND[0], hi)
    rate_hz = sr / hop

    # Local median floor over a 200 ms window, and the MAD about it.
    w = int(0.2 * rate_hz) | 1
    pad = np.pad(env, w // 2, mode="edge")
    med = np.array([np.median(pad[i:i + w]) for i in range(0, len(env), 8)])
    med = np.interp(np.arange(len(env)), np.arange(len(med)) * 8, med)
    mad = np.median(np.abs(env - med)) * 1.4826 + 1e-30

    thr = med + 6.0 * mad
    idx = []
    gap = max(1, int(0.004 * rate_hz))
    i = 1
    while i < len(env) - 1:
        if env[i] > thr[i] and env[i] >= env[i - 1] and env[i] > env[i + 1]:
            idx.append(i)
            i += gap
        else:
            i += 1
    idx = np.array(idx, int)
    if len(idx) < 8:
        return dict(name=os.path.basename(path), n=len(idx), rate=len(idx) / dur,
                    floor=floor_db(m, sr), sr=sr, dur=dur)

    peak = env[idx]
    bed = med[idx]
    prom = 20.0 * np.log10(peak / np.maximum(bed, 1e-30))

    # Decay: time from the peak to 10 dB below it, in the same band envelope.
    dec = []
    for i in idx:
        p = env[i]
        j = i
        lim = min(len(env), i + int(0.3 * rate_hz))
        while j < lim - 1 and env[j] > p * 0.3162:
            j += 1
        dec.append((j - i) / rate_hz * 1000.0)
    dec = np.array(dec)

    # Event-triggered spectrum with the bed just before it subtracted.
    NF = 512
    acc = np.zeros(NF // 2 + 1)
    k = 0
    for i in idx:
        a = i * hop
        if a - NF < 0 or a + NF >= len(m):
            continue
        ev = np.abs(np.fft.rfft(m[a:a + NF] * np.hanning(NF))) ** 2
        bg = np.abs(np.fft.rfft(m[a - NF:a] * np.hanning(NF))) ** 2
        acc += np.maximum(ev - bg, 0.0)
        k += 1
    f = np.fft.rfftfreq(NF, 1.0 / sr)
    cent = float((f * acc).sum() / max(acc.sum(), 1e-30)) if k else float("nan")

    iv = np.diff(idx) / rate_hz
    cv = float(iv.std() / max(iv.mean(), 1e-12)) if len(iv) > 4 else float("nan")

    # Amplitude law: is log10(peak/bed) normal? Report the dB spread.
    return dict(name=os.path.basename(path), n=len(idx), rate=len(idx) / dur,
                prom_med=float(np.median(prom)), prom_sd=float(prom.std()),
                prom_max=float(prom.max()),
                dec_med=float(np.median(dec)), dec_lo=float(np.percentile(dec, 10)),
                dec_hi=float(np.percentile(dec, 90)),
                cent=cent, cv=cv, iv_med=float(np.median(iv)),
                floor=floor_db(m, sr), sr=sr, dur=dur)


def main():
    files = sys.argv[1:] or sorted(glob.glob(os.path.join(DEFAULT_DIR, "*.wav")))
    rows = []
    print("%-48s %6s %6s %7s %6s %6s %6s %6s %7s %6s" % (
        "reference", "n", "rate/s", "prom dB", "sd", "dec ms", "d10", "d90", "cent", "ivCV"))
    for p in files:
        r = measure(p)
        rows.append(r)
        if "prom_med" not in r:
            print("%-48s %6d %6.2f   (too few events)" % (r["name"][:48], r["n"], r["rate"]))
            continue
        print("%-48s %6d %6.2f %7.1f %6.1f %6.1f %6.1f %6.1f %7.0f %6.2f" % (
            r["name"][:48], r["n"], r["rate"], r["prom_med"], r["prom_sd"],
            r["dec_med"], r["dec_lo"], r["dec_hi"], r["cent"], r["cv"]))
    rows = [r for r in rows if "prom_med" in r]
    print()
    for k in ("rate", "prom_med", "prom_sd", "dec_med", "cent", "cv"):
        v = np.array([r[k] for r in rows], float)
        v = v[np.isfinite(v)]
        print("%-10s min %8.2f   median %8.2f   max %8.2f" % (k, v.min(), np.median(v), v.max()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
