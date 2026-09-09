# ChirpParade

> Part of the **[Verdalis Plugin Suite](../README.md)** — synthesised weather.
> Built and released from the suite root.

A native **CLAP** instrument that generates birds — by synthesis. Every
syllable comes out of a model of the syrinx itself, computed at run time. No
samples, and no two calls alike.

Play a note and one bird sings once. Hold it and a flock carries on by itself.

- 61 parameters covering the syllable, the syrinx and the tube above it, the
  phrase, the flock, woodpecker drumming, distance and a full ADSR
- 22 factory presets from a single chirp to a dawn chorus, each fitted against
  real recordings and checked by rendering it back
- **A bird is an oscillator held just past its bifurcation.** The voice is the
  Gardner–Laje–Mindlin model of a syringeal labium, driven by two gestures:
  air sac pressure and syringeal tension
- **A syllable's shape is one number** — the phase between those two gestures.
  Up-sweeps, down-sweeps, arches and dips are four readings of one knob, not
  four oscillators
- **Timbre is not a filter.** It is the ratio of pressure to tension, which
  takes the oscillation from a sinusoidal whistle to the relaxation regime a
  crow has
- Ten species, every one of them the median of the recordings of that bird
- Woodpecker drumming as a separate layer, because it is sonation rather than
  voice: a bill against wood
- One shot and drone at once: a note fires a deliberate phrase, and a flock of
  individuals calls and answers around it
- Exposed to the host through CLAP preset discovery, sample-accurate note and
  parameter handling, host modulation, bounded CPU cost
- The suite's plugin window in ChirpParade's own finch-gold theme, with a
  sonogram scrolling across its header

## How a bird is made

```
    two gestures            one oscillator           one tube
    ────────────            ──────────────           ────────
    pressure  B(t)  ──┐
                      ├──►  x' = y                   trachea, c/4L
    tension   ε(t)  ──┘     y' = -εx - Cx²y + By  ──► beak, tracking
                                                     ─────────────►  radiated
                            the flow past the labia is one-sided
```

| Layer | What it is |
|---|---|
| **The gestures** | Two sinusoids over the syllable. B switches the oscillation on and sets its amplitude; ε sets its frequency. The *phase* between them is the syllable's shape. |
| **The syrinx** | The labium as a van der Pol oscillator. Its one shape parameter `μ = B/√ε` takes it from a sinusoid to a relaxation oscillation — from a whistle to a crow. |
| **The airflow** | Air only gets past while the labia are apart, so the source is the *one-sided* part of the gap. That rectification is where every even harmonic comes from. |
| **The tract** | The trachea as a closed tube at `c/4L`, with the beak both raising and tracking the resonance the way a songbird's gape does. |
| **The phrase** | Syllables at a rate of their own, with a motif, drift and variation, separated by a gap 5.9× the one inside them. |
| **The flock** | Birds as individuals, each with its own pitch, position, distance and voice, calling as a Poisson process and answering each other. |
| **The drum** | Not a voice: a broad wooden resonance excited by a contact, in an accelerating roll. |

A phrase owns no oscillator and makes no sound: it is a plan, which is what
keeps the pool cheap enough to be generous with.

`tools/analysis/README.md` records every measurement the model was fitted to —
4268 syllables in 58 recordings — the published work behind the syrinx model,
and the bugs the measurements found, each of which had survived sounding
plausible.

## The one prediction the references can falsify

If a syllable really is one turn of two coupled gestures, its envelope and its
pitch contour are two sinusoids of the same period with a phase between them —
so the shape a sonogram reader names follows from that phase alone. Arches must
correlate pitch with level; dips must anticorrelate; sweeps must sit near zero.

The library confirms the ordering, monotonically, across all six shapes.
`tools/analysis/gestures.py` is the test and *Where the model is not there yet*
in `STATUS.md` is honest about how strongly.

## The manual

This README is the developer's view. The user-facing manual is
`docs/manual.md`, which builds to a PDF with
`../shared/tools/make-manual.sh chirpparade` and ships in the release archives.
Its parameter reference and preset library are generated from the plugin
itself, so a new parameter documents itself.

## Build and install

Requires a C++17 compiler, CMake ≥ 3.16 and the CLAP headers.

```sh
./install.sh
```

That configures, builds, runs the self-test and installs to
`~/.clap/ChirpParade/`. Override the destination with
`CHIRPPARADE_PREFIX=/some/where ./install.sh`.

Manually, if you prefer:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build            # defaults to ~/.clap
```

`CLAP_INCLUDE_DIR` is auto-detected from a few common locations, including
`../CLAP/clap/include` — which is where the Verdalis suite keeps the SDK, at
the suite root beside this plugin folder. Pass it explicitly if the configure
step cannot find `clap/clap.h`.

The installed layout matters: the plugin locates its factory presets by looking
for a `presets` directory **next to its own binary**, so keep them together.

## Checking a change

The offline host renders, lists and self-tests without a DAW:

```sh
./build/chirpparade-render --plugin ./build/ChirpParade.clap --selftest
./build/chirpparade-render --plugin ./build/ChirpParade.clap --all --outdir /tmp/birds
./build/chirpparade-render --defaults > presets/new.chirpparade
```

`--defaults` prints every parameter at the value the table gives it, in the
preset format. The factory library is authored from it, so a preset cannot omit
a parameter or carry a stale default.

Then measure what came out:

```sh
cd tools/analysis && python3 fit.py            # species, presets, calibrations
```

## Status

Early. The engine, the parameter set and the window are complete, the self-test
passes and all 22 presets meet their own targets — see `STATUS.md` for what is
measured and `TODO.md` for what is known not to match yet.
