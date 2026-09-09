# ChirpParade status

Version 0.4.0. The reference library grew from 58 recordings to 98, the contour
pipeline was rebuilt around what that exposed, and **Crow and Raven were
removed**.

- **40 more references**, fetched with `tools/analysis/fetch-fss.py` and
  recorded in `!dev/references/fss/PROVENANCE.tsv`. 4268 segmented syllables
  became 7965. Raven went from 6 measured syllables to 146 and Goose from 12 to
  286 — figures that were never medians of anything before.
- **The fit budget was a hidden duration filter.** `PITCH_TERMS` was a fixed 40
  regardless of syllable length, so fit error carried duration and
  `MAX_FIT_CENTS` rejected long syllables for being long: 28 of the 30 raven
  syllables over 300 ms were dropped, several with an HNR above 20 dB. It is 96
  now, chosen by how far the fitted contour travels against how far the tracked
  one does — 40 terms reproduced only 79 % of the real motion, so every contour
  in every previous version was over-smoothed.
- **`shape()` clustered on an aliased curve.** It sampled the cosine series at a
  fixed 64 points, below Nyquist for even the old 40 terms, so contours
  differing only in fine motion could collide and the medoid between them was
  arbitrary. It now samples above Nyquist.
- **Nothing checked the fitted curve.** Every gate tested the tracked contour or
  how well the series threads it, and fit error is evaluated *at* the tracked
  points — so a series oscillating between them scored perfectly. With 96 terms
  one archetype came through travelling 1089 octaves in 255 ms.
  `MAX_PATH_OCT_PER_SEC` now bounds the fitted curve at 500 oct/s, headroom over
  the 440 the library's fastest syllable measures.
- **k-medoids built an n×n×d intermediate**, which the wider `shape()` turned
  into an 8.9 GB allocation and an OOM kill. Rewritten via
  `||a-b||² = |a|² + |b|² - 2ab`; identical medoids, 23 MB.

**Crow and Raven are gone.** They were modelled, measured, refitted against
twelve new corvid recordings, and still did not sound like corvids. A crow's
roughness has structure — subharmonics, period doubling — and this engine
reproduces it as a rough *tone*. The recordings stay in the library and stay in
its census; they no longer become an instrument. `Screech` is unaffected and is
still labelled as an effect rather than a bird.

**0.3.0 presets that selected Crow or Raven do not load meaningfully**, and the
three that did have been removed. 19 factory presets ship.

Version 0.3.0 added `Partials`: each archetype carries the measured balance
between its first six harmonics across the syllable, and one control crossfades
the synthetic valve into it.

Version 0.2.0 replaced the syllable model: 0.1.0's physical syrinx, fitted
to aggregate statistics, is gone and measured contours drive the oscillator
instead. **0.1.0 presets and saved state do not load meaningfully** — the
syllable parameters changed meaning and one was replaced. Nothing was released
at 0.1.0.

## What works

- **Measured contours as the voice.** 64 archetypes, the medoids of 3567
  clustered syllables extracted from the reference library, shipped as cosine
  coefficients in `src/dsp/contours_generated.h` — 12288 floats, 48 KB, no audio.
  Refitting a real syllable lands within **41 cents of pitch and 0.5 dB of
  level**.
- **`Contour`, `Detail`, `Sweep`, `Skew`** over those curves: which archetype,
  how much of its fine motion survives, how far it travels, how its time is
  warped. `Detail` is the one that matters most — it is the control that the
  first version structurally could not have.
- **`Pitch` means the pitch you hear.** The contour is anchored at the
  syllable's loudest moment rather than at its mean, so transposing it puts the
  audible pitch where the knob says. Anchored at the mean, an arching contour
  read 31 % high.
- **The valve.** A phase accumulator at the contour's frequency through a
  one-sided soft hinge; `Voice` is the fraction of each cycle it is shut. At
  zero it passes a pure sine, which is what 59 % of the library's syllables are.
  The one-sidedness is where the even harmonics come from — a symmetric
  oscillator has none at all, which is the one thing 0.1.0's physical model got
  right and the reason it is kept.
- **The tract.** The trachea as a closed tube at `c/4L`, with `Beak` both
  raising and broadening the resonance and making it follow the pitch across the
  syllable the way a songbird's gape does.
- **Ten species, each a measurement.** Pitch register, archetype set, syllable
  length, harmonic richness, roughness and rate. `Pitch` at its default is the
  library median, so every species at the default sings in its own register.
- **One shot and drone at once.** A note fires a deliberate phrase at
  `Shot Level` which **always completes, however short the note**; while the
  note is held, a flock of individuals calls unprompted at `Flock Level` as a
  Poisson process, and `Answer` makes one bird's phrase provoke a reply from
  another. Either half can be turned right off.
- **Phrases as plans.** A phrase owns no oscillator and makes no sound — it
  decides when something does — which is what keeps the pool cheap enough to run
  64 syllables at once.
- **Woodpecker drumming** as a separate layer: two broad wooden modes excited by
  a contact, in a roll that accelerates by the measured 23 %.
- **Preset discovery**, state save/load, sample-accurate parameters, host
  modulation, bounded pools.
- **The window**, in ChirpParade's own finch-gold theme, with a sonogram
  scrolling across the header — one stroke per syllable, deterministic in the
  syllable number, and a few dim strokes held when nothing is playing.
- **Self-test**: 0 failures.
- **Measured partial balance.** Six amplitude curves per archetype, phase-locked
  to the same accumulator, crossfaded in by `Partials` and scaled by how much of
  that syllable's energy the harmonic comb accounted for — so an archetype with
  a second bird in it does not pretend to know. It moves a sparrow's measured
  balance drift from 1.0 to 3.2 dB, against the references' 4.4.
- **Reproducible renders**: all 19 presets byte-identical across two runs at
  44.1, 48 and 96 kHz with `Random Seed` pinned.
- **CPU**: 60 s of *Dawn Chorus* — twelve birds, 48 voices — renders in 0.90 s,
  **67× realtime**; the six extra partial oscillators cost about a quarter of
  the previous headroom. Dropping 0.1.0's ODE and its up-to-eight integration
  substeps per sample is most of that.

## What is measured

Full numbers in `tools/analysis/README.md`. 58 field recordings, **4268
segmented syllables**, of which **1641 passed the contour quality gate** and 67
became archetypes.

`fit.py --species` renders each species as one isolated syllable and measures it
back with the same estimators:

| Species | pitch got/want | length got/want | harmonics |
|---|---|---|---|
| Whistler | 3784 / 3816 (−1 %) | 72 / 143 ms | 1 / 1 |
| Sparrow | 2958 / 3148 (−6 %) | 32 / 61 ms | 1 / 1 |
| Warbler | 2541 / 2655 (−4 %) | 96 / 97 ms | 1 / 1 |
| Budgie | 473 / 1351 (−65 %) | 21 / 69 ms | 2 / 3 |
| Woodpecker | 3051 / 3121 (−2 %) | 96 / 97 ms | 2 / 1 |
| Crane | 770 / 762 (+1 %) | 155 / 156 ms | 6 / 5 |
| Goose | 697 / 743 (−6 %) | 80 / 78 ms | 3 / 4 |
| Screech | 875 / 1800 (−51 %) | 133 / 134 ms | 13 / 6 |

Six of eight within ±6 % on pitch; five of eight within ±3 % on length. Two
rows fail and both are understood:

- **Budgie and Screech read low on pitch** because their archetypes swing so far
  that a fundamental estimate over the syllable is not a meaningful quantity.
  Screech has no reference and is an effect; Budgie's contours are the widest of
  any real species. The estimator is the limit here, not the engine.
- **Whistler and Sparrow render about half their `lengthSec`.** Stretching a
  short measured curve to a longer `Length` turns its internal amplitude
  modulation into separate notes, which the segmenter then counts separately.
  Choosing the archetype partly by how close its own duration is to `Length`
  would fix it; see TODO.md.

**18 of the 19 factory presets** meet the targets stated in their own files.
The exception is `single_chirp`, which misses on roughness (−21.2 dB measured
against its stated target): it is one dry syllable in silence, and spectral
flatness over a single fast-sweeping chirp carries the contour's motion rather
than the breath — the same limit recorded under *Fit* in TODO.md.

Two calibrations, both measured on the plugin's own output:

| Voice → closure | 0.00 | 0.13 | 0.26 | 0.39 | 0.65 | 0.78 | 0.91 |
|---|---|---|---|---|---|---|---|
| harmonics | 1 | 2 | 3 | 3 | 4 | 5 | 5 |

| effective Breath | 0 | 4.3 % | 12.9 % | 25.9 % |
|---|---|---|---|---|
| roughness | −31.5 | −27.8 | −22.0 | −17.7 dB |

## The two bugs in the measurement code

Worth their own heading, because both produced numbers that were then written
into documentation as findings.

**`smooth()` zero-padded.** numpy's `mode="same"` pads with zeros, and these
series are log2 of a frequency — around 11.6 — so the first and last samples
were dragged towards nothing and every contour got an invented three-octave
excursion at each end. *Everything* measured with it came out with a span of
about 3 octaves and a peak slew of 8000 oct/s, including a constant sine. The
original diagnosis of 0.1.0 was made with a different, correctly padded
estimator, so that finding stands — but every figure printed by `describe()`
before this was fixed was the artefact.

**The breath calibration measured the wrong quantity.** It swept the *parameter*
against roughness on a Whistler, whose species multiplier is 0.42, so it was
really measuring 0.42× what it thought. The slope came out at 17.7 dB/decade
instead of 13, and the default landed four times too high. It now reports both
the parameter and the effective value.

## What was tried and rejected

**Mel spectrograms with Griffin-Lim resynthesis**, from the SoundPlot framework.
Its analysis window at librosa's defaults is 92.9 ms — four times coarser than
the 21 ms that broke 0.1.0 — Griffin-Lim's worst case is frequency-modulated
transient material, and its own published metrics are `SNR −0.81 dB` and
`Spectral Corr 0.57`. A mel spectrogram at a resolution that would work is also
the recording with its phase thrown away.

**librosa's pYIN** for the pitch tracking, swept over three frame lengths and
three confidence gates. On identical syllables:

| tracker | span | path | peak slew | turns |
|---|---|---|---|---|
| numpy STFT | 0.518 | 2.013 | **424 oct/s** | 23 |
| pYIN, 1024 | 0.198 | 0.388 | 16 | 11 |
| pYIN, 2048 | 0.142 | 0.205 | 7 | 5 |

It discards 80 % of the path and cuts the slew by 26×, because its Viterbi
smoothing assumes slowly-varying pitch. `tools/analysis/setup-venv.sh` installs
it and the finding is recorded so nobody repeats it.

## What is not verified

- **Whether it sounds right.** That is the user's call and the whole reason
  0.1.0 shipped wrong. `!dev/listen-v3/` holds all 22 presets plus a `Partials`
  sweep on three species; `tools/analysis/contours.py --wav` writes
  reference/resynthesis pairs.
- **Pointer interaction with the window.** It opens at its design size, lays out
  correctly, animates and its theme was checked by screenshot, but `xdotool`'s
  clicks land on another window on top of the plugin's in this environment, so
  knob drags and typed value entry were not exercised. Shared code, unchanged
  from the other four plugins.
- **`clap-validator`**: no built copy in `CLAP/`.
- **Aliasing at 96 kHz** has not been looked at; the hinge width that limits it
  is derived rather than measured.

## Shared code touched

None. `shared/` is unchanged — the window, the parameter model, the preset
format, the DSP toolbox and the reverb are used exactly as the other four
plugins use them.

`src/plugin.cpp` is still the suite's one substantially duplicated file, now
five ways.
