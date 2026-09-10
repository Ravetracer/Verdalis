---
tagline: Synthesised ocean surf
subtitle: CLAP instrument for Linux and Windows
accent: #4FD0BA
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that generates ocean surf. It contains no
samples: every break, every sheet of foam, every bubble and the swell underneath
are computed from noise, filters and resonators while the plugin plays. No two
shores are ever alike.

A breaking wave is not one sound but four, and they overlap. The crest collapses
into a cloud of bubbles. It leaves a sheet of foam behind, which has no low end
at all and outlives the break by up to three times. The water washes back down
the shore and drains through whatever the shore is made of. And single bubbles
pop in the foam long after the wave itself has gone. Under all of it is the swell
bed: water moving without breaking, slowly breathing.

{{PLUGIN}} generates all five, and lets them interact rather than merely
overlap — an arriving wave bursts the foam the last one left, because a beach
never has two sheets of foam hanging in the air at once.

Every layer came out of measurement. The model and the factory presets were
fitted against 55 field recordings — 48 kHz stereo, 114 minutes, with anything
containing birds or aircraft excluded, since this plugin models waves only — by
measuring the recording and the plugin's own output with the same analysis code
and moving values until the two agreed. Where a number in this manual comes from
a measurement or a paper, it says so.

> **{{PLUGIN}} {{VERSION}} is an early version.** The engine, the parameter set
> and the window are complete and the self-test passes, but the preset library is
> a first fit. *What is still being fitted* at the end of this manual lists what
> is known not to match yet, and it is worth reading before concluding that
> something is broken.

## What is in this manual

*Installing* and *A first sound* are enough to get surf out of the plugin.
*How it works* explains the model, and is worth reading before spending time in
the parameter list — the layers are what the controls are named after.
*Parameter reference* and *The preset library* are generated from the plugin
itself, so they always describe the version on the cover. *Using it in a host*
covers notes, automation, reproducible renders and CPU cost.

# Installing

{{PLUGIN}} is one self-contained `.clap` file plus its `presets` folder. There is
no installer, no shared library to place and nothing to register.

## Linux

CLAP hosts scan `~/.clap`. Copy the plugin folder from the release archive into
it so that you end up with:

```
~/.clap/{{PLUGIN}}/{{PLUGIN}}.clap
~/.clap/{{PLUGIN}}/presets/
```

Then rescan plugins in your host. X11 and Cairo are the only external
dependencies of the plugin window, and any Linux machine that can run a DAW has
both.

## Windows

CLAP hosts scan the common CLAP folder. Copy the plugin folder into:

```
C:\Program Files\Common Files\CLAP\{{PLUGIN}}\
```

so that `{{PLUGIN}}.clap` and `presets\` sit inside it, and rescan. There are no
DLLs to install beside it — the window and its Cairo are linked in.

> **Keep the plugin and its presets together.** The plugin finds its factory
> presets by looking for a `presets` directory next to its own binary. Moving the
> `.clap` file on its own leaves it with no factory presets at all.

## Hosts

Tested with Bitwig Studio and Reaper. In Bitwig, `~/.clap` is scanned by default;
after installing, restart it or rescan under *Settings → Locations → Plug-in
Locations*. {{PLUGIN}} then appears as an instrument under the vendor
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

That configures, builds, runs the self-test and installs to
`~/.clap/{{PLUGIN}}/`. `{{PLUGIN}}_PREFIX=/some/where ./install.sh` overrides the
destination, and `-DCLAP_INCLUDE_DIR=/path/to/clap/include` points the build at a
CLAP checkout it cannot find by itself.

# A first sound

1. Load {{PLUGIN}} on an instrument track.
2. Draw a long note — eight bars or more. Waves keep breaking for as long as the
   note is held, at *Wave Period* apart, and the release fades the sea out.
3. Open the preset browser by clicking the preset name in the window header and
   try **Gentle Waves**, then **Distant Roar**, then **Uproar Waves**.

Surf needs length. A two-second note is one break; the character of a shore is
in the rhythm of several, so give it time.

## Three things to try first

**Change the shore.** *Shore* sets what the water is draining through — sand,
shingle, pebbles, rock, reef or harbour wall. It changes how much the wash
rattles, and it also sets how enclosed the space is: 0.04 for open sand against
0.95 for a harbour wall with something right there to bounce off.

**Change the breaker type.** *Breaker* is Galvin's classification — spilling,
plunging, collapsing, surging. It is not a cosmetic switch: the four measurably
differ in spectral slope above 1.5 kHz, and the parameter changes the slope, the
attack, the level, the foam share and the body weight together.

**Move the shore away.** Raise *Distance*. Air absorption takes the top off
everything, the individual breaks smear into each other, and the swell bed
becomes most of what is left. This is the best-fitting part of the model: the
distant presets track their references within about a decibel from 800 Hz up.

# How it works

## The four layers, and the bed

| Layer | What it is |
|---|---|
| **Break** | The crest collapsing into a cloud of bubbles. A cascade of discrete bubble events clustered around a kilohertz, with a noise band around them, opening upward as the cloud grows. It rises over a quarter to a whole second rather than striking. |
| **Foam** | The sheet of small bubbles left behind. Highpassed hard — the references measure −82 dB at 50 Hz, so it has no low end at all. It outlives the break by up to three times, which is why the quiet stretches between waves measure *brighter* than the waves. |
| **Wash** | Water running up the shore and draining back through whatever the shore is made of. A mid band with a slow random walk on it; coarser shores rattle more. |
| **Bubbles** | Individual resonators popping in the foam, each ringing at the pitch its radius gives it. |
| **Swell bed** | Water moving without breaking, slowly breathing. At distance it is most of what is left, because air absorption has taken the top off everything else. |

## A wave in three stages

The shore breaks, some bubbles are heard, and after a while the foam starts
bursting as a sizzle. The engine does that in order:

1. **The break**, deep coming in and opening up as it collapses.
2. **Bubbles**, a trickle at the break's tail and more in the foam that follows,
   individually audible at around a kilohertz.
3. **The sizzle**, arriving later again and outlasting everything: thousands of
   sub-millimetre bubbles bursting at once. At 0.25 to 0.6 mm they ring between 5
   and 13 kHz for a couple of milliseconds, and at that rate they overlap several
   deep, so they are not separately audible — they merge. It is generated as a
   high band with a granular envelope rather than as thousands of oscillators,
   which is the same sum for one multiply a sample.

## The break is a cascade, not a sweep

The first version of the engine built a break by sweeping a bandpass over noise,
which sounds like a slowed-down whip crack rather than a wave. The references say
a break is a cascade of discrete bubble events — 15 to 30 separately audible
onsets a second, clustered around 1 kHz — so the engine builds the break from
bubbles and uses the noise band only as what surrounds them. *Bubble Mix* is the
balance between the two.

The rate matters more than anything else about it. At the references' 27 onsets a
second with a 40 ms ring, about one bubble sounds at a time; push the rate up and
they overlap back into noise, which is how a bubble layer can be present and
still sound like a whoosh.

## A wave comes in deep and brightens as it breaks

The band opens *upward* into the break rather than sweeping down out of it, which
is what it does in the references: the balance of high to low rises by 1 to 7 dB
at the break across most of the library. *Crest Open* is how much darker the
approach is. The precursor — the crest of an incipient plunger bubbling slightly
before it collapses — is the low sound of water arriving, not of bubbles.

## Bubbles are oscillators, not filtered noise

A bubble is generated as a decaying sinusoid with a short pinch-off transient,
which is the impulse response of a damped harmonic oscillator. Its damping is
computed from the physics rather than set by hand: the radiative loss is a
constant 0.01368 for every size and the thermal term goes as `√f`, which gives Q
from 20 to 46 and ring times of 2 to 124 ms across the size range, against 5 to
100 ms measured. *Bubble Damping* multiplies that, 1 being physical.

Bubble pitch follows Minnaert's relation, `f₀ ≈ 3.26 / r` with the radius in
metres, so *Bubble Pitch* is a physical quantity: 2.6 kHz is a bubble about a
millimetre across.

The foam runs a **second, finer cascade**. The slowed references measure the foam
fizzle at 2.2 kHz and 29 onsets a second against the break's 850 Hz and 9, so
foam bubbles are smaller, higher and faster than the ones a break makes.
*Foam Bubbles* sets how much of the foam is that cascade rather than hiss.

Bubbles are a detail, not the main event: they are quiet, they sit behind the
break rather than on it, and several presets have none at all, because distant
surf has no separately audible bubbles.

*Break Body* drives a resonator at the cloud's collective mode, `bubblePitch`
over the cube root of the number of bubbles, which is where surf rumble
physically comes from. Collective oscillations of a bubble plume sit below
400 Hz and individual bubbles above 1 kHz, so the engine splits them: the body
filter is capped at 400 Hz and the bubbles are their own layer.

## Breaker type and size

*Breaker* is Galvin's classification, and the four types differ in ways that were
measured rather than chosen:

- The slope above 1.5 kHz is about −6 dB/octave, steepening momentarily to −10 in
  the first second of breaking and relaxing to −5 or −6 afterwards. Means and
  Heitmeyer measure −10 dB/oct for plungers against −8.3 for spillers. That is
  what *Breaker* sets, and why the slope is animated rather than fixed: one pole
  is −6 dB/oct, two are −12, and the mix between them is the slope.
- Bigger breakers decay *more slowly* — −7 dB/s at 1.6 to 2.0 m against −4.5 dB/s
  at 2.4 to 2.7 m — so *Wave Size* scales *Break Decay* as well as level.
- The rise is faster than the decay, for the same reason it is in the recordings.

## A wave washes the last one's foam away

Each wave used to own a private foam timer that ignored the sea around it, so
with a long *Foam Delay* a wave's foam could still be hanging in the air when the
next one broke — the one thing a beach never does. A breaking wave now runs over
whatever foam is already lying there: what has not sizzled yet is burst by the
arriving water within a few tens of milliseconds rather than waiting out its
delay, and what is already sizzling is carried back out and fades early.

Measured with *Foam Delay* at 2.5 s and waves every 1.2 s, so that every wave's
foam is still pending when the next breaks, the sizzle fires 0.06 s after each
break instead of at its 2.5 s delay.

## An open shore has no walls

Eight discrete early reflections at fixed fractions of a room dimension is what
makes a reverb sound like a bathroom, and no amount of tail will talk the ear out
of it. Outdoors there is nothing close enough to reflect. So the shore type scales
the early field — 0.04 for open sand, 0.95 for a harbour wall — and the room
model behind it is the suite's: eight early reflections and an eight-line
feedback delay network built from one physical size, with Sabine's law for the
decay time.

## Distance

*Distance* applies air absorption plus a downward tilt, and it also multiplies
the individual break events and smears their edges, because what arrives from a
kilometre away has been through a kilometre of scattering. This is the
best-fitting part of the model: *Distant Roar* tracks its reference within about
a decibel from 800 Hz up.

# The plugin window

{{PLUGIN}} draws its own window with X11 and Cairo on Linux, and Cairo on the
win32 backend on Windows. There is no toolkit dependency, which is what keeps the
plugin one self-contained file. It is embedded in the host's own window through
CLAP's GUI extension and repainted from the host's timer, and it resizes by
zooming one layout, from half size to four times.

The window is the suite's, in {{PLUGIN}}'s own sea-green theme, with a surf line
running through the header behind the wordmark and a wave-activity meter beside
the preset name.

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
| `MIXER` | Open the layer mixer |

The line at the bottom of the window shows what the control under the pointer
does — the same one-line explanation that appears in the *What it does* column of
the parameter reference.

## The mixer

Every layer's level lives on the panel that layer belongs to, which is right for
editing one layer and wrong for balancing them against each other. `MIXER` opens
the same controls arranged the other way round: one strip per layer, its level as
a fader, its pan and its width as slim sliders under it. They are the plugin's own
parameters, not a second set — moving a fader moves the knob on the panel, and the
host sees the same automation it always would.

`M` mutes a layer and `S` solos one, which is how a preset gets built from
nothing: solo one layer, get it right, bring the next one back. Several layers can
be soloed at once.

Mute and solo are **not** parameters and are not saved with a preset. They work by
holding a layer's level at the bottom of its range and remembering what it was, so
while either is active the preset bar shows a `SOLO ON` or `MUTE ON` chip — click
it to release every hold at once. `SAVE` releases them first, so a muted layer can
never be written into a preset as a silent one, and so does loading a preset.
Touching a held layer's fader also releases the holds rather than fighting the
hand on it. Closing the mixer does not: soloing a layer and then going to its
knobs on the panels is what the mixer is for.

# Parameter reference

<div class="paramref" markdown="1">

{{PARAMETER_REFERENCE}}

</div>

# The preset library

{{PRESET_LIBRARY}}

# Using it in a host

## Notes

A note starts the sea and holding it keeps the waves coming, *Wave Period* apart,
with *Set Variation* deciding how unevenly they arrive — metronomic at 0, grouped
into sets at high values. The ADSR shapes the whole shore rather than individual
waves, and *Release* is what fades it out.

Surf needs long notes. Most of the character of a shore is in the rhythm of
successive breaks and in the foam that outlives them, neither of which is audible
in a short note.

There are 16 voices, with 512 waves and 1024 bubbles in shared pools, so the
number of held notes cannot multiply the CPU cost without bound.

## Automation and modulation

Every parameter is automatable, and host parameter modulation
(`CLAP_EVENT_PARAM_MOD`) is supported globally. Per-note modulation and note
expressions are ignored. Parameter changes are sample accurate, so an automation
ramp over a bar — a tide coming in over *Wave Size* and *Distance* — is a smooth
move rather than a staircase at block boundaries.

## Reproducible renders

*Random Seed* at 0 never repeats. Any other value renders the same sea for the
same notes every time. The initial swell phase and the first wave timer are
derived from the voice's slot and key rather than from the shared generator, so a
fixed seed promises the same sea even when note events arrive before the seed has
been applied.

## CPU cost

Cost is bounded by the pools. The most expensive controls are *Bubble Rate* and
*Foam Bubbles*, which decide how many resonators are running, and *Space Amount*,
which is the room model. The sizzle is cheap by construction: it is a filtered
band with a granular envelope rather than thousands of oscillators.

## Rendering and tails

Surf has a long tail — the foam, the sizzle and the room outlive the last break
by seconds. When bouncing, leave several seconds past the end of the note, or the
sizzle is cut off mid-decay.

# What is still being fitted

{{PLUGIN}} {{VERSION}} is a first fit, and the measurements say where it is not
there yet. All of this is in the plugin's `TODO.md` with what is known about it.

**Crest factor is 3 to 9 dB below the references.** Real surf has more silence
between breaks than the synthesis leaves — 34 dB for *Sand and Foam* against
23.7, 29 dB for *Gentle Waves* against 20.8. The suspects, in order, are too many
waves alive at once, a break envelope whose decay is one exponential where the
references suggest two stages, and a swell bed filling gaps that should be quiet.

**The steadiest sources are still somewhat eventful.** *Distant Roar* measures an
envelope variation of 0.32 against the reference's 0.10, having been 0.53 before
distance began multiplying the events. At distance the individual breaks should
smear into the roar; they still punch through a little.

**Twelve of the seventeen presets have a single audible band 6 to 11 dB out**,
down from fifteen with deviations up to 37 dB. *Rhythmic Tide* is darker than its
reference throughout, and *Harbour Lapping* is 12 dB too bright at 12.5 kHz — a
harbour is a boxy, close, low sound, and the space may need to do more of the
work.

**Several presets are grainier than their references.** Erring towards more
bubbling was deliberate after the first version was too smooth, and *Bubble Mix*
dials it back, but the measurement cannot tell the difference between convincing
bubbling and too much of it, so this one has to be settled by ear.

# Reference and limits

## Where the model comes from

- Klusek, Z. and Lisimenka, A., *Acoustic noise generation under plunging
  breaking waves*, Oceanologia 55(4), 2013 — measurements under 1.6–2.8 m
  plungers, and the source of the spectral maximum, the slope, the decay rates
  and the precursor.
- Galvin, C. J., breaker type classification — spilling, plunging, collapsing and
  surging, which is what *Breaker* selects.
- Means, S. and Heitmeyer, R., on the spectral slope of plungers against
  spillers.
- Schindall, J. and Heitmeyer, R., 1996 — collective plume oscillations below
  400 Hz against individual bubbles above 1 kHz, which is why the body filter is
  capped where it is.
- Xue et al., on bubble damping and on the collective modes a bubble cloud
  forces.
- Minnaert, M., *On musical air-bubbles and the sounds of running water*, 1933 —
  `f₀ ≈ 3.26 / r`, which makes *Bubble Pitch* a radius.

The papers themselves are not part of the release; they are not ours to
redistribute. `tools/analysis/README.md` in the source tree records every
measurement the library was fitted to.

## Notes and limits

- Linux and Windows, x86-64. Plugin state is stored little-endian.
- 16 voices, with 512 waves and 1024 bubbles in shared pools.
- Host parameter modulation is supported globally. Per-note modulation and note
  expressions are ignored.
- Deliberately built without `-ffast-math`: on x86 GCC that links
  `crtfastmath.o`, which would flip the FTZ and DAZ flags for the entire host
  process.
- Waves only. The references that contained birds or aircraft were excluded from
  the fit for that reason.

## Versioning

Semantic versioning. **MAJOR** is an overhaul: a rewrite of the synthesis model,
or a change that breaks existing presets or saved host state. **MINOR** adds
something — a parameter, a layer, presets. **PATCH** is fixes that add nothing
new.

## License

MIT. {{PLUGIN}} contains no samples and no third-party code. The only external
dependencies are the CLAP headers, which are MIT licensed, plus X11 and Cairo for
the plugin window.

Reference recordings used during development are not part of the release and are
not redistributable.
