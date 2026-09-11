#!/bin/sh
# Prints the output gain each factory preset needs so that its true peak lands
# at -3 dBFS, which is what the GAIN table in makepresets.py holds.
#
# Rendering at a common -24 dB first is the point: a preset already clipping at
# its own gain reports a peak of 1.000 whatever its content is, so the peak has
# to be measured somewhere the output stage's soft clipper is not involved.
#
#   tools/analysis/headroom.sh [build-dir]
set -e
BUILD=${1:-build}
cd "$(dirname "$0")/../../$BUILD"
./crackleblaze-render --plugin ./CrackleBlaze.clap --all --outdir /tmp/cb-headroom \
   --seconds 25 --tail 2 --rate 48000 --param randomseed=7 --param outputgain=-24 2>&1 |
   grep dBFS |
   sed -E 's/^(.{24}) peak +[0-9.]+ \( *(-?[0-9.]+) dBFS\).*/\1 \2/' |
   awk '{p=$NF; $NF=""; g=-27-p; if (g > -3) g = -3;
         printf "%-24s peak@-24 %7.1f   gain %6.1f\n", $0, p, g}'
rm -rf /tmp/cb-headroom
