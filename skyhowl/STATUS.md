# SkyHowl status

Version 0.1.0. First working version: the engine, the parameter set, the window
and a first fitted preset library.

## A VST3 as well (2026-09-12)

SkyHowl ships as a VST3 alongside the CLAP, for Linux and Windows, in every
release archive. It is not a port: the clap-wrapper hosts the same
`SkyHowl-impl` static library the `.clap` is built from, so the engine, the
parameter table, the presets and the window are one copy behind both formats.

It passes Steinberg's validator 47/47 on Linux; the Windows build is checked by
loading it under wine and instantiating it, because the validator does not
cross-build. The window needed no change at all -- it was already an embedded
X11 window driven from `clap_host_timer_support`, which is what VST3's
`IRunLoop` embedding wants.

The factory presets are embedded in the binary, so the plugin's own browser is
fully stocked in a VST3 with no files on disk. What a VST3 host will *not* do is
list them in its own browser: the CLAP preset-discovery factory has no
equivalent in the wrapper release in use.

`./install.sh --vst3` builds and installs both. See *The VST3 builds* in the
suite `CLAUDE.md`.


## What works

- **The flow field**, and the fact that it is silent. Mean speed, turbulence
  shaped to Kolmogorov's `f^-5/3`, discrete gusts and a squall drift, all at a
  control rate of one update per 64 samples with the gains ramped between
  updates. All 53 parameters are wired and audible.
- **Aeolian howling.** A bank of up to 12 resonant bands at `f = St·U/d`, whose
  pitch tracks the wind speed. Nine obstacles, from a blade of grass to a cave
  mouth, each with its own size, Q, spread and — for the cavities — its own
  refusal to track the wind at all.
- **The U⁶ level law.** Curle's dipole, so amplitude follows U³ and doubling the
  wind is +18 dB. `Speed Law` scales the exponent, because a physically correct
  wind is hard to keep in a mix.
- **Terrain roughness as arithmetic.** The eight terrains' gustiness factors are
  `1/ln(z/z₀)` at a 10 m listening height, normalised to short grass, from the
  tabulated roughness lengths. *Forest* really is 2.52× as gusty as *Plain*.
- **The rustle layer**, with eight foliage types and the measured density
  ceiling: past 15–40 resolvable onsets a second, leaf clicks merge into a wash.
- **Preset discovery**, state save/load, sample-accurate parameters, host
  modulation, bounded voice/gust/leaf pools.
- **The window**, in SkyHowl's own dust-coral theme with streaklines in the
  header that flow faster with the gust load and take a shove from every new
  gust. Typed value entry, preset browser, gust meter.
- **Self-test**: 0 failures, including a fixed `Random Seed` rendering
  identically after a reset, identical output across two runs at 44.1, 48 and
  96 kHz, all parameters at their extremes staying finite and bounded, and
  preset round-tripping.

## What is measured

Full numbers in `tools/analysis/README.md`. Fitted against 63 field recordings
(48 kHz, 54.7 minutes), covering breezes, gusts, storms, howling, cave and
tunnel wind and rustling foliage.

The flow statistics land on the library's medians almost exactly:

| Quantity | the 23 presets | the 63 references |
|---|---|---|
| Turbulence intensity | 0.06 … 0.19, median 0.11 | 0.04 … 0.27, median **0.10** |
| Gust factor | 1.08 … 1.29, median 1.18 | 1.04 … 1.54, median **1.15** |
| Rise/fall ratio | 0.90 … 1.06, median 0.99 | 0.83 … 1.13, median **1.01** |
| Squall share below 0.1 Hz | 0.10 … 0.97, median 0.51 | 0 … 0.97, median **0.43** |
| Tone prominence | 9.0 … 17.7 dB, median 13.4 | 10.5 … 15.6, median **13.4** |
| Tone frequency | 223 … 1318 Hz, median 627 | 170 … 1374, median **639** |
| Tone Q | 1.9 … 14.4, median 6.1 | 1.1 … 16.5, median **5.1** |
| Implied obstacle diameter | 1.5 … 9.0 mm, median 3.2 | 1.5 … 11.8, median **3.1** |
| Tonal presets / recordings | 9 of 23 | 17 of 63 |

The default patch alone, rendered for two minutes with the seed pinned, measures
a crest factor of 21.1 dB against the library's median 15.8 (its range is
8.5–24.7), an L/R correlation of 0.51 against 0.49, a slope of −7.7 dB/octave
against −8.7, a turbulence intensity of 0.09 against 0.10, a gust factor of 1.13
against 1.15 and a rise/fall ratio of 1.01 against 1.01. It peaks 3.3 dB below
full scale and sits at −26.6 dBFS RMS: quiet for a default, deliberately, because
the U⁶ level law makes a strong gust a +18 dB event and the headroom has to be
there for it.

Third-octave shape: 20 of 23 presets have at least one band more than 6 dB out,
and all but two are within 10 dB. See `TODO.md` for the two.

## Howling is aeolian, and the library proves it

This is the one prediction of the model a recording can falsify. Both the pitch
and the level of a shed tone follow the same wind speed, so an aeolian tone must
rise in pitch as it gets louder — where a fixed resonance being excited harder
would not.

The tone's pitch rises with the level in 12 of the 17 recordings that hold a
steady tone, and in the four whose names say *howling* the correlation runs 0.52
to 0.87, with `howling_wind` itself at **0.87**. `Howl Track` therefore defaults
high.

## The prominence had to be measured frame by frame

A howl swoops, so averaging half a minute of one smears the tone across an
octave and leaves no peak to find. The first version of `howl.py` measured the
average spectrum and concluded that only 31 of 63 recordings had a tone, with a
median prominence of 6.7 dB and no useful tracking signal — a property of the
measurement, not of the sound.

Measuring frame by frame needs a second statistic beside it, because the tallest
peak in a single frame of *noise* also stands 7–9 dB above a 1.5-octave mean.
What separates a tone from a lump is that a tone moves smoothly: median
frame-to-frame jitter of 0.05 octaves for `howling_wind` against 0.90 for
`stormy_wind_howling`, which is named for what it sounds like rather than what it
contains. The 0.15-octave threshold picks out exactly the recordings that claim a
howl, plus the cave.

## Much of the reference library is band-limited by its codec

Far more severely than a listener would guess: one recording holds nothing above
3.1 kHz, several stop between 4 and 7, and only the `.wav` material runs to 24.
`fit.py` therefore does not compare bands above each reference's own bandwidth,
because fitting the synthesis to a band the reference does not contain would be
fitting it to an encoder.

Before this was noticed, five presets appeared to be 20–50 dB too bright and
three had had output filters put on them to compensate. Those filters are gone.

## A gust is silent

Gusts own no filter and no noise source. A gust is an envelope with a strength
and a position, and everything audible about it is the bed and the howl
responding — which is both the physics and what keeps the pool cheap enough to be
generous with. It is also why the gust meter can show 60 in flight without a
measurable change in CPU.

## Known bugs found and fixed in this version

Recorded because each was found by measurement, not by listening, and each had
survived sounding plausible. The same class of mistake is easy to repeat.

- **`Flow Tilt` did nothing above −12 dB/octave.** The bed's slope was made by
  crossfading between taps of a four-pole cascade, which looks like it should
  interpolate the slope and does not: the sum of two transfer functions is
  dominated at high frequency by whichever falls more slowly, so mixing a
  two-pole tap into a three-pole one still asymptotes at −12 dB/oct, merely
  3.5 dB quieter. The parameter could be set to −24 and the bed still fell at
  −12, which is why every dark preset measured far too bright. The whole part of
  the slope is now that many poles at the corner, and the fraction is one more
  pole whose corner slides down from five octaves above them. Measured, the
  setting now delivers −5.4, −10.7, −15.9 and −21 dB/octave where it claims −6,
  −12, −18 and −24.
- **A gust reached only half strength, and dipped in the middle.** Its raised
  cosine was written with the wrong quarter-turn offset, so the envelope ran
  0.5 → 1 → 0.5 over the arrival instead of 0 → 1.
- **The top of the spectrum was set by resonator skirts, not by the wind.** A
  state-variable bandpass falls away at only 6 dB/octave, so a leaf ringing at
  4 kHz put measurable energy at 16 kHz and a high-Q howl was the brightest
  thing in every preset fitted to a dark reference. Two more poles above each
  band take the skirt to 18 dB/octave, which is the 40 dB over two and a half
  octaves the full-bandwidth references measure.
- **The leaf transient bypassed the leaf.** The contact click was added to the
  output rather than to the band's input, putting flat white noise straight into
  the top of the spectrum. A leaf is not a broadband radiator.
- **Grass sheds at 16 kHz.** The `Grass` obstacle multiplied `Howl Size` by 0.25
  — a blade really is that thin — and with a small size setting that put a bank
  of aeolian tones in the top octave, 25 dB above anything any reference holds
  there.
- **Foliage types asked for implausible leaves.** `Grass` multiplied the leaf's
  frequency by 2.8, which is defensible on its own, but it meant that fitting a
  preset to a measured onset centroid asked for a leaf 170 mm across. A
  parameter that has to be set to an absurd number to produce the right sound is
  not a parameter, it is a fudge.
- **The howl chased the turbulence instead of the gust.** The shedding frequency
  followed the instantaneous wind speed, which smeared the tone over two octaves
  against a measured median swoop of 0.72. It now follows a smoothed speed,
  which is also the physics: an obstacle sees the flow averaged over the eddies
  that envelop it, not the free stream.

## Shared code touched

`shared/tools/docgen.cpp` — a tip written across several source lines carried a
real newline, and a Markdown table row ends at the first one, so the row was
silently truncated and the rest of the tip left as a stray paragraph. It now
collapses whitespace. Verified by rebuilding all four manuals; ShoreBreak's
*Bubble Rate* and *Bubble Damping* rows were the ones affected and are now
complete.
