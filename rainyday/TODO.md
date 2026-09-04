# RainyDay — open items

## 1. Realism: what two more sources say we are still missing

Added 2026-09-04 from `!dev/RealtimeSoundSimulationOfRain.pdf` (Miklavcic, Zita
& Arvidsson, DAFx'04) and the "How it works" of `github.com/gtnoble/drip`. Both
agree with what the engine already does -- Marshall-Palmer sizing, the entrained
bubble as the tonal layer, only some impacts entraining one, air absorption, a
statistical far field -- so what follows is only the difference. Ranked by how
much realism each is likely to buy.

Every one of these changes the sound of every preset and so costs a full re-fit.
1a and 1b are done, 1d is half done, and 1c is rejected -- each with the
measurements that settled it, so none of them gets tried again on the strength
of the same argument.

### 1a. Amplitude should follow impact energy, not drop mass -- DONE

The engine sets droplet amplitude proportional to volume, i.e. to mass. drip
uses `E_acoustic = 0.001 x (1/2 m v^2)`: about a tenth of a per cent of the
kinetic energy becomes sound. Amplitude is the square root of energy, so the law
should be `A ~ sqrt(m) * v_term`, not `A ~ m`.

Terminal velocity is itself a function of size (roughly 2 m/s at 0.5 mm rising
to 9 m/s at 5 mm), so this is not a constant factor. Over a ten-to-one range of
radius the present law spreads amplitude over 1000:1 where the physical one
gives about 143:1 (the 95:1 first written here was optimistic; it needs a range
where the velocity has saturated at both ends). If that is right, RainyDay is exaggerating its big drops
by an order of magnitude, which would read as isolated loud plonks over a bed
rather than as rain. **This is the most suspicious single thing in the engine**
and it is cheap to test: it is one expression in `spawnDroplet`.

Built 2026-09-04. Measured rather than assumed: over the range the engine draws,
the old law spread loudest against quietest by 130:1 and the energy law gives
45:1; over 0.5 to 5 mm it is 1000:1 against 143:1. So the order of magnitude was
right. Mean loudness still holds within half a decibel across Level Spread.

### 1b. The impact frequency is drawn at random -- DONE

`kImpactMinHz`..`kImpactMaxHz` drew the impact blip uniformly between 1 and
16 kHz for every droplet on every surface. That came from Liu, Cheng & Tong
(2019); drip derives it for a rigid surface as `v / 2R`, the drop's speed over
its own diameter.

Built 2026-09-04. Both sources are honoured and the surface chooses: liquid
surfaces keep the statistical draw, rigid ones derive it from the drop, and
`impactFromDrop` blends between them in the log domain. For the sizes the engine
draws, `v / 2R` lands between about 3 and 4 kHz, against four octaves of noise.

One correction to drip on the way. A single deterministic frequency per drop
measures as markedly more tonal than the recordings -- every rigid-surface
preset fell below its reference on per-frame flatness -- which is right, because
`v / 2R` is a characteristic contact time and the real one varies with the angle
a drop arrives at and how far it flattens. It sets the centre of a spread
instead. The width is a modelling choice: sweeping it from half an octave to two
moved the flatness by under 0.01, so the measurements have no opinion.

### 1c. Anchor the bubble pitch to Minnaert -- REJECTED, the numbers refute it

Minnaert gives `f = 3.26 / R`, so an audible plink at 1 kHz needs a bubble of
3.26 mm radius: a 6.5 mm bubble, larger than most raindrops entirely. Working
the other way, a bubble entrained by a 1.5 mm drop at the 0.1 to 0.3 of drop
radius the literature reports rings somewhere between 14 and 44 kHz, and even a
5 mm drop only reaches 4 to 13 kHz. RainyDay's fitted `Drop Pitch` values run
from 529 Hz to 8.3 kHz with a median of 2.1 kHz, which is where the recordings
plainly have their tonal energy.

So anchoring the tonal layer to Minnaert would push nearly all of it ultrasonic
and delete the character every preset is built on. The audible pitched content
in real rain is not a Minnaert bubble from a single raindrop; it is more likely
cavity and surface resonance, or larger coalesced bubbles.

It also would not buy what it promised. Minnaert relates pitch to *bubble*
radius, and nothing fixes the bubble radius given the drop, so the freedom moves
from "Drop Pitch in Hz" to "entrainment ratio" and the first is far more use to
whoever is turning the knob. The `1/R` relationship the engine already has is
the part of Minnaert that survives contact with the measurements, and the 2019
paper confirmed it. Physically implausible values are kept out by the per-preset
bounds in `fit.py`, which is where that belongs.

### 1d. Distance is a gain and a filter, not a distance -- PARTLY DONE

The propagation delay is built: a droplet's distance now sets when it is heard,
`r / 343 m/s`, so near droplets are loud, bright and early together instead of
the first two being asserted without the third. `startOffset` counts across
blocks to allow it.

It is worth almost nothing audibly, which is worth writing down. Delaying a
Poisson process by independent random amounts leaves a Poisson process of the
same rate, so a steady rain texture cannot tell the difference; measured, it
costs about 1 dB in the first 100 ms after a note and nothing after 500 ms. It
is kept because it is correct and free, not because it is a depth cue. The claim
that it was "the cheapest depth cue there is" was wrong.

The other two parts are **rejected**:

- Inverse-square spreading. True `1/r^2` across a 30 m field with any sane near
  limit is a 50-plus dB range, which makes Distance unusable as a control. The
  existing `1 / (1 + 3d)` is a deliberate regularisation, not an oversight.
- drip's air absorption, `alpha(f) = 0.02 (f/1000)^1.5 dB/100 m`. That is
  0.19 dB at 10 kHz over the whole field, and roughly sixteen times less than
  standard atmospheric absorption figures at every frequency checked between
  1 and 10 kHz. Adopting it would be a large regression against the exponential
  lowpass already there, which is more aggressive than physical absorption but
  is an artistic control the whole library is fitted around.

### 1e. Not applicable, recorded so it is not rediscovered

The DAFx paper's own conclusion is that bare rain rarely sounds like rain: what
listeners recognise is rain plus rooftops resonating, water flowing, leaves,
wind. That is a real finding, and it is also exactly the scope this project has
decided against. Worth remembering when a preset sounds thin for no reason the
measurements can explain.

## 2. The fit overfits its own seeds, and it is costing real presets

Each candidate is scored on two seeds (`FIT_SEEDS` in `fit.py`) and verified on
three others. On the sparse presets the gap between the two is enormous, because
a seven-second render of a preset at one drip a second contains about a dozen
audible events and the objective is averaging over noise: Dripping Faucet
reached 28.7 on the seeds it was fitted against and 732.1 on unseen ones, Storm
Front 23.4 and 194.9.

The immediate consequence is that six of the fifteen fitted presets in the last
run had to be thrown away, having measured worse on seeds the fit never saw. The
work was done and then discarded.

The fix is not subtle -- average more seeds per candidate -- and it costs
proportionally more time, which is why it has not simply been done: the last
full fit already took 68 minutes at two seeds. Worth measuring first how many
seeds it actually takes for the sparse presets to stabilise, rather than
guessing, since the dense ones clearly do not need it. A per-preset seed count
would buy most of it for very little.

`dripping_faucet` at 436.6 is now the worst preset in the library by a wide
margin and is the obvious test case.

## 3. GUI follow-ups

- **Text entry on a knob.** `paramTextToValue()` already parses everything the
  display prints, including `k` multipliers and seconds on millisecond fields.
  The save dialog now has a working text field to copy from, so what is left is
  routing a click on a value to it and deciding what happens when the host does
  not give the window key events.
- **Keyboard focus for the embedded window.** Confirmed in Bitwig: asking for
  the input focus does not work, so the save dialog takes a keyboard grab
  instead. That is defensible for a modal field but it does not generalise --
  text entry on a knob cannot grab the keyboard every time a value is clicked.
  The principled fix is the XEmbed protocol: set `_XEMBED_INFO` on the window,
  handle the `_XEMBED` client messages, and ask the embedder for focus with
  `XEMBED_REQUEST_FOCUS`. Worth doing before any further text entry is added,
  and it needs a real host to test against.
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

## 4. Later / nice to have

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
