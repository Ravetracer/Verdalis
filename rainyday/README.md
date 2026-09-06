# RainyDay

A native Linux **CLAP** instrument that generates rain — entirely by synthesis.
There are no samples anywhere in this project: every droplet, every splash and
the whole background wash are computed from noise, oscillators and filters at
run time. Two instances never produce the same rain.

Play a MIDI note and it rains for as long as you hold it.

- 42 parameters covering droplet statistics, impact surface, stereo field,
  distance, space, filter and a full ADSR
- 17 factory presets from a single drip in a cave to a tropical monsoon, each
  fitted against a real recording of the thing it is imitating
- Exposed to the host through CLAP preset discovery, so presets appear in the
  host's own browser
- Sample-accurate note and parameter handling, host modulation support,
  bounded CPU cost
- A plugin window drawn with X11 and Cairo: every parameter, a preset browser,
  typed value entry and a droplet-activity meter, resizable, with no toolkit
  dependency

## Build and install

Requires a C++17 compiler, CMake ≥ 3.16 and the CLAP headers.

```sh
./install.sh
```

That configures, builds, runs the self-test and installs to
`~/.clap/RainyDay/`. Override the destination with
`RAINYDAY_PREFIX=/some/where ./install.sh`.

Manually, if you prefer:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCLAP_INCLUDE_DIR=/path/to/clap/include
cmake --build build
cmake --install build            # defaults to ~/.clap
```

`CLAP_INCLUDE_DIR` is auto-detected from a few common locations (including
`../CLAP/clap/include` next to this repo); pass it explicitly if the configure
step cannot find `clap/clap.h`.

The installed layout matters: the plugin locates its factory presets by looking
for a `presets` directory **next to its own binary**, so keep them together.

```
~/.clap/RainyDay/RainyDay.clap
~/.clap/RainyDay/presets/*.rainyday
```

### Bitwig Studio

`~/.clap` is scanned by default. After installing, restart Bitwig or rescan
under *Settings → Locations → Plug-in Locations*. RainyDay then shows up as an
instrument (`Ravetracer`), and the factory presets are indexed through
CLAP's preset-discovery mechanism.

Drop it on an instrument track, add a long note, and it rains.

## The plugin window

RainyDay draws its own window with raw **X11** and **Cairo** — no toolkit, so
the plugin stays one self-contained `.clap` file and needs nothing a Linux
audio machine does not already have. It is embedded in the host's window
through `CLAP_EXT_GUI` (X11 API, non-floating) and repainted from the host's
timer. The layout is 960×740 in design pixels and the window resizes by zooming it:
the host is told to keep the aspect ratio, and any size it settles on becomes one
cairo scale over the same layout, from half size to four times.

The layout is generated from the parameter table in `src/params.cpp`: panels
are the modules, cells are the parameters, and the help line at the bottom is
the parameter's own `tip`. Adding a parameter there puts it on screen here with
no GUI change.

| Gesture | Effect |
|---|---|
| Drag a knob up/down | Change the value |
| Shift-drag | Fine control (a fifth of the travel) |
| Double-click a knob, or right-click anything | Back to the default |
| Scroll wheel | Step the value |
| Click a selector's name | Open its list and pick a value |
| Click a selector's `◀` / `▶` arrow | Step one choice |
| Click the preset name | Open the browser |
| `◀` / `▶` next to the name | Previous / next preset |
| `SAVE` | Save the current settings as your own preset |

`SAVE` writes the current parameter values to
`~/.config/RainyDay/presets`, creating the directory if it is not there, and the
browser picks the new preset up immediately. The name becomes the filename with
anything awkward replaced, so `My Rain / 2` is saved as `My_Rain_2.rainyday`; the
file is the same plain text format as the factory library and can be edited by
hand afterwards. While the field is open the plugin takes a keyboard grab, because an embedded
plugin window is not given the input focus by every host and CLAP has no way to
ask for it — Bitwig, for one, does not hand it over, and without the grab the
field never sees a keystroke. The grab lasts only as long as the dialog, so the
host's own shortcuts are unavailable until you press Enter or Escape or click
outside. If another client already holds a grab the dialog says so, and saving
under the offered name still works with the mouse.

The browser lists the factory library followed by anything in
`~/.config/RainyDay/presets`, marked `USER`. Loading goes through the same
`clap.preset-load` path a host uses, so there is one code path for it either
way. A `*` after the preset name means a parameter has been touched since it
was loaded.

Knob moves leave the window as real CLAP events — a gesture-begin, the values,
a gesture-end — so host automation recording sees them exactly as it would a
move made in the host's own panel. The window never writes the plugin's
parameters behind the host's back.

The **ACTIVITY** panel plots how many droplets are sounding, published by the
audio thread rather than read out of the engine. Its scale is compressed: rain
that uses 3 % of the droplet ceiling is perfectly ordinary, and a linear meter
would show nothing at all for it.

Build it out with `-DRAINYDAY_BUILD_GUI=OFF`, or let CMake drop it
automatically if X11 and Cairo are missing; the plugin then falls back to the
host's generic parameter view.


## How it is synthesised

Rain is not one sound, it is a very large number of small independent impacts
plus the collective wash they add up to. RainyDay models exactly that.

### 1. Droplet arrival — a Cox process

Droplets are scheduled as a **Poisson point process**: the waiting time until
the next impact is drawn as `-ln(U)/rate`, which is the exact inter-arrival
distribution for events happening independently at a constant average rate.
That is what makes the result sound organic rather than like a machine gun with
jitter added.

`Clumping` turns that rate into a random variable of its own (a *Cox process*,
or doubly stochastic Poisson process), modulated by a band-limited Gaussian
random walk. The log-normal modulation is mean-compensated by
`exp(-σ²/2)`, so the average density stays exactly where you set it while the
rain gains natural surges and lulls.

### 2. A single droplet — five layers

Each impact is a short event assembled from:

| Layer | Model | Controlled by |
|---|---|---|
| Bubble | Phase-accumulated sine, a difference of two exponentials for its amplitude, plus a per-droplet pitch sweep | `Tonality`, `Bubble Chance`, `Chirp`, `Drop Decay` |
| Second mode | A quieter partial near twice the bubble frequency, decaying twice as fast | surface |
| Wet | White-noise burst through a resonant state-variable bandpass tuned to the droplet's pitch | `Splash`, `Tonality` |
| Impact | A two-cycle damped sine at a frequency drawn afresh for every droplet | `Impact` |
| Body | One low mode of the struck surface, at the surface's own frequency and ring time | `Impact`, surface |

Bubble, second mode and splash all pass through the droplet's own radiation
highpass; the impact and the body do not, because they are the surface being
struck and not the droplet radiating. Everything then passes through a one-pole
lowpass standing in for air absorption, and is equal-power panned into the
stereo field.

**The surface has a voice of its own.** A droplet cannot put energy far below
its own resonance, but the thing it lands on can, and every recording of rain on
something has more in the low mids than a cloud of droplets radiating into air
can make: the fitted library was short by about 2 dB between 200 and 400 Hz on
thirteen presets out of sixteen however each was pointed, which is an engine's
bias and not a preset's. So each impact also excites one low mode of the
surface — 240 Hz and 40 ms for a canopy, 320 Hz and 150 ms for a tin roof,
nothing at all for water — scattered a little per droplet because a roof is not
one panel, scaled by `Impact` because it is the strike that sets it going, and
weighted as an *energy* ratio against the click rather than an amplitude one,
since a mode that rings for forty milliseconds carries far more energy than a
two-cycle tick of the same height. It radiates through its own highpass at
0.6× its frequency, for the same reason the droplet does: a panel is small
against the wavelengths below its mode.

**The impact is pitched, not noise.** Following Liu, Cheng and Tong (2019), the
initial impact is modelled as `A·e^(−2f·t)·sin(2πf·t)` with `f` drawn uniformly
between 1 and 16 kHz for each droplet and scaled by the surface's brightness.
Damping at twice the frequency leaves about two cycles, so a single drop is a
tick with a pitch of its own — 0.2 ms at the top of the range, 3.5 ms at the
bottom. One drop sounds like a tick; a thousand a second are broadband, and the
constant redrawing is what gives dense rain its shimmer. A fixed noise burst,
which is what this used to be, gives every drop in the field an identical
transient and measures several dB short of a real recording above 6 kHz.

**Not every drop rings.** Pumphrey and Elmore's measurements, quoted in the same
paper, have only a band of drop sizes entraining an air bubble on every impact;
the rest of the rain is splash and tick with no pitch at all. `Bubble Chance` is
that fraction, drawn per droplet. It is not the same control as `Tonality`:
tonality at 50 % makes every drop half-pitched, which is a uniform mush, while
`Bubble Chance` at 50 % makes half the drops plink clearly and leaves the other
half dry.

**There is a second bubble mode.** Measuring the isolated drops in the reference
recordings finds a partial at 1.8 to 2.15 times the fundamental, 15 to 25 dB
below it, on essentially every drop that lands in water — a bubble pulsating
hard enough to be heard radiates at twice its breathing frequency as well. Its
ratio and level are redrawn per droplet, it bends with the fundamental because
it is a mode of the same bubble, and it decays twice as fast in dB. Surfaces
that trap no bubble do not get it.

The `Chirp` layer is real physics: a droplet hitting water entrains an air
bubble whose resonant frequency **rises** as it shrinks, which is why a drip
into a puddle goes "plink" with an upward bend rather than a flat tone.

Two details of that model matter more than they look:

- **The bend is large, late, and over before the drop is.** Tracked cycle by
  cycle from the zero crossings of an isolated drop — the only method that
  survives the quiet tail — the reference dips from 775 to 728 Hz across the
  first four milliseconds, sits on a plateau near 750 Hz while it is within
  2 dB of peak, and then rises from 846 Hz to 2 kHz between 30 and 110 ms, by
  which point it is 36 dB down. That is +1.47 octaves in total, and almost none
  of it happens while the drop is loud.

  So the model is in two parts. A fast downward dip relaxes away within a cycle
  or two of the attack, and a rise whose per-sample step *grows* geometrically
  carries the rest. The accumulated bend after a fraction `f` of the sweep is
  `(e^(kf) − 1) / (e^k − 1)`; fitting `k` against a 20× time-stretch of the
  reference gives 0.5, and rejects steeper curves outright.

  Two anchoring details decide whether any of this is audible. The sweep
  **finishes at 0.6 ring times and then holds** — `decayCoef` takes the time to
  −60 dB, so that is the −36 dB point, where the reference has finished too.
  Spreading the same bend across the droplet's whole lifetime instead (1.6 ring
  times, about −96 dB) leaves it barely a fifth done before the drop is
  inaudible, which sounds like no bend at all. And the total sweep comes to
  `Chirp × surface` octaves *on average*, drawn per droplet as a uniform `2u`,
  because a field where every drop bends by the same amount does not sound like
  any of them.

  Only surfaces that trap a bubble get the large span — Water and Puddle carry
  1.30 and 1.45 octaves. A drop landing on something rigid excites a fixed mode
  of that thing and barely bends at all, so the hard surfaces stay near a
  hundredth of an octave. It is the back-loading that makes the large values
  usable: spent on the attack instead, anything past a tenth of an octave does
  stop sounding like water and start sounding like a laser.
- **The tone arrives behind the splash.** The impact happens first and the
  bubble is only entrained afterwards, so the tonal layer's envelope is
  `e^(-t/decay) − e^(-t/rise)` rather than a decay from full level. That gives
  it a short swell instead of a hard onset, and how long the swell takes is a
  property of the surface: water and puddles trap bubbles, metal and glass just
  ring on contact.

### 3. Droplet size follows from physics

Rather than randomising amplitude and pitch independently, RainyDay draws a
single **size** per droplet from a Marshall–Palmer-like skewed distribution
(many small drops, few large ones — `Level Spread` sets the skew). Everything
else is derived from it:

- amplitude scales with volume, so with radius cubed
- resonant pitch scales with `1/radius` — big drops plop low, fine drops tick high
- ring time scales with radius — big drops ring longer

So a fat drop is automatically loud, low and long, and a fine one is quiet, high
and short. No parameter tweaking required for that to hold.

### 4. The bed

Individually inaudible far-field droplets are not synthesised one by one, they
are summed statistically into a **noise bed**: two decorrelated white sources
mixed to the requested width (`a·n₁ + b·n₂` / `a·n₁ − b·n₂` with `a² + b² = 1`,
so the channel correlation is `cos(width·π/2)` with no level change), shaped by
a resonant lowpass and a highpass, and modulated by a slow random walk
(`Bed Drift`).

### 5. Space and distance

`Distance` attenuates and darkens, with `Air Absorption` setting how quickly
the high end is lost — near drops are bright and loud, far ones are dull and
soft, per droplet.

`Space` is a room model driven by one physical quantity. `Space Size` is the
dimension of the room, 3 m to 90 m, and everything else follows from it: the
distances the first reflections travel, the mean free path the late tank's
delay lengths are built on, and — through Sabine's law, with `Space Damping` as
the absorption of the surfaces — the decay time. A small room therefore cannot
ring for ten seconds and a bare stone hall cannot be dead, and the decay is
frequency dependent the way real rooms are: surfaces absorb mids and highs more
than lows, and the air itself takes the top end off over the distance sound has
to travel before it dies, so a large space is longer *and* darker.

Two stages. **Early reflections** are eight discrete taps per channel at fixed
fractions of the room dimension; in the reference cave recording the strongest
of these arrives 55 ms after each drop only 7 dB below it, and it is the most
audible thing about the room — a diffuse tank can only smear it. The **late
tail** is an 8-line feedback delay network through an orthonormal Hadamard
matrix, each line with its own low shelf and air lowpass so that every line
decays at the same rate per second whatever its length, and four of the lines
slowly modulated by a fraction of a millisecond, which breaks up the metallic
modes an unmodulated tank rings with on a long decay and is far too little to be
heard as pitch.

### 6. Level behaviour

Total loudness is normalised against the *expected number of simultaneously
ringing droplets* (`rate × mean ring time`). Incoherent sources sum as `√N`, so
each droplet is scaled by `1/√N`. That makes `Density` a texture control rather
than a disguised volume control: sweeping it from a drizzle to a downpour
changes the character, not the level. Sparse settings are never scaled *up*, so
an isolated drip keeps its natural amplitude. A soft clipper above 0.8 sits at
the very end purely as a safety net.

### 7. Matched against recordings

The model and the preset library were both tuned against a set of reference
recordings: measure the recording and RainyDay's own output with the same code,
then move values until the two agree. `tools/analysis/` holds that machinery and
its README explains how to run it. Three things came out of it that listening
alone had not made obvious.

**Real rain has almost nothing below 200 Hz.** Measured against their own peak,
the recordings sit 30 to 50 dB down at 100 Hz. RainyDay was only 10 to 15 dB
down, and that single error accounted for most of the distance between it and
the real thing. Three parts of the model were wrong in the same direction:

- The noise bed was a lowpass, and a lowpass passes everything below its corner
  flat. It is now a band: a 12 dB/oct highpass tracks the lowpass corner about
  two and a half octaves below it.
- Droplets had no radiation rolloff at all. A droplet is a small source and
  radiates poorly below its own resonance -- radiated power falls as f² once the
  source is much smaller than the wavelength -- so each droplet now runs through
  a 12 dB/oct highpass at 0.45x its own pitch.
- The pitch spread was symmetric in octaves, throwing as many droplets two
  octaves down as up. Drop size is skewed heavily towards small and pitch goes
  as 1/radius, so the real distribution has a long tail upwards and a short one
  down. The spread is now lopsided to match, and the downward reach is less than
  half the upward one.

**A drop's pitch bend is not small — it is late.** This one took three passes
to get right, and the first two were confidently wrong.

Tracking the instantaneous frequency of isolated drops across the 80 ms or so
that a drop is loud puts the bend between 0.01 and 0.09 octaves, a couple of
per cent, and that measurement is correct. The conclusion drawn from it was
not. Weighting by energy means only the plateau is ever measured, and the
plateau is the one part of a drop that genuinely does not move: the reference
sits within 0.07 octaves of 750 Hz for as long as it is within 2 dB of peak.
Everything else happens afterwards. From 30 to 110 ms it climbs from 846 Hz to
2 kHz while falling from −9 to −36 dB — +1.47 octaves in a stretch of the sound
that no energy-weighted fit will ever look at.

So the bend was capped at a tenth of an octave for two releases, on the
strength of a number that was right about the wrong part of the drop.

The shape mattered as much as the size, and it is what made the error stick.
While the bend was front-loaded into the attack, a large span really did sound
like a laser, which looked like confirmation that the span had to stay small.
Back-loading it removes the constraint. What remains is an anchoring question
that is easy to get wrong in the other direction: spread the sweep across the
droplet's whole lifetime and only about a sixth of it lands before the drop is
inaudible, which sounds like no bend at all. It has to *finish* while the drop
can still be heard — at 0.6 ring times, the −36 dB point — and then hold.

**One droplet in seven was being flattened against a ceiling.** Droplet
amplitude is drawn as `u^k · (k+1)`, which has mean 1 and a natural maximum of
`k+1`. The guard clamping it at 2.0 therefore sat *below* that maximum: at the
default Level Spread it was not catching freak drops, it was truncating the top
of the intended distribution 14 % of the time.

**Rain is noise, not a chord.** The first fit drove `Tonality` up until the
denser presets buzzed — Gutter Trickle reached 92 %, which is 245 pitched
plinks a second inside a 1.7-octave band and sounds exactly like that. The
cause was in the measurement rather than the model: flatness was being computed
on the *average* spectrum, and a few hundred randomly pitched droplets average
out perfectly smooth. Measured per frame instead, where any 20 ms window holds
only a handful of droplets, the two are easy to tell apart, and the presets
that were buzzing stand out by an order of magnitude. Setting `Tonality` from
that measurement puts Steady Rain at 0.30 — its original hand-dialled value,
before fitting, was 0.28.

**The objective could not hear how long a drop rings.** `decay_ms` — the median
ring time of isolated impacts — was measured from the first version of the
feature set and then never used in the distance, so two presets with the same
spectrum scored identically whether their droplets rang for 30 ms or 300 ms.
Ring time turns out to be the feature that per-droplet measurement of the
references pins down most sharply, and adding it to the objective moved the
library's mean distance by more than any single parameter change had. It is
compared in the log domain, so the term means "out by this factor" rather than
"out by this many milliseconds".

**A drop's own tick has a pitch.** Measured per droplet rather than over the
whole file, the references' onset transients carry 7 dB more energy above 6 kHz
than RainyDay's did. The cause was that every droplet got the same broadband
noise burst. Modelling the impact as the paper does — a two-cycle damped sine
at a frequency drawn afresh for each droplet — supplies that energy and, more
importantly, supplies it differently for every drop.

None of these is audible as a defect on its own. Together they were the
difference between a filtered noise wash and rain.


## Parameters

### Rain

| Parameter | Range | What it does |
|---|---|---|
| Density | 0.2 – 5000 drops/s | Average droplet arrival rate |
| Clumping | 0 – 100 % | How much the rate itself fluctuates (surges and lulls) |
| Drop Pitch | 40 Hz – 9 kHz | Base resonant frequency of a droplet |
| Pitch Spread | 0 – 5 oct | Random octave spread on top of the physical size coupling |
| Drop Decay | 1 – 1200 ms | Base ring time |
| Decay Spread | 0 – 100 % | Randomisation of ring time |
| Tonality | 0 – 100 % | Noisy splat ↔ pitched plink |
| Bubble Chance | 0 – 100 % | Fraction of droplets that ring at all; the rest are splash and tick |
| Impact | 0 – 100 % | Weight of the pitched tick at the moment of impact |
| Splash | 0 – 100 % | Length and weight of the wet noise burst |
| Level Spread | 0 – 100 % | Drop-size distribution skew (and so amplitude, pitch and decay spread) |
| Chirp | −100 – +100 % | Per-droplet pitch sweep; positive rises, as bubbles in water do |
| Surface | 7 choices | Water, Puddle, Leaves, Wood, Metal, Glass, Concrete |
| Note Tracking | 0 – 100 % | How far the MIDI note transposes droplet pitch |

`Surface` biases ring time, resonator Q, click and splash weighting, chirp
depth and brightness together — it is a whole material model, not one filter
setting.

### Distant and Close

The near droplets and the far-field bed are two layers of the same rain, and
each is placed in the stereo field on its own. Width spreads a layer; pan slides
it without narrowing it.

| Parameter | Range | What it does |
|---|---|---|
| Bed Level | −inf – +6 dB | Level of the statistical far-field wash |
| Bed Tone | 0 – 100 % | Lowpass corner, dark to bright (level-compensated) |
| Bed Body | 0 – 100 % | Resonance at that corner |
| Bed Drift | 0 – 100 % | Slow intensity drift of both bed and density |
| Bed Width | 0 – 100 % | Stereo spread of the bed |
| Bed Pan | −100 – +100 % | Slides the bed across, at equal power |
| Drop Width | 0 – 100 % | Stereo spread of the close droplets |
| Drop Pan | −100 – +100 % | Slides the droplets across |

`Drop Width` is the parameter that used to be called `Stereo Width` and drove
both layers at once. It keeps its `width` key, so presets written before the
split still load; they simply gain a `bed_width` of their own.

### Space

| Parameter | Range | What it does |
|---|---|---|
| Distance | 0 – 100 % | Pushes the whole rain field away |
| Air Absorption | 0 – 100 % | How much high end distance costs |
| Space Amount | 0 – 100 % | Room mix: early reflections and late tail |
| Space Size | 0 – 100 % | Room dimension, 3 m to 90 m; decay time follows |
| Space Damping | 0 – 100 % | Surface absorption: bright stone ↔ soft and absorbent |

### Filter

State-variable filter across the whole output: `Filter Type`
(Lowpass/Bandpass/Highpass/Notch), `Filter Cutoff` (20 Hz – 20 kHz),
`Filter Resonance` and `Filter Key Track`. A wide-open lowpass is bypassed
outright, costing neither CPU nor colouration.

Alongside it, and independent of it, `Highpass` (20 Hz – 2 kHz) is a permanent
12 dB/oct rolloff on the output, bypassed at the far left. It does not replace
the multimode filter, which every preset already uses as its lowpass. Given how
much of this project turned out to be about rain having no low end, a highpass
that is always there earns its place next to one that is a tone control.

### Envelope

`Attack`, `Decay`, `Sustain`, `Release` gate the rain from MIDI. The envelope
scales droplet amplitude *and* partially scales density, so a long attack
sounds like rain moving in rather than a fade-in on a finished loop.
`Velocity to Level` and `Velocity to Density` route note velocity.

### System

- `Output Gain` — final level, and how the factory presets are loudness matched
- `Max Droplets` — hard ceiling on simultaneously ringing droplets (32 – 2048).
  This is the CPU dial. When the pool is full, new droplets are dropped rather
  than stealing an audible voice, so it never clicks.
- `Random Seed` — `0` means a fresh random seed per plugin instance, so stacked
  copies decorrelate, and it never repeats itself. Any other value renders
  identically every time: the sequence restarts whenever the host resets the
  plugin, so bouncing the same passage twice gives the same rain.

## Presets

| Preset | Character |
|---|---|
| Cave Drips | Sparse tonal drips in a deep reverberant cavern |
| Dripping Faucet | One near-regular tap, tight and close |
| First Drops | Scattered first drops on dry pavement |
| Light Drizzle | Fine high-frequency mist |
| Steady Rain | Everyday rain, the reference point |
| Concrete Alley | Hard flat urban surfaces |
| Rain On Leaves | Soft, dull forest canopy |
| Tin Roof | Bright ringing metal impacts |
| Window Pane | Close, hard, glassy ticks |
| Puddle Plinks | Fat drops landing in standing water |
| Gutter Trickle | Narrow, wet, gurgling downspout |
| Inside The Car | Muffled through a windscreen |
| Downpour | Heavy saturated rainfall |
| Storm Front | Violent gusting squall line |
| Tropical Monsoon | Dark, dense and relentless |
| Distant Rain Wall | A downpour heard from far away |

Nine of these were fitted numerically against a recording of the thing they are
imitating, and the rest had their tone corrected against the nearest one, using
`tools/analysis/`. Across the library that moved the mean distance to the
reference recordings down by about 18x, with the dense rain presets landing
within a few dB in every band. Fitting is not allowed to touch Surface, Chirp,
the space controls or the envelope: those are what make a preset itself rather
than a solution.

Every preset's Output Gain is then matched so the library plays at a consistent
level, targeting -22 dBFS RMS and backing off where that would push the peak
past -4 dBFS. Sparse presets are peak-limited by nature and end up quieter in
RMS terms, which is correct -- a dripping tap is not as loud as a downpour.

### Preset format

Presets are plain text, in real-world units, and are read both from disk and
from copies embedded in the plugin binary:

```ini
name = Steady Rain
description = Well-behaved everyday rain.
features = ambient, texture, noise

density = 700          # drops per second
drop_pitch = 900       # Hz
drop_decay = 30        # ms
surface = Water        # by name
tonality = 0.28        # 0..1 ratios
release = 2000         # ms
```

Unknown keys are ignored and missing keys keep their current value, so
hand-editing is safe. Drop your own `.rainyday` files into
`~/.config/RainyDay/presets/` and they are indexed as user content — the
directory is only declared to the host if it already exists, so create it
yourself first.

## Verification

The repo ships a small CLAP host used to test the plugin without a DAW. It
drives the real preset-discovery factory the way a host does.

```sh
./build/rainyday-render --selftest                  # 43 host-contract checks
./build/rainyday-render --list                      # walk preset discovery
./build/rainyday-render --preset downpour --out /tmp/rain.wav --seconds 10
./build/rainyday-render --all --outdir /tmp/rain     # render the whole library
./build/rainyday-render --preset tin_roof --param "Density=2500" \
                        --param "Random Seed=7" --out /tmp/x.wav
```

`--param` accepts parameter names or numeric ids, with values in display units
(`"Filter Cutoff=2.5k"`), routed through the plugin's own `text_to_value`.

The self-test covers parameter metadata, text round-tripping, state
save/load/restore equality, garbage-state rejection, parameter clamping, all
parameters at their extremes, odd block sizes, silence before the first note,
and activate/deactivate cycles.

`clap-validator` is the other useful check, but version 0.4.1 requires
rustc ≥ 1.95.

## Layout

```
src/plugin.cpp           CLAP entry, extensions, parameters, state, events
src/preset_provider.cpp  CLAP preset-discovery factory
src/preset.cpp           preset text parser, path resolution
src/params.cpp           the parameter table, its tips and unit conversions
src/gui/gui.cpp          the plugin window: X11, Cairo, layout, interaction
src/dsp/rain_engine.*    voices, droplet pool, scheduling, the noise bed
src/dsp/filters.h        state-variable and one-pole filters
src/dsp/reverb.h         delay line, allpass, the room (early reflections + 8-line tank)
src/dsp/rng.h            xoshiro128+, uniform/Gaussian/exponential draws
src/dsp/adsr.h           the envelope
src/dsp/fastmath.h       fast sine, decay coefficients
presets/*.rainyday       the factory library, also embedded at build time
tools/render.cpp         the offline verification host
tools/fithost.cpp        renders presets back to back for the fitting loop
tools/analysis/          measures recordings and renders, and fits presets to them
```

`!dev/` is not tracked. It holds local reference material used while working on
the plugin — rain recordings to fit the presets against, screenshots of other
plugins' interfaces — none of which is ours to redistribute. Point
`RAINYDAY_SOUNDS` at your own directory of recordings to run the analysis
tools; `tools/analysis/README.md` says what they need to be called.

## References

The droplet model follows the acoustics literature rather than being dialled in
by ear. The two that shaped it most:

- Liu, Cheng and Tong, *Physically-based Statistical Simulation of Rain Sound*,
  ACM TOG 38(4), 2019 — the two-mechanism raindrop model this engine's impact
  and bubble layers are taken from, and the observation that a bubble is not
  entrained on every impact.
- Minnaert, *On musical air-bubbles and the sounds of running water*, 1933 —
  the breathing frequency of a bubble, which is why droplet pitch goes as
  1/radius and ring time as radius.

Everything the recordings themselves settled — the size of the pitch bend, the
ring-time spread, the second bubble mode, how much energy the onset carries
above 6 kHz — is measured in `tools/analysis/`, not taken from either.

## Notes and limits

- Linux/x86-64, tested with GCC 13. State is stored little-endian.
- 16 voices; droplets live in one shared pool so held-note count cannot
  multiply the CPU cost without bound.
- Host parameter modulation (`CLAP_EVENT_PARAM_MOD`) is supported globally.
  Per-note modulation and note expressions are ignored.
- `Filter Key Track` follows the most recently played note, since the filter is
  a single global stage rather than per voice.
- Deliberately built without `-ffast-math`: on x86 GCC that links
  `crtfastmath.o`, which would flip FTZ/DAZ for the entire host process.
- No wind, no thunder — this is rain only.

## Versioning

Semantic versioning, `MAJOR.MINOR.PATCH`:

- **MAJOR** — an overhaul: a rewrite of the synthesis model, or a change that
  breaks existing presets or saved host state.
- **MINOR** — new features: a new parameter, a new layer, a new surface, new
  presets, anything that adds to what the plugin can do.
- **PATCH** — bug fixes and corrections that add nothing new.

The version is set in two places that must agree: `kPluginVersion` in
`src/rainyday.h`, which is what the host reports, and the `project()` line in
`CMakeLists.txt`.

## License

MIT. See `LICENSE`.

RainyDay contains no samples and no third-party code. The only external
dependency is the CLAP headers, which are MIT licensed, plus X11 and Cairo for
the plugin window.
