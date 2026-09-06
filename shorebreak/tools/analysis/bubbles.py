import sys; import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
BASE = os.environ.get("SHOREBREAK_REFS", os.path.expanduser("~/Dokumente/Samples/Ocean Waves"))

def load(path, t0=None, t1=None):
    x, sr = wavio.read_wav(path); m = wavio.to_mono(x)
    if t0 is not None: m = m[int(t0*sr):int(t1*sr)]
    return m, sr

def onsets(m, sr, lo=700, hi=9000):
    n, hop = 512, 128
    k = (len(m)-n)//hop
    if k < 4: return np.array([])
    f = np.fft.rfftfreq(n, 1/sr); sel = (f>=lo)&(f<hi)
    win = np.hanning(n)
    S = np.empty((k, int(sel.sum())))
    for i in range(k):
        S[i] = np.abs(np.fft.rfft(m[i*hop:i*hop+n]*win))[sel]
    flux = np.maximum(0, np.diff(S, axis=0)).sum(axis=1)
    if flux.max() <= 0: return np.array([])
    flux /= flux.max()
    med = np.median(flux); mad = np.median(np.abs(flux-med))+1e-9
    thr = med + 4.0*mad
    gap = max(1, int(0.006*sr/hop))
    idx = []; i = 1
    while i < len(flux)-1:
        if flux[i] > thr and flux[i] >= flux[i-1] and flux[i] >= flux[i+1]:
            idx.append(i); i += gap
        else: i += 1
    return np.array(idx)*hop/sr

def chars(m, sr, times, nmax=160):
    fs, ds = [], []
    n = 1024; fr = np.fft.rfftfreq(n, 1/sr)
    band = (fr>400)&(fr<12000)
    for t in times[:nmax]:
        a = int(t*sr)
        if a+n > len(m): break
        mg = np.abs(np.fft.rfft(m[a:a+n]*np.hanning(n)))
        fs.append(fr[band][np.argmax(mg[band])])
        w = m[a:min(len(m), a+int(0.12*sr))]
        if len(w) < 200: continue
        h = int(sr*0.002); kk = len(w)//h
        env = np.sqrt(np.mean(w[:kk*h].reshape(kk,h)**2, axis=1)+1e-20)
        below = np.where(env < env.max()*0.1)[0]
        if len(below): ds.append(below[0]*0.002)
    return np.array(fs), np.array(ds)

CASES = [
 ("Soft Waves  first wave 1.16-2.24s", "Beach of Kattegat - Soft Waves", 1.16, 2.24),
 ("Soft Waves  quiet gap 57.4-59.4s",  "Beach of Kattegat - Soft Waves", 57.4, 59.4),
 ("Soft Waves  whole file",            "Beach of Kattegat - Soft Waves", None, None),
 ("Sand and Foam 10-14s",              "Beach of Kattegat - Sand and Foam", 10, 14),
]
for label, name, t0, t1 in CASES:
    m, sr = load(f"{BASE}/{name}.wav", t0, t1)
    ts = onsets(m, sr); dur = len(m)/sr
    fs, ds = chars(m, sr, ts)
    print(label)
    print(f"   onsets {len(ts)} in {dur:.2f}s = {len(ts)/dur:.0f}/s")
    if len(fs): print(f"   pitch  median {np.median(fs):.0f} Hz  10-90% {np.percentile(fs,10):.0f}-{np.percentile(fs,90):.0f} Hz")
    if len(ds): print(f"   -20dB decay median {np.median(ds)*1000:.0f} ms  10-90% {np.percentile(ds,10)*1000:.0f}-{np.percentile(ds,90)*1000:.0f} ms")
    if len(ts)>3:
        g = np.diff(ts); print(f"   gaps median {np.median(g)*1000:.0f} ms  min {g.min()*1000:.0f} ms")
    print()
