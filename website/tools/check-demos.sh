#!/usr/bin/env bash
#
# Numeric sanity check of rendered demo FLACs:
#
#   website/tools/check-demos.sh [website/demos/<slug> ...]
#
# Per file: format, duration, integrated loudness, true peak, sample peak, DC
# offset, samples at full scale, the loudest and quietest 1 s windows, and the
# level of the last 0.25 s. Flags anything silent, clipped, offset, malformed
# or cut off.
set -euo pipefail
export LC_ALL=C
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
dirs=("$@")
[ ${#dirs[@]} -gt 0 ] || dirs=("${here}/../demos"/*/)

for d in "${dirs[@]}"; do
   echo "== $(basename "$d")"
   for f in "$d"/*.flac; do
      sr="$(ffprobe -v error -select_streams a:0 -show_entries stream=sample_rate -of csv=p=0 "$f")"
      ch="$(ffprobe -v error -select_streams a:0 -show_entries stream=channels -of csv=p=0 "$f")"
      bits="$(ffprobe -v error -select_streams a:0 -show_entries stream=bits_per_raw_sample -of csv=p=0 "$f")"
      dur="$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$f")"
      ebu="$(ffmpeg -nostats -hide_banner -i "$f" -filter_complex ebur128=peak=true -f null - 2>&1 |
         awk '/Integrated loudness:/{s=1} s&&/^ *I: /{i=$2} /True peak:/{p=1} p&&/^ *Peak: /{t=$2} END{print i, t}')"
      stats="$(ffmpeg -nostats -hide_banner -i "$f" -ac 1 -f s16le - 2>/dev/null | python3 -c '
import sys, array, math
d = array.array("h"); d.frombytes(sys.stdin.buffer.read())
n = len(d); sr = 48000
peak = max(abs(x) for x in d) / 32768
dc = sum(d) / max(1, n) / 32768
full = sum(1 for x in d if x >= 32767 or x <= -32768)
def rms(a):
    return 20 * math.log10(max(1e-9, math.sqrt(sum(x * x for x in a) / max(1, len(a))) / 32768))
wins = [rms(d[i:i + sr]) for i in range(0, n - sr + 1, sr)] or [rms(d)]
end = rms(d[-sr // 4:])
print("%.2f %.5f %d %.1f %.1f %.1f" % (20 * math.log10(max(peak, 1e-9)), dc, full, max(wins), min(wins), end))')"
      read -r lufs tp <<<"$ebu"
      read -r pk dc full wmax wmin endl <<<"$stats"
      flags=""
      [ "$sr" = 48000 ] && [ "$ch" = 2 ] && [ "$bits" = 16 ] || flags+=" FORMAT"
      python3 -c "import sys; sys.exit(0 if float('$wmax') > -40 else 1)" || flags+=" SILENT"
      python3 -c "import sys; sys.exit(0 if float('$tp') <= -0.95 else 1)" || flags+=" TRUEPEAK"
      [ "$full" -eq 0 ] || flags+=" CLIP"
      python3 -c "import sys; sys.exit(0 if abs(float('$dc')) < 0.002 else 1)" || flags+=" DC"
      python3 -c "import sys; sys.exit(0 if float('$endl') < float('$wmax') - 30 else 1)" || flags+=" ENDS-LOUD"
      python3 -c "import sys; sys.exit(0 if 8 <= float('$dur') <= 40 else 1)" || flags+=" LENGTH"
      printf '  %-34s %5.1fs %6s LUFS %5s dBTP pk %6s dc %8s fs %d  1s-rms %5s..%6s  end %6s %s\n' \
         "$(basename "$f")" "$dur" "$lufs" "$tp" "$pk" "$dc" "$full" "$wmax" "$wmin" "$endl" "${flags:- ok}"
   done
done
