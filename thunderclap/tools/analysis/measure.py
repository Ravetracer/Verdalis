#!/usr/bin/env python3
"""Measures thunder recordings and ThunderClap renders with the same code.

    python3 tools/analysis/measure.py bands  <wav or directory>...
    python3 tools/analysis/measure.py time   <wav> [step seconds]
    python3 tools/analysis/measure.py onset  <wav>
    python3 tools/analysis/measure.py clap   <wav or directory>...
    python3 tools/analysis/measure.py impact <wav or directory>...

`bands` prints, per file, the peak and RMS level, where the envelope peaks and
how long it stays within 20 and 30 dB of that, and the energy in ten octave
bands relative to the loudest band. `time` prints the level and six band
levels per step, which is where the clap and rumble structure shows. `onset`
prints the 5 ms envelope of the first second and a half, for the attack.

`clap` and `impact` are for the strike rather than the whole thunder, and are
what the hardness of the crack was set against. `clap` gives the ten octave
bands of the 400 ms from the onset alone, so the rumble and the tail do not
average the strike away -- which is what `bands` does to it. `impact` gives the
figures that separate a hard strike from a soft one: the crest factor over the
50 ms around the peak and over the whole file, the rise from -20 and -40 dB,
the steepest climb in dB per millisecond, how far the peak sample stands above
its own 2 ms, how dense the first 200 and 500 ms are (the share of 5 ms frames
within 6 dB of the peak), and the RMS of the 200 ms after the onset against the
peak sample. A hard clap is dense and low-crest; a crackle is sparse and
high-crest.

Only numpy is needed. Any sample rate, 16/24/32-bit PCM or 32-bit float.
"""
import glob
import os
import sys

import numpy as np

# wavio is shared with the rest of the suite; see shared/tools/analysis.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', '..', 'shared', 'tools', 'analysis'))
from wavio import read_wav, to_mono  # noqa: E402

EDGES = [20, 40, 80, 160, 320, 640, 1280, 2560, 5120, 10240, 20000]
TIME_EDGES = [20, 60, 150, 400, 1000, 3000, 10000]


def band_energy_db(x, sr, edges, frame=8192):
    win = np.hanning(frame)
    acc = np.zeros(frame // 2 + 1)
    count = 0
    for start in range(0, len(x) - frame, frame // 2):
        spectrum = np.fft.rfft(x[start:start + frame] * win)
        acc += np.abs(spectrum) ** 2
        count += 1
    acc /= max(count, 1)
    freqs = np.fft.rfftfreq(frame, 1 / sr)
    out = []
    for lo, hi in zip(edges[:-1], edges[1:]):
        mask = (freqs >= lo) & (freqs < hi)
        out.append(10 * np.log10(acc[mask].mean() + 1e-20))
    return np.array(out)


def files_in(args):
    for a in args:
        if os.path.isdir(a):
            yield from sorted(glob.glob(os.path.join(a, '*.wav')))
        else:
            yield a


def cmd_bands(args):
    print("%-36s %7s %7s %7s %7s %7s | %s" % ("file", "peak", "rms", "t_peak", ">-20dB", ">-30dB",
                                              " ".join("%5d" % lo for lo in EDGES[:-1])))
    for path in files_in(args):
        x, sr = read_wav(path)
        x = to_mono(x)
        peak = np.abs(x).max()
        rms = np.sqrt((x ** 2).mean())
        w = int(sr * 0.05)
        frames = len(x) // w
        env = np.sqrt((x[:frames * w].reshape(frames, w) ** 2).mean(axis=1))
        env_db = 20 * np.log10(env + 1e-9)
        top = env_db.max()
        bands = band_energy_db(x, sr, EDGES)
        bands -= bands.max()
        print("%-36s %7.1f %7.1f %7.1f %7.1f %7.1f | %s" % (
            os.path.basename(path)[:36], 20 * np.log10(peak + 1e-9), 20 * np.log10(rms + 1e-9),
            env_db.argmax() * 0.05, (env_db > top - 20).sum() * 0.05,
            (env_db > top - 30).sum() * 0.05, " ".join("%5.0f" % v for v in bands)))


def cmd_time(args):
    path = args[0]
    step = float(args[1]) if len(args) > 1 else 0.5
    x, sr = read_wav(path)
    x = to_mono(x)
    n = int(sr * step)
    frames = len(x) // n
    win = np.hanning(n)
    freqs = np.fft.rfftfreq(n, 1 / sr)
    print(os.path.basename(path), "step=%gs  columns: t  rms_dB | band dB for" % step,
          TIME_EDGES[:-1])
    for k in range(frames):
        seg = x[k * n:(k + 1) * n]
        rms = 20 * np.log10(np.sqrt((seg ** 2).mean()) + 1e-9)
        spectrum = np.abs(np.fft.rfft(seg * win)) ** 2
        bands = []
        for lo, hi in zip(TIME_EDGES[:-1], TIME_EDGES[1:]):
            mask = (freqs >= lo) & (freqs < hi)
            bands.append(10 * np.log10(spectrum[mask].mean() + 1e-20))
        print("%6.2f %6.1f | %s" % (k * step, rms, " ".join("%5.0f" % v for v in bands)))


def cmd_onset(args):
    path = args[0]
    x, sr = read_wav(path)
    x = to_mono(x)
    n = int(sr * 0.005)
    frames = len(x) // n
    env = np.sqrt((x[:frames * n].reshape(frames, n) ** 2).mean(axis=1))
    db = 20 * np.log10(env + 1e-9)
    top = db.max()
    onset = int(np.argmax(db > top - 30))
    print(os.path.basename(path), "onset at %.3f s, peak at %.3f s" % (onset * 0.005,
                                                                     db.argmax() * 0.005))
    for k in range(onset, min(onset + 300, frames), 4):
        v = db[k] - top
        print("%5.3f %6.1f %s" % ((k - onset) * 0.005, v, '#' * int(max(0, (v + 40) * 1.2))))


def clap_window(x, sr, seconds=0.4):
    """The strike alone: from the onset of the loudest 5 ms frame, forward."""
    n = max(1, int(sr * 0.005))
    frames = len(x) // n
    env = np.sqrt((x[:frames * n].reshape(frames, n) ** 2).mean(axis=1))
    peak = int(np.argmax(env))
    j = peak
    while j > 0 and env[j] > env[peak] * 0.0316:   # back to -30 dB
        j -= 1
    return x[j * n:j * n + int(sr * seconds)], j * n / sr


def cmd_clap(args):
    print("%-28s %s" % ("file", "   20    40    80   160   320   640   1k3   2k6   5k1   10k"))
    for path in files_in(args):
        x, sr = read_wav(path)
        seg, _ = clap_window(to_mono(x), sr)
        if len(seg) < 2048:
            continue
        frame = int(2 ** np.floor(np.log2(len(seg))))
        bands = band_energy_db(seg, sr, EDGES, frame=frame)
        bands -= bands.max()
        print("%-28s %s" % (os.path.basename(path)[:28],
                            " ".join("%5.0f" % v for v in bands)))


def cmd_impact(args):
    print("%-28s %6s %6s %6s %6s %6s %6s %6s %6s %6s" %
          ("file", "crst50", "crstAll", "rise20", "rise40", "dB/ms", "spike",
           "dns200", "dns500", "body"))
    for path in files_in(args):
        x, sr = read_wav(path)
        x = to_mono(x)
        x = x - x.mean()
        peak = np.abs(x).max()
        if peak <= 0:
            continue
        ip = int(np.argmax(np.abs(x)))

        half = int(sr * 0.025)
        near = x[max(0, ip - half):ip + half]
        crest50 = 20 * np.log10(peak / max(np.sqrt(np.mean(near ** 2)), 1e-12))
        crestall = 20 * np.log10(peak / max(np.sqrt(np.mean(x ** 2)), 1e-12))

        # 1 ms peak envelope, for the rise times and the steepest climb
        n = max(1, int(sr * 0.001))
        frames = len(x) // n
        env = np.abs(x[:frames * n].reshape(frames, n)).max(axis=1)
        ie = int(np.argmax(env))

        def back_to(fraction):
            j = ie
            while j > 0 and env[j] > env[ie] * fraction:
                j -= 1
            return (ie - j) * 1000.0 / (sr / n)

        rise20, rise40 = back_to(0.1), back_to(0.01)
        climb = np.maximum(env[max(0, ie - 200):ie + 1], peak * 1e-4)
        slope = float(np.percentile(np.diff(20 * np.log10(climb)), 99)) if len(climb) > 2 else 0.0

        w = int(sr * 0.002)
        local = x[max(0, ip - w):ip + w]
        spike = 20 * np.log10(peak / max(np.sqrt(np.mean(local ** 2)), 1e-12))

        seg, onset = clap_window(x, sr, 0.5)
        fn = max(1, int(sr * 0.005))
        m = len(seg) // fn
        dens200 = dens500 = 0.0
        if m > 8:
            fe = np.sqrt((seg[:m * fn].reshape(m, fn) ** 2).mean(axis=1))
            top = fe.max()
            dens200 = float(np.mean(fe[:m // 5 + 1] > top * 0.5) * 100)
            dens500 = float(np.mean(fe > top * 0.5) * 100)
        body = 20 * np.log10(max(np.sqrt(np.mean(seg[:int(sr * 0.2)] ** 2)), 1e-12) / peak)

        print("%-28s %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %5.0f%% %5.0f%% %6.1f" %
              (os.path.basename(path)[:28], crest50, crestall, rise20, rise40, slope,
               spike, dens200, dens500, body))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    cmd, args = sys.argv[1], sys.argv[2:]
    if cmd == 'bands':
        cmd_bands(args)
    elif cmd == 'time':
        cmd_time(args)
    elif cmd == 'onset':
        cmd_onset(args)
    elif cmd == 'clap':
        cmd_clap(args)
    elif cmd == 'impact':
        cmd_impact(args)
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
