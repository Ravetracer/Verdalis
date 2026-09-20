---
tagline: Synthesised insects
subtitle: CLAP instrument for Linux and Windows
accent: #E6D93B
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that generates insects: a single bee, a hive, a
wasp crossing the microphone, a mosquito that will not leave, a cicada chorus at
noon, a field of crickets after dark. It contains no samples. Every wingbeat,
every pass and every tymbal click is computed from phase accumulators, pulses,
noise and filters while the plugin plays, so no two takes are alike and there is
no loop point to find.

## A bee is half noise

One number decides the whole design, and it is the first thing the reference
library of 67 field recordings says.

| | harmonic-to-noise ratio |
|---|---|
| Honeybee | **6.6 dB** |
| Mosquito | **16.0 dB** |
| a hive or a swarm | 0 – 2 dB |

A mosquito really is the thin whine it sounds like. A honeybee is very nearly as
much turbulent air as tone — and that is why a stack of oscillators never sounds
like one, however carefully its harmonics are set. It is not a missing harmonic.
It is that most of what you are hearing is not harmonic at all.

So the engine generates turbulence alongside the wingbeat, and *Rasp* is the
ratio between the two in dB, referenced to that measurement. At the centre of its
travel each species reproduces its own figure.

## Many insects are noise

The second finding is what makes the swarm work, and it means the plugin needed
no ensemble stage, no chorus and no detune.

A single close insect measures 8 to 13 dB harmonic-to-noise. A hive measures 0 to
2. Nothing was added to the recording to make that happen: enough fundamentals,
scattered widely enough, *are* noise. So *Count* and *Spread* are the whole
mechanism, and rendering the engine back confirms it — twelve individuals at 150
cents apart come out at 1.7 dB, inside the measured hive figure.

At a *Count* of one, *Spread* correctly does nothing. One insect is one insect
however wide the distribution it was drawn from.

## The Doppler shift is real and inaudible

The third finding is a negative one, and it is worth knowing before you go
looking for the control that must be broken.

Ten of the library's recordings are a single clean pass. They rise a median of
**13.3 dB** over their own approach, over a **1.37 s** bump, at about **3.2
m/s**. At that speed the Doppler shift across the pass is **30 cents** — while
the same insects' wingbeat rates wander by 43 to 103 cents on their own.

The pitch shift is buried in the wander. A flyby reads as movement because of its
level, its filtering and its pan, not because of its pitch. {{PLUGIN}} computes
the Doppler anyway, because it is physically right and costs nothing, and *Speed*
goes far past anything an insect can do for when you want it as an effect.

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
2. Draw a long note — eight bars or more. The insects fly for as long as the note
   is held, and the release fades them out.
3. Open the preset browser by clicking the preset name in the window header and
   try **Single Bee**, then **Hive Wall**, then **Cicada Noon**.

Those three are the span of the instrument. The first is one insect, close enough
to count its wingbeats. The second is sixty of them and no longer countable at
all. The third is not a wingbeat in any sense.

## Three things to try first

**Change the species.** *Species* selects one of seven measured insects, and they
are not names someone invented for a menu: each carries the wingbeat rate
measured across that species' recordings, the shelf and resonance fitted to its
measured harmonic stack, and how harmonic its buzz is to begin with. A hornet
beats at 85.7 Hz and a mosquito at 463.7, and everything between them is in the
library.

**Turn Count up.** One becomes four becomes sixty. Watch the sound stop being a
tone and start being a texture somewhere around eight, and note that nothing was
switched on to make that happen — the individuals simply stopped agreeing with
each other. If it stays too tonal, the control you want is *Spread*.

**Move Rasp.** At the centre each species sits at its measured harmonic-to-noise
ratio. Turned down, a honeybee becomes the clean buzz it is usually drawn as and
never is; turned up it becomes the rasp it actually measures as. This is the
single knob with the most say over whether the result sounds like an insect or
like a synthesiser imitating one.

# How it works

## A wingbeat is a pulse train, twice a cycle

A wing pushes air on the downstroke and again on the upstroke. Each push is a
pressure pulse, and the tone you hear is that train — so the fundamental is the
wingbeat rate and everything above it is a harmonic of it.

{{PLUGIN}} generates the pulse as (1 − u²)² over a width set by *Bite*. That
shape was chosen for two reasons: its first derivative vanishes at both ends, so
narrowing it brightens the buzz instead of adding a click, and its first two
moments are closed forms, so the DC it would otherwise carry is removed exactly
rather than filtered out afterwards.

*Stroke* is how unequal the two half-strokes are. At zero there is one pulse a
cycle and every harmonic is present. Turned up, the second pulse grows until the
two are equal — at which point the odd harmonics cancel completely and the buzz
sits an octave above the wingbeat rate. Real insects are somewhere in between,
and the measured stacks say so: a bumblebee's second harmonic is 11.7 dB *above*
its fundamental and a hornet's 10.7 dB above, while a mosquito's is 0.3 dB below.

## Where the species table comes from

Every species' measured harmonic stack turns out to have the same shape: a peak
somewhere between 150 and 460 Hz, then a fall of 5 to 10 dB per octave. That is
one resonance and one tilt, so four fitted numbers per species are enough — a
one-pole shelf about 300 Hz and one resonant bandpass, crossfaded in.

The fit is against the discrete-time response of the very filters the engine
runs, and against the excitation the engine actually generates, so what is
shipped is a shape the plugin can make rather than one it approximates.
Residuals run 1.5 to 4.0 dB RMS across stacks 25 dB deep, and rendering each
species back out lands its wingbeat rate **within one cent** of the row it came
from.

*Tilt*, *Formant* and *Resonance* move that fit rather than replacing it. At
their centres you have the measurement.

Note that the resonance is an **absolute frequency**, not a multiple of the
wingbeat rate. So raising *Rate* without touching *Formant* is what a real insect
does when it flies harder: the beat changes and the body it radiates from does
not.

## The dragonfly row

One species is shipped as something that is barely a tone, and that is the
measurement rather than a shortcut. Only **8 per cent** of the dragonfly
recordings' frames are periodic at all, against 56 to 94 per cent for every other
species, and they carry the highest crest factor in the library at 32.9 dB.

A dragonfly is a clatter of wings. Forcing it into a clean buzz would have been
inventing a sound the recordings do not contain.

## A swarm is only ever individuals

There is no ensemble stage in this plugin. *Count* spawns that many independent
individuals; each gets its own rate drawn from a distribution *Spread* wide, its
own place across the field, its own distance and its own slow rate wander. That
is all.

*Spread* is the one default in the plugin that is calibrated rather than
measured, and the reason is worth stating. The library cannot measure it: a hive
recording yields one dominant pitch frame by frame, not sixty separate ones, so
what comes out of it is how much *that* pitch moved — which is *Wander*, and is a
different quantity. What the library does measure is where the scatter ends up,
and 150 cents is the value that puts a dozen individuals inside the hive's
measured 0 to 2 dB.

*Wander* itself is measured directly: every species drifts 10 to 24 cents from
one 85 ms frame to the next. A rendered single bee at the default 18 cents
measures 14 cents of frame-to-frame drift, which is what an 85 ms analysis
window sees of a 3 Hz walk.

*Roam* is the same idea applied to level rather than rate. An insect is never a
fixed distance from the microphone: it closes on it and backs off again, and
close in, the inverse square law turns a few centimetres into several decibels.
At a *Count* of one that is most of what separates a fly from a held tone; in a
swarm of sixty the individual drifts are independent and largely cancel, so the
same control reads as the crowd breathing rather than as one insect moving. That
is why the factory presets carry 6 to 10 dB of it on the single insects and half
a decibel on the hives.

The excursion is bounded and the compensation is set so that the loudest moments
stay where *Swarm Level* put them. Turning *Roam* up adds the quieter moments
rather than louder ones, so it cannot overload a patch that was in range without
it; a solo insect gives up about 2 dB of average level at 8 dB of *Roam*, which
is the insect spending time further away. *Roam* does not touch the flyby layer,
which has a trajectory already.

## A flyby is geometry, not an envelope

*Flyby* puts one individual on a straight line past the listener at a closest
distance d₀ and a speed v. Everything then follows from r(t) = √(d₀² + (vt)²):

- the **level** as 1/r,
- the **pan** as the sine of the bearing, scaled by *Sweep*,
- the **air absorption** as a lowpass falling with distance travelled,
- the **Doppler** from the radial velocity.

The two measured numbers fix the trajectory rather than shaping an envelope.
*Rise* is how far the pass comes up over its own approach — a median of 13.3 dB
across the library's clean passes — and *Pass Time* is the width of the level
bump 6 dB down from its peak, a median of 1.37 s. For a 1/r law that happens at
r = 2d₀, so those two between them give d₀ and where the trajectory starts.

The library's passes range from 0.26 to 6.8 s wide, and that spread is the pass
*distance* rather than the speed: a near miss is over in a quarter of a second
whatever the insect is doing.

## Cicadas and crickets are a different instrument

Neither beats its wings. A cicada buckles a tymbal membrane and a cricket draws a
scraper across a file, and what comes out is a resonant body driven in pulses.
That is a carrier and a pulse rate, not a fundamental and a harmonic stack, which
is why it is a separate layer with its own panel.

| | carrier | Q | pulses/s | chirps/s | duty | within-chirp duty |
|---|---|---|---|---|---|---|
| Cicada | 5549 Hz | **13.2** | **268** | 12.5 | 0.48 | **0.79** |
| Cricket | 4518 Hz | **25.8** | **36** | 10.5 | 0.33 | **0.27** |

Twice as sharp and seven times slower is the whole distance between a dry rattle
and a pure whistling trill. Nothing else in the measurement separates them — so
there is deliberately **no Stridulator selector**. A chip that picked between two
*models* would be claiming a difference the recordings do not contain. Both sets
of numbers are shipped as factory presets, and every one of them is a knob you
can move.

*Scrape* is the last column of that table: how much of each pulse period the
insect is actually driving its body. A scraper crossing a file drives the harp
for most of a wing stroke; a tymbal snaps rib by rib and drives it briefly. Model
both as a single click into the resonator — which is what version 0.2.0 did — and
the insect is sounding for 6 per cent of each period, which is neither of them.
At *Scrape* 0 you get that single click back exactly.

*Chorus* is how many are calling, each with its own phase, its own rates and its
own drifting clock. *Scatter* is how unalike they are, and its range is set by
the library rather than chosen — but in two different ways, because the library
bounds the two halves of it differently.

The **carrier** is bounded tightly. A chorus recording measures the *composite*
resonance, and a composite at Q 25.8 cannot come from callers spread much wider
than a thirteenth of their carrier. At the top of the knob the carriers scatter
3 per cent and no further: a species' carrier is its anatomy, and every member of
it shares one.

The **rhythms** are not bounded at all, and they scatter far more — 45 per cent
on the chirp rate, 30 on the pulse rate, 35 on the duty. A clock is not anatomy
and no two callers keep the same time.

## A chorus is not twelve of the same insect

Neither of those is enough on its own, and finding out why is the one thing in
this plugin that was found by ear first and measured afterwards.

Twelve callers within a few per cent of one chirp rate drift in and out of phase
*together*: the chorus throbs at its own chirp rate and drops into near-silence
between. Measured as the tenth percentile of the band's envelope over its median,
nine cricket references sit between 0.16 and 0.85 and version 0.2.0 rendered
0.08 — holes no field has.

And widening the spread does not fix it by itself, because twelve *exactly
periodic* trains are a picket fence however far apart the pickets stand. A real
caller is not periodic: a lone cricket's own chirp-rate peak is 0.73 of its
centre wide. So every caller here carries a filtered random walk on its chirp
rate, at the corner *Wander Rate* already sets for the wing layer — the same
mechanism that keeps a swarm from sounding like a chord.

## Nothing below 80 Hz

The lowest wingbeat in the library is the hornet's 85.7 Hz. Below that, the
recordings carry wind, traffic and handling noise and nothing else, so the
plugin generates nothing there and *Highpass* sits at 80 by default. It is not a
cautious setting; it is the bottom of what an insect makes.

## Distance and air

*Distance* is air absorption plus a downward tilt, and it is bypassed outright at
zero rather than parked out of the way, because no distance means no air to
absorb. *Air* is how much of the top end that distance costs — dry air absorbs
more than humid.

*Space* is kept low by default and its enclosure is fixed small. These are
outdoor recordings, and a field has almost nothing close enough to reflect off;
eight discrete early reflections outdoors is exactly what makes a reverb sound
like a bathroom. *Beehive Interior* is the one factory preset that wants a real
enclosure, and it is inside a box.

# The plugin window

{{PLUGIN}} draws its own window with X11 and Cairo on Linux, and Cairo on the
win32 backend on Windows. There is no toolkit dependency, which is what keeps the
plugin one self-contained file. It is embedded in the host's own window through
CLAP's GUI extension and repainted from the host's timer, and it resizes by
zooming one layout, from half size to four times.

The window is the suite's, in {{PLUGIN}}'s own wasp-yellow theme on a chitin-dark
chassis, with a cloud of insects milling behind the wordmark and a streak drawn
right across whenever one of them passes the listener. The cloud thickens with
how busy the plugin is; the streaks are deterministic in the flyby number, so a
pinned *Random Seed* gives a repeatable picture as well as a repeatable sound.

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
the same controls arranged the other way round: one strip per layer — swarm,
flyby, stridulation, bed — with its level as a fader. They are the plugin's own
parameters, not a second set: moving a fader moves the knob on the panel, and the
host sees the same automation it always would.

`M` mutes a layer and `S` solos one, which is how a preset gets built from
nothing: solo one layer, get it right, bring the next one back. Several layers
can be soloed at once.

Mute and solo are **not** parameters and are not saved with a preset. They work
by holding a layer's level at the bottom of its range and remembering what it
was, so while either is active the preset bar shows a `SOLO ON` or `MUTE ON` chip
— click it to release every hold at once. `SAVE` releases them first, so a muted
layer can never be written into a preset as a silent one, and so does loading a
preset. Touching a held layer's fader also releases the holds rather than
fighting the hand on it.

# Parameter reference

Every range, default, unit and explanation below is read from the plugin's own
parameter table when this manual is built, so it cannot drift from what the
window and the host show.

{{PARAMETER_REFERENCE}}
# The preset library

{{PRESET_LIBRARY}}
# Using it in a host

## Notes

A note starts the swarm and holding it keeps it flying. The ADSR shapes the whole
scene rather than individual insects, and *Release* is what fades it out. The
played note transposes the wingbeat rate and the stridulation carrier together,
so {{PLUGIN}} is playable as an instrument as well as usable as an ambience —
though an insect two octaves up stops being an insect fairly quickly.

*Velocity To Swarm* makes velocity behave like a disturbed swarm: it raises the
number of individuals and how hard they beat together. It works downwards from
full velocity, unlike the usual convention, and deliberately — most hosts send
velocity 1, and at any other convention an untouched note would land a semitone
off the measured species row.

Insects need long notes. Their character is in the statistics of many
individuals and in the slow wander of their rates, and neither is audible in a
short one.

There are 4 voices, each with up to 64 individuals, and *Max Individuals* caps
the total across all of them.

## Automation and modulation

Every parameter is automatable, and host parameter modulation
(`CLAP_EVENT_PARAM_MOD`) is supported globally. Per-note modulation and note
expressions are ignored. Parameter changes are sample accurate, and everything
that reaches an individual — its rate, its excitation, its shaper, and how many
of them there are — is re-derived while the note is held, so an automation ramp
over a bar is a swarm changing rather than one that had to be retriggered.

## Reproducible renders

*Random Seed* at 0 never repeats. Any other value renders the same swarm for the
same notes every time, and the output is byte-identical across runs at both 48
and 96 kHz. Each voice's individuals are seeded from its own slot and key rather
than from the shared generator, so a fixed seed promises the same swarm even when
note events arrive before the seed has been applied.

## CPU cost

Cost is bounded by *Count*, the note count and *Max Individuals*. One individual
is a phase accumulator, two stroke pulses, a band-limited noise source, a
one-pole shelf and one resonator — cheap enough that the default preset's twelve
are a small fraction of one core. Nothing else in the engine varies much:
*Chorus* adds one resonator each, and *Space* is the room model.

Two calibrations run outside the audio path and only when a parameter that
reaches them has moved: one measures what a single individual's excitation and
shaper come out at, the other does the same for one caller. They are what make
*Swarm Level* and *Stridulate Level* mean a level rather than a function of
whichever timbre knob was touched last.

## Rendering and tails

The tail is the release plus whatever the last flyby and the room are still
doing. A flyby runs to the end of its own trajectory, which is a couple of *Pass
Time*s, so a preset with slow passes wants two or three seconds past the end of
the note when bouncing.

## Headroom

Every factory preset renders between −20.8 and −28.8 dBFS RMS with peaks no
higher than −3.5 dBFS, at the default output gain. The one layer to watch is the
stridulation, and how much depends on *Scrape*: at the bottom of that knob a
cricket is 36 clicks a second each ringing for under two milliseconds, so 6 per
cent of it is sounding and it peaks some 18 dB over its own average. Opening
*Scrape* fills those gaps in and the crest factor falls with it — the level stays
put either way, because the layer's normalisation is measured from the same shape
the audio path plays. At a level a swarm would use both are comfortable; pushed
to the top of its knob it will reach the output stage's soft clipper, which is
what the top of that knob is for.

# What is still being fitted

{{PLUGIN}} {{VERSION}} is a first fit, and the measurements say where it is not
there yet. All of this is in the plugin's `TODO.md` with what is known about it.

- **The Hornet row rests on one recording**, 4.2 seconds of it. Its fitted
  resonance sits between the first and second harmonics, which reproduces the
  measured stack faithfully — but means the pitch you hear is an octave above the
  wingbeat. That is what the recording is; it is also the thinnest evidence in
  the table.
- **The Wasp and Dragonfly rows rest on two each.**
- **The flyby's level trajectory is the 1/r law**, which is right for a point
  source in free field and is not quite what the library's ten clean passes
  trace. The suite's rule allows a measured curve shipped as coefficients;
  extracting those ten envelopes is the obvious next step.
- **The pulse train inside a chirp is exactly periodic**, which no tymbal is.
  The *chirp* clock wanders as of 0.3.0 and the pulse clock does not, because the
  references bound the first and say nothing about the second.
- **Nothing measures a swarm's inter-individual rate spread**, which is why
  *Spread* is calibrated rather than fitted.

# Reference and limits

## Where the model comes from

- **The reference library itself**, which supplied nearly everything: the seven
  wingbeat rates, the seven harmonic stacks and the shaper fitted to each, the
  harmonic-to-noise ratio of a single insect and of a hive, the rate wander, the
  flyby's rise and width, and both stridulators' carriers, resonances, click
  rates, chirp rates and duty cycles. The numbers and the scripts that produced
  them are in the plugin's `tools/analysis/`.
- The physical reading behind the layers is the standard one: **flight tone** as
  the pressure pulses of a reciprocating wing, which is why it is a harmonic
  series on the wingbeat rate and why the two half-strokes matter; **turbulence**
  as the air shed by the same wing, which is why it carries the stroke's
  bandwidth rather than being flat; and **stridulation** as a click train ringing
  a resonant body, which is why it has a carrier and a Q and no harmonic stack at
  all. The library decided every number; the physics only decided what to
  measure.
- Where nothing in the library isolates a quantity — *Spread*, *Flutter*, the
  space controls — the model is stated as chosen or calibrated rather than
  fitted, both here and in the chapter above.

## What it does not do

- **No birds, no rain, no wind.** Those are other instruments in the suite.
- **No night-chorus arrangement.** Which species call at which hour, how a chorus
  starts and stops, how one caller triggers its neighbours — that is a scheduler
  rather than a resonator, and it belongs to NightLife. This plugin owns the
  stridulation *mechanism*, not the arrangement of an evening.
- **No sub-bass.** Nothing below 80 Hz is generated, for the measured reason
  above.
- **No audible Doppler**, for the measured reason above. *Speed* is there and it
  is physically correct; it is simply smaller than the insect's own wander until
  you push it past what an insect can do.
