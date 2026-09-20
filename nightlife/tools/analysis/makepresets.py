"""Writes NightLife's factory presets.

    cd build && python3 ../tools/analysis/makepresets.py

A preset is a text file of parameter values, and sixteen of them written by
hand drift: one gets a layer the others do not, another keeps a default that
has since moved. So they are generated from one table instead, and the table is
here where the measurements are.

Two rules, both learned elsewhere in the suite:

  * **every optional layer starts at -60 dB.** A preset that does not mention
    the chorus should not have one. The base below silences all four layers and
    each preset turns on what it is.

  * **the description is documentation, not marketing.** It says what was
    measured and why the numbers are what they are; the manual prints it. The
    sentence a musician reads is a different text and lives in
    presets/demo-descriptions.txt.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "../../presets")
RENDER = os.environ.get("NIGHTLIFE_RENDER", "./nightlife-render")

# Everything off. A preset adds what it is.
BASE = {
    "shot_level": "-60", "pack_level": "-60", "chorus_level": "-60",
    "insect_level": "-60", "bed_level": "-60",
}

PRESETS = [
    ("Wolf Valley", {
        "caller": "Wolf", "pitch": "484", "contour": "0.55", "length": "1290",
        "calls": "1", "repeats": "1", "variation": "0.35", "legato": "0",
        "shot_level": "6", "pack_level": "-14", "pack_rate": "6", "animals": "2",
        "pitch_spread": "0.35", "answer": "0.25", "restless": "0.5",
        "voice": "0.26", "breath": "0.05", "throat": "26", "muzzle": "0.55",
        "distance": "0.62", "distance_spread": "0.4", "air": "0.45", "width": "0.6",
        "space_amount": "0.5", "space_size": "0.85", "space_damping": "0.45",
        "bed_level": "-26", "bed_tilt": "-0.2", "attack": "80", "release": "3000", "gain": "12",
    }, "One wolf, far off. The howl is a measured contour: the eight wolf "
       "archetypes run 311 ms to 2.8 s and six of them carry a vibrato of their own "
       "at 2.1 to 6.4 Hz, which is what a howl is and what no oscillator with a "
       "vibrato on it sounds like. Distance is doing most of the work -- the "
       "measured group median is 484 Hz and the air absorption at 0.62 takes "
       "everything above about 2 kHz off it."),

    ("Pack Answering", {
        "caller": "Wolf", "pitch": "470", "contour": "0.45", "length": "1200",
        "calls": "1", "repeats": "1", "variation": "0.5", "legato": "0",
        "shot_level": "-3", "pack_level": "-4", "pack_rate": "26", "animals": "6",
        "pitch_spread": "0.85", "voice_spread": "0.5", "answer": "0.85",
        "restless": "0.55", "voice": "0.3", "breath": "0.06", "throat": "24",
        "distance": "0.45", "distance_spread": "0.6", "width": "0.85",
        "space_amount": "0.55", "space_size": "0.9", "space_damping": "0.4",
        "bed_level": "-30", "release": "4000", "gain": "0",
    }, "A chorus of six. Answer is at 0.85, which is the whole preset: one animal "
       "calls and another replies from somewhere else a third of a second to two "
       "seconds later, and that behaviour -- not the voice -- is what makes a group "
       "of wolves sound like a group. Pitch Spread is wide at 0.85 octaves because "
       "wolves in a chorus avoid each other's pitch, which is why a pack sounds "
       "like more animals than it holds."),

    ("Tawny Wood", {
        "caller": "Owl", "pitch": "497", "contour": "0.4", "length": "181",
        "calls": "2", "call_rate": "2.85", "phrase_gap": "6.3", "repeats": "2",
        "variation": "0.2", "legato": "0.1", "motif": "-1.5",
        "shot_level": "6", "pack_level": "-12", "pack_rate": "30", "animals": "2",
        "pitch_spread": "0.25", "answer": "0.5", "voice": "0.2", "breath": "0.04",
        "throat": "22", "muzzle": "0.35", "formant": "0.7", "partials": "0.7",
        "distance": "0.4", "distance_spread": "0.35", "width": "0.55",
        "space_amount": "0.45", "space_size": "0.6", "space_damping": "0.7",
        "bed_level": "-28", "insect_level": "-34", "release": "2500", "gain": "6",
    }, "The two-note hoot, at the measured numbers: the owl group's phrases hold a "
       "median of two calls 2.85 Hz apart with 6.3 s between phrases, and its "
       "fundamental is 497 Hz with a single harmonic -- the purest voice in the "
       "library at a measured -34 dB of roughness. Motif drops the second note a "
       "tone and a half, which is what the tawny recordings do."),

    ("Barred Phrase", {
        "caller": "Owl", "pitch": "484", "contour": "0.7", "length": "220",
        "calls": "8", "call_rate": "3.2", "rate_drift": "0.18", "phrase_gap": "7",
        "repeats": "1", "variation": "0.25", "legato": "0.35", "motif": "-0.8",
        "shot_level": "6", "pack_level": "-16", "pack_rate": "22", "animals": "2",
        "answer": "0.4", "voice": "0.28", "breath": "0.05", "throat": "20",
        "distance": "0.35", "distance_spread": "0.3", "width": "0.5",
        "space_amount": "0.4", "space_size": "0.55", "space_damping": "0.65",
        "bed_level": "-30", "insect_level": "-36", "release": "2000", "gain": "9",
    }, "Eight notes in a row, which is the other thing an owl does: the barred owl "
       "recording segments into eleven calls of a median 240 ms in under four "
       "seconds. Rate Drift speeds the phrase up slightly as it goes and Motif "
       "walks it down, which is that species' signature and not a general owl "
       "behaviour."),

    ("Scops Metronome", {
        "caller": "Scops", "pitch": "1272", "contour": "0.6", "length": "907",
        "calls": "1", "phrase_gap": "2.6", "repeats": "16", "variation": "0.08",
        "legato": "0", "jitter": "0.05",
        "shot_level": "4", "pack_level": "-10", "pack_rate": "24", "animals": "1",
        "voice": "0.12", "breath": "0.03", "throat": "12", "muzzle": "0.3",
        "formant": "0.5", "partials": "0.75",
        "distance": "0.5", "distance_spread": "0.2", "width": "0.4",
        "space_amount": "0.4", "space_size": "0.65", "space_damping": "0.6",
        "bed_level": "-26", "insect_level": "-22", "trill_rate": "42",
        "release": "2500", "gain": "0",
    }, "One pip every two and a half seconds, for as long as you hold the note. "
       "The scops owl recording is 105 seconds of exactly that -- 42 calls at a "
       "median 1272 Hz whose measured contour barely moves, a sweep of 0.04 "
       "octaves, the flattest thing in the library. Variation is almost off "
       "because that is the point of the bird. The insect bed under it is from the "
       "same recording, which carries a cricket band at 3.2 kHz."),

    ("Screech Whinny", {
        "caller": "Screech", "pitch": "1256", "contour": "0.5", "length": "456",
        "calls": "1", "phrase_gap": "4.5", "repeats": "2", "variation": "0.4",
        "legato": "0.2", "shot_level": "6", "pack_level": "-12", "pack_rate": "22",
        "animals": "2", "pitch_spread": "0.4", "answer": "0.45",
        "voice": "0.35", "breath": "0.08", "rasp": "0.12", "throat": "14",
        "muzzle": "0.6", "distance": "0.35", "distance_spread": "0.4", "width": "0.65",
        "space_amount": "0.45", "space_size": "0.6", "space_damping": "0.6",
        "bed_level": "-30", "insect_level": "-30", "release": "2000", "gain": "5",
    }, "The descending trill, which is why the two screech owls are a caller of "
       "their own rather than filed with the hoots: their contours turn direction "
       "up to 143 times in a single call, against an owl's ten. The archetypes here "
       "are the one part of the library the contour fit under-resolves -- 48 terms "
       "over a three-second whinny draws 68 of those 143 turns -- so Detail is left "
       "wide open and the trill is as fine as the table can make it."),

    ("Vixen Scream", {
        "caller": "Fox", "pitch": "1053", "contour": "0.65", "length": "949",
        "calls": "2", "call_rate": "0.77", "phrase_gap": "3.4", "repeats": "2",
        "variation": "0.4", "legato": "0.15",
        "shot_level": "6", "pack_level": "-18", "pack_rate": "14", "animals": "1",
        "voice": "0.55", "breath": "0.18", "rasp": "0.35", "throat": "11",
        "muzzle": "0.7", "formant": "0.65", "radiate": "0.5", "partials": "0.7",
        "distance": "0.25", "distance_spread": "0.25", "width": "0.45",
        "space_amount": "0.35", "space_size": "0.6", "space_damping": "0.5",
        "bed_level": "-32", "release": "1500", "gain": "5",
    }, "Close, and deliberately unpleasant. The fox group is the noisiest thing in "
       "the library by a distance: a measured roughness of -23 dB against every "
       "other caller's -32 to -34, and three harmonics against their one. Rasp is "
       "up at 0.35 because a scream's roughness has structure -- the valve does not "
       "close the same way twice -- and no amount of extra harmonics is the same "
       "thing."),

    ("Fox Across Fields", {
        "caller": "Fox", "pitch": "980", "contour": "0.3", "length": "700",
        "calls": "3", "call_rate": "0.9", "phrase_gap": "6", "repeats": "1",
        "variation": "0.45", "legato": "0.1",
        "shot_level": "6", "pack_level": "-10", "pack_rate": "12", "animals": "2",
        "pitch_spread": "0.4", "answer": "0.55", "restless": "0.5",
        "voice": "0.45", "breath": "0.12", "rasp": "0.2", "throat": "13",
        "distance": "0.7", "distance_spread": "0.5", "air": "0.4", "width": "0.8",
        "space_amount": "0.5", "space_size": "0.8", "space_damping": "0.5",
        "bed_level": "-24", "bed_tilt": "-0.25", "insect_level": "-28",
        "release": "3000", "gain": "8",
    }, "The same animal at four hundred metres. Nothing about the voice changes "
       "except Distance, which is the point: the air absorption and the "
       "inverse-distance loss between them turn a scream into the thing people "
       "report as somebody shouting in the next field."),

    ("Loon Lake", {
        "caller": "Loon", "pitch": "1132", "contour": "0.45", "length": "268",
        "calls": "3", "call_rate": "1.74", "phrase_gap": "8", "repeats": "1",
        "variation": "0.3", "legato": "0.5", "motif": "1.2",
        "shot_level": "6", "pack_level": "-14", "pack_rate": "18", "animals": "2",
        "pitch_spread": "0.3", "answer": "0.6",
        "voice": "0.18", "breath": "0.04", "throat": "16", "muzzle": "0.45",
        "formant": "0.6", "partials": "0.7",
        "distance": "0.55", "distance_spread": "0.45", "air": "0.6", "width": "0.75",
        "space_amount": "0.62", "space_size": "0.95", "space_damping": "0.3",
        "bed_level": "-28", "release": "4000", "gain": "10",
    }, "A wail over open water. The loon is the only bird among the callers and the "
       "thinnest caller in the table -- one recording, eleven usable contours -- so "
       "its eight archetypes come from less material than any other's. The space is "
       "the largest in the factory set at 0.95 with the damping down at 0.3, "
       "because what carries a loon is a lake with a rock shore on the far side."),

    ("Pond Chorus", {
        "chorus_level": "-2", "croak": "0.44", "croak_pitch": "1535",
        "pulse_rate": "20", "pulses": "13", "croak_length": "608",
        "frogs": "16", "croak_rate": "48", "regularity": "0.95",
        "chorus_spread": "0.8", "chorus_width": "0.95",
        "distance": "0.3", "air": "0.55",
        "space_amount": "0.35", "space_size": "0.7", "space_damping": "0.6",
        "bed_level": "-24", "insect_level": "-26", "attack": "400", "release": "2500", "gain": "-3",
    }, "Sixteen frogs on one pond. Every number is one row of the croak table -- "
       "`frogs-6.wav`, measured at a 20 Hz pulse rate, thirteen pulses, a 608 ms "
       "croak and resonances at 1535 and 442 Hz -- and Chorus Spread puts the pond's "
       "other species around it. Regularity is at 0.95, which is not a taste "
       "judgement: the measured Fano factor of croak arrivals is 0.30 at four "
       "seconds where a Poisson process is 1.0, and this is the setting that "
       "renders it."),

    ("Spring Peepers", {
        "chorus_level": "-4", "croak": "0.56", "croak_pitch": "3299",
        "pulse_rate": "33", "pulses": "8", "croak_length": "60",
        "frogs": "24", "croak_rate": "300", "regularity": "0.9",
        "chorus_spread": "0.5", "chorus_width": "1.0",
        "distance": "0.4", "air": "0.6", "width": "0.9",
        "space_amount": "0.3", "space_size": "0.6", "space_damping": "0.65",
        "bed_level": "-26", "insect_level": "-24", "attack": "600", "release": "2000", "gain": "0",
    }, "The high, fast end of the table: `leopard-frogs-croaking.wav`, whose croaks "
       "are 43 ms at 3.3 kHz and arrive 480 times a minute -- four times the rate of "
       "anything else measured. Twenty-four of them at that rate is a wall, and the "
       "shortest croak in the library is what makes it a shimmer rather than a "
       "rattle."),

    ("Bullfrog Bank", {
        "chorus_level": "-3", "croak": "0.06", "croak_pitch": "1200",
        "pulse_rate": "9", "pulses": "4", "croak_length": "420",
        "frogs": "6", "croak_rate": "36", "regularity": "0.85",
        "chorus_spread": "0.35", "chorus_width": "0.7",
        "distance": "0.35", "air": "0.4",
        "space_amount": "0.4", "space_size": "0.75", "space_damping": "0.55",
        "bed_level": "-22", "bed_tilt": "-0.35", "attack": "500", "release": "3000", "gain": "4",
    }, "The slow end: a 9.8 Hz pulse rate, which is slow enough to hear the "
       "individual pulses rather than a rattle, and four of them to a croak. Croak "
       "Pitch is pulled below the measured 2169 Hz of that row, which is the one "
       "liberty in this preset -- the library has no recording of a true bullfrog, "
       "and its lowest measured resonance is 1529 Hz."),

    ("Cricket Field", {
        "insect_level": "-4", "insect_pitch": "3164", "insect_width": "0.25",
        "trill_rate": "49", "trill_depth": "0.75", "shimmer": "0.6",
        "bed_level": "-18", "bed_tilt": "0.15", "bed_motion": "0.35",
        "distance": "0.3", "width": "0.9",
        "space_amount": "0.25", "space_size": "0.5", "space_damping": "0.7",
        "attack": "800", "release": "2500", "gain": "-7",
    }, "The insect bed on its own, which is the weakest measurement in the plugin "
       "and says so. The library has no recording of insects alone: the carrier and "
       "the trill rate here come from the cricket band *behind* a scops owl and "
       "behind a chorus of frogs, isolated by being narrow, high and steady where "
       "everything else in those files is none of the three. 2.9 to 3.2 kHz, pulsed "
       "at 49 Hz."),

    ("Summer Night", {
        "caller": "Owl", "pitch": "497", "contour": "0.4", "length": "181",
        "calls": "2", "call_rate": "2.85", "phrase_gap": "6.3", "repeats": "1",
        "variation": "0.3", "legato": "0.1",
        "shot_level": "-4", "pack_level": "-16", "pack_rate": "8", "animals": "2",
        "answer": "0.45", "restless": "0.6", "voice": "0.2", "throat": "22",
        "chorus_level": "-14", "croak": "0.69", "croak_pitch": "2016",
        "pulse_rate": "48", "pulses": "18", "croak_length": "373",
        "frogs": "12", "croak_rate": "90", "regularity": "0.95",
        "chorus_spread": "0.7", "chorus_width": "0.9",
        "insect_level": "-14", "insect_pitch": "2982", "trill_rate": "49",
        "trill_depth": "0.7", "shimmer": "0.5",
        "bed_level": "-18", "bed_motion": "0.35",
        "distance": "0.5", "distance_spread": "0.5", "width": "0.8",
        "space_amount": "0.4", "space_size": "0.7", "space_damping": "0.6",
        "attack": "700", "release": "3500", "gain": "4",
    }, "All four layers at once, which is what the plugin is for: a bed measured "
       "from the quiet frames of every reference in the library, a cricket band in "
       "it, twelve frogs on a pond at a 48 Hz pulse rate, and an owl that says "
       "something every eight seconds or so. The balance is the part worth keeping "
       "-- the callers are 4 dB under the chorus and the chorus 4 dB over the bed."),

    ("Midnight Marsh", {
        "caller": "Wolf", "pitch": "455", "contour": "0.5", "length": "1400",
        "calls": "1", "repeats": "1", "variation": "0.4", "legato": "0",
        "shot_level": "-6", "pack_level": "-18", "pack_rate": "4", "animals": "3",
        "pitch_spread": "0.6", "answer": "0.7", "restless": "0.65",
        "voice": "0.26", "throat": "26",
        "chorus_level": "-10", "croak": "0.31", "croak_pitch": "1529",
        "pulse_rate": "13", "pulses": "2", "croak_length": "192",
        "frogs": "18", "croak_rate": "110", "regularity": "0.95",
        "chorus_spread": "0.9", "chorus_width": "1.0",
        "insect_level": "-20", "insect_pitch": "2800", "trill_rate": "33",
        "trill_depth": "0.8", "shimmer": "0.65",
        "bed_level": "-20", "bed_tilt": "-0.2", "bed_motion": "0.4",
        "distance": "0.6", "distance_spread": "0.55", "air": "0.45", "width": "0.85",
        "space_amount": "0.55", "space_size": "0.9", "space_damping": "0.45",
        "attack": "900", "release": "5000", "gain": "9",
    }, "A wet place with something large a long way off. The chorus is the two-pulse "
       "creak of `croaking-frogs.wav` at 13 Hz, eighteen of them; the wolf calls "
       "about once every fifteen seconds and three animals answer each other at "
       "0.7. Everything is far away -- Distance 0.6, Depth 0.55 -- which is what "
       "makes the space rather than the reverb."),

    ("Deep Forest Bed", {
        "bed_level": "-8", "bed_tilt": "-0.3", "bed_motion": "0.5",
        "insect_level": "-22", "insect_pitch": "2600", "insect_width": "0.45",
        "trill_rate": "28", "trill_depth": "0.5", "shimmer": "0.75",
        "distance": "0.55", "width": "0.85",
        "space_amount": "0.45", "space_size": "0.8", "space_damping": "0.75",
        "attack": "1500", "release": "4000", "gain": "1",
    }, "Nothing calls. This is the measured night on its own -- the third-octave "
       "curve of the quietest third of the frames of all 45 references, solved onto "
       "an eight-band filterbank and then corrected against the engine's own "
       "rendered curve until it sat within 3.9 dB rms of it. Bed Tilt pulls it "
       "darker, which is what standing under a canopy does."),
]


def defaults():
    out = subprocess.run([RENDER, "--defaults"], stdout=subprocess.PIPE)
    if out.returncode != 0:
        raise SystemExit("could not run %s --defaults" % RENDER)
    return out.stdout.decode()


def write(text, name, overrides, description):
    lines = []
    for line in text.splitlines():
        if line.startswith("name = "):
            lines.append("name = " + name)
            continue
        if line.startswith("description = "):
            lines.append("description = " + description)
            continue
        if line.startswith("author = "):
            lines.append("author = NightLife")
            continue
        if " = " in line and not line.startswith("#"):
            key = line.split(" = ", 1)[0]
            if key in overrides:
                lines.append("%s = %s" % (key, overrides[key]))
                continue
        lines.append(line)
    fname = name.lower().replace(" ", "_") + ".nightlife"
    path = os.path.join(OUT, fname)
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return fname


def main():
    text = defaults()
    os.makedirs(OUT, exist_ok=True)
    known = set(l.split(" = ", 1)[0] for l in text.splitlines()
                if " = " in l and not l.startswith("#"))
    written = []
    for name, over, desc in PRESETS:
        merged = dict(BASE)
        merged.update(over)
        for k in merged:
            if k not in known:
                raise SystemExit("preset '%s' sets unknown parameter '%s'" % (name, k))
        written.append(write(text, name, merged, desc))
    print("wrote %d presets to %s" % (len(written), os.path.abspath(OUT)))
    for w in sorted(written):
        print("   " + w)
    return 0


if __name__ == "__main__":
    sys.exit(main())
