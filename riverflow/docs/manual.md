---
tagline: Synthesised running water
subtitle: CLAP instrument for Linux and Windows
accent: #57C77A
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that generates running water: rivers, creeks and
waterfalls. It contains no samples. The broadband bed, every pocket of air
trapped between the stones and every drop that lands on them are computed from
noise, filters and resonators while the plugin plays, so no two takes are alike
and there is no loop point to find.

It covers a wide span — from a river loud enough to talk over down to single
drops off an overhang — and it covers it that way because the reference library
says the two ends are different things rather than the same thing at different
volumes.

## Some rivers really are white noise

One measurement decides the whole design of this plugin, and it is worth stating
plainly because it sounds like a criticism and is not.

Each of the 77 reference recordings was measured for the variation of its 4 ms
envelope, band by band, **beside a Gaussian white-noise control put through the
same statistics**. The control row is what makes the numbers mean anything: a
transient detector finds "events" in band-limited noise at a steady ten to
twenty a second, so an event rate quoted for a river without it is not a
measurement of the river.

| | 200-800 Hz | 0.8-2 kHz | 2-6 kHz | 6-14 kHz |
|---|---|---|---|---|
| **Gaussian control** | 0.30 | 0.22 | 0.12 | 0.09 |
| library minimum | 0.30 | 0.22 | 0.13 | 0.11 |
| library median | 0.39 | 0.33 | 0.30 | 0.29 |
| library maximum | 1.01 | 0.93 | 1.64 | 1.05 |

The smoothest third of the library lands *on* the control row, and the event
detector finds too few discrete events in those recordings to characterise at
all. They are not "like" noise. By these measures they are noise, spectrally
shaped — and shaped noise is therefore a complete and correct model of them,
which is why several of the presets in this manual ship with both event layers
switched off.

The other end of the library measures two to thirteen times the control, and the
band it happens in says what kind of water it is: around 500 Hz for a creek
dabbling between stones, and 2-6 kHz for drops on wet rock, where one reference
measures 1.64 against a control of 0.12.

So {{PLUGIN}} is a bed plus two populations of discrete events, and the
instrument is the ratio between them.

## What is in this manual

The chapters below cover installing it, getting a first sound, how the model
works and why it is built the way it is, the window, then a generated reference
for every parameter and every factory preset. The last two chapters are honest
about what does not fit yet and where the model comes from.

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
2. Draw a long note — eight bars or more. The water runs for as long as the note
   is held, and the release fades it out.
3. Open the preset browser by clicking the preset name in the window header and
   try **Mountain River**, then **Bubbling Creek**, then **Hanging Trickle**.

Those three are the span of the instrument. The first is close to pure shaped
noise; the last is almost nothing but countable events.

## Three things to try first

**Change the water.** *Water* selects one of six colours, and they are not
names someone invented for a menu: they are the six clusters the reference
library falls into, each shipped as the measured octave-band centroid of its
members. *Blend* crossfades to the next one, so the six are a continuum rather
than six settings. Deep Rush peaks at 500 Hz and falls 25 dB by 16 kHz; Trickle
rises all the way to the top.

**Turn the events down, then up.** Set *Dabble Level* and *Trickle Level* to
their minimum and you have a river that is honestly just shaped noise — which is
what a third of the reference library measures as. Bring *Dabble Level* up and
the water starts folding over stones; bring *Trickle Level* up and drops start
landing on them.

**Move the water away.** Raise *Distance*. Air absorption takes the top off,
the individual events multiply and their edges smear, until nothing is countable
any more and what is left is the roar. At zero the air filter is bypassed
outright rather than parked out of the way, because no distance means no air to
absorb.

# How it works

## The bed, and where its colour comes from

The bed is one noise source per band per channel through eight octave-wide
bandpasses from 125 Hz to 16 kHz — which is the resolution the reference
colours were measured at.

The gains are not set to the measured curve. They are *solved* so that the
bands' **sum** is the measured curve, which is a different thing: octave-wide
bandpasses an octave apart overlap by design, and setting each band to its
target leaves the two outermost bands 3 to 9 dB low, having a neighbour on one
side only. Where a colour's own skirt is steeper than the bank can manage — Creek
falls 10 dB from 250 Hz to 125, Deep Rush 10 dB from 8 to 16 kHz — a two-pole
filter at the outer band edge makes that skirt and the bank is solved for what
is left.

Each band is driven by its **own** noise. That is not an optimisation waiting to
be undone: overlapping bandpasses fed one signal add coherently, and solving the
gains against a model that assumes their energies add left the steepest colours
7 dB out in their outermost octave. It is also the more faithful arrangement,
which is the next section.

## A river's bands move independently

The bed's level wanders, and the measurement of *how* is the single thing that
most separates this from gated noise.

Two findings. First, the depth of the wander falls with frequency: relative to
the 1-2 kHz band, the 125-250 Hz band moves 2.43 times as far and the 4-8 kHz
band 0.84 times. That is why a river's low end seems to surge while its hiss
sits still, and it is the opposite of what you might guess — more energetic
turbulence entrains smaller bubbles, so the top ought to move more. It does not.
The reason is probably that the top of the spectrum is the sum of a vast number
of tiny bubbles whose average is stable, while the bottom is a handful of large
pockets appearing and vanishing in lumps.

Second, the bands do not move **together**. The correlation between one band's
100 ms envelope and its neighbour's measures 0.07 to 0.25 across the library.
So the bed is not one gain being moved: each band gets its own random walk, at
its own measured depth.

And it is a walk, not an oscillator. The references' envelope spectra fall
smoothly at about −1.5 dB per decade over 0.3-10 Hz, with no peak that survives
from one recording to the next. A river has no rhythm. *Surge Rate* is therefore
the corner of the noise that drives the walk, and there is no LFO anywhere in
this plugin.

## Grain: the bubbles too small to count

The bed's own graininess is *Grain*, and it is frequency-weighted from the
measurements: the library's departure from the Gaussian control grows from 30%
at 200-800 Hz to 220% at 6-14 kHz. So Grain does far more to the top of the bed
than to the bottom.

What it models is a vast number of sub-millimetre bubbles bursting — at that size
and rate they overlap several deep and are not separately audible, so they are
generated as a granular envelope on each band rather than as thousands of
oscillators. That is the same sum for one multiply a sample.

Zero is the correct value for the smooth third of the library, and it is what
several presets use.

## A dabble is a cluster, not a bubble

Water folding over a stone traps a pocket of air, and the pocket rings at the
Minnaert frequency for its radius: f₀ ≈ 3.26/r kHz, so 3.3 mm rings at 988 Hz.
The library's dabbles measure a median pitch of 984 Hz. The relation and the
recordings agree to one per cent, which is a good reason to trust it.

But one pocket is not what a dabble is, and the measurement is unambiguous about
it. The event-triggered spectrum gives a Q between 0.7 and 5, which is a ring of
about a millisecond. The event-triggered *envelope* takes 7 to 34 ms to fall
10 dB. One bubble cannot do both. A dabble is a short burst of several — which is
also what water folding over a stone physically does — so *Cluster* is how many
and *Spill* is how long they are spread over. A cluster shares its pan and its
size, because it is one stone.

*Glug* is how much of a dabble is the water being displaced rather than the air
ringing. Without it a cascade of pockets is a music box; with it, it is water.

## A drop is an impact and then a pocket

*Trickle* is single drops striking stone or standing water: the impact off
whatever they hit, then the tiny pocket they entrain. Measured 5 to 25 a second
at 0.42-1.72 mm, ringing 2-8 kHz — and again Minnaert lands within a per cent of
the measured median pitch.

*Impact* balances the strike against the pocket: a drop on rock is mostly a
broadband tick, a drop into water mostly the bubble. *Stone* is what it landed
on. *Splash* is the wash where the water is deeper, which exists because the
references' drop decays are bimodal — 8-13 ms for a tick against 56-86 ms where
there is a pool under it.

## The plunge pool

A cloud of bubbles rings far below any bubble in it, which is where the weight
under a waterfall comes from. It is not a lowpass of the water; it is a
resonance of its own. Three of them, at the 1.00 / 1.53 / 1.90 ratios Xue et al.
measure for a pour: one resonator reads as a tuned pipe where three read as a
body of water.

## Nothing below 60 Hz

Four references carry up to a quarter of their total energy below 60 Hz, and it
would be easy to model. By Minnaert, 32 Hz is a 100 mm air pocket — conceivable
in the plunge pool under a big fall, out of the question in a creek.

The two candidates are distinguishable. Water in the low band is made by the
same events as the water above it, so its envelope correlates with the mid
band's; wind and handling noise on the microphone know nothing about the water.
Measured, the correlation is below 0.15 in every recording, including the ones
where the low end is a fifth of the energy.

It is the recordist's afternoon, not the river. So nothing below 60 Hz is
generated, and *Highpass* defaults there for a measured reason rather than a
cautious one.

## Distance and the banks

*Distance* is air absorption plus a downward tilt, with the events multiplied
and their edges smeared — because a river heard from far off is not one stretch
of water heard quietly, it is a whole valley of it arriving over a wide arc.

*Banks* sets how much is close enough to reflect: 0.03 for an open river against
0.90 for a culvert. This is a lesson the suite learned elsewhere and did not
have to learn twice — eight discrete early reflections outdoors is what makes a
reverb sound like a bathroom, and no amount of tail talks the ear out of it.

# The plugin window

{{PLUGIN}} draws its own window with X11 and Cairo on Linux, and Cairo on the
win32 backend on Windows. There is no toolkit dependency, which is what keeps
the plugin one self-contained file. It is embedded in the host's own window
through CLAP's GUI extension and repainted from the host's timer, and it resizes
by zooming one layout, from half size to four times.

The window is the suite's, in {{PLUGIN}}'s own river-green theme, with
streamlines drifting across the header behind the wordmark and a ring spreading
wherever a dabble happens. The rings are deterministic in the dabble number, so
a pinned *Random Seed* gives a repeatable picture as well as a repeatable sound.

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

# Parameter reference

<div class="paramref" markdown="1">

{{PARAMETER_REFERENCE}}

</div>

# The preset library

Every preset below is fitted to one named field recording, and checked by
rendering it back and measuring it against that recording. Fifteen of the twenty
match their reference within 6 dB in every octave band.

Several of them ship with *Grain* at zero and both event layers off. That is not
a preset left unfinished — it is what the reference measures as.

{{PRESET_LIBRARY}}

# Using it in a host

## Notes

A note starts the water and holding it keeps it running. The ADSR shapes the
whole scene rather than individual events, and *Release* is what fades it out.
*Velocity To Flow* makes velocity behave like more water: it raises both event
rates and the bed's weight together, which is what a rising river actually does.

Running water needs long notes. Its character is in the statistics of many
events and in the slow independent wander of the bed, and neither is audible in
a short one.

There are 8 voices, each with its own bed, and 2048 pockets in a shared pool, so
the number of held notes cannot multiply the cost without bound. The voice pool
is deliberately smaller than elsewhere in the suite because one stretch of water
costs sixteen bandpasses a sample.

## Automation and modulation

Every parameter is automatable, and host parameter modulation
(`CLAP_EVENT_PARAM_MOD`) is supported globally. Per-note modulation and note
expressions are ignored. Parameter changes are sample accurate, so an automation
ramp over a bar — a river rising over *Flow Level* and *Dabble Rate* — is a
smooth move rather than a staircase at block boundaries.

## Reproducible renders

*Random Seed* at 0 never repeats. Any other value renders the same river for the
same notes every time, and the output is byte-identical across runs at both 48
and 96 kHz. Each voice's noise generators and its first event timers are derived
from its own slot and key rather than from the shared generator, so a fixed seed
promises the same river even when note events arrive before the seed has been
applied.

## CPU cost

Cost is bounded by the pools. The bed is the fixed part — sixteen bandpasses per
voice per sample — and it does not vary with any parameter. What varies is
*Dabble Rate* times *Cluster*, and *Trickle Rate*, which together decide how
many resonators are running, and *Space Amount*, which is the room model. Grain
is cheap by construction: it is a multiply per band, not a bank of oscillators.

## Rendering and tails

Running water has a short tail compared with surf or thunder — the longest thing
still to come after a note is the release plus the room. When bouncing, a second
or two past the end of the note is enough.

# What is still being fitted

{{PLUGIN}} {{VERSION}} is a first fit, and the measurements say where it is not
there yet. All of this is in the plugin's `TODO.md` with what is known about it.

**Nothing has been validated by ear.** Every number in this manual is a
measurement, and the suite's own rule is that a number agreeing with a number
proves nothing about the sound. This is the first thing on the list.

**The 16 kHz octave.** Creek and Deep Rush come out about 5 dB high there and
within 2 dB everywhere else, and all five presets that miss their reference by
more than 6 dB in any band have references peaking in that octave. The cause is
understood: an octave-wide bandpass leaks 6-8 dB into its neighbour, so a 15 dB
step between the last two bands is out of reach, and one two-pole skirt at the
band edge does not close it.

**The peakiest references are not reached.** The library's median crest factor is
matched exactly — 19.5 dB against 19.6 — but its extremes are not: *Bubbly
Falls* measures 32.5 dB in its reference against 20.1 in the render, and
*Hanging Trickle* 31.1 against 22.2.

**The most correlated references cannot be reached.** Four measure an L/R
correlation of 1.00 — one recording of one place — and the renders give 0.77 to
0.88, because a pooled event still lands at one pan and decorrelates whatever it
is added to.

**The plunge pool, the space controls and *Banks* are chosen, not measured.**
Separating a plunge pool's resonance from the bed's own low end needs references
that isolate one, and the space controls need a reverberation measurement the
library cannot give, having neither an impulse nor a known source position.

# Reference and limits

## Where the model comes from

- Minnaert, M., *On musical air-bubbles and the sounds of running water*,
  Philosophical Magazine 16, 1933 — f₀ ≈ 3.26/r, which is why a pocket of air
  has a pitch at all, and the title of which is this plugin's subject exactly.
- Xue et al., on bubble damping and on the collective modes of a bubble cloud —
  the radiative and thermal loss terms the pockets use, and the 1.00 / 1.53 /
  1.90 mode ratios of the plunge pool.
- Prosperetti, A., on the acoustics of bubble clouds and the origin of the
  broadband spectrum of running water.
- The reference library itself, which supplied everything else: the six
  octave-band colours, the per-band surge depths and their independence, the
  event rates, pitches, ring times and prominences, and the Gaussian control the
  whole design rests on. The numbers and the scripts that produced them are in
  the plugin's `tools/analysis/`.

## What it does not do

- **No rain, no wind, no wildlife.** Those are other instruments in the suite.
- **No sub-bass.** Nothing below 60 Hz is generated, for the measured reason
  above. If you want the low rumble a distant weir puts through a floor, that is
  not in the references and is not in here.
- **No per-note modulation.** Global parameter modulation only.
- **Nothing above 16 kHz is fitted.** The topmost measured octave straddles the
  limits of the reference recordings themselves.
