"""Re-matches the library's Output Gain after a change to the engine.

Each preset is rendered at three fixed seeds; the gain is moved so the mean RMS
lands on target, then pulled back if that would push the peak too high. Sparse
presets are peak-limited by nature and simply keep whatever RMS falls out.
"""
import os
import sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fitlib

TARGET_RMS_DB = -22.0
MAX_PEAK_DB = -4.0
SEEDS = [11, 12, 13]
SECONDS = 8.0


def measure(r, params, meta):
    peaks, rmss = [], []
    for s in SEEDS:
        p = dict(params)
        p['seed'] = str(s)
        x = r.render(p, meta, SECONDS)
        x = x[int(1.0 * fitlib.SR):]
        peaks.append(float(np.abs(x).max()))
        rmss.append(float(np.sqrt((x ** 2).mean())))
    return 20 * np.log10(max(peaks) + 1e-12), 20 * np.log10(np.mean(rmss) + 1e-12)


def match(src_dir, dst_dir, names):
    os.makedirs(dst_dir, exist_ok=True)
    r = fitlib.Renderer()
    rows = []
    for name in names:
        params, meta = fitlib.read_preset(os.path.join(src_dir, name + '.rainyday'))
        gain = float(params.get('gain', 0.0))
        params['gain'] = f'{gain:.6g}'
        peak, rms = measure(r, params, meta)
        delta = TARGET_RMS_DB - rms
        if peak + delta > MAX_PEAK_DB:
            delta = MAX_PEAK_DB - peak
        new_gain = float(np.clip(gain + delta, -60.0, 12.0))
        params['gain'] = f'{new_gain:.6g}'
        peak2, rms2 = measure(r, params, meta)
        with open(os.path.join(dst_dir, name + '.rainyday'), 'w') as f:
            f.write(fitlib.preset_text(params, meta))
        rows.append((name, gain, new_gain, peak2, rms2))
        print(f'  {name:22s} gain {gain:+6.2f} -> {new_gain:+6.2f} dB   '
              f'peak {peak2:6.1f}  rms {rms2:6.1f}', flush=True)
    r.close()
    return rows


if __name__ == '__main__':
    root = fitlib.ROOT
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, 'presets')
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, 'presets')
    names = sorted(n[:-9] for n in os.listdir(src) if n.endswith('.rainyday'))
    match(src, dst, names)
