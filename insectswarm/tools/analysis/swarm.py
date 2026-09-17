"""Measures what separates one insect from many: harmonicity, pitch dispersion, the bed.

    python3 swarm.py [--class bee] [dir]

`wingbeat.py` says what one voice is. This says how many of them there are and
what sits underneath them, which is the difference between a bee and a hive.

- **HNR**, the harmonic-to-noise ratio: the share of band energy that lies
  within +-4% of a harmonic of the estimated fundamental, against everything
  else in 60-4000 Hz. A single close insect is strongly harmonic; a hive is
  many fundamentals at once and measures as noise, so a low HNR with a high
  voiced fraction is the signature of a swarm rather than of a bad recording.
- **dispersion**: the interquartile spread of the per-frame fundamental, in
  cents. In a swarm this is the spread of wingbeat rates across individuals.
- **the bed**: the octave-band spectrum of what is left after the harmonics are
  notched out -- the wing noise and the air, which the engine generates as a
  separate layer.
- **width**: the mid/side ratio, where the recording is stereo.
"""
import sys, os, glob, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
from refs import classify, DEFAULT_DIR, OCT
from cache import cached_dir
import pitch
from wingbeat import F0_LO, F0_HI, BAND, FRAME_SEC, MIN_CLARITY

MAX_SEC = 120.0
HARM_TOL = 0.04             # +-4% of a harmonic counts as harmonic energy
N_HARM = 20


def measure(path, max_sec=MAX_SEC):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    if len(m) > int(max_sec * sr):
        s = (len(m) - int(max_sec * sr)) // 2
        m = m[s:s + int(max_sec * sr)]
    n = 1 << int(round(math.log2(FRAME_SEC * sr)))
    if len(m) < n * 4:
        return None
    hop, win = n // 2, np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1.0 / sr)
    inband = (freqs >= BAND[0]) & (freqs <= BAND[1])
    lo_lag, hi_lag = sr / F0_HI, sr / F0_LO
    rms_all = math.sqrt(float(np.mean(m * m)) + 1e-30)
    hnrs, f0s, beds = [], [], []
    for i in range(0, len(m) - n, hop):
        seg = m[i:i + n]
        if math.sqrt(float(np.mean(seg * seg))) < 0.05 * rms_all:
            continue
        b = pitch.bandpass(seg, sr, *BAND)
        p, c = pitch.pick_period(pitch.nsdf(b), lo_lag, hi_lag)
        if p is None or c < MIN_CLARITY:
            continue
        f0 = sr / p
        pw = np.abs(np.fft.rfft(seg * win)) ** 2
        mask = np.zeros(len(freqs), bool)
        for k in range(1, N_HARM + 1):
            fk = k * f0
            if fk > BAND[1]:
                break
            mask |= np.abs(freqs - fk) <= HARM_TOL * fk
        h = pw[inband & mask].sum()
        nse = pw[inband & ~mask].sum()
        hnrs.append(10.0 * math.log10(max(h, 1e-30) / max(nse, 1e-30)))
        f0s.append(f0)
        beds.append(pw * ~mask)
    if len(f0s) < 4:
        return None
    f0s = np.array(f0s)
    med = float(np.median(f0s))
    keep = np.abs(np.log2(f0s / med)) < 0.585
    f0k = f0s[keep]
    q1, q3 = np.percentile(f0k, [25, 75])
    bed = np.mean(np.array(beds), axis=0)
    bands = []
    for fc in OCT:
        sel = (freqs >= fc / math.sqrt(2)) & (freqs < fc * math.sqrt(2))
        bands.append(bed[sel].sum() if sel.any() else 1e-30)
    bands = 10.0 * np.log10(np.maximum(np.array(bands), 1e-30) / max(np.sum(bands), 1e-30))
    width = 0.0
    if x.shape[1] >= 2:
        L, R = x[:, 0], x[:, 1]
        mid = np.sqrt(np.mean(((L + R) * 0.5) ** 2) + 1e-30)
        side = np.sqrt(np.mean(((L - R) * 0.5) ** 2) + 1e-30)
        width = 20.0 * math.log10(side / mid)
    return dict(hnr=float(np.median(hnrs)), f0=med,
                disp=1200.0 * math.log2(q3 / max(q1, 1e-9)),
                bed=bands, width=width, n=len(f0k))


def main():
    args = sys.argv[1:]
    want = args[args.index("--class") + 1] if "--class" in args else None
    # stereo width needs the originals; the cache is mono
    d = next((a for a in args if os.path.isdir(a)), cached_dir())
    orig = DEFAULT_DIR
    print("%-50s %-9s %7s %7s %7s %7s  %s" %
          ("file", "class", "f0", "HNR", "disp_ct", "width", "bed 63..16k (dB)"), flush=True)
    rows = []
    for p in sorted(glob.glob(os.path.join(d, "*.wav"))):
        name = os.path.basename(p)
        cls = classify(name)
        if want and cls != want:
            continue
        r = measure(p)
        if r is None:
            print("%-50s %-9s  (unvoiced)" % (name[:50], cls), flush=True); continue
        op = os.path.join(orig, name)
        if os.path.exists(op):
            try:
                xo, _ = wavio.read_wav(op)
                if xo.shape[1] >= 2:
                    L, R = xo[:, 0], xo[:, 1]
                    mid = math.sqrt(float(np.mean(((L + R) * 0.5) ** 2)) + 1e-30)
                    side = math.sqrt(float(np.mean(((L - R) * 0.5) ** 2)) + 1e-30)
                    r["width"] = 20.0 * math.log10(side / mid)
                else:
                    r["width"] = float("nan")
            except Exception:
                pass
        print("%-50s %-9s %7.1f %7.1f %7.0f %7.1f  %s" %
              (name[:50], cls, r["f0"], r["hnr"], r["disp"], r["width"],
               " ".join("%5.1f" % v for v in r["bed"])), flush=True)
        rows.append((cls, r))

    print("\n%-10s %3s %8s %7s %8s %7s  %s" %
          ("class", "n", "f0", "HNR", "disp_ct", "width", "bed 63..16k (dB)"))
    for cls in sorted(set(r[0] for r in rows)):
        sel = [r[1] for r in rows if r[0] == cls]
        B = np.median(np.array([s["bed"] for s in sel]), axis=0)
        w = [s["width"] for s in sel if s["width"] == s["width"]]
        print("%-10s %3d %8.1f %7.1f %8.0f %7.1f  %s" % (
            cls, len(sel), np.median([s["f0"] for s in sel]),
            np.median([s["hnr"] for s in sel]), np.median([s["disp"] for s in sel]),
            np.median(w) if w else float("nan"),
            " ".join("%5.1f" % v for v in B)))


if __name__ == "__main__":
    main()
