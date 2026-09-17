"""Measures a single pass: level trajectory, Doppler ratio and the speed that implies.

    python3 flyby.py [file ...]

A flyby is what makes an insect read as moving rather than as getting louder, and
it carries two independent measurements in one gesture. The level rises and falls
with distance; the pitch falls once, monotonically, as the insect goes from
approaching to receding. The ratio of the two plateaux is

    f_approach / f_recede = (c + v) / (c - v)

so the observed ratio gives the flight speed directly, with no assumption about
how far away it passed.

Files are scored by how well they fit that shape and the ones that do not are
reported as such -- most of the library is not a clean pass.
"""
import sys, os, glob, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
from refs import classify
from cache import cached_dir
import pitch
from wingbeat import F0_LO, F0_HI, BAND, FRAME_SEC

C_AIR = 343.0
MIN_CLARITY = 0.40


def track(path, max_sec=30.0):
    x, sr = wavio.read_wav(path)
    m = wavio.to_mono(x)
    if len(m) > int(max_sec * sr):
        m = m[:int(max_sec * sr)]
    n = 1 << int(round(math.log2(FRAME_SEC * sr)))
    hop = n // 4
    if len(m) < n * 4:
        return None
    lo_lag, hi_lag = sr / F0_HI, sr / F0_LO
    t, f0, lev = [], [], []
    for i in range(0, len(m) - n, hop):
        seg = m[i:i + n]
        r = math.sqrt(float(np.mean(seg * seg)) + 1e-30)
        b = pitch.bandpass(seg, sr, *BAND)
        p, c = pitch.pick_period(pitch.nsdf(b), lo_lag, hi_lag)
        t.append(i / float(sr))
        lev.append(20.0 * math.log10(r))
        f0.append(sr / p if (p is not None and c >= MIN_CLARITY) else float("nan"))
    return np.array(t), np.array(f0), np.array(lev), len(m) / float(sr)


def analyse(path):
    r = track(path)
    if r is None:
        return None
    t, f0, lev, dur = r
    ok = ~np.isnan(f0)
    if ok.sum() < 8:
        return None
    med = np.nanmedian(f0)
    ok &= np.abs(np.log2(f0 / med)) < 0.585
    if ok.sum() < 8:
        return None
    k = int(np.argmax(lev))                       # the closest point of approach
    tpk = t[k]
    # the plateaux: the voiced frames well before and well after the pass
    pre = ok & (t < tpk - 0.10)
    post = ok & (t > tpk + 0.10)
    if pre.sum() < 3 or post.sum() < 3:
        return None
    fa = float(np.median(f0[pre])); fr = float(np.median(f0[post]))
    ratio = fa / max(fr, 1e-9)
    v = C_AIR * (ratio - 1.0) / (ratio + 1.0)
    # how much the level rises over the quietest tenth of the pass
    floor = float(np.percentile(lev, 10))
    return dict(dur=dur, tpk=tpk, f_app=fa, f_rec=fr, ratio=ratio, speed=v,
                peak=float(lev[k]), rise=float(lev[k]) - floor,
                voiced=ok.mean(), f0=med,
                # -6 dB width of the level bump, in seconds
                width=float(np.sum(lev > lev[k] - 6.0)) * (t[1] - t[0]))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    files = args if args else sorted(glob.glob(os.path.join(cached_dir(), "*.wav")))
    print("%-52s %-9s %6s %6s %7s %7s %6s %6s %6s %6s" %
          ("file", "class", "dur", "t_pk", "f_app", "f_rec", "ratio", "m/s", "rise", "w_-6dB"),
          flush=True)
    rows = []
    for p in files:
        name = os.path.basename(p)
        r = analyse(p)
        if r is None:
            continue
        cls = classify(name)
        print("%-52s %-9s %6.1f %6.2f %7.1f %7.1f %6.3f %6.1f %6.1f %6.2f" %
              (name[:52], cls, r["dur"], r["tpk"], r["f_app"], r["f_rec"],
               r["ratio"], r["speed"], r["rise"], r["width"]), flush=True)
        rows.append((cls, r, name))

    # a clean pass: short, one clear level peak, and the pitch genuinely falls
    clean = [r for r in rows if r[1]["dur"] < 12.0 and r[1]["rise"] > 8.0
             and 1.0 < r[1]["ratio"] < 1.25]
    print("\n%d of %d files fit a single clean pass" % (len(clean), len(rows)))
    if clean:
        sp = np.array([r[1]["speed"] for r in clean])
        ri = np.array([r[1]["rise"] for r in clean])
        wd = np.array([r[1]["width"] for r in clean])
        print("  speed   median %.1f m/s   range %.1f-%.1f" % (np.median(sp), sp.min(), sp.max()))
        print("  rise    median %.1f dB    range %.1f-%.1f" % (np.median(ri), ri.min(), ri.max()))
        print("  -6 dB   median %.2f s     range %.2f-%.2f" % (np.median(wd), wd.min(), wd.max()))
        for cls, r, name in clean:
            print("    %-9s %5.1f m/s  %5.1f dB  %.2f s  %s" %
                  (cls, r["speed"], r["rise"], r["width"], name[:44]))


if __name__ == "__main__":
    main()
