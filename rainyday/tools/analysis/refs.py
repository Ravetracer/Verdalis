"""Loads the reference recordings and caches their features."""
import os
import numpy as np
import feat
from wavio import read_wav, to_mono

SOUND_DIR = os.environ.get('RAINYDAY_SOUNDS', '')


def rumble_filter(x, sr, fc=70.0):
    """Removes what is below ~70 Hz: wind and handling noise RainyDay cannot make."""
    k = max(1, int(sr / (2 * np.pi * fc)))
    ker = np.ones(k) / k
    lo = np.convolve(x, ker, mode='same')
    lo = np.convolve(lo, ker, mode='same')
    return x - lo


_cache = {}


def reference(name):
    if name in _cache:
        return _cache[name]
    if not SOUND_DIR:
        raise RuntimeError('set RAINYDAY_SOUNDS to the reference recordings directory')
    x, sr = read_wav(os.path.join(SOUND_DIR, name + '.wav'))
    m = to_mono(x)
    m -= m.mean()
    f = feat.extract(rumble_filter(m, sr), sr, name, denoise=True)
    _cache[name] = f
    return f
