# RainyDay — open items

## 1. GUI follow-ups

- **Text entry on a knob.** `paramTextToValue()` already parses everything the
  display prints, including `k` multipliers and seconds on millisecond fields.
  The save dialog now has a working text field to copy from, so what is left is
  routing a click on a value to it and deciding what happens when the host does
  not give the window key events.
- **Keyboard focus for the embedded window.** Typing into the save field needs
  the host to route key events through. The window asks for the focus when the
  field opens, and pre-fills the name so mouse-only saving still works, but this
  has only been tried outside a DAW. Worth checking in Bitwig.
- **Resizable window.** `can_resize` currently reports false and the layout is
  a fixed 960×740 in design pixels. Everything is already drawn through a cairo
  scale, so honouring `set_size` is mostly a matter of choosing a scale from
  the requested size and reporting sensible resize hints.
- **Wayland.** Only the X11 window API is offered. Under a Wayland host this
  falls back to the generic parameter view.
- **A scrollbar in the browser.** Presets past the panel's height are currently
  not drawn. Sixteen factory presets in three columns fit comfortably; a large
  user library would not.
- **The selector list has no keyboard or scroll handling.** Clicking the name
  opens it and clicking an entry picks one, but arrow keys do not move through
  it and the wheel does not scroll it. Fine for four and seven entries; worth
  revisiting if a list ever gets long, which `kSurfaces` plausibly will.

`tools/guihost.cpp` opens the editor outside a DAW, which is how any of this
gets checked. It opens a window on the current display, so it is not something
to run unannounced.

## 2. Later / nice to have

- **Wind and thunder.** Explicitly out of scope for now; the plugin is rain
  only. When added, they belong as separate parameter groups, and thunder needs
  its own event scheduler rather than reusing the droplet pool.
- **Per-note modulation.** `CLAP_EVENT_PARAM_MOD` is handled globally;
  per-note-id modulation is currently ignored.
- **Per-voice filter.** `Filter Key Track` follows the most recent note
  because the state-variable filter is a single global stage.
- **Fit the new layer controls.** `Bed Width` and `Drop Width` were split out of
  the old single `Stereo Width`, and every preset simply kept its old value for
  both, which is why the library measures exactly as it did before. Nothing has
  yet asked whether a preset wants its bed wider than its droplets, and the
  reference recordings are mono, so this needs either stereo references or a
  decision made by ear.
- **More surfaces.** The surface model is a small table in
  `src/dsp/rain_engine.cpp` (`kSurfaces`); adding e.g. canvas, water butt or
  car roof is a one-line change plus a parameter enum entry.
- **More reference recordings.** The fit in `tools/analysis/` is only as good as
  what it is fitted to. Distant Rain Wall has no usable reference at all and is
  derived from the fitted Downpour by hand; Gutter Trickle and Window Pane are
  matched to the nearest neighbour rather than to themselves. A true far-field
  recording, a downspout, rain on a tent and rain on a water surface would each
  earn their keep.
- **Teach the fit to tell a droplet from a room.** The objective in
  `tools/analysis/` has no feature that separates a long droplet ring from a
  long reverb tail, and on Cave Drips it put the cavern inside the droplet:
  `drop_decay` was fitted to 260 ms with `space_amount` at 0.07, which reads as
  a synthetic swoop rather than a drip in a cave. It needs a per-band
  reverberation-time feature, and probably a ceiling on `drop_decay` relative to
  the surface, before the space controls can be fitted at all.
