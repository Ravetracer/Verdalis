# InsectSwarm — status

**0.3.0. The stridulation layer was wrong about the chorus, not about the
insect.** Every spectral statistic it was fitted to was inside the references'
spread and stayed there; what it got wrong was in time, and no per-file median
could have said so. Found by ear, against NightLife's crickets, and then
measured: `tools/analysis/chorus.py` is the new file, and these are its numbers.

| statistic (9 cricket references) | references | 0.2.0 | 0.3.0 |
|---|---|---|---|
| steadiness — how deep the holes are | 0.16 .. 0.85, med 0.36 | **0.08** | 0.44 |
| chirp-peak width / centre | 0.07 .. 0.73, med 0.55 | **0.04** | 0.11 |
| within-chirp crest | 1.70 .. 9.06, med 2.49 | 2.28 | 1.99 |
| within-chirp duty | 0.08 .. 0.69, med 0.27 | 0.33 | 0.39 |

Three changes, in the order they mattered:

- **A stroke, not a click.** A scraper dragged across a file drives the harp for
  most of a wing stroke and a tymbal buckles rib by rib; 0.2.0 modelled both as
  one sample into a resonator, and its own calibration comment recorded the
  cost — "6 per cent of it is sounding". The new `Scrape` parameter is how much
  of each pulse period the insect is driving its body, and it is the one control
  that separates the two mechanisms: measured inside a chirp the references
  sound 0.27 of the time for a cricket and 0.79 for a cicada. At 0 it is 0.2.0's
  single click exactly.
- **A caller's clock is not a metronome.** A lone cricket's chirp-rate peak is
  0.73 of its own centre wide; 0.2.0 rendered 0.04, because twelve exactly
  periodic trains are a picket fence however far apart the pickets are. Each
  caller now has a filtered random walk on its chirp rate, at the corner `Wander
  Rate` already sets for the wing layer.
- **The rhythms scatter as far as the library says.** 0.2.0's comment said "the
  rhythms scatter several times as far, and nothing in the library bounds them"
  while scattering them by a tenth. Echeme ±45 %, pulse ±30 %, duty ±35 % at
  full `Scatter`. The carrier stays at ±3 %, which the composite-Q argument
  really does bound.

54 parameters now, up one. **0.2.0 presets load and sound different**: `Scrape`
defaults to 0.45 and a 0.2.0 preset does not set it, so a stridulation preset
from before will sound as this version intends rather than as it did. Setting
Scrape to 0 restores the old behaviour exactly.

**0.2.0.** Adds `Roam`, the level counterpart of `Wander`: an individual closing
on the listener and backing off again. Corrects the rate wander, which was
running a factor of 64 too slow and a factor of 1.7 too shallow.

**0.1.0.** First version. Builds as a `.clap` and a `.vst3` for Linux and
Windows, passes its own self-test and the suite's window checks, and ships
sixteen factory presets.

## What is measured

A reference library of **67 field recordings**, 61 minutes, 22 to 192 kHz:
21 honeybee, 14 cicada, 9 cricket, 7 mosquito, 6 housefly, 3 bumblebee, 3 wasp,
3 dragonfly, 1 hornet. Not in this repository; see `tools/analysis/README.md` for
the full record.

| Quantity | Measured | What it set |
|---|---|---|
| Wingbeat rate, per species | 85.7 – 463.7 Hz | the `Species` rows |
| Harmonic stack, per species | a peak at 150 – 460 Hz then −5 to −10 dB/oct | the fitted shelf and resonance |
| Harmonic-to-noise, one insect | 6.6 dB honeybee, 16.0 mosquito | `Rasp`'s reference point |
| Harmonic-to-noise, a hive | 0 – 2 dB | `Spread`'s default, by calibration |
| Voiced fraction | 0.08 dragonfly, 0.56 – 0.94 everything else | the Dragonfly row is a clatter |
| Frame-to-frame rate drift | 10 – 24 cents | `Wander`'s default of 18 |
| Flyby level rise | 13.3 dB, 9.5 – 44.2 | `Rise` |
| Flyby −6 dB width | 1.37 s, 0.26 – 6.8 | `Pass Time` |
| Flight speed | 3.2 m/s | `Speed`, and the finding that Doppler is inaudible |
| Cicada carrier / Q / clicks | 5549 Hz / 13.2 / 268 a second | the Cicada preset |
| Cricket carrier / Q / clicks | 4518 Hz / 25.8 / 36 a second | the Cricket preset |
| Chorus composite Q | 13.2 and 25.8 | bounds the *carrier* scatter at ±3 % |
| Chorus steadiness | 0.16 – 0.85, median 0.36 | `Scrape`, and the rhythm scatter |
| Chirp-peak width | 0.07 – 0.73, median 0.55 | the callers' clock wander |
| Within-chirp duty | 0.27 cricket, 0.79 cicada | `Scrape`'s default and the two presets |

## What is verified

- **The self-test passes**, 38 checks, run as part of `install.sh`: state
  round-trip, preset round-trip, block sizes, parameter extremes, tail, silence,
  reproducibility under a pinned `Random Seed`.
- **The engine reproduces its own table.** `fit.py` renders each species and
  measures it with the estimators the references went through: wingbeat rates
  land within **1 cent**, harmonic stacks within **1.8 to 4.2 dB RMS** — about
  the fit's own residual — and harmonic-to-noise within **2.6 dB**.
- **Count and Spread reach the hive figure.** Twelve individuals at 150 cents
  render at 1.7 dB harmonic-to-noise against the library's 0 to 2 for a hive, and
  a Count of one stays harmonic at every Spread.
- **The stridulation layer reproduces its settings.** Solo, with the scatter off,
  the carrier renders exactly; a chorus holds the cricket's measured Q to within
  25 %.
- **Every factory preset renders** without clipping, between −19.6 and −27.9 dBFS
  RMS, peaking no higher than −1.9 dBFS.
- **`Wander` and `Roam` deliver what they say.** A rendered single bee at
  `Wander` 18 measures 14.4 cents of drift between 85 ms frames, against 1.8 at
  a setting of zero; at 40 it measures 33.2. A 50 ms level envelope of the same
  render measures 3.7, 7.3 and 10.5 dB of standard deviation at `Roam` 4, 8 and
  12. Both read slightly under the setting because the analysis window is wider
  than the walk, which is also true of the references the numbers came from.
- **`Roam` cannot overload a patch.** Its excursion is bounded at 1.6 standard
  deviations and the compensation is set on the power, so a solo insect's peak
  stays between −9.5 and −3.7 dBFS across the whole range of the control while
  its average level falls.

## What is known and not fixed

- **The Hornet row rests on one recording**, 4.2 seconds of it. Its fitted
  resonance sits between the first and second harmonics, which reproduces the
  measured stack — the second harmonic is 10.7 dB above the fundamental in the
  library and 14 dB in the render — but means the perceived pitch is an octave
  above the wingbeat. That is what the recording is. It is the weakest row in the
  table.
- **The Wasp row rests on two**, after one reverberant reference was dropped for
  locking to the room rather than the insect.
- **`pulse.py` cannot read a synthetic click train's Q.** Measured on one clean
  train it reports the width of a spectral line rather than the resonator's, so a
  solo cicada at Q 13 comes back as Q 641. It is correct on the references, which
  are dense and noisy, and on a rendered chorus. The estimator meeting a signal
  no recording ever is, not the engine.
- **`Roam` is modelled, not measured.** The library has no recording of a known
  insect at a known distance, so there is nothing in it to fit the size of the
  level drift against. What it is fitted to is the geometry — a 1/r law close in
  — and what sets the factory values is the ear. It is the one control in the
  plugin whose default is neither measured nor calibrated.
- **No inter-individual rate spread was ever measured**, because a hive recording
  cannot yield one. `Spread` is calibrated against the hive's harmonic-to-noise
  ratio instead, which is the nearest thing the library has to it.

## Cost

Four notes at once, up to 64 individuals each, capped by `Max Individuals` at 256
across all of them. One individual is a phase accumulator, two stroke pulses, a
band-limited noise source, a one-pole shelf and one resonator. The default preset
— twelve individuals — is a small fraction of one core.

The two normalisations that make the level knobs mean what they say are measured
rather than derived: `calibrate()` runs one individual's excitation through a
throwaway copy of its shaper, and `calibrateStrid()` does the same for one
caller. Both run only when a parameter that reaches them has actually moved.
