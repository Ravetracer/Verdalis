#!/usr/bin/env bash
#
# Creates the local Python environment the analysis tools use for pYIN.
#
# Most of tools/analysis needs nothing but numpy, deliberately -- the same rule
# the rest of the suite follows. Two things want more: `contours.py` gets a
# better fundamental out of librosa's pYIN than out of its own peak tracker,
# and that is what decides how many of the library's syllables are usable as
# archetypes. It is an optional import: without this venv the tools all still
# run, on the numpy tracker.
#
# A venv rather than a plain pip install because system Python on current
# Debian and Ubuntu refuses one (PEP 668) -- the same reason
# ../../setup-winbuild.sh puts meson in one.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python3 -m venv "${here}/.venv"
"${here}/.venv/bin/pip" install --quiet --upgrade pip
"${here}/.venv/bin/pip" install --quiet librosa soundfile

echo "analysis venv ready:"
"${here}/.venv/bin/python" -c "import librosa; print('  librosa', librosa.__version__)"
echo
echo "Run the tools with it where pYIN is wanted:"
echo "  tools/analysis/.venv/bin/python tools/analysis/contours.py --emit"
