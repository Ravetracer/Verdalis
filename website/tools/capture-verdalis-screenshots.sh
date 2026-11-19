#!/bin/bash
#
# Captures the Verdalis plugin windows for the website into website/images/<folder>.png,
# each with a characteristic factory preset loaded, by opening the real window
# with the plugin's own guihost on a private X display.
#
#    website/tools/capture-verdalis-screenshots.sh [folder ...]    (default: all)
#
# Needs Xvfb, xdotool and ImageMagick's import, and every plugin built in
# <plugin>/build (the guihost is one of the offline tools).
#
# VerdaliScene's layer and effects pages need clicks into the window; the
# coordinates are passed with --click and are found by looking at the main
# capture first.
set -euo pipefail
suite="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
disp=":97"

# folder -> preset name ("" = whatever the plugin opens with)
declare -A PRESET=(
   [rainyday]="Steady Rain"      [thunderclap]="Summer Storm"   [shorebreak]="Big Waves Crashing"
   [skyhowl]="Howling Wind"      [chirpparade]="Dawn Chorus"    [riverflow]="Bubbling Creek"
   [crackleblaze]="Camp Fire"    [insectswarm]="Honeybee Field" [nightlife]="Summer Night"
   [verdaliscene]="Forest River" [whooshpact]="Deep Impact"
)

# capture <folder> <out.png> [xdotool clicks as "x,y" ...]
capture() {
   local folder="$1" out="$2"; shift 2
   local name host clap preset file wid
   name="$(sed -n 's/^project(\([A-Za-z0-9_]*\) .*/\1/p' "$suite/$folder/CMakeLists.txt" | head -1)"
   host="$suite/$folder/build/$folder-guihost"
   clap="$suite/$folder/build/$name.clap"
   preset="${PRESET[$folder]:-}"
   file=""
   if [ -n "$preset" ]; then
      file="$(grep -l -x "name = $preset" "$suite/$folder/presets/"*."$folder" | head -1 || true)"
      [ -n "$file" ] || echo "  warning: no preset file named '$preset' in $folder" >&2
   fi
   # Loaded from a throwaway user folder, so the browser recognises the file
   # by its path and shows the preset's name instead of "Init".
   local cfg; cfg="$(mktemp -d)"
   if [ -n "$file" ]; then
      mkdir -p "$cfg/$name/presets"
      cp "$file" "$cfg/$name/presets/"
      file="$cfg/$name/presets/$(basename "$file")"
   fi
   XDG_CONFIG_HOME="$cfg" DISPLAY="$disp" "$host" "$clap" "$file" 20 >/dev/null 2>"$cfg/guihost.log" &
   local pid=$!
   for _ in $(seq 1 40); do
      wid="$(DISPLAY="$disp" xdotool search --name "^${name}\$" 2>/dev/null | head -1 || true)"
      [ -n "$wid" ] && break
      sleep 0.25
   done
   [ -n "$wid" ] || { echo "  no window for $name: $(tail -1 "$cfg/guihost.log")" >&2; kill "$pid" 2>/dev/null; rm -rf "$cfg"; return 1; }
   sleep 2
   eval "$(DISPLAY="$disp" xdotool getwindowgeometry --shell "$wid")"
   for c in "$@"; do
      DISPLAY="$disp" xdotool mousemove $((X + ${c%,*})) $((Y + ${c#*,})) click 1
      sleep 1
   done
   DISPLAY="$disp" xdotool mousemove 0 0
   sleep 1
   DISPLAY="$disp" import -window "$wid" "$out"
   kill "$pid" 2>/dev/null || true
   wait "$pid" 2>/dev/null || true
   rm -rf "$cfg"
   echo "  $out ($(identify -format '%wx%h' "$out"))"
}

Xvfb "$disp" -screen 0 2400x1600x24 >/dev/null 2>&1 &
xvfb=$!
trap 'kill $xvfb 2>/dev/null' EXIT
sleep 1

folders=("$@")
[ ${#folders[@]} -gt 0 ] || folders=(rainyday thunderclap shorebreak skyhowl chirpparade riverflow crackleblaze insectswarm nightlife whooshpact verdaliscene)
for f in "${folders[@]}"; do
   echo "==> $f"
   # The plugins' guihost reparents with XFlush and now and then loses that
   # race on an idle Xvfb (BadWindow); a second attempt is the documented fix.
   capture "$f" "$suite/website/images/$f.png" || capture "$f" "$suite/website/images/$f.png"
done

# VerdaliScene's other two pages, by clicking into the Forest River scene:
# a layer tab (Birds 1), then that layer's effects.
if [ ${#folders[@]} -eq 0 ] || [[ " ${folders[*]} " == *" verdaliscene "* ]]; then
   echo "==> verdaliscene (layer and effects pages)"
   capture verdaliscene "$suite/website/images/verdaliscene-layer.png" "${VS_LAYER_CLICK:-275,88}" ||
      capture verdaliscene "$suite/website/images/verdaliscene-layer.png" "${VS_LAYER_CLICK:-275,88}"
   VS_FX_CLICK="${VS_FX_CLICK:-774,126}"
   if [ -n "$VS_FX_CLICK" ]; then
      capture verdaliscene "$suite/website/images/verdaliscene-fx.png" "${VS_LAYER_CLICK:-275,88}" "$VS_FX_CLICK" ||
         capture verdaliscene "$suite/website/images/verdaliscene-fx.png" "${VS_LAYER_CLICK:-275,88}" "$VS_FX_CLICK"
   fi
fi
