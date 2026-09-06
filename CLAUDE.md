# Verdalis Plugin Suite

Verdalis is a suite of native **CLAP** audio plugins built around one idea:
**everything is synthesised, nothing is sampled.** Each plugin models a natural
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
└── thunderclap/       ThunderClap — synthesised thunder
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

Seven plugins, each modelling one natural sound source:

| # | Plugin | Folder | Status | Simulates |
|---|--------|--------|--------|-----------|
| 1 | **RainyDay** | `rainyday/` | WIP | rain |
| 2 | **ThunderClap** | `thunderclap/` | WIP | thunder |
| 3 | **ShoreBreak** | `shorebreak/` | WIP | ocean waves |
| 4 | **SkyHowl** | `skyhowl/` | planned | winds, storms |
| 5 | **ChirpParade** | `chirpparade/` | planned | bird chirps |
| 6 | **RiverFlow** | `riverflow/` | planned | rivers, streams |
| 7 | **CrackleBlaze** | `crackleblaze/` | planned | fire |

Naming follows a consistent pattern: a two-word CamelCase compound naming the
phenomenon, lowercase and joined for the folder, the CLAP id
(`de.ravetracer.<folder>`) and the preset extension (`.<folder>`).

**This roadmap is why `shared/` exists.** Every one of the five remaining
plugins starts as a copy of an existing one, and without a shared foundation
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
- CrackleBlaze and RainyDay share **stochastic impulse spawning** — the Poisson
  process in `rng.h` already covers both.
- ChirpParade is the outlier: it needs pitched, formant-shaped voices rather
  than noise, so expect it to add to the shared DSP rather than reuse much.

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
├── presets/                 factory presets, one file per preset, .<plugin>
├── src/
│   ├── <plugin>.h           identity constants + the preset bindings
│   ├── plugin.cpp           CLAP entry point, host glue, audio callback
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
```

Reference versions currently in use: `clap` 1.2.10, `clap-wrapper` v0.16.0,
`clap-validator` 0.4.1, `clap-info` v1.2.2.

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

It produces ten archives, so a site can offer a plain "Windows download" beside
a "Linux download" for each plugin as well as for the suite:

```
RainyDay-1.5.1-linux-x86_64.zip             one plugin, one platform
RainyDay-1.5.1-windows-x86_64.zip
ThunderClap-1.0.0-linux-x86_64.zip
ThunderClap-1.0.0-windows-x86_64.zip
ShoreBreak-0.1.0-linux-x86_64.zip
ShoreBreak-0.1.0-windows-x86_64.zip
verdalis-suite-0.2.0-linux-x86_64.zip       every plugin, one platform
verdalis-suite-0.2.0-windows-x86_64.zip
verdalis-suite-0.2.0.zip                    everything, both platforms
```

**Everything is a `.zip`, the Linux builds included.** That is not the Unix
habit, but the download manager the site runs handles zip alone, and a release
nobody can publish is worse than one in the wrong format. Nothing is lost by it:
zip records Unix permissions, and a `.clap` is `dlopen`'d, which needs no execute
bit — verified by extracting one and loading it. `--tarball` emits `.tar.gz`
alongside for anywhere that prefers it.

The per-plugin archives carry that plugin's **own** version from its `project()`
line, not the suite's, because they are downloaded and updated separately.

Every archive is self-contained: the plugin folder with its presets, the
plugin's README, the LICENSE, a BUILD-INFO.txt, and an INSTALL.txt written for
that operating system alone -- somebody downloading the Windows build should not
have to read past the Linux instructions to find theirs.

```
RainyDay-1.5.1-windows-x86_64/
├── RainyDay/
│   ├── RainyDay.clap
│   └── presets/            (17 files)
├── README.md
├── LICENSE
├── INSTALL.txt             Windows only
└── BUILD-INFO.txt
```

Options: `--tarball` adds `.tar.gz` beside every `.zip`; `--linux-only` skips
the Windows half; `--windows-no-gui` allows a
Windows build with no plugin window. Offline tools are switched off for release
builds (`-D<PLUGIN>_BUILD_TOOLS=OFF`).

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

## Shared components

`shared/` holds the code the whole suite is built on, compiled as the static
library `verdalis::shared` and reached through `<verdalis/...>` includes:

```
shared/
├── CMakeLists.txt              builds verdalis::shared
├── include/verdalis/
│   ├── params.h                ParamDesc, ParamKind, FilterKind, conversions
│   ├── param_macros.h          table-building shorthand (params.cpp only)
│   ├── preset.h                PresetContext, PresetData, the text format
│   ├── preset_provider.h       PresetProviderSpec, the discovery factory
│   ├── dsp/{adsr,denormals,fastmath,filters,reverb,rng}.h
│   └── gui/
│       ├── gui.h               Gui + GuiDelegate, the plugin/window contract
│       ├── toolkit.h           Cairo drawing primitives, Rect, Align
│       └── window.h            Theme, PanelSpec, HeaderOrnament, WindowSpec
├── src/{params,preset,preset_provider}.cpp
├── src/gui/window.cpp          the window: layout, widgets, browser, entry
├── cmake/                      embed_presets, mingw toolchain, Windows Cairo,
│                               clap_entry.version
└── tools/                      install-plugin.sh, fithost.cpp, analysis/wavio.py
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
the same bolt. `animating()` decides whether the window repaints, and must not
change state. A plugin with nothing to animate leaves `ornament` null.

**The delegate** (`verdalis/gui/gui.h`) is deliberately generic:
`guiVoiceCount()` and `guiVoiceLimit()` are whatever the activity meter counts
-- droplets, shock waves, grains -- and `guiEventCounter()` is an optional
monotonic count of discrete events, defaulting to zero. The meter's wording
comes from the spec's `voiceNoun` and `eventNoun`.

### What each plugin still owns

Genuinely per-plugin, and correctly so:

- `src/dsp/<name>_engine.{h,cpp}` — the synthesis itself
- `src/params.cpp` / `params.h` — its ParamId enum and its ParamDesc table
- `src/preset.cpp`, `src/preset_provider.cpp` — ~35-line bindings that supply
  the plugin's name, extension and table to the shared implementations
- `src/gui/gui.cpp` — its theme, its panel layout and its header ornament
- `presets/`, `README.md`, `STATUS.md`, `TODO.md`, `!dev/`

### Still duplicated

One file remains substantially shared but is **not** extracted:

| File | Size | Differing lines | What blocks extraction |
|------|------|-----------------|------------------------|
| `src/plugin.cpp` | ~1140 | ~128 | The CLAP lifecycle is common; the engine type is not. Wants a template parameter or an engine interface, and is best done when a third plugin shows which parts are really common. |

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
7. Keep the visual language: the window comes from `shared/`, so the layout
   engine and geometry are automatic. What the plugin writes is its own `Theme`
   (a new accent, with the greys tinted towards it), its panel table, and a
   `HeaderOrnament` if it has something to animate. See *The window, and its
   theme*.

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

- **Pure synthesis.** No samples ship in any plugin, ever.
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
