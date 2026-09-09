# ChirpParade status

Version 0.1.0. First working version: the engine, the parameter set, the window
and a first fitted preset library.

## What works

- **The syringeal voice.** The Gardner–Laje–Mindlin labium
  (`x' = y`, `y' = −εx − Cx²y + By`), integrated in Liénard coordinates so the
  pitch is exact, and driven by two gestures per syllable: air sac pressure and
  syringeal tension. The syllable begins and ends at the Hopf bifurcation rather
  than being gated. All 61 parameters are wired and audible.
- **A syllable's shape as one number.** `Contour` is the phase between the two
  gestures, so up-sweeps, down-sweeps, arches and dips are four readings of one
  knob. `Turns` sets how much of a gesture cycle the syllable spans, and `Sweep`
  stays the whole excursion whatever `Turns` is.
- **Timbre as the drive, not a filter.** `Voice` is `μ = B/√ε`, which takes the
  oscillation from a sinusoidal whistle to the relaxation regime a crow has —
  measured at 1 harmonic at μ = 0.15 and 9 at μ = 3.5.
- **The one-sided airflow.** Air only gets past while the labia are apart, so
  the source is the rectified gap. That is where every even harmonic comes from;
  see *Known bugs found and fixed* below.
- **The tract.** The trachea as a closed tube at `c/4L`, with `Beak` both
  raising and broadening the resonance and making it follow the pitch across the
  syllable the way a songbird's gape does.
- **Ten species, each of them a measurement.** Pitch, sweep, length, skew,
  harmonic richness, roughness, syllable rate, contour and turns are the medians
  of the recordings of that bird, applied as ratios to the library-wide medians
  so the knobs keep their meaning. `Pitch` at its default is the library median,
  so every species at the default sings in its own register.
- **One shot and drone at once.** A note fires a deliberate phrase at
  `Shot Level`; while it is held, a flock of individuals calls unprompted at
  `Flock Level` as a Poisson process, and `Answer` makes one bird's phrase
  provoke a reply from another. Either half can be turned right off.
- **Phrases as plans.** A phrase owns no oscillator and makes no sound — it
  decides when something does — which is what keeps the pool cheap enough to run
  64 syllables at once.
- **A note's phrase always completes.** The envelope gates the flock, which is a
  bed, but not the shot, which is a deliberate event: a 50 ms note on a
  fourteen-syllable woodpecker laugh gets all fourteen. The voice is held until
  its own phrase finishes so that its birds keep their pitches and it is not
  stolen out from under itself.
- **Woodpecker drumming** as a separate layer, because it is sonation rather
  than voice: two broad wooden modes excited by a contact, in a roll that
  accelerates by the measured 23 %.
- **Preset discovery**, state save/load, sample-accurate parameters, host
  modulation, bounded voice/syllable/strike pools.
- **The window**, in ChirpParade's own finch-gold theme, with a sonogram
  scrolling across the header — one stroke per syllable, deterministic in the
  syllable number. Typed value entry, preset browser, activity meter. Clicking
  the version label fires one free phrase with no note behind it.
- **Self-test**: 0 failures, including a fixed `Random Seed` rendering
  identically after a reset, all parameters at their extremes staying finite and
  bounded, odd block sizes, out-of-range values clamped, and preset
  round-tripping with enums written by name.
- **Reproducible renders**: all 22 presets byte-identical across two runs at
  44.1, 48 and 96 kHz with `Random Seed` pinned, at two different seeds, and
  different seeds giving different output.
- **`--defaults`**, which prints every parameter at its table value in the
  preset format. The factory library is authored from it, so a preset cannot
  omit a parameter or carry a stale default.

## What is measured

Full numbers in `tools/analysis/README.md`. Fitted against 58 field recordings
holding **4268 measured syllables**, covering garden chirps, robins,
nightingales, budgies, woodpecker calls and drumming, crows, a raven, a goose,
cranes and several multi-bird soundscapes.

The syllable defaults are the library medians almost exactly:

| Quantity | the default | the 4268 syllables |
|---|---|---|
| Duration | 96 ms | 27 … 481, median **96** |
| Fundamental | 2600 Hz | 292 … 5925, median **2579** |
| Sweep | 0.35 oct | 0.03 … 1.01, median **0.35** |
| Contour turns | 0.25 | 0.25 … 4.0, median **0.25** |
| Skew | 0.37 | rise 24 ms against fall 41 ms |
| Harmonics | 1 (Sparrow) | 1 … 12, median **1** |
| Roughness | −28 dB, via `Breath` 10 % | −35 … −16, median **−28** |
| Tract length | 4.9 cm | loudest harmonic at 1749 Hz |
| Pulse rate | 13.2 Hz | 8.5 … 66, median **13.2** |
| Syllables per phrase | 3 | 1 … 22, median **3** |
| Syllable rate | 7.5 /s | 2.0 … 22.8, median **7.4** |
| Phrase gap | 0.31 s | 0.11 … 1.64, median **0.31** |
| Legato | 0.70 | 70 % of gaps have no silence in them |
| Flock rate | 273 /min | 87 … 711, median **273** |
| Strikes per roll | 10 | 4 … 21, median **10** |
| Strike rate | 15 /s | 8.1 … 20.8, median **15** |
| Accelerate | +0.23 | interval drift −23 % across a roll |
| Knock | 1 kHz | centroid 1251 Hz, bandwidth 935 |

`fit.py --species` renders each species as one isolated syllable and measures it
back with the same estimators. Nine of the ten land within ±4 % on pitch and
within one or two harmonics of their group's measurement. All 22 factory presets
meet the targets stated in their own files.

## The one prediction the references can falsify

If a syllable is one turn of two coupled gestures, its envelope and its pitch
contour are two sinusoids with a phase between them, so the shape a sonogram
reader names follows from that phase alone: arches must correlate pitch with
level, dips must anticorrelate, sweeps must sit near zero.

**The ordering holds, monotonically, across all six shapes** over 3600
syllables — arch +0.27, down +0.16, up +0.15, flat +0.11, wobble +0.08,
valley −0.10 — and 29 % of arches are above +0.5 against 4 % of dips.

It is weaker than two clean sinusoids would give, and the dips especially so.
The library carries a general positive bias (+0.11 overall), which is the same
model with the gestures broadly in phase, and a real syllable is not one clean
cycle of anything. So the phase control is justified and the claim that it is
*sufficient* is not: `Turns` and `Variation` are there because of that, and
neither is in the papers.

## Rolls accelerate, against the usual description

A woodpecker's roll is usually described as slowing down towards its end. The
library says the opposite: 22 of 35 rolls speed up, the fitted interval drift is
a median −23 %, and asked without a fit the last third of a roll runs at 0.79 of
the interval of the first. A detector losing quiet late strikes would lengthen
the late intervals, so it can only bias this the other way — the finding
survives the obvious objection. `Accelerate` therefore defaults to +0.23.

## A trill is not a modulation

Only 10 of the 4268 syllables carry a periodic wobble of their own pitch. What
the ear calls a trill here is syllables arriving too fast to separate, up to
22.8 a second with no silence between them. So there is no trill oscillator: a
trill is `Syllable Rate` high and `Legato` high, which is what the measurement
says it is.

## Known bugs found and fixed in this version

Each of these had already been listened to and accepted before the analysis
caught it. Full write-ups in `tools/analysis/README.md`.

1. **Odd harmonics only.** The equation is odd-symmetric, so the labial
   displacement has energy at f, 3f, 5f and nothing between — a 234 Hz
   fundamental came out as 234, 656, 1125 Hz and no drive would make a crow. The
   missing physics is that the source is the *airflow*, and air only gets past
   while the labia are apart. Rectifying the gap at closure fixed it.
2. **The oscillator ran flat, by up to 80 %.** Discretised as written, the
   nonlinear term is a scaling of the velocity — a shear, not a rotation — and
   its mean around the limit cycle is −1, not zero, so the error does not
   average out. Liénard coordinates make the nonlinearity additive; the residual
   is now within ±3 % across the whole range of `Voice`.
3. **`Sweep` and `Turns` fought.** A contour that turned twice swept twice as
   far as its label said. The excursion is now measured at spawn and scaled.
4. **`Pitch` was not the pitch.** The pressure gesture peaks at 0.37 of the
   syllable, so the pitch at its loudest is not the middle of the contour. The
   contour is now anchored so the two agree, which is what makes the instrument
   play in tune.
5. **A whistle had a 400 ms tail** against a measured 41 ms, because `μ` was
   allowed near the bifurcation where the oscillation neither grows nor decays.
6. **`Ring` did nothing to the drum.** It was set on the resonators, which at
   the measured bandwidth have rung out in a fifth of a millisecond. It belongs
   on the excitation.
7. **The trill detector was measuring the analysis frame rate** and reported a
   37–43 Hz trill for a fifth of the library.
8. **The pitch tracker jumped between harmonics**, making a woodpecker call
   sweep from 1.2 to 7.1 kHz.
9. **The pitch range included the inaudible ramps**, where the tracker wanders:
   a syllable with no sweep measured 1.5 octaves of it.
10. **The segmenter's floor test failed on clean audio**, measuring one 200 ms
    chirp as a 603 ms syllable. Re-running the library with the fix moved no
    median by more than 3 %.

## What is not verified

- **Pointer interaction with the window was not driven in this environment.**
  The window opens at its design size, lays out correctly, animates, and its
  theme and ornament were checked by screenshot — but `xdotool`'s clicks were
  landing on another window on top of the plugin's (`getmouselocation` reports a
  different window under the pointer), so knob drags and typed value entry were
  not exercised here. Both are shared code, unchanged from the other four
  plugins, where they are verified.
- **`clap-validator` was not run**: no built copy is present in `CLAP/`.

## Shared code touched

None. `shared/` is unchanged by this plugin — the window, the parameter model,
the preset format, the DSP toolbox and the reverb are used exactly as the other
four plugins use them.

`src/plugin.cpp` is still the suite's one substantially duplicated file. With a
fifth plugin now sharing it, the case for a template or an engine interface is
stronger than it was; see `TODO.md`.
