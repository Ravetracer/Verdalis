"""Are crackles Poisson, or do they arrive in trains?

    python3 bursts.py [file...]

crackles.py puts the interval CV at 1.2-3.4 on the clean references against a
Poisson process's 1.0, so they are bursty. This says *how*. Three measures:

  Fano       variance/mean of the count in a 1 s window. Poisson gives 1.
  L(t)       the fraction of intervals shorter than t, against the exponential
             with the same mean. An excess at short t is a train.
  after      the probability that another crackle follows within 50 ms, and the
             mean number that do -- the branching ratio of a self-exciting
             process. Above 0 and below 1 is a Hawkes cascade; 0 is Poisson.
"""
import sys, os, math, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import crackles

DEFAULT_DIR = crackles.DEFAULT_DIR
SECS = 40.0


def onsets(path, secs=SECS):
    x, sr = wavio.read_wav(path)
    n = int(min(secs * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = wavio.to_mono(x[st:st + n])
    hi = min(crackles.BAND[1], sr * 0.45)
    env, hop, _ = crackles.band_env(m, sr, crackles.BAND[0], hi)
    rate = sr / hop
    w = int(0.2 * rate) | 1
    pad = np.pad(env, w // 2, mode="edge")
    med = np.array([np.median(pad[i:i + w]) for i in range(0, len(env), 8)])
    med = np.interp(np.arange(len(env)), np.arange(len(med)) * 8, med)
    mad = np.median(np.abs(env - med)) * 1.4826 + 1e-30
    thr = med + 6.0 * mad
    idx, i, gap = [], 1, max(1, int(0.004 * rate))
    while i < len(env) - 1:
        if env[i] > thr[i] and env[i] >= env[i - 1] and env[i] > env[i + 1]:
            idx.append(i); i += gap
        else:
            i += 1
    t = np.array(idx, float) / rate
    amp = env[np.array(idx, int)] / np.maximum(med[np.array(idx, int)], 1e-30) if len(idx) else np.array([])
    return t, amp, n / sr


print("%-46s %6s %6s %7s %7s %7s %7s %7s" % (
    "reference", "rate", "Fano", "P<10ms", "exp", "P<50ms", "exp", "branch"))
rows = []
for p in (sys.argv[1:] or sorted(glob.glob(os.path.join(DEFAULT_DIR, "*.wav")))):
    t, amp, dur = onsets(p)
    if len(t) < 40:
        continue
    iv = np.diff(t)
    lam = len(t) / dur
    counts = np.histogram(t, bins=int(dur))[0]
    fano = counts.var() / max(counts.mean(), 1e-12)
    p10 = float((iv < 0.010).mean()); e10 = 1 - math.exp(-lam * 0.010)
    p50 = float((iv < 0.050).mean()); e50 = 1 - math.exp(-lam * 0.050)
    # branching ratio: excess events inside 50 ms of an event over the Poisson
    # expectation, per event.
    near = float(((iv < 0.050)).sum())
    branch = (near - len(iv) * e50) / max(len(t), 1)
    rows.append((fano, p10 / max(e10, 1e-9), p50 / max(e50, 1e-9), branch,
                 float(np.log10(np.maximum(amp, 1e-9)).std())))
    print("%-46s %6.1f %6.2f %7.2f %7.2f %7.2f %7.2f %7.2f" % (
        os.path.basename(p)[:46], lam, fano, p10, e10, p50, e50, branch))
a = np.array(rows)
print()
print("%-14s %8s %8s %8s" % ("", "min", "median", "max"))
for i, k in enumerate(("Fano", "P<10/exp", "P<50/exp", "branch", "log10amp sd")):
    print("%-14s %8.2f %8.2f %8.2f" % (k, a[:, i].min(), np.median(a[:, i]), a[:, i].max()))
