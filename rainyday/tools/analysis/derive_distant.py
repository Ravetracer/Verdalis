"""Rebuild Distant Rain Wall from a freshly fitted Downpour.

Distant Rain Wall has no reference recording to fit against (see pairs.py), so
the fit never writes it. It is the fitted Downpour heard from far away: the same
rain, with Distance and Air Absorption pushed up, the transients filtered off and
the far-field bed brought forward. Run this over a fit output directory before
the loudness pass, so the derived preset gets its Output Gain matched with the
rest of the library.

    python3 tools/analysis/derive_distant.py /tmp/tuned
"""
import os
import sys
import fitlib

# What "the same rain, further away" means, on top of the fitted Downpour.
OVERRIDES = {
    'density': 3000,          # a wall, not individual impacts
    'surface': 'Concrete',
    'slosh': 0.15,           # no individual slap survives the trip
    'bed_level': -6,          # far field is mostly bed
    'bed_tone': 0.75,
    'bed_drift': 0.5,
    'distance': 0.95,
    'air': 0.75,              # air absorption is what makes distance audible
    'space_amount': 0.4,
    'space_size': 0.8,
    'space_damping': 0.6,
    'filter_cutoff': 6000,    # no transients survive the trip
    'attack': 2000,
    'release': 4000,
}

META = {
    'name': 'Distant Rain Wall',
    'author': 'RainyDay',
    'description': 'A heavy downpour heard from far away. All transients lost to air absorption.',
    'features': 'ambient, texture, dark, distant, pad',
}


def derive(directory):
    src = os.path.join(directory, 'downpour.rainyday')
    if not os.path.exists(src):
        raise SystemExit(f'no fitted Downpour in {directory}')
    params, _ = fitlib.read_preset(src)
    params.update(OVERRIDES)
    dst = os.path.join(directory, 'distant_rain_wall.rainyday')
    with open(dst, 'w') as fh:
        fh.write(fitlib.preset_text(params, META))
    print(f'wrote {dst}')


if __name__ == '__main__':
    derive(sys.argv[1] if len(sys.argv) > 1 else 'presets')
