# RainyDay — open items

## 1. Next up: Close/Distant layers, a dedicated highpass, an output meter

Agreed 2026-09-03, after looking at a commercial rain plugin for ideas. The
design below was worked out before the session ended; it is not started.

### 1a. Separate Close and Distant layers

RainyDay already splits the near droplets from the far-field bed internally,
but exposes a single global `Stereo Width` that drives both, and no pan at all.
Give each layer its own placement.

Four new parameters, **appended** to the table so existing ids keep their
meaning:

| key | name | module | range | default |
|---|---|---|---|---|
| `drop_pan` | Drop Pan | Close | -1 .. +1 | 0 |
| `bed_width` | Bed Width | Bed | 0 .. 1 | 0.85 |
| `bed_pan` | Bed Pan | Bed | -1 .. +1 | 0 |

`width` (id 18) keeps its key so presets still load, but is renamed to
**Drop Width** and moved to a `Close` module -- it already only controls
droplet panning once `bed_width` exists.

Engine:

- Droplet pan becomes `clamp(white() * dropWidth + dropPan, -1, 1)`.
- `mBedMixA` / `mBedMixB` derive from `bedWidth` instead of `width`.
- Bed pan is an equal-power balance applied to the already-decorrelated pair,
  the same mapping the droplets use.

**Before building this**, write `bed_width = <that preset's width>` into all 16
presets. Otherwise every one of them silently picks up the 0.85 default instead
of its own fitted width, and the library drifts off its fit.

### 1b. Dedicated highpass

Add `highpass` (Filter module, log 20 Hz .. 2 kHz, default 20 Hz = off) as an
always-available 12 dB/oct `Hp2` on the output chain, bypassed under about
25 Hz. This does **not** replace the existing multimode filter: that keeps
providing the lowpass every preset already uses, plus bandpass and notch. Given
how much of this project turned out to be about rain having no low end, a
permanent highpass earns its place next to it.

### 1c. Output meter

The audio thread publishes decaying peak levels for L and R into atomics, the
way `mDropletMeter` already works -- the GUI must not read engine state
directly. Draw it inside the existing ACTIVITY panel next to the droplet
history rather than as a panel of its own.

### 1d. Layout

Four new parameters take the table from 37 to 41, which the current 10-column
grid cannot hold. Worked-out arrangement, still 10 columns and 912 px wide,
about 72 px taller:

```
Row A (2 tall):  RAIN 7 cols (14 cells, all 14 now used)  |  DISTANT 3 cols (6)
Row B (2 tall):  ENVELOPE 3 (6) | FILTER 3 (5 of 6) | SPACE 3 (5 of 6) | CLOSE 1 (2)
Row C (1 tall):  OUTPUT 3 (3)   | ACTIVITY 7 (droplet history + output meter)
```

DISTANT is the renamed BED panel and CLOSE sits directly beneath it on the
right-hand edge, so the two layer panels read as a pair. Note that RAIN is now
exactly full: Bubble Chance took the cell that Surface used to occupy as its
second column, so a fifteenth Rain parameter needs a real relayout, not another
cell freed by a span change. The layout is
generated from `kPanelSpecs` in `src/gui/gui.cpp`, so this is a table edit plus
`kWindowH`, not a rewrite.

Explicitly **not** doing: XY pads pairing two parameters per control. Compact,
but a plugin should glare with quality rather than with its interface.

## 2. Preset saving

Reading presets is complete and the browser lists both the factory library and
`~/.config/RainyDay/presets`. Writing one out from the plugin is now the
obvious gap: the window has a preset bar with nowhere to save to. It needs a
serialiser for the text format (the parser in `src/preset.cpp` defines it), a
name-entry field in the window, and a rescan of the user directory afterwards
so the new preset appears in the browser immediately.

## 3. GUI follow-ups

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
- **The selector list has no keyboard or scroll handling.** Clicking the name
  opens it and clicking an entry picks one, but arrow keys do not move through
  it and the wheel does not scroll it. Fine for four and seven entries; worth
  revisiting if a list ever gets long, which `kSurfaces` plausibly will.

`tools/guihost.cpp` opens the editor outside a DAW, which is how any of this
gets checked. It opens a window on the current display, so it is not something
to run unannounced.

## 4. Later / nice to have

- **Wind and thunder.** Explicitly out of scope for now; the plugin is rain
  only. When added, they belong as separate parameter groups, and thunder needs
  its own event scheduler rather than reusing the droplet pool.
- **Per-note modulation.** `CLAP_EVENT_PARAM_MOD` is handled globally;
  per-note-id modulation is currently ignored.
- **Per-voice filter.** `Filter Key Track` follows the most recent note
  because the state-variable filter is a single global stage.
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
