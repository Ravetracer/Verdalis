# ChirpParade status

Version 0.5.0. The reference library was **cleaned rather than grown** — every
file isolated to a single bird by hand, the dense multi-bird recordings dropped
— and that one change did more for the plugin than any fitting so far.

- **82 % of segmented syllables now pass the contour quality gate, against
  45 % before.** 88 files, 6991 syllables, **5749 usable contours** — 61 % more
  usable material out of 10 % fewer recordings. The gate was never the problem;
  the recordings were.
- **`lengthSec` now comes from the archetypes**, and that fixed the two oldest
  entries in TODO.md. Budgie's pitch read 65 % low and now reads 4 % low;
  Whistler and Sparrow rendered half their stated length and now render within
  5 % of it. Eight of nine species land inside ±4 % on pitch and ±5 % on length.
- **A ninth species, `Piper`**, from two oystercatcher recordings: 372 usable
  contours, fast, clean and mid-pitched, which nothing else in the table is.
  Its nearest neighbour is Woodpecker at a distance of 1.5, far enough that
  folding it into an existing species would have moved that table rather than
  joined it.
- **Seven new recordings joined existing species** — blackbird ×2, nuthatch,
  treecreeper and a great spotted woodpecker into Whistler, a winter wren into
  Sparrow, a pheasant into Crane — chosen by measured distance to each group's
  centroid, all within 1.1. The groups are acoustic, not taxonomic: the great
  spotted woodpecker's sharp 5 kHz "kik" belongs nowhere near the Woodpecker
  table, which is built from the green woodpecker's laugh.
- **Each syllable now plays at its archetype's own measured duration.**
  `Contour::durationSec` was stored for every archetype from the start and never
  read: the length came from the species median, so every curve was stretched to
  one target and a species could vary its syllable length only by the +-2.3x
  `Variation` gives. One nightingale recording spans 60x. The phrase now also
  advances by the slot a syllable actually took, so a long curve pushes the next
  one out rather than being truncated. At a species' median archetype the timing
  is unchanged, so the difference is entirely in the spread around it.
- **`Hollow Tree` was removed** -- a drumming woodpecker under a songbird read as
  neither -- and **`Nightingale Trill` became `Nightingale Song`**, because a
  nightingale's whole character is that it does not repeat itself: 254 syllables
  in 45 s of reference, 21 to 1296 ms, 1644 to 5731 Hz, median pairwise shape
  distance 5.22. It now runs `Variation` wide enough to cross the whole
  archetype set. **18 factory presets ship.**
- 72 archetypes, 54 kB. **`Piper` is appended to the enum**, so unlike 0.4.0
  this is not a state break: 0.4.0 saved state and presets load correctly.
  They will *sound* different, because the species medians moved.

**Every preset changed except `Woodpecker_Drum`**, which is pure drumming and is
byte-identical. `!dev/listen-v6/` holds old-and-new pairs of all 19 at a pinned
seed. Whether this is an improvement is a question for the ear, and it has not
been answered yet.

## 0.4.0

The reference library grew from 58 recordings to 98, the contour
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

- **Measured contours as the voice.** 72 archetypes, the medoids of 5749
  clustered syllables extracted from the reference library, shipped as cosine
  coefficients in `src/dsp/contours_generated.h` — 13824 floats, 54 KB, no audio.
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
- **Nine species, each a measurement.** Pitch register, archetype set, syllable
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
- **CPU**: 60 s of *Dawn Chorus* — twelve birds, 48 voices — renders in 0.87 s,
  **69× realtime**, unchanged by the ninth species and the wider table; the six extra partial oscillators cost about a quarter of
  the previous headroom. Dropping 0.1.0's ODE and its up-to-eight integration
  substeps per sample is most of that.

## What is measured

88 field recordings, every one isolated by hand to a single bird. **6991
segmented syllables**, of which **5749 passed the contour quality gate** (82 %)
and 72 became archetypes, eight for each of the nine species.

| group | files | usable contours | | group | files | usable contours |
|---|---|---|---|---|---|---|
| Sparrow | 14 | 903 | | Crane | 7 | 193 |
| Warbler | 7 | 737 | | Woodpecker | 8 | 187 |
| Whistler | 11 | 495 | | Raven | 3 | 89 |
| Piper | 2 | 372 | | Goose | 5 | 63 |
| Budgie | 5 | 296 | | Crow | 1 | 43 |

Crow and Raven are measured but are not species the engine offers; they stay in
the census. 17 files are still ungrouped and contribute nothing — see TODO.md.

`fit.py --species` renders each species as one isolated syllable and measures it
back with the same estimators:

| Species | pitch got/want | length got/want | harmonics |
|---|---|---|---|
| Whistler | 3642 / 3728 (−2 %) | 80 / 94 ms (−15 %) | 1 / 1 |
| Sparrow | 3452 / 3491 (−1 %) | 59 / 71 ms (−17 %) | 1 / 1 |
| Warbler | 2779 / 2812 (−1 %) | 64 / 78 ms (−18 %) | 1 / 1 |
| Budgie | 1340 / 1353 (−1 %) | 59 / 66 ms (−11 %) | 2 / 3 |
| Woodpecker | 2514 / 2550 (−1 %) | 107 / 129 ms (−17 %) | 2 / 2 |
| Crane | 807 / 821 (−2 %) | 117 / 137 ms (−14 %) | 4 / 5 |
| Goose | 525 / 528 (−1 %) | 152 / 178 ms (−15 %) | 4 / 6 |
| Screech | 888 / 1800 (−51 %) | 144 / 208 ms (−31 %) | 11 / 6 |
| Piper | 2531 / 2559 (−1 %) | 51 / 59 ms (−14 %) | 1 / 2 |

Each row is the median over that species' eight archetypes, rendered one at a
time -- one render at the default `Contour` measures one arbitrary archetype,
which stopped being a fair test of the species once each archetype kept its own
duration.

**Every species but Screech is within ±2 % on pitch.** Length reads 11-18 %
short *everywhere*, and a bias that uniform is the estimator rather than the
engine: the segmenter measures the voiced part of a syllable, not the full
stretched contour. Rendering a single archetype against its own stored duration
lands exactly (Warbler[2], 27 ms measured against 27 ms stored). Screech is the
one real failure and is understood -- no reference, borrowed extreme contours,
an effect rather than a bird.

**Eight of nine inside ±4 % on pitch and ±5 % on length.** The two failures
recorded at 0.4.0 are both gone, and both for the same reason — `lengthSec` is
now the median duration of a species' *archetypes* rather than of all its
measured syllables, so `Length` and the curve being stretched to it agree:

- **Budgie** read 65 % low on pitch and now reads 4 % low. Its contours are the
  widest of any real species, and stretching them to a mismatched `Length` was
  what made a fundamental estimate meaningless, not the width itself.
- **Whistler and Sparrow** rendered about half their `lengthSec` and now render
  within 5 % of it.

One row still fails and is understood: **Screech** reads 51 % low because it has
no reference of its own and borrows the library's most extreme contours by
construction. It is an effect, not a bird.

Roughness is the quantity that still does not meet its target — Goose measures
−16 dB against −25, Screech −8 against −16 — and that is the open question in
TODO.md about whether spectral flatness can tell a rough tone from a noisy one
at all.

**All 18 factory presets** meet the targets stated in their own files.
`single_chirp` had been the standing exception, missing on roughness; it passes
at 0.5.0. That is not a fix aimed at it -- the roughness estimator is unchanged
and still carries the contour's motion as well as the breath, as recorded under
*Fit* in TODO.md -- the archetype it lands on simply moved.

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
  0.1.0 shipped wrong, and it matters more at 0.5.0 than at any release since:
  every measured quantity improved, which is exactly what the statistics said at
  0.1.0 too. `!dev/listen-v6/` holds old-and-new pairs of all 19 presets at a
  pinned seed, plus a `Contour` sweep on Piper;
  `tools/analysis/contours.py --wav` writes reference/resynthesis pairs.
- **`clap-validator`**: no built copy in `CLAP/`.
- **Aliasing at 96 kHz** has not been looked at; the hinge width that limits it
  is derived rather than measured.

## Shared code touched

None. `shared/` is unchanged — the window, the parameter model, the preset
format, the DSP toolbox and the reverb are used exactly as the other four
plugins use them.

`src/plugin.cpp` is still the suite's one substantially duplicated file, now
five ways.
