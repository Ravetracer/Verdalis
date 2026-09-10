"""Is a reference's low end water, or is it the microphone?

    python3 lowend.py <file>...

Six of the recordings carry 30-60 Hz energy within 10 dB of their loudest
band. By Minnaert, 32 Hz is a 100 mm air pocket -- possible in the plunge pool
under a big fall, out of the question in a creek. The alternative is wind or
handling noise on the microphone, and the two can be told apart: water in the
low band is made by the same events as the water above it, so its envelope
correlates with the mid band's. Wind does not know about the water at all.

Prints the correlation of the 20-60 Hz envelope with the 1-4 kHz envelope, and
what fraction of total energy sits below 60 Hz.
"""
import sys, os, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import grain

print("%-52s %8s %10s %10s" % ("reference", "corr", "<60Hz %", "<60Hz dB"))
for p in sys.argv[1:]:
    x, sr = wavio.read_wav(p)
    n = int(min(20.0 * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = wavio.to_mono(x[st:st + n])
    lo = grain.band_envelope(m, sr, 20.0, 60.0, env_ms=25.0)
    hi = grain.band_envelope(m, sr, 1000.0, 4000.0, env_ms=25.0)
    k = min(len(lo), len(hi))
    a, b = lo[:k] - lo[:k].mean(), hi[:k] - hi[:k].mean()
    d = math.sqrt(float(np.sum(a * a) * np.sum(b * b)))
    corr = float(np.sum(a * b) / d) if d > 0 else float("nan")
    tot = float(np.mean(m ** 2))
    sub = float(np.mean(grain.band_envelope(m, sr, 20.0, 60.0, env_ms=25.0) ** 2))
    print("%-52s %8.2f %10.2f %10.1f" % (os.path.basename(p)[:52], corr,
          100.0 * sub / max(tot, 1e-30), 10 * math.log10(max(sub / max(tot, 1e-30), 1e-12))))
