"""Loads the reference recordings and caches their features."""
import os
import numpy as np
import feat
from wavio import read_wav, to_mono

SOUND_DIR = os.environ.get('RAINYDAY_SOUNDS', '')

# Recordings arrive in batches and live in their own directories; a reference is
# named by its bare filename and looked up across all of them.
def _search_dirs():
    if not SOUND_DIR:
        return []
    parent = os.path.dirname(os.path.abspath(SOUND_DIR))
    return [SOUND_DIR] + [os.path.join(parent, d) for d in ('newSounds', '99sounds')]


def _find(name):
    for d in _search_dirs():
        path = os.path.join(d, name + '.wav')
        if os.path.exists(path):
            return path
    raise FileNotFoundError(f'no recording named {name}.wav under {_search_dirs()}')


# A composite is a class of recordings averaged into one target, so a preset is
# fitted to what that kind of rain measures like rather than to one microphone in
# one room on one afternoon. Several presets were matched to the nearest
# neighbour in the old set for want of anything better; these are the real thing.
#
# Only recordings that pass the low-frequency transient screen are listed. Every
# rain-traffic file is left out on Christian's word that they contain traffic:
# the screen looks for impulsive events and so catches thunder, but traffic is a
# steady rumble and passes it. Rain-sound-4 is out for the opposite reason, being
# rain with thunder over it.
COMPOSITE_CAP_SEC = 60

COMPOSITES = {
    # Wet asphalt and stone, out in the open.
    'street': ['rain-street-01', 'rain-street-02', 'rain-street-03', 'rain-street-04',
               'rain-street-06', 'rain-street-07', 'rain-street-08', 'rain-street-09',
               'rain-street-11'],
    # Close-up against glass.
    'window': ['rain-window-01', 'rain-window-02', 'rain-window-03', 'rain-window-04',
               'rain-window-05', 'rain-window-06', 'rain-window-07', 'rain-window-08',
               'rain-window-09', 'rain-window-10', 'rain-window-11'],
    # Water running away down a drain.
    'sewer': ['rain-sewer-01', 'rain-sewer-02', 'rain-sewer-03'],
    # Taut fabric a foot above your head. No preset uses this yet.
    'umbrella': ['rain-umbrella-01', 'rain-umbrella-02', 'rain-umbrella-04'],
    # General open rain, from the second batch. Six recordings that measure alike.
    'open_rain': ['Rain-sound-1', 'Rain-sound-5', 'Rain-sound-7', 'Rain-sound-10',
                  'Rain-sound-11', 'Rain-sound-12'],
}


def rumble_filter(x, sr, fc=70.0):
    """Removes what is below ~70 Hz: wind and handling noise RainyDay cannot make."""
    k = max(1, int(sr / (2 * np.pi * fc)))
    ker = np.ones(k) / k
    lo = np.convolve(x, ker, mode='same')
    lo = np.convolve(lo, ker, mode='same')
    return x - lo


_cache = {}


def _measure(name, cap_sec=None):
    x, sr = read_wav(_find(name))
    m = to_mono(x)
    if cap_sec:
        m = m[:int(cap_sec * sr)]
    m = m - m.mean()
    return feat.extract(rumble_filter(m, sr), sr, name, denoise=True)


def _average(parts, label):
    """Mean of several feature sets: the prototype of a class of recordings."""
    out = dict(parts[0])
    out['label'] = label
    for k, v in parts[0].items():
        if k in ('label', 'sr'):
            continue
        vals = [p[k] for p in parts]
        if isinstance(v, np.ndarray):
            out[k] = np.mean(np.stack(vals), axis=0)
        else:
            finite = [float(x) for x in vals
                      if x is not None and np.isfinite(np.float64(x))]
            if finite:
                out[k] = float(np.mean(finite))
    return out


def reference(name):
    if name in _cache:
        return _cache[name]
    if not SOUND_DIR:
        raise RuntimeError('set RAINYDAY_SOUNDS to the reference recordings directory')
    if name in COMPOSITES:
        # Each contributor is capped to the same length so that a long recording
        # does not quietly outvote a short one.
        parts = [_measure(n, COMPOSITE_CAP_SEC) for n in COMPOSITES[name]]
        f = _average(parts, name)
    else:
        f = _measure(name)
    _cache[name] = f
    return f
