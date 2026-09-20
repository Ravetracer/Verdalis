"""Writes the factory preset library.

    python3 makepresets.py [--check]

Every preset carries every parameter, because the plugin's preset loader sets
only the keys it finds -- a sparse preset would leave whatever the previous one
had in the parameters it did not mention. So the defaults are read out of
`src/params.cpp` rather than repeated here, and each preset below is a short
list of what it changes.

The base every preset starts from has the flyby, the stridulation and the bed at
-60 dB. A layer that is not deliberately turned on by a preset must be off in
it, or it ends up quietly in all of them.
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "../.."))
PARAMS = os.path.join(ROOT, "src/params.cpp")
OUT = os.path.join(ROOT, "presets")

MACRO = re.compile(
    r'^\s*(LIN|PCT|BIPCT|LOG|STEP|ENUM)\((kParam\w+),\s*"([\w]+)",\s*"([^"]+)",\s*"([^"]+)",\s*',
    re.M)


def enum_names(src, symbol):
    m = re.search(r'const char \*const %s\[\] = \{(.*?)\};' % re.escape(symbol), src, re.S)
    return [x.strip().strip('"') for x in m.group(1).split(",") if x.strip()]


def defaults():
    """(key, display-value) for every parameter, in table order."""
    src = open(PARAMS).read()
    out = []
    for m in MACRO.finditer(src):
        kind, _sym, key, _name, _mod = m.groups()
        rest = src[m.end():]
        # the numeric arguments up to the tip string
        args = rest[:rest.index('"')] if kind != "ENUM" else rest[:rest.index(",")]
        nums = [float(x) for x in re.findall(r'-?\d+\.?\d*(?:e-?\d+)?', args)]
        if kind in ("LIN", "STEP"):
            val = nums[2]
        elif kind in ("PCT", "BIPCT"):
            val = nums[0]
        elif kind == "LOG":
            raw, lo, hi = nums[0], nums[1], nums[2]
            val = lo * (hi / lo) ** raw
        elif kind == "ENUM":
            sym = re.match(r'\s*([\d.]+),\s*(\w+)', rest).group(2)
            idx = int(nums[0])
            val = enum_names(src, sym)[idx]
        out.append((key, val))
    return out


def fmt(v):
    if isinstance(v, str):
        return v
    if abs(v - round(v)) < 1e-9:
        return "%d" % round(v)
    return ("%.4f" % v).rstrip("0").rstrip(".")


# Every layer a preset does not ask for is silent in the base.
BASE = {"flyby_level": -60, "strid_level": -60, "bed_level": -60}

# name, description, features, overrides
PRESETS = [
   ("Honeybee Field",
    "Twelve honeybees working a flower bed. The wingbeat is the library's measured "
    "median of 221 Hz and the spread across the twelve is what puts the layer at the "
    "harmonic-to-noise ratio a hive measures.",
    "bees, swarm, warm",
    {}),

   ("Single Bee",
    "One honeybee, close. At a Count of one the Spread does nothing -- one insect is "
    "one insect -- so this is the measured stack on its own, at the 8 to 13 dB "
    "harmonic-to-noise the library's close single recordings come out at.",
    "bees, solo, close",
    {"count": 1, "rasp": 0.35, "distance": 0.05, "swarm_level": -16, "width": 0.35}),

   ("Hive Wall",
    "Forty-eight bees at the entrance of a hive. Wide enough in rate that the "
    "individual wingbeats stop resolving and the whole thing measures as noise, which "
    "is what a hive is.",
    "bees, hive, dense",
    {"count": 48, "spread": 240, "swarm_level": -11, "distance": 0.1, "width": 0.85,
     "space_amount": 0.18, "rasp": 0.6}),

   ("Beehive Interior",
    "Inside the box. The same dense swarm in a small hard space, with the reverberant "
    "field up and its size down -- the one place in this plugin where a real enclosure "
    "belongs.",
    "bees, hive, indoor",
    {"count": 56, "spread": 260, "swarm_level": -12, "space_amount": 0.42,
     "space_size": 0.18, "space_damping": 0.35, "distance": 0.05, "rasp": 0.65}),

   ("Bumblebee Garden",
    "Four bumblebees, measured at 143 Hz. Its second harmonic sits 11.7 dB above its "
    "fundamental in the references, which is the whole reason a bumblebee reads as "
    "heavier than a honeybee rather than merely lower.",
    "bumblebee, garden, low",
    {"species": "Bumblebee", "count": 4, "spread": 90, "swarm_level": -13,
     "flyby_level": -20, "flyby_rate": 0.35, "distance": 0.12}),

   ("Hornet Close",
    "Two hornets at arm's length. The lowest wingbeat in the library at 85.7 Hz, and "
    "the one species whose fitted resonance sits between its first and second "
    "harmonics -- so the pitch you hear is an octave above the beat.",
    "hornet, close, threat",
    {"species": "Hornet", "count": 2, "spread": 70, "swarm_level": -14, "rasp": 0.62,
     "distance": 0.04, "width": 0.4, "flyby_level": -16, "flyby_rate": 0.5}),

   ("Wasp Picnic",
    "Half a dozen wasps circling, with one passing close every few seconds. The flyby "
    "rises 13.3 dB over its own approach, which is the median of the ten clean passes "
    "in the library.",
    "wasp, flyby, summer",
    {"species": "Wasp", "count": 6, "spread": 120, "swarm_level": -18,
     "flyby_level": -14, "flyby_rate": 0.4, "flyby_pass": 0.9, "distance": 0.18}),

   ("Mosquito In The Ear",
    "One mosquito, as close as it ever gets. The most harmonic thing in the library at "
    "16 dB harmonic-to-noise, which is why a mosquito is a whine where a bee is a buzz.",
    "mosquito, solo, close",
    {"species": "Mosquito", "count": 1, "swarm_level": -18, "rasp": 0.3,
     "distance": 0, "width": 0.2, "space_amount": 0.02, "flutter": 0.4}),

   ("Mosquito Night",
    "A few of them in a dark room, passing at irregular intervals. The passes are what "
    "make it unbearable; the swarm underneath is barely there.",
    "mosquito, flyby, night",
    {"species": "Mosquito", "count": 3, "spread": 110, "swarm_level": -26,
     "flyby_level": -12, "flyby_rate": 0.55, "flyby_pass": 1.1, "flyby_sweep": 0.95,
     "distance": 0.1, "rasp": 0.4}),

   ("Housefly Window",
    "Three flies against glass, measured at 193 Hz with a resonance at 415. Bite up, "
    "because a fly is a papery buzz rather than a round one.",
    "fly, indoor, busy",
    {"species": "Housefly", "count": 3, "spread": 100, "swarm_level": -15,
     "bite": 0.62, "flyby_level": -17, "flyby_rate": 0.6, "flyby_pass": 0.5,
     "space_amount": 0.25, "space_size": 0.2}),

   ("Dragonfly Pond",
    "Four dragonflies. The row the library would not give a tone to: only 8 per cent "
    "of its frames are periodic at all, so this is the clatter of wings it actually "
    "measures as.",
    "dragonfly, water, clatter",
    {"species": "Dragonfly", "count": 4, "spread": 60, "swarm_level": -14,
     "rasp": 0.78, "bite": 0.7, "flutter": 0.45, "distance": 0.22,
     "flyby_level": -18, "flyby_rate": 0.25}),

   ("Cicada Noon",
    "Sixteen cicadas in the heat. Not a wingbeat at all: a tymbal buckling 268 times a "
    "second into a body resonant at 5549 Hz with a Q of 13, which the library measures "
    "as sounding nearly half the time. Scrape is low, at 0.15: a tymbal snaps rib by "
    "rib, so each buckling drives the body briefly -- unlike a cricket, which drags a "
    "scraper across a file and sounds for most of every stroke.",
    "cicada, chorus, heat",
    {"swarm_level": -60, "strid_level": -2, "chorus": 16, "carrier": 5549,
     "carrier_q": 13.2, "pulse_rate": 268.4, "echeme_rate": 12.54, "duty": 0.48,
     "scrape": 0.15,
     "strid_spread": 0.55, "distance": 0.25, "width": 0.9, "space_amount": 0.15}),

   ("Cricket Field",
    "Twelve crickets after dark. Not the same mechanism as a cicada, which is what "
    "0.3.0 changed: twice the Q at 25.8, a seventh of the pulse rate at 35.9, two "
    "thirds of the time silent -- and Scrape at 0.45, because a file-and-scraper drives "
    "the harp for most of each wing stroke where a tymbal only snaps. Measured inside a "
    "chirp the references are sounding 0.27 of the time for a cricket and 0.79 for a "
    "cicada; one click into a resonator is 0.06 and is neither.",
    "cricket, chorus, night",
    {"swarm_level": -60, "strid_level": -3, "chorus": 12, "carrier": 4518,
     "carrier_q": 25.8, "pulse_rate": 35.9, "echeme_rate": 10.51, "duty": 0.33,
     "scrape": 0.45,
     "strid_spread": 0.45, "distance": 0.3, "width": 0.95, "space_amount": 0.12}),

   ("Summer Meadow",
    "Everything at once and nothing in front: bees working the flowers, cicadas in the "
    "trees behind them, and a little of the field itself. The only factory preset that "
    "uses the bed.",
    "mixed, ambience, wide",
    {"count": 20, "spread": 180, "swarm_level": -21, "distance": 0.3,
     "strid_level": -10, "chorus": 10, "carrier": 5200, "carrier_q": 16,
     "pulse_rate": 200, "echeme_rate": 9, "duty": 0.42, "scrape": 0.3,
     "bed_level": -34, "bed_tone": 900, "bed_tilt": -0.35,
     "flyby_level": -22, "flyby_rate": 0.18, "width": 0.95, "space_amount": 0.2}),

   ("Distant Swarm",
    "A swarm across a field. Distance is air absorption and a downward tilt, and at "
    "this range the top of every wingbeat has gone.",
    "bees, distant, soft",
    {"count": 40, "spread": 200, "swarm_level": -12, "distance": 0.82, "air": 0.72,
     "width": 1.0, "space_amount": 0.22, "space_size": 0.85, "rasp": 0.62}),

   ("Swarm Rising",
    "A disturbed swarm, played hard. Velocity To Swarm is at its top, so a soft note "
    "is a calm and thinner swarm and a hard one is every individual flying -- which is "
    "the mapping the whole table is defined at.",
    "bees, dynamic, playable",
    {"count": 44, "spread": 210, "swarm_level": -13, "vel_to_swarm": 1.0,
     "vel_to_level": 0.7, "attack": 900, "release": 2200, "rasp": 0.6,
     "flyby_level": -19, "flyby_rate": 0.45, "distance": 0.14}),
]


def main():
    base = defaults()
    keys = [k for k, _ in base]
    os.makedirs(OUT, exist_ok=True)
    written = []
    for name, desc, features, over in PRESETS:
        unknown = [k for k in over if k not in keys]
        if unknown:
            raise SystemExit("preset '%s' sets unknown parameters: %s" % (name, unknown))
        values = dict(base)
        values.update(BASE)
        values.update(over)
        slug = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")
        path = os.path.join(OUT, slug + ".insectswarm")
        lines = ["# InsectSwarm preset", "format = 1", "name = " + name,
                 "author = Ravetracer", "description = " + desc,
                 "features = " + features, ""]
        lines += ["%s = %s" % (k, fmt(values[k])) for k in keys]
        text = "\n".join(lines) + "\n"
        if "--check" in sys.argv:
            old = open(path).read() if os.path.exists(path) else ""
            if old != text:
                print("would change:", os.path.basename(path))
        else:
            with open(path, "w") as f:
                f.write(text)
        written.append(slug)
    if "--check" not in sys.argv:
        print("wrote %d presets into %s" % (len(written), OUT))
        stale = [f for f in os.listdir(OUT)
                 if f.endswith(".insectswarm") and f[:-len(".insectswarm")] not in written]
        for f in stale:
            os.remove(os.path.join(OUT, f))
            print("removed stale preset:", f)


if __name__ == "__main__":
    main()
