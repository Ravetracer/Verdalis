# CrackleBlaze

**Synthesised fire — a CLAP instrument for Linux and Windows.**

CrackleBlaze generates fire: campfires, hearths, wood stoves and large open
blazes. It contains no samples. The combustion roar, every crackle, every hiss of
steam out of wet wood and every thump of a log giving way are computed while the
plugin plays, so no two takes are alike and there is no loop point to find.

Part of the [Verdalis](https://github.com/Ravetracer/Verdalis) suite.

## A fire is not a river

The design rests on one number, and it is the first thing the reference library
says.

| | CrackleBlaze's library | RiverFlow's library |
|---|---|---|
| crest factor, median | **31.7 dB** | 19.6 dB |
| envelope variation at 4 ms | **0.85** | 0.26 |

Half of a river is its bed — a third of RiverFlow's library is statistically
indistinguishable from shaped noise. A fire is the opposite: a *quiet* bed with
very loud, very short things happening on top of it. Twelve decibels of crest
factor separate the two, and almost every decision in this plugin follows from
that ratio.

## How a crackle arrives

The second finding took three timescales to see, and they do not agree — which is
the point.

| Timescale | Measured | Poisson gives |
|---|---|---|
| 1 second | variance/mean of the count = **3.90**, up to 20.1 | 1.0 |
| 50 ms | P(gap < 50 ms) = **1.03x** the exponential | 1.0 |
| 10 ms | P(gap < 10 ms) = **1.97x**, up to 4.0 | 1.0 |
| 50 ms | branching ratio = **0.02** | 0 |

At 50 ms the arrivals are exactly Poisson and the branching ratio rules out a
cascade: one crackle does not make the next one more likely. Below 10 ms there
are twice as many gaps as Poisson allows — a crackle is a short *train* of one to
three pulses, gas breaking out of a split in the wood in stages. And at one second
the rate itself wanders, on the same seconds-long timescale as the flame's own
surging.

So the spawner has three levels and no cascade: a slowly wandering rate, a
Poisson process at that instantaneous rate, and a one-to-three pulse train per
arrival. The wander is shared with the roar, because in a real fire a flare-up is
both.

## How the fire is made

**Roar** — eight bandpasses at octave centres, driven at gains *solved* so their
sum is the measured octave-band curve rather than each band being set to it. Five
colours, shipped as the centroids of the five clusters the library's *beds* fall
into — gated first, because at a crest factor of 31.7 dB the plain spectrum of a
fire is largely the spectrum of its crackles.

**One flare, not eight.** The correlation between one octave band's 100 ms
envelope and its neighbour's is 0.54 to 0.91, median **0.85**, where a river
measures 0.07 to 0.25. A river's bands wander independently; a fire is one object,
and when it flares all of it flares. One common walk at √0.85 plus a small
independent one per band reproduces that exactly.

**Crackle** — a click, and measurably not a ring. The bed-subtracted
event-triggered spectrum has a spectral flatness of **0.67**, so there is no
resonator here: modelling a crackle as a struck piece of wood would be inventing
an object the recordings do not contain.

**Sizzle** — the same event held open. Steam leaving wet wood takes a measured
19 ms to fall 10 dB where a dry tick takes 2.5, and the two populations' spectra
agree to within 2 dB per octave. They differ in duration, not in tone — which is
why `Sap` is one control moving events between them, and why it is a parameter at
all: its measured range across the library is 0.03 to 0.57, the widest spread of
anything measured here.

**Settle** — a log giving way. Rare (a median of six a minute), 80–300 Hz, and the
only layer with no top end at all.

**Nothing below 60 Hz.** Eight of the twenty-five references carry between 27 and
60 per cent of their energy there, and in twenty-four of the twenty-five that
band's envelope is uncorrelated with the fire above it. It is traffic, ventilation
and handling noise. Those same eight carry a flat −45 dB dither plateau above
800 Hz and are discarded outright; seventeen references carry the whole fit.

## How well it fits

The default patch, rendered with the seed pinned and measured by the same scripts
that measured the library:

| | render | library |
|---|---|---|
| crest factor | 29.8 dB | 31.7 dB |
| spectral centroid | 2531 Hz | 2175 Hz |
| crackle rate | 31.3 /s | 29 /s |
| crackle prominence | 12.3 dB | 13.2 dB |
| crackle decay, −10 dB | 3.0 ms | 3.0 ms |
| interval CV | 1.49 | 1.45 |
| **Fano factor, 1 s** | **3.55** | **3.90** |
| **P(gap < 10 ms) / exponential** | **1.82x** | **1.97x** |
| **P(gap < 50 ms) / exponential** | **1.01x** | **1.03x** |
| branching ratio | 0.01 | 0.02 |
| L/R correlation | 0.61 | 0.69 |

`STATUS.md` has the full picture and `TODO.md` what is still out.

## Build and install

```sh
./install.sh          # configure, build, self-test, install to ~/.clap
```

Needs a C++17 compiler, CMake 3.16+, and the CLAP headers beside the suite in
`../CLAP/clap/include`. X11 and Cairo are the only dependencies of the window.

## Verifying

```sh
./build/crackleblaze-render --plugin ./build/CrackleBlaze.clap --selftest
./build/crackleblaze-render --plugin ./build/CrackleBlaze.clap --all \
   --outdir /tmp/fire --seconds 20 --rate 48000 --param randomseed=7
```

With `Random Seed` non-zero the output is byte-identical across runs at both 48
and 96 kHz.

## Licence

MIT, with the suite.
