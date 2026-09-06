#!/usr/bin/env python3
"""Match the RainyDay preset library against reference recordings.

    export RAINYDAY_SOUNDS=/path/to/reference/wavs
    python3 tools/analysis/match.py report              # how far off is the library
    python3 tools/analysis/match.py fit --out /tmp/new  # tune preset values
    python3 tools/analysis/match.py loudness --in /tmp/new --out presets

`report` measures each preset's render against the recording it is meant to
evoke and prints the per-band error. `fit` runs a coordinate descent over the
parameters listed in fit.py and writes tuned presets. `loudness` re-matches
Output Gain across the library, which any change to the engine invalidates.

Needs the plugin and rainyday-fithost built (./install.sh does both) and numpy.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import numpy as np
import feat
import fit
import fitlib
import loudness
import refs
from pairs import PAIRS, BAND_WEIGHT, FULL, DROP

PRESETS = os.path.join(fitlib.ROOT, 'presets')


def names_for(mode):
    if mode == DROP:
        return fit.DROP_PARAMS
    return fit.FULL_PARAMS if mode == FULL else fit.TONE_PARAMS


def selected(args):
    only = getattr(args, 'only', None)
    return [p for p in PAIRS if not only or p[0] in only]


def cmd_report(args):
    r = fitlib.Renderer()
    print(f"{'preset':20s} {'reference':28s} {'dist':>8s}   "
          + ' '.join(f'{n:>6s}' for n in feat.BAND_NAMES))
    total = 0.0
    pairs = selected(args)
    for preset, ref, _ in pairs:
        params, meta = fitlib.read_preset(os.path.join(args.presets, preset + '.rainyday'))
        target = refs.reference(ref)
        ds, bands = [], []
        seconds = max(7.0, fit.seconds_for(target))
        for seed in args.seeds:
            p = dict(params)
            p['seed'] = str(seed)
            f = fitlib.analyse_render(r.render(p, meta, seconds))
            ds.append(fitlib.distance(f, target, BAND_WEIGHT))
            bands.append(f['bands'])
        err = np.mean(bands, axis=0) - target['bands']
        total += float(np.mean(ds))
        print(f'{preset:20s} {ref:28s} {np.mean(ds):8.1f}   '
              + ' '.join(f'{v:+6.1f}' for v in err))
    print(f"\n{'mean':20s} {'':28s} {total / len(pairs):8.1f}")
    r.close()


def cmd_fit(args):
    os.makedirs(args.out, exist_ok=True)
    pool = fit.RendererPool(args.jobs)
    f = fit.Fitter(pool)
    print(f'fitting with {pool.jobs} render processes', flush=True)
    t0 = time.time()
    rows = []
    for preset, ref, mode in selected(args):
        rows.append(fit.fit_preset(f, preset, ref, names_for(mode), args.out))
        print(f'  [{time.time() - t0:6.0f}s] {preset}\n', flush=True)
    pool.close()

    # The output directory is a complete library either way: a refused preset is
    # written back unchanged. This summary says which ones actually moved.
    kept = [r for r in rows if r['accepted']]
    print(f'\n{"preset":20s} {"held-out":>18s} {"gain vs noise":>18s}   verdict')
    for r in rows:
        print(f'{r["preset"]:20s} {r["held_before"]:8.1f} -> {r["held_after"]:6.1f} '
              f'{r["improvement"]:+10.1f} vs {r["se"]:5.1f}   '
              f'{"kept" if r["accepted"] else "refused"}')
    print(f'\n{len(kept)} of {len(rows)} presets improved on unseen seeds')


def cmd_loudness(args):
    names = sorted(n[:-9] for n in os.listdir(args.src) if n.endswith('.rainyday'))
    loudness.match(args.src, args.out, names)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)

    p = sub.add_parser('report')
    p.add_argument('--presets', default=PRESETS)
    p.add_argument('--seeds', type=int, nargs='+', default=[21, 22, 23])
    p.add_argument('--only', nargs='+', help='preset names to report; default all')
    p.set_defaults(func=cmd_report)

    p = sub.add_parser('fit')
    p.add_argument('--out', required=True)
    p.add_argument('--only', nargs='+', help='preset names to fit; default all')
    p.add_argument('--jobs', type=int, default=fit.DEFAULT_JOBS,
                   help='render processes to run at once (default %(default)s)')
    p.set_defaults(func=cmd_fit)

    p = sub.add_parser('loudness')
    p.add_argument('--src', dest='src', default=PRESETS)
    p.add_argument('--out', default=PRESETS)
    p.set_defaults(func=cmd_loudness)

    args = ap.parse_args()
    args.func(args)


if __name__ == '__main__':
    main()
