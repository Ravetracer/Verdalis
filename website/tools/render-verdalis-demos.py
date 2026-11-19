#!/usr/bin/env python3
"""
Renders the website demos for the Verdalis plugins as FLAC.

At most ten demos per plugin, chosen below to show each instrument's range.
Every demo comes straight out of the plugin's own offline renderer with the
seed pinned; the only post-step is one linear gain towards -16 LUFS, backed
off so the true peak stays at or below -1 dBTP. No compression, no limiting,
so a sparse preset stays quiet next to a dense one.

The blurb shown on the website comes from <plugin>/presets/demo-descriptions.txt.

WhooshPact's presets are one-shots, so its demos are the family montages from
whooshpact/tools/make-demos.sh rather than one render per preset,
rebuilt here with ffmpeg so sox is not needed.

Output: website/demos/<slug>/NN-Title.flac plus manifest.json per plugin.

Usage:
    website/tools/render-verdalis-demos.py [slug ...]     (default: all)

Needs ffmpeg, and every plugin built in <plugin>/build.
"""
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SUITE = Path(__file__).resolve().parents[2]
OUT = SUITE / "website" / "demos"
SEED = 7
RATE = 48000
TARGET_LUFS = -16.0
MAX_TP = -1.0

# slug -> (folder, display name, seconds held, [preset or (preset, seconds)])
PLUGINS = {
    "rainyday": ("rainyday", "RainyDay", 16, [
        "Steady Rain", "Light Drizzle", "Downpour", "Tin Roof", "Window Pane",
        "Puddle Plinks", "Rain On Leaves", "Inside The Car", "Cave Drips", "Storm Front"]),
    "thunderclap": ("thunderclap", "ThunderClap", 16, [
        ("Summer Storm", 32), "Close Strike", ("Rolling Thunder", 24), "Distant Rumble",
        "Overhead Crack", "Mountain Echoes", "Deep Sub", "City Thunder",
        ("Cloud Crawler", 30), ("Under The Rain", 24)]),
    "shorebreak": ("shorebreak", "ShoreBreak", 16, [
        "Gentle Waves", "Big Waves Crashing", "Rolling Tide", "Crystal Bubbles",
        "Harbour Lapping", "Receding Sand", "Distant Roar", "Hollow Shore",
        "Shallow Clear Water", "Uproar Waves"]),
    "skyhowl": ("skyhowl", "SkyHowl", 16, [
        "Gentle Gusts", "Howling Wind", "Winter Gale", "Rustling Leaves", "Conifer Sough",
        "Cave Mouth", "Open Plain", "Between Houses", "Desert Dune", "Spooky Wind"]),
    "chirpparade": ("chirpparade", "ChirpParade", 16, [
        "Dawn Chorus", "Blackbird Song", "Garden Sparrows", "Cuckoo Valley", "Nightingale Song",
        "Budgie Chatter", "Goose Flight", "Green Woodpecker", ("Forest Morning", 24), "Robin Whistle"]),
    "riverflow": ("riverflow", "RiverFlow", 16, [
        "Bubbling Creek", "Forest Creek", "Mountain River", "Rapids Close", "Big Falls",
        "Small Falls", "Hanging Trickle", "Culvert Drain", "Wild River", "Distant River"]),
    "crackleblaze": ("crackleblaze", "CrackleBlaze", 16, [
        "Camp Fire", "Cottage Hearth", "Bonfire", "Crackle Close", "Wet Wood",
        "Dying Embers", "Log Collapse", "Burning Roof", "Sauna Stove", "Distant Fire"]),
    "insectswarm": ("insectswarm", "InsectSwarm", 16, [
        "Honeybee Field", "Single Bee", "Bumblebee Garden", "Hornet Close", "Housefly Window",
        "Mosquito In The Ear", "Cicada Noon", "Cricket Field", "Summer Meadow", "Swarm Rising"]),
    "nightlife": ("nightlife", "NightLife", 20, [
        "Summer Night", "Pack Answering", "Tawny Wood", "Loon Lake", "Pond Chorus",
        "Spring Peepers", "Midnight Marsh", "Fox Across Fields", "Scops Metronome", "Cricket Field"]),
    "verdaliscene": ("verdaliscene", "VerdaliScene", 40, [
        "Forest River", "Stormy Shore", "Campfire Night", "Rain on the Cabin", "Summer Meadow",
        "Monsoon Jungle", "Winter Cabin", "Wolf Valley", "Dawn Chorus", "Beach Bonfire"]),
}

# WhooshPact: the family montages, with the website text for each.
WHOOSHPACT = [
    ("whooshpact-1-transitions", "Transitions",
     "Five whooshes in a row: a plain sweep, a deep one, a fast pass-by, a slow reveal and a whoosh that lands on a hit."),
    ("whooshpact-2-impacts", "Impacts",
     "Five impacts, from a big cinematic hit with a long low tail to a dull thud and two metal objects colliding."),
    ("whooshpact-3-booms", "Booms",
     "Five booms for the low end: a bottomless drop, a sonar-like boom with a note in it, ground breaking apart and a distant rumble."),
    ("whooshpact-4-braams", "Braams",
     "Five trailer horns: the classic swelling braam, a dark villain version, a wall of brass and a riser that never stops climbing."),
    ("whooshpact-5-downshifters", "Downshifters",
     "Five power-down effects: falling tones, grinding stutters and a retro machine drop for the end of a section."),
    ("whooshpact-6-accents", "Accents",
     "Six short accents: a snap, a heavy door, breaking glass, a jumpscare, a steel stab and a war drum."),
    ("whooshpact-7-variation", "Same Hit, Never Twice",
     "The same war drum eight times identical, then eight times with variation on. No two hits are exactly the same, so a cut full of them never sounds like one sample repeated."),
]


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def measure(wav):
    """Integrated loudness (LUFS) and true peak (dBTP) via ffmpeg's ebur128."""
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", str(wav),
                        "-af", "ebur128=peak=true", "-f", "null", "-"],
                       capture_output=True, text=True)
    summary = r.stderr[r.stderr.rfind("Summary:"):]
    lufs = float(re.search(r"I:\s+(-?[\d.]+|-inf) LUFS", summary).group(1))
    tp = float(re.search(r"Peak:\s+(-?[\d.]+|-inf) dBFS", summary).group(1))
    return lufs, tp


def to_flac(wav, flac, title, artist):
    lufs, tp = measure(wav)
    gain = min(TARGET_LUFS - lufs, MAX_TP - tp)
    flac.parent.mkdir(parents=True, exist_ok=True)
    run(["ffmpeg", "-hide_banner", "-v", "error", "-y", "-i", str(wav),
         "-af", f"volume={gain:.3f}dB", "-ar", str(RATE), "-ac", "2",
         "-c:a", "flac", "-sample_fmt", "s16", "-compression_level", "8",
         "-metadata", f"TITLE={title}", "-metadata", f"ARTIST={artist}",
         "-metadata", "ALBUM=Verdalis demos", str(flac)])
    lufs2, tp2 = measure(flac)
    print(f"    {flac.name:40s} {lufs2:6.1f} LUFS  {tp2:5.1f} dBTP")


def file_name(n, title):
    return f"{n:02d}-{re.sub(r'[^A-Za-z0-9]+', '-', title).strip('-')}.flac"


def descriptions(folder):
    f = SUITE / folder / "presets" / "demo-descriptions.txt"
    out = {}
    for line in f.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            k, v = line.split("=", 1)
            out[k.strip()] = v.strip()
    return out


def render_plugin(slug):
    folder, name, seconds, presets = PLUGINS[slug]
    build = SUITE / folder / "build"
    render, clap = build / f"{folder}-render", build / f"{name}.clap"
    for f in (render, clap):
        if not f.exists():
            sys.exit(f"missing {f} -- build the plugin first")
    blurbs = descriptions(folder)
    out = OUT / slug
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    print(f"==> {name}")
    manifest = []
    with tempfile.TemporaryDirectory() as tmp:
        for n, entry in enumerate(presets, 1):
            preset, secs = entry if isinstance(entry, tuple) else (entry, seconds)
            wav = Path(tmp) / f"{n}.wav"
            run([str(render), "--plugin", str(clap), "--preset", preset, "--out", str(wav),
                 "--seconds", str(secs), "--tail", "3", "--rate", str(RATE),
                 "--param", f"randomseed={SEED}"])
            fn = file_name(n, preset)
            to_flac(wav, out / fn, preset, name)
            if preset not in blurbs:
                print(f"    warning: no demo description for {preset}")
            manifest.append({"file": fn, "title": preset, "preset": preset,
                             "description": blurbs.get(preset, "")})
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")


WP_FAMILIES = {
    "whooshpact-1-transitions": ["transition_simple_whoosh", "transition_deep_whoosh", "transition_passby",
                                 "transition_reveal", "transition_whoosh_hit"],
    "whooshpact-2-impacts": ["impact_deep_impact", "impact_head_on", "impact_low_thud",
                             "impact_metallic_collision", "impact_blast_off"],
    "whooshpact-3-booms": ["boom_bottomless", "boom_deep_sonar", "boom_shattered_earth",
                           "boom_underground_roll", "boom_whisper"],
    "whooshpact-4-braams": ["braam_big_horn", "braam_bad_guy", "braam_brass_wall",
                            "braam_annihilation", "braam_rising_dread"],
    "whooshpact-5-downshifters": ["downshifter_analog_fall", "downshifter_digging_deep", "downshifter_grind_halt",
                                  "downshifter_retro_drop", "downshifter_stutter_scream"],
    "whooshpact-6-accents": ["accent_dry_snap", "accent_gate_closed", "accent_glass_break",
                             "accent_jumpscare", "accent_steel_stab", "accent_war_drum"],
}
WP_GAP, WP_STEP, WP_LEAD = 0.45, 0.8, 0.2
TRIM = "areverse,silenceremove=start_periods=1:start_duration=0.1:start_threshold=-70dB,areverse"


def wp_preset_field(stem, key):
    text = (SUITE / "whooshpact" / "presets" / f"{stem}.whooshpact").read_text(encoding="utf-8")
    return re.search(rf"^{key} = (.*)$", text, re.M).group(1).strip()


def wp_hit(render, clap, stem, seed, dest, *extra):
    secs = round(min(float(wp_preset_field(stem, "span")) * 0.92, 7.5), 2)
    run([str(render), "--plugin", str(clap), "--preset", wp_preset_field(stem, "name"), "--out", str(dest),
         "--seconds", str(secs), "--tail", "2.5", "--rate", str(RATE), "--param", f"randomseed={seed}", *extra])


def ff(inputs, graph, dest):
    cmd = ["ffmpeg", "-hide_banner", "-v", "error", "-y"]
    for i in inputs:
        cmd += ["-i", str(i)]
    run(cmd + ["-filter_complex", graph, "-c:a", "pcm_f32le", str(dest)])


def render_whooshpact():
    """The family montages of whooshpact/tools/make-demos.sh, done with ffmpeg."""
    print("==> WhooshPact")
    build = SUITE / "whooshpact" / "build"
    render, clap = build / "whooshpact-render", build / "WhooshPact.clap"
    out = OUT / "whooshpact"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        wavs = {}
        for name, stems in WP_FAMILIES.items():
            parts = []
            for i, stem in enumerate(stems, 1):
                wp_hit(render, clap, stem, i * 7 + 3, tmp / f"{i}.wav")
                parts.append(tmp / f"{i}.wav")
            chains = "".join(f"[{i}:a]{TRIM},apad=pad_dur={WP_GAP}[p{i}];" for i in range(len(parts)))
            ff(parts, chains + "".join(f"[p{i}]" for i in range(len(parts))) + f"concat=n={len(parts)}:v=0:a=1",
               tmp / f"{name}.wav")
            wavs[name] = tmp / f"{name}.wav"
        # Variation: one render placed eight times (identical), then eight separate renders.
        wp_hit(render, clap, "accent_war_drum", 5, tmp / "a.wav", "--param", "variation=0")
        delays = [int((WP_LEAD + i * WP_STEP) * 1000) for i in range(8)]
        mix = lambda labels: "".join(labels) + "amix=inputs=8:normalize=0:duration=longest"
        ff([tmp / "a.wav"] * 8,
           "".join(f"[{i}:a]adelay={d}:all=1[d{i}];" for i, d in enumerate(delays)) + mix(f"[d{i}]" for i in range(8)),
           tmp / "halfA.wav")
        bs = []
        for i in range(8):
            wp_hit(render, clap, "accent_war_drum", i * 13 + 5, tmp / f"b{i}.wav", "--param", "variation=0.55")
            bs.append(tmp / f"b{i}.wav")
        ff(bs, "".join(f"[{i}:a]adelay={d}:all=1[d{i}];" for i, d in enumerate(delays)) + mix(f"[d{i}]" for i in range(8)),
           tmp / "halfB.wav")
        ff([tmp / "halfA.wav", tmp / "halfB.wav"],
           f"[0:a]{TRIM},apad=pad_dur=1.2[a];[a][1:a]concat=n=2:v=0:a=1", tmp / "whooshpact-7-variation.wav")
        wavs["whooshpact-7-variation"] = tmp / "whooshpact-7-variation.wav"

        manifest = []
        for n, (stem, title, text) in enumerate(WHOOSHPACT, 1):
            fn = file_name(n, title)
            to_flac(wavs[stem], out / fn, title, "WhooshPact")
            manifest.append({"file": fn, "title": title, "preset": stem, "description": text})
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")


def main():
    for t in ("ffmpeg",):
        if not shutil.which(t):
            sys.exit(f"{t} is required")
    slugs = sys.argv[1:] or [*PLUGINS, "whooshpact"]
    for s in slugs:
        if s == "whooshpact":
            render_whooshpact()
        elif s in PLUGINS:
            render_plugin(s)
        else:
            sys.exit(f"unknown plugin: {s}")


if __name__ == "__main__":
    main()
