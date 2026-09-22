---
tagline: Synthesised fire
subtitle: CLAP instrument for Linux and Windows
accent: #FF5A2C
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that generates fire: campfires, hearths, wood
stoves and large open blazes. It contains no samples. The combustion roar, every
crackle, every hiss of steam out of wet wood and every thump of a log giving way
are computed from noise, filters and envelopes while the plugin plays, so no two
takes are alike and there is no loop point to find.

## A fire is not a river

One number decides the whole design, and it is the first thing the reference
library says.

| | {{PLUGIN}}'s library | RiverFlow's library |
|---|---|---|
| crest factor, median | **31.7 dB** | 19.6 dB |
| envelope variation at 4 ms | **0.85** | 0.26 |
| envelope variation at 50 ms | **0.56** | 0.12 |

Half of a river is its bed — a third of RiverFlow's reference library is
statistically indistinguishable from shaped noise. A fire is the opposite: a
*quiet* bed with very loud, very short things happening on top of it. Twelve
decibels of crest factor separate the two, and almost every decision in this
plugin follows from that ratio.

So the roar here is deliberately modest, and the layers above it carry the sound.
Turning *Crackle Level* down until the fire is only its roar is not a broken
patch — it is a fire heard from far enough away that nothing resolves any more,
which is what *Distant Fire* is.

## How a crackle arrives

The second finding took three timescales to see, and they do not agree — which
is the point.

| Timescale | What was measured | A Poisson process gives |
|---|---|---|
| 1 second | variance/mean of the count = **3.90**, up to 20.1 | 1.0 |
| 50 ms | P(gap < 50 ms) = **1.03x** the exponential | 1.0 |
| 10 ms | P(gap < 10 ms) = **1.97x**, up to 4.0 | 1.0 |
| 50 ms | branching ratio = **0.02** | 0 |

Read together:

- **At 50 ms the arrivals are exactly Poisson**, and a branching ratio of 0.02
  rules out a cascade. One crackle does not make the next one more likely — which
  is worth knowing, because "crackles come in bursts" is the obvious thing to
  assume and it is measurably wrong at that scale.
- **Below 10 ms there are twice as many gaps as Poisson allows.** A crackle is
  not one impulse. It is a short train of one to three, inside ten milliseconds,
  and then nothing: gas breaking out of a split in the wood in stages rather than
  all at once. That is *Burst*.
- **At one second the count varies four to twenty times more than its mean.** The
  *rate itself* wanders, on the same seconds-long timescale as the flame's own
  surging. That is *Flare*, and once the rate is allowed to wander the whole
  excess is accounted for without anything else being bursty.

So the spawner has three levels and no cascade: a slowly wandering rate, a
Poisson process at that instantaneous rate, and a one-to-three pulse train per
arrival.

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

### Folders

A preset library is a shelf, and a long one is worth tidying. Saving a preset as
`Folder/Name` puts it in a folder of that name under the user preset directory,
creating the folder if it is not there; a name with no slash in it saves into the
library's root, as every save did before. Typing the folder is the whole gesture
— there is no separate "new folder" step, and a folder with nothing in it cannot
be made.

Folders are one level deep on purpose: a preset library is a shelf rather than a
filesystem, and a tree deep enough to get lost in is one somebody will get lost
in. The browser then grows a column of shelves down its left side — *All*, the
factory library, each of your own folders, and *Unfiled* for anything saved
without one — each with the number of presets on it. Clicking a shelf lists just
that shelf.

### Preset packs

A whole folder can be written out as a single file — a **preset pack**, extension
`.crackleblazepack` — so a library can be handed to somebody else, or moved between
machines, as one file rather than a directory.

With a folder selected in the browser:

| Button | What it does |
|---|---|
| `EXPORT` | Writes the selected folder as a pack into the `packs` directory beside `presets` |
| `EXPORT AS...` | The same, to a location you choose |
| `IMPORT...` | Lists the packs already in `packs`, plus *Other file...* for one from anywhere |
| `REVEAL` | Opens the folder the pack was just written to in the system's file browser |

Importing adds the pack's presets to the library as a new folder named after the
pack. Nothing is ever overwritten: importing the same pack twice gives two
folders rather than a mixture of both versions in one.

A pack is the preset format again with a separator line between the presets, so
it can be read, diffed and edited by hand like everything else here. It carries
each preset's text verbatim rather than a re-serialised copy, which is what makes
a round trip through a pack lossless.

`EXPORT AS...`, *Other file...* and `REVEAL` need the desktop's own file chooser —
`zenity` or `kdialog` on Linux, `xdg-open` to reveal, and the operating system's
own on Windows. Where none is installed the buttons that need one are not drawn,
and the plugin's own `packs` directory, which is where `EXPORT` writes, is enough
on its own.

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
2. Draw a long note — eight bars or more. The fire burns for as long as the note
   is held, and the release fades it out.
3. Open the preset browser by clicking the preset name in the window header and
   try **Cottage Hearth**, then **Dry Kindling**, then **Furnace Roar**.

Those three are the span of the instrument. The middle one is almost nothing but
countable ticks; the last is almost nothing but bed.

## Three things to try first

**Change the fire.** *Fire* selects one of five colours, and they are not names
someone invented for a menu: they are the five clusters the reference library's
*beds* fall into, each shipped as the measured octave-band centroid of its
members. *Blend* crossfades to the next one, so the five are a continuum rather
than five settings. Deep Blaze peaks at 125 Hz and falls 15 dB by 8 kHz; Stove
Draught is 22 dB down at 1 kHz and peaks at the very top of the spectrum.

**Move Sap.** This is the wet/dry axis, and it is the widest per-recording
variable in the whole library: from 0.03 in a big open blaze to 0.57 in a
fireplace full of unseasoned wood. At the bottom the fire is all sharp ticks; at
the top most of what you hear is steam leaving the wood, which takes seven times
as long to die away. Nothing else changes — the two populations have the same
measured colour.

**Move the fire away.** Raise *Distance*. Air absorption takes the top off, the
individual crackles multiply and shrink, their edges smear, and at the far end
nothing is countable any more. At zero the air filter is bypassed outright rather
than parked out of the way, because no distance means no air to absorb.

# How it works

## The roar, and where its colour comes from

The bed is one noise source per channel through eight bandpasses at octave
centres from 125 Hz to 16 kHz, and the gains are **solved** rather than set: an
octave-wide bandpass leaks into the octave beside it, so driving each band at its
target leaves the sum wrong. The solver asks instead for the gains whose *summed*
response is the measured curve.

The five colours are the five clusters `tools/analysis/shapes.py` finds in the
library, each shipped as its cluster's centroid. A table of eight numbers is a
formula and not a sample; nothing here reproduces recorded audio.

**The crackles had to be gated out before that clustering meant anything.** At a
median crest factor of 31.7 dB, the plain spectrum of a fire is substantially the
spectrum of its crackles — so clustering on it sorts the recordings by how close
the microphone was, not by what kind of fire they are. `tools/analysis/bed.py`
drops the loudest quarter of every recording's 4 ms frames first. What the
crackles add back is measurable and is exactly where you would expect:

| | 125 Hz | 250 | 500 | 1k | 2k | 4k | 8k | 16k |
|---|---|---|---|---|---|---|---|---|
| the crackles add | −7.6 | −4.0 | −1.8 | +0.4 | **+4.0** | **+5.9** | **+3.8** | +1.0 |

## A fire flares as one flame

This is the sharpest difference between this plugin's bed and RiverFlow's, and it
is a single measurement. The correlation between one octave band's 100 ms
envelope and its neighbour's is:

| | measured |
|---|---|
| {{PLUGIN}}'s library | **0.54 … 0.91, median 0.85** |
| RiverFlow's library | 0.07 … 0.25 |

A river's bands wander independently — it is a great many small events at
different scales, and the low end surging tells you nothing about what the hiss
is doing. A fire is one object. When it flares, all of it flares.

So the model is one common random walk at √0.85 plus a small independent walk per
band at √0.15, which reproduces the measured 0.85 exactly. And the same common
walk drives the crackle rate, because in a real fire a flare-up is both more roar
and more crackling — that sharing is what produces the measured variance-to-mean
ratio of 3.9 without any part of the spawner being bursty.

It is a filtered random walk and **not** an oscillator. The envelope spectrum of
every reference falls smoothly at about −5.7 dB per decade over 0.1–10 Hz, and
the strongest frequency in it ranges from 0.12 to 2.54 Hz with no peak that
survives from one recording to the next. A fire has no rhythm. *Flare Rate* sets
the corner of the noise that drives the walk, not a frequency you will hear.

## A crackle is a click, not a ring

It is tempting to model a crackle as a struck piece of wood — a resonator with a
pitch. The measurement says no.

For every detected crackle, `tools/analysis/crackshape.py` takes 8 ms from the
onset, subtracts the spectrum of the 8 ms before it, and averages the excess over
every event in the recording. What is left is the crackle alone, with the roar it
sat in removed. Its **spectral flatness is 0.67** across the library, where 1.0 is
white noise and a resonance would be a small fraction. There is no ring in it.
The few recordings that measure low (0.15–0.19) are the ones whose event spectra
are dominated by bed leakage at the bottom, not by a resonance.

So a crackle here is a shaped noise burst with a fast attack and an exponential
decay, and nothing else. Giving it a resonator would be inventing a physical
object the recordings do not contain.

What it *does* have is a low end: the measured event spectrum is still within 8 dB
of its peak at 250 Hz, which is *Body*. Without that a crackle is all fizz and no
wood.

| | measured across the library |
|---|---|
| rate | 11.5 … 51.0 /s, median **29** |
| prominence over the roar | 6.5 … 20.2 dB, median **13.2** |
| amplitude spread | log-normal, σ = **6.0 dB** |
| decay to −10 dB | median **2.5 ms** (1.0 … 5.5) |
| spectral flatness | median **0.67** |

## A sizzle is a crackle held open

The population is bimodal in duration, and only in duration. Splitting the
detected events at 8 ms and measuring each half separately:

| | share | decay to −10 dB | spectrum |
|---|---|---|---|
| ticks | 0.79 | **2.5 ms** | 0 / −2.2 / −6.6 / −6.4 / −7.9 / −11.0 / −18.3 dB |
| hisses | 0.21 | **19 ms** | 0 / −3.6 / −3.7 / −5.5 / −7.0 / −7.9 / −20.5 dB |

Seven to eight times the decay, and the same colour to within about 2 dB per
octave. These are not two different kinds of event. They are the same event held
open for different lengths of time, because steam has to leave the wood and a
bursting pocket of dry gas does not.

That is why *Sap* is a single control that moves events from one population to
the other, rather than two independent layers — and why it is a parameter at all
rather than a constant. Its measured range across the library is 0.03 to 0.57,
which is the widest spread of any quantity measured here.

*Steam* adds a narrow, high, per-event jet on top of the sizzle's broadband
noise: the whistle of steam leaving a split. That one is modelled from the
physics rather than fitted, because nothing in the library isolates it.

## A log giving way

The fourth layer is rare and easy to miss. Finding it at all needs an onset
detector run on 80–300 Hz with anything that *also* jumps in the top thrown away,
because a loud crackle leaks into every band and would otherwise be counted
twice. What survives:

| | measured |
|---|---|
| rate | 0 … 136 a minute, median **6** |
| prominence over the low bed | 13.9 … 25.7 dB, median **16.1** |
| decay | **6 … 10 ms** |

A thump, not a boom, and the only layer in the plugin with no top end at all.
Two of the seventeen usable recordings have none. *Log Collapse* is the one
factory preset that puts it where you cannot miss it.

## Nothing below 60 Hz

Eight of the twenty-five reference recordings carry between 27 and 60 per cent of
their total energy below 60 Hz. None of it is fire.

`tools/analysis/lowend.py` correlates each recording's 20–60 Hz envelope against
its 1–4 kHz envelope. In twenty-four of the twenty-five the correlation is under
0.2, and in half of them it is indistinguishable from zero — while the one
exception carries only 2.4 per cent of its energy down there. It is traffic,
ventilation, wind and handling noise on the microphone. Synthesising it would be
fitting the recordist's afternoon.

So nothing below 60 Hz is generated, *Highpass* defaults there, and every
analysis script high-passes before measuring anything.

Eight recordings were discarded outright on the same evidence. Above 800 Hz they
carry a flat −45 dB plateau out to 12 kHz — a dither floor, not content — and
their crest factors of 11.8 to 16.5 dB say the same thing: nothing impulsive is
in them. They are rumble beds with a fire somewhere underneath. Seventeen
references carry the whole fit.

## Distance and the hearth

*Distance* does what distance does outdoors, all at once: air absorption takes
the top off, what is left tilts downwards, the individual events multiply and
shrink — a fire far enough away is a bonfire, not a hearth heard quietly — and
their edges smear with the air they crossed. Energy is held roughly constant
across that, so *Distance* is a character control and not a volume control.

*Hearth* sets how much early reflected field there is. `Open` has almost none: a
fire in a field has nothing close enough to reflect off, and eight discrete early
reflections outdoors is exactly what makes a reverb sound like a bathroom.
`Fireplace` and `Stove` have a great deal — the two references recorded inside a
fireplace measure as the most reverberant in the library, and *Open Fireplace* is
fitted to them.

# The plugin window

{{PLUGIN}} draws its own window with X11 and Cairo on Linux, and Cairo on the
win32 backend on Windows. There is no toolkit dependency, which is what keeps the
plugin one self-contained file. It is embedded in the host's own window through
CLAP's GUI extension and repainted from the host's timer, and it resizes by
zooming one layout, from half size to four times.

The window is the suite's, in {{PLUGIN}}'s own ember-orange theme, with tongues
of flame along the bottom of the header behind the wordmark and an ember thrown
up wherever a crackle happens. The embers are deterministic in the crackle
number, so a pinned *Random Seed* gives a repeatable picture as well as a
repeatable sound.

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
a fader, its width as a slim slider under it. They are the plugin's own
parameters, not a second set — moving a fader moves the knob on the panel, and
the host sees the same automation it always would.

`M` mutes a layer and `S` solos one, which is how a preset gets built from
nothing: solo one layer, get it right, bring the next one back. Several layers
can be soloed at once.

Mute and solo are **not** parameters and are not saved with a preset. They work
by holding a layer's level at the bottom of its range and remembering what it
was, so while either is active the preset bar shows a `SOLO ON` or `MUTE ON` chip
— click it to release every hold at once. `SAVE` releases them first, so a muted
layer can never be written into a preset as a silent one, and so does loading a
preset. Touching a held layer's fader also releases the holds rather than
fighting the hand on it. Closing the mixer does not: soloing a layer and then
going to its knobs on the panels is what the mixer is for.

# Parameter reference

Every range, default, unit and explanation below is read from the plugin's own
parameter table when this manual is built, so it cannot drift from what the
window and the host show.

{{PARAMETER_REFERENCE}}

# The preset library

Generated from the preset files themselves.

{{PRESET_LIBRARY}}

# Using it in a host

## Notes

A note lights the fire and holding it keeps it burning. The ADSR shapes the whole
scene rather than individual events, and *Release* is what fades it out.
*Velocity To Fire* makes velocity behave like a bigger fire: it raises the event
rates and the roar's weight together, which is what feeding a fire actually does.

Fire needs long notes. Its character is in the statistics of many events and in
the slow wander of the flare, and neither is audible in a short one.

There are 8 voices, each with its own roar, and 2048 events in a shared pool, so
the number of held notes cannot multiply the cost without bound. The event pool
is larger than elsewhere in the suite because a fire spawns far more events a
second than a river does — up to 51 crackles a second, each of them a burst of
two or three.

## Automation and modulation

Every parameter is automatable, and host parameter modulation
(`CLAP_EVENT_PARAM_MOD`) is supported globally. Per-note modulation and note
expressions are ignored. Parameter changes are sample accurate, so an automation
ramp over a bar — a fire catching over *Crackle Rate* and *Roar Level* — is a
smooth move rather than a staircase at block boundaries.

## Reproducible renders

*Random Seed* at 0 never repeats. Any other value renders the same fire for the
same notes every time, and the output is byte-identical across runs at both 48
and 96 kHz. Each voice's noise generators and its first event timers are derived
from its own slot and key rather than from the shared generator, so a fixed seed
promises the same fire even when note events arrive before the seed has been
applied.

## CPU cost

Cost is bounded by the pools. The roar is the fixed part — sixteen bandpasses per
voice per sample — and it does not vary with any parameter. What varies is
*Crackle Rate* times *Burst*, which decides how many events are running, *Sizzle
Decay*, which decides how long each one lives, and *Space*, which is the room
model. At the default settings roughly two events are alive at any moment.

## Rendering and tails

Fire has a very short tail compared with surf or thunder — the longest thing
still to come after a note is the release plus the room, since even a sizzle is
gone in a tenth of a second. When bouncing, a second or two past the end of the
note is enough.

## Headroom

Every factory preset carries a trim in its own output gain, measured rather than
judged: each was rendered 24 dB down with the seed pinned, its true peak read
off, and its gain set so that peak lands at −3 dBFS — then capped at −3 dB, so a
preset that is meant to be quiet stays quiet instead of being normalised up to
meet the others.

That cap matters more here than elsewhere in the suite. A fire's amplitudes are
log-normal with a measured 6 dB spread, so the loudest crackle in half a minute
stands some 18 dB over the median one. Without the trim, several presets rendered
into the output stage's soft clipper — and a clipped crackle is a crackle with
its transient taken off, which is the one thing this plugin must not do.

# What is still being fitted

{{PLUGIN}} {{VERSION}} is a first fit, and the measurements say where it is not
there yet. All of this is in the plugin's `TODO.md` with what is known about it.

**Nothing has been validated by ear.** Every number in this manual is a
measurement, and the suite's own rule is that a number agreeing with a number
proves nothing about the sound. This is the first thing on the list.

**The default patch is not varied enough.** It renders with an envelope variation
of 0.56 at 4 ms against the library's 0.85, and 0.41 at 50 ms against 0.56. The
crest factor is 29.8 dB against a median of 31.7. The fire is a little too even,
minute to minute, and the likeliest cause is the next item.

**The flare depth above 500 Hz is extrapolated, not measured.** The bed's
envelope variation was measured cleanly at 125, 250 and 500 Hz (0.39, 0.45, 0.57).
Above that no gate separates the bed from the crackle and sizzle layers riding on
it, and since this engine generates those separately, shipping the measured 1.28
at 2 kHz would make the bed wobble by an amount that in the reference *was* the
crackles — and then add the crackles on top. The upper five bands therefore
continue the trend and flatten. Fitting them properly means fitting the render's
own per-band variation against the library rather than the bed's.

**The crackles are not unequal enough.** The rendered amplitude spread measures
4.2 dB against the library's 6.1, even though the parameter is set to the measured
6.0. Something in the chain between the draw and the detector is narrowing the
distribution.

**The crackles are a little bright.** The rendered event centroid is 3.7 kHz
against a library median of 3.0.

**The bed's octave shape is within about 1 dB, not exact.** The bandpasses are set
to a Q of 2.2, which was chosen by modelling: at the octave-wide Q of 1.4 the
bank could not reach the contrast two of the five shapes ask for and left them 3
to 4 dB short, and at 2.8 it is exact but the ripple between band centres reaches
2.8 dB peak-to-peak. 2.2 is where the error stops mattering before the ripple
starts to.

**The hearths and the space controls are chosen, not measured.** A reverberation
measurement needs an impulse and a known source position, and the library has
neither.

**Eight of the twenty-five references were discarded**, for the measured reasons
in *Nothing below 60 Hz*. A larger library — particularly of large open fires,
where only a handful are usable — would narrow every number here.

# Reference and limits

## Where the model comes from

- **The reference library itself**, which supplied nearly everything: the five
  octave-band colours, the flare's depth and its correlation across bands, the
  crackle rate, amplitude law, decay and spectrum, the tick/hiss split, the
  settle population, and the three timescales the spawner is built on. The
  numbers and the scripts that produced them are in the plugin's
  `tools/analysis/`.
- The physical reading behind the layers is the standard one: **combustion roar**
  as broadband low-frequency noise radiated by unsteady heat release in a
  turbulent flame, which is why the roar is a low-frequency phenomenon whatever
  the crackles above it do; and **crackling** as pyrolysis gas breaking out of
  heated wood, which is why it is impulsive and why it has no pitch. The library
  decided every number; the physics only decided what to measure.
- Where nothing in the library isolates a quantity — *Steam*, the hearth
  enclosures, the space controls — the model is stated as chosen rather than
  fitted, both here and in the chapter above.

## What it does not do

- **No rain, no wind, no wildlife.** Those are other instruments in the suite.
- **No sub-bass.** Nothing below 60 Hz is generated, for the measured reason
  above. If you want the low rumble a large fire puts through a floor, that is
  not in the references and is not in here.
- **No pitched resonance in the crackles.** Measured flatness says there is none
  to model.
- **No per-note modulation.** Global parameter modulation only.
- **Nothing above 16 kHz is fitted.** The topmost measured octave straddles the
  limits of the reference recordings themselves.
