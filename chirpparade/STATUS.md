# ChirpParade status

Version 0.2.0. The syllable model was replaced: 0.1.0's physical syrinx, fitted
to aggregate statistics, is gone and measured contours drive the oscillator
instead. **0.1.0 presets and saved state do not load meaningfully** — the
syllable parameters changed meaning and one was replaced. Nothing was released
at 0.1.0.

## What works

- **Measured contours as the voice.** 67 archetypes, the medoids of 1641
  clustered syllables extracted from the reference library, shipped as cosine
  coefficients in `src/dsp/contours_generated.h` — 4288 floats, 17 KB, no audio.
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
- **Reproducible renders**: all 22 presets byte-identical across two runs at
  44.1, 48 and 96 kHz with `Random Seed` pinned.
- **CPU**: 60 s of *Dawn Chorus* — twelve birds, 48 voices — renders in 0.78 s,
  **77× realtime**. Dropping 0.1.0's ODE and its up-to-eight integration
  substeps per sample is most of that.

## What is measured

Full numbers in `tools/analysis/README.md`. 58 field recordings, **4268
segmented syllables**, of which **1641 passed the contour quality gate** and 67
became archetypes.

`fit.py --species` renders each species as one isolated syllable and measures it
back with the same estimators:

| Species | pitch got/want | length got/want | harmonics |
|---|---|---|---|
| Whistler | 4734 / 4748 (−0 %) | 91 / 89 ms | 1 / 1 |
| Sparrow | 3106 / 3147 (−1 %) | 59 / 60 ms | 1 / 1 |
| Warbler | 1209 / 1128 (+7 %) | 107 / 107 ms | 1 / 2 |
| Budgie | 1292 / 1351 (−4 %) | 37 / 37 ms | 2 / 3 |
| Woodpecker | 3328 / 3312 (+0 %) | 101 / 99 ms | 1 / 2 |
| Crane | 947 / 982 (−4 %) | 48 / 48 ms | 3 / 4 |
| Goose | 604 / 566 (+7 %) | 91 / 90 ms | 2 / 4 |
| Crow | 807 / 806 (+0 %) | 117 / 114 ms | 5 / 5 |
| Raven | 1161 / 1171 (−1 %) | 197 / 197 ms | 7 / 6 |

Nine of ten within ±7 % on pitch and ±3 % on length. All 22 factory presets meet
the targets stated in their own files.

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

## What is not verified

- **Whether it sounds right.** That is the user's call and the whole reason
  0.1.0 shipped wrong. `!dev/listen-v2/` holds all 22 presets plus sweeps of
  `Contour`, `Detail` and `Voice`; `tools/analysis/contours.py --wav` writes
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
