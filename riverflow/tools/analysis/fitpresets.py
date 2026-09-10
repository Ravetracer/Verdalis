"""Fits the factory preset library to named references, and writes the files.

    python3 fitpresets.py [--refs DIR] [--out DIR] [--dry]

Each preset is one reference recording. What can be measured is measured; what
cannot is in PRESETS below with a reason. The measured half:

  shape, blend   The reference's octave-band colour is matched against the six
                 shipped cluster centroids, searching the shape and the
                 crossfade to the next one for the least squared error.
  tilt, body     The least-squares residual after that, as a slope about 1 kHz
                 and a weight on the lowest two bands.
  grain          From the 6-14 kHz band's 4 ms envelope variation, the cleanest
                 indicator of it: the engine's own calibration runs 0.13 at
                 Grain 0 to 0.25 at 0.85, and the event layers barely touch
                 that band.
  dabble level   From the 200-800 Hz band's variation above what Grain alone
                 produces, through the engine's measured calibration.
  trickle level  Likewise from the 2-6 kHz band.
  sizes          From the event-triggered spectra: a peak frequency is a radius
                 by Minnaert, r = 3.26 / f0.
  rates          From the detected event rates, relative to the library median
                 the defaults already sit at.
  turbulence     From the 100 ms modulation depth of the 1-2 kHz band.
  surge rate     From the strongest envelope modulation frequency.
  flow width     One minus the measured L/R correlation.

Distance is the one that cannot be measured, and the attempt is instructive: a
distant river and a dark river have the same spectrum, so fitting distance from
the top-octave slope returns nearly zero for every reference -- the shape match
has already absorbed whatever the air did. It is therefore chosen from what the
recording is of, and then *deconvolved out of the target* before the shape is
matched: the reference's band energies are divided by the engine's own air
absorption and distance tilt at that setting, so putting them back reproduces
the reference instead of darkening it twice.

That inversion is only stable while the air has not already taken the top away.
At Distance 0.7 the air filter sits at 2.7 kHz and undoing it asks the bed for
20 dB more top end than any of the six shapes has, so the chosen distances are
kept at or below 0.32. A reference that really was recorded from far off is
therefore represented by a genuinely darker *shape* at a modest distance rather
than a bright one heard through a lot of air. The two render the same; only the
first is numerically sound.

What is chosen rather than measured, and why: `bank_type`, the space controls
and the plunge pool. The first two would need a reverberation measurement the
library cannot give -- no impulse, no known source position -- and the plunge
pool's resonance is not separable from the bed's own low end by anything
measurable here. They are set from what the recording is of, and this paragraph
is the honest record of that.
"""
import sys, os, math, argparse
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../shared/tools/analysis"))
import numpy as np, wavio
import refs as R
import grain as G
import events as EV

OCT = R.OCT[R.OCT >= 125.0]

SHAPES = {
    "Deep Rush":      [-6.2, -3.2, -1.3, -1.5, -5.6, -10.5, -14.8, -24.5],
    "Rapids":         [-8.5, -4.7, -1.5, -0.7, -2.6, -5.5, -8.4, -14.2],
    "Mountain River": [-22.2, -9.2, -2.6, -0.8, -1.7, -6.5, -11.8, -19.0],
    "Stream":         [-15.2, -12.1, -6.5, -1.5, -1.2, -2.2, -4.6, -10.3],
    "Creek":          [-31.2, -21.0, -7.8, -2.8, -2.7, -1.1, -2.7, -17.6],
    "Trickle":        [-14.4, -15.8, -12.3, -9.8, -7.7, -6.2, -5.9, -0.5],
}
SHAPE_ORDER = list(SHAPES.keys())

# The engine's own calibration, measured by sweeping one parameter at a time and
# reading the band statistics back off the render. See README.md.
GRAIN_CV0, GRAIN_CV85 = 0.13, 0.25
DABBLE_BASE, DABBLE_K = 0.32, 31.7
TRICKLE_BASE, TRICKLE_K = 0.26, 6.4
LIB_DABBLE_RATE, LIB_TRICKLE_RATE = 5.5, 16.0
LIB_DABBLE_DETECT, LIB_TRICKLE_DETECT = 5.5, 15.9

def air_band_response(f, distance, air=0.5):
    """The engine's distance chain as power at frequency `f`.

    Two poles of air absorption plus a one-pole downward tilt mixed in at the
    distance's own weight: the same expressions updateFilters() uses.
    """
    d = float(np.clip(distance, 0.0, 1.0))
    f = np.asarray(f, float)
    if d <= 0.001:
        return np.ones_like(f)
    air_hz = float(np.clip(20000.0 * 0.06 ** (d * (1.35 - 0.7 * air)), 400.0, 20000.0))
    r = f / air_hz
    lp2 = (1.0 / (1.0 + r * r)) ** 2
    tilt_hz = float(np.clip(20000.0 * 0.25 ** d, 800.0, 20000.0))
    rt = f / tilt_hz
    re_ = (1.0 - d) + d / (1.0 + rt * rt)      # |(1-d) + d/(1+j rt)|^2
    im_ = -d * rt / (1.0 + rt * rt)
    return lp2 * (re_ * re_ + im_ * im_)


# name, reference, bank, distance, space (amount, size, damping),
# plunge (dB, Hz, depth, Q), description
PRESETS = [
 ("mountain_river", "mountain-river-001.wav", "Open", 0.22, (0.14, 0.55, 0.55), (-46, 150, 0.35, 0.45),
  "A mountain river in the open, heard from its bank. Almost no low end at all: the "
  "reference measures 48 dB down at 32 Hz."),
 ("mountain_river_large", "mountain-river-large-001.wav", "Open", 0.26, (0.18, 0.68, 0.5),
  (-40, 130, 0.4, 0.45),
  "The same river with more water in it. Wider and heavier, and the events thin out into "
  "the roar."),
 ("forest_stream", "forest-stream.wav", "Forest", 0.18, (0.22, 0.45, 0.62), (-48, 170, 0.3, 0.4),
  "A stream under trees. The banks are close but soft, so there is a little space and no ring."),
 ("wild_river", "forest_wildriver_atmo.wav", "Forest", 0.26, (0.24, 0.6, 0.55), (-38, 140, 0.45, 0.5),
  "A river running hard through woodland. One of the library's smoothest recordings -- "
  "statistically it is shaped noise, and this preset is honest about that."),
 ("river_rush", "river_waterrushing.wav", "Open", 0.22, (0.16, 0.6, 0.5), (-40, 160, 0.4, 0.45),
  "Water rushing over a shallow bed. Broad and even, with the events well down in it."),
 ("distant_river", "river_wilder_fartheraway.wav", "Open", 0.32, (0.3, 0.85, 0.45), (-44, 120, 0.3, 0.4),
  "The same water from further off. Nothing is countable any more: distance multiplies the "
  "events and smears their edges until they are the roar."),
 ("winter_river", "winter-river-at-night.wav", "Forest", 0.26, (0.2, 0.55, 0.6), (-42, 150, 0.4, 0.45),
  "A river at night in the cold. Cold air absorbs less, so more of the top survives the way over."),
 ("bubbling_creek", "creek-02-loop.wav", "Forest", 0.10, (0.2, 0.4, 0.6), (-52, 200, 0.3, 0.4),
  "The grainiest reference in the library below 800 Hz. This is the dabbling the plugin "
  "exists for: water folding over stones, one pocket of air at a time."),
 ("stony_creek", "creek-ambience-2.wav", "Rock Pool", 0.08, (0.26, 0.35, 0.4), (-50, 220, 0.35, 0.6),
  "A creek over bare stone. Grainy in every band at once, and bright with it."),
 ("forest_creek", "creek-ambience.wav", "Forest", 0.12, (0.22, 0.42, 0.6), (-50, 190, 0.3, 0.45),
  "A small creek in woodland, close enough that the individual pockets are countable."),
 ("wispy_creek", "wispy-creek-1.wav", "Forest", 0.10, (0.18, 0.4, 0.6), (-54, 210, 0.25, 0.4),
  "Barely any water at all. Thin, high and quiet."),
 ("upper_rapids", "tucker-creek-larger-upper-rapids.wav", "Gorge", 0.20, (0.34, 0.6, 0.4),
  (-34, 110, 0.5, 0.6),
  "Rapids in a cut, where the rock is close enough on both sides to send it back."),
 ("rapids_close", "bull-creek-rapids-close.wav", "Gorge", 0.05, (0.3, 0.5, 0.42), (-36, 120, 0.5, 0.6),
  "Standing at the rapids. Loud, close, and all mid band."),
 ("big_falls", "waterfall-saas-fee-catchment-south-big-01.wav", "Gorge", 0.30, (0.36, 0.7, 0.38),
  (-26, 90, 0.55, 0.65),
  "A big fall into a plunge pool. The weight underneath is the collective modes of the "
  "bubble cloud the falling water holds down there, not the bed."),
 ("glacier_falls", "waterfall-saas-fee-gletschersee-discharge-volley-big.wav", "Gorge", 0.26,
  (0.32, 0.65, 0.4), (-30, 100, 0.5, 0.6),
  "Meltwater discharging from under a glacier. Hard, cold, and bright at the top."),
 ("small_falls", "waterfall-small-c.wav", "Rock Pool", 0.15, (0.28, 0.42, 0.45), (-40, 150, 0.45, 0.6),
  "A small fall over a step of rock into a shallow bowl."),
 ("bubbly_falls", "waterfall_small_bubbly.wav", "Rock Pool", 0.08, (0.3, 0.4, 0.42), (-44, 180, 0.4, 0.65),
  "The bubbliest recording in the library. Almost all of this is countable events; the bed "
  "is barely there under them."),
 ("hanging_trickle", "waterfall-saas-fee-hannig-trickle.wav", "Cavern", 0.05, (0.4, 0.5, 0.35),
  (-56, 240, 0.25, 0.7),
  "Single drops off an overhang onto wet stone. The far end of the plugin: its 2-6 kHz band "
  "varies thirteen times as much as noise would, which is what makes it tick rather than hiss."),
 ("culvert_drain", "waterfall-saas-fee-dorfrundweg-drainage-system.wav", "Culvert", 0.10,
  (0.5, 0.3, 0.3), (-38, 130, 0.4, 0.75),
  "Water down a stone culvert. Boxy and close, and everything comes back off the walls."),
 ("gorge_cascade", "waterfall_meraner_hohenweg.wav", "Gorge", 0.30, (0.38, 0.75, 0.4),
  (-32, 105, 0.5, 0.6),
  "A cascade down a gorge wall, heard across the gap. Wide and bright, with the rock ringing."),
]


def band_cvs(m, sr):
    out = []
    for _, lo, hi in G.BANDS:
        if hi > sr * 0.45:
            out.append(float("nan")); continue
        e = G.band_envelope(m, sr, lo, hi)
        out.append(G.stats(e)[0] if e is not None else float("nan"))
    return out


def mod_depth(m, sr, lo, hi, ms=100.0):
    e = G.band_envelope(m, sr, lo, hi, env_ms=ms)
    return float(e.std() / max(e.mean(), 1e-12)) if e is not None else 0.0


def match_shape(bands):
    """Closest shipped centroid and crossfade, both normalised to their peak."""
    b = bands - bands.max()
    best = (1e30, 0, 0.0)
    for i in range(len(SHAPE_ORDER)):
        a = np.array(SHAPES[SHAPE_ORDER[i]], float)
        c = np.array(SHAPES[SHAPE_ORDER[min(i + 1, len(SHAPE_ORDER) - 1)]], float)
        for k in range(21):
            t = k / 20.0
            s = a * (1 - t) + c * t
            s = s - s.max()
            e = float(np.sum((s - b) ** 2))
            if e < best[0]:
                best = (e, i, t)
    return best[1], best[2], best[0]


def fit_one(path, spec, corr_tilt=0.0, corr_body=0.5):
    name, ref, bank, distance, space, plunge, desc = spec
    x, sr = wavio.read_wav(path)
    n = int(min(20.0 * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    x = x[st:st + n]
    m = wavio.to_mono(x)

    bands = R.band_levels(m, sr, OCT)
    if OCT[-1] > sr * 0.45:  # a 44.1 kHz reference has no 16 k octave
        bands[-1] = bands[-2] + (bands[-2] - bands[-3])
    # Take the chosen distance back out, so the bed is fitted to what the water
    # sounded like at the source and the distance chain puts the air back.
    bands = bands - 10.0 * np.log10(np.maximum(air_band_response(OCT, distance), 1e-6))
    wt, blend, _ = match_shape(bands)
    # A blend that has run all the way to the next shape *is* that shape.
    if blend > 0.975 and wt + 1 < len(SHAPE_ORDER):
        wt, blend = wt + 1, 0.0

    s = np.array(SHAPES[SHAPE_ORDER[wt]], float) * (1 - blend) + \
        np.array(SHAPES[SHAPE_ORDER[min(wt + 1, 5)]], float) * blend
    resid = (bands - bands.max()) - (s - s.max())
    lf = np.log2(OCT / 1000.0)
    slope = float(np.polyfit(lf, resid, 1)[0])
    tilt = float(np.clip(slope / 6.0 + corr_tilt, -1.0, 1.0))
    resid2 = resid - slope * lf
    body = float(np.clip(corr_body + float(np.mean(resid2[:2])) / 24.0, 0.0, 1.0))

    cvs = band_cvs(m, sr)
    cv_hi = cvs[3] if np.isfinite(cvs[3]) else 0.2
    grain = float(np.clip((cv_hi - GRAIN_CV0) / (GRAIN_CV85 - GRAIN_CV0) * 0.85, 0.0, 1.0))

    dabble_db = float(np.clip(10.0 * math.log10(max(cvs[0] - DABBLE_BASE, 1e-4) / DABBLE_K),
                              -60.0, -8.0))
    trickle_db = float(np.clip(10.0 * math.log10(max(cvs[2] - TRICKLE_BASE, 1e-4) / TRICKLE_K),
                               -60.0, -8.0))

    turb = float(np.clip(mod_depth(m, sr, 1000.0, 2000.0) / 0.50, 0.0, 1.0))
    e50 = R.envelope(m, sr, 50.0)
    modf, _ = R.mod_peak(e50, sr / max(1, int(sr * 0.05)))
    surge = float(np.clip(modf if modf == modf else 1.3, 0.1, 10.0))

    corr = 1.0
    if x.shape[1] >= 2:
        a, b = x[:, 0], x[:, 1]
        d = math.sqrt(float(np.sum(a * a) * np.sum(b * b)))
        corr = float(np.sum(a * b) / d) if d > 0 else 1.0
    flow_width = float(np.clip(1.0 - corr, 0.0, 1.0))
    # The events are panned as well as the bed, and they decorrelate what they
    # are added to. A reference measuring a correlation of 1.0 is one recording
    # of one place: its events have to sit in the middle too, or the render
    # comes out wider than the thing it is fitted to.
    width = float(np.clip((1.0 - corr) * 1.2, 0.0, 1.0))

    dab_size, tri_size = 3.3, 1.24
    dab_rate, tri_rate = LIB_DABBLE_RATE, LIB_TRICKLE_RATE
    try:
        r = EV.measure(path, "low", 12.0)
        if r:
            dab_size = float(np.clip(r["radius_mm"], 1.0, 12.0))
            dab_rate = float(np.clip(r["rate"] / LIB_DABBLE_DETECT * LIB_DABBLE_RATE, 0.2, 40.0))
    except Exception:
        pass
    try:
        r = EV.measure(path, "high", 12.0)
        if r:
            tri_size = float(np.clip(r["radius_mm"], 0.15, 3.0))
            tri_rate = float(np.clip(r["rate"] / LIB_TRICKLE_DETECT * LIB_TRICKLE_RATE, 0.5, 200.0))
    except Exception:
        pass

    return dict(name=name, ref=ref, desc=desc, bank=bank, space=space, plunge=plunge,
                water=SHAPE_ORDER[wt], blend=blend, tilt=tilt, body=body, grain=grain,
                dabble_db=dabble_db, trickle_db=trickle_db, turb=turb, surge=surge,
                flow_width=flow_width, width=width, dab_size=dab_size, tri_size=tri_size,
                dab_rate=dab_rate, tri_rate=tri_rate, corr=corr, distance=distance)


TEMPLATE = """# RiverFlow preset
format = 1
name = {title}
author = RiverFlow
description = {desc}
features = {features}

# Flow
water_type = {water}
water_blend = {blend:.2f}
flow_level = -24
flow_tilt = {tilt:.2f}
flow_body = {body:.2f}
turbulence = {turb:.2f}
surge_rate = {surge:.2f}
flow_grain = {grain:.2f}

# Stones
dabble_rate = {dab_rate:.2f}
dabble_level = {dabble_db:.1f}
dabble_size = {dab_size:.2f}
dabble_spread = 0.9
dabble_cluster = 4
dabble_spill = 25
dabble_damping = 1
dabble_glug = 0.35

# Trickle
trickle_rate = {tri_rate:.2f}
trickle_level = {trickle_db:.1f}
trickle_size = {tri_size:.2f}
trickle_spread = 1.2
trickle_decay = 13
trickle_impact = 0.45
stone_tone = 3500
splash = 0.25

# Plunge
plunge_level = {plunge_db}
plunge_tone = {plunge_tone}
plunge_depth = {plunge_depth}
plunge_q = {plunge_q}

# Reach
bank_type = {bank}
distance = {distance:.2f}
air = 0.5
width = {width:.2f}
flow_width = {flow_width:.2f}
space_amount = {space_amount}
space_size = {space_size}
space_damping = {space_damping}

# Filter
filter_type = Lowpass
highpass = 60
filter_cutoff = 20000
filter_reso = 0.1
filter_keytrack = 0

# Envelope
attack = 800
decay = 500
sustain = 1
release = 1500
vel_to_level = 0.4
vel_to_flow = 0.3

# Output
gain = 0
max_events = 512
seed = 0
"""


def features_for(f):
    tags = [f["water"].lower().replace(" ", "-")]
    if f["grain"] > 0.9:
        tags.append("grainy")
    if f["dabble_db"] > -20:
        tags.append("bubbly")
    if f["trickle_db"] > -20:
        tags.append("tickling")
    tags.append("distant" if f["distance"] > 0.45 else "close")
    return ", ".join(tags)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--refs", default=os.path.join(os.path.dirname(__file__),
                                                   "../../!dev/references"))
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "../../presets"))
    ap.add_argument("--dry", action="store_true")
    ap.add_argument("--iterate", type=int, default=0,
                    help="render the written presets and correct their tilt and body from the "
                         "measured band error, this many times")
    ap.add_argument("--render", default="", help="riverflow-render binary, for --iterate")
    ap.add_argument("--clap", default="", help="RiverFlow.clap, for --iterate")
    a = ap.parse_args()

    # The events add energy the bed's shape was already fitted to carry, so the
    # first pass comes out brighter than its reference. The bed's four degrees
    # of freedom cannot take a per-band correction, but the error is mostly a
    # slope and a low-end offset, and those it can: render, measure, fold the
    # error back into tilt and body, write again.
    corr = {p[0]: (0.0, 0.5) for p in PRESETS}
    passes = max(1, a.iterate + 1) if not a.dry else 1
    for it in range(passes):
        if it:
            corr = measure_correction(a, corr)
            print()
            print("--- pass %d, after correcting tilt and body from the render" % (it + 1))
        run_pass(a, corr)
    return 0


def measure_correction(a, prev):
    """Render what was just written and fold each preset's band error back in."""
    import subprocess, tempfile, shutil
    out = tempfile.mkdtemp(prefix="riverflow-fit-")
    try:
        subprocess.run([a.render, "--plugin", a.clap, "--all", "--outdir", out,
                        "--seconds", "20", "--tail", "1", "--rate", "48000",
                        "--param", "randomseed=7"], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        new = {}
        for spec in PRESETS:
            name, ref = spec[0], spec[1]
            gp = os.path.join(out, name.replace("_", " ").title().replace(" ", "_") + ".wav")
            rp = os.path.join(a.refs, ref)
            pt, pb = prev.get(name, (0.0, 0.0))
            if not (os.path.exists(gp) and os.path.exists(rp)):
                new[name] = (pt, pb); continue
            gb = band_shape(gp)
            rb = band_shape(rp)
            err = gb - rb
            lf = np.log2(OCT / 1000.0)
            slope = float(np.polyfit(lf, err, 1)[0])
            resid = err - slope * lf
            # Bounded. Where the shape plus its events simply cannot reach a
            # reference the loop would otherwise walk the correction into its
            # clamp and leave a preset with a 6 dB/octave tilt on it, which
            # sounds like a tone control at its end stop rather than a river.
            # A preset that hits this bound is one fit.py should still flag.
            new[name] = (float(np.clip(pt - slope / 6.0, -0.35, 0.35)),
                         float(np.clip(pb - float(np.mean(resid[:2])) / 24.0, 0.12, 0.88)))
        return new
    finally:
        shutil.rmtree(out, ignore_errors=True)


def band_shape(path):
    x, sr = wavio.read_wav(path)
    n = int(min(20.0 * sr, x.shape[0]))
    st = (x.shape[0] - n) // 2
    m = wavio.to_mono(x[st:st + n])
    b = R.band_levels(m, sr, OCT)
    if OCT[-1] > sr * 0.45:
        b[-1] = b[-2] + (b[-2] - b[-3])
    return b - b.max()


def run_pass(a, corr):
    print("%-22s %-15s %5s %6s %5s %5s %7s %7s %5s %5s %5s %5s %5s %5s" % (
        "preset", "shape", "blend", "tilt", "body", "grain", "dab dB", "tri dB",
        "turb", "surge", "fwid", "dabR", "triR", "dist"))
    for spec in PRESETS:
        path = os.path.join(a.refs, spec[1])
        if not os.path.exists(path):
            print("%-22s MISSING %s" % (spec[0], spec[1])); continue
        ct, cb = corr.get(spec[0], (0.0, 0.5))
        f = fit_one(path, spec, ct, cb)
        print("%-22s %-15s %5.2f %6.2f %5.2f %5.2f %7.1f %7.1f %5.2f %5.2f %5.2f %5.1f %5.1f %5.2f" % (
            f["name"], f["water"], f["blend"], f["tilt"], f["body"], f["grain"],
            f["dabble_db"], f["trickle_db"], f["turb"], f["surge"], f["flow_width"],
            f["dab_rate"], f["tri_rate"], f["distance"]))
        if a.dry:
            continue
        pdb, ptone, pdep, pq = f["plunge"]
        sa, ss, sd = f["space"]
        text = TEMPLATE.format(
            title=f["name"].replace("_", " ").title(), desc=f["desc"],
            features=features_for(f), water=f["water"], blend=f["blend"], tilt=f["tilt"],
            body=f["body"], turb=f["turb"], surge=f["surge"], grain=f["grain"],
            dab_rate=f["dab_rate"], dabble_db=f["dabble_db"], dab_size=f["dab_size"],
            tri_rate=f["tri_rate"], trickle_db=f["trickle_db"], tri_size=f["tri_size"],
            plunge_db=pdb, plunge_tone=ptone, plunge_depth=pdep, plunge_q=pq,
            bank=f["bank"], distance=f["distance"], flow_width=f["flow_width"],
            width=f["width"],
            space_amount=sa, space_size=ss, space_damping=sd)
        with open(os.path.join(a.out, f["name"] + ".riverflow"), "w") as fh:
            fh.write(text)


if __name__ == "__main__":
    sys.exit(main())
