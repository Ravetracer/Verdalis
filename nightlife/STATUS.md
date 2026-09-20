# NightLife status

Version 0.1.0, the suite's ninth plugin and the first with **four sounding
layers**. Nothing has been released yet.

## What is measured

45 field recordings: 12 wolf and dog, 9 owl, 2 screech owl, 1 scops owl, 4 fox,
1 loon, 13 frog, and 2 performed "werewolf" recordings that are measured, named
and deliberately given to no caller.

| | |
|---|---|
| calls segmented from the six callers | **225** |
| usable contours after the quality gate | **208** |
| archetypes shipped | **48** (8 per caller), 43 kB |
| croaks segmented | **627** across 13 recordings |
| croak types shipped | **8**, one per close recording |
| factory presets | **16** |
| parameters | **75** |

## What the plugin does that the suite has not done before

- **The fit budget is per second of call, not per call.** ChirpParade fits a
  fixed 96 terms to every syllable and records in its own TODO that this makes
  the budget a hidden duration filter. Over calls running 75 ms to 3.1 s it is
  not survivable: at 96 terms a howl reproduces 97 % of its tracked contour's
  path and a scops owl pip 253 %. 55 terms per second holds all six callers
  between 77 % and 115 %, and each archetype carries its own count.

- **The chorus is not a Poisson process.** Measured Fano factor of croak
  arrivals: 0.90 / 0.81 / 0.59 / 0.30 at 50 ms / 250 ms / 1 s / 4 s, against a
  Poisson process's 1.0 at every window. CrackleBlaze measured 3.90 at one
  second for fire; frogs are the other side of it. Independent per-frog clocks
  do *not* reproduce it (Palm-Khintchine: the superposition of many renewal
  processes tends to Poisson), so the pond keeps one period and the frogs take
  places in it. At the default the render measures 0.90 / 0.79 / 0.56 / 0.39.

- **The bed's filterbank is solved offline and then corrected against the
  engine's own output.** The four plugins that carry a measured bed solve their
  banks at run time because their controls reshape the target; nothing here
  does. The analytic solve alone left the rendered bed 9.6 dB rms from the curve
  it was fitted to, because the model of an octave bandpass is not the filter the
  engine builds; seven correction passes against rendered audio brought it to
  **3.9 dB rms**.

## What is fitted, and how well

Each archetype fired on its own, measured with the same estimators that measured
the references (`tools/analysis/fit.py`):

| caller | pitch | vs measured | length | vs measured |
|---|---|---|---|---|
| Wolf | 543 Hz | +12 % | 1141 ms | −12 % |
| Owl | 550 Hz | +11 % | 160 ms | −12 % |
| Screech | 1407 Hz | +12 % | 400 ms | −12 % |
| Scops | 1416 Hz | +11 % | 800 ms | −12 % |
| Fox | 1182 Hz | +12 % | 811 ms | −15 % |
| Loon | 1259 Hz | +11 % | 240 ms | −10 % |

**Both biases are uniform across all six**, which is what makes them worth
believing rather than fixing one caller at a time:

- The pitch bias is *definitional*. The contour table is anchored at each call's
  loudest moment, so `Pitch` places the pitch you hear; the census figure is a
  median over every loud frame of every call, which for these contours sits 11 %
  lower. ChirpParade anchors the same way and for the same reason.
- The length bias is the segmenter, not the engine — it measures the voiced
  portion of a call and cannot see the quiet ends. ChirpParade measures 11–18 %
  short on the same estimator for the same reason.

Neither has been compensated for, deliberately: a default tuned to cancel an
estimator's bias is a plugin fitted to its own analysis rather than to the world.

## The ear, so far

The first listening pass has happened and it was positive, which is worth
recording because the measurements alone would not have predicted all of it:

- **The insect layer is the best-sounding thing in the plugin by ear, and it is
  the worst-measured.** It rests on a cricket band isolated from *behind* a
  scops owl and two frog choruses, with two trill rates that disagree by an
  octave — and it was judged more convincing than InsectSwarm's cricket
  stridulation layer, which is built from a click train ringing a resonant body
  and is fitted to references of the insect itself. A narrow band of noise with
  a trill on it may simply be closer to what a cricket *is* than a tymbal model
  is. That is a finding about InsectSwarm rather than about this plugin.

What has **not** been done is the systematic pass: each preset beside the
recording its numbers came from, sat through end to end.
`tools/analysis/listen.py` writes the fifteen loudness-matched pairs into
`!dev/listen-v1/` for it. Until that is done nothing here is finished — the
suite's own rule is that a number agreeing with a number proves nothing about
the sound, and ChirpParade 0.1.0 agreed with twenty of them.

The other open items, in short: the insect layer rests on the weakest
measurement in the plugin (the library has no recording of insects alone); the
screech owl's whinny is the one call type the contour table under-resolves; the
loon has one reference recording and eleven usable contours behind its eight
archetypes.
