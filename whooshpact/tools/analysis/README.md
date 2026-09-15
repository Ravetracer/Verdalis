# WhooshPact reference analysis

What was measured, how, and what the plugin does about it.

The reference library is **211 files in six folders** — 35 accents, 16 booms,
17 braams, 13 downshifters, 24 impacts and 106 transitions — in
`!dev/references/`, which is gitignored and stays on this machine. They are not
ours to redistribute.

## What these references are, and what that changes

Every other plugin in the suite is fitted against *recordings of the world*: 25
fires, 17 rivers, a library of bird song. The physics is there to be found, and
the measurement's job is to find it.

These are not recordings of anything. They are **finished production sounds**,
made by somebody else out of synthesis, samples and processing. There is no
underlying physical object to fit, and pretending otherwise would be inventing a
mechanism the material does not contain.

So the measurement asks a different question. Not *what made this sound* but
*what is this sound*: how long it lasts, where in it the peak sits, how its
spectrum moves from beginning to end, how much of it is below 100 Hz, and
whether its amplitude is being chopped. Those are the quantities a gesture has,
and they are what the engine's parameters are.

The consequence is worth stating plainly: **WhooshPact is fitted to a target,
not derived from a model.** It is the one plugin in the suite where the
measurement describes the goal rather than the mechanism.

## The scripts

| Script | What it measures |
|---|---|
| `refs.py` | The whole survey: span, peak position, rise, fall, centroid sweep, octave-band shape, sub-100 Hz fraction, crest factor, stereo correlation. One row per file, medians and percentiles per family. `--csv` writes the raw table. |
| `shape.py` | The *contours* rather than the scalars: the envelope, the centroid and the low partial on a common normalised time axis, 16 points each, median per family. |
| `flutter.py` | The amplitude modulation: its rate in the first and last third of each gesture, its depth, and how strongly periodic it is at all. |
| `decay.py` | Per-octave-band decay to −20 dB, and the strongest partial below 200 Hz. |
| `verify.py` | Applies `refs.py` and `shape.py` to a rendered WAV, so a preset can be held against the family it was fitted to. |
| `makepresets.py` | Writes the 31 factory presets. Generated rather than typed so that all of them carry every parameter. |

Everything is numpy and the suite's `shared/tools/analysis/wavio.py`. No scipy,
no plotting: the analysis is FFT and arithmetic, as it is everywhere else in the
suite.

## What the survey found

### The shape in time is the whole thing

`refs.py`, medians per family:

| family | span | peak | fall | sweep | <100 Hz | crest | L/R corr |
|---|---|---|---|---|---|---|---|
| accents | 3.55 s | **0.043** | 1.40 s | −0.69 oct | 0.69 | 19.0 dB | 0.58 |
| booms | 5.50 s | **0.024** | 2.84 s | −0.12 oct | 0.97 | 14.2 dB | 0.89 |
| braams | 5.63 s | **0.148** | 2.37 s | **+0.60 oct** | 0.74 | 12.4 dB | 0.74 |
| downshifters | 4.57 s | **0.024** | 3.02 s | −1.07 oct | 0.98 | 7.7 dB | 0.99 |
| impacts | 4.34 s | **0.060** | 2.57 s | −0.63 oct | 0.92 | 14.1 dB | 0.87 |
| transitions | 3.71 s | **0.328** | 1.29 s | −0.67 oct | 0.62 | 17.1 dB | 0.60 |

*peak* is where the loudest frame sits as a fraction of the active span, and it
separates the families more sharply than anything else in the table. A boom and
a downshifter peak within the first 2.4 % of themselves. A transition peaks a
third of the way in. That single number is the difference between a hit and a
whoosh, and it is why `Peak` is a parameter rather than a mode.

### The contours, and why they are not tables in the binary

`shape.py` puts every gesture on a normalised 0..1 time axis and takes the
median contour per family. The transitions:

```
t      0.00  0.07  0.13  0.20  0.27  0.33  0.40  0.47  0.53  0.60 ...
env   0.012 0.065 0.162 0.348 0.519 0.560 0.528 0.388 0.299 0.188
```

That is `(t/0.33)^1.8` on the way up and roughly `(1-u)^2.5` on the way down,
and the same five numbers — span, peak, hold, rise curvature, fall curvature —
fit all six families to within the spread of the library itself. So the engine
carries the *formula* and not the contour: sixteen numbers per family would be
permissible under the suite's rules, and they would also be redundant.

What the engine does carry from the measurement is the **spectral profile**: two
numbers per family, the slope of its octave curve above 125 Hz and how far
63 Hz stands over 125 Hz, both relative to Transition.

| family | slope above 125 Hz | 63 Hz over 125 Hz |
|---|---|---|
| accents | −3.73 dB/oct | +7.4 dB |
| booms | −9.13 | +12.5 |
| braams | −5.23 | +4.0 |
| downshifters | −10.18 | +10.6 |
| impacts | −4.43 | +7.0 |
| transitions | −4.08 | +2.3 |

A real −10 dB/octave would take 60 dB off the top of a boom and leave nothing to
mix with, so the plugin applies 2.2 octaves' worth of it through one high shelf.
The measurement is a statement about balance, and that is how it is used.

### The sweep happens late

The centroid contour of a transition is flat for its first third and then falls
0.8 octaves. That is why the Air layer has a `Curve` as well as a `Sweep`: an
envelope that moves the filter evenly across the gesture does not sound like any
of these.

Braams are the one family that rises — +0.60 octaves of centroid, and more than
an octave of low partial. Every braam preset therefore has a positive `Sweep`
and a positive `Glide`, and it is the only family where that is true.

### The bottom is the instrument

97 % of a boom's energy and 98 % of a downshifter's is below 100 Hz. The median
fundamental of the pitched families is 41–49 Hz, all within a tone of G1.

This is where WhooshPact departs from the rest of the suite. RainyDay,
CrackleBlaze and the others highpass at 60 Hz, because in a *field recording*
everything below that is traffic, ventilation and handling noise on the
microphone. Here it is the sound. The highpass default is 25 Hz, and it is there
to keep out DC and inaudible cone travel, nothing else.

`decay.py` on the per-band decays:

| family | 63 Hz | 1 kHz | 8 kHz |
|---|---|---|---|
| accents | 0.35 s | 0.44 s | 0.27 s |
| booms | 0.63 s | 0.28 s | 0.65 s |
| braams | 1.82 s | 2.20 s | 1.06 s |
| downshifters | 1.90 s | 0.82 s | 0.20 s |
| impacts | 0.78 s | 0.45 s | 0.21 s |

An impact's bottom rings nearly four times as long as its top. That ratio is
`Sub Decay` against `Hit Decay`, and getting it wrong is what makes a
synthesised impact sound like a snare.

### There is mostly no flutter, and where there is, it glides

This is the finding that decided how the feature works.

`flutter.py` takes the amplitude envelope with its slow shape divided out and
asks what fraction of the in-band modulation energy sits at a single rate. A
metronome would give 1.0; noise with no rate in it gives something near zero.
The medians:

| family | rate, first third | rate, last third | peakiness |
|---|---|---|---|
| accents | 7.7 Hz | 7.0 Hz | 0.031 |
| booms | 9.1 Hz | 8.4 Hz | 0.022 |
| braams | 6.5 Hz | 5.6 Hz | 0.031 |
| downshifters | 8.1 Hz | **16.4 Hz** | 0.045 |
| impacts | 6.8 Hz | 7.7 Hz | 0.038 |
| transitions | 6.8 Hz | 7.4 Hz | 0.033 |

**Five of the six families are not fluttering at all.** The rates in that table
are where the noise happened to peak. Reporting them as "the measured flutter
rate" would be reading a number out of a measurement that says there is nothing
to read.

Where it does appear it is unmistakable and it is in the downshifters:

| file | rate, first third | rate, last third | ratio | peakiness |
|---|---|---|---|---|
| Downshifter - Stutter Scream | 4.1 Hz | 25.0 Hz | **6.08** | **0.123** |
| Transition - Quick 05 | 2.6 Hz | 7.2 Hz | 2.75 | 0.160 |
| Boom - Spacious | 22.6 Hz | 22.5 Hz | 0.99 | 0.123 |
| Braam - Grim Reaper | 17.9 Hz | 17.9 Hz | 1.00 | 0.101 |
| Downshifter - Groin Kick | 4.8 Hz | 2.4 Hz | 0.49 | 0.080 |

So: 2–25 Hz, both accelerating and slowing down, with the extremes at 0.22× and
6.08×. That is exactly a start speed and an end speed with an exponential glide
between them, which is what the plugin has — and it is **off by default**,
because the library says most of these sounds do not have it.

One measurement trap, recorded because it cost an hour. A 4 ms RMS envelope has
its Nyquist at 125 Hz and will happily track the *carrier* of a sound that has
descended to 30 Hz — which is precisely what a downshifter does. The first run
reported "flutter at 34 Hz" for files with none. `flutter.py` uses a 12 ms hop
and a 25 Hz ceiling for that reason.

## Verifying a preset

Render the library with the seed pinned and measure it the same way:

```sh
cd build
./whooshpact-render --plugin ./WhooshPact.clap --all --outdir /tmp/wp \
    --seconds 9 --tail 4 --param 63=7
cd ../tools/analysis
python3 verify.py /tmp/wp/*.wav
```

`--param 63=7` is `Random Seed`; `--param` matches on a parameter's *display*
name with spaces removed, or on its numeric id, and several display names repeat
across panels here, so the ids are the reliable way to drive it. Percent
parameters take 0–100 through `--param`, not 0–1.

Held against the table at the top of this file, the library comes out inside its
families on every quantity except where it is deliberate: `Blast Off` and
`Metallic Collision` measure a *positive* sweep because their tails outlive their
low end on purpose, and the presets with no sub layer sit far under their
family's low fraction on purpose -- `Simple Whoosh` and `Passby` at 0.01 and
`Dry Snap` and `Glass Break` at 0.00 against 0.62 and 0.69. `Deep Whoosh` is the
one over its family, at 0.78 against 0.62: its brown noise and 900 Hz corner are
what put it there, not its sub, which was trimmed 6 dB and moved the figure by
0.04.

### Deciding the sub layer per preset

The Sub was originally on in all thirty presets because its base value was
audible, so every preset overrode it and none could inherit "off". The base is
now -60 dB, as Tone and Hit already were. Which presets keep a sub was decided by
rendering each candidate twice, with the layer and without, and measuring the low
fraction of both against the family median:

| preset | family | with sub | without | family | decision |
|---|---|---|---|---|---|
| Gate Closed | accents | 0.90 | 0.72 | 0.69 | off -- the sub carried 12.9 dB of peak in an accent |
| Bad Guy | braams | 0.73 | 0.71 | 0.74 | off -- the square at 37 is already the bottom |
| Annihilation | braams | 0.79 | 0.68 | 0.74 | trimmed 3 dB, to 0.76 |
| Rising Dread | braams | 0.81 | 0.56 | 0.74 | trimmed 3 dB, to 0.75 |
| Deep Whoosh | transitions | 0.82 | 0.75 | 0.62 | trimmed 6 dB, to 0.78 |
| Big Horn | braams | 0.74 | 0.63 | 0.74 | kept |
| Brass Wall | braams | 0.73 | 0.64 | 0.74 | kept |
| Jumpscare | accents | 0.72 | 0.01 | 0.69 | kept -- the sub is its whole low end |
| Steel Stab | accents | 0.47 | 0.04 | 0.69 | kept, already under |
| Reveal | transitions | 0.49 | 0.00 | 0.62 | kept, already under |
| Whoosh Hit | transitions | 0.58 | 0.01 | 0.62 | kept |

The booms, impacts and downshifters were not auditioned: at 0.92-0.98 below
100 Hz the sub is the sound. Taking a layer out changes the trimmed output level,
so the five changed presets were re-rendered and their output gains reset to the
library's -8 dBFS peak. The A/B renders are in `!dev/audition-sub/`.

## What is not measured here

- **Nothing about how the references were made.** No attempt to identify sample
  content, layering or processing. The survey describes finished sounds.
- **Loudness normalisation between families.** The references are mastered at
  wildly different levels and the survey deliberately works on shapes and
  ratios, not absolute levels. The preset library's own levels were set by
  rendering and trimming to about −8 dBFS peak.
