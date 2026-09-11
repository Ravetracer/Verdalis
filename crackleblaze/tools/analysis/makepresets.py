"""Writes the factory preset library.

    python3 makepresets.py [--check]

A preset file has to carry *every* parameter. Loading one only writes the keys
it contains, so a preset that leaves a key out inherits whatever the previously
loaded preset set it to -- which makes the library's behaviour depend on the
order the user clicked through it. So the defaults come from `src/params.cpp`,
parsed here, and each preset below is written as those defaults plus its own
overrides. Adding a parameter to the table therefore adds it to all eighteen
presets at their default, rather than silently leaving eighteen holes.

Values are written in the units the preset format uses, which are the units
`paramToReal` produces: real for Log (Hz, ms, /s) and Linear (dB), 0..1 for
Percent, the name for Enum.

`--check` rewrites into a temporary directory and reports differences instead of
writing, for use after a parameter table change.
"""
import sys, os, re, math

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "../.."))
PARAMS = os.path.join(ROOT, "src/params.cpp")
OUT = os.path.join(ROOT, "presets")
EXT = "crackleblaze"


def enum_tables(src):
    tabs = {}
    for m in re.finditer(r'const char \*const (\w+)\[\] = \{(.*?)\};', src, re.S):
        tabs[m.group(1)] = re.findall(r'"([^"]*)"', m.group(2))
    return tabs


def defaults():
    """key -> (value as written in a preset file, order index)."""
    src = open(PARAMS).read()
    tabs = enum_tables(src)
    out, order = {}, []
    pat = re.compile(r'\b(LIN|PCT|BIPCT|LOG|STEP|ENUM)\((\w+),\s*"([^"]+)",\s*"([^"]*)",\s*"([^"]*)",\s*([^,]+),(.*)')
    for line in src.splitlines():
        m = pat.search(line)
        if not m:
            continue
        kind, _, key, _, _, a, rest = m.groups()
        a = a.strip()
        if kind in ("PCT", "BIPCT"):
            v = float(a)
        elif kind == "ENUM":
            names = tabs[rest.split(",")[0].strip()]
            v = names[int(round(float(a)))]
        elif kind == "LOG":
            dlo, dhi = (float(x) for x in rest.split(",")[:2])
            v = dlo * (dhi / dlo) ** float(a)
        elif kind in ("LIN", "STEP"):
            # lo, hi, def
            nums = [float(x) for x in rest.split(",")[:2]]
            v = nums[1]
            a_lo = float(a)
            _ = a_lo
        out[key] = v
        order.append(key)
    return out, order


def fmt(v):
    if isinstance(v, str):
        return v
    if abs(v - round(v)) < 1e-9 and abs(v) < 1e9:
        return str(int(round(v)))
    return ("%.4f" % v).rstrip("0").rstrip(".")


# -------------------------------------------------------------- the library
#
# Every preset is the default patch plus what is listed here. The default patch
# is itself a measurement -- it renders at a crest factor of 29.8 dB against the
# library's median of 31.7, 31.3 crackles a second against 29, and a Fano factor
# of 3.55 against 3.90 -- so a preset that overrides little is not unfinished.
PRESETS = [
 ("camp_fire", "Camp Fire", "camp, outdoor, default",
  "The default patch, and the one the engine was fitted at. A camp fire in the open "
  "at arm's length: the measured Camp Fire bed, a fire ring with almost nothing to "
  "reflect off, and crackles standing the library's median 13 dB over the roar.",
  {"fire_type": "Camp Fire", "hearth_type": "Fire Ring", "space_amount": 0.18,
   "distance": 0.10}),

 ("cottage_hearth", "Cottage Hearth", "hearth, indoor, cosy",
  "A small fire in a cottage fireplace. Log Fire is the library's commonest bed shape "
  "and this is what it was named for: a strong 125 Hz, a scooped middle, and the top "
  "coming back for the crackles.",
  {"fire_type": "Log Fire", "hearth_type": "Fireplace", "roar_level": -30,
   "crackle_rate": 22, "space_amount": 0.32, "space_size": 0.35, "sap": 0.24}),

 ("open_fireplace", "Open Fireplace", "fireplace, reverberant, close",
  "Fitted against `fire-in-fireplace_close-up_reverberant`, the most reverberant pair "
  "in the library: the microphone inside the opening, so the early field is enormous "
  "and the 125 Hz band is 10 dB down on everything above it.",
  {"fire_type": "Open Flame", "hearth_type": "Fireplace", "distance": 0.0,
   "space_amount": 0.45, "space_size": 0.30, "space_damping": 0.70,
   "crackle_level": -14, "roar_width": 0.30}),

 ("wood_stove", "Wood Stove", "stove, draught, bright",
  "The lid open on a wood stove. Stove Draught is the one shape in the library with "
  "no members but itself -- 22 dB down at 1 kHz and peaking at 16 -- and the draught "
  "pulling air through it is most of what is heard.",
  {"fire_type": "Stove Draught", "hearth_type": "Stove", "draught": 0.55,
   "crackle_tone": 7000, "sizzle_tone": 7000, "crackle_body": 0.12,
   "space_amount": 0.30, "roar_level": -28}),

 ("big_blaze", "Big Blaze", "large, deep, outdoor",
  "A large open fire a few metres back. Deep Blaze carries the most low end of the "
  "five measured beds and the dullest top, which is what a big fire heard from a "
  "distance actually is: the crackles that survive the air are the loud ones.",
  {"fire_type": "Deep Blaze", "hearth_type": "Open", "roar_level": -27,
   "roar_body": 0.68, "distance": 0.35, "crackle_rate": 40, "crackle_spread": 7.5,
   "settle_rate": 0.25, "width": 0.85}),

 ("burning_roof", "Burning Roof", "large, structure, roaring",
  "A building alight: flames stacked metres high, heard from across the street. The "
  "roar is most of it, the crackle rate is at the top of the measured range, and the "
  "settles are frequent because a structure fire is continually collapsing.",
  {"fire_type": "Deep Blaze", "fire_blend": 0.45, "hearth_type": "Open",
   "roar_level": -24, "roar_body": 0.75, "flare": 0.72, "flare_rate": 0.35,
   "distance": 0.45, "crackle_rate": 55, "crackle_spread": 8.5, "burst": 2.6,
   "settle_rate": 0.6, "settle_level": -20, "settle_tone": 105, "width": 0.95,
   "gain": -5}),

 ("bonfire", "Bonfire", "bonfire, outdoor, wide",
  "A bonfire at ten paces, wide across the field of view. Camp Fire's bed with more "
  "body under it and a rate near the top of the library.",
  {"fire_type": "Camp Fire", "hearth_type": "Open", "roar_level": -27,
   "roar_body": 0.65, "crackle_rate": 44, "distance": 0.22, "width": 0.92,
   "roar_width": 0.6, "settle_rate": 0.2, "flare": 0.65}),

 ("dying_embers", "Dying Embers", "embers, quiet, sparse",
  "Almost out. The roar has gone, the sharp ticks with it, and what is left is the "
  "occasional sizzle out of a hot log -- Sap near the top of its measured range, "
  "because what is still evaporating is all that is still making sound.",
  {"fire_type": "Log Fire", "hearth_type": "Stone Hearth", "roar_level": -42,
   "draught": 0.05, "crackle_rate": 3.5, "crackle_level": -20, "sap": 0.55,
   "sizzle_decay": 34, "sizzle_level": -21, "settle_rate": 0.04,
   "flare": 0.35, "crackle_tone": 2600, "space_amount": 0.30}),

 ("wet_wood", "Wet Wood", "sizzle, steam, damp",
  "Wood that has not dried. Sap at the top of the measured range: 57 per cent of the "
  "events are sizzles rather than ticks, which is `fire_near_open_close` -- the "
  "wettest recording in the library.",
  {"fire_type": "Log Fire", "hearth_type": "Fireplace", "sap": 0.57,
   "steam": 0.55, "sizzle_decay": 26, "sizzle_level": -20, "crackle_rate": 24,
   "space_amount": 0.32}),

 ("dry_kindling", "Dry Kindling", "kindling, sharp, fast",
  "Dry sticks catching. Sap at the bottom of the measured range, the rate at the top, "
  "and Snap as far up as it goes -- these are ticks with nothing behind them.",
  {"fire_type": "Open Flame", "hearth_type": "Fire Ring", "sap": 0.04,
   "crackle_rate": 48, "crackle_decay": 1.2, "snap": 0.92, "crackle_tone": 5500,
   "crackle_body": 0.18, "crackle_level": -15, "burst": 2.4, "roar_level": -34,
   "settle_rate": 0.02, "distance": 0.05}),

 ("distant_fire", "Distant Fire", "distant, soft, air",
  "The same fire, across a field. Distance multiplies the events and divides them, "
  "smears their edges and takes the top off with the air they crossed -- so a fire "
  "far enough away stops being countable ticks and becomes a texture.",
  {"fire_type": "Deep Blaze", "hearth_type": "Open", "distance": 0.75,
   "air": 0.35, "roar_level": -30, "crackle_rate": 26, "width": 0.9,
   "space_amount": 0.15}),

 ("hearth_glow", "Hearth Glow", "hearth, quiet, warm",
  "A settled fire in a stone hearth, heard from a chair across the room. Quiet, slow "
  "and low: the bed does most of the work and the crackles are punctuation.",
  {"fire_type": "Log Fire", "hearth_type": "Stone Hearth", "roar_level": -33,
   "crackle_rate": 12, "crackle_level": -19, "flare": 0.42, "flare_rate": 0.30,
   "distance": 0.28, "space_amount": 0.34, "space_size": 0.45, "sap": 0.28}),

 ("sauna_stove", "Sauna Stove", "stove, enclosed, deep",
  "A sauna stove: an enclosed firebox with a deep body and a hard, bright early "
  "field off the metal and the stones.",
  {"fire_type": "Log Fire", "fire_blend": 0.35, "hearth_type": "Stove",
   "roar_body": 0.66, "draught": 0.35, "space_amount": 0.40, "space_size": 0.22,
   "space_damping": 0.40, "crackle_rate": 20, "crackle_tone": 5000}),

 ("forest_campfire", "Forest Campfire", "camp, forest, night",
  "A camp fire among trees at night. A little more space than the open, because a "
  "forest does return something -- but nothing like a room.",
  {"fire_type": "Camp Fire", "hearth_type": "Fire Ring", "space_amount": 0.26,
   "space_size": 0.62, "space_damping": 0.72, "crackle_rate": 24, "sap": 0.26,
   "width": 0.82, "distance": 0.18}),

 ("log_collapse", "Log Collapse", "settle, thumps, shifting",
  "A fire that keeps giving way. The settle layer is rare in the library -- a median "
  "of six a minute -- and this is the one preset that puts it where you cannot miss "
  "it, at the top of the measured range.",
  {"fire_type": "Log Fire", "hearth_type": "Fireplace", "settle_rate": 1.4,
   "settle_level": -18, "settle_tone": 120, "settle_decay": 14,
   "crackle_rate": 18, "space_amount": 0.30, "roar_body": 0.62}),

 ("furnace_roar", "Furnace Roar", "furnace, roar, cavern",
  "Almost all bed. A furnace in a vaulted space, where the roar and its early field "
  "swamp the individual crackles -- the far end of the plugin's range from Dry "
  "Kindling.",
  {"fire_type": "Deep Blaze", "hearth_type": "Cavern", "roar_level": -23,
   "roar_body": 0.80, "draught": 0.45, "flare": 0.68, "flare_rate": 0.22,
   "crackle_rate": 14, "crackle_level": -26, "sizzle_level": -30,
   "space_amount": 0.48, "space_size": 0.75, "gain": -6}),

 ("crackle_close", "Crackle Close", "close, detailed, dry",
  "The microphone on top of the fire. No distance at all, so nothing is smeared and "
  "the crackles stand at the top of the measured prominence range -- 20 dB over the "
  "roar, which is `fire_near` and `fireplace-at-castle-mountain-bungalows`.",
  {"fire_type": "Open Flame", "hearth_type": "Open", "distance": 0.0,
   "roar_level": -35, "crackle_level": -13, "crackle_rate": 34,
   "crackle_spread": 8.0, "snap": 0.80, "space_amount": 0.08, "width": 0.55}),

 ("slow_burn", "Slow Burn", "slow, sparse, long",
  "A fire barely being fed. Everything slow: a low rate, a long flare and sizzles "
  "held near the top of their measured decay.",
  {"fire_type": "Log Fire", "hearth_type": "Stone Hearth", "crackle_rate": 7,
   "flare": 0.60, "flare_rate": 0.18, "sap": 0.40, "sizzle_decay": 30,
   "roar_level": -34, "settle_rate": 0.06, "space_amount": 0.28}),
]


# Per-preset output gain, in dB, and every one of these is measured rather than
# judged. Each preset was rendered at a common -24 dB with the seed pinned, its
# true peak read off, and its gain set to whatever puts that peak at -3 dBFS --
# then capped at -3 dB, so a preset that is *meant* to be quiet stays quiet
# instead of being normalised up to meet the others.
#
# The cap matters more here than in the suite's other plugins. A fire's
# amplitudes are log-normal with a measured 6 dB spread, so its loudest crackle
# in half a minute stands 18 dB over the median one; without this, half the
# library rendered into the output stage's soft clipper, and a clipped crackle
# is a crackle with its transient taken off -- which is the one thing this
# plugin must not do.
#
# Regenerate with tools/analysis/headroom.sh after changing any level.
GAIN = {
    "big_blaze": -3.0, "bonfire": -3.0, "burning_roof": -3.0, "camp_fire": -5.0,
    "cottage_hearth": -3.0, "crackle_close": -13.0, "distant_fire": -3.0,
    "dry_kindling": -4.0, "dying_embers": -3.0, "forest_campfire": -3.5,
    "furnace_roar": -3.0, "hearth_glow": -3.0, "log_collapse": -3.0,
    "open_fireplace": -6.5, "sauna_stove": -3.0, "slow_burn": -3.0,
    "wet_wood": -3.0, "wood_stove": -3.0,
}


def build():
    dflt, order = defaults()
    files = {}
    for slug, name, features, desc, over in PRESETS:
        for k in over:
            if k not in dflt:
                raise SystemExit("preset %s sets unknown key '%s'" % (slug, k))
        vals = dict(dflt)
        vals.update(over)
        vals["gain"] = GAIN[slug]
        lines = ["# CrackleBlaze preset", "format = 1", "name = " + name,
                 "author = Ravetracer", "description = " + desc,
                 "features = " + features, ""]
        for k in order:
            lines.append("%s = %s" % (k, fmt(vals[k])))
        files[slug + "." + EXT] = "\n".join(lines) + "\n"
    return files


def main():
    files = build()
    if "--check" in sys.argv:
        bad = 0
        for fn, body in files.items():
            p = os.path.join(OUT, fn)
            cur = open(p).read() if os.path.exists(p) else None
            if cur != body:
                print("differs:", fn)
                bad += 1
        print("%d of %d presets differ" % (bad, len(files)))
        return 1 if bad else 0
    os.makedirs(OUT, exist_ok=True)
    for fn, body in files.items():
        open(os.path.join(OUT, fn), "w").write(body)
    print("wrote %d presets into %s" % (len(files), OUT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
