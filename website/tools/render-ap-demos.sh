#!/usr/bin/env bash
#
# Renders the website demo audio for the six plugins in the audio-plugins
# repository (SaeureKiste, RumpelKiste, Aurum, Substrike, Shapeau, ReSIDent)
# into website/demos/<slug>/ as FLAC, with a manifest.json per plugin.
#
#   website/tools/render-ap-demos.sh <audio-plugins-dir> <build-dir> [slug...]
#
#   <audio-plugins-dir>  the audio-plugins checkout. It is only read: every
#                        plugin is configured and built out of tree in
#                        <build-dir>/<folder>, nothing is installed.
#   <build-dir>          scratch space for the builds, the two helper tools
#                        (ap-fxhost, aurum-preset-dump) and the intermediate
#                        WAVs (<build-dir>/demo-work).
#   slug...              any of saeurekiste rumpelkiste aurum substrike shapeau
#                        resident (default: all, in that order -- aurum and
#                        shapeau need the sources rendered by the others, which
#                        the script renders itself either way).
#
# Every sound is one of the six plugins; nothing recorded goes in. Aurum and
# Shapeau are effects, so their input is a groove rendered by RumpelKiste,
# SaeureKiste and ReSIDent, played through them by ap-fxhost (a minimal
# offline CLAP host in this folder), because neither plugin's own renderer
# takes input audio.
#
# Post-processing is the same for every file and nothing else: the trailing
# part more than 66 dB below the file's peak is cut with a 0.25 s fade, then
# one linear gain towards -16 LUFS integrated, held back so the true peak stays
# at or under -1 dBTP. No compression, no limiting. FLAC, 48 kHz, 16 bit,
# stereo, with TITLE and ARTIST tags.
#
# ReSIDent needs the C64 programs and ROMs in <audio-plugins>/resident/_dev_
# (developer-local, never shipped); RESIDENT_DEMO_ROMS overrides the ROM folder.
#
# Needs cmake, ninja, g++, ffmpeg (with flac) and python3.
set -euo pipefail
export LC_ALL=C

[ $# -ge 2 ] || { sed -n '2,36p' "$0" >&2; exit 2; }
AP="$(cd "$1" && pwd)"
BUILD="$(mkdir -p "$2" && cd "$2" && pwd)"
shift 2
SLUGS=("$@")
[ ${#SLUGS[@]} -gt 0 ] || SLUGS=(saeurekiste rumpelkiste aurum substrike shapeau resident)

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTROOT="$(cd "${HERE}/.." && pwd)/demos"
WORK="${BUILD}/demo-work"
SRC="${WORK}/sources"
mkdir -p "$WORK" "$SRC"
for t in cmake ninja g++ ffmpeg python3; do
   command -v "$t" >/dev/null || { echo "$t is required" >&2; exit 1; }
done

# ------------------------------------------------------------------ building

build_plugin() { # <folder> <CMAKE_PREFIX>
   local d="$1" p="$2"
   [ -f "${BUILD}/${d}/build.ninja" ] ||
      cmake -S "${AP}/${d}" -B "${BUILD}/${d}" -G Ninja -DCMAKE_BUILD_TYPE=Release \
         -D"${p}"_BUILD_VST3=OFF -D"${p}"_BUILD_AUV2=OFF >"${BUILD}/${d}.cfg.log"
   cmake --build "${BUILD}/${d}" >"${BUILD}/${d}.build.log"
}

echo "==> building the plugins out of tree in ${BUILD}"
build_plugin saeure-kiste SAEUREKISTE
build_plugin rumpel-kiste RUMPELKISTE
build_plugin substrike SUBSTRIKE
build_plugin shapeau SHAPEAU
build_plugin aurum AURUM
build_plugin resident RESIDENT

g++ -std=c++17 -O2 -I"${AP}/CLAP/clap/include" "${HERE}/ap-fxhost.cpp" -o "${BUILD}/ap-fxhost" -ldl
g++ -std=c++20 -O1 -I"${AP}/aurum/src" "${HERE}/aurum-preset-dump.cpp" \
   "${AP}/aurum/src/state/FactoryPresets.cpp" "${AP}/aurum/src/state/StateIO.cpp" \
   "${AP}/aurum/src/plugin/Params.cpp" -o "${BUILD}/aurum-preset-dump"

SK_RENDER="${BUILD}/saeure-kiste/saeurekiste-render"
SK_CLAP="${BUILD}/saeure-kiste/SaeureKiste.clap"
RK_RENDER="${BUILD}/rumpel-kiste/rumpelkiste-render"
RK_CLAP="${BUILD}/rumpel-kiste/RumpelKiste.clap"
SS_RENDER="${BUILD}/substrike/substrike-render"
SS_CLAP="${BUILD}/substrike/Substrike.clap"
SH_CLAP="${BUILD}/shapeau/Shapeau.clap"
AU_CLAP="${BUILD}/aurum/Aurum.clap"
RS_RUN="${BUILD}/resident/resident-run"
FXHOST="${BUILD}/ap-fxhost"

# ------------------------------------------------------------ post-processing

# Integrated loudness and true peak of a file: "<LUFS> <dBTP>".
measure() {
   ffmpeg -nostats -hide_banner -i "$1" -filter_complex ebur128=peak=true -f null - 2>&1 |
      awk '/Integrated loudness:/{s=1} s&&/^ *I: /{i=$2} /True peak:/{p=1} p&&/^ *Peak: /{t=$2} END{print i, t}'
}

# Where the file has decayed for good: the end of the last 10 ms window whose
# peak is within 66 dB of the file's peak, plus 0.25 s for the fade.
audible_end() {
   ffmpeg -nostats -hide_banner -loglevel error -i "$1" -ac 1 -f f32le - | python3 -c '
import sys, array
d = array.array("f"); d.frombytes(sys.stdin.buffer.read())
n = 480
peaks = [max(map(abs, d[i:i+n]), default=0.0) for i in range(0, len(d), n)]
top = max(peaks) or 1.0
last = max((k for k, p in enumerate(peaks) if p > top * 10 ** (-66 / 20)), default=0)
print("%.3f" % min(len(d) / 48000.0, (last + 1) * n / 48000.0 + 0.25))'
}

# finish <in.wav> <slug> <NN-Short-Title> <title> <artist> <preset> <facts>
finish() {
   local in="$1" slug="$2" base="$3" title="$4" artist="$5" preset="$6" facts="$7"
   local out="${OUTROOT}/${slug}/${base}.flac"
   mkdir -p "${OUTROOT}/${slug}"
   local end fadeat trimmed="${WORK}/${slug}-${base}-trim.wav"
   end="$(audible_end "$in")"
   fadeat="$(python3 -c "print(max(0.0, ${end} - 0.25))")"
   ffmpeg -nostats -hide_banner -loglevel error -y -i "$in" \
      -af "atrim=end=${end},afade=t=out:st=${fadeat}:d=0.25" -ac 2 -c:a pcm_f32le "$trimmed"
   local lufs tp gain
   read -r lufs tp <<<"$(measure "$trimmed")"
   gain="$(python3 -c "print(round(min(-16.0 - (${lufs}), -1.0 - (${tp})), 2))")"
   ffmpeg -nostats -hide_banner -loglevel error -y -i "$trimmed" -af "volume=${gain}dB" \
      -ar 48000 -ac 2 -sample_fmt s16 -c:a flac -compression_level 8 \
      -metadata TITLE="$title" -metadata ARTIST="$artist" "$out"
   local flufs ftp
   read -r flufs ftp <<<"$(measure "$out")"
   printf '    %-34s %6.2f s  gain %+6.2f dB  -> %6s LUFS  %5s dBTP\n' \
      "${base}.flac" "$end" "$gain" "$flufs" "$ftp"
   printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "${base}.flac" "$title" "$preset" "$facts" \
      "$flufs" "$ftp" "$end" >>"${WORK}/${slug}.tsv"
}

begin() { # <slug>: clear the plugin's output folder of earlier FLACs and its manifest list
   mkdir -p "${OUTROOT}/$1"
   rm -f "${OUTROOT}/$1"/*.flac "${WORK}/$1.tsv"
}

manifest() { # <slug>
   python3 - "${WORK}/$1.tsv" "${OUTROOT}/$1/manifest.json" <<'PY'
import json, sys
rows = [l.rstrip("\n").split("\t") for l in open(sys.argv[1], encoding="utf-8") if l.strip()]
data = [{"file": r[0], "title": r[1], "preset": r[2], "facts": r[3]} for r in rows]
with open(sys.argv[2], "w", encoding="utf-8") as fh:
    json.dump(data, fh, indent=2, ensure_ascii=False)
    fh.write("\n")
PY
}

# Scales a file to a loudness (one linear gain), for mixing sources.
to_lufs() { # <in> <lufs> <out>
   local lufs tp
   read -r lufs tp <<<"$(measure "$1")"
   ffmpeg -nostats -hide_banner -loglevel error -y -i "$1" -af "volume=$(python3 -c "print(round($2 - (${lufs}), 2))")dB" \
      -ac 2 -c:a pcm_f32le "$3"
}

# ------------------------------------------------------------------ SaeureKiste

# Every take uses --demo-moves: the cutoff sweeps down about 1.5 octaves, up to
# an octave above the preset's value and settles a little under it; resonance
# rises by a quarter of its travel; drive leans in by a third over the second
# half. All relative to the preset. --seconds 16 is rounded up to whole passes
# of the pattern by the renderer.
#
# The renderer keeps the host transport playing through the tail, so the
# sequencer would run on and the file would end mid-note. So each take is
# rendered twice: the first pass only tells how long the whole passes are (its
# length minus the tail), and the second switches Mode to MIDI at exactly that
# moment with --move, which stops the sequencer at the end of the last pass and
# lets the last note (and any delay repeats) decay naturally in the 3 s tail.
sk_seq_stop() { # <wav> <tail> -> the held length in seconds
   python3 -c "print('%.4f' % ($(ffprobe -v error -show_entries format=duration -of csv=p=0 "$1") - $2))"
}
sk_take() { # <out.wav> <preset> <bpm> [extra renderer args...]
   local out="$1" preset="$2" bpm="$3" stop; shift 3
   "$SK_RENDER" --plugin "$SK_CLAP" --preset "$preset" --seconds 16 --tail 3 --rate 48000 \
      --bpm "$bpm" --demo-moves "$@" --out "$out" >/dev/null
   stop="$(sk_seq_stop "$out" 3)"
   "$SK_RENDER" --plugin "$SK_CLAP" --preset "$preset" --seconds 16 --tail 3 --rate 48000 \
      --bpm "$bpm" --demo-moves "$@" --move "Mode=Sequencer..MIDI@${stop}:$(python3 -c "print(${stop} + 0.001)")" \
      --out "$out" >/dev/null
}

do_saeurekiste() {
   echo "==> SaeureKiste"
   local s=saeurekiste a="SäureKiste" w="${WORK}/sk"
   local moves="Played by its own sequencer, with the cutoff swept down and up, the resonance opened a quarter of its travel and the drive pushed over the second half (relative to the preset)."
   mkdir -p "$w"; begin $s
   sk_take "$w/01.wav" "Classic Squelch" 130 --param Mode=Sequencer --seq
   finish "$w/01.wav" $s 01-Classic-Squelch "Classic Squelch" "$a" "Classic Squelch" \
      "Factory preset Classic Squelch at 130 BPM: high resonance and a short decay on a sawtooth line in C minor. ${moves}"
   sk_take "$w/02.wav" "Machine Running" 130
   finish "$w/02.wav" $s 02-Machine-Running "Machine Running" "$a" "Machine Running" \
      "Factory preset Machine Running at 130 BPM, the stock drive stage (Soft Clip). ${moves}"
   sk_take "$w/03.wav" "Basement" 126
   finish "$w/03.wav" $s 03-Basement "Basement" "$a" "Basement" \
      "Factory preset Basement at 126 BPM: the square wave an octave down with the filter almost shut and a long decay. ${moves}"
   sk_take "$w/04.wav" "Daylight" 124
   finish "$w/04.wav" $s 04-Daylight "Daylight" "$a" "Daylight" \
      "Factory preset Daylight at 124 BPM: a major pentatonic line, short and bright, with light swing (56 %). ${moves}"
   sk_take "$w/05.wav" "Homesick" 118
   finish "$w/05.wav" $s 05-Homesick "Homesick" "$a" "Homesick" \
      "Factory preset Homesick at 118 BPM: a natural minor line in eighth notes with a long decay and long gates. ${moves}"
   sk_take "$w/06.wav" "Swung Line" 126
   finish "$w/06.wav" $s 06-Swung-Line "Swung Line" "$a" "Swung Line" \
      "Factory preset Swung Line at 126 BPM: swing at 66.7 %, every second sixteenth on the triplet. ${moves}"
   sk_take "$w/07.wav" "Stompbox" 132
   finish "$w/07.wav" $s 07-Stompbox "Stompbox" "$a" "Stompbox" \
      "Factory preset Stompbox at 132 BPM: the Germanium drive model at full mix, drive 70 %. ${moves}"
   sk_take "$w/08.wav" "Halo" 130
   finish "$w/08.wav" $s 08-Halo "Halo" "$a" "Halo" \
      "Factory preset Halo at 130 BPM: the Lead drive model mixed in at 30 % under the clean filter. ${moves}"
   sk_take "$w/09.wav" "Teeth" 138
   finish "$w/09.wav" $s 09-Teeth "Teeth" "$a" "Teeth" \
      "Factory preset Teeth at 138 BPM: resonance 92 %, accent sweep 4.5 octaves, drive 85 % and the ladder driven harder than stock. ${moves}"
   sk_take "$w/10.wav" "Classic Squelch" 130 --param Mode=Sequencer --seq \
      --param 54=On --param 60=Ping-Pong --param 57=1/8. --param 58=45 --param 59=30
   finish "$w/10.wav" $s 10-Squelch-Ping-Pong "Squelch Through the Delay" "$a" "Classic Squelch + Delay" \
      "Classic Squelch at 130 BPM as in take 01, with the built-in delay switched on: ping-pong, synced dotted eighth, feedback 45 %, mix 30 %. ${moves}"
   manifest $s
}

# ------------------------------------------------------------------ RumpelKiste

rk_take() { # <out.wav> <preset> <bpm> <bars>
   local secs
   secs="$(python3 -c "print(round($4 * 240.0 / $3, 4))")"
   "$RK_RENDER" --plugin "$RK_CLAP" --preset "$2" --bpm "$3" --seconds "$secs" --tail 2 --rate 48000 \
      --out "$1" >/dev/null
}

do_rumpelkiste() {
   echo "==> RumpelKiste"
   local s=rumpelkiste a="RumpelKiste" w="${WORK}/rk"
   local seq="Played by the plugin's own sequencer"
   mkdir -p "$w"; begin $s
   rk_take "$w/01.wav" "Warehouse" 130 12
   finish "$w/01.wav" $s 01-Warehouse "Warehouse" "$a" "Warehouse" \
      "${seq}, 12 bars at 130 BPM: four chained patterns (floor, rim shot, ride, tom fill) with a long kick; no drive."
   rk_take "$w/02.wav" "Jack Box" 122 12
   finish "$w/02.wav" $s 02-Jack-Box "Jack Box" "$a" "Jack Box" \
      "${seq}, 12 bars at 122 BPM: Chicago house with Shuffle on 3, doubled claps at the end of the bar and a tight kick; no drive."
   rk_take "$w/03.wav" "Motor City" 128 12
   finish "$w/03.wav" $s 03-Motor-City "Motor City" "$a" "Motor City" \
      "${seq}, 12 bars at 128 BPM: Detroit techno with the toms tuned to a minor triad over the ride; no drive."
   rk_take "$w/04.wav" "Planet Funk" 110 12
   finish "$w/04.wav" $s 04-Planet-Funk "Planet Funk" "$a" "Planet Funk" \
      "${seq}, 12 bars at 110 BPM: electro with a broken, booming kick, snare and clap on two and four and a tom run; no drive."
   rk_take "$w/05.wav" "Breaks" 135 12
   finish "$w/05.wav" $s 05-Breaks "Breaks" "$a" "Breaks" \
      "${seq}, 12 bars at 135 BPM: a breakbeat with ghost notes, a flam and an open hat at the end of the bar."
   rk_take "$w/06.wav" "Two Step" 132 12
   finish "$w/06.wav" $s 06-Two-Step "Two Step" "$a" "Two Step" \
      "${seq}, 12 bars at 132 BPM: UK garage with Shuffle on 5, a skipping kick and rim shots in the gaps."
   rk_take "$w/07.wav" "Big Room" 126 12
   finish "$w/07.wav" $s 07-Big-Room "Big Room" "$a" "Big Room" \
      "${seq}, 12 bars at 126 BPM: four chained bars with a crash on the first and a flammed snare roll building through the last."
   rk_take "$w/08.wav" "Hot Desk" 124 12
   finish "$w/08.wav" $s 08-Hot-Desk "Hot Desk" "$a" "Hot Desk" \
      "${seq}, 12 bars at 124 BPM: a house groove with the whole kit through the Tube model on the drive bus, mixed at 80 %."
   rk_take "$w/09.wav" "Bitcrushed" 130 12
   finish "$w/09.wav" $s 09-Bitcrushed "Bitcrushed" "$a" "Bitcrushed" \
      "${seq}, 12 bars at 130 BPM: a techno groove through the Crush model on the drive bus at 70 %, the hats' converter at five bits."
   rk_take "$w/10.wav" "Hardcore Fuzz" 172 16
   finish "$w/10.wav" $s 10-Hardcore-Fuzz "Hardcore Fuzz" "$a" "Hardcore Fuzz" \
      "${seq}, 16 bars at 172 BPM: only the kick routed through the Fuzz model, pushed hard; hats and snare stay clean."
   manifest $s
}

# -------------------------------------------------------------------- Substrike

ss_take() { # <out.wav> <preset key> <bpm>: one hit per beat, whole bars, >= 14 s, 2.5 s tail
   local hits secs
   hits="$(python3 -c "import math; print(4 * math.ceil(14.0 / (240.0 / $3)))")"
   secs="$(python3 -c "print(round(${hits} * 60.0 / $3 + 2.5, 4))")"
   "$SS_RENDER" --plugin "$SS_CLAP" --preset "$2" --hits "$hits" --bpm "$3" --seconds "$secs" \
      --rate 48000 --out "$1" >/dev/null
   echo "$hits"
}

do_substrike() {
   echo "==> Substrike"
   local s=substrike a="Substrike" w="${WORK}/ss" n
   mkdir -p "$w"; begin $s
   n=$(ss_take "$w/01.wav" techno-concrete-thump 140)
   finish "$w/01.wav" $s 01-Concrete-Thump "Concrete Thump" "$a" "Concrete Thump" \
      "Hard techno kick, ${n} hits on every beat at 140 BPM, kick only: a long pitch drop, a saturated, clipped body and a sharp click."
   n=$(ss_take "$w/02.wav" rumble-classic 138)
   finish "$w/02.wav" $s 02-Classic-Rumble "Classic Rumble" "$a" "Classic Rumble" \
      "Techno rumble, ${n} hits at 138 BPM: the kick through a big dark reverb, clipped into a low floor that is ducked by every hit. All from the plugin."
   n=$(ss_take "$w/03.wav" techno-berlin-pressure 132)
   finish "$w/03.wav" $s 03-Berlin-Pressure "Berlin Pressure" "$a" "Berlin Pressure" \
      "Techno kick, ${n} hits at 132 BPM: heavy and long, through a valve stack held back by lowpasses."
   n=$(ss_take "$w/04.wav" house-deep-round 124)
   finish "$w/04.wav" $s 04-Deep-Round "Deep Round" "$a" "Deep Round" \
      "Deep house kick, ${n} hits at 124 BPM: a round sine at 55 Hz with a soft beater, the clean end of the range."
   n=$(ss_take "$w/05.wav" hiphop-boom-bap-dust 90)
   finish "$w/05.wav" $s 05-Boom-Bap-Dust "Boom Bap Dust" "$a" "Boom Bap Dust" \
      "Hip hop kick, ${n} hits at 90 BPM: a skin over a short body, crushed to twelve bits, with synthesised vinyl crackle."
   n=$(ss_take "$w/06.wav" trap-sub-boom-long 70)
   finish "$w/06.wav" $s 06-Sub-Boom-Long "Sub Boom Long" "$a" "Sub Boom Long" \
      "Trap 808-style boom, ${n} hits at 70 BPM (half notes at 140) on its root note: a long tuned sub tail."
   n=$(ss_take "$w/07.wav" hardstyle-euphoric 150)
   finish "$w/07.wav" $s 07-Euphoric-Hardstyle "Euphoric Hardstyle" "$a" "Euphoric Hardstyle" \
      "Hardstyle kick, ${n} hits at 150 BPM: a zap into a long distorted tail whose pitch climbs back after the drop."
   n=$(ss_take "$w/08.wav" hardcore-gabber-classic 180)
   finish "$w/08.wav" $s 08-Gabber-Classic "Gabber Classic" "$a" "Gabber Classic" \
      "Gabber kick, ${n} hits at 180 BPM: a long body driven into fuzz until it buzzes, then hard clipped."
   n=$(ss_take "$w/09.wav" acoustic-studio 110)
   finish "$w/09.wav" $s 09-Studio-Kick "Studio Kick" "$a" "Studio Kick" \
      "Acoustic-style kick, ${n} hits at 110 BPM: a tuned skin, a body, a felt beater and a small room, all synthesised."
   n=$(ss_take "$w/10.wav" exp-chaos-engine 128)
   finish "$w/10.wav" $s 10-Chaos-Engine "Chaos Engine" "$a" "Chaos Engine" \
      "Experimental kick, ${n} hits at 128 BPM: random values and free LFOs move pitch, decay, drive, filter and noise, so no two hits match."
   manifest $s
}

# ----------------------------------------------------------- effect sources

# Aurum's input: a two-bar phrase at 120 BPM -- RumpelKiste "Minimal" plus
# SaeureKiste "Wide Range" playing the renderer's acid test line from C4 --
# heard at 0 s and at 8 s, with two bars of silence after each. 16 s long.
make_aurum_source() {
   local out="${SRC}/aurum-dry.wav"
   "$RK_RENDER" --plugin "$RK_CLAP" --preset "Minimal" --bpm 120 --seconds 4 --tail 1.5 --rate 48000 \
      --out "${SRC}/au-drums-raw.wav" >/dev/null
   "$SK_RENDER" --plugin "$SK_CLAP" --preset "Wide Range" --key 60 --bpm 120 --seconds 4 --tail 1.5 \
      --rate 48000 --out "${SRC}/au-line-raw.wav" >/dev/null
   to_lufs "${SRC}/au-drums-raw.wav" -21 "${SRC}/au-drums.wav"
   to_lufs "${SRC}/au-line-raw.wav" -22 "${SRC}/au-line.wav"
   ffmpeg -nostats -hide_banner -loglevel error -y -i "${SRC}/au-drums.wav" -i "${SRC}/au-line.wav" \
      -filter_complex "[0][1]amix=inputs=2:normalize=0:duration=longest,asplit[a][b];[b]adelay=8000|8000[c];[a][c]amix=inputs=2:normalize=0:duration=longest,apad=whole_dur=16,atrim=end=16" \
      -ac 2 -c:a pcm_f32le "$out"
}

# Shapeau's input: eight bars at 130 BPM -- RumpelKiste "Acid Companion"
# drums, SaeureKiste "Classic Squelch" from its own sequencer as the bass, and
# ReSIDent playing Cynthcart chords (Cm, Ab, Fm, Cm, one per bar) -- plus a
# kick-only render of the same drums for the sidechain.
make_shapeau_source() {
   local out="${SRC}/shapeau-dry.wav"
   local bars=8 bpm=130 secs roms dev
   secs="$(python3 -c "print(round(${bars} * 240.0 / ${bpm}, 4))")"
   "$RK_RENDER" --plugin "$RK_CLAP" --preset "Acid Companion" --bpm $bpm --seconds "$secs" --tail 2 \
      --rate 48000 --out "${SRC}/sh-drums-raw.wav" >/dev/null
   "$RK_RENDER" --plugin "$RK_CLAP" --preset "Acid Companion" --bpm $bpm --seconds "$secs" --tail 2 \
      --rate 48000 --param sd_level=0 --param lt_level=0 --param mt_level=0 --param ht_level=0 \
      --param rs_level=0 --param cp_level=0 --param hh_level=0 --param cr_level=0 --param rd_level=0 \
      --out "${SRC}/shapeau-kick.wav" >/dev/null
   "$SK_RENDER" --plugin "$SK_CLAP" --preset "Classic Squelch" --param Mode=Sequencer --seq \
      --move "Mode=Sequencer..MIDI@${secs}:$(python3 -c "print(${secs} + 0.001)")" \
      --loops $bars --seconds 1 --bpm $bpm --tail 2 --rate 48000 --out "${SRC}/sh-bass-raw.wav" >/dev/null
   # Cynthcart boots in about three seconds; the chords start at 4 s and the
   # boot is cut away afterwards.
   roms="${RESIDENT_DEMO_ROMS:-${AP}/resident/_dev_/roms}"
   dev="${AP}/resident/_dev_"
   local args=() t chord bar=0
   for chord in 60,63,67 56,60,63 53,56,60 60,63,67 60,63,67 56,60,63 53,56,60 60,63,67; do
      t="$(python3 -c "print(round(4 + ${bar} * 240.0 / ${bpm}, 4))")"
      for k in ${chord//,/ }; do args+=(--note "${t}:${k}:90:1.7"); done
      bar=$((bar + 1))
   done
   "$RS_RUN" --roms "$roms" --prg "${dev}/cynthcart/bin/cynthcart.prg" --no-video \
      --seconds "$(python3 -c "print(4 + ${secs} + 2)")" "${args[@]}" --wav "${SRC}/sh-chords-full.wav" >/dev/null
   ffmpeg -nostats -hide_banner -loglevel error -y -ss 4 -i "${SRC}/sh-chords-full.wav" -c:a pcm_f32le \
      "${SRC}/sh-chords-raw.wav"
   to_lufs "${SRC}/sh-drums-raw.wav" -19 "${SRC}/sh-drums.wav"
   to_lufs "${SRC}/sh-bass-raw.wav" -22 "${SRC}/sh-bass.wav"
   to_lufs "${SRC}/sh-chords-raw.wav" -27 "${SRC}/sh-chords.wav"
   ffmpeg -nostats -hide_banner -loglevel error -y -i "${SRC}/sh-drums.wav" -i "${SRC}/sh-bass.wav" \
      -i "${SRC}/sh-chords.wav" -filter_complex "amix=inputs=3:normalize=0:duration=longest" \
      -ac 2 -c:a pcm_f32le "$out"
   # The bass and chords alone: the ducking presets go on this, as they would
   # on a music bus, with the drums added back dry afterwards.
   ffmpeg -nostats -hide_banner -loglevel error -y -i "${SRC}/sh-bass.wav" -i "${SRC}/sh-chords.wav" \
      -filter_complex "amix=inputs=2:normalize=0:duration=longest" -ac 2 -c:a pcm_f32le "${SRC}/shapeau-music.wav"
}

# ----------------------------------------------------------------------- Aurum

au_take() { # <out.wav> <preset name> [ap-fxhost args...]
   local out="$1" preset="$2"; shift 2
   "$FXHOST" --plugin "$AU_CLAP" --in "${SRC}/aurum-dry.wav" --state "${WORK}/aurum-states/${preset}.aurumstate" \
      --bpm 120 --tail 16 --block 256 "$@" --out "$out"
}

do_aurum() {
   echo "==> Aurum"
   local s=aurum a="Aurum" w="${WORK}/au"
   mkdir -p "$w" "${WORK}/aurum-states"; begin $s
   "${BUILD}/aurum-preset-dump" "${WORK}/aurum-states" >/dev/null
   make_aurum_source
   local src="Input: a two-bar phrase at 120 BPM (RumpelKiste Minimal drums and a SäureKiste line), played twice with two bars of silence after each."
   finish "${SRC}/aurum-dry.wav" $s 01-Dry "Dry" "$a" "(none, dry input)" \
      "The input on its own, no reverb: a two-bar phrase at 120 BPM (RumpelKiste Minimal drums and a SäureKiste Wide Range line from C4), played twice with two bars of silence after each."
   au_take "$w/02.wav" "Drum Room"
   finish "$w/02.wav" $s 02-Drum-Room "Drum Room" "$a" "Drum Room" \
      "Factory preset Drum Room as shipped (Natural, 0.7 s, mix 30 %). ${src}"
   au_take "$w/03.wav" "Small Chamber"
   finish "$w/03.wav" $s 03-Small-Chamber "Small Chamber" "$a" "Small Chamber" \
      "Factory preset Small Chamber as shipped (Natural, 1.2 s, mix 25 %). ${src}"
   au_take "$w/04.wav" "Concert Hall"
   finish "$w/04.wav" $s 04-Concert-Hall "Concert Hall" "$a" "Concert Hall" \
      "Factory preset Concert Hall as shipped (Natural, 2.4 s, 18 ms pre-delay, mix 30 %). ${src}"
   au_take "$w/05.wav" "Cathedral"
   finish "$w/05.wav" $s 05-Cathedral "Cathedral" "$a" "Cathedral" \
      "Factory preset Cathedral as shipped (Natural, 9 s, 50 ms pre-delay, mix 30 %). ${src}"
   au_take "$w/06.wav" "Classic Plate"
   finish "$w/06.wav" $s 06-Classic-Plate "Classic Plate" "$a" "Classic Plate" \
      "Factory preset Classic Plate as shipped (Plate algorithm, 2 s, mix 25 %). ${src}"
   au_take "$w/07.wav" "Shimmer Wash"
   finish "$w/07.wav" $s 07-Shimmer-Wash "Shimmer Wash" "$a" "Shimmer Wash" \
      "Factory preset Shimmer Wash as shipped (Classic algorithm, 7 s, heavy modulation, mix 40 %). ${src}"
   au_take "$w/08.wav" "Gated Drums"
   finish "$w/08.wav" $s 08-Gated-Drums "Gated Drums" "$a" "Gated Drums" \
      "Factory preset Gated Drums as shipped (Natural, 1.8 s, auto gate with 180 ms hold, mix 35 %). ${src}"
   au_take "$w/09.wav" "Ducked Delay Space"
   finish "$w/09.wav" $s 09-Ducked-Delay-Space "Ducked Delay Space" "$a" "Ducked Delay Space" \
      "Factory preset Ducked Delay Space as shipped (Natural, 2.5 s, 250 ms pre-delay, ducking 70 %). ${src}"
   au_take "$w/10.wav" "Freeze Pad" --at "11.9:Freeze=On" --at "18:Freeze=Off"
   finish "$w/10.wav" $s 10-Freeze-Pad "Freeze Pad" "$a" "Freeze Pad" \
      "Factory preset Freeze Pad (Natural, 6 s, mix 50 %), with Freeze switched on at 11.9 s, the end of the second phrase, and off at 18 s, releasing it into the decay. ${src}"
   manifest $s
}

# --------------------------------------------------------------------- Shapeau

sh_take() { # <out.wav> <preset file stem> [ap-fxhost args...]
   local out="$1" preset="$2"; shift 2
   "$FXHOST" --plugin "$SH_CLAP" --in "${SRC}/shapeau-dry.wav" --preset "${AP}/shapeau/presets/${preset}.shapeau" \
      --bpm 130 --tail 1 --block 256 "$@" --out "$out"
}

# The ducking presets: Shapeau on the bass and chords only, the drums mixed back
# in dry afterwards, at the same levels as in the dry loop.
sh_bus_take() { # <out.wav> <preset file stem> [ap-fxhost args...]
   local out="$1" preset="$2"; shift 2
   "$FXHOST" --plugin "$SH_CLAP" --in "${SRC}/shapeau-music.wav" --preset "${AP}/shapeau/presets/${preset}.shapeau" \
      --bpm 130 --tail 1 --block 256 "$@" --out "${out%.wav}-bus.wav"
   ffmpeg -nostats -hide_banner -loglevel error -y -i "${SRC}/sh-drums.wav" -i "${out%.wav}-bus.wav" \
      -filter_complex "amix=inputs=2:normalize=0:duration=longest" -ac 2 -c:a pcm_f32le "$out"
}

do_shapeau() {
   echo "==> Shapeau"
   local s=shapeau a="Shapeau" w="${WORK}/sh"
   mkdir -p "$w"; begin $s
   make_shapeau_source
   local src="Input: the same eight bars at 130 BPM as the dry take (RumpelKiste drums, SäureKiste bass, ReSIDent chords), host tempo 130 BPM from bar 1."
   finish "${SRC}/shapeau-dry.wav" $s 01-Dry "Dry Loop" "$a" "(none, dry input)" \
      "The input on its own, no Shapeau: eight bars at 130 BPM of RumpelKiste Acid Companion drums, SäureKiste Classic Squelch bass and Cynthcart chords played by ReSIDent (Cm, Ab, Fm, Cm)."
   sh_bus_take "$w/02.wav" kick_duck
   finish "$w/02.wav" $s 02-Kick-Duck "Kick Duck" "$a" "Kick Duck" \
      "Factory preset Kick Duck on the bass and chords only (the drums are mixed back in dry afterwards): their low band dips on every quarter note, mid and high bands untouched. Same eight bars at 130 BPM as the dry take."
   sh_bus_take "$w/03.wav" sidechain_duck --sidechain "${SRC}/shapeau-kick.wav"
   finish "$w/03.wav" $s 03-Sidechain-Duck "Sidechain Duck" "$a" "Sidechain Duck" \
      "Factory preset Sidechain Duck on the bass and chords only, its sidechain input fed a kick-only render of the same drums, so they duck at every kick; the drums are mixed back in dry afterwards. Same eight bars at 130 BPM as the dry take."
   sh_take "$w/04.wav" filter_wobble
   finish "$w/04.wav" $s 04-Filter-Wobble "Filter Wobble" "$a" "Filter Wobble" \
      "Factory preset Filter Wobble: a resonant cutoff sweep on the eighth notes, darkest in the low band. ${src}"
   sh_take "$w/05.wav" gated_stabs
   finish "$w/05.wav" $s 05-Gated-Stabs "Gated Stabs" "$a" "Gated Stabs" \
      "Factory preset Gated Stabs: a hard sixteenth-note gate on all bands, the low band open only on the off-beats. ${src}"
   sh_take "$w/06.wav" drive_pump
   finish "$w/06.wav" $s 06-Drive-Pump "Drive Pump" "$a" "Drive Pump" \
      "Factory preset Drive Pump: drive shaped like a duck, clean between hits and saturated on them, hardest in the mid band. ${src}"
   sh_take "$w/07.wav" live_stutter
   finish "$w/07.wav" $s 07-Live-Stutter "Live Stutter" "$a" "Live Stutter" \
      "Factory preset Live Stutter: sixteenth-note stutter on the mid and high bands only, low band untouched. ${src}"
   sh_take "$w/08.wav" crush_steps
   finish "$w/08.wav" $s 08-Crush-Steps "Crush Steps" "$a" "Crush Steps" \
      "Factory preset Crush Steps: a sixteen-step bit and sample-rate crush pattern on all bands, mixed at 75 %. ${src}"
   sh_take "$w/09.wav" pan_steps
   finish "$w/09.wav" $s 09-Pan-Steps "Pan Steps" "$a" "Pan Steps" \
      "Factory preset Pan Steps: the low band jumps left and right in sixteen steps, mid and high bands stay centred. ${src}"
   sh_take "$w/10.wav" half_speed_stop
   finish "$w/10.wav" $s 10-Half-Speed-Stop "Half Speed Stop" "$a" "Half Speed Stop" \
      "Factory preset Half Speed Stop: the read head falls across each bar into half speed and holds, mixed against the dry signal. ${src}"
   manifest $s
}

# -------------------------------------------------------------------- ReSIDent

# notes <start> <step> <len> <vel> <chord>... : one --note per key, chord after chord.
rs_notes() {
   local t="$1" step="$2" len="$3" vel="$4"; shift 4
   local chord k
   for chord in "$@"; do
      for k in ${chord//,/ }; do printf -- '--note %s:%s:%s:%s ' "$t" "$k" "$vel" "$len"; done
      t="$(python3 -c "print(round(${t} + ${step}, 3))")"
   done
}

# A CC ramp on channel 1: rs_ramp <cc> <t0> <t1> <v0> <v1> <steps>
rs_ramp() {
   python3 -c "
cc, t0, t1, v0, v1, n = $1, $2, $3, $4, $5, $6
for i in range(n + 1):
    f = i / n
    print('--cc %.3f:%d:%d' % (t0 + (t1 - t0) * f, cc, round(v0 + (v1 - v0) * f)), end=' ')"
}

# rs_take <out.wav> <program option> <file> <start> <seconds> <run args...>
rs_take() {
   local out="$1" opt="$2" file="$3" start="$4" secs="$5"; shift 5
   "$RS_RUN" --roms "$RS_ROMS" "$opt" "$file" --no-video \
      --seconds "$(python3 -c "print(${start} + ${secs})")" "$@" --wav "${out%.wav}-full.wav" >/dev/null
   ffmpeg -nostats -hide_banner -loglevel error -y -ss "$start" -t "$secs" -i "${out%.wav}-full.wav" \
      -c:a pcm_f32le "$out"
}

do_resident() {
   echo "==> ReSIDent"
   local s=resident a="ReSIDent" w="${WORK}/rs"
   RS_ROMS="${RESIDENT_DEMO_ROMS:-${AP}/resident/_dev_/roms}"
   local dev="${AP}/resident/_dev_"
   local cyn="${dev}/cynthcart/bin/cynthcart.prg"
   [ -d "$RS_ROMS" ] && [ -f "$cyn" ] || { echo "    no ROMs or programs in ${dev}; skipping ReSIDent"; return; }
   mkdir -p "$w"; begin $s
   local turn=(69,72,76 65,69,72 60,64,67 67,71,74 69,72,76 65,69,72 67,71,74 69,72,76)
   local eng="reSIDfp engine, offline, 48 kHz"

   # shellcheck disable=SC2046
   rs_take "$w/01.wav" --prg "$cyn" 4 18 --model 1:8580R5 $(rs_notes 4 2 1.9 100 "${turn[@]}")
   finish "$w/01.wav" $s 01-Cynthcart-Chords "Cynthcart Chords" "$a" "Cynthcart (start-up sound), 8580" \
      "Cynthcart by Paul Slocum running on the emulated C64 with one 8580 SID, its start-up sound, played eight held chords (an A minor turnaround, 2 s each) over the emulated MIDI cartridge. ${eng}."
   # shellcheck disable=SC2046
   rs_take "$w/02.wav" --prg "$cyn" 4 18 --model 1:6581 $(rs_notes 4 2 1.9 100 "${turn[@]}")
   finish "$w/02.wav" $s 02-Cynthcart-Chords-6581 "Cynthcart Chords on a 6581" "$a" "Cynthcart (start-up sound), 6581" \
      "The same notes and program as take 01, with the SID set to the original 6581 model instead of the 8580. ${eng}."
   # Voice Mode Arp1 (CC 2 = 80), the cutoff (CC 1) opened slowly over the take.
   # shellcheck disable=SC2046
   rs_take "$w/03.wav" --prg "$cyn" 4 18 --model 1:6581 --cc 3.6:2:80 $(rs_ramp 1 4 20 20 110 32) \
      $(rs_notes 4 4 3.9 100 57,60,64 53,57,60 48,52,55 55,59,62)
   finish "$w/03.wav" $s 03-Cynthcart-Arpeggio "Cynthcart Arpeggio" "$a" "Cynthcart, Voice Mode Arp1, 6581" \
      "Cynthcart on a 6581 with Voice Mode set to Arp1 by MIDI CC 2, four chords held 4 s each (Am, F, C, G) and the cutoff (CC 1) raised from 20 to 110 across the take. ${eng}."
   # shellcheck disable=SC2046
   rs_take "$w/04.wav" --d64 "${dev}/SID_Synth/sidsynth.d64" 14 18 \
      --key 9:RUNSTOP:0.2 --key 11:RETURN:0.2 --key 12:RUNSTOP:0.2 \
      $(rs_notes 14 4 3.8 100 57,60,64 53,57,60 48,52,55 55,59,62)
   finish "$w/04.wav" $s 04-SID-Synth "SID Synth Arpeggio" "$a" "SID Synth (start-up instrument)" \
      "SID Synth by LukHash loaded from its disk image, MIDI switched on in its menu by typed keys, then four chords held 4 s each (Am, F, C, G) on its start-up instrument, an arpeggio. ${eng}."
   local bass=(45 45 57 45 43 43 55 43 41 41 53 41 43 43 55 47 45 45 57 45 43 43 55 43 41 41 53 41 40 40 52 44)
   # shellcheck disable=SC2046
   rs_take "$w/05.wav" --d64 "${dev}/Station64/sta64.d64" 15 19 $(rs_notes 15 0.5 0.4 100 "${bass[@]}")
   finish "$w/05.wav" $s 05-Station64-Bass "Station64 Bass" "$a" "Station64 (start-up sound)" \
      "Station64 by DJ Indikator loaded from its disk image, its start-up sound playing a 32-note bass line, one note every 0.5 s. ${eng}."
   # The same line with the cutoff (CC 1) swept down and back up and the
   # resonance (CC 20) raised.
   # shellcheck disable=SC2046
   rs_take "$w/06.wav" --d64 "${dev}/Station64/sta64.d64" 15 19 --cc 14.5:20:100 \
      $(rs_ramp 1 15 23 110 15 32) $(rs_ramp 1 23 31 15 110 32) \
      $(rs_notes 15 0.5 0.4 100 "${bass[@]}")
   finish "$w/06.wav" $s 06-Station64-Filter-Sweep "Station64 Filter Sweep" "$a" "Station64 (start-up sound) + CC" \
      "The same Station64 line as take 05, with the resonance raised (CC 20 = 100) and the cutoff (CC 1) swept from 110 down to 15 and back over the 16 s, as a DAW would automate it. ${eng}."
   manifest $s
}

for slug in "${SLUGS[@]}"; do
   case "$slug" in
      saeurekiste) do_saeurekiste ;;
      rumpelkiste) do_rumpelkiste ;;
      aurum)       do_aurum ;;
      substrike)   do_substrike ;;
      shapeau)     do_shapeau ;;
      resident)    do_resident ;;
      *) echo "unknown slug: $slug" >&2; exit 2 ;;
   esac
done
echo "==> done: ${OUTROOT}"
