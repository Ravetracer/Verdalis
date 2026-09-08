# SkyHowl analysis

Every default and every factory preset in SkyHowl comes from measurements of a
reference library of **63 field recordings of wind** — breezes, gusts, storms,
howling, cave and tunnel wind, rustling foliage. They are decoded to 48 kHz
stereo and capped at the first two minutes of each, which is 54.7 minutes of
material; the figures that need one channel take the mean of the two. The
recordings themselves are not part of this repository and are not ours to
redistribute; they live outside it and are read, never copied.

`!dev/references/` holds 64 files, two of which share a name in different
formats, so 63 are analysed.

The scripts here read a directory of references and print the numbers below.
`wavio.py` comes from `shared/tools/analysis`.

| Script | What it measures |
|---|---|
| `spectra.py` | third-octave shape, high-frequency slope, crest factor, L/R correlation |
| `gusts.py` | the flow: turbulence intensity, gust factor, gust rate, rise/fall, squall share |
| `howl.py` | the aeolian tone: prominence, jitter, pitch, swoop, Q, and whether it tracks the level |
| `rustle.py` | foliage: onset rate, onset pitch, flux variation |
| `fit.py` | every factory preset against the reference it was fitted to |

## The starting point: wind is silent

Air in motion radiates essentially nothing. Everything a listener calls wind is
the flow meeting something, so the engine is split into a flow field, which
makes no sound at all, and the sources it drives. That is not a stylistic
choice — it is what makes the parameter set fall out the way it does, and it is
why `Wind Speed` is the master control of level, brightness, pitch and rustle
rate at once.

## What was measured, and what it decided

| Quantity | Measured across the library | What it set |
|---|---|---|
| Slope above 500 Hz | −22 … +1.5 dB/oct, median **−8.7** | `Flow Tilt` range and default |
| Crest factor | 8.5 … 24.7 dB, median 15.8 | wind is a bed, not a sequence of events |
| L/R correlation | −0.38 … 1.00, median **0.49** | `Width` default |
| Turbulence intensity *I* | 0.04 … 0.27, median **0.10** | `Turbulence` range and default |
| Gust factor U_max/U_mean | 1.04 … 1.54, median **1.15** | `Gust Depth` |
| Gusts per minute | 0.6 … 32.8, median **3.5** | `Gust Rate` range and default |
| Envelope rise/fall ratio | 0.83 … 1.13, median **1.01** | `Gust Shape` default: a gust is symmetric |
| Share of envelope variance below 0.1 Hz | 0 … 0.97, median **0.43** | `Squall` — not a detail |
| Recordings holding a steady tone | **17 of 63** | `Howl Amount` is legitimately zero |
| Tone prominence (tonal set) | 10.5 … 15.6 dB, median 13.4 | how loud the howl has to be |
| Tone frequency | 170 … 1374 Hz, median **639** | `Howl Size` default (2.5 mm at 8 m/s) |
| Tone Q | 1.1 … 16.5, median **5.1** | `Howl Resonance` default |
| Tone swoop | 0.30 … 2.29 oct, median **0.72** | how far `Howl Track` moves it |
| Implied obstacle diameter | 1.5 … 11.8 mm, median **3.1** | `Howl Size` range |
| Rustle onsets, resolvable | **15 … 40 /s** | `Rustle Density` default |
| Rustle onset centroid | 2.8 … 6.6 kHz, median **4.2** | `Leaf Size` (≈ c/2L) |
| Onset flux variation | 0.20 (merged hiss) … 0.80 (dry clatter) | `Clatter` |
| Share below 50 Hz | −58 … −0.2 dB, median **−15.5** | `Buffet` range |
| Broadband centroid vs level | median r **−0.10**, positive in 27 of 63 | see *What does not fit* |

## Howling is aeolian, and the library proves it

This is the one prediction of the model a recording can falsify.

A bluff body in a flow sheds vortices alternately from each side at the
Strouhal frequency `f = St·U/d`, with St ≈ 0.2 over the whole Reynolds range
that occurs outdoors. Both the frequency and the level follow the same wind
speed U, so **if a wind tone is aeolian its pitch must rise and fall with its
loudness**. If the tone were a fixed resonance being excited harder, the pitch
would not move.

`howl.py` tracks the tallest spectral peak frame by frame and correlates its
frequency against the frame's level:

| Reference | prominence | jitter | peak | swoop | Q | track r |
|---|---|---|---|---|---|---|
| `howling_wind` | 15.1 dB | 0.049 | 480 Hz | 0.72 oct | 1.2 | **+0.87** |
| `heavy_windstorm` | 13.9 | 0.064 | 1324 Hz | 0.76 | 3.6 | **+0.83** |
| `howling_wind_3` | 14.2 | 0.061 | 1102 Hz | 0.52 | 3.5 | **+0.52** |
| `howling_wind_4` | 11.4 | 0.092 | 475 Hz | 0.72 | 4.8 | **+0.60** |
| `cave_wind` | 15.4 | 0.082 | 820 Hz | 2.08 | 6.7 | −0.01 |
| `very_soft_wind` | 13.5 | 0.049 | 170 Hz | 1.02 | 9.7 | −0.33 |

The pitch rises with the level in 12 of the 17 tonal recordings, and in the
four whose file names actually say *howling* the correlation runs from 0.52 to
0.87. `Howl Track` therefore defaults high.

The one clear exception is the cave, which is what the physics predicts: a
cavity resonates at a frequency its own geometry fixes and the flow only
excites it, so its pitch does not move. The `Gap` and `Cave` obstacles scale
`Howl Track` down almost to nothing for that reason. **This part is physics,
not measurement** — one recording is not a trend, and across the tonal set
there is no correlation at all between Q and tracking (r = 0.05), so the
library cannot separate the two families on its own.

## The prominence has to be measured frame by frame

A howl swoops. Averaging half a minute of one smears the tone across an octave
and leaves no peak to find, which is a property of the measurement and not of
the sound: the first version of `howl.py` measured the average spectrum and
concluded that only 31 of 63 recordings had a tone, with a median prominence of
6.7 dB and no useful tracking signal.

Measuring each frame instead — smooth to a twelfth of an octave, take a
one-and-a-half-octave running mean as the bed, find the tallest peak — is what a
listener hears. But it needs a second statistic beside it, because **the tallest
peak in a single frame of noise also stands 7–9 dB above a 1.5-octave mean**,
purely because a frame of noise is lumpy. What separates a tone from a lump is
that a tone moves smoothly:

| | jitter (median frame-to-frame move of the peak) |
|---|---|
| `howling_wind` | 0.049 oct |
| `cave_wind` | 0.082 oct |
| `rustling_leafs` | 0.451 oct |
| `wind_dune_medium_desert` | 0.841 oct |
| `stormy_wind_howling` | 0.904 oct |

A recording counts as tonal below 0.15 octaves. That threshold picks out
exactly the recordings whose names claim a howl, plus the cave — and it
correctly rejects `stormy_wind_howling`, which is named for what it sounds like
rather than what it contains.

## Much of the library is band-limited by its codec

Most of the references are MP3. The encoders put a hard lowpass in, and it is
far lower than a listener would guess:

| Reference | usable to |
|---|---|
| `spooky_wind` | 3.1 kHz |
| `wind_tone_room` | 3.4 kHz |
| `blowing_wind_2`, `howling_wind_2` | 4.7 kHz |
| `wind_underground` | 6.5 kHz |
| `cave_wind` | 6.9 kHz |
| `cold_arctic_wind` | 7.1 kHz |
| `wind_far_away` | 9.6 kHz |
| `wind_plain` | 11.0 kHz |
| the `.wav` material | 22–24 kHz |

"Usable to" is the highest frequency still within 60 dB of the in-band peak.
It is a blunt definition — it cannot tell a codec cliff from a genuinely steep
natural rolloff — but it is the honest one to fit against, because either way
the reference holds nothing measurable up there. `fit.py` therefore **does not
compare bands above each reference's own bandwidth**, and prints the cut-off
beside every pair. Fitting the synthesis to a band the reference does not
contain would be fitting it to an encoder.

Before this was noticed, five presets appeared to be 20–50 dB too bright and
three had had output filters put on them to compensate. Those filters are gone.

## Terrain roughness is arithmetic, not taste

The logarithmic wind profile gives a turbulence intensity of about
`I = 1 / ln(z/z₀)` at height z over ground of roughness length z₀, and z₀ is
tabulated (Wieringa 1992). At a listening height of 10 m:

| Terrain | z₀ | I | factor vs. short grass |
|---|---|---|---|
| Coast (open water) | 0.0002 m | 0.093 | 0.54 |
| Desert (sand) | 0.0003 | 0.096 | 0.56 |
| Tundra (snow) | 0.005 | 0.132 | 0.77 |
| Plain (short grass) | 0.03 | 0.172 | **1.00** |
| Meadow (long grass) | 0.1 | 0.217 | 1.26 |
| Mountain (broken country) | 0.5 | 0.334 | 1.94 |
| Forest | 1.0 | 0.434 | 2.52 |
| Street (city) | 1.5 | 0.527 | 3.06 |

Those factors are the `Terrain` table in `wind_engine.cpp`, so choosing
*Forest* really does make the wind two and a half times as gusty as choosing
*Plain*, by the same arithmetic a wind engineer would use. It is also why the
tree and street presets carry a low `Turbulence` number: the terrain has
already multiplied it.

## The level law

Aerodynamic sound from flow over a rigid surface radiates as a dipole, and
Curle (1955) gives its power as U⁶ — so amplitude goes as U³ and a doubling of
wind speed is +18 dB. That is `Speed Law` at 100 %, and it is why a gust is such
a large event: the synthesis reaches the output ceiling on a strong one, which
is what the soft clipper is for. The buffet follows the dynamic pressure
instead, U², so it grows more slowly than the bed.

## What does not fit

**The library's broadband centroid does not reliably rise with level** — median
correlation −0.10, positive in only 27 of 63 recordings — and the model says it
should, because the bed follows U³ and the buffet only U². The recordings that
darken as they get louder are the ones with the most energy below 50 Hz, which
suggests that what is being measured there is microphone pseudo-sound and the
drag of moving branches, neither of which is radiated wind. The model does not
reproduce it, and `Buffet` is what a preset has instead: raising it is what
makes a gust rumble rather than hiss. The bed's own corner therefore tracks the
speed at half the exponent the flow scaling implies, rather than at all of it.

**The presets swoop less than the references.** Across the 9 tonal presets the
median swoop is 0.28 of an octave against the references' 0.72. The pitch
follows a smoothed wind speed — an obstacle sees the flow averaged over the
eddies that envelop it, not the instantaneous free stream — and that smoothing
is currently a fixed 0.65 s time constant. It is probably too much.

## The preset library, measured

`fit.py` compares all 23 presets against their references. Aggregate:

| Quantity | the 23 presets | the 63 references |
|---|---|---|
| Turbulence intensity | 0.06 … 0.19, median 0.11 | 0.04 … 0.27, median 0.10 |
| Gust factor | 1.08 … 1.29, median 1.18 | 1.04 … 1.54, median 1.15 |
| Rise/fall ratio | 0.90 … 1.06, median 0.99 | 0.83 … 1.13, median 1.01 |
| Squall share | 0.10 … 0.97, median 0.51 | 0 … 0.97, median 0.43 |
| Tonal, by jitter | 9 of 23 | 17 of 63 |
| Tone prominence | 9.0 … 17.7 dB, median 13.4 | 10.5 … 15.6, median 13.4 |
| Tone frequency | 223 … 1318 Hz, median 627 | 170 … 1374, median 639 |
| Tone Q | 1.9 … 14.4, median 6.1 | 1.1 … 16.5, median 5.1 |
| Implied diameter | 1.5 … 9.0 mm, median 3.2 | 1.5 … 11.8, median 3.1 |

Third-octave shape: 20 of 23 presets have at least one band more than 6 dB from
their reference, and all but two are within 10 dB. The two that are not are in
`TODO.md`.

## Bugs these measurements found

Each of these was found by measurement, not by listening, and each had survived
sounding plausible:

- **`Flow Tilt` did nothing above −12 dB/octave.** The bed's slope was made by
  crossfading between taps of a four-pole cascade, which looks like it should
  interpolate the slope and does not: the sum of two transfer functions is
  dominated at high frequency by whichever falls more slowly, so mixing a
  two-pole tap into a three-pole one still asymptotes at −12 dB/oct, merely
  3.5 dB quieter. The parameter could be set to −24 and the bed still fell at
  −12, which is why every dark preset measured far too bright. The whole part
  of the slope is now that many poles at the corner and the fraction is one
  more pole whose corner slides down from five octaves above them; measured,
  the setting now delivers −5.4, −10.7, −15.9 and −21 dB/oct where it claims
  −6, −12, −18 and −24.
- **A gust reached only half strength, and dipped in the middle.** Its raised
  cosine was written with the wrong quarter-turn offset, so the envelope ran
  0.5 → 1 → 0.5 over the arrival instead of 0 → 1.
- **The top of the spectrum was set by resonator skirts, not by the wind.** A
  state-variable bandpass falls away at only 6 dB/octave, so a leaf ringing at
  4 kHz put measurable energy at 16 and a high-Q howl was the brightest thing
  in every preset fitted to a dark reference. Two more poles above each band
  take the skirt to 18 dB/octave, which is the 40 dB over two and a half
  octaves the full-bandwidth references measure.
- **The leaf transient bypassed the leaf.** The contact click was added to the
  output rather than to the band's input, which put flat white noise straight
  into the top of the spectrum.
- **Grass sheds at 16 kHz.** The `Grass` obstacle multiplied `Howl Size` by
  0.25 — a blade really is that thin — and with a small size setting that put a
  bank of aeolian tones in the top octave, 25 dB above anything any reference
  holds there.
- **Foliage types asked for implausible leaves.** `Grass` multiplied the leaf's
  frequency by 2.8, which is defensible on its own, but it meant that fitting a
  preset to a measured onset centroid asked for a leaf 170 mm across. A
  parameter that has to be set to an absurd number to produce the right sound
  is not a parameter.
