# RainyDay — current status

Written 2026-09-03. See `README.md` for the design and parameter reference, and
`TODO.md` for what is still open (preset saving is now the headline item).

## State: working and playable

A complete, native Linux CLAP instrument that synthesises rain. Builds clean
with GCC 13 (`-Wall -Wextra`, no warnings), passes 43 self-test checks, and is
installed to `~/.clap/RainyDay/`.

## What is implemented

**Synthesis** (`src/dsp/`) — no samples, everything computed:

- Droplets arrive as a Cox process (Poisson with a modulated rate), giving
  correct clustering statistics rather than jittered regularity.
- Each droplet is four layers: a chirped decaying sine for the entrained bubble,
  a quieter second bubble mode near twice that frequency, a noise burst through
  a resonant state-variable bandpass, and the initial impact — a two-cycle
  damped sine whose frequency is drawn afresh for every droplet, uniformly
  between 1 and 16 kHz. Bubble, second mode and splash go through the droplet's
  radiation highpass; the impact does not, because it is the surface being
  struck. Then a one-pole air-absorption lowpass and equal-power panning.
- Not every impact traps a bubble. `Bubble Chance` is the fraction of droplets
  that ring at all; the rest are splash and tick with no pitch.
- One drop **size** is drawn per droplet from a Marshall-Palmer-like skewed
  distribution, and amplitude, pitch and ring time all derive from it, so big
  drops are automatically loud, low and long.
- Far-field droplets are summed statistically into a filtered, drifting,
  width-controlled noise bed instead of being synthesised individually.
- 4-line feedback delay network with an orthonormal Hadamard matrix for space.
- Loudness is normalised against expected droplet concurrency, so `Density` is
  a texture control and not a hidden volume control.
- The noise bed is a band rather than a lowpass, each droplet has a 12 dB/oct
  radiation rolloff below its own pitch, and the pitch spread is skewed upward.
  Together these are what stop the bottom two octaves flooding: real rain sits
  30 to 50 dB down at 100 Hz and the engine now does too.

**Plugin side** (`src/plugin.cpp`):

- 41 parameters, grouped, with real-unit display and text entry both ways.
- Extensions: `params`, `audio-ports`, `note-ports`, `state`, `tail`,
  `voice-info`, `preset-load`, `gui`, `timer-support`.
- Sample-accurate event handling (the block is split at every event boundary),
  CLAP and raw-MIDI note dialects, host parameter modulation, 16 voices with a
  shared droplet pool so CPU stays bounded.
- Versioned, per-parameter-id state, so adding parameters later cannot break
  saved projects.
- Denormals are flushed inside `process()` only, restoring the host's FPU mode
  on exit. Deliberately not built with `-ffast-math`, which would change the
  mode process-wide.
- Exactly one exported symbol (`clap_entry`), pinned by a linker version script.

**GUI** (`src/gui/gui.cpp`) — raw X11 and Cairo, no toolkit:

- Embedded through `CLAP_EXT_GUI` (X11, non-floating), repainted from the
  host's timer at 30 Hz, with a fallback thread for hosts that offer no timer.
- All 41 parameters laid out from the parameter table itself, so the panels are
  the modules and the help line is each parameter's own tip.
- Preset browser over the factory library plus `~/.config/RainyDay/presets`,
  loading through the same `clap.preset-load` path a host uses, and a `SAVE`
  button that writes the current settings back out to that directory in the
  same text format and rescans it.
- Enum parameters open a list rather than only stepping one value per click.
- Knob moves leave as real `PARAM_GESTURE_BEGIN` / `PARAM_VALUE` /
  `PARAM_GESTURE_END` events, via a lock-free queue drained by `process()` and
  `params.flush()`, so host automation recording behaves.
- Droplet-activity meter and a dB-scaled output peak meter, both fed by
  atomics the audio thread publishes.
- The panel grid is checked at compile time: every parameter must sit on
  exactly one panel, no panel may hold more parameters than it has cells, and
  no row may be wider or taller than the window.

**Presets**: 17 factory presets in a documented plain-text format, served both
from disk and from copies embedded in the binary, exposed via the CLAP
preset-discovery factory (so they appear in the host's own browser).

The library was fitted numerically against a set of reference recordings of
real rain (`tools/analysis/`): measure the recording and RainyDay's output with
the same feature set, then coordinate-descend the preset values until they
agree. Mean distance to the references fell by roughly 18x; the dense rain
presets now match within a few dB in every band. Output Gain is matched
afterwards across the library at -22 dBFS RMS, backing off where that would
push the peak past -4 dBFS.

**Verification** (`tools/render.cpp`): a real mini CLAP host that walks the
preset-discovery factory the way a DAW does, renders WAVs, and runs 30
host-contract checks (parameter metadata, text round-trips, state
save/change/restore equality, garbage-state rejection, out-of-range clamping,
all parameters at their extremes, odd block sizes, silence before the first
note, activate/deactivate cycles).

It also passes `clap-validator` 0.4.1: 44 tests run, 38 passed, 0 failed. The
six skips are all "not applicable" -- no 64-bit audio path, no input audio port
for the denormals test, and three optional port-layout extensions the plugin
does not implement.

## Windows

Cross-compiled from Linux with MinGW-w64; the Linux build is unaffected.

```sh
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-win
```

The window needs a Cairo cross-built with its win32 backend, which
`cmake/build-windows-cairo.sh` does; pass its prefix as `RAINYDAY_WIN_CAIRO`.
Without it the build still works and the host draws the parameters itself.

The result is `build-win/RainyDay.clap`, a PE32+ DLL exporting `clap_entry` and
nothing else, with the runtimes and Cairo linked in so it imports only KERNEL32,
USER32, GDI32, MSIMG32, ole32 and msvcrt. It goes in
`C:\Program Files\Common Files\CLAP\RainyDay\` alongside a `presets` folder.

Verified under Wine rather than assumed: all 45 self-test checks pass against
the Windows DLL, preset discovery finds all 17 presets, the window renders and
is pixel-identical to the Linux one everywhere except glyph rasterisation, and
at a fixed Random Seed the audio matches the Linux build to within 1 LSB on 14
samples out of 672000, which is the two toolchains' libm rounding differently.
It has not been run on real Windows.

The porting work was four POSIX-only spots -- `dladdr` for the plugin's own
path, `$XDG_CONFIG_HOME` for the preset folder, `dirent.h` for scanning it, and
a hand-rolled `mkdir -p` -- which are now `std::filesystem` plus
`GetModuleFileNameW`, and `dlopen` in the test host.

## Pick it up here

```sh
./install.sh                                        # build, self-test, install
./build/rainyday-render --selftest                  # 43 checks
./build/rainyday-render --list                      # preset discovery
./build/rainyday-render --all --outdir /tmp/rain    # render the library
```

Then in Bitwig: rescan plug-ins, drop RainyDay on an instrument track, hold a
long note.

## Known limitations

- The window is a fixed size, and X11 or Win32 only; under a Wayland host the
  plugin falls back to the host's generic parameter view.
- Rain only, by design. No wind, no thunder.
- `Filter Key Track` follows the most recently played note (single global
  filter stage).
- The save dialog holds a keyboard grab for as long as it is open, so the host's
  own shortcuts do not work while a preset name is being typed. This is how the
  field gets keystrokes at all: an embedded window is not given the input focus
  by every host, Bitwig among them, and CLAP has no way to ask for it.
- Per-note parameter modulation and note expressions are ignored.
- The sparse drip presets match their references less closely than the dense
  rain ones. Isolated drops in a room are dominated by the room, and the
  reference recordings carry reverb the synth has to approximate with a single
  feedback delay network.
- The fit still overfits its own seeds on the sparse presets, though far less
  than it did. Scoring each candidate over twelve seeds instead of two took
  Dripping Faucet from 732.1 on held-out seeds to 43.0, and Storm Front from
  197.7 to 70.0. Fitted values are still accepted only where they also win on
  three seeds the fit neither optimised nor verified against; six of sixteen
  were rejected on that test.
- Inside the Car is 91.9 against 54.6 before the impact model changed. Its
  residual is 5 dB heavy at 3 to 6 kHz and 13 dB short at the top, which is
  where the drop-derived impact frequency now sits, and a car roof takes that
  at full strength. Tin Roof and Concrete Alley improved from the same change,
  so it is not uniformly right for rigid surfaces.
