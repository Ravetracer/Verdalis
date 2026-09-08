---
tagline: Synthesised rain
subtitle: CLAP instrument for Linux and Windows
accent: #58B6E8
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that generates rain. It contains no samples and
loads no audio files: every droplet, every splash and the whole background wash
are computed from noise, oscillators and filters while the plugin plays. Two
instances never produce the same rain, and one instance never repeats itself.

That is the point of it. Recorded rain arrives with its distance, its impact
surface and its room already fixed in the file, and however long the recording
is, the ear eventually finds the place where it comes round again. A synthesised
rain field has no loop point to find, and the things a recording has baked in
are separate controls: what the rain is falling on, how far away it is, how much
high end the air between you and it has taken, and the space the whole field
sits in.

The engine is not a noise generator with filters on it. Droplets arrive as a
statistical process, each one is given a size, and its pitch, its level and its
ring time all follow from that size the way they follow in physics. The
individually audible drops are synthesised one at a time; the ones too distant
to resolve are summed statistically into a bed, because synthesising ten
thousand inaudible drops a second would cost a great deal and sound the same.
*How it works* describes the whole chain.

The parameter values and the factory presets were not dialled in by ear alone.
Both were fitted against reference recordings by measuring the recording and the
plugin's own output with the same analysis code and moving values until the two
agreed. Where a number in this manual comes from a measurement or a paper, it
says so.

## What is in this manual

*Installing* and *A first sound* are enough to get rain out of the plugin.
*How it works* explains the model, which is worth reading before spending time
in the parameter list — most of the controls make immediate sense once the model
does. *Parameter reference* and *The preset library* are generated from the
plugin itself, so they always describe the version on the cover. *Using it in a
host* covers notes, automation, reproducible renders and CPU cost.

# Installing

{{PLUGIN}} is one self-contained `.clap` file plus its `presets` folder. There
is no installer, no shared library to place and nothing to register.

## Linux

CLAP hosts scan `~/.clap`. Copy the plugin folder from the release archive into
it so that you end up with:

```
~/.clap/{{PLUGIN}}/{{PLUGIN}}.clap
~/.clap/{{PLUGIN}}/presets/
```

Then rescan plugins in your host. Nothing else is needed: X11 and Cairo are the
only external dependencies of the plugin window, and any Linux machine that can
run a DAW already has both.

## Windows

CLAP hosts scan the common CLAP folder. Copy the plugin folder into:

```
C:\Program Files\Common Files\CLAP\{{PLUGIN}}\
```

so that `{{PLUGIN}}.clap` and `presets\` sit inside it, and rescan. There are no
DLLs to install beside it — the window and its Cairo are linked in.

> **Keep the plugin and its presets together.** The plugin finds its factory
> presets by looking for a `presets` directory next to its own binary. Moving
> the `.clap` file on its own leaves it with no factory presets at all.

## Hosts

Tested with Bitwig Studio and Reaper. In Bitwig, `~/.clap` is scanned by
default; after installing, restart it or rescan under *Settings → Locations →
Plug-in Locations*. {{PLUGIN}} then appears as an instrument under the vendor
*Ravetracer*, and the factory presets are indexed through CLAP's preset
discovery, so they show up in the host's own browser as well as in the plugin
window.

## Your own presets

`SAVE` in the plugin window writes to a user preset folder, which is created on
first use:

| Platform | Location |
|---|---|
| Linux | `$XDG_CONFIG_HOME/{{PLUGIN}}/presets`, or `~/.config/{{PLUGIN}}/presets` |
| Windows | `%APPDATA%\{{PLUGIN}}\presets` |

User presets appear in the browser immediately, listed after the factory ones.
They are plain text, so they can be copied between machines, kept in version
control, or edited in any editor.

## Building from source

The suite builds with a C++17 compiler and CMake 3.16 or newer. From the plugin
folder:

```sh
./install.sh
```

That configures, builds, runs the self-test and installs to `~/.clap/{{PLUGIN}}/`.
`{{PLUGIN}}_PREFIX=/some/where ./install.sh` overrides the destination. The
CLAP headers are found automatically in a few usual places, including a `CLAP/`
checkout beside the plugin folder; `-DCLAP_INCLUDE_DIR=/path/to/clap/include`
points the build at one kept elsewhere.

# A first sound

1. Load {{PLUGIN}} on an instrument track.
2. Draw a long note — four bars or more. Rain plays for as long as the note is
   held, and the release fades it out.
3. Open the preset browser by clicking the preset name in the window header,
   and try **Steady Rain**.

That is the whole workflow. Everything else is shaping.

## Three things to try first

**Change the surface.** *Surface* in the *Rain* panel is the single most audible
control in the plugin. The same droplet statistics over Water, Leaves, Metal and
Fabric are four different recordings. It biases decay, resonance, the impact
click and how far a droplet's pitch bends.

**Move it away from you.** Raise *Distance*. The rain does not merely get
quieter: the air takes the top end off it, the individually audible drops recede
and the far-field bed becomes most of what is left. Add *Space Amount* and it is
rain heard from inside a building rather than rain in the open.

**Sweep the density.** *Density* runs from a drip to five thousand drops a
second, and it is level-compensated: it changes the texture, not the volume.
Sweep it with *Clumping* up and the rain gains surges and lulls instead of an
even patter.

# How it works

Rain is not one sound. It is a very large number of small independent impacts,
plus the collective wash they add up to. {{PLUGIN}} models exactly that.

## Droplet arrival

Droplets are scheduled as a **Poisson point process**: the waiting time until
the next impact is drawn as `-ln(U)/rate`, the exact inter-arrival distribution
for events happening independently at a constant average rate. That is what
makes the result sound organic rather than like a machine gun with jitter added.

*Clumping* turns the rate into a random variable of its own — a *Cox process*,
or doubly stochastic Poisson process — modulated by a band-limited Gaussian
random walk. The modulation is mean-compensated by `exp(-σ²/2)`, so the average
density stays exactly where it is set while the rain gains natural surges and
lulls.

## A single droplet

Each impact is a short event assembled from five layers:

| Layer | Model | Controlled by |
|---|---|---|
| Bubble | Phase-accumulated sine, a difference of two exponentials for its amplitude, plus a per-droplet pitch sweep | *Tonality*, *Bubble Chance*, *Chirp*, *Drop Decay* |
| Second mode | A quieter partial near twice the bubble frequency, decaying twice as fast | *Surface* |
| Wet | White-noise burst through a resonant state-variable bandpass tuned to the droplet's pitch | *Splash*, *Tonality* |
| Impact | A two-cycle damped sine at a frequency drawn afresh for every droplet | *Impact* |
| Body | One low mode of the struck surface, at the surface's own frequency and ring time | *Impact*, *Surface* |

Bubble, second mode and splash pass through the droplet's own radiation
highpass; the impact and the body do not, because they are the surface being
struck rather than the droplet radiating. Everything then passes through a
one-pole lowpass standing in for air absorption, and is equal-power panned into
the stereo field.

### The surface has a voice of its own

A droplet cannot put energy far below its own resonance, but the thing it lands
on can, and every recording of rain on something has more in the low mids than a
cloud of droplets radiating into air can produce. The fitted library was short
by about 2 dB between 200 and 400 Hz on thirteen presets out of sixteen however
each was pointed, which is an engine's bias rather than a preset's.

So each impact also excites one low mode of the surface — 240 Hz and 40 ms for a
canopy, 320 Hz and 150 ms for a tin roof, nothing at all for water — scattered a
little per droplet, because a roof is not one panel. It is scaled by *Impact*,
since it is the strike that sets it going, and weighted as an *energy* ratio
against the click rather than an amplitude one, because a mode ringing for forty
milliseconds carries far more energy than a two-cycle tick of the same height.

### The impact is pitched, not noise

Following Liu, Cheng and Tong (2019), the initial impact is modelled as
`A·e^(−2f·t)·sin(2πf·t)`, with `f` drawn uniformly between 1 and 16 kHz for each
droplet and scaled by the surface's brightness. Damping at twice the frequency
leaves about two cycles, so a single drop is a tick with a pitch of its own —
0.2 ms at the top of the range, 3.5 ms at the bottom. One drop sounds like a
tick; a thousand a second are broadband, and the constant redrawing is what
gives dense rain its shimmer. A fixed noise burst instead gives every drop in
the field an identical transient, and measures several dB short of a real
recording above 6 kHz.

### Not every drop rings

Pumphrey and Elmore's measurements have only a band of drop sizes entraining an
air bubble on impact; the rest of the rain is splash and tick with no pitch at
all. *Bubble Chance* is that fraction, drawn per droplet, and it is not the same
control as *Tonality*: tonality at 50 % makes every drop half-pitched, which is
a uniform mush, while *Bubble Chance* at 50 % makes half the drops plink clearly
and leaves the other half dry.

### There is a second bubble mode

Measuring the isolated drops in the reference recordings finds a partial at 1.8
to 2.15 times the fundamental, 15 to 25 dB below it, on essentially every drop
that lands in water: a bubble pulsating hard enough to be heard also radiates at
twice its breathing frequency. Its ratio and level are redrawn per droplet, it
bends with the fundamental because it is a mode of the same bubble, and it decays
twice as fast in dB. Surfaces that trap no bubble do not get it.

### The chirp is real physics

A droplet hitting water entrains an air bubble whose resonant frequency **rises**
as the bubble shrinks, which is why a drip into a puddle goes "plink" with an
upward bend rather than a flat tone. Two details of that decide whether it is
audible.

**The bend is large, late, and over before the drop is.** Tracked cycle by cycle
from the zero crossings of an isolated drop, the reference dips from 775 to
728 Hz across the first four milliseconds, sits on a plateau near 750 Hz while it
is within 2 dB of peak, then rises from 846 Hz to 2 kHz between 30 and 110 ms —
by which point it is 36 dB down. That is +1.47 octaves in total, and almost none
of it happens while the drop is loud. So the model is in two parts: a fast
downward dip that relaxes within a cycle or two of the attack, and a rise whose
per-sample step grows geometrically. The sweep finishes at 0.6 ring times and
then holds, which is the −36 dB point, where the reference has finished too.

**The tone arrives behind the splash.** The impact happens first and the bubble
is entrained afterwards, so the tonal layer's envelope is
`e^(-t/decay) − e^(-t/rise)` rather than a decay from full level. That gives it a
short swell instead of a hard onset, and how long the swell takes is a property
of the surface: water and puddles trap bubbles, metal and glass ring on contact.

Only surfaces that trap a bubble get a large chirp span — Water and Puddle carry
1.30 and 1.45 octaves, while hard surfaces stay near a hundredth of an octave,
because a drop landing on something rigid excites a fixed mode of that thing.

## Droplet size follows from physics

Rather than randomising amplitude and pitch independently, {{PLUGIN}} draws a
single **size** per droplet from a Marshall–Palmer-like skewed distribution —
many small drops, few large ones, with *Level Spread* setting the skew — and
derives everything else from it:

- amplitude scales with volume, and so with radius cubed
- resonant pitch scales with `1/radius`: big drops plop low, fine drops tick high
- ring time scales with radius: big drops ring longer

A fat drop is therefore automatically loud, low and long, and a fine one quiet,
high and short, with no parameter tweaking required for that to hold. The
relations are Minnaert's, from 1933.

## The bed

Individually inaudible far-field droplets are not synthesised one by one. They
are summed statistically into a **noise bed**: two decorrelated white sources
mixed to the requested width — `a·n₁ + b·n₂` against `a·n₁ − b·n₂` with
`a² + b² = 1`, so the channel correlation is `cos(width·π/2)` with no level
change — shaped by a resonant lowpass and a highpass, and modulated by a slow
random walk set by *Bed Drift*.

The bed is a band rather than a lowpass, which matters more than it sounds:
measured against their own peak, real recordings sit 30 to 50 dB down at 100 Hz,
and a lowpass passes everything below its corner flat.

## Space and distance

*Distance* attenuates and darkens, with *Air Absorption* setting how quickly the
high end is lost — per droplet, so near drops stay bright while far ones go dull.

*Space* is a room model driven by one physical quantity. *Space Size* is the
dimension of the room, 3 m to 90 m, and everything else follows from it: the
distances the first reflections travel, the mean free path the late tank's delay
lengths are built on, and — through Sabine's law, with *Space Damping* as the
absorption of the surfaces — the decay time. A small room therefore cannot ring
for ten seconds and a bare stone hall cannot be dead. The decay is frequency
dependent the way real rooms are, so a large space is longer *and* darker.

There are two stages. **Early reflections** are eight discrete taps per channel
at fixed fractions of the room dimension; in the reference cave recording the
strongest of these arrives 55 ms after each drop only 7 dB below it, and it is
the most audible thing about the room. The **late tail** is an eight-line
feedback delay network through an orthonormal Hadamard matrix, each line with its
own low shelf and air lowpass so that every line decays at the same rate per
second whatever its length, with four of the lines slowly modulated by a fraction
of a millisecond to break up the metallic modes an unmodulated tank rings with.

## Level behaviour

Total loudness is normalised against the *expected number of simultaneously
ringing droplets*, which is the rate times the mean ring time. Incoherent sources
sum as `√N`, so each droplet is scaled by `1/√N`. That is what makes *Density* a
texture control rather than a disguised volume control: sweeping it from a
drizzle to a downpour changes the character, not the level. Sparse settings are
never scaled *up*, so an isolated drip keeps its natural amplitude. A soft
clipper above 0.8 sits at the very end purely as a safety net.

# The plugin window

{{PLUGIN}} draws its own window with X11 and Cairo on Linux, and Cairo on the
win32 backend on Windows. There is no toolkit dependency, which is what keeps the
plugin one self-contained file. It is embedded in the host's own window through
CLAP's GUI extension and repainted from the host's timer.

The layout is 960 × 740 in design pixels, and the window resizes by zooming: the
host is asked to keep the aspect ratio, and whatever size it settles on becomes
one Cairo scale over the same layout, from half size to four times. The panels
are the parameter modules and the cells are the parameters, generated from the
same table this manual's parameter reference comes from.

The header carries the wordmark, an animated ornament whose density follows how
many droplets are sounding, the preset name with its browser, and the activity
meter.

## Gestures

| Gesture | Effect |
|---|---|
| Drag a knob up or down | Change the value |
| Shift-drag | Fine control, a fifth of the travel |
| Double-click a knob, or right-click anything | Back to the default |
| Scroll wheel over a control | Step the value |
| Click a knob's value | Type a value in, with units |
| Click a selector's name | Open its list and pick a value |
| Click a selector's ◀ or ▶ | Step one choice |
| Click the preset name | Open the preset browser |
| ◀ or ▶ beside the preset name | Previous or next preset |
| `SAVE` | Save the current settings as a user preset |

The line at the bottom of the window shows what the control under the pointer
does — the same one-line explanation that appears in the *What it does* column of
the parameter reference.

## Saving a preset

`SAVE` opens a name field and writes the current parameter values to the user
preset folder, creating it if it is not there. The name becomes the filename with
awkward characters replaced, so *My Rain / 2* is saved as `My_Rain_2.rainyday`,
and the browser picks it up immediately.

# Parameter reference

<div class="paramref" markdown="1">

{{PARAMETER_REFERENCE}}

</div>

# The preset library

{{PRESET_LIBRARY}}

# Using it in a host

## Notes

A note starts rain and holding it keeps the rain going; the ADSR shapes the
whole field rather than individual droplets, and *Release* is what fades a
downpour out. *Note Tracking* decides how far the played note transposes droplet
pitch, from none at all — so that any key gives the same rain — to a full
transposition, which turns the keyboard into a size control.

*Velocity to Level* and *Velocity to Density* route velocity, so a soft note can
be a lighter shower rather than only a quieter one.

There are 16 voices. Droplets live in one shared pool, so the number of held
notes cannot multiply the CPU cost without bound.

## Automation and modulation

Every parameter is automatable, and host parameter modulation
(`CLAP_EVENT_PARAM_MOD`) is supported globally. Per-note modulation and note
expressions are ignored: the engine is one global rain field rather than one per
voice, which is also why *Filter Key Track* follows the most recently played
note.

Parameter changes are sample accurate, so an automation ramp over a bar is a
smooth move and not a staircase at block boundaries.

## Reproducible renders

The synthesis is stochastic, so the same project rendered twice does not produce
identical audio — which is usually the point, but not always.

*Random Seed* controls it. At 0 the rain never repeats. At any other value, the
same notes render the same rain every time, whatever the plugin was doing before
them. Set a non-zero seed before bouncing anything that has to match a previous
render, and set it back to 0 afterwards if you want the variation back.

## CPU cost

Cost is bounded by design. *Max Droplets* caps the pool, and the level
normalisation means a high *Density* spends its budget on more, quieter droplets
rather than running away. The far-field bed costs the same whatever its level,
because it is two noise sources and two filters however much rain it stands for.
*Space* is the most expensive single block: an eight-line feedback delay network
plus eight early taps per channel.

If a project is tight on CPU, the order worth cutting in is *Space Amount*, then
*Max Droplets*, then *Density*.

## Rendering and tails

Rain has a tail: the room, the bed and the last droplets keep sounding after the
note is released. When bouncing, leave a few seconds past the end of the note, or
the tail is cut off mid-decay.

# Reference and limits

## Where the model comes from

The droplet model follows the acoustics literature rather than being dialled in
by ear:

- Liu, Cheng and Tong, *Physically-based Statistical Simulation of Rain Sound*,
  ACM TOG 38(4), 2019 — the two-mechanism raindrop model this engine's impact
  and bubble layers are taken from, and the observation that a bubble is not
  entrained on every impact.
- Minnaert, *On musical air-bubbles and the sounds of running water*, 1933 — the
  breathing frequency of a bubble, which is why droplet pitch goes as `1/radius`
  and ring time as radius.
- Marshall and Palmer, *The distribution of raindrops with size*, 1948 — the
  shape of the drop-size distribution *Level Spread* skews.

Everything the recordings themselves settled — the size of the pitch bend, the
ring-time spread, the second bubble mode, how much energy the onset carries above
6 kHz — was measured rather than taken from either.

## Notes and limits

- Linux and Windows, x86-64. Plugin state is stored little-endian.
- 16 voices; droplets live in one shared pool.
- Host parameter modulation is supported globally. Per-note modulation and note
  expressions are ignored.
- *Filter Key Track* follows the most recently played note, because the filter is
  a single global stage rather than one per voice.
- Deliberately built without `-ffast-math`: on x86 GCC that links
  `crtfastmath.o`, which would flip the FTZ and DAZ flags for the entire host
  process.
- No wind and no thunder — this is rain only. ThunderClap does the thunder.

## Versioning

Semantic versioning. **MAJOR** is an overhaul: a rewrite of the synthesis model,
or a change that breaks existing presets or saved host state. **MINOR** adds
something — a parameter, a layer, a surface, presets. **PATCH** is fixes that add
nothing new.

## License

MIT. {{PLUGIN}} contains no samples and no third-party code. The only external
dependencies are the CLAP headers, which are MIT licensed, plus X11 and Cairo for
the plugin window.

Reference recordings used during development are not part of the release and are
not redistributable.
