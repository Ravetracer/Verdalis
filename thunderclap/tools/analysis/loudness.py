#!/usr/bin/env python3
"""Sets each factory preset's Output Gain from its rendered peak level.

    python3 tools/analysis/loudness.py --plugin build/ThunderClap.clap --presets presets
            [--render build/thunderclap-render] [--seeds 3] [--target -4]
            [--per-km 0.35] [--floor -14] [--dry-run]

Every preset is rendered once per seed with the render host, the loudest peak
over the seeds is taken, and the preset's `gain` line is rewritten so that peak
lands at the target. Thunder is normalised inside the engine so that a far
strike is not inaudible, but a far strike still ought to be quieter than a near
one, so the target is lowered by `per-km` decibels per kilometre of the
preset's Distance down to `floor`.

Storm-mode presets are rendered as a single flash (the note is released after a
second), which is the loudest thing they do anyway.
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile


def read_preset(path):
    values = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            key, _, value = line.partition('=')
            values[key.strip()] = value.strip()
    return values


def render_peak(render, plugin, preset_key, seed, tail, gain_db=0.0):
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
        out = tmp.name
    try:
        cmd = [render, '--plugin', plugin, '--preset', preset_key, '--out', out, '--seconds', '1',
               '--tail', str(tail), '--param', 'Random Seed=%d' % seed,
               '--param', 'Output Gain=%g' % gain_db]
        text = subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    finally:
        try:
            os.remove(out)
        except OSError:
            pass
    m = re.search(r'peak\s+([0-9.]+)\s+\(\s*([-+0-9.]+) dBFS\)', text)
    if not m:
        raise RuntimeError('could not read the peak from:\n' + text)
    return float(m.group(2))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--plugin', required=True)
    ap.add_argument('--presets', required=True)
    ap.add_argument('--render', default=None)
    ap.add_argument('--seeds', type=int, default=3)
    ap.add_argument('--tail', type=float, default=12.0)
    ap.add_argument('--target', type=float, default=-4.0)
    ap.add_argument('--per-km', type=float, default=0.35)
    ap.add_argument('--floor', type=float, default=-14.0)
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args()

    render = args.render or os.path.join(os.path.dirname(os.path.abspath(args.plugin)),
                                         'thunderclap-render')
    files = sorted(f for f in os.listdir(args.presets) if f.endswith('.thunderclap'))
    print('%-22s %8s %8s %8s %8s' % ('preset', 'dist km', 'peak dB', 'target', 'gain'))
    for name in files:
        path = os.path.join(args.presets, name)
        key = name[:-len('.thunderclap')]
        values = read_preset(path)
        distance = float(values.get('distance', '2'))
        target = max(args.floor, args.target - args.per_km * distance)
        # The compressor and the clipper make the peak a non-linear function of
        # the gain, so approach the target in a few steps rather than one.
        gain = 0.0
        peak = 0.0
        for _ in range(4):
            peak = max(render_peak(render, args.plugin, key, seed, args.tail, gain)
                       for seed in range(1, args.seeds + 1))
            if abs(peak - target) < 0.4:
                break
            gain = max(-60.0, min(12.0, round(gain + (target - peak), 1)))
        print('%-22s %8.1f %8.1f %8.1f %8.1f' % (key, distance, peak, target, gain))
        if args.dry_run:
            continue
        with open(path) as f:
            text = f.read()
        new_text, n = re.subn(r'^gain = .*$', 'gain = %g' % gain, text, flags=re.M)
        if n == 0:
            new_text = text.rstrip('\n') + '\ngain = %g\n' % gain
        with open(path, 'w') as f:
            f.write(new_text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
