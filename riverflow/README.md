# RiverFlow

**Synthesised running water — a CLAP instrument for Linux and Windows.**

RiverFlow generates rivers, creeks and waterfalls. It contains no samples: the
bed of noise, every pocket of air trapped between the stones and every drop that
lands on them are computed while the plugin plays, so no two takes are alike and
there is no loop point to find.

It covers the whole span from a river's roar down to a single drop on wet stone,
and it does that because the reference library says the two ends are different
things rather than the same thing at different volumes.

Part of the [Verdalis](https://github.com/Ravetracer/Verdalis) suite.

## Some rivers really are white noise

The plugin's design rests on one measurement. `tools/analysis/grain.py` compares
each of the 77 reference recordings against a Gaussian white-noise control put
through the same statistics, and the smoothest third of the library **lands on
the control row** — a coefficient of variation of 0.30 against the control's 0.30
at 200-800 Hz, 0.13 against 0.12 at 2-6 kHz. Those recordings are not "like"
noise; by these measures they are indistinguishable from it, and the event
detector finds too few discrete events in them to characterise at all.

The other end of the library measures 2 to 13 times the control, and the band it
happens in says what kind of water it is: around 500 Hz for a creek dabbling
between stones, and 2-6 kHz for drops off an overhang — where the trickle
reference measures **1.64 against a control of 0.12**.

So RiverFlow is a bed plus two populations of discrete events, and moving
between them is what the instrument does.

## How the water is made

- **Flow** — the bed. One noise source per band per channel through eight
  octave-wide bandpasses, driven at the measured octave-band gains of whichever
  of six colours is selected. The six are not names someone invented: they are
  the six clusters the reference library falls into, shipped as the measured
  centroid of each. `Blend` crossfades between them, so they are a continuum.

  Each band's level wanders on its own, and more at the bottom than the top:
  measured, the correlation between one band's 100 ms envelope and its
  neighbour's is 0.07-0.25, and the 125-250 Hz band moves 2.4 times as far as
  the 1-2 kHz band. That is why a river's low end seems to surge while its hiss
  sits still. The movement is a filtered random walk and not an oscillator,
  because the references' envelope spectra fall smoothly with no peak that
  survives from one recording to the next: a river has no rhythm.

- **Stones** — the dabbling. Water folding over a stone traps a pocket of air,
  and the pocket rings at the Minnaert pitch for its radius. Measured 1.4-9.3 a
  second at 2.3-5.8 mm — and each one is a short *cluster* rather than a single
  pocket, because the event-triggered spectrum gives a ring of about a
  millisecond while the event-triggered envelope takes 7-34 ms to fall 10 dB.
  One bubble cannot do both. A cluster shares its pan and its size, because it
  is one stone.

- **Trickle** — single drops. The impact off whatever they hit, then the tiny
  pocket they entrain. Measured 5-25 a second at 0.42-1.72 mm, ringing 2-8 kHz.
  `Stone` is what they are landing on and `Splash` is the wash where the water
  is deeper — the references' drop decays are bimodal, 8-13 ms against 56-86 ms.

- **Plunge** — the pool under a fall. A cloud of bubbles rings far below any
  bubble in it, and three modes at Xue et al.'s measured 1.00 / 1.53 / 1.90
  ratios read as a body of water where one reads as a tuned pipe.

- **Reach** — distance, air absorption, stereo width and the banks the water
  runs between, which decide how much of it comes back.

Nothing below 60 Hz is generated, and that is measured too: four references
carry up to a quarter of their energy there, and in every one of them its
envelope is uncorrelated with the water above it. It is wind and handling noise
on the microphone. Synthesising it would be fitting the recordist's afternoon.

## Presets

20 factory presets, each fitted to one named reference recording and each
checked by rendering it back and measuring it. 15 of the 20 match their
reference within 6 dB in every octave band. They run from `distant_river` and
`big_falls`, which are honestly nothing but shaped noise, through
`bubbling_creek` and `stony_creek` to `hanging_trickle` at the far end.

## Build and install

From this folder:

```sh
./install.sh          # configure, build, self-test, install to ~/.clap/RiverFlow
```

Or by hand:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

Needs a C++17 compiler, CMake 3.16, the CLAP headers (found automatically in
`../CLAP/clap/include`), and X11 plus Cairo for the window. Without Cairo the
plugin still builds, without a window.

Offline tools, built unless `-DRIVERFLOW_BUILD_TOOLS=OFF`:

```sh
./build/riverflow-render --plugin ./build/RiverFlow.clap --selftest
./build/riverflow-render --plugin ./build/RiverFlow.clap --all --outdir /tmp/wav \
    --seconds 20 --rate 48000 --param randomseed=7
./build/riverflow-guihost ./build/RiverFlow.clap "" 8
```

## The manual

A PDF manual is built from `docs/manual.md`, with the parameter reference and
the preset library generated from the plugin's own tables so they cannot drift:

```sh
../shared/tools/make-manual.sh riverflow
```

## Status

See [STATUS.md](STATUS.md) for what works and what is measured, and
[TODO.md](TODO.md) for what does not fit yet. The numbers behind all of it are
in [tools/analysis/README.md](tools/analysis/README.md).

MIT licensed.
