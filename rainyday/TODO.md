# RainyDay — open items

The GUI is done: see the "The plugin window" section of `README.md`. What is
left is below.

## 1. Preset saving

Reading presets is complete and the browser lists both the factory library and
`~/.config/RainyDay/presets`. Writing one out from the plugin is now the
obvious gap: the window has a preset bar with nowhere to save to. It needs a
serialiser for the text format (the parser in `src/preset.cpp` defines it), a
name-entry field in the window, and a rescan of the user directory afterwards
so the new preset appears in the browser immediately.

## 2. GUI follow-ups

- **Text entry on a knob.** `paramTextToValue()` already parses everything the
  display prints, including `k` multipliers and seconds on millisecond fields.
  Only the field and the keyboard focus handling are missing.
- **Resizable window.** `can_resize` currently reports false and the layout is
  a fixed 912×648 in design pixels. Everything is already drawn through a cairo
  scale, so honouring `set_size` is mostly a matter of choosing a scale from
  the requested size and reporting sensible resize hints.
- **Wayland.** Only the X11 window API is offered. Under a Wayland host this
  falls back to the generic parameter view.
- **A scrollbar in the browser.** Presets past the panel's height are currently
  not drawn. Sixteen factory presets in three columns fit comfortably; a large
  user library would not.

## 3. Later / nice to have

- **Wind and thunder.** Explicitly out of scope for now; the plugin is rain
  only. When added, they belong as separate parameter groups, and thunder needs
  its own event scheduler rather than reusing the droplet pool.
- **Per-note modulation.** `CLAP_EVENT_PARAM_MOD` is handled globally;
  per-note-id modulation is currently ignored.
- **Per-voice filter.** `Filter Key Track` follows the most recent note
  because the state-variable filter is a single global stage.
- **`clap-validator` run.** Version 0.4.1 needs rustc >= 1.95, the machine has
  1.90. Worth running once the toolchain is newer; the in-repo self-test covers
  the same contracts in the meantime.
- **More surfaces.** The surface model is a small table in
  `src/dsp/rain_engine.cpp` (`kSurfaces`); adding e.g. canvas, water butt or
  car roof is a one-line change plus a parameter enum entry.
- **More reference recordings.** The fit in `tools/analysis/` is only as good as
  what it is fitted to. Distant Rain Wall has no usable reference at all and is
  derived from the fitted Downpour by hand; Gutter Trickle and Window Pane are
  matched to the nearest neighbour rather than to themselves. A true far-field
  recording, a downspout, rain on a tent and rain on a water surface would each
  earn their keep.
- **Fit the space controls properly.** The reverb in the drip recordings is
  currently approximated by hand. Fitting a feedback delay network against a
  measured decay would need a reverberation-time feature per band, which the
  analysis code does not have yet.
