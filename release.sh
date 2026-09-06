#!/usr/bin/env bash
#
# Builds every plugin in the suite and packs one "verdalis-suite" archive
# containing the Linux and Windows binaries plus everything needed to install
# them.
#
#   ./release.sh 0.1.0                  Linux + Windows (needs cross-built Cairo)
#   ./release.sh 0.1.0 --linux-only     skip the Windows half
#
# Windows builds need a Cairo cross-built with mingw-w64. Build it once with
#
#   ./setup-winbuild.sh
#
# which puts it in winbuild/cairo-mingw, where this script finds it by itself.
# VERDALIS_WIN_CAIRO overrides that location.
#
# Without it the Windows plugins would build with no plugin window at all, so
# this script refuses to produce that silently -- pass --windows-no-gui if a
# window-less Windows build is genuinely what you want.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
version="${1:-}"
shift || true

if [ -z "$version" ]; then
   echo "usage: $0 <version> [--linux-only] [--windows-no-gui]" >&2
   exit 1
fi

linux_only=0
windows_no_gui=0
for arg in "$@"; do
   case "$arg" in
      --linux-only)     linux_only=1 ;;
      --windows-no-gui) windows_no_gui=1 ;;
      *) echo "unknown option: $arg" >&2; exit 1 ;;
   esac
done

# The Windows Cairo. Built once into winbuild/cairo-mingw by setup-winbuild.sh
# and found there automatically; VERDALIS_WIN_CAIRO overrides for a copy kept
# somewhere else.
win_cairo="${VERDALIS_WIN_CAIRO:-${here}/winbuild/cairo-mingw}"
[ -n "$win_cairo" ] && win_cairo="$(cd "$win_cairo" 2>/dev/null && pwd || echo "$win_cairo")"

stage="${here}/dist/verdalis-suite-${version}"
out_dir="${here}/dist"

# ---------------------------------------------------------------- plugin list
#
# Every subdirectory with a CMakeLists.txt is a plugin, minus the few that are
# not. Keeping this discovered rather than hardcoded means a new plugin joins a
# release by existing; "shared" carries a CMakeLists.txt of its own and is a
# library, not a plugin.
plugins=()
for d in "${here}"/*/; do
   d="${d%/}"
   name="$(basename "$d")"
   case "$name" in
      CLAP|dist|shared|winbuild|_designs) continue ;;
   esac
   [ -f "${d}/CMakeLists.txt" ] || continue
   # What actually makes it a plugin: a CLAP entry point of its own.
   [ -f "${d}/src/plugin.cpp" ] || continue
   plugins+=("$name")
done

if [ ${#plugins[@]} -eq 0 ]; then
   echo "no plugins found in ${here}" >&2
   exit 1
fi

# CMake project name, e.g. rainyday -> RainyDay. Read from the CMakeLists rather
# than guessed, because the folder is lowercase and the artifact is not.
project_name() {
   sed -n 's/^project(\([A-Za-z0-9_]*\).*/\1/p' "${here}/$1/CMakeLists.txt" | head -1
}
project_version() {
   sed -n 's/^project([A-Za-z0-9_]* VERSION \([0-9.]*\).*/\1/p' "${here}/$1/CMakeLists.txt" | head -1
}

generator=()
command -v ninja >/dev/null 2>&1 && generator=(-G Ninja)

echo "==> Verdalis suite ${version}"
echo "    plugins: ${plugins[*]}"

# ------------------------------------------------------------ windows preflight
build_windows=1
if [ "$linux_only" = 1 ]; then
   build_windows=0
   echo "    windows: skipped (--linux-only)"
elif ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
   echo "!!  x86_64-w64-mingw32-g++ not found -- skipping the Windows build." >&2
   echo "    Install mingw-w64, or pass --linux-only to silence this." >&2
   build_windows=0
elif [ -z "$win_cairo" ] || [ ! -f "${win_cairo}/lib/libcairo.a" ]; then
   if [ "$windows_no_gui" = 1 ]; then
      echo "!!  No Windows Cairo -- Windows plugins will have NO plugin window." >&2
      win_cairo=""
   else
      echo "!!  No Windows Cairo at ${win_cairo}" >&2
      echo "    Windows plugins would be built without a plugin window, which is" >&2
      echo "    not something to ship by accident. Build it once with:" >&2
      echo "      ./setup-winbuild.sh" >&2
      echo "    or re-run with --linux-only, or --windows-no-gui to accept it." >&2
      exit 1
   fi
fi

rm -rf "$stage"
mkdir -p "$stage"

# ------------------------------------------------------------------ the builds
build_one() {
   local plugin="$1" target="$2" build_dir="$3" install_root="$4"
   local name upper
   name="$(project_name "$plugin")"
   upper="$(echo "$plugin" | tr '[:lower:]' '[:upper:]')"

   local args=(
      -S "${here}/${plugin}"
      -B "$build_dir"
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX="$install_root"
   )
   # The offline tools are development aids and are not part of a release.
   args+=("-D${upper}_BUILD_TOOLS=OFF")

   if [ "$target" = windows ]; then
      args+=(-DCMAKE_TOOLCHAIN_FILE="${here}/shared/cmake/mingw-w64-x86_64.cmake")
      [ -n "$win_cairo" ] && args+=("-D${upper}_WIN_CAIRO=${win_cairo}")
   fi

   echo "--> ${name} (${target})"
   cmake "${generator[@]}" "${args[@]}" >/dev/null
   cmake --build "$build_dir" --parallel "$(nproc)" >/dev/null
   cmake --install "$build_dir" >/dev/null
}

work="${here}/dist/.build"
rm -rf "$work"

for plugin in "${plugins[@]}"; do
   build_one "$plugin" linux "${work}/${plugin}-linux" "${stage}/linux"
   if [ "$build_windows" = 1 ]; then
      build_one "$plugin" windows "${work}/${plugin}-windows" "${stage}/windows"
   fi
done

# --------------------------------------------------------------- archive extras
cp "${here}/README.md" "${here}/LICENSE" "$stage/"

cat > "${stage}/INSTALL.txt" <<'TXT'
Verdalis Plugin Suite
=====================

CLAP hosts load plugins from a fixed location. Copy the plugin folders from
this archive into it, keeping each plugin's folder and its presets/ together.

Linux
  Copy the contents of linux/ into:
      ~/.clap/
  so that you end up with ~/.clap/RainyDay/RainyDay.clap and so on.

Windows
  Copy the contents of windows/ into:
      C:\Program Files\Common Files\CLAP\
  so that you end up with ...\CLAP\RainyDay\RainyDay.clap and so on.

Then rescan plugins in your host. Tested with Bitwig Studio and Reaper.
TXT

{
   echo "Verdalis Plugin Suite ${version}"
   echo "built $(date -u '+%Y-%m-%d %H:%M UTC') on $(uname -srm)"
   echo
   echo "plugins:"
   for plugin in "${plugins[@]}"; do
      echo "  $(project_name "$plugin") $(project_version "$plugin")   (${plugin}/)"
   done
   echo
   echo "targets:"
   echo "  linux/    x86_64, GUI via X11 + Cairo"
   if [ "$build_windows" = 1 ]; then
      if [ -n "$win_cairo" ]; then
         echo "  windows/  x86_64 (mingw-w64), GUI via win32 + Cairo"
      else
         echo "  windows/  x86_64 (mingw-w64), NO PLUGIN WINDOW"
      fi
   else
      echo "  windows/  not built"
   fi
} > "${stage}/BUILD-INFO.txt"

# ----------------------------------------------------------------------- pack
cd "$out_dir"
base="verdalis-suite-${version}"
rm -f "${base}.tar.gz" "${base}.zip"
tar czf "${base}.tar.gz" "$base"
command -v zip >/dev/null 2>&1 && zip -qr "${base}.zip" "$base"
rm -rf "$work"

echo
echo "==> ${out_dir}/${base}.tar.gz"
[ -f "${out_dir}/${base}.zip" ] && echo "==> ${out_dir}/${base}.zip"
echo
cat "${stage}/BUILD-INFO.txt"
