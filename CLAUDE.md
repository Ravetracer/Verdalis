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
work included. The old repositories were deleted afterwards; **this repository
is the only copy of that history.**

Commit at suite level. There are no per-plugin remotes any more.

## Suite roadmap

Seven plugins, each modelling one natural sound source:

| # | Plugin | Folder | Status | Simulates |
|---|--------|--------|--------|-----------|
| 1 | **RainyDay** | `rainyday/` | WIP | rain |
| 2 | **ThunderClap** | `thunderclap/` | WIP | thunder |
| 3 | **ShoreBreak** | `shorebreak/` | planned | ocean waves |
| 4 | **SkyHowl** | `skyhowl/` | planned | winds, storms |
| 5 | **ChirpParade** | `chirpparade/` | planned | bird chirps |
| 6 | **RiverFlow** | `riverflow/` | planned | rivers, streams |
| 7 | **CrackleBlaze** | `crackleblaze/` | planned | fire |

Naming follows a consistent pattern: a two-word CamelCase compound naming the
phenomenon, lowercase and joined for the folder, the CLAP id
(`de.ravetracer.<folder>`) and the preset extension (`.<folder>`).

**This roadmap is the argument for extracting `shared/` now.** Every one of the
five remaining plugins will start as a copy of an existing one, and each copy
duplicates the DSP toolbox and the entire GUI layer again. Extracting after
plugin 7 means unpicking seven copies; extracting before plugin 3 means the
remaining five are built against a shared foundation from their first commit.
See *Shared components* below.

Likely shared additions as the suite grows — worth anticipating rather than
retrofitting:

- ShoreBreak, RiverFlow and RainyDay are all **water**, and will want a common
  bubble/droplet resonator and filtered-noise bed.
- SkyHowl and ThunderClap both need **large-scale air movement** and distance
  modelling; ThunderClap's `Lp2` air-absorption filter generalises directly.
- CrackleBlaze and RainyDay share **stochastic impulse spawning** — the Poisson
  process in `rng.h` already covers both.
- ChirpParade is the outlier: it needs pitched, formant-shaped voices rather
  than noise, so expect it to add to the shared DSP rather than reuse much.

## Plugin anatomy

Every plugin follows the same skeleton. Reproduce it exactly when starting a new
one — the consistency is what makes shared components possible.

```
<plugin>/
├── CMakeLists.txt           C++17, CLAP module + offline tools
├── install.sh               configure → build → self-test → install to ~/.clap
├── README.md                user-facing documentation
├── STATUS.md                what works, what is measured
├── TODO.md                  planned work
├── cmake/
│   ├── <plugin>.version     linker script: exports only clap_entry
│   ├── embed_presets.cmake  bakes presets/ into a generated header
│   ├── mingw-w64-x86_64.cmake  Windows cross-build toolchain
│   └── build-windows-cairo.sh
├── presets/                 factory presets, one file per preset, .<plugin>
├── src/
│   ├── <plugin>.h           shared plugin-wide declarations
│   ├── plugin.cpp           CLAP entry point, host glue, audio callback
│   ├── params.{h,cpp}       parameter table and formatting
│   ├── preset.cpp           preset (de)serialisation
│   ├── preset_provider.cpp  CLAP preset-discovery factory
│   ├── factories.h
│   ├── dsp/                 the synthesis engine + DSP building blocks
│   └── gui/                 X11/Win32 + Cairo plugin window
├── tools/
│   ├── render.cpp           offline renderer + self-test
│   ├── fithost.cpp          parameter fitting against reference recordings
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

Pre-existing `build/` and `build-win/` trees inside the plugins still carry
absolute paths from before the move — **delete them and reconfigure** before the
next local build. A fresh configure and full build of both plugins from the new
location is verified working.

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

Archive layout:

```
verdalis-suite-<version>/
├── README.md, LICENSE, INSTALL.txt, BUILD-INFO.txt
├── linux/     RainyDay/RainyDay.clap + presets/, ThunderClap/…
└── windows/   RainyDay/RainyDay.clap + presets/, ThunderClap/…
```

Options: `--linux-only` skips the Windows half; `--windows-no-gui` allows a
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

The two plugins were built in sequence, and the second inherited the first's
structure. A file-level comparison shows how much is genuinely common — this is
the roadmap for the planned `shared/` folder.

**Identical, or identical apart from the plugin name** — these are already
shared code that merely got copied, and should move to `shared/` as-is:

- `src/factories.h`
- `src/preset.cpp` — preset serialisation
- `src/preset_provider.cpp` — CLAP preset-discovery factory
- `src/dsp/denormals.h`
- `cmake/embed_presets.cmake`
- `cmake/mingw-w64-x86_64.cmake`
- `cmake/build-windows-cairo.sh`
- `cmake/<plugin>.version` — the linker script, byte-identical
- `tools/fithost.cpp`
- `tools/analysis/wavio.py`
- `install.sh`, `.gitignore`, `LICENSE`

**Same code, diverged only by domain-flavoured comments plus small per-plugin
additions.** These need a superset with neutral wording, not a copy:

| File | Divergence |
|------|-----------|
| `src/dsp/rng.h` | RainyDay adds `RngLite` (4-byte state, per-droplet) |
| `src/dsp/fastmath.h` | RainyDay adds `SinTable` / `sin2piFast` |
| `src/dsp/filters.h` | RainyDay adds `Svf::ringing()`; ThunderClap adds `Lp2` |
| `src/dsp/reverb.h` | Same tank; ThunderClap lowers `kLoopHighpassHz` 35 → 16 Hz |
| `src/dsp/adsr.h` | Comments only |

**The GUI is one toolkit with a per-plugin palette.** `src/gui/gui.cpp` has the
same top-level structure in both — `Cell`, `PanelSpec`, `Rgb`, `roundedRect`,
`drawTriangle`, `drawText`, `textWidth`, `upperCase`, `normalised`, `isChip`,
`isStepped`, `isBipolar`, `Rect`, `X11Gui` — with an identical layout engine and
identical static layout assertions. What actually differs is the colour table
and the engine-specific meter. ThunderClap's own comment states the intent:
*"RainyDay's greys, an octave darker, with the sky's own colour for the accent."*

This is the strongest case for extraction: a shared widget/layout/paint layer
plus a small per-plugin theme struct.

**Genuinely per-plugin, never share:** `src/dsp/<name>_engine.{h,cpp}`,
`src/params.{h,cpp}`, `presets/`, `README.md`, `STATUS.md`, `TODO.md`, `!dev/`.

### Extraction is not done yet

`shared/` does **not** exist. The analysis above is the plan, not the state.

The monorepo removes what used to be the hard part: a sibling `shared/` folder
is now plainly reachable from every plugin, so extraction is an ordinary
refactor — no submodules, no subtrees, no vendoring. A `shared/` directory with
`add_subdirectory(../shared)` from each plugin's `CMakeLists.txt` is enough.

Suggested order, easiest first:

1. The already-identical files — `preset.cpp`, `preset_provider.cpp`,
   `factories.h`, `denormals.h`, the `cmake/` helpers, `fithost.cpp`,
   `wavio.py`. Pure de-duplication, no behaviour change.
2. The DSP headers, as a superset with neutral comments.
3. The GUI: shared layout/paint/widget layer plus a per-plugin theme struct.

Do this before the third plugin exists, not after — every new plugin copies the
duplication forward.

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
7. Keep the visual language: same layout engine and geometry, new accent hue.

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

Note the plugin windows do **not** use this palette directly: each plugin keeps
its own near-black chassis with a hue that suits its subject (RainyDay's rain
blue `#58B6E8`, ThunderClap's lightning violet `#B396FA`). The suite palette is
for brand surfaces — README, site, packaging. Keep plugin windows identical in
layout and geometry, distinct only in accent hue.

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
