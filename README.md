<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="_designs/verdalis-logo-horizontal-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="_designs/verdalis-logo-horizontal.svg">
    <img alt="Verdalis" src="_designs/verdalis-logo-horizontal.svg" width="560">
  </picture>
</p>

<p align="center">
  <strong>A suite of native CLAP and VST3 instruments that synthesise the weather.</strong><br>
  Every sound the instruments make is computed at run time.
</p>

---

Verdalis is a family of audio plugins built around a single rule: **nothing is
sampled.** Every sound is generated from noise, oscillators, filters and the
physical statistics of the phenomenon being modelled. No two instances ever
produce the same output.

Each plugin is a self-contained instrument with its own synthesis engine, its
own factory presets — fitted against real reference recordings — and a
hand-drawn plugin window with no toolkit dependency. Every one ships as both a
**CLAP** and a **VST3**, for Linux and Windows; the two are the same plugin,
built from one implementation, so they share an engine, a parameter set, a
preset library and a window.

## The plugins

| Plugin | Simulates | Version |
|--------|-----------|---------|
| **[RainyDay](rainyday/)** — rain, from a single drip to a tropical monsoon | rain | 1.8.0 |
| **[ThunderClap](thunderclap/)** — thunder, from a distant rumble to an overhead strike | thunder | 1.4.0 |
| **[ShoreBreak](shorebreak/)** — ocean surf, from a distant roar to a shore in uproar | ocean waves | 0.2.0 |
| **[SkyHowl](skyhowl/)** — wind, from a soft breath to a howling storm | winds, storms | 0.2.0 |
| **[ChirpParade](chirpparade/)** — birds, from a single chirp to a dawn chorus | bird chirps | 0.6.0 |
| **[RiverFlow](riverflow/)** — running water, from a river's roar to one drop on stone | rivers, streams | 0.2.0 |
| **[CrackleBlaze](crackleblaze/)** — fire, from a cottage hearth to a burning roof | fire | 0.1.0 |

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

**RiverFlow** — built on a measurement that reads as a criticism and is not: the
smoothest third of its reference library is statistically indistinguishable from
shaped Gaussian noise, so a bed of it is a complete model of those rivers. The
other end measures thirteen times as grainy, and the band it happens in says
whether that is a creek dabbling between stones or drops off an overhang. 50
parameters, 20 presets, six octave-band colours taken as cluster centroids from
77 recordings, and a bed whose bands each wander on their own because the
measured correlation between neighbours is 0.07.

**CrackleBlaze** — a fire is the opposite of a river: a median crest factor of
31.7 dB against a river's 19.6, which is a quiet bed with very loud, very short
things on top of it. Its crackles arrive on three timescales that disagree —
exactly Poisson at 50 ms, twice Poisson below 10 ms, and four to twenty times
Poisson at one second — so the spawner is a wandering rate, a Poisson process at
it, and a one-to-three pulse train per arrival. A crackle is measurably a click
and not a ring, so no resonator is fitted. 47 parameters, 18 presets, five
octave-band colours taken from the beds of 17 recordings with their crackles
gated out first.

**ChirpParade** — a bird syllable *is* its frequency contour, so 72 of them were
measured off real recordings and ship as coefficients that drive the oscillator
directly, each at its own measured duration. Above them a one-sided valve, which
is where every even harmonic comes from, and a tracheal resonance that follows
the pitch the way a beak does. 62 parameters, 18 presets, nine species,
woodpecker drumming as a layer of its own. Plays one shot on a note and a flock
while it is held.

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

### SDKs

No SDK is included in this repository — they are third-party code and not ours
to redistribute. Check them out yourself into `CLAP/` beside the plugins, where
the build finds them automatically:

```sh
mkdir -p CLAP && cd CLAP

# required for a CLAP build
git clone https://github.com/free-audio/clap.git

# additionally required for a VST3 build
git clone https://github.com/free-audio/clap-wrapper.git
git clone --branch v3.8.1_build_84 https://github.com/steinbergmedia/vst3sdk.git
cd vst3sdk && git submodule update --init base pluginterfaces public.sdk && cd ..
```

The first repository is all that is required to build a CLAP. `CLAP/` is
gitignored. If you keep the CLAP SDK elsewhere, point CMake at it with
`-DCLAP_INCLUDE_DIR=/path/to/clap/include`.

The VST3 is produced by the [clap-wrapper](https://github.com/free-audio/clap-wrapper),
which hosts the same plugin behind the VST3 API — there is no separate VST3
codebase. It needs **VST 3.8 or newer**, the first MIT-licensed release of the
SDK; earlier ones are GPLv3 or a proprietary Steinberg agreement. The wrapper
needs one small patch to compile against 3.8, kept in `shared/patches/`.

VST is a trademark of Steinberg Media Technologies GmbH.

## Installing

CLAP plugins are loaded from `~/.clap` on Linux and
`%COMMONPROGRAMFILES%\CLAP` on Windows; VST3 bundles from `~/.vst3` and
`%COMMONPROGRAMFILES%\VST3`. Each plugin's `install.sh` installs the CLAP to the
right place by default, and `./install.sh --vst3` installs both. Release
archives ship with the same layout, so copying the folders in by hand works too.

Tested with Bitwig Studio and Reaper.

## How this was built

**These plugins were vibe coded.** The code was written with AI — Claude — doing
most of the typing, working from direction, reference recordings and listening
notes. That is worth saying plainly on the front page rather than leaving anyone
to work it out from the commit history.

It is not a claim that nothing was checked. Each model is fitted against real
reference recordings and the fit is measured: each plugin's `STATUS.md` records
what it was fitted to, which explanations were tested and discarded, and where
the model still falls short. Every build runs a self-test, every VST3 passes
Steinberg's validator, and nothing is accepted until it has been listened to
against the reference it imitates — a number agreeing with a number proves
nothing about how something sounds.

It is also free software given away as-is, tested by one person on Linux and
Windows in Bitwig Studio and Reaper. Reports from other hosts are welcome.

## License

Released under the terms in [LICENSE](LICENSE). Reference recordings and papers
used during development are not part of this repository and are not
redistributable.
