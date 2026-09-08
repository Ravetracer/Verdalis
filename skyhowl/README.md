# SkyHowl

> Part of the **[Verdalis Plugin Suite](../README.md)** — synthesised weather,
> no samples. Built and released from the suite root.

A native **CLAP** instrument that generates wind — entirely by synthesis. There
are no samples anywhere in this project: every gust, every aeolian tone and every
rustling leaf is computed from noise, oscillators and filters at run time. No two
winds are ever alike.

Play a MIDI note and the wind blows for as long as you hold it.

- 53 parameters covering the flow itself, the airflow bed, the low-frequency
  buffet, the aeolian tones, foliage, terrain, distance and a full ADSR
- 23 factory presets from a soft breath to a howling storm, each fitted against
  a real recording of the thing it is imitating
- **Wind is silent**, so the engine is two halves: a flow field that makes no
  sound at all, and the sources it drives
- Howling as it physically is — vortex shedding at the Strouhal frequency, so
  the pitch rises and falls with the wind speed
- Rustling foliage as a bonus layer: a Poisson stream of leaf clicks, eight
  foliage types from broadleaf to bare branches
- Terrain roughness taken from the tabulated roughness lengths, so *Forest*
  really is 2.5× as gusty as *Plain*
- Exposed to the host through CLAP preset discovery, so presets appear in the
  host's own browser
- Sample-accurate note and parameter handling, host modulation support,
  bounded CPU cost
- The suite's plugin window, in SkyHowl's own dust-coral theme: every parameter,
  a preset browser, typed value entry and a gust meter, resizable, with no
  toolkit dependency

## How wind is made

Air in motion radiates essentially nothing. Everything a listener calls wind is
the flow meeting something, so the model splits in two:

| Layer | What it is |
|---|---|
| **The flow** | A wind speed U(t) per channel, and completely silent. Mean speed, plus turbulence shaped to Kolmogorov's `f^-5/3`, plus discrete gusts, plus a squall drift below 0.1 Hz. |
| **Airflow** | The broadband bed: turbulent noise through a four-pole tilt. Its amplitude follows U³, because aerodynamic sound power from flow over a rigid surface goes as U⁶ (Curle). |
| **Buffet** | The low end: the pressure of the moving air rather than the noise it radiates, following the dynamic pressure U². |
| **Howl** | Aeolian tones. Resonant bands at `f = St·U/d`, so a 2.5 mm twig in an 8 m/s wind sheds at 640 Hz — and the pitch swoops with every gust. |
| **Rustle** | Foliage. Leaf clicks at roughly `c/2L`, spawned as a Poisson stream whose rate follows the wind. |

A gust owns no filter and no noise source, because a gust makes no sound of its
own: it is an envelope with a strength and a position, and what is heard is the
bed and the howl responding to it.

`tools/analysis/README.md` records every measurement these were fitted to, the
published work behind the Strouhal relation, the U⁶ law and the roughness table
— and the five bugs the measurements found, each of which had survived sounding
plausible.

## The one prediction the references can falsify

If a wind tone is aeolian, its pitch has to rise and fall with its loudness,
because both follow the same U. The library confirms it: the tone's pitch rises
with the level in 12 of the 17 recordings that hold a steady tone at all, and in
the four whose file names actually say *howling* the correlation runs from 0.52
to 0.87.

The one clear exception is a cave, which is what the physics predicts — a cavity
resonates at a frequency its own geometry fixes.

## The manual

This README is the developer's view. The user-facing manual is
`docs/manual.md`, which builds to a PDF with
`../shared/tools/make-manual.sh skyhowl` and ships in the release archives.
Its parameter reference and preset library are generated from the plugin
itself, so a new parameter documents itself.

## Build and install

Requires a C++17 compiler, CMake ≥ 3.16 and the CLAP headers.

```sh
./install.sh
```

That configures, builds, runs the self-test and installs to
`~/.clap/SkyHowl/`. Override the destination with
`SKYHOWL_PREFIX=/some/where ./install.sh`.

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
