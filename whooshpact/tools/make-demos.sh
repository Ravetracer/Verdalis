#!/bin/bash
#
# Renders the website demo audio for WhooshPact into DemoTracks/WhooshPact/.
#
# Everything here comes out of the shipping factory presets through the plugin's
# own offline renderer. There is no editing, no processing and no layering: the
# only post-steps are trimming the silence off the end of each hit, mixing the
# sequenced hits at a fixed spacing, and normalising each finished file to
# -1 dBFS. The seeds are fixed and sox's dither is off (-D) on the normalising
# pass, so a re-run reproduces the files byte for byte.
#
# Needs sox and lame. Run it from anywhere:
#
#    whooshpact/tools/make-demos.sh [build-dir]
#
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
plugin_dir="$(dirname "$here")"
suite_dir="$(dirname "$plugin_dir")"
build_dir="${1:-${plugin_dir}/build}"

render="${build_dir}/whooshpact-render"
clap="${build_dir}/WhooshPact.clap"
out="${suite_dir}/DemoTracks/WhooshPact"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

for f in "$render" "$clap"; do
   [ -x "$f" ] || [ -f "$f" ] || { echo "missing: $f -- build the plugin first" >&2; exit 1; }
done
command -v sox  >/dev/null || { echo "sox is required" >&2; exit 1; }
command -v lame >/dev/null || { echo "lame is required" >&2; exit 1; }

mkdir -p "$out"

gap=0.45        # between presets in the family demos
step=0.8        # between hits in the sequenced variation demo
lead=0.2

# preset_name <preset-file-stem>
preset_name() { sed -n 's/^name = //p' "${plugin_dir}/presets/$1.whooshpact" | head -1; }
preset_span() { sed -n 's/^span = //p' "${plugin_dir}/presets/$1.whooshpact" | head -1 | tr -d ' '; }

# hit <stem> <seed> <dest> [extra render args...]
hit() {
   local stem="$1" seed="$2" dest="$3"; shift 3
   local span secs
   span="$(preset_span "$stem")"
   secs="$(python3 -c "print(round(min(float('$span')*0.92, 7.5), 2))")"
   "$render" --plugin "$clap" --preset "$(preset_name "$stem")" --out "$dest" \
             --seconds "$secs" --tail 2.5 --rate 48000 --param randomseed="$seed" \
             "$@" >/dev/null
}

# family <outname> <preset-stem>...
family() {
   local name="$1"; shift
   local i=0 parts=()
   for stem in "$@"; do
      i=$((i + 1))
      hit "$stem" $((i * 7 + 3)) "$work/${i}.wav"
      # trim the silent tail, then space the next one off it
      sox -D "$work/${i}.wav" "$work/${i}t.wav" reverse silence 1 0.1 -70d reverse
      sox -D "$work/${i}t.wav" "$work/${i}g.wav" pad 0 "$gap"
      parts+=("$work/${i}g.wav")
   done
   sox -D "${parts[@]}" "$out/${name}.wav" gain -n -1
   rm -f "$work"/*.wav
   echo "  ${name}.wav"
}

echo "family demos:"
family whooshpact-1-transitions \
   transition_simple_whoosh transition_deep_whoosh transition_passby \
   transition_reveal transition_whoosh_hit
family whooshpact-2-impacts \
   impact_deep_impact impact_head_on impact_low_thud \
   impact_metallic_collision impact_blast_off
family whooshpact-3-booms \
   boom_bottomless boom_deep_sonar boom_shattered_earth \
   boom_underground_roll boom_whisper
family whooshpact-4-braams \
   braam_big_horn braam_bad_guy braam_brass_wall \
   braam_annihilation braam_rising_dread
family whooshpact-5-downshifters \
   downshifter_analog_fall downshifter_digging_deep downshifter_grind_halt \
   downshifter_retro_drop downshifter_stutter_scream
family whooshpact-6-accents \
   accent_dry_snap accent_gate_closed accent_glass_break \
   accent_jumpscare accent_steel_stab accent_war_drum

# The variation demo: one preset struck eight times, twice over.
#
# The first half is deliberately ONE render padded eight times, so those hits are
# the identical waveform rather than merely similar -- that is the point being
# made, and rendering each one separately would not make it even at Variation 0,
# because each voice still draws its own noise seed.
echo "variation demo:"
hit accent_war_drum 5 "$work/a.wav" --param variation=0
parts=()
for i in $(seq 0 7); do
   sox -D "$work/a.wav" "$work/a${i}.wav" pad "$(python3 -c "print(round($lead + $i * $step, 3))")" 0
   parts+=("$work/a${i}.wav")
done
sox -D --combine mix "${parts[@]}" "$work/halfA.wav"

parts=()
for i in $(seq 0 7); do
   hit accent_war_drum $((i * 13 + 5)) "$work/b${i}.wav" --param variation=0.55
   sox -D "$work/b${i}.wav" "$work/b${i}o.wav" pad "$(python3 -c "print(round($lead + $i * $step, 3))")" 0
   parts+=("$work/b${i}o.wav")
done
sox -D --combine mix "${parts[@]}" "$work/halfB.wav"

sox -D "$work/halfA.wav" "$work/halfAt.wav" reverse silence 1 0.1 -70d reverse
sox -D "$work/halfAt.wav" "$work/halfAg.wav" pad 0 1.2
sox -D "$work/halfAg.wav" "$work/halfB.wav" "$out/whooshpact-7-variation.wav" gain -n -1
echo "  whooshpact-7-variation.wav"

echo "mp3:"
for f in "$out"/whooshpact-*.wav; do
   lame --quiet -b 256 --cbr "$f" "${f%.wav}.mp3"
done
echo "  $(ls "$out"/*.mp3 | wc -l) files at 256 kbps CBR"
echo
echo "written to $out"
