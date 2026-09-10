"""Is a river's slow level movement periodic, or is it a random walk?

    python3 surge.py <file>...

refs.py reports a strongest modulation frequency for every recording, but a
peak-picker always returns something. The question that decides how the engine
generates the movement is whether the envelope spectrum has a *peak* -- water
arriving in a rhythm -- or falls off smoothly as 1/f^a, which is a random walk
with no rhythm at all. A sine LFO is right for the first and wrong for the
second.

Prints the slope of the envelope spectrum over 0.3-10 Hz, the height of the
strongest peak above that fitted line, and the modulation depth.
"""
import sys, os, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import grain

print("%-50s %8s %9s %9s %8s" % ("reference", "slope", "peak Hz", "peak dB", "depth %"))
rows = []
for p in sys.argv[1:]:
    x, sr = wavio.read_wav(p)
    n = int(min(30.0 * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = wavio.to_mono(x[st:st + n])
    e = grain.band_envelope(m, sr, 300.0, 3000.0, env_ms=20.0)
    rate = 1000.0 / 20.0
    mu = e.mean()
    y = e - mu
    k = 1 << int(math.floor(math.log2(len(y))))
    y = y[:k] * np.hanning(k)
    mag = np.abs(np.fft.rfft(y)) ** 2
    f = np.fft.rfftfreq(k, 1.0 / rate)
    sel = (f >= 0.3) & (f <= 10.0)
    lf, lm = np.log10(f[sel]), 10.0 * np.log10(mag[sel] + 1e-30)
    a = np.polyfit(lf, lm, 1)
    resid = lm - np.polyval(a, lf)
    i = int(np.argmax(resid))
    depth = 100.0 * e.std() / max(mu, 1e-12)
    rows.append((a[0], f[sel][i], resid[i], depth))
    print("%-50s %8.2f %9.2f %9.1f %8.1f" % (os.path.basename(p)[:50], a[0], f[sel][i], resid[i], depth))
A = np.array(rows)
print()
print("slope   dB/decade  min %6.2f median %6.2f max %6.2f" % (A[:,0].min(), np.median(A[:,0]), A[:,0].max()))
print("peak    excess dB  min %6.1f median %6.1f max %6.1f" % (A[:,2].min(), np.median(A[:,2]), A[:,2].max()))
print("depth   %%          min %6.1f median %6.1f max %6.1f" % (A[:,3].min(), np.median(A[:,3]), A[:,3].max()))
