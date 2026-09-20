# NightLife

**A synthesised night — a CLAP and VST3 instrument for Linux and Windows.**

NightLife generates the sounds of a night: a wolf howling across a valley, an
owl in a wood, a fox screaming in a field, a loon over a lake, a pond full of
frogs, a field of crickets, and the quiet the whole of it sits on. It contains
no samples. Every howl, every croak and every cricket is computed while the
plugin plays, so no two takes are alike and there is no loop point to find.

Part of the [Verdalis](https://github.com/Ravetracer/Verdalis) suite.

## Four layers, three different measurements

A night is not one texture, so the plugin is not one mechanism. Each layer is
built from the measurement that actually describes it — and they are not the
same kind of measurement.

| Layer | What it is | What it is built from |
|---|---|---|
| **Callers** | wolf, owl, screech owl, scops owl, fox, loon | 48 measured pitch contours |
| **Chorus** | frogs | 8 measured pulse trains |
| **Insects** | crickets and katydids | a narrow band, measured behind other recordings |
| **Bed** | the night itself | the third-octave curve of 45 recordings' quiet frames |

## A call is its frequency contour

This is ChirpParade's finding and NightLife is its second user. A physical model
of the animal, fitted to aggregate statistics, measures correctly on twenty
quantities and sounds like nothing alive — that plugin proved it the expensive
way.

So the contour is measured instead. `tools/analysis/contours.py` pulls the pitch
and level contour of every well-isolated call in the reference library out of
the WAV, fits each as a cosine series in normalised call time, clusters them per
caller and keeps the medoids. **Eight archetypes per caller, 48 in all, 43 kB.**
*Contour* walks across them.

No audio is stored. A contour is a formula in the same sense as the one van
Hunter Adams reads off a cardinal's spectrogram by hand.

**The wolf's vibrato needs no oscillator.** Six of the eight wolf archetypes
carry one at a measured 2.1 to 6.4 Hz, and it sits in the series where the wolf
that was recorded put it.

### One thing had to change from ChirpParade

Its fit budget is a fixed 96 terms per syllable. Over this library, whose calls
run from a 75 ms pip to a 3.1 s whinny, a fixed count is a hidden duration
filter — measured against the contours it is supposed to reproduce:

| | length | 48 terms | 64 | 96 | 128 |
|---|---|---|---|---|---|
| Wolf | 1.19 s | 0.66 | 0.80 | **0.97** | 1.15 |
| Owl | 0.35 s | **1.19** | 1.38 | 1.84 | 2.39 |
| Scops | 0.57 s | 1.61 | 2.02 | 2.53 | 3.48 |

(the ratio of the fitted contour's path to the tracked contour's; 1.00 is right)

There is no column worth having. At 96 the wolf is right and every short caller
rings at twice the motion it was asked to draw. **55 terms per second of call**
holds all six within 20 % of it, and each archetype carries its own count.

## A croak is not a contour

A frog is a pulse train through a body resonance — 10 to 69 pulses a second, and
the rate is the species. Eight recordings were measured for their rate, pulse
count, depth, both resonances and envelope, and each is one row of the croak
table.

| | measured across the library |
|---|---|
| pulse rate | 10 – 69 Hz, median 27 |
| pulses per croak | 2 – 25 |
| first resonance | 1.5 – 3.3 kHz, median 2063 |
| second resonance | a measured **0.50** of the first |
| Q of the first | 9 – 31, median 20 |

## A frog chorus is more regular than random

Every event-spawning plugin in the suite uses a Poisson process. Here that is
measurably wrong, and the statistic that says so is the Fano factor — the
variance of the count in a window over its mean, which is exactly 1.0 for a
Poisson process at every window length.

| window | 50 ms | 250 ms | 1 s | 4 s |
|---|---|---|---|---|
| measured chorus | 0.90 | 0.81 | 0.59 | **0.30** |
| a Poisson process | 1.00 | 1.00 | 1.00 | 1.00 |
| CrackleBlaze's fire | | | **3.90** | |

Fire is clustered; a frog chorus is the other side of 1. And giving each frog a
clock of its own does not reproduce it — the superposition of many independent
renewal processes tends to a Poisson process however regular each one is, and
eight frogs on their own clocks rendered 1.15 at four seconds. **The regularity
belongs to the pond, not to the frog.** So the chorus keeps one period and each
frog takes a place in it, which is what a chorus of real anurans does. At
*Regularity* 0.95 the rendered chorus measures 0.90 / 0.79 / 0.56 / 0.39.

## A pack is individuals answering each other

Each animal has its own pitch, position, distance, voice and tempo, held for as
long as the note is. *Answer* decides how often one replies to another, shortly
after — and that behaviour, rather than the voice, is most of what makes a group
of wolves sound like a group. *Pitch Spread* is wide by default because wolves in
a chorus avoid each other's pitch, which is why a pack sounds like more animals
than it holds.

## Build and install

```sh
./install.sh                # configure, build, self-test, install to ~/.clap
./install.sh --vst3         # also build and install the VST3
```

Needs a C++17 compiler, CMake, Ninja, X11 and Cairo, and the CLAP headers beside
the suite in `../CLAP/clap/include`.

## Offline tools

```sh
nightlife-render --list                     # walk the preset discovery factory
nightlife-render --preset wolf_valley --out night.wav --seconds 20
nightlife-render --all --outdir /tmp/night  # every factory preset
nightlife-render --selftest                 # what install.sh runs
```

## The measurements

`tools/analysis/` holds the reference measurements and the generators for the
three tables the engine ships. The recordings themselves are not in this
repository and are not ours to redistribute; they live in `!dev/references/`,
outside the build. See `tools/analysis/README.md` for what each script measures
and every number it produced.

## Licence

MIT, with the suite. See `LICENSE`.
