import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
SPEED = 0.40          # the files play at 40% of the original speed
F = 1.0/SPEED         # frequencies and rates scale up by this to get real values

def onsets(m, sr, lo, hi, min_gap):
    n, hop = 1024, 128
    k = (len(m)-n)//hop
    f = np.fft.rfftfreq(n, 1/sr); sel = (f>=lo)&(f<hi); win = np.hanning(n)
    S = np.empty((k, int(sel.sum())))
    for i in range(k): S[i] = np.abs(np.fft.rfft(m[i*hop:i*hop+n]*win))[sel]
    flux = np.maximum(0, np.diff(S, axis=0)).sum(axis=1)
    flux /= max(flux.max(), 1e-12)
    med = np.median(flux); mad = np.median(np.abs(flux-med))+1e-9
    thr = med + 4.0*mad; gap = max(1, int(min_gap*sr/hop))
    idx=[]; i=1
    while i < len(flux)-1:
        if flux[i] > thr and flux[i] >= flux[i-1] and flux[i] >= flux[i+1]: idx.append(i); i+=gap
        else: i+=1
    return np.array(idx)*hop/sr

def chars(m, sr, ts, lo, hi):
    n=2048; fr=np.fft.rfftfreq(n,1/sr); band=(fr>lo*0.7)&(fr<hi*1.3)
    fs, ds = [], []
    for t in ts[:400]:
        a=int(t*sr)
        if a+n>len(m): break
        mg=np.abs(np.fft.rfft(m[a:a+n]*np.hanning(n)))
        fs.append(fr[band][np.argmax(mg[band])])
        w=m[a:min(len(m),a+int(0.30*sr))]
        if len(w)<400: continue
        h=int(sr*0.004); kk=len(w)//h
        env=np.sqrt(np.mean(w[:kk*h].reshape(kk,h)**2,axis=1)+1e-20)
        below=np.where(env<env.max()*0.1)[0]
        if len(below): ds.append(below[0]*0.004)
    return np.array(fs), np.array(ds)

for path in sorted(sys.argv[1:]):
    x, sr = wavio.read_wav(path); m = wavio.to_mono(x)
    name = os.path.basename(path).split("_slowed")[0]
    dur = len(m)/sr
    # split into a loud phase (the break) and the quieter tail (the foam)
    h = int(sr*0.02); k = len(m)//h
    e = np.sqrt(np.mean(m[:k*h].reshape(k,h)**2, axis=1)+1e-20)
    sm = np.convolve(e, np.ones(25)/25, 'same')
    loud = sm >= np.percentile(sm, 70)
    print(f"=== {name}   ({dur:.2f} s slowed = {dur*SPEED:.2f} s real)")
    for label, mask in (("break/bubbling (loud 30%)", loud), ("foam fizzle (quiet 70%)", ~loud)):
        seg = np.concatenate([m[i*h:(i+1)*h] for i in np.nonzero(mask)[0]])
        if len(seg) < sr//2: continue
        sdur = len(seg)/sr
        # the analysis band and gap are scaled into the slowed domain
        ts = onsets(seg, sr, 700*SPEED, 9000*SPEED, 0.006/SPEED)
        fs, ds = chars(seg, sr, ts, 700*SPEED, 9000*SPEED)
        rate_real = (len(ts)/sdur) * F
        print(f"  {label}: {sdur*SPEED:.2f} s real")
        print(f"     onsets {len(ts)}  ->  {rate_real:.0f}/s real")
        if len(fs):
            print(f"     pitch  median {np.median(fs)*F:.0f} Hz real   10-90%% {np.percentile(fs,10)*F:.0f}-{np.percentile(fs,90)*F:.0f} Hz")
        if len(ds):
            print(f"     -20dB decay median {np.median(ds)*SPEED*1000:.0f} ms real   10-90%% {np.percentile(ds,10)*SPEED*1000:.0f}-{np.percentile(ds,90)*SPEED*1000:.0f} ms")
        if len(ts)>3:
            g=np.diff(ts)*SPEED
            print(f"     gaps median {np.median(g)*1000:.0f} ms real   min {g.min()*1000:.0f} ms")
    print()
