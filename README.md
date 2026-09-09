<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="_designs/verdalis-logo-horizontal-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="_designs/verdalis-logo-horizontal.svg">
    <img alt="Verdalis" src="_designs/verdalis-logo-horizontal.svg" width="560">
  </picture>
</p>

<p align="center">
  <strong>A suite of native CLAP instruments that synthesise the weather.</strong><br>
  Every sound the instruments make is computed at run time.
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
| **[ShoreBreak](shorebreak/)** — ocean surf, from a distant roar to a shore in uproar | ocean waves | in progress |
| **[SkyHowl](skyhowl/)** — wind, from a soft breath to a howling storm | winds, storms | in progress |
| **[ChirpParade](chirpparade/)** — birds, from a single chirp to a dawn chorus | bird chirps | in progress |
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

**ShoreBreak** — a breaking wave in four overlapping layers: the crest
collapsing into its bubble cloud, the foam it leaves, the wash back down the
shore and the individual bubbles popping in it. 49 parameters, 17 presets, and
Galvin's four breaker types, which measurably differ in the slope above 1.5 kHz.

**SkyHowl** — built on the fact that wind is silent: a flow field that makes no
sound at all, and the sources it drives. Aeolian tones shed at the Strouhal
frequency, so a howl's pitch swoops with every gust — which the reference
library confirms. 53 parameters, 23 presets, and rustling foliage as a bonus
layer.

**ChirpParade** — a bird syllable *is* its frequency contour, so 67 of them were
measured off real recordings and ship as coefficients that drive the oscillator
directly. Above them a one-sided valve, which is where every even harmonic comes
from, and a tracheal resonance that follows the pitch the way a beak does.
61 parameters, 22 presets, ten species, woodpecker drumming as a layer of its
own. Plays one shot on a note and a flock while it is held.

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

Or build the whole suite and pack the release archives:

```sh
./release.sh 0.2.0
```

That produces one `.zip` per plugin per platform, one per platform for the whole
suite, and one containing everything. Each is self-contained, with the plugin,
its presets, its manual as a PDF, and install instructions for that platform.
Pass `--tarball` to get `.tar.gz` alongside.

## Documentation

Every plugin has a manual: what it models, how the synthesis works, a reference
for every parameter and every factory preset, and notes on using it in a host.
It ships as a PDF in the release archives, and its prose source is
`<plugin>/docs/manual.md`. Build one with:

```sh
shared/tools/make-manual.sh rainyday        # -> dist/manuals/
```

The parameter reference and the preset library are generated from the plugin
itself rather than written by hand, so they cannot drift from the build they
describe.

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
