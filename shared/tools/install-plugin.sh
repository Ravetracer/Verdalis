#!/usr/bin/env bash
#
# Builds one Verdalis plugin and installs it where CLAP hosts look, so Bitwig
# Studio (and any other CLAP host) picks it up.
#
#   shared/tools/install-plugin.sh <plugin-dir>
#
# Normally reached through a plugin's own ./install.sh, which passes its own
# directory. Everything else is derived: the display name and the tool names
# come from the plugin's CMakeLists.txt, so this script never needs to know
# which plugin it is building.
#
# The install prefix is $<PLUGIN>_PREFIX if set (e.g. RAINYDAY_PREFIX), else
# $VERDALIS_PREFIX, else ~/.clap.
set -euo pipefail

plugin_dir="${1:?usage: $0 <plugin-dir>}"
plugin_dir="$(cd "$plugin_dir" && pwd)"

folder="$(basename "$plugin_dir")"
name="$(sed -n 's/^project(\([A-Za-z0-9_]*\).*/\1/p' "${plugin_dir}/CMakeLists.txt" | head -1)"
if [ -z "$name" ]; then
   echo "could not read the project name from ${plugin_dir}/CMakeLists.txt" >&2
   exit 1
fi

upper="$(echo "$folder" | tr '[:lower:]' '[:upper:]')"
# Indirect expansion, so RAINYDAY_PREFIX still works for RainyDay.
prefix_var="${upper}_PREFIX"
prefix="${!prefix_var:-${VERDALIS_PREFIX:-${HOME}/.clap}}"

build_dir="${plugin_dir}/build"

generator=()
if command -v ninja >/dev/null 2>&1; then
   generator=(-G Ninja)
fi

echo "==> configuring"
cmake -S "${plugin_dir}" -B "${build_dir}" "${generator[@]}" \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_INSTALL_PREFIX="${prefix}" \
   ${CLAP_INCLUDE_DIR:+-DCLAP_INCLUDE_DIR="${CLAP_INCLUDE_DIR}"}

echo "==> building"
cmake --build "${build_dir}" --parallel

echo "==> verifying"
"${build_dir}/${folder}-render" --plugin "${build_dir}/${name}.clap" --selftest

echo "==> installing to ${prefix}/${name}"
cmake --install "${build_dir}"

cat <<EOF

Installed:
  ${prefix}/${name}/${name}.clap
  ${prefix}/${name}/presets/   ($(ls -1 "${plugin_dir}/presets" | wc -l) presets)

Restart Bitwig Studio, or rescan plugins under
Settings -> Locations -> Plug-in Locations.
EOF
