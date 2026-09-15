#!/usr/bin/env bash
# Builds WhooshPact and installs it into ~/.clap so Bitwig Studio (and any other
# CLAP host) picks it up.
#
# The work is shared by the suite; this only says which plugin to build.
# Override the destination with WHOOSHPACT_PREFIX=/some/where ./install.sh
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${here}/../shared/tools/install-plugin.sh" "$here" "$@"
