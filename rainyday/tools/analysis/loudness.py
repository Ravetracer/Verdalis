"""Re-matches the library's Output Gain after a change to the engine.

Each preset is rendered at three fixed seeds; the gain is moved so the mean RMS
lands on target, then pulled back if that would push the peak too high. Sparse
presets are peak-limited by nature and simply keep whatever RMS falls out.

The correction is applied more than once, because a single round does not
converge when the engine's soft clipper is already engaged. Above its 0.8 knee
the output is compressed, so cutting the gain by n dB moves the measured peak by
much less than n and the pass stops with the preset still over target -- which
is how Concrete Alley came to ship 5.7 dB into the clipper after the slosh layer
was added. Each round measures again, so once the peaks are back under the knee
the remaining correction is linear and it settles.
"""
import os
import sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fitlib

TARGET_RMS_DB = -22.0
MAX_PEAK_DB = -4.0
# Enough rounds for the soft clipper to be backed out of; it settles in two or
# three for everything in the library.
MAX_ROUNDS = 6
TOLERANCE_DB = 0.2
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
        new_gain = gain
        peak2, rms2 = peak, rms
        for _ in range(MAX_ROUNDS):
            delta = TARGET_RMS_DB - rms2
            if peak2 + delta > MAX_PEAK_DB:
                delta = MAX_PEAK_DB - peak2
            stepped = float(np.clip(new_gain + delta, -60.0, 12.0))
            if abs(stepped - new_gain) < TOLERANCE_DB:
                break
            new_gain = stepped
            params['gain'] = f'{new_gain:.6g}'
            peak2, rms2 = measure(r, params, meta)
        params['gain'] = f'{new_gain:.6g}'
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
