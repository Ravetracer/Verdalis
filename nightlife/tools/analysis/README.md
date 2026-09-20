# NightLife analysis

Every default, every caller and every factory preset in NightLife comes from
measurements of a reference library of **45 field recordings of a night** —
wolves howling alone and in chorus, dogs, tawny and barred and scops and screech
owls, foxes calling and screaming, a loon, thirteen recordings of frogs from a
single close croak to a pond full of them, and two performed "werewolf" tracks
that are measured and deliberately used for nothing. They are read at their own
sample rates (24, 44.1 and 48 kHz) and capped at the first 45 to 60 seconds of
each; the figures that need one channel take the mean of the two.

The recordings are not part of this repository and are not ours to
redistribute; they live in `!dev/references/`, outside the build, and are read,
never copied.

| Script | What it measures |
|---|---|
| `calls.py` | the segmenter and the per-call measurements everything else reads |
| `callers.py` | the census per caller, which *is* the engine's Caller table |
| `phrases.py` | the temporal structure: gaps, phrases, calls per phrase |
| `contours.py` | the pitch and level contours, and the archetype table |
| `frogs.py` | what a croak is made of, and how a chorus is spaced in time |
| `bed.py` | the night bed's spectrum, the insect band, the filterbank solve |
| `makepresets.py` | writes the sixteen factory presets from one table |
| `fit.py` | the plugin's own output, measured back against all of the above |
| `listen.py` | A/B pairs: each preset beside the recording its numbers came from |

`wavio.py` comes from `shared/tools/analysis`. Nothing here needs anything but
numpy.

## The three measurements, and why they are three

A night is not one texture and the layers are not built from the same kind of
number.

### The callers: a call is its frequency contour

This is ChirpParade's finding, learned there the expensive way: a physical model
of the animal fitted to *aggregate statistics* agrees with every statistic and
sounds like nothing alive, because the statistics were taken through an analysis
window that cannot see what a call does.

`contours.py` does van Hunter Adams' procedure automatically and at scale: pull
the instantaneous frequency and amplitude of a real call out of the WAV, fit
each as a cosine series in normalised call time, cluster per caller, keep the
medoids. **208 usable contours became 48 archetypes.**

Three things are different from ChirpParade's version of the same pipeline, and
each is measured rather than assumed:

**The analysis window.** 21 ms with a 2.7 ms hop, against its 5.3 ms and
0.33 ms. A 5.3 ms window is 188 Hz of resolution and cannot see a 220 Hz tawny
owl at all. The cost is modulation above 47 Hz, and nothing in this library has
any.

**The length gate.** 55 ms to 6 s, against 18 to 900 ms. The longest single howl
in the library runs 5.6 s; past six seconds what the segmenter has found is a
chorus of animals it could not separate, which is what `wolves-howling-1.wav`
is — one 9.2 s run of three wolves over each other.

**The term count is per second, not per call.** The ratio of the fitted
contour's path to the tracked contour's, at a fixed count:

| | length | 48 terms | 64 | 96 | 128 |
|---|---|---|---|---|---|
| Wolf | 1.19 s | 0.66 | 0.80 | 0.97 | 1.15 |
| Owl | 0.35 s | 1.19 | 1.38 | 1.84 | 2.39 |
| Screech | 0.47 s | 1.67 | 1.90 | 1.94 | 2.42 |
| Scops | 0.57 s | 1.61 | 2.02 | 2.53 | 3.48 |
| Fox | 0.45 s | 1.25 | 1.53 | 2.05 | 2.57 |
| Loon | 0.27 s | 1.36 | 1.54 | 2.08 | 2.38 |

There is no column worth having. Scaling with the call's own duration collapses
the spread, and 55 terms per second is where the median lands on 1.00:

| terms/s | median | Wolf | Owl | Screech | Scops | Fox | Loon |
|---|---|---|---|---|---|---|---|
| 40 | 0.83 | 0.69 | 0.82 | 0.91 | 0.91 | 0.85 | 1.01 |
| 55 | ~1.00 | 0.77 | 0.92 | 1.15 | 1.09 | 1.04 | 1.01 |
| 85 | 1.14 | 0.92 | 1.00 | 1.34 | 1.36 | 1.29 | 1.10 |

**The vibrato is not a parameter.** Six of the eight wolf archetypes carry one,
measured from the fitted curve at 2.1 to 6.4 Hz. It is in the series, where the
wolf that was recorded put it. (The frame-domain estimator in `calls.py` cannot
measure it — its hop is 10.7 ms and it refuses lags under five frames, so it
cannot report below an 18.7 Hz period, and asked for a wolf's vibrato it returns
exactly that rail for every group in the library. `contours.vibrato()` measures
it on the fitted curve instead, which is continuous.)

### The tonality gate

What each threshold keeps, and what it costs in fit error:

| gate dB | usable | err c | Wolf | Owl | Screech | Scops | Fox | Loon |
|---|---|---|---|---|---|---|---|---|
| 6.0 | 146 | 5 | 40 | 31 | 15 | 37 | 15 | 8 |
| 3.0 | 166 | 7 | 42 | 33 | 16 | 37 | 30 | 8 |
| 0.0 | 181 | 8 | 48 | 33 | 16 | 37 | 39 | 8 |
| **−3.0** | **188** | **8** | 50 | 33 | 16 | 37 | 44 | 8 |
| −6.0 | 191 | 8 | 50 | 33 | 16 | 37 | 47 | 8 |

−3 dB nearly triples the fox candidates against +6 for no fit error at all, and
the four quiet callers stop moving well before it. Below −3 only the fox gains,
and what it gains is the part of a scream where the tracker follows the noise.

### The chorus: a croak is a pulse train

Not a contour, which is why the frogs are in `frogs.py` and not in
`contours.py`. Measured over 627 croaks in 13 recordings:

| | close recordings (8) |
|---|---|
| croak length | 43 – 608 ms, median 325 |
| pulse rate | 10 – 69 Hz, median 27 |
| pulses per croak | 2 – 25, median 10.5 |
| pulse depth | 0.96 – 0.99 |
| first resonance | 1529 – 3299 Hz, median 2063 |
| second resonance | a measured **0.50** of the first |
| Q of the first | 9 – 31, median 20 |
| croaks a minute | 48 – 480, median 121 |

The Q figure has a history worth recording. The first version of `frogs.py`
smoothed the croak's spectrum over a third of an octave to bridge the comb a
pulse train makes, and returned a Q of 4.0 to 4.2 for **all thirteen
recordings** — which is not a frog, it is the smoother: a third-octave smoother
returns a third of an octave. The smoother is linear and 2.5 pulse rates wide
now, and the answer spreads from 9 to 31.

### The chorus in time: the Fano factor

The variance of the arrival count in a window over its mean. Exactly 1.0 at
every window for a Poisson process, which is what every other event-spawning
plugin in the suite uses.

| window | 50 ms | 250 ms | 1 s | 4 s |
|---|---|---|---|---|
| **measured** | 0.90 | 0.81 | 0.59 | 0.30 |
| Poisson | 1.00 | 1.00 | 1.00 | 1.00 |
| CrackleBlaze's fire | | | 3.90 | |

A frog chorus is *more* regular than random and fire is less. Two further
findings came out of trying to render it:

- Independent per-frog clocks do not reproduce it. The superposition of many
  independent renewal processes tends to a Poisson process however regular each
  one is, and eight frogs on their own clocks rendered 1.15 at four seconds.
  **The regularity belongs to the pond.**
- The comparison is limited by the segmenter on both sides: a dense chorus
  merges into runs, so the `Regularity` default was swept on a sparse render
  (three frogs, 25 croaks a minute) where the events resolve.

### The bed, and the insects in it

The third-octave curve of the quietest third of the frames of every reference,
median across 45 files, collapsed onto eight octave bands and solved
non-negatively in power.

The solve is done here rather than at run time, which is the one place this
differs from ShoreBreak, SkyHowl, RiverFlow and CrackleBlaze: their controls
reshape the target, so they have to solve live. Nothing here does.

Two things had to be learned before the rendered bed matched the solve:

- **A bank of octave bandpasses cannot make a steep end.** The measured night
  falls 20 dB between 2.5 and 5 kHz; an Svf bandpass's skirts fall at 6 dB per
  octave. With the top two bands solved to *zero* the bed still rendered within
  2 dB of flat to 10 kHz. It has a second-order lowpass at 2.6 kHz and a
  highpass at 40 Hz now, swept rather than chosen, and the bank is solved for
  what is left.
- **The analytic model of a bandpass is not the filter the engine builds.**
  Even with the end filters the first render sat 6.2 dB rms from its target. So
  the last step is measured: `bed.py --calibrate <render.wav>` takes the
  rendered bed's own third-octave curve and pushes each band by the error in its
  own octave. Seven passes took it to **3.9 dB rms**, which is close to the
  floor for eight octave-wide bands against a curve with third-octave features
  in it.

The insect band is the weakest measurement in the plugin and the tools say so.
The library has no recording of insects alone; what is measured is the cricket
band behind a scops owl and behind two frog choruses, isolated by being narrow,
high and steady where everything else in those files is none of the three. The
first version of the search found 2203 to 2520 Hz in nine of the thirteen frog
recordings, which is the distant part of the frog chorus itself — hence the
2.8 kHz floor and the steadiness test.

| | |
|---|---|
| carrier | 2.9 – 3.2 kHz |
| Q (−6 dB width) | 18 – 23 |
| trill rate | 33 Hz and 49 Hz, an octave apart, in two recordings |

## Regenerating the tables

```sh
cd tools/analysis
python3 callers.py                 # the census
python3 phrases.py                 # the temporal structure
python3 contours.py --sweep        # the gates, re-swept
python3 contours.py --emit         # -> src/dsp/contours_generated.h
python3 frogs.py --emit            # -> src/dsp/chorus_generated.h
python3 bed.py --emit              # -> src/dsp/bed_generated.h
cd ../../build && python3 ../tools/analysis/makepresets.py
```

and to check the result against the library:

```sh
cd build && python3 ../tools/analysis/fit.py
```

and, because a number agreeing with a number proves nothing about the sound:

```sh
cd build && python3 ../tools/analysis/listen.py   # -> !dev/listen-v1/
```
