# Matching RainyDay against real rain

These scripts measure a set of reference recordings and RainyDay's own output
with the same code, then move preset values until the two agree. They are how
the factory library was tuned, and how it should be re-tuned whenever the
engine changes.

Nothing here ships with the plugin. It needs `numpy` and a built plugin plus
`rainyday-fithost` (`./install.sh` produces both).

## Use

```sh
export RAINYDAY_SOUNDS=/path/to/reference/wavs
python3 tools/analysis/match.py report                 # how far off is the library
python3 tools/analysis/match.py fit --out /tmp/tuned   # tune preset values
python3 tools/analysis/match.py loudness --src /tmp/tuned --out presets
```

The recordings are not in the repository. Supply your own mono WAVs named after
the keys in `pairs.py` — `rain_on_concrete.wav`, `multiple_water_drops.wav` and
so on. Any sample rate and any of 16/24/32-bit PCM or 32-bit float will do.

## What is measured

`feat.py` computes, identically for a recording and for a render:

| Feature | What it pins down |
|---|---|
| Energy in nine log bands | Spectral shape — the spine of the match |
| Impulsiveness per band | Continuous wash against separate impacts |
| Temporal flatness per band | The texture in between those two extremes |
| Windowed crest factor | Dynamic contrast, independent of duration |
| Slow envelope modulation | Surges and lulls |
| Spectral flatness | Noisy against pitched |

Two details matter more than they look. References are **noise-floor
compensated**: a per-band floor is estimated from the 10th percentile of the
short-time energy and subtracted, because field recordings carry room tone and
preamp hiss that RainyDay is not trying to reproduce, and without this the fit
chases the hiss instead of the rain. And crest is measured over **fixed
windows**, because the peak of a 100-second recording is drawn from far more
impacts than the peak of a 7-second render, so a whole-signal crest would
compare lengths rather than textures.

## How the fit works

`fit.py` runs a coordinate descent: each parameter in turn is tried at a few
values around its current one, the best is kept, and the step shrinks each
pass. Every candidate renders with the same fixed seeds, so two candidates
differ by their parameters and not by which droplets happened to fall. Two
seeds are averaged — one is enough to rank candidates but lets the fit chase
the accidents of a single realisation. The winner is then re-scored on seeds it
never saw, and that number is the one worth believing.

Surface, Chirp, the space controls and the envelope are never fitted. Those are
what give a preset its identity; the fit is there to correct tone and texture,
not to redesign the sound.
