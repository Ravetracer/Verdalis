<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="_designs/verdalis-logo-horizontal-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="_designs/verdalis-logo-horizontal.svg">
    <img alt="Verdalis" src="_designs/verdalis-logo-horizontal.svg" width="560">
  </picture>
</p>

<p align="center">
  <strong>A suite of native CLAP instruments that synthesise the weather.</strong><br>
  No samples. Everything computed at run time.
</p>

---

Verdalis is a family of audio plugins built around a single rule: **nothing is
sampled.** Every sound is generated from noise, oscillators, filters and the
physical statistics of the phenomenon being modelled. No two instances ever
produce the same output.

Each plugin is a self-contained CLAP instrument with its own synthesis engine,
its own factory presets — fitted against real reference recordings — and a
hand-drawn plugin window with no toolkit dependency.

## The plugins

| Plugin | Simulates | Status |
|--------|-----------|--------|
| **[RainyDay](rainyday/)** — rain, from a single drip to a tropical monsoon | rain | in progress |
| **[ThunderClap](thunderclap/)** — thunder, from a distant rumble to an overhead strike | thunder | in progress |
| **ShoreBreak** | ocean waves | planned |
| **SkyHowl** | winds, storms | planned |
| **ChirpParade** | bird chirps | planned |
| **RiverFlow** | rivers, streams | planned |
| **CrackleBlaze** | fire | planned |

Every plugin shares the suite's visual language, its synthesis-only principle,
and a common DSP and GUI foundation.

**RainyDay** — 42 parameters covering droplet statistics, impact surface, stereo
field, distance, space, filter and a full ADSR. 17 factory presets, each fitted
against a real recording of the thing it imitates.

**ThunderClap** — shock-wave modelling from the bolt channel outwards, with
distance-dependent air absorption. 17 presets from a distant rumble to a strike
directly overhead, and a window that flashes with the bolt.

## Build

Requires a C++17 compiler, CMake ≥ 3.16, and the CLAP headers (see below).
Linux is the primary target; Windows builds are produced by cross-compiling
with mingw-w64.

```sh
git clone https://github.com/Ravetracer/Verdalis.git
cd Verdalis
```

Then build a single plugin:

```sh
cd rainyday
./install.sh          # configure, build, self-test, install to ~/.clap
```

Or build the whole suite and pack a release archive:

```sh
./release.sh 0.1.0
```

### CLAP SDK

The CLAP SDK is **not** included in this repository — it is third-party code
from [free-audio](https://github.com/free-audio) and not ours to redistribute.
Check it out yourself into `CLAP/` beside the plugins, where the build finds it
automatically:

```sh
mkdir -p CLAP && cd CLAP
git clone https://github.com/free-audio/clap.git
```

That one repository is all that is required to build. `CLAP/` is gitignored.
If you keep the SDK elsewhere, point CMake at it with
`-DCLAP_INCLUDE_DIR=/path/to/clap/include`.

## Installing

CLAP plugins are loaded from `~/.clap` on Linux and
`%COMMONPROGRAMFILES%\CLAP` on Windows. Each plugin's `install.sh` installs to
the right place by default; release archives ship with the same layout, so
copying the folder in by hand works too.

Tested with Bitwig Studio and Reaper.

## License

Released under the terms in [LICENSE](LICENSE). Reference recordings and papers
used during development are not part of this repository and are not
redistributable.
