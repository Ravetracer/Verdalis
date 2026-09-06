#!/usr/bin/env bash
# Builds ThunderClap and installs it into ~/.clap so Bitwig Studio (and any other
# CLAP host) picks it up.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${here}/build"
prefix="${THUNDERCLAP_PREFIX:-${HOME}/.clap}"

generator=()
if command -v ninja >/dev/null 2>&1; then
   generator=(-G Ninja)
fi

echo "==> configuring"
cmake -S "${here}" -B "${build_dir}" "${generator[@]}" \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_INSTALL_PREFIX="${prefix}" \
   ${CLAP_INCLUDE_DIR:+-DCLAP_INCLUDE_DIR="${CLAP_INCLUDE_DIR}"}

echo "==> building"
cmake --build "${build_dir}" --parallel

echo "==> verifying"
"${build_dir}/thunderclap-render" --plugin "${build_dir}/ThunderClap.clap" --selftest

echo "==> installing to ${prefix}/ThunderClap"
cmake --install "${build_dir}"

cat <<EOF

Installed:
  ${prefix}/ThunderClap/ThunderClap.clap
  ${prefix}/ThunderClap/presets/   ($(ls -1 "${here}/presets" | wc -l) presets)

Restart Bitwig Studio, or rescan plugins under
Settings -> Locations -> Plug-in Locations.
EOF
