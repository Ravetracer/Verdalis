import sys; import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio, os

def grain(path, t0=None, t1=None):
    """Peakiness of the 4 ms envelope inside the loudest stretch, in the band
    where bubbles live. A cascade of discrete events fluctuates strongly; a
    smooth noise sweep does not."""
    x, sr = wavio.read_wav(path); m = wavio.to_mono(x)
    if t0 is not None: m = m[int(t0*sr):int(t1*sr)]
    # band-limit to where the bubbles are
    n = len(m); F = np.fft.rfft(m); f = np.fft.rfftfreq(n, 1/sr)
    F[(f < 600) | (f >= 6000)] = 0
    y = np.fft.irfft(F, n)
    h = max(1, int(sr*0.004)); k = len(y)//h
    e = np.sqrt(np.mean(y[:k*h].reshape(k,h)**2, axis=1) + 1e-20)
    # the loudest 20% of the file, so we compare breaks with breaks
    sm = np.convolve(e, np.ones(50)/50, 'same')
    sel = e[sm >= np.percentile(sm, 80)]
    if len(sel) < 50: return None
    return sel.std()/sel.mean(), np.percentile(sel,99)/np.median(sel)

B = os.environ.get("SHOREBREAK_REFS", os.path.expanduser("~/Dokumente/Samples/Ocean Waves"))
print(f"{'':<34}{'grainCV':>8}{'p99/med':>9}")
refs = [("REF Soft Waves","Beach of Kattegat - Soft Waves",None,None),
        ("REF Soft Waves first wave","Beach of Kattegat - Soft Waves",1.16,2.24),
        ("REF Soft Waves quiet gap","Beach of Kattegat - Soft Waves",57.4,59.4),
        ("REF Big Waves Crashing","Kattegat Uproar - Big Waves Crashing",None,None),
        ("REF Sand and Foam","Beach of Kattegat - Sand and Foam",None,None),
        ("REF Gentle Waves","The City Coast - Gentle Waves",None,None)]
for label, name, a, b in refs:
    r = grain(f"{B}/{name}.wav", a, b)
    if r: print(f"{label:<34}{r[0]:>8.2f}{r[1]:>9.2f}")
print()
for f in ["Gentle_Waves","Sand_and_Foam","Big_Waves_Crashing","Rolling_Tide","Rhythmic_Tide"]:
    p = os.path.join(sys.argv[1], f+".wav")
    if not os.path.exists(p): continue
    r = grain(p)
    if r: print(f"{'SB '+f:<34}{r[0]:>8.2f}{r[1]:>9.2f}")
