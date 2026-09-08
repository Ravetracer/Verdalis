#!/usr/bin/env bash
#
# Builds every plugin in the suite and packs one "verdalis-suite" archive
# containing the Linux and Windows binaries plus everything needed to install
# them.
#
#   ./release.sh 0.1.0                  Linux + Windows (needs cross-built Cairo)
#   ./release.sh 0.1.0 --linux-only     skip the Windows half
#   ./release.sh 0.1.0 --tarball        also emit .tar.gz beside the .zip files
#   ./release.sh 0.1.0 --no-manuals     do not build or ship the PDF manuals
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

# Each build step sends its normal output to /dev/null so the log stays short.
# That makes a failure look like the script simply stopped, so say what died.
step=""
trap 'rc=$?; [ $rc -eq 0 ] || echo "!!  failed during: ${step:-startup} (exit $rc)" >&2' EXIT

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
version="${1:-}"
shift || true

if [ -z "$version" ]; then
   echo "usage: $0 <version> [--linux-only] [--windows-no-gui] [--tarball] [--no-manuals]" >&2
   exit 1
fi

linux_only=0
windows_no_gui=0
tarball=0
no_manuals=0
for arg in "$@"; do
   case "$arg" in
      --linux-only)     linux_only=1 ;;
      --windows-no-gui) windows_no_gui=1 ;;
      --tarball)        tarball=1 ;;
      --no-manuals)     no_manuals=1 ;;
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
   # `plugin` is a loop variable in the callers too, and a function that
   # walks the plugin list without declaring it local leaves the caller
   # iterating the last entry instead of its own.
   local plugin
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
   step="${name} (${target}) configure"
   cmake "${generator[@]}" "${args[@]}" >/dev/null
   step="${name} (${target}) build"
   cmake --build "$build_dir" --parallel "$(nproc)" >/dev/null
   step="${name} (${target}) install"
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

# ----------------------------------------------------------------- manuals
#
# One PDF per plugin, generated from <plugin>/docs/manual.md with the parameter
# reference and the preset library read out of the plugin itself. They go into
# that plugin's own archives and into a manuals/ folder in the suite archives.
#
# Documentation is not worth failing a release over: if the toolchain for it is
# not installed, this says so and the archives simply go out without manuals.
manual_dir="${here}/dist/manuals"
rm -rf "$manual_dir"

manual_for() {
   local plugin
   echo "${manual_dir}/$(project_name "$1")-$(project_version "$1")-Manual.pdf"
}

if [ "$no_manuals" = 1 ]; then
   echo "    manuals: skipped (--no-manuals)"
else
   echo "--> manuals"
   for plugin in "${plugins[@]}"; do
      step="$(project_name "$plugin") manual"
      if [ ! -f "${here}/${plugin}/docs/manual.md" ]; then
         echo "!!  ${plugin} has no docs/manual.md -- shipping it without a manual" >&2
         continue
      fi
      if ! "${here}/shared/tools/make-manual.sh" "$plugin" "$manual_dir"; then
         echo "!!  the ${plugin} manual could not be built -- shipping it without one" >&2
      fi
   done
fi

# ------------------------------------------------------------- install notes
#
# Written per operating system, because someone downloading the Windows build
# should not have to read past the Linux instructions to find theirs.
install_note() {
   local os="$1" what="$2" prefix="$3"
   if [ "$os" = linux ]; then
      cat <<TXT
${what}
$(printf '=%.0s' $(seq ${#what}))

CLAP hosts load plugins from ~/.clap on Linux.

Copy ${prefix} into:

    ~/.clap/

so that you end up with ~/.clap/RainyDay/RainyDay.clap and so on. Keep each
plugin's folder and its presets/ together: a plugin finds its factory presets by
looking for a presets directory next to its own binary.

Then rescan plugins in your host. Tested with Bitwig Studio and Reaper.

Needs X11 and Cairo, which any Linux machine that can run a DAW already has.
TXT
   else
      cat <<TXT
${what}
$(printf '=%.0s' $(seq ${#what}))

CLAP hosts load plugins from the common CLAP folder on Windows.

Copy ${prefix} into:

    C:\\Program Files\\Common Files\\CLAP\\

so that you end up with ...\\CLAP\\RainyDay\\RainyDay.clap and so on. Keep each
plugin's folder and its presets\\ together: a plugin finds its factory presets by
looking for a presets directory next to its own binary.

Then rescan plugins in your host.

Nothing else is needed: the plugin window and its Cairo are linked in, so there
are no DLLs to install beside it.
TXT
   fi
}

build_info() {
   local plugin
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
}

# --------------------------------------------------------------- archive extras
#
# A plugin may carry licence terms of its own on top of the suite's -- an
# embedded third-party asset with an attribution requirement, for instance --
# and those terms have to travel with anything that redistributes it. Where a
# plugin's LICENSE differs from the suite's, its own copy is what goes into its
# archives, and the suite archives carry it alongside as LICENSE-<Name>.
plugin_license() {
   if [ -f "${here}/$1/LICENSE" ] && ! cmp -s "${here}/$1/LICENSE" "${here}/LICENSE"; then
      printf '%s\n' "${here}/$1/LICENSE"
   fi
}

copy_extra_licenses() {
   local d="$1" p extra
   for p in "${plugins[@]}"; do
      extra="$(plugin_license "$p")"
      if [ -n "$extra" ]; then
         cp "$extra" "${d}/LICENSE-$(project_name "$p")"
      fi
   done
}

cp "${here}/README.md" "${here}/LICENSE" "$stage/"
copy_extra_licenses "$stage"

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

for plugin in "${plugins[@]}"; do
   manual="$(manual_for "$plugin")"
   [ -f "$manual" ] || continue
   mkdir -p "${stage}/manuals"
   cp "$manual" "${stage}/manuals/"
done

build_info > "${stage}/BUILD-INFO.txt"

# ----------------------------------------------------------------------- pack
#
# One archive per plugin per operating system, so a site can offer a plain
# "Windows download" beside a "Linux download", plus the whole suite the same
# way and a single archive with everything in it.
#
# Everything is a .zip, including the Linux builds. That is not the Unix habit,
# but a release nobody can publish is worse than one in the wrong format, and
# download managers commonly handle zip alone. Nothing is lost by it: zip records
# Unix permissions and a .clap is dlopen'd, which needs no execute bit. Pass
# --tarball to get .tar.gz alongside for anywhere that prefers it.
step="packing"
cd "$out_dir"

made=()

pack() {
   local dir="$1"
   if command -v zip >/dev/null 2>&1; then
      rm -f "${dir}.zip"; zip -qr "${dir}.zip" "$dir"; made+=("${dir}.zip")
   else
      echo "!!  zip not found: install it, or the release cannot be published" >&2
   fi
   if [ "$tarball" = 1 ]; then
      rm -f "${dir}.tar.gz"; tar czf "${dir}.tar.gz" "$dir"; made+=("${dir}.tar.gz")
   fi
   rm -rf "$dir"
}

targets=(linux)
[ "$build_windows" = 1 ] && targets+=(windows)

# --- one per plugin, per operating system
for plugin in "${plugins[@]}"; do
   name="$(project_name "$plugin")"
   pver="$(project_version "$plugin")"
   for os in "${targets[@]}"; do
      [ -d "${stage}/${os}/${name}" ] || continue
      d="${name}-${pver}-${os}-x86_64"
      rm -rf "$d"; mkdir -p "$d"
      cp -r "${stage}/${os}/${name}" "${d}/"
      own="$(plugin_license "$plugin")"
      cp "${own:-${here}/LICENSE}" "${d}/LICENSE"
      [ -f "${here}/${plugin}/README.md" ] && cp "${here}/${plugin}/README.md" "${d}/"
      manual="$(manual_for "$plugin")"
      [ -f "$manual" ] && cp "$manual" "${d}/"
      install_note "$os" "${name} ${pver}" "the ${name} folder" > "${d}/INSTALL.txt"
      build_info > "${d}/BUILD-INFO.txt"
      pack "$d"
   done
done

# --- the whole suite, per operating system
for os in "${targets[@]}"; do
   [ -d "${stage}/${os}" ] || continue
   d="verdalis-suite-${version}-${os}-x86_64"
   rm -rf "$d"; mkdir -p "$d"
   cp -r "${stage}/${os}/"* "${d}/"
   cp "${here}/README.md" "${here}/LICENSE" "${d}/"
   copy_extra_licenses "$d"
   for plugin in "${plugins[@]}"; do
      manual="$(manual_for "$plugin")"
      [ -f "$manual" ] || continue
      mkdir -p "${d}/manuals"
      cp "$manual" "${d}/manuals/"
   done
   install_note "$os" "Verdalis Plugin Suite ${version}" "every plugin folder in this archive" \
      > "${d}/INSTALL.txt"
   build_info > "${d}/BUILD-INFO.txt"
   pack "$d"
done

# --- and everything at once, both platforms. Kept out of pack() because the
# staging tree it names is also where BUILD-INFO.txt is read from below.
base="verdalis-suite-${version}"
if command -v zip >/dev/null 2>&1; then
   rm -f "${base}.zip"; zip -qr "${base}.zip" "$base"; made+=("${base}.zip")
fi
if [ "$tarball" = 1 ]; then
   rm -f "${base}.tar.gz"; tar czf "${base}.tar.gz" "$base"; made+=("${base}.tar.gz")
fi
rm -rf "$work"

echo
echo "==> ${out_dir}"
for f in "${made[@]}"; do
   printf "    %-46s %s\n" "$f" "$(du -h "$f" | cut -f1)"
done
echo
cat "${stage}/BUILD-INFO.txt"
