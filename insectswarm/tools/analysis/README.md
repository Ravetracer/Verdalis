# InsectSwarm analysis

Every default and every factory preset in InsectSwarm comes from measurements of
a reference library of **67 field recordings** of insects — bees, bumblebees,
wasps, a hornet, houseflies, mosquitoes, dragonflies, cicadas and crickets — at
22 to 192 kHz, mostly stereo, **61 minutes** in total. The recordings are not
part of this repository and are not ours to redistribute; they live in
`!dev/references`, which is gitignored, and are read, never copied.

The scripts here read that directory and print the numbers below. `wavio.py`
comes from `shared/tools/analysis`.

| | |
|---|---|
| `cache.py` | decodes the library once to 48 kHz mono in `!dev/work`; everything else reads that |
| `refs.py` | inventory: duration, rate, crest factor, centroid, octave bands, and the class each filename names |
| `pitch.py` | the periodicity estimator the rest share |
| `wingbeat.py` | the wingbeat rate, its harmonic stack, how far it wanders |
| `swarm.py` | harmonic-to-noise ratio, pitch dispersion, the bed under the buzz, stereo width |
| `pulse.py` | the stridulators: carrier, Q, click rate, chirp rate, duty |
| `chorus.py` | the stridulation layer *in time*: steadiness, chirp-peak width, within-chirp duty |
| `flyby.py` | a single pass: level trajectory, Doppler ratio, the speed it implies |
| `species.py` | reduces all of it to `src/dsp/species_generated.h` |
| `fit.py` | renders the plugin back out and measures it with the same estimators |
| `makepresets.py` | writes the factory preset library |

## The estimator had to be replaced before anything could be measured

The first attempt scored candidate fundamentals by harmonic summation and gated
on how far the winner beat its nearest rival. That cannot be thresholded: a
harmonic sum is smooth in f0 and its runner-up is almost always the octave, so
the gate called **55 of the 67 references unvoiced**. Everything here uses
McLeod's normalised square difference function instead, which comes with a
clarity value in [0, 1] that means something.

## What separates the species, and what does not

| | rate (Hz) | refs | harmonic-to-noise (dB) | voiced |
|---|---|---|---|---|
| Hornet | **85.7** | 1 | 2.0 | 0.94 |
| Dragonfly | 118.8 | 2 | 1.1 | **0.08** |
| Bumblebee | 143.4 | 3 | 6.3 | 0.74 |
| Wasp | 150.6 | 2 | 6.6 | 0.93 |
| Housefly | 193.1 | 6 | 5.9 | 0.86 |
| Honeybee | 221.2 | 16 | 6.6 | 0.56 |
| Mosquito | **463.7** | 5 | **16.0** | 0.92 |

Three things follow.

**A bee is half noise.** A honeybee's buzz measures 6.6 dB harmonic-to-noise and
a mosquito's 16.0. That is the difference between a buzz and a whine, and it is
why a stack of oscillators never sounds like a bee however carefully its
harmonics are set. The engine generates turbulence alongside the wingbeat and
`Rasp` is the ratio between them, in dB, against that measurement.

**A dragonfly is not a buzz at all.** Only 8 per cent of its frames are periodic,
against 56 to 94 for every other species, and it has the highest crest factor in
the library at 32.9 dB. It is a clatter of wings. It is shipped as that rather
than forced into a tone the recordings do not contain.

**The rate wander does not separate the species.** Every one of them drifts 10 to
24 cents from one 85 ms frame to the next, and the interquartile spread of a
single recording's pitch runs 43 to 103 cents — against rates that differ by a
factor of five. So `Wander` is one knob rather than a column of the species
table.

## The harmonic stacks are all the same shape

Every species' median stack is a peak somewhere between 150 and 460 Hz followed
by a fall of 5 to 10 dB per octave. That is one resonance and one tilt, which is
why four fitted numbers per species are enough:

| | tilt (dB) | resonance (Hz) | Q-ish | bandpass mix | fit residual |
|---|---|---|---|---|---|
| Hornet | 6.0 | 152 | 0.94 | 0.98 | 3.3 dB |
| Bumblebee | −15.0 | 323 | 0.86 | 0.68 | 1.6 dB |
| Wasp | 3.5 | 216 | 0.94 | 0.95 | 3.8 dB |
| Housefly | 4.5 | 375 | 0.94 | 0.58 | 1.5 dB |
| Honeybee | 36.0 | 2190 | 0.94 | 0.75 | 2.1 dB |
| Mosquito | 9.0 | 1137 | 0.94 | 0.68 | 1.6 dB |
| Dragonfly | 16.0 | 1196 | 0.90 | 0.83 | 4.0 dB |

Residuals of 1.5 to 4.0 dB RMS across stacks 25 dB deep. Three things about that
fit are worth stating, because each of them was got wrong first:

- **It is fitted against the engine's own filters**, evaluated on the unit
  circle, not against an idealised response. A fit to an ideal filter lands
  coefficients the plugin does not reproduce.
- **The excitation is part of the model.** A (1−u²)² stroke pulse 7 per cent of a
  cycle wide is already 20 dB down by its tenth harmonic. Fitted as if the
  excitation were flat, the shaper comes out far too steep — and on the hornet,
  whose second harmonic is 10.7 dB *above* its fundamental to begin with, steep
  enough that the rendered buzz jumped the octave.
- **The harmonics are weighted 1/√k**, the same weighting the pitch estimator
  uses and for the same reason: the first few carry the perceived pitch. Weighted
  flat, the hornet traded 3.6 dB of error at the harmonic that *is* its pitch for
  a fraction of a dB spread over harmonics nobody hears separately.

## Many insects are noise, and that is the whole swarm

| | harmonic-to-noise |
|---|---|
| a single close insect | 8 to 13 dB |
| a hive or a swarm | 0 to 2 dB |

Nothing has to be added to make a crowd sound like one. Enough fundamentals
scattered widely enough *are* noise. Rendering the engine back confirms it:

| individuals | spread 0 ct | 70 ct | 150 ct | 250 ct |
|---|---|---|---|---|
| 1 | 4.5 | 4.2 | 8.0 | 4.2 |
| 4 | 3.7 | 3.7 | **0.6** | −2.1 |
| 16 | 4.8 | 4.0 | **1.7** | −0.9 |
| 64 | 3.6 | 4.3 | **1.5** | −1.7 |

At a Count of one the spread does nothing, correctly — one insect is one insect
however wide the distribution it was drawn from.

**`Spread`'s default is the one calibrated number in the plugin**, and the reason
is worth stating plainly: the library cannot measure it. A hive recording yields
one dominant pitch frame by frame, not sixty separate ones, so what comes out of
it is how much *that* pitch moved — which is `Wander`, a different quantity. What
the library does measure is where the scatter ends up, and 150 cents is what puts
a dozen individuals inside the measured hive figure.

## The Doppler shift is real and inaudible

Ten of the 57 usable files are a single clean pass. They measure:

| | median | range |
|---|---|---|
| level rise over the approach | **13.3 dB** | 9.5 – 44.2 |
| width of the bump 6 dB down | **1.37 s** | 0.26 – 6.8 |
| flight speed from the frequency ratio | 3.2 m/s | 0.3 – 26.7 |

That speed column is the negative result. At 3.2 m/s the shift across a pass is
**30 cents**, while the same insects' wingbeat rates wander by 43 to 103 cents on
their own. The Doppler is buried in the wander, and the files that appear to show
a large one — a "26 m/s bee" — are showing an insect changing gear, not crossing
the microphone at highway speed.

So the engine computes the Doppler because it is physically right, and the
flyby's audible cue is the level and the filtering. `Speed` is a knob you can
push past anything an insect can do, at which point it becomes an effect.

## The stridulators are one mechanism with two sets of numbers

A cicada's tymbal buckles and a cricket's scraper crosses a file. Both are a
train of clicks ringing a resonant body — a carrier and a pulse rate, not a
fundamental and a stack — which is why `wingbeat.py` measures both classes as
"950 Hz with the energy 25 dB up at the fourth harmonic", that being the wrong
estimator on the wrong mechanism rather than a result.

| | refs | carrier | Q | clicks/s | chirps/s | duty |
|---|---|---|---|---|---|---|
| Cicada | 14 | 5549 Hz | **13.2** | **268.4** | 12.5 | 0.48 |
| Cricket | 9 | 4518 Hz | **25.8** | **35.9** | 10.5 | 0.33 |

Twice as sharp and seven times slower is the whole distance between a dry rattle
and a pure whistling trill. Nothing else separates them, which is why there is no
Stridulator chip in the window: a control that picked between two *models* would
be claiming a difference the recordings do not contain. Both are shipped as
factory presets instead.

The chorus scatter is bounded by the same measurement. A chorus recording
measures the *composite* peak, and a composite at Q 25.8 cannot come from callers
spread much wider than a thirteenth of their carrier: rendered at ±10 per cent, a
cicada chorus came back at Q 7.2 and a cricket at 12.2, both half the library's
figure. So `Scatter` reaches ±3 per cent at its top and no further — which is
also the biology, since a species' carrier is its anatomy and every member shares
one. Their clocks are not, so the click and chirp rates scatter several times as
far.

## Rendering it back

`fit.py` renders the engine and measures it with the same estimators, which is
what makes the table checkable rather than merely stated.

| | wingbeat rate | stack, RMS error | harmonic-to-noise |
|---|---|---|---|
| Bumblebee | +1 ct | 2.3 dB | +1.9 dB |
| Wasp | +1 ct | 4.0 dB | −1.6 dB |
| Housefly | −1 ct | 2.4 dB | +2.6 dB |
| Honeybee | 0 ct | 1.8 dB | −2.2 dB |
| Mosquito | 0 ct | 3.0 dB | +0.6 dB |
| Dragonfly | 0 ct | 4.2 dB | +2.3 dB |
| Hornet | see below | 9.2 dB | −4.4 dB |

The rates are exact. The stacks agree with the table to about what the fit's own
residual was, which is the most that can be asked of them.

**The hornet is measured an octave high, and it is not a fault in the engine.**
Its second harmonic is 10.7 dB above its fundamental in the library and 14 dB
above it in the render, and at that ratio the pitch estimator locks to the
harmonic — as a listener would. What is genuinely thin about that row is its
evidence: one recording, 4.2 seconds of it. It is the weakest row in the table
and the first one to revisit if a second hornet reference ever turns up.

## What was deliberately not done

- **No separate "swarm" mechanism.** The measurement says a crowd is many
  individuals and nothing else, so the engine has no chorus, no detune stage and
  no ensemble effect.
- **Nothing below 80 Hz is generated.** The lowest wingbeat in the library is the
  hornet's 85.7 Hz. Below that is the recordist's afternoon.
- **No pitch-tracking resonance.** Every species' fitted formant is an absolute
  frequency, so moving `Rate` without moving `Formant` is what a real insect does
  when it flies harder: the beat changes and the body it radiates from does not.
