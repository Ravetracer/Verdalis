# Verdalis Plugin Suite

Verdalis is a suite of native **CLAP** audio plugins built around one idea:
**everything is synthesised, nothing is sampled.** (One documented exception
exists; see *Suite-wide goals*.) Each plugin models a natural
sound phenomenon from first principles — noise, oscillators, filters, physical
statistics — so no two instances ever produce the same output.

This directory is the root of the suite. All work on any Verdalis plugin starts
here, and every new plugin gets its own subfolder alongside the existing ones.

## Layout

```
Verdalis/
├── README.md          the GitHub front page, with the suite logo
├── CLAUDE.md          this file
├── LICENSE            MIT, suite-wide
├── .gitignore
├── release.sh         builds every plugin and packs the suite archive
├── setup-winbuild.sh  one-time Windows cross-build setup
├── _designs/          brand assets (logo, emblem; SVG + PNG) — committed,
│                      the README renders the logo from here
├── CLAP/              CLAP SDK checkouts       — GITIGNORED, see below
├── winbuild/          meson venv + Windows Cairo — GITIGNORED, see below
├── dist/              release archives         — GITIGNORED
├── rainyday/          RainyDay — synthesised rain
├── thunderclap/       ThunderClap — synthesised thunder
└── ...                one folder per plugin; see the roadmap below
```

**The suite is one git repository**, the whole of `Verdalis/`:

```
https://github.com/Ravetracer/Verdalis
```

The plugins were originally separate repositories (`Ravetracer/RainyDay` and
`Ravetracer/ThunderClap`). They were merged into this monorepo with their
histories rewritten under `rainyday/` and `thunderclap/`, so `git log` and
`git blame` reach back through all of it — RainyDay's 46 commits of DSP fitting
work included.

Commit at suite level. There are no per-plugin remotes any more.

Both former repositories have since been deleted, so **this repository is the
only copy of that history** — including the DSP fitting work, where the commit
messages are often the only record of what a coefficient was fitted against. It
is not a working copy of something published elsewhere. Keep at least one clone
or mirror off this machine.

## Suite roadmap

Eight plugins, each modelling one natural sound source:

| # | Plugin | Folder | Status | Simulates |
|---|--------|--------|--------|-----------|
| 1 | **RainyDay** | `rainyday/` | WIP | rain |
| 2 | **ThunderClap** | `thunderclap/` | WIP | thunder |
| 3 | **ShoreBreak** | `shorebreak/` | WIP | ocean waves |
| 4 | **SkyHowl** | `skyhowl/` | WIP | winds, storms |
| 5 | **ChirpParade** | `chirpparade/` | WIP | bird chirps |
| 6 | **RiverFlow** | `riverflow/` | WIP | rivers, streams |
| 7 | **CrackleBlaze** | `crackleblaze/` | WIP | fire |
| 8 | **InsectSwarm** | `insectswarm/` | WIP | flying and calling insects |
| 9 | **NightLife** | `nightlife/` | WIP | night insects, howling wolves, foxes, owls, frogs |

Plus one plugin that is deliberately **not** on that list:

| # | Plugin | Folder | Status | Simulates |
|---|--------|--------|--------|-----------|
| — | **WhooshPact** | `whooshpact/` | WIP | transitions, impacts — production sounds, not a natural source |

WhooshPact is the suite's first non-nature plugin and the first whose reference
library is *finished production sounds* rather than recordings of the world.
There is no physics under them to fit, so its measurement describes the target
rather than a mechanism -- see its `tools/analysis/README.md`, which says so at
the top. Everything else about it is the suite's: no samples, the shared window,
the shared parameter model, the same skeleton.

Naming follows a consistent pattern: a two-word CamelCase compound naming the
phenomenon, lowercase and joined for the folder, the CLAP id
(`de.ravetracer.<folder>`) and the preset extension (`.<folder>`).

**This roadmap is why `shared/` exists.** Every one of the remaining plugins
starts as a copy of an existing one, and without a shared foundation
each copy would duplicate the DSP toolbox and the GUI layer again. The
extraction was done before ShoreBreak, so the remaining five are built against
it from their first commit. See *Shared components* below for what is in it and
what is deliberately still per-plugin.

Likely shared additions as the suite grows — worth anticipating rather than
retrofitting:

- ShoreBreak, RiverFlow and RainyDay are all **water**. ShoreBreak's bubble
  layer and RainyDay's droplet resonators are already the same idea built twice;
  RiverFlow will make it three. A shared bubble resonator, with Minnaert's
  radius-to-pitch relation in it, is the obvious next extraction.
- SkyHowl and ThunderClap both need **large-scale air movement** and distance
  modelling; ThunderClap's `Lp2` air-absorption filter generalises directly.
- **The Fano factor now says three different things in three plugins, and it is
  the one statistic that decides whether a spawner is right.** CrackleBlaze
  measures 3.90 at one second for fire (clustered); NightLife measures 0.30 for
  a frog chorus (regular); RainyDay and ShoreBreak spawn Poisson and have never
  been measured. NightLife also found the trap in rendering the regular case:
  the superposition of many independent renewal processes tends to Poisson
  however regular each one is, so per-individual clocks cannot produce an
  under-dispersed chorus -- the regularity has to belong to the pond. The
  measurement belongs in `shared/tools/analysis` beside `wavio.py`.
- CrackleBlaze and RainyDay share **stochastic impulse spawning**, and
  CrackleBlaze has now shown that a plain Poisson process is not the whole story.
  Its references measure a Fano factor of 3.90 at one second against a Poisson
  process's 1.0, exactly Poisson at 50 ms, and twice Poisson below 10 ms -- so
  the spawner is three levels: a slowly wandering rate, a Poisson process at
  that instantaneous rate, and a short pulse train per arrival. RainyDay and
  ShoreBreak are both plain Poisson today, and the Fano factor is the one
  statistic that would say whether they should not be. Worth extracting with the
  measurement that motivates it.
- CrackleBlaze's **shaped noise burst** -- a click with a tone, a body and a
  decay, and deliberately *no* resonator, because the measured spectral flatness
  of a crackle is 0.67 -- is the cheapest event generator in the suite and
  nothing else has one. ShoreBreak's foam and RainyDay's impacts are both close
  to it.
- The **measured-bed filterbank with a solved gain vector** now exists in four
  copies (ShoreBreak, SkyHowl, RiverFlow, CrackleBlaze). CrackleBlaze also had
  to raise its bandpass Q from the octave-wide 1.4 to 2.2 to reach the contrast
  its measured shapes ask for, which is the same problem RiverFlow's 16 kHz
  octave has. A shelving-filter tilt carrying the curve's average slope, with
  the bank solving only the residual, would fix it for both -- and belongs in
  `shared/`.
- **The contour pipeline is now in two plugins and diverging.** NightLife's
  `contours.py` is ChirpParade's with a different analysis window, different
  gates and -- the one change that is a finding rather than a setting -- a term
  count *per second of call* rather than per call, because a fixed budget over
  calls running 75 ms to 3.1 s is a hidden duration filter. Every difference is
  measured and recorded in both plugins' analysis READMEs, which is exactly the
  argument for extracting the pipeline with its parameters exposed rather than
  copying it a third time.
- ChirpParade was the outlier, and it turned out that way: it needs pitched,
  formant-shaped voices rather than noise, and it reuses only the parameter
  model, the preset format, the window, `Svf`/`Lp2`/`OnePoleHp`, `Space` and
  `rng.h`. What it adds is worth taking back into `shared/` when a second
  plugin needs it -- see its `TODO.md`:
  - **a one-sided flow source.** Any plugin modelling a valve (a syrinx, a
    glottis, a reed) needs it, and the trap it avoids is easy to fall into
    twice: an odd-symmetric oscillator has no even harmonics at all, whatever
    it is driven with.
  - **the phrase scheduler with per-individual identities.** RainyDay,
    ShoreBreak and CrackleBlaze all spawn events at a rate; ChirpParade is the
    first to model *individuals* -- each with its own pitch, position, distance
    and voice, answering each other. NightLife is the second: its `Animal`,
    `Phrase` and answering behaviour are that code with the names changed, which
    settles the question of whether it is reusable.

## Plugin anatomy

Every plugin follows the same skeleton. Reproduce it exactly when starting a new
one — the consistency is what makes shared components possible. The build
tooling, the DSP toolbox, the parameter model, the preset format and the GUI
drawing primitives all come from `shared/`; see *Shared components* below.

```
<plugin>/
├── CMakeLists.txt           C++17, CLAP module + offline tools
├── install.sh               thin wrapper over shared/tools/install-plugin.sh
├── README.md                user-facing documentation
├── STATUS.md                what works, what is measured
├── TODO.md                  planned work
├── docs/manual.md           the manual's prose; the parameter reference and
│                            the preset library are generated into it
├── presets/                 factory presets, one file per preset, .<plugin>
├── src/
│   ├── <plugin>.h           identity constants + the preset bindings
│   ├── entry.{h,cpp}        the CLAP entry point, split so the .clap and the
│   │                        .vst3 can be built from one implementation
│   ├── plugin.cpp           CLAP host glue and the audio callback
│   ├── params.{h,cpp}       this plugin's ParamId enum and ParamDesc table
│   ├── preset.cpp           binds the shared preset format to this plugin
│   ├── preset_provider.cpp  binds the shared discovery provider
│   ├── factories.h
│   ├── dsp/                 the synthesis engine (the DSP toolbox is shared)
│   └── gui/                 its theme, panel layout and header ornament;
│                            the window itself comes from shared/
├── tools/
│   ├── render.cpp           offline renderer + self-test
│   ├── guihost.cpp          standalone GUI host for window development
│   └── analysis/            Python analysis helpers (loudness, spectra, fits)
└── !dev/                    LOCAL ONLY — reference recordings, papers, renders.
                             Gitignored. Large (hundreds of MB). Never commit,
                             never ship: this material is not ours to
                             redistribute.
```

## Build and install

From inside a plugin folder:

```sh
./install.sh                     # configure, build, self-test, install to ~/.clap
```

Or manually:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build            # defaults to ~/.clap/<PluginName>
```

A `build/` tree records the absolute source path it was configured against, so
moving or renaming a plugin folder makes its existing tree fail hard with
*"does not match the source used to generate cache"*. The fix is always to
delete the tree and let it reconfigure; nothing in it is worth keeping.

## CLAP SDK

`CLAP/` holds the CLAP SDK and the surrounding free-audio ecosystem. Each
subfolder is an independent upstream clone.

**It is gitignored and must stay that way.** It is third-party code, it is not
ours to redistribute, and everyone working on the suite checks it out
themselves.

Each plugin's `CMakeLists.txt` resolves the headers through
`${CMAKE_CURRENT_SOURCE_DIR}/../CLAP/clap/include`, so with `CLAP/` sitting
beside the plugins the suite is self-contained and builds need no extra
configuration. Only `clap/` is required to build; the rest are development and
validation tools.

To recreate the tree:

```sh
mkdir -p CLAP && cd CLAP
git clone https://github.com/free-audio/clap.git                  # required
git clone https://github.com/free-audio/clap-helpers.git
git clone https://github.com/free-audio/clap-host.git
git clone https://github.com/free-audio/clap-validator.git
git clone https://github.com/free-audio/clap-info.git
git clone https://github.com/free-audio/clap-wrapper.git
git clone https://github.com/free-audio/clap-plugins.git
git clone https://github.com/free-audio/clap-juce-extensions.git
git clone https://github.com/free-audio/clap-imgui-support.git
git clone https://github.com/free-audio/clap-saw-demo-imgui.git

# Only for the VST3 build; see "The VST3 builds" below. vstgui4 is deliberately
# not checked out: the wrapper does not use it, and it is the one part of the
# SDK that is not MIT.
git clone --branch v3.8.1_build_84 https://github.com/steinbergmedia/vst3sdk.git
cd vst3sdk && git submodule update --init base pluginterfaces public.sdk && cd ..
```

Reference versions currently in use: `clap` 1.2.10, `clap-wrapper` v0.16.0,
`clap-validator` 0.4.1, `clap-info` v1.2.2, `vst3sdk` 3.8.1.

`clap-wrapper` needs two patches, kept in `shared/patches/` because `CLAP/` is
gitignored and a fresh clone would lose them: one to build against VST3 3.8 at
all, and one fixing a use-after-free in its Linux run-loop timers that crashed
REAPER whenever a plugin window was closed and opened again. Apply both before
building a VST3.

If the checkout lives anywhere else, point CMake at it explicitly with
`-DCLAP_INCLUDE_DIR=/path/to/clap/include`.

Windows cross-build: `cmake/build-windows-cairo.sh` plus the mingw toolchain
file, output into `build-win/`.

## Releases

The suite ships as **one archive containing every plugin for both platforms**,
not as per-plugin downloads.

```sh
./setup-winbuild.sh        # once, sets up the Windows cross-build
./release.sh 0.1.0         # -> dist/verdalis-suite-0.1.0.{tar.gz,zip}
```

`release.sh` discovers plugins by looking for subdirectories with a
`CMakeLists.txt`, so **a new plugin joins a release simply by existing** — there
is no list to update. It reads each plugin's display name and version from its
`project()` line, builds Release for Linux and Windows, installs into a staging
tree, and writes `BUILD-INFO.txt` recording what went in.

It produces **one archive: the suite, both platforms in it**, so the visitor
never has to pick an operating system first:

```
verdalis-suite-0.2.0.zip        every plugin, Linux + Windows
```

There used to be a per-plugin archive beside it. The plugins are free and the
site offers the suite on every page, so a download per plugin was a second link
to the same thing; the per-plugin pack loop is gone.

**Everything is a `.zip`, the Linux builds included.** That is not the Unix
habit, but the download manager the site runs handles zip alone, and a release
nobody can publish is worse than one in the wrong format. Nothing is lost by it:
zip records Unix permissions, and a `.clap` is `dlopen`'d, which needs no execute
bit — verified by extracting one and loading it. `--tarball` emits `.tar.gz`
alongside for anywhere that prefers it.

The archive carries the suite version, which is independent of the individual
plugin versions in each `CMakeLists.txt`.

It is self-contained: a `linux/` and a `windows/` folder each holding **both
formats** for every plugin — the plugin's CLAP folder with its presets, and its
`.vst3` bundle beside it — plus the README, every manual in `manuals/`, the
LICENSE, a BUILD-INFO.txt, and an INSTALL.txt with a section per platform and
per format, written from the `targets` and formats the build actually produced,
so a `--linux-only` release does not describe a `windows/` folder that is not in
it, and a `--no-vst3` one does not describe a VST3.

```
verdalis-suite-0.2.0/
├── linux/
│   ├── RainyDay/
│   │   ├── RainyDay.clap
│   │   └── presets/        (18 files)
│   ├── RainyDay.vst3/      bundle: Contents/x86_64-linux + Contents/Resources
│   └── ...                 one pair per plugin
├── windows/
│   └── ...                 the same, Contents/x86_64-win
├── manuals/                one PDF per plugin
├── README.md
├── LICENSE
├── INSTALL.txt             both platforms, both formats
└── BUILD-INFO.txt
```

Options: `--tarball` adds `.tar.gz` beside every `.zip`; `--linux-only` skips
the Windows half; `--windows-no-gui` allows a
Windows build with no plugin window; `--no-manuals` skips the PDF manuals;
`--no-vst3` ships CLAP only. A missing `CLAP/clap-wrapper` or `CLAP/vst3sdk`
downgrades to CLAP-only with a warning rather than failing the release.
Offline tools are switched off for release builds
(`-D<PLUGIN>_BUILD_TOOLS=OFF`).

The suite archive carries its own version, independent of the individual plugin
versions, which stay in each `CMakeLists.txt`.

### The Windows plugin window

There is **one** drawing implementation for both platforms: Cairo. On Linux it
draws against X11; on Windows against Cairo's win32 backend, which paints
through GDI onto the `HDC` from `BeginPaint` (`cairo_win32_surface_create`,
linking `gdi32 msimg32`).

**There is no GDI+ renderer and never was** — the plugins contain no `gdiplus`
code at all. GDI appears only as the surface Cairo paints onto. If a Windows
window ever needs fixing, it is Cairo code, not a Windows drawing API.

Cairo has no MinGW package, so it is cross-built from upstream source into
`winbuild/cairo-mingw` by `setup-winbuild.sh` (meson goes into a local venv,
since system Python on current Debian/Ubuntu refuses a plain `pip install`).

Without that Cairo, `CMakeLists.txt` sets `<PLUGIN>_BUILD_GUI OFF` and the
Windows plugin builds **with no window at all**. `release.sh` therefore refuses
to build Windows without it rather than shipping a window-less plugin by
accident. The Windows binaries are ~5 MB because Cairo is linked statically.

## The VST3 builds

The suite is CLAP first and stays that way. A VST3 is produced from the same
plugin by the **clap-wrapper** (`CLAP/clap-wrapper`), which implements a CLAP
host against the VST3 API: there is no VST3 port, no second engine and no second
parameter table. What the plugin gains is a second entry point.

This is possible at all because **VST 3.8 is MIT licensed** (3.8.0, October
2025; the suite builds against 3.8.1). Every earlier SDK was GPLv3 or a
proprietary Steinberg agreement, neither of which suits an MIT suite. The
trademark is still Steinberg's, so using the VST name or logo follows their
trademark rules — that is a packaging question, not a code one.

**Every plugin builds a VST3, for Linux and Windows, and every release archive
ships one.** It is off by default in a plugin's own CMake (`<PLUGIN>_BUILD_VST3`)
because it needs two extra checkouts; `release.sh` turns it on.

```sh
cd rainyday && ./install.sh --vst3      # -> ~/.clap/RainyDay + ~/.vst3/RainyDay.vst3
./release.sh 0.12.0                     # both formats, both platforms
./release.sh 0.12.0 --no-vst3           # CLAP only
```

### How a plugin is wired for it

- **The plugin is a static library.** `<Name>-impl` holds everything; the
  `.clap` and the `.vst3` are each a module that adds one small entry file and
  links it. `src/entry.h` declares `<plugin>EntryInit`/`Deinit`/`GetFactory`,
  `src/entry.cpp` builds `clap_entry` from them for the CLAP, and the wrapper's
  own shim does the same for the VST3. `plugin.cpp` no longer defines
  `clap_entry`. All seven are identical here -- copy any of them.
- Anything the module must inherit from the impl library is `PUBLIC`
  (`target_link_libraries`, `target_include_directories`,
  `target_link_directories`); compile options and definitions stay `PRIVATE`.
- **The window needed nothing.** VST3 embeds through `IPlugFrame`/`IRunLoop`,
  and `plugin.cpp` already drove the GUI off `clap_host_timer_support` with an
  embedded (not floating) X11 window, which is exactly what that wants.
- **Two things the wrapper leaves to the plugin on a mingw cross build**, both
  in the `<PLUGIN>_BUILD_VST3` block: CMake gives a MODULE library a `lib`
  prefix with the GNU toolchain and the wrapper only clears it on its
  single-file path, so `PREFIX ""` is set explicitly; and the mingw runtime has
  to be linked in exactly as the `.clap` does it. There is no
  `--exclude-all-symbols` -- `GetPluginFactory` carries dllexport from
  `SMTG_EXPORT_SYMBOL` and must stay visible.
- **On Linux the `.vst3` gets a version script of its own**,
  `shared/cmake/vst3_entry.version`, exporting `GetPluginFactory`, `ModuleEntry`
  and `ModuleExit` and nothing else. Without it each `.vst3` exported ~1700
  symbols, identical in every plugin because they all link the same wrapper and
  SDK sources -- among them `Steinberg::gPluginFactory`, which the SDK treats as
  one-per-module. A host that loads plugins with `RTLD_GLOBAL` then binds the
  second plugin's calls to the first plugin's copies, and every plugin after the
  first hands out the first one's factory and reports itself as that plugin.
  Reproduced with a two-`dlopen` probe: RainyDay then ThunderClap, and
  ThunderClap's factory came back naming "RainyDay". REAPER uses `RTLD_LOCAL`
  and is not affected, which is exactly why this stayed invisible.

### Presets in a VST3, and what actually carries them

**The factory library is embedded in the binary**, by `embed_presets.cmake`, and
the window's browser reads it from there. So a VST3 shows all of its presets
with no files on disk at all -- verified by finding the preset text in the
shipped `.so` and `.vst3` binaries, identical to the `.clap`.

Two things follow, and they are easy to get backwards:

- `factoryPresetDir()` in `shared/src/preset.cpp` is **not** what makes presets
  work in the window. Its only caller is the CLAP preset-discovery provider.
  It learned `../Resources/presets` so that it is correct for a bundle layout,
  after `<dir>/presets`, leaving every other layout as it was.
- **The CLAP preset-discovery factory does not cross over.** A VST3 host will
  not list the factory presets in *its own* browser. The plugin's own browser
  lists all of them. Upstream has work on this on its `next` branch
  (`preset discovery: let wrapped formats browse a CLAP's presets`), so revisit
  when the wrapper next releases.

The `Contents/Resources/presets` folder staged into each bundle is therefore a
readable on-disk copy for the user -- and what that upstream bridge would read
-- not something the plugin needs to run.

### Archive layout

Both formats sit side by side in each platform folder, for every plugin:

```
verdalis-suite-<version>/
├── linux/
│   ├── RainyDay/           RainyDay.clap + presets/
│   ├── RainyDay.vst3/      Contents/x86_64-linux/ + Contents/Resources/presets/
│   └── ...                 one pair per plugin
├── windows/
│   └── ...                 Contents/x86_64-win/ + Contents/Resources/presets/
├── manuals/, README.md, LICENSE, INSTALL.txt, BUILD-INFO.txt
```

`INSTALL.txt` names `~/.clap` and `~/.vst3` on Linux, and the two
`Common Files` folders on Windows. `BUILD-INFO.txt` records the clap-wrapper and
VST3 SDK versions the binaries came from, because the wrapper needs a local
patch to build at all and "which checkout" is not rhetorical.

**A staged plugin missing its VST3 is a silent failure** -- the archive still
builds, it is just missing a format, and the file sizes are the only tell. It
shipped that way once. `release.sh` hard-fails before packing if any staged
plugin has no `.vst3` beside its `<Name>/` folder, so it cannot happen quietly
again. That check outlived the per-plugin archives it was written for.

### Verifying one

Steinberg's own validator is the bar on Linux, and it builds out of the SDK
checkout:

```sh
cmake -S CLAP/vst3sdk -B /tmp/vst3sdk-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
   -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_CREATE_PLUGIN_LINK=OFF \
   -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF
cmake --build /tmp/vst3sdk-build --target validator
/tmp/vst3sdk-build/bin/Release/validator ~/.vst3/RainyDay.vst3        # 47 tests
/tmp/vst3sdk-build/bin/Release/validator -e ~/.vst3/RainyDay.vst3     # 537 tests
```

Hosting examples must be off: `editorhost` wants gtkmm, which is not installed
and is not needed. All seven plugins pass 47/47.

`-e` reports a few `getParamValueByString` notes -- three on RainyDay (Density,
Trickle Rate, Trickle Decay). They are not failures. The displayed text is
rounded ("107 drops/s"), so parsing it back lands a fraction off the value it
came from; that is display precision, and the CLAP self-test's own round-trip
check covers the same ground and passes. The ~2000 further notes are on
parameter ids at `0xB00000` and above, which are the wrapper's synthesised MIDI
CC parameters, not the plugin's.

**The validator does not cross-build**, so the Windows side is checked
differently. Its CMake decides the platform before the toolchain file is read,
so it compiles `module_linux.cpp` into a Windows target; forcing `SMTG_WIN` past
that then hits a `WinMain` link failure. Not worth fighting for a test tool. The
Windows binaries are instead loaded under wine by a small probe that does
`LoadLibrary` -> `InitDll` -> `GetPluginFactory` -> `getClassInfo` ->
`createInstance` -> release, which is the part actually in doubt on a mingw
cross build. All seven load and instantiate. Also worth checking per build:

```sh
x86_64-w64-mingw32-objdump -p <the .vst3> | grep "DLL Name"
```

Only system DLLs may appear. `libgcc`, `libstdc++` or `libwinpthread` in that
list means the runtime is not linked in and the plugin will not load on a
machine without a compiler.

### Still open

- The VST3 class id (TUID) is hashed from the CLAP id by the wrapper. That is
  stable and deterministic, so the shipped ids are consistent -- but they are
  now *released*, so they can never change. Pin them explicitly with
  `CLAP_VST3_TUID_STRING` before anything reorganises the plugin ids.
- The 3.8 build break has not been reported upstream.

## Shared components

`shared/` holds the code the whole suite is built on, compiled as the static
library `verdalis::shared` and reached through `<verdalis/...>` includes:

```
shared/
├── CMakeLists.txt              builds verdalis::shared
├── include/verdalis/
│   ├── params.h                ParamDesc, ParamKind, FilterKind, conversions
│   ├── param_macros.h          table-building shorthand (params.cpp only)
│   ├── preset.h                PresetContext, PresetData, the text format,
│   │                            user folders and the preset pack format
│   ├── preset_library.h        the library as the browser sees it: the scan,
│   │                            the folder split, export and import
│   ├── preset_provider.h       PresetProviderSpec, the discovery factory
│   ├── dsp/{adsr,biquad,denormals,fastmath,filters,reverb,rng}.h
│   └── gui/
│       ├── filedialog.h        the desktop's file chooser, and reveal-in-files
│       ├── gui.h               Gui + GuiDelegate, the plugin/window contract
│       ├── toolkit.h           Cairo drawing primitives, Rect, Align
│       └── window.h            Theme, PanelSpec, HeaderOrnament, MixerStrip,
│                                WindowSpec
├── src/{params,preset,preset_library,preset_provider}.cpp
├── src/gui/window.cpp          the window: layout, widgets, browser, entry
├── src/gui/filedialog.cpp      zenity/kdialog on Linux, commdlg on Windows
├── cmake/                      embed_presets, mingw toolchain, Windows Cairo,
│                               clap_entry.version
├── patches/                    fixes for the gitignored CLAP/ checkouts
└── tools/                      install-plugin.sh, fithost.cpp, analysis/wavio.py,
                                make-demos.sh (the website demo audio), and the
                                manual toolchain: make-manual.sh, docgen.cpp,
                                manual.py, manual.css
```

A plugin pulls it in with

```cmake
add_subdirectory("${VERDALIS_SHARED_DIR}" verdalis-shared)
target_link_libraries(<target> PRIVATE verdalis::shared)
```

and each plugin's `params.h` re-exports the shared names into its own namespace
with a scoped `using namespace verdalis;`, so plugin code calls `paramToReal`,
`Svf`, `parsePreset` and so on unqualified, exactly as before.

**How the shared code stays plugin-agnostic.** It never hardcodes a name. The
preset code takes a `PresetContext` (plugin name, file extension, parameter
table and its size); the discovery provider takes a `PresetProviderSpec`.
`embed_presets.cmake` requires `NAMESPACE` and `PRESET_EXT` as arguments and
fails without them, because a default would silently generate the wrong
namespace and still compile in the plugin it was copied from.

Because `verdalis::shared` is a **static** library linked into each plugin
separately, file-scope state in shared code belongs to one plugin binary. That
is what makes the single preset-discovery provider safe.

### The window, and its theme

Every Verdalis plugin has **the same window**: one layout engine, one set of
widgets, one set of interactions, in `shared/src/gui/window.cpp`. Knobs, chips,
dropdowns, the preset browser, the save field, the typed value entry and the
meters are written once. A plugin describes itself with a `WindowSpec` and calls
`createWindow()`.

**Every plugin has its own colour theme, and that is the only thing that tells
the windows apart.** The `Theme` in the spec carries the whole palette, not just
the accent: the backgrounds, the panel fill and edge, the knob face, the track
and the three text greys are all part of it. A plugin tints its greys towards
its own accent so the window reads as one instrument rather than a grey chassis
with a coloured knob.

| Plugin | Accent | Character |
|--------|--------|-----------|
| RainyDay | `#58B6E8` | rain blue, cool neutral greys |
| ThunderClap | `#B396FA` | lightning violet, greys an octave darker |
| ShoreBreak | `#4FD0BA` | sea green, a warmer and greener chassis |
| SkyHowl | `#F0845C` | dust coral -- greys with a red-brown cast |
| ChirpParade | `#F2C744` | finch gold, greys with an olive cast |
| InsectSwarm | `#E6D93B` | wasp yellow at hue 56, sharper and more acid than ChirpParade's warm gold at 43; a chitin-green-black chassis so the greys lean away from ChirpParade's olive |
| RiverFlow | `#57C77A` | river green -- moss on wet stone, cool green-grey greys |
| CrackleBlaze | `#FF5A2C` | ember orange, the darkest chassis in the suite -- a deep red-brown cast, deliberately hotter and redder than SkyHowl's dusty coral |
| WhooshPact | `#FF3C97` | impact magenta at hue 330, with a cool blue-violet chassis -- the one plugin that models nothing natural, and the one accent that reads as obviously synthetic |
| NightLife | `#9FB4E8` | moonlight: the only *desaturated* accent in the suite, a pale indigo at hue 221 over the darkest blue-black chassis. It sits between RainyDay's saturated cyan and ThunderClap's saturated violet and is mistakable for neither, because it is washed out where both of those are vivid |

A new plugin picks its own accent and derives its greys from it. Do not reuse
another plugin's theme, and do not fall back to the suite brand palette — that
one is for the README and packaging, not the window.

`Theme::highlight` is a second, brighter colour for an ornament that needs one
(the white-hot core of a lightning channel). Set it to the text colour when
there is nothing like that.

**The header ornament** is the other per-plugin signature: a `HeaderOrnament`
subclass drawn behind the wordmark, clipped to the header. RainyDay runs rain
streaks whose density follows `voiceLoad`; ThunderClap grows a bolt from
`eventCounter`, deterministic in the flash number so the same flash always draws
the same bolt. ChirpParade scrolls a sonogram, one stroke per syllable, deterministic in the
syllable number, so the same syllable always draws the same stroke -- and it
keeps a few dim strokes on screen when nothing is playing, because a sonogram
with nothing on it reads as a broken graph rather than a quiet one.
`animating()` decides whether the window repaints, and must not
change state. A plugin with nothing to animate leaves `ornament` null.

**The ornament belongs to one window, and the window deletes it.** `createGui()`
allocates a fresh one per call rather than pointing the spec at a file-scope
instance, which is what all ten plugins used to do: an ornament carries
animation state -- a phase counter, a sonogram's scroll history -- and one
instance behind two windows of the same plugin made them drive each other's
picture. The window owning it is what keeps two instances independent.

**The layer mixer.** Every plugin layers several generators, and each layer's
level sits on whichever panel that layer belongs to -- right for editing one
layer, wrong for balancing them against each other, which is most of what
building a preset from scratch is. So the shared window has a mixer: a `MIXER`
button in the preset bar opens an overlay with one strip per layer -- the
layer's level as a fader, its pan and width as slim sliders under it, and a mute
and a solo button. A plugin describes its layers with a `MixerStrip` table
(`label`, `level`, `pan`, `width`, `master`) and gets the mixer for free;
`kNoParam` stands for a placement the layer does not have, and a plugin with
nothing to mix leaves `mixer` null and gets no button.

The parameters are the plugin's own and also live on the panels -- the mixer is
a second view of them, which is why it does not disturb the
`placedParams() == kNumParams` assertion.

**Mute and solo cannot be parameters**, and that is the one thing to understand
before touching this. A preset is a set of parameter values, so a mute that
reached the parameters would be saved as a layer that comes back silent. They
are window state: the mixer holds a muted layer's level at the bottom of its
range and remembers what it was. Everything else follows from that --
`clearHolds()` restores, `SAVE` and every preset load call it first, editing a
held fader releases rather than fights it, and the preset bar carries a
`SOLO ON` / `MUTE ON` chip for as long as a hold is active, because a
forced-down layer that looks like a saved one is the whole trap.

**The preset library has folders, and a folder is a file.** Saving a preset as
`Folder/Name` puts it one level under the user preset directory; a name with no
slash saves into the root, as every save did before. One level only, and typing
the folder is the whole of the "make a folder" gesture -- a folder with nothing
in it cannot be made, which is right for something whose only purpose is to hold
presets. The browser then draws a column of shelves: *All*, the plugin's own
factory set, each user folder, and *Unfiled* for the root.

A whole shelf can be written out as a **preset pack**: one text file, extension
`<presetext>pack`, kept in a `packs` directory beside `presets`. It is the preset
format again with a separator line between the presets, and it carries each
preset's *text* rather than a re-serialised copy -- which is what makes a round
trip lossless even for a plugin whose presets hold lines the shared format knows
nothing about. Importing never overwrites: a pack whose folder already exists is
imported beside it under a numbered name.

All of it is shared. `shared/src/preset_library.cpp` has the scan, the folder
split and the pack read and write; the browser's shelves, footer and import list
are in `window.cpp`; `shared/src/gui/filedialog.cpp` is the only place that
talks to the desktop, and it reports honestly when there is no chooser rather
than offering a button that does nothing. A plugin opts in by implementing five
`GuiDelegate` methods that all default to "this plugin does not do that", so the
flat browser is still what a plugin gets for free -- see any plugin's
`guiPresetFoldersSupported()`.

The feature was taken from **SäureKiste** in `Ravetracer/audio-plugins`, whose
`shared/` is a fork of this one. Its browser lives in its own
`src/gui/seqwindow.cpp` rather than the shared window, so the backend ported
across unchanged and the browser UI had to be transplanted. If either copy
changes here, the other is worth a look -- there is no submodule between them.

**Not every layer can have a strip.** A strip is its fader, so a layer with no
level parameter of its own gets none: RainyDay's close droplets are loudness
compensated, ThunderClap's crack is set by distance and focus, ShoreBreak's
break by wave size. `stripValid()` skips a strip whose `level` is not a real
parameter index rather than reading past the end of the table. The consequence
is worth knowing: in those three plugins solo cannot silence the main layer,
because nothing in the parameter table can.

**The delegate** (`verdalis/gui/gui.h`) is deliberately generic:
`guiVoiceCount()` and `guiVoiceLimit()` are whatever the activity meter counts
-- droplets, shock waves, grains -- and `guiEventCounter()` is an optional
monotonic count of discrete events, defaulting to zero. The meter's wording
comes from the spec's `voiceNoun` and `eventNoun`.

### What each plugin still owns

Genuinely per-plugin, and correctly so:

- `src/dsp/<name>_engine.{h,cpp}` — the synthesis itself
- `src/entry.{h,cpp}` — the entry-point split; identical in all seven bar the
  plugin's own prefix
- `src/params.cpp` / `params.h` — its ParamId enum and its ParamDesc table
- `src/preset.cpp`, `src/preset_provider.cpp` — ~35-line bindings that supply
  the plugin's name, extension and table to the shared implementations
- `src/gui/gui.cpp` — its theme, its panel layout, its mixer strips and its
  header ornament
- `presets/`, `README.md`, `STATUS.md`, `TODO.md`, `!dev/`

### Still duplicated

One file remains substantially shared but is **not** extracted:

| File | Size | Differing lines | What blocks extraction |
|------|------|-----------------|------------------------|
| `src/plugin.cpp` | ~1140 | ~128 | The CLAP lifecycle is common; the engine type is not. Wants a template parameter or an engine interface. With nine copies of it now, the question of which parts are really common is answered: everything except the engine type and `syncEngineParams`. |

### Rule for a shared change

Anything in `shared/` is used by every plugin. A change there must be verified
against all of them — see the verification recipe under *Working notes*.

## Conventions for a new plugin

1. New folder directly under `Verdalis/`, lowercase, single word if possible
   (`rainyday`, `thunderclap`).
2. Copy the skeleton above from the closest existing plugin.
3. Naming: folder and preset extension lowercase (`.rainyday`); CMake project,
   installed artifact and display name CamelCase (`RainyDay`, `RainyDay.clap`).
4. No new repository and no new remote — it is a folder in this one. It joins
   the next release automatically; add a row to the README's plugin table.
5. Identity constants live in `src/<plugin>.h` and follow the suite pattern:
   - `kPluginId` = `de.ravetracer.<plugin>` (lowercase)
   - `kPluginVendor` = `Ravetracer`
   - `kPluginUrl` = `https://github.com/Ravetracer/Verdalis`
6. Presets are fitted against real reference recordings where possible, and the
   references live in `!dev/`, uncommitted.
7. Copy `src/entry.{h,cpp}` and the `<PLUGIN>_BUILD_VST3` block from an
   existing plugin, and keep `plugin.cpp` free of a `clap_entry` definition.
   Both formats come out of that split; nothing else in the plugin knows about
   VST3. See *The VST3 builds*.
8. Keep the visual language: the window comes from `shared/`, so the layout
   engine and geometry are automatic. What the plugin writes is its own `Theme`
   (a new accent, with the greys tinted towards it), its panel table, and a
   `HeaderOrnament` if it has something to animate. See *The window, and its
   theme*.

## The manuals

Every plugin ships a PDF manual in its release archives, built by
`shared/tools/make-manual.sh <plugin>` into `dist/manuals/` as
`<Plugin>-<version>-Manual.pdf`, with the HTML it was rendered from beside it —
that HTML is self-contained (inlined stylesheet, logo as a data URI) and is
what a website would publish.

**Two of the chapters are generated, not written.** `shared/tools/docgen.cpp`
reads the plugin's own `paramTable()` and its preset files and emits the
parameter reference and the preset library as Markdown, which the script
substitutes into `<plugin>/docs/manual.md` at `{{PARAMETER_REFERENCE}}` and
`{{PRESET_LIBRARY}}` (`{{PLUGIN}}` and `{{VERSION}}` are substituted too, and an
unsubstituted `{{...}}` is an error rather than silently shipped). So the
parameter tables cannot drift from the build: every range, default, unit, enum
choice and one-line explanation in the manual is the same `ParamDesc` field the
window and the host read. A parameter added to `params.cpp` appears in the next
manual with no documentation change at all.

`docgen` is compiled directly with `g++` by the script rather than through the
plugin's CMake project, because release builds switch the offline tools off and
the manual still has to build. It needs only the plugin's `params.cpp` and the
shared parameter code — no CLAP headers, no X11, no Cairo.

The toolchain is `python3` with the `markdown` module, plus `wkhtmltopdf` for
the PDF. `release.sh` treats it as optional: if it is missing the release says
so and goes out without manuals rather than failing.

**What each manual owns** is `docs/manual.md`: a key/value header (`tagline`,
`subtitle`, `accent` — the plugin's own accent, which colours the headings and
the rules), then the prose chapters. Keep the chapter structure the same across
plugins; the three existing manuals are the template.

Two constraints come from `wkhtmltopdf`, and both are in `manual.css`:

- Its WebKit is old. No grid, no custom properties, no `@page` margin boxes —
  page geometry is set on the command line.
- The build on this machine is the unpatched-Qt one, which cannot write page
  footers or a PDF outline. The manuals therefore have **no page numbers**, and
  the contents page is a list of links instead. A row of a long table can also
  be split across a page break and leave a sliver behind; `page-break-inside`
  is only partly honoured.

## The website material

**Every new plugin needs two things for the site, and neither is optional:
website copy and demo tracks.** They are as much part of finishing a plugin as
its manual is, and they are easy to forget because neither lives in the
repository — both are gitignored, local to this machine.

**Website copy** goes in `_designs/website-copy.md`, which holds the suite's
download-page text, the AI disclosure, and then one `## <Plugin>` section per
instrument carrying **three variants** of 2–3 paragraphs each in a plain product
register: what the plugin aims to be, then what it features. A new plugin gets
its own section in the same shape as the existing ones. Where the author has
given his own description of a plugin, it is kept **verbatim** under *The
author's own brief* ahead of the variants and the marketing prose is written
around it rather than replacing it. Every factual claim in a variant comes from
the plugin's README, STATUS and parameter table, so re-check the numbers when a
version changes them — including the suite section's instrument, preset and
parameter totals.

**Demo tracks** go in `dist/demos/<Plugin>/`, one MP3 per factory preset, built
by

```sh
shared/tools/make-demos.sh <plugin-folder>
```

which renders 16 seconds plus a 3 second tail at 48 kHz from the plugin's own
offline renderer, applies a single linear gain towards -16 LUFS backed off to
keep the true peak under -1 dBTP, and encodes at 256 kbps. There is no
compression and no limiting, so the relative loudness the presets were fitted to
survives and a sparse preset stays quiet. Nothing about the script is
per-plugin; it then updates `dist/demos/demos.json` and `dist/demos/README.md`
in place, touching only that plugin's section. `--text-only` refreshes that
metadata without re-rendering, which is what a wording change needs.

**A demo's blurb and a preset's description are two different texts, and a new
plugin needs both.** The preset's `description` is documentation: it says what
was measured and why, and it is what the manual's preset library prints. The
website wants a sentence a musician can read. So each plugin carries
`presets/demo-descriptions.txt` — one `Preset Name = text` line per preset — and
the script prefers it, falling back to the preset's own description for anything
with no line. It warns about every preset it had to fall back on, so a missing
one is visible rather than silently technical. The plugin's own documentation
never has to be dumbed down to make the site readable, which is the whole point
of keeping them apart.

`DemoTracks/` is the other, smaller thing: curated demos that are not one plain
render per preset. WhooshPact has its own `tools/make-demos.sh` there because its
presets are one-shots and a 19 second render of one is mostly silence. Reach for
that pattern only when a plugin's presets are not sustained.

## Branding

Assets live in `_designs/` and are committed — the root `README.md` renders
`verdalis-logo-horizontal.svg` from there, with the `-dark` variant swapped in
via `<picture>` and `prefers-color-scheme` so it reads correctly in both GitHub
themes.

| File | Use |
|------|-----|
| `verdalis-logo-horizontal.svg` / `-dark.svg` | wordmark + emblem; README, sites |
| `verdalis-emblem.svg` | emblem alone; icons, small spaces |
| `*-4000.png`, `*-2048.png` | raster fallbacks where SVG is not accepted |

Verdalis palette, taken from the logo:

| Colour | Hex | Role |
|--------|-----|------|
| Deep green | `#0E3328` | ground, darkest |
| Forest | `#1B5A48` | gradient mid |
| Teal | `#2E8B7A` | secondary |
| Bright teal | `#5FC8BF` | primary accent |
| Leaf | `#9BDC8E` | dark-variant highlight |
| Pale mint | `#DCEFE3` | light text on dark |
| Amber | `#F2AE3F` | single warm accent |

Note the plugin windows do **not** use this palette. Each plugin has its own
theme, near-black and tinted towards its own accent — see *The window, and its
theme* for the rule and the current accents. The suite palette above is for
brand surfaces only: README, site, packaging.

## Suite-wide goals

- **Pure synthesis, and what that does and does not forbid.** Every sound a
  plugin's engine produces is computed at run time. **A sample is a recorded
  piece of sound.** Tables of numbers are not samples, and the suite is free to
  ship them: coefficients, calculation tables, measured contours, fitted curves,
  lookup tables. The test is whether audio is being reproduced, not whether a
  number came from a recording — a pitch contour extracted from a real bird and
  stored as forty cosine coefficients is a formula, in the same sense as
  `f(x) = -260 sin(-pi x / 5200) + 1740` read off a spectrogram by hand. The
  only limit is size: a plugin binary must not balloon.

  This is a deliberate widening of the rule, made after ChirpParade's first
  attempt failed. Fitting a physical model to *aggregate statistics* — medians
  of duration, sweep, harmonic count — produced something that measured
  correctly on twenty quantities and sounded nothing like a bird, because the
  statistics had been taken through an analysis window too long to see what a
  syllable actually does. Measured contours are what fixed it.

  **It applies to every plugin, not just ChirpParade**, and the others are worth
  revisiting on the strength of it: RainyDay's droplet resonances, ShoreBreak's
  breaker envelopes and ThunderClap's N-wave shapes are all currently
  parametric where a measured curve would be truer.

  The one genuine exception in the suite remains the small block of CC-BY
  licensed *audio* embedded in SkyHowl for one hidden, non-instrument function,
  added deliberately after the synthesised version was rejected by ear; the
  attribution its licence requires is in `skyhowl/LICENSE`, and
  `skyhowl/tools/make-vent-samples.py` records what it is and how it was
  prepared. That is a recording, so it is an exception. A coefficient table is
  not.

- **Validate by ear, then by measurement.** ChirpParade 0.1.0 was 11,000 lines
  validated entirely against statistics the plugin's own analysis code produced,
  and it was wrong. Measurement catches what the ear cannot quantify; it does
  not replace the ear, and a number agreeing with a number proves nothing about
  the sound. Render A/B pairs against the references and listen before declaring
  a fit good.

- **Shared visual identity.** One window design across the suite, distinguished
  only by accent colour; brand assets in `_designs/`.
- **No toolkit dependency.** GUI is X11/Win32 plus Cairo, drawn by hand.
- **Measured, not guessed.** Presets and DSP are fitted against references and
  verified by the offline render self-test.
- **Bounded CPU cost** and sample-accurate note/parameter handling.
- Linux first, Windows via mingw cross-build.

## Working notes

- `!dev/` is large and gitignored. Never `git add -A` without checking; the
  `.gitignore` comment records that a 5 MB DLL once got committed that way.
- The self-test runs as part of `install.sh` — do not skip it when changing DSP.
- `STATUS.md` and `TODO.md` in each plugin are the current source of truth for
  that plugin's state; read them before starting work there.
- Both plugins' `kPluginUrl` now point at the suite repository. They previously
  pointed at the per-plugin repos, which no longer exist — a new plugin must use
  the suite URL, not invent its own.

### Verifying a change to shared/

The synthesis is deliberately stochastic — the same build renders differently
every run — so comparing output only works with the seed pinned. `Random Seed`
is non-zero-means-reproducible, and `render --param` matches on the *display*
name, so the override is `randomseed=N`, not `seed=N`.

The recipe that proves a refactor changed nothing:

```sh
# 1. a reference build from before the change
git worktree add /tmp/ref <commit>
ln -s "$PWD/CLAP" /tmp/ref/CLAP
cmake -S /tmp/ref/rainyday -B /tmp/ref-build -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/ref-build

# 2. render every preset from both, with the seed pinned
for side in ref new; do
   ./rainyday-render --plugin ./RainyDay.clap --all --outdir /tmp/wav-$side \
      --seconds 3 --tail 2 --rate 48000 --param randomseed=7
done

# 3. compare
for f in /tmp/wav-ref/*.wav; do cmp "$f" "/tmp/wav-new/$(basename "$f")"; done
```

Vary `--rate` and the seed; identical output across several of each is strong
evidence. Also run `render --selftest` (it round-trips a saved preset) and
`render --list` (it exercises preset discovery, and its output should be
byte-identical).

**A GUI change must actually open a window before it is called done.** Nothing
else exercises one: `render --selftest` never creates a GUI, and Steinberg's
validator only *scans* the editor classes without creating or attaching a view,
so both pass on a window that crashes the moment a host opens it. `guihost`
exiting 0 is the bar, and it is a real check -- a dangling `WindowSpec` shipped
once because the self-test and 47/47 from the validator were taken as enough.

For a GUI change, `<plugin>-guihost <plugin>.clap "" 8` opens the real window
for eight seconds; capture it with `import -window $(xdotool search --name
RainyDay | head -1)` and compare with `compare -metric AE`. Zero differing
pixels is the bar. Match the window by its expected width when picking it out
of `xdotool search` -- the search also matches other windows with the plugin's
name in the title, and grabbing the wrong one silently "passes".

Interactions can be driven the same way, which is how the shared typed value
entry was checked in both plugins:

```sh
eval "$(xdotool getwindowgeometry --shell "$wid")"
xdotool mousemove $((X+150)) $((Y+180)) click 1   # a knob's value field
xdotool type --delay 60 "55"
xdotool key Return
```

Click a *knob*, not an enum chip: a chip opens its dropdown instead, which is
correct behaviour and looks like a failed test.
