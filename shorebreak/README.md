# ShoreBreak

> Part of the **[Verdalis Plugin Suite](../README.md)** — synthesised weather,
> no samples. Built and released from the suite root.

A native **CLAP** and **VST3** instrument that generates ocean surf — entirely by
synthesis. There are no samples anywhere in this project: every break, every
sheet of foam, every bubble and the swell underneath are computed from noise,
oscillators and filters at run time. No two shores are ever alike.

Play a MIDI note and the sea comes in for as long as you hold it.

- 49 parameters covering the breaking waves, the foam they leave, the wash back
  down the shore, individual bubbles, the swell bed, distance and a full ADSR
- 17 factory presets from a distant roar to a shore in uproar, each fitted
  against a real recording of the thing it is imitating
- A four-layer model of a breaking wave — break, foam, wash, bubbles — with the
  spectral slope, the decay and the precursor bubbling taken from published
  measurements of plunging breakers
- Galvin's four breaker types (spilling, plunging, collapsing, surging) as a
  parameter, because they measurably differ in the slope above 1.5 kHz
- Exposed to the host through CLAP preset discovery, so presets appear in the
  host's own browser
- Sample-accurate note and parameter handling, host modulation support,
  bounded CPU cost
- The suite's plugin window, in ShoreBreak's own sea-green theme: every
  parameter, a preset browser, typed value entry and a wave-activity meter,
  resizable, with no toolkit dependency

## How a wave is made

A breaking wave is not one sound but four, and they overlap:

| Layer | What it is |
|---|---|
| **Break** | The crest collapsing into a cloud of bubbles. Noise through a bandpass centred where the cloud resonates — 400–1600 Hz in the references — sweeping downwards as the cloud grows. It rises over a quarter to a whole second rather than striking. |
| **Foam** | The sheet of small bubbles left behind. Highpassed hard: the references measure −82 dB at 50 Hz, so it has no low end at all. It outlives the break by up to three times, which is why the quiet stretches between waves measure *brighter* than the waves. |
| **Wash** | Water running up the shore and draining back through whatever the shore is made of. A mid band with a slow random walk on it; coarser shores rattle more. |
| **Bubbles** | Individual resonators popping in the foam, each ringing at the pitch its radius gives it. |

Under all of it is the swell bed: water moving without breaking, slowly
breathing. At distance it is most of what is left, because air absorption has
taken the top off everything else.

`tools/analysis/README.md` records every measurement these were fitted to, and
the published work behind the spectral slope, the size-dependent decay and the
precursor.

## The manual

This README is the developer's view. The user-facing manual is
`docs/manual.md`, which builds to a PDF with
`../shared/tools/make-manual.sh shorebreak` and ships in the release archives.
Its parameter reference and preset library are generated from the plugin
itself, so a new parameter documents itself.

## Build and install

Requires a C++17 compiler, CMake ≥ 3.16 and the CLAP headers.

```sh
./install.sh
```

That configures, builds, runs the self-test and installs to
`~/.clap/ShoreBreak/`. Override the destination with
`SHOREBREAK_PREFIX=/some/where ./install.sh`.

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

## Status

Early. The engine and the window are complete and the self-test passes, but the
preset library is a first fit: see `STATUS.md` for what is measured and
`TODO.md` for what is known not to match yet.
