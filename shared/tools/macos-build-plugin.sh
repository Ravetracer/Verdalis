#!/usr/bin/env bash
#
# Builds, tests and signs one plugin for macOS, and copies its three bundles
# into <outdir>. Called once per plugin by .github/workflows/macos.yml; runs on
# a Mac with the SDKs in CLAP/ (AudioUnitSDK included) and the static Cairo in
# macbuild/cairo-macos (shared/cmake/build-macos-cairo.sh).
#
#   shared/tools/macos-build-plugin.sh rainyday dist/macos
#
# Steps, any failure stopping the plugin:
#   - configure and build universal (arm64 + x86_64): the .clap, the .vst3, the
#     .component and the offline renderer
#   - every bundle universal, exporting clap_entry (on macOS the VST3 and the
#     Audio Unit find the plugin in their own bundle through it; without it
#     they would look for a .clap installed elsewhere), carrying its presets
#   - the plugin's self-test through the built .clap, on arm64 and, where the
#     machine has Rosetta, on x86_64
#   - ad hoc signing
#   - Apple's auval on the Audio Unit, installed into the user's Components
#     folder (it has to be, for auval to find it)
set -euo pipefail

folder="${1:?usage: $0 <plugin folder> <outdir>}"
outdir="${2:?usage: $0 <plugin folder> <outdir>}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$here"

name="$(sed -n 's/^project(\([A-Za-z]*\) VERSION.*/\1/p' "${folder}/CMakeLists.txt")"
[ -n "$name" ] || { echo "no project() name in ${folder}/CMakeLists.txt"; exit 1; }
prefix="$(echo "$folder" | tr '[:lower:]' '[:upper:]')"
build="${folder}/build"

cmake -S "$folder" -B "$build" -G Ninja \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
   -D"${prefix}_BUILD_TOOLS=ON" \
   -D"${prefix}_BUILD_VST3=ON" \
   -D"${prefix}_BUILD_AUV2=ON"

# -k 0: every compile error of this plugin in one go, not the first.
cmake --build "$build" --target "$name" "${name}-vst3" "${name}-auv2" "${folder}-render" -- -k 0

# The wrapper decides where its bundles land on macOS, so they are looked up.
bundles=()
for ext in clap vst3 component; do
   b="$(find "$build" -maxdepth 3 -type d -name "${name}.${ext}" -not -path '*/CMakeFiles/*' -print -quit)"
   [ -n "$b" ] || { echo "no ${name}.${ext} in ${build}"; exit 1; }
   bin="$b/Contents/MacOS/${name}"
   lipo "$bin" -verify_arch arm64 x86_64
   lipo -info "$bin"
   # Not grep -q: it closes the pipe early, and pipefail would count that.
   nm -gU "$bin" | grep ' _clap_entry$' >/dev/null || { echo "$bin does not export clap_entry"; exit 1; }
   [ -d "$b/Contents/Resources/presets" ] || { echo "$b has no presets"; exit 1; }
   bundles+=("$b")
done
clap="${bundles[0]}"
component="${bundles[2]}"

render="${build}/${folder}-render"
plugin="${clap}/Contents/MacOS/${name}"
echo "--- self-test, arm64"
"$render" --plugin "$plugin" --selftest
if arch -x86_64 /usr/bin/true 2>/dev/null; then
   echo "--- self-test, x86_64 (Rosetta)"
   arch -x86_64 "$render" --plugin "$plugin" --selftest
else
   echo "no Rosetta on this machine: x86_64 not run"
fi

for b in "${bundles[@]}"; do
   codesign --force --deep --sign - "$b"
   codesign --verify --deep --strict --verbose=2 "$b"
done

components="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$components"
rm -rf "${components}/${name}.component"
cp -R "$component" "$components/"
killall -9 AudioComponentRegistrar 2>/dev/null || true
# shellcheck disable=SC2046 # "aumu <subtype> Rvtr", three words on purpose
auval -v $(cat "${build}/auv2-id.txt")

mkdir -p "$outdir"
for b in "${bundles[@]}"; do
   rm -rf "${outdir}/$(basename "$b")"
   cp -R "$b" "$outdir/"
done
echo "${name}: built, tested, signed, validated"
