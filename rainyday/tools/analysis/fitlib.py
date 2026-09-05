"""Driver for fitting RainyDay presets against reference recordings."""
import os
import struct
import subprocess
import tempfile
import numpy as np
import feat

SR = 48000
ROOT = os.environ.get('RAINYDAY_ROOT', os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

SECTIONS = [
    ('Rain', ['density', 'clumping', 'drop_pitch', 'pitch_spread', 'drop_decay',
              'decay_spread', 'tonality', 'bubble', 'impact', 'splash',
              'level_spread', 'chirp', 'surface', 'note_tracking']),
    ('Bed', ['bed_level', 'bed_tone', 'bed_body', 'bed_drift', 'bed_width',
             'bed_pan']),
    ('Space', ['width', 'drop_pan', 'distance', 'air', 'space_amount',
               'space_size', 'space_damping']),
    ('Filter', ['filter_type', 'highpass', 'filter_cutoff', 'filter_reso',
                'filter_key_track']),
    ('Envelope', ['attack', 'decay', 'sustain', 'release', 'vel_to_level',
                  'vel_to_density']),
    ('Output', ['gain', 'max_droplets', 'seed']),
]

KEYS = [k for _, keys in SECTIONS for k in keys]


def format_value(v):
    """Preset files are meant to be read and hand-edited, so a fitted value gets
    rounded to something a person would have typed. Enum names pass through."""
    if isinstance(v, str):
        try:
            v = float(v)
        except ValueError:
            return v
    v = float(v)
    if v == int(v) and abs(v) < 1e6:
        return str(int(v))
    a = abs(v)
    if a >= 100:
        text = f'{v:.0f}'
    elif a >= 10:
        text = f'{v:.1f}'
    elif a >= 1:
        text = f'{v:.2f}'
    else:
        text = f'{v:.3f}'
    return text.rstrip('0').rstrip('.') if '.' in text else text


def preset_text(params, meta=None):
    meta = meta or {}
    out = ['# RainyDay preset', 'format = 1']
    for k in ('name', 'author', 'description', 'features'):
        if k in meta:
            out.append(f'{k} = {meta[k]}')
    for title, keys in SECTIONS:
        rows = [(k, params[k]) for k in keys if k in params]
        if not rows:
            continue
        out.append('')
        out.append(f'# {title}')
        for k, v in rows:
            out.append(f'{k} = {format_value(v)}')
    return '\n'.join(out) + '\n'


def read_preset(path):
    p = {}
    meta = {}
    for line in open(path):
        line = line.split('#')[0].strip()
        if '=' not in line:
            continue
        k, v = [s.strip() for s in line.split('=', 1)]
        if k in ('name', 'author', 'description', 'features', 'format'):
            meta[k] = v
        else:
            p[k] = v
    return p, meta


class Renderer:
    """Keeps one fithost process alive and feeds it preset files."""

    def __init__(self, host=None, plugin=None):
        host = host or os.path.join(ROOT, 'build', 'rainyday-fithost')
        plugin = plugin or os.path.join(ROOT, 'build', 'RainyDay.clap')
        self.p = subprocess.Popen([host, plugin], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, cwd=os.path.join(ROOT, 'build'))
        self.tmp = tempfile.mkdtemp(prefix='rainyfit')
        self.n = 0

    def render(self, params, meta=None, seconds=6.0):
        path = os.path.join(self.tmp, 'c%d.rainyday' % (self.n % 4))
        self.n += 1
        with open(path, 'w') as f:
            f.write(preset_text(params, meta))
        self.p.stdin.write(f'{path}\t{seconds}\n'.encode())
        self.p.stdin.flush()
        head = self.p.stdout.read(4)
        n = struct.unpack('<I', head)[0]
        buf = b''
        while len(buf) < 4 * n:
            chunk = self.p.stdout.read(4 * n - len(buf))
            if not chunk:
                break
            buf += chunk
        return np.frombuffer(buf, dtype='<f4').astype(np.float64)

    def close(self):
        try:
            self.p.stdin.write(b'quit\n')
            self.p.stdin.flush()
        except Exception:
            pass
        self.p.wait(timeout=5)


def analyse_render(x, skip=1.5):
    """Features of a render, ignoring the envelope attack at the start."""
    s = int(skip * SR)
    if len(x) <= s + SR:
        s = 0
    return feat.extract(x[s:], SR)


# ---------------------------------------------------------------- objective
#
# Weights say what "sounds like this recording" means. The band spectrum is the
# spine of it; impulsiveness separates a wash from individual drops; temporal
# flatness catches the texture in between. Level is deliberately not compared,
# only spectral shape, because the library is loudness matched separately.

W_BANDS = 1.0
W_IMP = 0.55
W_TFLAT = 14.0
W_CREST = 0.15
W_MOD = 1.2
# Long-term spectral flatness is deliberately not used. It is the feature that
# misled the fit into buzzing: `multiple_water_drops` is three isolated drops in
# a quiet room and measures 0.03, so matching that number actively demands a
# dense cloud of tones. Per-frame flatness above measures what this was meant to
# measure, and measures it correctly.
W_FLAT = 0.0
# Per-frame flatness carries the heaviest weight of any single number here. It
# is the only feature that separates a noise band from a dense cloud of pitched
# droplets, and the two have the same long-term spectrum, so without it the fit
# will match a spectrum with the wrong microstructure and buzz. At this weight a
# preset that is right to within a few per cent pays almost nothing, while one
# that is an order of magnitude too tonal pays as much as a bad spectrum does.
W_FFLAT = 4000.0
# Ring time. `decay_ms` was measured from the very first version of this file
# and then not used, which left the objective unable to tell a droplet that
# rings for 30 ms from one that rings for 300 ms as long as the two had the same
# spectrum -- and ring time is the single feature that per-droplet measurement
# of the references pins down most sharply. Compared in the log domain, so the
# term means "out by this factor" rather than "out by this many milliseconds":
# a ring twice as long as the reference's costs as much as being 6 dB out in
# every band at once.
W_DECAY = 40.0
# The room and the rhythm, from isolated events (feat.room_stats). None of the
# features above can tell a long droplet ring from a long reverb tail, or a drop
# every three seconds from a drop every tenth of a second once the spectrum and
# the flatness agree, and Cave Drips was fitted to fourteen drops a second with
# no cave as a result. Rate and late RT60 are compared as log ratios, so a
# factor of two in either costs about as much as being 5 dB out in every band.
# The direct-to-late ratio is in dB. All three apply only where the reference
# has isolated events to measure them on; dense rain does not, and skips them.
W_RATE = 60.0
W_LATE_RT = 30.0
W_DIRECT_LATE = 1.0
# Per-frame flatness again, as a log ratio, for sparse references only. A cave
# drip measures 0.001 -- one tone per frame, the room ringing at the drop's own
# pitch -- and a render of it with a little splash noise measures 0.010. Squared,
# that difference is nothing against the weight above, which is scaled for rain
# at 0.1 to 0.3; heard, it is the difference between a drop and a filtered-noise
# drum hit. A factor of ten costs about as much here as 5 dB in every band.
W_FFLAT_LOG = 10.0
FFLAT_FLOOR = 5.0e-4
SPARSE_RATE_MAX = 15.0


def _finite(v):
    return v is not None and np.isfinite(np.float64(v))


def distance(a, b, band_weight=None):
    bw = np.ones(len(feat.BAND_NAMES)) if band_weight is None else np.asarray(band_weight)
    d = 0.0
    d += W_BANDS * float((bw * (a['bands'] - b['bands']) ** 2).sum()) / bw.sum()
    d += W_IMP * float(((a['imp'] - b['imp']) ** 2).mean())
    d += W_TFLAT * float(((a['tflat'] - b['tflat']) ** 2).mean()) * 100
    d += W_CREST * (a['crest'] - b['crest']) ** 2
    m = [v if np.isfinite(v) else 0.0 for v in (a['mod_db'], b['mod_db'])]
    d += W_MOD * (m[0] - m[1]) ** 2
    d += W_FFLAT * (a.get('fflat', 0.0) - b.get('fflat', 0.0)) ** 2
    da, db_ = a.get('decay_ms'), b.get('decay_ms')
    if da and db_ and np.isfinite(da) and np.isfinite(db_) and da > 0 and db_ > 0:
        d += W_DECAY * np.log2(da / db_) ** 2
    # b is the reference. Only a sparse reference has a rhythm and a room that
    # can be measured event by event.
    rb = b.get('event_rate', 0.0)
    # feat.room_stats decides what is sparse (most of the time, nothing much is
    # happening) and it is the reference's call: a composite of recordings
    # averages to the fraction of them that are, and half or more carries it.
    if (b.get('sparse', 0.0) or 0.0) >= 0.5 and _finite(rb) and 0.05 <= rb <= SPARSE_RATE_MAX:
        ra = max(float(a.get('event_rate', 0.0) or 0.0), 0.02)
        d += W_RATE * np.log2(ra / rb) ** 2
        ta, tb = a.get('late_rt'), b.get('late_rt')
        if _finite(tb) and tb > 0:
            ta = ta if (_finite(ta) and ta > 0) else 0.05
            d += W_LATE_RT * np.log2(ta / tb) ** 2
        la, lb = a.get('direct_late'), b.get('direct_late')
        if _finite(lb):
            la = la if _finite(la) else 40.0
            d += W_DIRECT_LATE * (la - lb) ** 2
        fa = max(float(a.get('fflat', 0.0) or 0.0), FFLAT_FLOOR)
        fb = max(float(b.get('fflat', 0.0) or 0.0), FFLAT_FLOOR)
        d += W_FFLAT_LOG * np.log2(fa / fb) ** 2
    return float(d)
