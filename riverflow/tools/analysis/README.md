# RiverFlow analysis

Every default and every factory preset in RiverFlow comes from measurements of a
reference library of **77 field recordings** of rivers, creeks and waterfalls:
48 or 96 kHz, mostly stereo, 52 minutes in total. The recordings themselves are
not part of this repository and are not ours to redistribute; they live in
`!dev/references`, which is gitignored, and are read, never copied.

The scripts here read that directory and print the numbers below. `wavio.py`
comes from `shared/tools/analysis`.

## The finding the whole plugin rests on

The author's brief for this plugin says *"some rivers and waterfalls just sound
like white noise but they aren't"*. Half of that is measurably wrong, and it is
the more useful half.

`grain.py` measures the coefficient of variation of each recording's 4 ms
envelope, band by band, **beside a Gaussian white-noise control of the same
length put through the same measurement**. The control row is the whole point:
a flux-based onset detector finds "events" in band-limited noise at a steady ten
to twenty a second, so any event rate quoted for a river is meaningless without
knowing what the same detector says about noise.

| | 200-800 Hz | 0.8-2 kHz | 2-6 kHz | 6-14 kHz |
|---|---|---|---|---|
| **Gaussian control** | 0.30 | 0.22 | 0.12 | 0.09 |
| library minimum | 0.30 | 0.22 | 0.13 | 0.11 |
| library median | 0.39 | 0.33 | 0.30 | 0.29 |
| library maximum | 1.01 | 0.93 | **1.64** | 1.05 |

The smoothest third of the library **lands on the control row**. Not "close to
it": `forest_river_atmo`, `forest_wildriver_atmo`, `river_wilder_fartheraway`,
`wissahickon-creek-3` and `waterfall-saas-fee-gletschersee-bridge-big` are
statistically indistinguishable from shaped Gaussian noise by these statistics,
and `events.py` finds too few discrete events in them to characterise at all.
Those rivers *are* white noise, spectrally shaped. The bed alone is a complete
and correct model of them.

The other end of the library is 2 to 13 times the control, and the band it
happens in says what kind of water it is:

- **200-800 Hz** — `creek-02-loop` (1.01), `creek-ambience-2` (0.86),
  `creek-05-loop` (0.86). Large air pockets: water folding over stones.
- **2-6 kHz** — `waterfall-saas-fee-hannig-trickle` (**1.64** against a control
  of 0.12), `waterfall_small_bubbly` (1.10). Single drops on wet stone.

So the design is a bed plus two populations of discrete events, and the whole
span the plugin covers is the ratio between them. That is the brief's other
half, and it holds exactly.

## What was measured, and what it decided

| Quantity | Measured across the library | What it set |
|---|---|---|
| Octave-band colour, 125 Hz-16 kHz | six clusters (`shapes.py`) | the six `Water` shapes, shipped as cluster centroids |
| Sub-60 Hz energy | up to **24%** of total, and uncorrelated with the water above it in every file (\|r\| < 0.15) | nothing below 60 Hz is generated; `Highpass` defaults there |
| PSD slope 60-400 Hz | −7.0 … +15.1, median **+2.2** dB/oct | the shapes' lower skirts |
| PSD slope 400 Hz-2 kHz | −8.5 … +3.1, median **−1.4** dB/oct | their plateau |
| PSD slope 2-16 kHz | −13.7 … +0.4, median **−7.0** dB/oct | their upper skirts |
| Spectral centroid | 351 … 12336 Hz, median **2146** | the brightness range the shapes have to span |
| 90% rolloff | 905 … 20068 Hz, median 5348 | as above |
| Crest factor | 12.8 … 32.5 dB, median **19.6** | the event layers' level, and the log-normal spread on it |
| Envelope CV at 50 ms | 0.03 … 0.44, median **0.12** | rivers are steady; far steadier than surf's 0.10-1.86 |
| Envelope CV at 4 ms | 0.09 … 0.82, median **0.26** | `Grain` |
| Envelope spectrum, 0.3-10 Hz | **−1.55 dB/decade**, no peak that survives between recordings | the surge is a filtered random walk, **not an LFO** |
| Surge depth at 1-2 kHz, 100 ms | 5 … 35%, median **11%** | `Turbulence` |
| Surge depth per band, rel. 1-2 kHz | 125-250 Hz **2.43x**, falling to 0.84x at 4-8 kHz | `kSurgeDepth`: a river's low end surges while its hiss sits still |
| Band-to-band envelope correlation | **0.07 … 0.25** | the bands wander independently; each gets its own walk *and* its own noise |
| Dabble rate (150 Hz-1.5 kHz, 4 MAD) | 1.4 … 9.3/s, median **5.5** | `Dabble Rate` |
| Dabble pitch | 562 … 1406 Hz, median **984** | `Dabble Size`, via Minnaert |
| Dabble radius from that | 2.3 … 5.8 mm, median **3.3** | 3260/3.3 = 988 Hz, so the two agree to 1% |
| Dabble spectral Q | 0.7 … 5.0 → a ring of 0.4-1.1 ms | — |
| Dabble envelope, −10 dB | **7 … 34 ms**, median 25 | `Cluster` and `Spill`: one pocket cannot do both, so a dabble is several |
| Dabble prominence over the bed | 5.5 … 16.7 dB, median 9.4 | `Dabble Level` |
| Trickle rate (1.8-16 kHz) | 5.0 … 25.5/s, median **16** | `Trickle Rate` |
| Trickle pitch | 1.9 … 7.8 kHz, median **2625** | `Trickle Size` |
| Trickle radius from that | 0.42 … 1.72 mm, median **1.24** | 3260/1.24 = 2629 Hz, again to 1% |
| Trickle envelope, −10 dB | 8 … 86 ms, median 13 — bimodal | `Trickle Decay` and `Splash` |
| Trickle prominence | 2.2 … 11.7 dB | `Trickle Level` |
| L/R correlation | −0.33 … 1.00, median **0.60** | `Flow Width` (the bed's correlation is 1 − it) and `Width` |

## The low end is the weather, not the river

`lowend.py` exists because six recordings carry 30-60 Hz energy within 10 dB of
their loudest band, and one of them — `waterfall-saas-fee-gletschersee-04` —
puts 24% of its total energy below 60 Hz. By Minnaert, 32 Hz is a 100 mm air
pocket: conceivable in the plunge pool under a big fall, out of the question in
a creek.

The two candidates are distinguishable. Water in the low band is made by the
same events as the water above it, so its envelope correlates with the mid
band's; wind and handling noise on the microphone know nothing about the water.

| reference | corr(20-60 Hz, 1-4 kHz) | energy < 60 Hz |
|---|---|---|
| waterfall-saas-fee-gletschersee-04 | −0.01 | 23.8% |
| waterfall-saas-fee-hannig-wald | 0.04 | 21.1% |
| creek-ambience-2 | 0.05 | 12.8% |
| creek-ambience | 0.02 | 6.6% |
| mountain-river-002 | −0.04 | 0.00% |

Zero correlation in every case, including the ones where it is a fifth of the
energy. It is the recordist's afternoon, not the river. So the shape clustering
runs from 125 Hz up, the engine generates nothing below 60 Hz, and `Highpass`
defaults to 60 Hz for a documented reason rather than a cautious one.

## The six shapes

`shapes.py --k 6 --emit` clusters the 77 recordings by octave-band colour, each
normalised to its own loudest band so that loudness plays no part, and prints
the centroids as the C table the engine ships. In dB relative to each cluster's
peak band:

| | 125 | 250 | 500 | 1k | 2k | 4k | 8k | 16k | members |
|---|---|---|---|---|---|---|---|---|---|
| Deep Rush | −6.2 | −3.2 | −1.3 | −1.5 | −5.6 | −10.5 | −14.8 | −24.5 | 9 |
| Rapids | −8.5 | −4.7 | −1.5 | −0.7 | −2.6 | −5.5 | −8.4 | −14.2 | 15 |
| Mountain River | −22.2 | −9.2 | −2.6 | −0.8 | −1.7 | −6.5 | −11.8 | −19.0 | 15 |
| Stream | −15.2 | −12.1 | −6.5 | −1.5 | −1.2 | −2.2 | −4.6 | −10.3 | 27 |
| Creek | −31.2 | −21.0 | −7.8 | −2.8 | −2.7 | −1.1 | −2.7 | −17.6 | 7 |
| Trickle | −14.4 | −15.8 | −12.3 | −9.8 | −7.7 | −6.2 | −5.9 | −0.5 | 4 |

Trickle is the only one that rises all the way to the top, and its four members
are the four recordings the author's brief singles out.

A table of eight numbers is a formula, not a sample. See the suite's note on
what pure synthesis does and does not forbid.

## Making the bed produce them

`bedfit.py` renders the bed alone at each shape and compares. Getting from
"set each band to its target" to "the sum is the target" took four separate
corrections, each found by measurement and each worth recording because the
same mistakes are easy to repeat:

1. **The bands' energies must add, so their drives must be independent.** All
   eight bands were first driven from one noise sample per channel. Overlapping
   bandpasses fed the same signal add *coherently*, and the cross terms between
   neighbours are large by design, so the solved gains and the rendered result
   disagreed by up to 7 dB in the outermost octave. One `white()` per band per
   channel fixed it — and it is the more faithful arrangement anyway, since the
   measured band-to-band envelope correlation is 0.07-0.25.
2. **Octave-band energy is not spectral density at a band centre.** An octave's
   width is proportional to its centre, so matching the centre value tilts the
   whole bed up by 3 dB/octave. The engine integrates each band's response
   across each band instead, on a grid fixed at `prepare()`.
3. **The skirt filters have to be inside that integral, not corrected for.**
   Three shapes fall 10-15 dB across their outermost octave, which is steeper
   than an octave-wide bandpass can manage — it leaks 6-8 dB into its
   neighbour. A two-pole filter at the outer band edge makes that skirt. But
   dividing the *target* by the filter's response asks the bank for exactly the
   gain the filter removes, so the two cancel and the skirt never appears. The
   filter belongs in the model of what the bank can do.
4. **The drive's spectral density belongs in the normalisation.** Leaving it out
   is a 30 dB level error with the shape exactly right, which is the kind of
   mistake no amount of looking at a spectrum reveals.

What is left, with the seed pinned and every event layer off:

| shape | worst octave-band error |
|---|---|
| Rapids | 0.8 dB |
| Stream | 0.8 dB |
| Trickle | 0.9 dB |
| Mountain River | 1.6 dB |
| Creek | 5.0 dB (16 kHz band only; ≤ 2.2 dB elsewhere) |
| Deep Rush | 5.2 dB (16 kHz band only; ≤ 0.6 dB elsewhere) |

The two residuals are the same limitation: an octave-wide bank plus one
two-pole skirt cannot put a 15 dB step between the last two bands. The rendered
spectra are smooth — no comb ripple at octave spacing, checked at third-octave
resolution — which was the risk of narrowing the bands to fix it.

## The engine's own calibration

The preset fitter needs to invert the engine, so the engine was swept one
parameter at a time and the band statistics read back off the render. These are
the numbers `fitpresets.py` uses:

`Grain`, 6-14 kHz band cv (the layer barely touches this band, so it is the
clean indicator):

| Grain | 0 | 0.30 | 0.60 | 0.85 | 1.00 |
|---|---|---|---|---|---|
| 6-14 kHz cv | 0.13 | 0.17 | 0.24 | 0.25 | 0.25 |

`Dabble Level`, 200-800 Hz cv above a baseline of 0.32, and `Trickle Level`,
2-6 kHz cv above 0.26:

| dB | −34 | −28 | −22 | −16 | −10 |
|---|---|---|---|---|---|
| dabble, 200-800 cv | 0.33 | 0.36 | 0.52 | 0.85 | — |
| trickle, 2-6 kHz cv | 0.26 | 0.26 | 0.28 | 0.42 | 0.70 |

Both fit `excess = K * 10^(dB/10)` with K = 31.7 and 6.4 respectively, which is
what the fitter inverts.

## Two things the sweeps caught

**Event levels needed a tail.** With every band statistic at its measured value
the renders were still 4 dB short of the references' crest factor. Event
amplitudes were drawn uniformly, and a uniform draw has no tail; a real event's
energy follows the volume of water in it, which is log-normal. A 0.9-octave
log-normal multiplier with unit mean took the crest factor from 15.4 to 17.6 and
then to 19.5 against the library's 19.6.

**The last 2 dB of that was the soft clipper, not the model.** At unity output
gain the default patch peaked at exactly 1.000 and measured a crest factor of
17.3; the same render 6 dB down measured 19.5. The layer defaults are now set so
the default patch peaks around −2 dBFS.

## The default patch, against the library median

Rendered from `stream`-class defaults with the seed pinned:

| | render | library median |
|---|---|---|
| spectral centroid | 2095 Hz | 2146 Hz |
| crest factor | 19.5 dB | 19.6 dB |
| envelope CV, 50 ms | 0.12 | 0.12 |
| envelope CV, 4 ms | 0.23 | 0.26 |
| L/R correlation | 0.57 | 0.60 |
| band cv, 200-800 Hz | 0.39 | 0.39 |
| band cv, 0.8-2 kHz | 0.31 | 0.33 |
| band cv, 2-6 kHz | 0.27 | 0.30 |
| band cv, 6-14 kHz | 0.25 | 0.29 |
| PSD slope, 400 Hz-2 kHz | −0.9 dB/oct | −1.4 dB/oct |

## Fitting the presets

`fitpresets.py` fits each factory preset to one named reference and writes the
file. What is measured is measured; `bank_type`, the space controls, the plunge
pool and `Distance` are chosen, and the script says why in its own docstring.

Distance deserves the note. A distant river and a dark river have the same
spectrum, so fitting distance from the top-octave slope returns nearly zero for
every reference — the shape match has already absorbed whatever the air did. It
is therefore chosen from what the recording is of, and then *deconvolved out of
the target* before the shape is matched, so that putting it back reproduces the
reference instead of darkening it twice. The inversion is only stable while the
air has not already taken the top away, so the chosen distances stay at or below
0.32.

The events then add energy the bed's shape was already fitted to carry, so the
first pass comes out brighter than its reference. The bed's four degrees of
freedom cannot absorb a per-band correction, but the error is mostly a slope and
a low-end offset, and those they can: `--iterate` renders what it wrote,
measures the band error, folds it back into `Tilt` and `Body`, and writes again.
The correction is bounded at ±0.35, so a preset that genuinely cannot be reached
stays sane instead of shipping with a 6 dB/octave tilt on it.

`fit.py` then checks the result. Of the 20 presets, **15 match their reference
within 6 dB in every octave band**; 5 have at least one band out, worst 17 dB.
The five are all the same case: their references peak in the 16 kHz octave,
which is the band the bed reaches least well.

Recipe:

```sh
cd tools/analysis
python3 refs.py                       # the library survey
python3 grain.py                      # graininess against the noise control
python3 shapes.py --k 6 --emit        # the shapes, as the C table
python3 lowend.py <files>             # is the low end water, or the microphone
python3 events.py --band low  <files> # what one dabble is
python3 events.py --band high <files> # what one drop is
python3 bandsurge.py <files>          # per-band surge depth and independence
python3 surge.py <files>              # rhythm or random walk

# fit and verify the preset library
B=../../build
python3 fitpresets.py --iterate 3 --render $B/riverflow-render --clap $B/RiverFlow.clap
cmake --build $B                      # re-embed the presets
$B/riverflow-render --plugin $B/RiverFlow.clap --all --outdir /tmp/wav \
    --seconds 20 --tail 1 --rate 48000 --param randomseed=7
python3 fit.py /tmp/wav
python3 bedfit.py <dir-of-bed-only-renders>
```

`--param` matches on a parameter's **display** name with spaces removed, and a
percentage parameter takes a percentage: `--param grain=85`, not `grain=0.85`.
An hour went into a sweep that did nothing because of that.
