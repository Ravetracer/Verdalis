#!/usr/bin/env python3
"""Measures thunder recordings and ThunderClap renders with the same code.

    python3 tools/analysis/measure.py bands  <wav or directory>...
    python3 tools/analysis/measure.py time   <wav> [step seconds]
    python3 tools/analysis/measure.py onset  <wav>

`bands` prints, per file, the peak and RMS level, where the envelope peaks and
how long it stays within 20 and 30 dB of that, and the energy in ten octave
bands relative to the loudest band. `time` prints the level and six band
levels per step, which is where the clap and rumble structure shows. `onset`
prints the 5 ms envelope of the first second and a half, for the attack.

Only numpy is needed. Any sample rate, 16/24/32-bit PCM or 32-bit float.
"""
import glob
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
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
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
