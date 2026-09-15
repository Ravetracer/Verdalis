"""Writes the factory preset library.

    python3 makepresets.py [--outdir ../../presets]

Thirty presets, five for each of the six families the reference library is
filed under. They are generated rather than typed because they have to stay
consistent with one another: every one of them carries all 64 parameters, and a
parameter added to params.cpp has to appear in all thirty at its measured
default rather than in whichever ones somebody remembered to edit.

The base values below are the parameter defaults. Each preset is that base with
a handful of overrides, and the overrides are what the family was measured to
need -- see tools/analysis/README.md for where the numbers come from and
verify.py for holding a rendered preset against them.
"""
import os, sys

OUTDIR = os.path.join(os.path.dirname(__file__), "../../presets")

# The parameter defaults, in table order. Percent parameters are written as the
# fraction the preset format stores; enums as the name the format stores.
BASE = [
    ("type", "Transition"), ("blend", 0), ("span", 3.7), ("peak", 0.33),
    ("hold", 0.10), ("rise", 1.8), ("fall", 2.5), ("variation", 0.35),

    ("noise", "White"), ("air_level", -6), ("air_cutoff", 2400), ("air_sweep", -1.6),
    ("air_reso", 0.25), ("air_curve", 2.0), ("air_tilt", 0), ("air_width", 0.6),

    ("wave", "Saw"), ("tone_level", -60), ("tone_pitch", 49), ("tone_glide", -12),
    ("tone_detune", 0.25), ("tone_width", 0.6),

    ("pan_start", -0.6), ("pan_end", 0.6), ("space_amount", 0.22), ("space_size", 0.45),
    ("space_damping", 0.5), ("space_width", 0.8),

    ("sub_level", -60), ("sub_pitch", 49), ("sub_drop", -12), ("sub_decay", 700),
    ("sub_drive", 0.2), ("sub_click", 0.3),

    ("hit_level", -60), ("hit_tone", 900), ("hit_decay", 220), ("hit_noise", 0.6),
    ("hit_body", 0.5), ("hit_time", 0.33),

    ("flutter_depth", 0), ("flutter_start", 6), ("flutter_end", 14),
    ("flutter_shape", "Sine"), ("flutter_target", "Level"), ("flutter_smooth", 0.35),

    ("highpass", 25), ("lowpass", 18000), ("eq_low_freq", 90), ("eq_low_gain", 0),
    ("eq_mid_freq", 900), ("eq_mid_gain", 0), ("eq_high_freq", 5000), ("eq_high_gain", 0),

    ("attack", 1), ("decay", 2000), ("sustain", 1.0), ("release", 300),
    ("vel_to_level", 0.5), ("vel_to_tone", 0.3),

    ("gain", -4), ("drive", 0.12), ("max_voices", 8), ("seed", 0),
]

# The module headings the preset writer groups by, so a generated file and a
# file saved from the window look the same.
GROUPS = [
    ("Gesture", 0, 8), ("Air", 8, 16), ("Tone", 16, 22), ("Motion", 22, 28),
    ("Sub", 28, 34), ("Hit", 34, 40), ("Flutter", 40, 46), ("EQ", 46, 54),
    ("Envelope", 54, 60), ("Output", 60, 64),
]


def preset(file, name, description, features, **over):
    return dict(file=file, name=name, description=description, features=features, over=over)


# --------------------------------------------------------------------- accents
#
# Measured: span 3.55 s, peak 4.3 % in, fall 1.40 s, centroid -0.69 octaves,
# 69 % below 100 Hz, crest 19.0 dB, L/R correlation 0.58. The shortest tails in
# the library and the highest crest factor -- these are stabs.
ACCENTS = [
    preset("accent_steel_stab", "Steel Stab",
           "A struck metal accent: the hit layer's three inharmonic modes with almost no "
           "noise over them, a short bright body and a room that lets them ring out.",
           "accent, metal, stab",
           type="Accent", span=2.6, peak=0.02, hold=0.0, rise=0.6, fall=4.0,
           air_level=-22, air_cutoff=5000, air_sweep=-2.2, air_reso=0.45,
           hit_level=-4, hit_tone=1600, hit_decay=300, hit_noise=0.18, hit_body=0.35,
           hit_time=0.02, sub_level=-16, sub_pitch=70, sub_drop=-8, sub_decay=280,
           pan_start=-0.15, pan_end=0.15, space_amount=0.3, space_size=0.4,
           space_damping=0.45, eq_high_gain=3, gain=0),
    preset("accent_war_drum", "War Drum",
           "The low end of an accent rather than its edge: a struck drum with the click "
           "turned well up, tuned an octave under the stabs and given nothing above 3 kHz.",
           "accent, drum, low",
           type="Accent", span=3.6, peak=0.015, hold=0.0, rise=0.5, fall=3.2,
           air_level=-26, air_cutoff=1200, noise="Brown",
           hit_level=-9, hit_tone=280, hit_decay=200, hit_noise=0.35, hit_body=0.85,
           hit_time=0.015, sub_level=-2, sub_pitch=58, sub_drop=-9, sub_decay=600,
           sub_drive=0.4, sub_click=0.7, pan_start=0, pan_end=0,
           space_amount=0.26, space_damping=0.7, lowpass=3000, eq_low_gain=3, gain=-4),
    preset("accent_glass_break", "Glass Break",
           "All top and no bottom -- no sub layer at all, and a 180 Hz corner under it: a "
           "bright noise burst with the hit layer pushed fully into debris, and violet "
           "noise over it so the shards keep coming.",
           "accent, bright, debris",
           type="Accent", span=2.6, peak=0.02, hold=0.0, rise=0.5, fall=3.6,
           noise="Violet", air_level=-10, air_cutoff=8000, air_sweep=-1.2, air_reso=0.15,
           air_width=0.85, hit_level=-7, hit_tone=4200, hit_decay=140, hit_noise=0.92,
           hit_body=0.12, hit_time=0.02, pan_start=-0.3, pan_end=0.3, space_amount=0.34, space_damping=0.3,
           highpass=180, eq_high_gain=4, gain=-2),
    preset("accent_jumpscare", "Jumpscare",
           "The one accent with a run-up: a very short rising whoosh into a hard hit, so "
           "the ear has just enough time to know something is coming.",
           "accent, riser, scare",
           type="Accent", span=2.2, peak=0.34, hold=0.0, rise=3.0, fall=5.0,
           noise="Blue", air_level=-7, air_cutoff=1800, air_sweep=2.6, air_reso=0.5,
           air_curve=1.2, air_width=0.8, hit_level=-6, hit_tone=2200, hit_decay=260,
           hit_noise=0.55, hit_body=0.4, hit_time=0.35,
           sub_level=-8, sub_pitch=52, sub_drop=-10, sub_decay=520, sub_click=0.5,
           pan_start=-0.25, pan_end=0.25, space_amount=0.3, drive=0.2, gain=-5),
    preset("accent_gate_closed", "Gate Closed",
           "A heavy door: a dull thud with one metallic mode left ringing on top of it, "
           "and the room doing most of the work afterwards. No sub layer -- the hit's "
           "own body carries the weight, and a sub under it measured 0.90 below 100 Hz "
           "against the family's 0.69.",
           "accent, thud, metal",
           type="Accent", span=4.4, peak=0.018, hold=0.0, rise=0.5, fall=2.6,
           noise="Brown", air_level=-12, air_cutoff=900, air_sweep=-1.0,
           hit_level=0, hit_tone=420, hit_decay=520, hit_noise=0.25, hit_body=0.7,
           hit_time=0.018, sub_level=-60, sub_pitch=62, sub_drop=-7, sub_decay=450,
           sub_drive=0.3, pan_start=-0.1, pan_end=0.1,
           space_amount=0.4, space_size=0.6, space_damping=0.6, lowpass=6000, gain=4),
    preset("accent_dry_snap", "Dry Snap",
           "The quiet end of the library, and the accent counterpart to Simple Whoosh: "
           "one short noise stab and nothing at all under it -- no sub, no transient, "
           "barely any room. Deliberately outside the family's 0.69 below 100 Hz, for a "
           "cut where the mix has no space left for a low layer.",
           "accent, thin, dry",
           type="Accent", span=1.8, peak=0.03, hold=0.0, rise=0.5, fall=4.5,
           air_level=-4, air_cutoff=6000, air_sweep=-2.6, air_reso=0.55,
           air_curve=2.4, air_width=0.5, pan_start=-0.1, pan_end=0.1,
           space_amount=0.12, space_size=0.3, space_damping=0.5,
           highpass=120, eq_high_gain=2, gain=-6),
]

# ----------------------------------------------------------------------- booms
#
# Measured: 97 % below 100 Hz, peak 2.4 % in, fall 2.84 s, 63 Hz the loudest
# octave by 12.5 dB over 125, and essentially no sweep at all (-0.12 octaves).
# A boom is one long low decay and very little else.
BOOMS = [
    preset("boom_bottomless", "Bottomless",
           "The deepest thing in the library: a sub at 42 Hz falling most of an octave "
           "across four seconds, with a large soft room under it and nothing above 1 kHz.",
           "boom, deep, long",
           type="Boom", span=7.0, peak=0.012, hold=0.0, rise=0.5, fall=2.2,
           noise="Brown", air_level=-30, air_cutoff=500, air_sweep=-1.5,
           sub_level=0, sub_pitch=42, sub_drop=-9, sub_decay=3200, sub_drive=0.25,
           sub_click=0.25, pan_start=0, pan_end=0,
           space_amount=0.34, space_size=0.75, space_damping=0.75, space_width=0.7,
           lowpass=1600, eq_low_gain=3, gain=-8),
    preset("boom_deep_sonar", "Deep Sonar",
           "A boom with a note in it: the tone layer at the same pitch as the sub and "
           "barely detuned, so the decay has a pitch to follow all the way down.",
           "boom, tonal, sonar",
           type="Boom", span=6.4, peak=0.02, hold=0.04, rise=0.8, fall=2.0,
           air_level=-34, wave="Sine", tone_level=-11, tone_pitch=55, tone_glide=-5,
           tone_detune=0.06, tone_width=0.35,
           sub_level=-2, sub_pitch=55, sub_drop=-6, sub_decay=2600, sub_drive=0.15,
           sub_click=0.15, pan_start=0, pan_end=0,
           space_amount=0.38, space_size=0.7, space_damping=0.6,
           lowpass=2400, gain=-7),
    preset("boom_underground_roll", "Underground Roll",
           "Driven rather than deep: the sub saturated hard so it carries on a small "
           "speaker, with a slow flutter under it that keeps the decay moving.",
           "boom, driven, roll",
           type="Boom", span=6.0, peak=0.02, hold=0.06, rise=0.7, fall=1.8,
           noise="Brown", air_level=-24, air_cutoff=400, air_sweep=-1.0, air_width=0.4,
           sub_level=-1, sub_pitch=48, sub_drop=-10, sub_decay=2400, sub_drive=0.75,
           sub_click=0.35,
           flutter_depth=0.28, flutter_start=3.0, flutter_end=1.4, flutter_shape="Sine",
           flutter_target="Level", flutter_smooth=0.7,
           pan_start=0, pan_end=0, space_amount=0.3, space_size=0.65, space_damping=0.7,
           lowpass=2000, drive=0.3, gain=-4),
    preset("boom_whisper", "Whisper",
           "A boom a long way off: quiet, almost no click, and mostly the room it "
           "happened in. Useful under dialogue, where a loud one is unusable.",
           "boom, distant, quiet",
           type="Boom", span=6.8, peak=0.03, hold=0.0, rise=1.2, fall=2.0,
           noise="Brown", air_level=-28, air_cutoff=700, air_sweep=-1.8, air_width=0.75,
           sub_level=-9, sub_pitch=46, sub_drop=-7, sub_decay=2000, sub_drive=0.1,
           sub_click=0.05, pan_start=0, pan_end=0,
           space_amount=0.55, space_size=0.85, space_damping=0.55, space_width=0.95,
           lowpass=3200, gain=-9),
    preset("boom_shattered_earth", "Shattered Earth",
           "A boom that breaks something: the full sub with the hit layer pushed into "
           "debris over it, landing a moment after the impact rather than with it.",
           "boom, debris, impact",
           type="Boom", span=6.6, peak=0.015, hold=0.0, rise=0.5, fall=2.4,
           noise="Brown", air_level=-17, air_cutoff=1400, air_sweep=-2.4, air_reso=0.3,
           sub_level=-1, sub_pitch=50, sub_drop=-11, sub_decay=2800, sub_drive=0.45,
           sub_click=0.5,
           hit_level=-13, hit_tone=700, hit_decay=800, hit_noise=0.85, hit_body=0.6,
           hit_time=0.05, pan_start=-0.2, pan_end=0.2,
           space_amount=0.36, space_size=0.7, space_damping=0.65,
           lowpass=5000, drive=0.24, gain=-6),
]

# ---------------------------------------------------------------------- braams
#
# The only family whose spectrum *rises*: +0.60 octaves of centroid and more
# than an octave of low partial across the gesture. Measured hold plateau about
# a third of the span, the flattest octave curve in the library through the
# middle, and the longest per-band decays -- 1.5 to 2.2 s everywhere.
BRAAMS = [
    preset("braam_big_horn", "Big Horn",
           "The archetype: a detuned saw stack that swells for a second and a half and "
           "opens as it goes, which is the one thing the measurement says a braam does "
           "and nothing else in the library does.",
           "braam, horn, swell",
           type="Braam", span=5.6, peak=0.16, hold=0.34, rise=2.0, fall=2.4,
           air_level=-22, air_cutoff=1800, air_sweep=1.4, air_reso=0.2,
           wave="Supersaw", tone_level=-8, tone_pitch=49, tone_glide=7,
           tone_detune=0.35, tone_width=0.8,
           sub_level=-8, sub_pitch=49, sub_drop=-3, sub_decay=1800, sub_drive=0.3,
           sub_click=0.1, pan_start=-0.15, pan_end=0.15,
           space_amount=0.3, space_size=0.6, space_damping=0.55,
           lowpass=9000, attack=6, release=600, drive=0.2, gain=-3),
    preset("braam_annihilation", "Annihilation",
           "The same horn pulled apart: detune at the top of its range and the stack "
           "spread fully across the field, so it reads as a section rather than a note.",
           "braam, wide, dense",
           type="Braam", span=6.2, peak=0.12, hold=0.40, rise=1.5, fall=2.0,
           noise="Brown", air_level=-18, air_cutoff=1200, air_sweep=1.8, air_reso=0.3,
           air_width=0.85, wave="Supersaw", tone_level=-9, tone_pitch=41,
           tone_glide=5, tone_detune=0.72, tone_width=1.0,
           sub_level=-9, sub_pitch=41, sub_drop=-2, sub_decay=2200, sub_drive=0.4,
           sub_click=0.08, pan_start=-0.2, pan_end=0.2,
           space_amount=0.34, space_size=0.65, space_damping=0.5,
           lowpass=7000, attack=8, release=800, drive=0.26, gain=-3),
    preset("braam_bad_guy", "Bad Guy",
           "Square rather than saw, and low: the hollow odd-harmonic version of a braam, "
           "slow to arrive and slow to leave. The square at 37 is already the bottom of "
           "the sound, so there is no sub under it -- adding one moved the low fraction "
           "by 0.02 and nothing else.",
           "braam, square, menace",
           type="Braam", span=8.0, peak=0.22, hold=0.34, rise=2.4, fall=2.0,
           air_level=-26, air_cutoff=900, air_sweep=1.2,
           wave="Square", tone_level=-10, tone_pitch=37, tone_glide=3,
           tone_detune=0.18, tone_width=0.55,
           sub_level=-60, sub_pitch=37, sub_drop=-2, sub_decay=2600, sub_drive=0.35,
           sub_click=0.05, pan_start=-0.1, pan_end=0.1,
           space_amount=0.32, space_size=0.7, space_damping=0.6,
           lowpass=5000, attack=12, release=900, gain=0),
    preset("braam_rising_dread", "Rising Dread",
           "A braam used as a riser: a full octave of glide, the noise layer opening with "
           "it, and no plateau at all -- it arrives at the top and stops.",
           "braam, riser, glide",
           type="Braam", span=7.0, peak=0.62, hold=0.02, rise=2.6, fall=5.5,
           noise="Blue", air_level=-14, air_cutoff=1100, air_sweep=3.0, air_reso=0.35,
           air_curve=1.4, air_width=0.75, wave="Supersaw", tone_level=-10,
           tone_pitch=44, tone_glide=12, tone_detune=0.4, tone_width=0.85,
           sub_level=-14, sub_pitch=44, sub_drop=2, sub_decay=2400, sub_click=0.05,
           pan_start=-0.25, pan_end=0.25,
           space_amount=0.28, space_size=0.6, space_damping=0.4,
           attack=10, release=400, drive=0.22, gain=-1),
    preset("braam_brass_wall", "Brass Wall",
           "The loudest of the five: saw stack, sub and noise all up, driven into the "
           "output stage so the crest factor comes down to where the references sit.",
           "braam, loud, brass",
           type="Braam", span=5.0, peak=0.10, hold=0.42, rise=1.4, fall=2.6,
           air_level=-16, air_cutoff=2200, air_sweep=1.0, air_reso=0.28,
           wave="Saw", tone_level=-7, tone_pitch=55, tone_glide=5,
           tone_detune=0.3, tone_width=0.7,
           sub_level=-4, sub_pitch=55, sub_drop=-4, sub_decay=1600, sub_drive=0.5,
           sub_click=0.2, pan_start=-0.12, pan_end=0.12,
           space_amount=0.26, space_size=0.55, space_damping=0.6,
           lowpass=8000, eq_mid_gain=-3, eq_mid_freq=700,
           attack=4, release=500, drive=0.45, gain=-3),
]

# ----------------------------------------------------------------- downshifters
#
# The steepest spectrum in the library -- 10.2 dB/octave above 125 Hz, 98 %
# below 100 Hz, nothing at all above 4 kHz -- the lowest crest factor at 7.7 dB,
# and the only family with real flutter in it. `Downshifter - Stutter Scream`
# accelerates from 4.1 Hz to 25.0 Hz across its span; the measured end-to-start
# ratios across the family run from 0.22 to 6.08.
DOWNSHIFTERS = [
    preset("downshifter_analog_fall", "Analog Fall",
           "Two octaves of falling saw with the flutter gliding up underneath it, from "
           "3 Hz at the top to 14 by the bottom. The measured median of the family is a "
           "doubling; this is closer to five times, which is what the ear reads as "
           "falling faster than it is.",
           "downshifter, analog, fall",
           type="Downshifter", span=4.6, peak=0.03, hold=0.58, rise=0.8, fall=2.0,
           noise="Brown", air_level=-13, air_cutoff=1600, air_sweep=-3.6, air_reso=0.4,
           air_curve=1.0, air_width=0.35,
           wave="Saw", tone_level=-11, tone_pitch=110, tone_glide=-24,
           tone_detune=0.2, tone_width=0.45,
           sub_level=-4, sub_pitch=55, sub_drop=-12, sub_decay=3000, sub_drive=0.4,
           flutter_depth=0.45, flutter_start=3.0, flutter_end=14.0,
           flutter_shape="Sine", flutter_target="Both", flutter_smooth=0.45,
           pan_start=0, pan_end=0, space_amount=0.24, space_size=0.5, space_damping=0.7,
           lowpass=4000, drive=0.4, gain=-4),
    preset("downshifter_stutter_scream", "Stutter Scream",
           "Fitted directly to `Downshifter - Stutter Scream`, which accelerates from "
           "4.1 Hz to 25.0 Hz across its span and is the most strongly periodic thing in "
           "the whole 211-file library. A square gate on the level, so it chops rather "
           "than wobbles.",
           "downshifter, stutter, gate",
           type="Downshifter", span=4.2, peak=0.02, hold=0.62, rise=0.6, fall=1.8,
           air_level=-10, air_cutoff=2600, air_sweep=-4.0, air_reso=0.55, air_curve=1.0,
           air_width=0.3, wave="Saw", tone_level=-13, tone_pitch=140, tone_glide=-28,
           tone_detune=0.3, tone_width=0.4,
           sub_level=-5, sub_pitch=58, sub_drop=-14, sub_decay=2800, sub_drive=0.5,
           flutter_depth=0.85, flutter_start=4.1, flutter_end=25.0,
           flutter_shape="Square", flutter_target="Level", flutter_smooth=0.3,
           pan_start=0, pan_end=0, space_amount=0.2, space_damping=0.65,
           lowpass=5000, drive=0.5, gain=-2),
    preset("downshifter_grind_halt", "Grind To A Halt",
           "The other direction, which the measurement says happens just as often: the "
           "flutter starts fast and slows to almost nothing, so the gesture reads as "
           "something losing power rather than gaining it.",
           "downshifter, decelerate, grind",
           type="Downshifter", span=5.5, peak=0.04, hold=0.56, rise=0.9, fall=1.6,
           noise="Brown", air_level=-12, air_cutoff=1100, air_sweep=-2.2, air_reso=0.5,
           air_width=0.3, wave="Square", tone_level=-12, tone_pitch=90, tone_glide=-20,
           tone_detune=0.16, tone_width=0.35,
           sub_level=-4, sub_pitch=50, sub_drop=-10, sub_decay=3400, sub_drive=0.45,
           flutter_depth=0.65, flutter_start=18.0, flutter_end=2.2,
           flutter_shape="Ramp", flutter_target="Both", flutter_smooth=0.5,
           pan_start=0, pan_end=0, space_amount=0.22, space_damping=0.7,
           lowpass=3500, drive=0.45, gain=-3),
    preset("downshifter_digging_deep", "Digging Deep",
           "No pitched layer at all: the sub and the noise together, with the flutter on "
           "the cutoff rather than the level. That is what most of the references are "
           "actually doing -- the filter is fluttering, not the amplitude.",
           "downshifter, filter, deep",
           type="Downshifter", span=5.8, peak=0.03, hold=0.60, rise=0.8, fall=2.2,
           noise="Brown", air_level=-9, air_cutoff=900, air_sweep=-3.2, air_reso=0.62,
           air_width=0.25,
           sub_level=-3, sub_pitch=46, sub_drop=-13, sub_decay=3600, sub_drive=0.6,
           flutter_depth=0.7, flutter_start=5.0, flutter_end=16.0,
           flutter_shape="Triangle", flutter_target="Filter", flutter_smooth=0.4,
           pan_start=0, pan_end=0, space_amount=0.26, space_size=0.55, space_damping=0.75,
           lowpass=2600, eq_low_gain=3, drive=0.45, gain=-4),
    preset("downshifter_retro_drop", "Retro Drop",
           "A square wave, a fast hard gate and no room to speak of: the one preset in "
           "the family that sounds like a machine rather than like a collapse.",
           "downshifter, square, retro",
           type="Downshifter", span=3.4, peak=0.02, hold=0.64, rise=0.5, fall=2.4,
           air_level=-18, air_cutoff=2000, air_sweep=-2.0, air_reso=0.45, air_width=0.2,
           wave="Square", tone_level=-9, tone_pitch=160, tone_glide=-30,
           tone_detune=0.08, tone_width=0.3,
           sub_level=-7, sub_pitch=64, sub_drop=-12, sub_decay=2600, sub_drive=0.55,
           flutter_depth=0.9, flutter_start=9.0, flutter_end=22.0,
           flutter_shape="Square", flutter_target="Level", flutter_smooth=0.12,
           pan_start=0, pan_end=0, space_amount=0.12, space_size=0.3, space_damping=0.5,
           lowpass=6000, drive=0.35, gain=-3),
]

# --------------------------------------------------------------------- impacts
#
# Measured: peak 6.0 % in, 92 % below 100 Hz, centroid falling 0.63 octaves,
# per-band decays of 0.78 s at 63 Hz against 0.21 at 8 kHz. A boom with a top on
# it: the same low decay, plus a transient that is gone in a fifth of the time.
IMPACTS = [
    preset("impact_deep_impact", "Deep Impact",
           "The cinematic one: a hard transient on top of a long low decay, with the "
           "0.78 s at 63 Hz and 0.21 s at 8 kHz the library measures built in as the "
           "difference between Sub Decay and Hit Decay.",
           "impact, cinematic, deep",
           type="Impact", span=5.2, peak=0.012, hold=0.0, rise=0.5, fall=2.6,
           noise="Brown", air_level=-16, air_cutoff=2600, air_sweep=-3.0, air_reso=0.3,
           air_width=0.5,
           sub_level=-1, sub_pitch=48, sub_drop=-11, sub_decay=2300, sub_drive=0.4,
           sub_click=0.55,
           hit_level=-7, hit_tone=900, hit_decay=260, hit_noise=0.7, hit_body=0.6,
           hit_time=0.012, pan_start=0, pan_end=0,
           space_amount=0.32, space_size=0.6, space_damping=0.72,
           lowpass=9000, eq_low_gain=2, drive=0.22, gain=-5),
    preset("impact_metallic_collision", "Metallic Collision",
           "Two large metal things meeting: the hit layer's inharmonic cluster with the "
           "noise mix low, so the modes ring past the transient that excited them.",
           "impact, metal, ring",
           type="Impact", span=6.0, peak=0.015, hold=0.0, rise=0.5, fall=2.2,
           air_level=-18, air_cutoff=4000, air_sweep=-2.4, air_reso=0.4, air_width=0.6,
           sub_level=-5, sub_pitch=55, sub_drop=-9, sub_decay=1900, sub_drive=0.3,
           sub_click=0.45,
           hit_level=-6, hit_tone=1400, hit_decay=650, hit_noise=0.22, hit_body=0.45,
           hit_time=0.015, pan_start=-0.15, pan_end=0.15,
           space_amount=0.36, space_size=0.65, space_damping=0.62,
           lowpass=9000, gain=-4),
    preset("impact_head_on", "Head-on Collision",
           "Loud and broken: everything driven, the transient pushed fully into debris, "
           "and the widest stereo picture of the five.",
           "impact, debris, loud",
           type="Impact", span=5.6, peak=0.018, hold=0.02, rise=0.6, fall=2.0,
           noise="Pink", air_level=-11, air_cutoff=3400, air_sweep=-2.8, air_reso=0.35,
           air_width=0.9,
           sub_level=-2, sub_pitch=44, sub_drop=-12, sub_decay=2400, sub_drive=0.6,
           sub_click=0.6,
           hit_level=-5, hit_tone=1100, hit_decay=520, hit_noise=0.95, hit_body=0.55,
           hit_time=0.018, pan_start=-0.3, pan_end=0.3,
           space_amount=0.34, space_size=0.6, space_damping=0.72,
           lowpass=9000, drive=0.45, gain=-5),
    preset("impact_low_thud", "Low Thud",
           "An impact with the top taken off: nothing above 1.2 kHz, a short body and a "
           "dry room. What an impact sounds like from the other side of a wall.",
           "impact, dark, dry",
           type="Impact", span=4.0, peak=0.012, hold=0.0, rise=0.5, fall=3.0,
           noise="Brown", air_level=-20, air_cutoff=700, air_sweep=-1.6, air_width=0.35,
           sub_level=-2, sub_pitch=46, sub_drop=-10, sub_decay=1500, sub_drive=0.35,
           sub_click=0.5,
           hit_level=-12, hit_tone=260, hit_decay=180, hit_noise=0.5, hit_body=0.9,
           hit_time=0.012, pan_start=0, pan_end=0,
           space_amount=0.16, space_size=0.4, space_damping=0.85,
           lowpass=1200, eq_low_gain=4, gain=-6),
    preset("impact_blast_off", "Blast Off",
           "An impact that leaves something behind: the hit lands first and the noise "
           "layer opens *upwards* after it, which is the one place in this family the "
           "measured downward sweep is deliberately inverted.",
           "impact, riser, tail",
           type="Impact", span=6.5, peak=0.02, hold=0.30, rise=0.6, fall=1.8,
           noise="Pink", air_level=-12, air_cutoff=600, air_sweep=3.2, air_reso=0.42,
           air_curve=1.6, air_width=0.8,
           sub_level=-3, sub_pitch=50, sub_drop=-9, sub_decay=2600, sub_drive=0.45,
           sub_click=0.5,
           hit_level=-8, hit_tone=1000, hit_decay=340, hit_noise=0.75, hit_body=0.5,
           hit_time=0.02, pan_start=-0.2, pan_end=0.2,
           space_amount=0.4, space_size=0.7, space_damping=0.5,
           drive=0.28, gain=-6),
]

# ----------------------------------------------------------------- transitions
#
# The broadest curve in the library and the only family whose peak is not at the
# front: 32.8 % of the way in, with a measured rise of t^1.8, a centroid that is
# flat for the first third and then falls 0.80 octaves, and an L/R correlation
# of 0.60 -- the lowest of the six, because these are the ones that travel.
TRANSITIONS = [
    preset("transition_simple_whoosh", "Simple Whoosh",
           "The default patch, and the one everything else is built out of: white noise "
           "through one swept filter, crossing from left to right, and nothing else at "
           "all -- no sub, no tone, no transient. Every measured quantity in the gesture "
           "is the library's median for the family.",
           "transition, whoosh, default",
           gain=-1),
    preset("transition_deep_whoosh", "Deep Whoosh",
           "The same gesture an octave and a half down: brown noise, a low corner and a "
           "little sub under it. The brown noise and the 900 Hz corner do most of the "
           "work -- the sub is trimmed to sit behind them rather than replace them.",
           "transition, deep, low",
           span=4.2, peak=0.30, hold=0.12, rise=1.6, fall=2.2,
           noise="Brown", air_level=-4, air_cutoff=900, air_sweep=-1.8, air_reso=0.3,
           air_width=0.55,
           sub_level=-12, sub_pitch=46, sub_drop=-9, sub_decay=1400, sub_drive=0.35,
           sub_click=0.2, pan_start=-0.45, pan_end=0.45,
           space_amount=0.3, space_size=0.55, space_damping=0.65,
           lowpass=6000, eq_low_gain=3, gain=-5),
    preset("transition_passby", "Passby",
           "Short, fast and fully across the field: a narrow resonant band sweeping "
           "hard, which is what makes the ear hear something go past rather than fade. "
           "No low layer -- the 90 Hz corner would take it out again anyway.",
           "transition, passby, movement",
           span=1.9, peak=0.40, hold=0.04, rise=2.2, fall=2.2,
           noise="Pink", air_level=-4, air_cutoff=3200, air_sweep=-2.8, air_reso=0.72,
           air_curve=1.3, air_width=0.9,
           pan_start=-0.95, pan_end=0.95,
           space_amount=0.18, space_size=0.4, space_damping=0.4, space_width=0.95,
           highpass=90, gain=-4),
    preset("transition_reveal", "Reveal",
           "A long slow opening: the whole span is the rise, the filter opens instead of "
           "closing, and the gesture stops the moment it arrives. The one transition "
           "shape a sample library files separately and the measurement bears out.",
           "transition, riser, reveal",
           span=7.5, peak=0.88, hold=0.0, rise=2.6, fall=6.0,
           noise="Blue", air_level=-6, air_cutoff=400, air_sweep=4.0, air_reso=0.3,
           air_curve=1.5, air_width=0.8,
           sub_level=-12, sub_pitch=44, sub_drop=4, sub_decay=1600, sub_click=0.05,
           pan_start=-0.3, pan_end=0.3,
           space_amount=0.3, space_size=0.6, space_damping=0.4,
           attack=20, release=500, gain=1),
    preset("transition_whoosh_hit", "Whoosh Hit",
           "A whoosh that lands: the noise layer rising to a peak two thirds of the way "
           "in and the transient timed to arrive exactly there. The library files "
           "fifteen of these under Whoosh Hit, and the timing is the whole trick.",
           "transition, whoosh, impact",
           span=3.4, peak=0.66, hold=0.0, rise=2.4, fall=4.5,
           noise="White", air_level=-7, air_cutoff=2000, air_sweep=1.4, air_reso=0.4,
           air_curve=1.4, air_width=0.7,
           sub_level=-4, sub_pitch=48, sub_drop=-10, sub_decay=1200, sub_drive=0.4,
           sub_click=0.5,
           hit_level=-8, hit_tone=1200, hit_decay=280, hit_noise=0.75, hit_body=0.55,
           hit_time=0.66, pan_start=-0.55, pan_end=0.3,
           space_amount=0.3, space_size=0.55, space_damping=0.6,
           drive=0.2, gain=-5),
]

ALL = ACCENTS + BOOMS + BRAAMS + DOWNSHIFTERS + IMPACTS + TRANSITIONS


def render(p):
    values = dict(BASE)
    unknown = set(p["over"]) - set(values)
    if unknown:
        raise SystemExit("preset %s sets unknown parameters: %s" % (p["file"], sorted(unknown)))
    values.update(p["over"])

    out = ["# WhooshPact preset", "format = 1", "name = %s" % p["name"],
           "author = Ravetracer", "description = %s" % p["description"],
           "features = %s" % p["features"], ""]
    for _, lo, hi in GROUPS:
        for key, _default in BASE[lo:hi]:
            v = values[key]
            if isinstance(v, str):
                out.append("%s = %s" % (key, v))
            elif isinstance(v, float):
                out.append("%s = %s" % (key, ("%.6g" % v)))
            else:
                out.append("%s = %s" % (key, v))
    return "\n".join(out) + "\n"


def main(argv):
    outdir = OUTDIR
    if "--outdir" in argv:
        outdir = argv[argv.index("--outdir") + 1]
    os.makedirs(outdir, exist_ok=True)

    names = [p["file"] for p in ALL]
    if len(set(names)) != len(names):
        raise SystemExit("duplicate preset file names")

    # Anything already there that this script no longer writes is stale: the
    # library is defined here and nowhere else.
    keep = set(n + ".whooshpact" for n in names)
    for existing in sorted(os.listdir(outdir)):
        if existing.endswith(".whooshpact") and existing not in keep:
            os.remove(os.path.join(outdir, existing))
            print("removed stale %s" % existing)

    for p in ALL:
        path = os.path.join(outdir, p["file"] + ".whooshpact")
        with open(path, "w") as f:
            f.write(render(p))
    print("wrote %d presets into %s" % (len(ALL), outdir))


if __name__ == "__main__":
    main(sys.argv)
