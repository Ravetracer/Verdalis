# ChirpParade

> Part of the **[Verdalis Plugin Suite](../README.md)** — synthesised weather.
> Built and released from the suite root.

A native **CLAP** and **VST3** instrument that generates birds — by synthesis.
Every syllable
is computed at run time from a measured frequency contour driving an oscillator.
No samples, and no two calls alike.

Play a note and one bird sings once. Hold it and a flock carries on by itself.

- 62 parameters covering the syllable, the voice and the tube above it, the
  phrase, the flock, woodpecker drumming, distance and a full ADSR
- 18 factory presets, each checked by rendering it back and measuring it
- **A bird syllable *is* its frequency contour.** 72 contours measured off real
  recordings — the medoids of 5749 clustered syllables out of 88 field
  recordings, every one isolated to a single bird — ship as cosine coefficients
  and drive the oscillator directly, each at its own measured duration
- **Timbre is a valve, not a filter.** `Voice` is the fraction of each cycle the
  syrinx is shut. At zero it passes a pure sine, which is what 59 % of the
  library's syllables are; closing it makes the airflow a one-sided pulse with
  the harmonic stack a goose has
- **`Partials` reaches for the recording itself.** Each archetype also carries
  the measured balance between its first six partials *across* the syllable —
  because that balance moves 4.4 dB over one syllable, and a fixed valve through
  a fixed tract cannot do that at all
- Nine species, each a measurement: pitch register, syllable length, harmonic
  richness, roughness and rate are the medians of the recordings of that bird
- Woodpecker drumming as a separate layer, because it is sonation rather than
  voice: a bill against wood, in a roll that accelerates
- One shot and drone at once: a note fires a deliberate phrase that always
  completes, and a flock of individuals calls and answers around it
- 67× realtime for a twelve-bird dawn chorus (60 s rendered in 0.90 s)
- The suite's plugin window in ChirpParade's own finch-gold theme, with a
  sonogram scrolling across its header

## How a bird is made

```
  contours_generated.h                       one oscillator        one tube
  ────────────────────                       ──────────────        ────────
  pitch curve  ──►  read out over        ──► phase accumulator  ──► trachea
  level curve       the syllable             one-sided valve         beak,
                    Sweep · Detail · Skew    Voice = closure         tracking
```

| Layer | What it is |
|---|---|
| **The contour** | Two measured curves read out over the syllable: pitch in octaves about its loudest moment, level in dB. `Contour` chooses the archetype, `Detail` how much of its fine motion survives, `Sweep` how far it travels, `Skew` how its time is warped. |
| **The valve** | A phase accumulator at the contour's frequency through a one-sided hinge. Air passes only while the labia are apart, and *that* is where the even harmonics come from — a symmetric oscillator has none at all. |
| **The partials** | Six measured amplitude curves, one per harmonic, phase-locked to the same accumulator. `Partials` crossfades the valve into them, scaled by how much of that syllable's energy the measurement actually accounted for. |
| **The tract** | The trachea as a closed tube at `c/4L`, with the beak both raising the resonance and making it follow the pitch the way a songbird's gape does. |
| **The phrase** | Syllables at a rate of their own, with a motif, drift and variation, separated by a gap 5.9× the one inside them. |
| **The flock** | Birds as individuals, each with its own pitch, position, distance and voice, calling as a Poisson process and answering each other. |
| **The drum** | Not a voice: a broad wooden resonance excited by a contact, in a roll that accelerates by the measured 23 %. |

A phrase owns no oscillator and makes no sound: it is a plan, which is what
keeps the pool cheap enough to be generous with.

## Why contours, and not a physical model

The first version of this plugin modelled the syrinx from first principles —
the Gardner–Laje–Mindlin labium, two gestures, the phase between them as the
syllable's shape — and fitted it to the medians of the reference library. Every
number agreed. It sounded nothing like a bird.

The medians had been measured through a 21 ms analysis window:

| | through a 21 ms window | at 0.33 ms |
|---|---|---|
| peak pitch slew | **2.7 oct/s** | **20 – 440 oct/s** |
| direction changes per syllable | **0.25** | **2 – 40** |

A real syllable is a scribble. One sinusoidal gesture cannot draw one, and no
amount of correct physics above it helps. `tools/analysis/README.md` records the
whole thing, including the eleven bugs the measurements found and the two in the
measurement code itself.

What replaced it is the procedure van Hunter Adams uses to synthesise a northern
cardinal, and the one the Bitwig Grid patch in `!dev` uses: read the contour off
the recording and drive an oscillator with it. Adams does it by drawing lines on
a spectrogram and fitting one sine term; this does it automatically, over the
whole library, with forty. Refitting a real syllable lands within **41 cents of
pitch and 0.5 dB of level**.

`contours_generated.h` holds **9656 floats** — 38 KB, no audio: 71 archetypes ×
(40 pitch + 24 level + 6 × 12 harmonic) coefficients. A contour is a formula in
exactly the sense `f(x) = -260·sin(-πx/5200) + 1740` is.

### What was tried and rejected

**Mel spectrograms with Griffin-Lim resynthesis** (the SoundPlot approach).
librosa's defaults are a 92.9 ms window at 22.05 kHz — four times coarser than
the window that broke 0.1.0 — and Griffin-Lim's worst case is exactly
frequency-modulated transient material. A mel spectrogram at usable resolution
is also the recording with its phase discarded, which is the wrong side of the
suite's line.

**librosa's pYIN** for the pitch tracking, swept over three frame lengths and
three confidence gates. On the same syllables it discards 80 % of the contour's
path and cuts the peak slew from 424 to 16 oct/s: its Viterbi smoothing assumes
slowly-varying pitch, which birdsong violates. `tools/analysis/setup-venv.sh`
still installs it, and `contours.py` will use it if asked, but the pipeline does
not.

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

`CLAP_INCLUDE_DIR` is auto-detected from a few common locations, including
`../CLAP/clap/include`. The installed layout matters: the plugin locates its
factory presets by looking for a `presets` directory **next to its own binary**.

## Checking a change

```sh
./build/chirpparade-render --plugin ./build/ChirpParade.clap --selftest
./build/chirpparade-render --plugin ./build/ChirpParade.clap --all --outdir /tmp/birds
./build/chirpparade-render --defaults > presets/new.chirpparade
```

`--defaults` prints every parameter at its table value in the preset format. The
factory library is authored from it, so a preset cannot omit a parameter or
carry a stale default.

Then measure what came out:

```sh
cd tools/analysis
python3 contours.py                 # extract, refit and resynthesise references
python3 contours.py --wav /tmp/ab   # write original/resynth pairs to listen to
python3 contours.py --emit          # regenerate contours_generated.h
python3 fit.py                      # species, presets, calibrations
```

Regenerating the contour table needs the reference recordings in `!dev`, which
are not part of this repository.

**And then listen.** The lesson of 0.1.0 is that 61 parameters can agree with
4268 syllables' worth of statistics and still sound wrong. `contours.py --wav`
exists to make the A/B easy.

## Status

The engine, the parameter set and the window are complete, the self-test passes,
all 22 presets meet their own targets and renders are byte-identical across
44.1, 48 and 96 kHz. See `STATUS.md` for what is measured and `TODO.md` for what
is known not to match yet.
