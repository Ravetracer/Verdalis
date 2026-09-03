# RainyDay — current status

Written 2026-09-03. See `README.md` for the design and parameter reference, and
`TODO.md` for what is still open (preset saving is now the headline item).

## State: working and playable

A complete, native Linux CLAP instrument that synthesises rain. Builds clean
with GCC 13 (`-Wall -Wextra`, no warnings), passes 30 self-test checks, and is
installed to `~/.clap/RainyDay/`.

## What is implemented

**Synthesis** (`src/dsp/`) — no samples, everything computed:

- Droplets arrive as a Cox process (Poisson with a modulated rate), giving
  correct clustering statistics rather than jittered regularity.
- Each droplet is three layers: a chirped decaying sine, a noise burst through a
  resonant state-variable bandpass, and a short broadband impact click. Then a
  one-pole air-absorption lowpass and equal-power panning.
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

- 36 parameters, grouped, with real-unit display and text entry both ways.
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
- All 36 parameters laid out from the parameter table itself, so the panels are
  the modules and the help line is each parameter's own tip.
- Preset browser over the factory library plus `~/.config/RainyDay/presets`,
  loading through the same `clap.preset-load` path a host uses.
- Knob moves leave as real `PARAM_GESTURE_BEGIN` / `PARAM_VALUE` /
  `PARAM_GESTURE_END` events, via a lock-free queue drained by `process()` and
  `params.flush()`, so host automation recording behaves.
- Droplet-activity meter fed by an atomic the audio thread publishes.

**Presets**: 16 factory presets in a documented plain-text format, served both
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

## Pick it up here

```sh
./install.sh                                        # build, self-test, install
./build/rainyday-render --selftest                  # 30 checks
./build/rainyday-render --list                      # preset discovery
./build/rainyday-render --all --outdir /tmp/rain    # render the library
```

Then in Bitwig: rescan plug-ins, drop RainyDay on an instrument track, hold a
long note.

## Known limitations

- The window is X11 only and a fixed size; under a Wayland host the plugin
  falls back to the generic parameter view.
- Presets can be loaded but not yet saved from the plugin.
- Rain only, by design. No wind, no thunder.
- `Filter Key Track` follows the most recently played note (single global
  filter stage).
- Per-note parameter modulation and note expressions are ignored.
- Not yet run through `clap-validator`: version 0.4.1 requires rustc >= 1.95
  and this machine has 1.90.
- The sparse drip presets match their references less closely than the dense
  rain ones. Isolated drops in a room are dominated by the room, and the
  reference recordings carry reverb the synth has to approximate with a single
  feedback delay network.
- Not under version control yet — no git repository was initialised.
