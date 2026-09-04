"""Coordinate-descent fit of RainyDay presets against reference recordings.

Deterministic: every candidate renders with the same fixed seed, so two
candidates differ only by their parameters and not by which droplets happened
to fall. The winner is re-checked on unseen seeds at the end.
"""
import atexit
import concurrent.futures as cf
import os
import sys
import time
import numpy as np
import fitlib
import feat
import refs
from pairs import BAND_WEIGHT

# name -> (kind, low, high). 'mul' searches multiplicatively, 'add' linearly.
SPACE = {
    'density':       ('mul', 0.2, 5000.0),
    'drop_pitch':    ('mul', 60.0, 9000.0),
    'drop_decay':    ('mul', 1.0, 1200.0),
    'filter_cutoff': ('mul', 200.0, 20000.0),
    'pitch_spread':  ('add', 0.0, 5.0),
    'clumping':      ('add', 0.0, 1.0),
    'decay_spread':  ('add', 0.0, 1.0),
    'tonality':      ('add', 0.0, 1.0),
    'bubble':        ('add', 0.0, 1.0),
    'impact':        ('add', 0.0, 1.0),
    'splash':        ('add', 0.0, 1.0),
    'level_spread':  ('add', 0.0, 1.0),
    'bed_tone':      ('add', 0.0, 1.0),
    'bed_body':      ('add', 0.0, 1.0),
    'bed_drift':     ('add', 0.0, 1.0),
    'distance':      ('add', 0.0, 1.0),
    'air':           ('add', 0.0, 1.0),
    'bed_level':     ('add', -60.0, 6.0),
    'space_amount':  ('add', 0.0, 1.0),
    'space_size':    ('add', 0.0, 1.0),
    'space_damping': ('add', 0.0, 1.0),
    'filter_reso':   ('add', 0.0, 1.0),
}

# How many droplet realisations a candidate is scored over, per preset.
#
# Two was not close to enough. Measured over forty fresh seeds, Dripping Faucet's
# distance to its reference runs from 45 to 1286 and Steady Rain's from 11 to 51.
# Averaging two draws from that and calling the winner an improvement is how the
# fit came to produce a Dripping Faucet scoring 28.7 on the seeds it optimised
# against and 732.1 on seeds it had not seen.
#
# The first version of this table gave two seeds to the expensive presets on the
# reasoning that they were the stable ones. Storm Front disproved it: it costs
# 1.0 s a candidate, got two seeds, and was the worst overfitter left in the run,
# going from 44.4 on held-out seeds to 197.7. Expensive and unstable are not
# opposites.
#
# Since candidates and seeds now run across eight workers the economics are
# different anyway -- twelve seeds of a cheap preset finish in the time two used
# to take -- so everything gets twelve, and only Downpour, at 5.7 s a candidate,
# is held lower.
#
# This is a table rather than something timed at startup on purpose. Deriving it
# from a measurement made the fit depend on how fast the machine happened to be
# and on how many workers were running, so the same preset fitted to different
# values with one worker and with eight. A fit has to be reproducible.
# Regenerate it if the engine's cost changes materially; the numbers below were
# measured on 2026-09-04.
PRESET_SEEDS = {
    'downpour': 6,
}
DEFAULT_SEEDS = 12
MAX_SEEDS = 12
# Disjoint from VERIFY_SEEDS, so the held-out check stays honest however many of
# these are used.
SEED_POOL = [1, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41]
FIT_SEEDS = SEED_POOL[:2]
VERIFY_SEEDS = [2, 3, 4]
SECONDS = 6.0


def seeds_for(preset):
    return SEED_POOL[:min(MAX_SEEDS, PRESET_SEEDS.get(preset, DEFAULT_SEEDS))]


# Per-preset overrides of SPACE, for the cases where the objective is happy with
# something that does not sound like rain.
#
# Tin Roof is the one that needed this. The Metal surface triples the ring time,
# raises the resonator Q, boosts the tonal layer by 30 % and cuts the wet splash
# to 45 %. Give that a high centre pitch and a Bubble Chance of 1 and every one
# of six hundred drops a second becomes a bright, tuned, 30 ms bell with the
# water taken out of it -- which measures perfectly respectably and sounds like
# breaking icicles. The objective has no feature that can tell those apart, so
# the bounds say it instead: a water drop on a panel is not a 6 kHz bell, and
# most impacts do not entrain a ringing bubble at all.
PRESET_BOUNDS = {
    'tin_roof': {
        'drop_pitch': ('mul', 60.0, 3000.0),
        'bubble':     ('add', 0.0, 0.5),
        'splash':     ('add', 0.5, 1.0),
    },
}

# The full set, and the subset used for presets that only need their tone
# corrected without losing the density and rhythm that define them. Surface,
# Chirp, the space controls and the envelope are never fitted: those are the
# preset's identity, not something to be solved for.
FULL_PARAMS = ['density', 'clumping', 'drop_pitch', 'pitch_spread', 'drop_decay',
        'decay_spread', 'tonality', 'bubble', 'impact', 'splash', 'level_spread',
        'bed_level', 'bed_tone', 'bed_body', 'bed_drift', 'distance', 'air']
# Sparse drip presets live or die on the single droplet and the room around it,
# so those get the space and filter controls as well.
DROP_PARAMS = ['density', 'clumping', 'drop_pitch', 'pitch_spread', 'drop_decay',
               'decay_spread', 'tonality', 'bubble', 'impact', 'splash',
               'level_spread', 'bed_level', 'bed_tone', 'bed_body', 'distance',
               'air', 'space_amount', 'space_size', 'space_damping', 'filter_cutoff']
TONE_PARAMS = ['drop_pitch', 'pitch_spread', 'drop_decay', 'tonality', 'bubble',
        'impact', 'splash', 'bed_level', 'bed_tone', 'bed_body', 'distance', 'air']


def candidates(name, value, scale, bounds=None):
    kind, lo, hi = (bounds or {}).get(name, SPACE[name])
    if kind == 'mul':
        f = 1.0 + 1.4 * scale
        vals = [value / f, value / np.sqrt(f), value, value * np.sqrt(f), value * f]
    else:
        step = (hi - lo) * 0.28 * scale
        vals = [value - step, value - step / 2, value, value + step / 2, value + step]
    return sorted({float(np.clip(v, lo, hi)) for v in vals})


DEFAULT_JOBS = 8

# Rendering is only part of the work. Measured per candidate: Downpour spends
# 4.74 s rendering and 0.13 s being analysed, but Cave Drips spends 0.03 s and
# 0.14 s -- so for the sparse presets four fifths of the time is numpy in the
# parent process, under the GIL. A thread pool therefore made the cheap presets
# slower, not faster. Workers are processes so that the analysis parallelises
# with the rendering.
#
# Each worker owns one fithost. Only the finished distance crosses back, and the
# reference features are passed in, so no worker ever loads a recording: the
# cave reference alone is 181 seconds and 1.6 GB once analysed.

_worker_renderer = None


def _init_worker():
    global _worker_renderer
    _worker_renderer = fitlib.Renderer()
    atexit.register(_close_worker)


def _close_worker():
    global _worker_renderer
    if _worker_renderer is not None:
        try:
            _worker_renderer.close()
        except Exception:
            pass
        _worker_renderer = None


def _score_job(params, meta, target, want_timing=False):
    t0 = time.time()
    x = _worker_renderer.render(params, meta, SECONDS)
    rendered = time.time() - t0
    d = fitlib.distance(fitlib.analyse_render(x), target, BAND_WEIGHT)
    return (d, rendered, time.time() - t0) if want_timing else d


class RendererPool:
    """A pool of worker processes, each with its own fithost.

    Safe to spread work across processes only because a render is fully
    determined by its parameters and seed. The droplet allocation cursor used to
    survive a reset, which made a render depend on whatever the same process
    rendered before it; results would then have depended on how the work
    happened to be shared out.
    """

    def __init__(self, jobs=DEFAULT_JOBS):
        self.jobs = max(1, jobs)
        self.ex = cf.ProcessPoolExecutor(max_workers=self.jobs,
                                         initializer=_init_worker)

    def submit(self, *a, **kw):
        return self.ex.submit(_score_job, *a, **kw)

    def close(self):
        self.ex.shutdown(wait=True)


class Fitter:
    def __init__(self, pool):
        self.pool = pool
        self.cache = {}
        self.seeds = list(FIT_SEEDS)

    @staticmethod
    def _key(params, seed):
        p = dict(params)
        p['seed'] = str(seed)
        return tuple(sorted((k, str(v)) for k, v in p.items())), p

    def score(self, params, meta, target, seed=None):
        return self.score_many([params], meta, target, seed)[0]

    def score_many(self, param_list, meta, target, seed=None):
        """Score candidates together, one job per candidate and seed.

        Splitting by seed as well as by candidate matters: Downpour is scored
        over two seeds, so candidates alone would leave most of the pool idle on
        exactly the preset that takes the longest.
        """
        if not param_list:
            return []
        seeds = [seed] if seed is not None else self.seeds
        totals = [0.0] * len(param_list)
        pending = {}
        for i, params in enumerate(param_list):
            for s in seeds:
                key, p = self._key(params, s)
                hit = self.cache.get(key)
                if hit is not None:
                    totals[i] += hit
                else:
                    pending[self.pool.submit(p, meta, target)] = (i, key)
        for fut in cf.as_completed(pending):
            i, key = pending[fut]
            d = fut.result()
            self.cache[key] = d
            totals[i] += d
        return [t / len(seeds) for t in totals]

    def run(self, params, meta, target, names, passes=4, log=None, bounds=None):
        cur = dict(params)
        # A bound that the preset already violates has to be honoured before the
        # search starts, or the first pass simply keeps the offending value.
        for name, (_, lo, hi) in (bounds or {}).items():
            if name in cur:
                cur[name] = float(np.clip(float(cur[name]), lo, hi))
        best = self.score(cur, meta, target)
        for p in range(passes):
            scale = 1.0 * (0.55 ** p)
            improved = False
            for name in names:
                if name not in cur:
                    continue
                try:
                    base = float(cur[name])
                except (TypeError, ValueError):
                    continue
                # All of one parameter's candidates are independent, so they
                # are rendered together and then applied in the original order.
                # Only `best` carries between them, and it only ever falls, so
                # the outcome is the same as testing them one at a time.
                vals = [v for v in candidates(name, base, scale, bounds)
                        if abs(v - base) >= 1e-9]
                trials = []
                for v in vals:
                    trial = dict(cur)
                    trial[name] = v
                    trials.append(trial)
                for v, d in zip(vals, self.score_many(trials, meta, target)):
                    if d < best - 1e-6:
                        best, cur[name], improved = d, v, True
            if log:
                log(f'    pass {p + 1}: distance {best:.1f}')
            if not improved:
                break
        return cur, best


def fit_preset(fitter, preset, reference, names, out_dir):
    root = fitlib.ROOT
    params, meta = fitlib.read_preset(os.path.join(root, 'presets', preset + '.rainyday'))
    target = refs.reference(reference)

    def log(m):
        print(m, flush=True)

    bounds = PRESET_BOUNDS.get(preset)
    fitter.seeds = seeds_for(preset)
    before = fitter.score(params, meta, target)
    print(f'  {preset} <- {reference}   start {before:.1f}   {len(fitter.seeds)} seeds'
          f'{"   (bounded)" if bounds else ""}', flush=True)
    tuned, after = fitter.run(params, meta, target, names, log=log, bounds=bounds)

    checks = [fitter.score(tuned, meta, target, seed=s) for s in VERIFY_SEEDS]
    base_checks = [fitter.score(params, meta, target, seed=s) for s in VERIFY_SEEDS]
    print(f'  {preset}: {before:.1f} -> {after:.1f}   unseen seeds '
          f'{np.mean(base_checks):.1f} -> {np.mean(checks):.1f}', flush=True)

    with open(os.path.join(out_dir, preset + '.rainyday'), 'w') as f:
        f.write(fitlib.preset_text(tuned, meta))
    return before, after, float(np.mean(base_checks)), float(np.mean(checks))
