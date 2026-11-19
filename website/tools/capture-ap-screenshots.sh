#!/bin/bash
#
# Captures current window screenshots of the audio-plugins for the website,
# into website/images/<slug>*.png.
#
#    website/tools/capture-ap-screenshots.sh <audio-plugins-dir> <build-dir>
#
# <build-dir> holds out-of-tree builds with the tools on, one folder per plugin
# (as render-ap-demos.sh makes them). The audio-plugins tree is only read:
# each plugin's own tools/make-screenshots.sh is run from a patched temporary
# copy whose output, work folders and build paths all point outside it, so a
# run leaves nothing behind in that repository. RumpelKiste has no such script
# and is captured here with its guihost.
#
# Needs Xvfb, xdotool and ImageMagick.
set -euo pipefail
ap="$(cd "$1" && pwd)"
bd="$(cd "$2" && pwd)"
web="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
img="$web/images"
work="$(mktemp -d)"
trap 'kill ${xvfb:-} 2>/dev/null; rm -rf "$work"' EXIT

# run_patched <plugin-folder> <outdir> [sed expressions...] -- [script args...]
run_patched() {
   local folder="$1" out="$2"; shift 2
   local seds=() args=()
   while [ $# -gt 0 ] && [ "$1" != "--" ]; do seds+=(-e "$1"); shift; done
   [ "${1:-}" = "--" ] && shift
   args=("$@")
   local tools="$ap/$folder/tools"
   sed -e "s|^here=.*|here=\"$tools\"|" "${seds[@]}" "$tools/make-screenshots.sh" > "$work/$folder.sh"
   mkdir -p "$out"
   bash "$work/$folder.sh" "${args[@]}"
}

Xvfb :87 -screen 0 2400x1600x24 >/dev/null 2>&1 &
xvfb=$!
sleep 1
export DISPLAY=:87

echo "==> SaeureKiste"
SAEUREKISTE_BUILD="$bd/saeure-kiste" run_patched saeure-kiste "$work/sk" -- "$work/sk"
cp "$work/sk/window.png" "$img/saeurekiste-window.png"
cp "$work/sk/window-advanced.png" "$img/saeurekiste-window-advanced.png"
cp "$work/sk/window-fx.png" "$img/saeurekiste-window-fx.png"

echo "==> Aurum"
AURUM_BUILD="$bd/aurum" run_patched aurum "$work/au" -- "$work/au"
cp "$work/au/window.png" "$img/aurum-window.png"

echo "==> Substrike"
run_patched substrike "$work/ss" "s|\${src}/build|$bd/substrike|g" -- "$work/ss"
cp "$work/ss/window.png" "$img/substrike-window.png"

echo "==> Shapeau"
run_patched shapeau "$work/sh" "s|^out_dir=.*|out_dir=\"$work/sh\"|" -- "$bd/shapeau"
cp "$work/sh/curve.png" "$img/shapeau-window.png"
cp "$work/sh/chain.png" "$img/shapeau-chain.png"
cp "$work/sh/filter.png" "$img/shapeau-filter.png"
cp "$work/sh/sidechain.png" "$img/shapeau-sidechain.png"

echo "==> ReSIDent"
run_patched resident "$work/rs" "s|\${src}/build/resident-gui-snapshot|$bd/resident/resident-gui-snapshot|" "s|\${src}/build/screenshot-work|$work/rs-work|" -- "$work/rs"
cp "$work/rs/window.png" "$img/resident-window.png"
cp "$work/rs/sidsynth.png" "$img/resident-sidsynth.png"
cp "$work/rs/machine.png" "$img/resident-machine.png"
cp "$work/rs/cc.png" "$img/resident-cc.png"

echo "==> RumpelKiste"
cfg="$work/rk-config"; mkdir -p "$cfg"
preset="$(grep -l -x 'name = Warehouse' "$ap/rumpel-kiste/presets/"*.rumpelkiste | head -1)"
mkdir -p "$cfg/RumpelKiste/presets"; cp "$preset" "$cfg/RumpelKiste/presets/"
for attempt in 1 2; do
   XDG_CONFIG_HOME="$cfg" "$bd/rumpel-kiste/rumpelkiste-guihost" "$bd/rumpel-kiste/RumpelKiste.clap" \
      "$cfg/RumpelKiste/presets/$(basename "$preset")" 20 >/dev/null 2>&1 &
   pid=$!
   wid=""
   for _ in $(seq 1 40); do wid="$(xdotool search --name '^RumpelKiste$' 2>/dev/null | head -1 || true)"; [ -n "$wid" ] && break; sleep 0.25; done
   if [ -n "$wid" ]; then sleep 2; import -window "$wid" "$img/rumpelkiste-window.png"; kill $pid 2>/dev/null || true; break; fi
   kill $pid 2>/dev/null || true
done

for f in saeurekiste-window saeurekiste-window-advanced saeurekiste-window-fx aurum-window substrike-window \
         shapeau-window shapeau-chain shapeau-filter shapeau-sidechain resident-window resident-sidsynth resident-machine resident-cc rumpelkiste-window; do
   echo "  $f.png $(identify -format '%wx%h' "$img/$f.png")"
done
