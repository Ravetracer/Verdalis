---
tagline: Synthesised birds
subtitle: CLAP instrument for Linux and Windows
accent: #F2C744
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that generates birds. Every syllable is computed
while the plugin plays — nothing is played back. No two calls are ever alike.

It does not work the way a bird synthesiser usually does. There is no oscillator
with a pitch envelope on it, and no sample of a chirp. The voice is a model of
the **syrinx** itself: a membrane in the bird's throat, driven by two things it
controls independently — the pressure in its air sacs, and the tension of the
muscle that holds the membrane. Below a certain pressure nothing sounds at all;
above it the membrane oscillates. A syllable is one push of pressure and one
pull of tension, and the sound is what the airflow does between them.

That model has two consequences you will feel immediately at the controls.

**A syllable's shape is one knob.** Whether a chirp sweeps up, sweeps down,
arches over or dips in the middle is not four different settings — it is the
*phase* between the pressure gesture and the tension gesture. *Contour* is that
phase. Turning it walks continuously through all four shapes.

**Timbre is not a filter.** *Voice* is how hard the syrinx is driven, as the
ratio of pressure to tension. Low down, the membrane moves almost like a sine
wave and the bird whistles: one harmonic, pure. Turn it up and the oscillation
goes into a relaxation regime, the membrane starts closing against itself, and a
whole harmonic stack appears — which is what a crow is. There is nothing in
between because there is nothing in between in the bird.

Above the syrinx sits the trachea, a closed tube that resonates at `c/4L`, with
the beak both raising that resonance and following the pitch with it the way a
songbird's gape does. Above *that* sits the part that actually makes birds sound
like birds: syllables into phrases, phrases separated by silence, and a flock of
individuals calling and answering.

Every layer came out of measurement. The model, the ten species and the factory
presets were fitted against 58 field recordings — 4268 measured syllables — by
measuring the recording and the plugin's own output with the same analysis code
and moving values until the two agreed. Where a number in this manual comes from
a measurement or a paper, it says so.

> **{{PLUGIN}} {{VERSION}} is an early version.** The engine, the parameter set
> and the window are complete, the self-test passes and every preset meets its
> own targets, but the fit is a first one. *What is still being fitted* at the
> end of this manual lists what is known not to match yet, and it is worth
> reading before concluding that something is broken.

## What is in this manual

*Installing* and *A first sound* are enough to get birds out of the plugin.
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
2. Play a short note. One bird sings one phrase, where you put the note.
3. Now hold a long one. The same phrase fires, and then a flock carries on by
   itself for as long as the note lasts.
4. Open the preset browser by clicking the preset name and try **Single Chirp**,
   then **Dawn Chorus**, then **Crow Call**.

Those two behaviours are the two halves of the plugin, and they are separate
controls. *Shot Level* is the phrase a note fires — the deliberate one, on the
beat you chose. *Flock Level* is the birds that sing unprompted while the note is
held. Turn either right down and you have the other on its own: **Single Chirp**
is the first with no second, and **Dawn Chorus** is the second with no first.

## Four things to try first

**Turn *Contour* slowly, on Single Chirp.** From zero it goes arch, down-sweep,
dip, up-sweep and back to arch, continuously. That one knob is the phase between
the two gestures, and it is the whole shape vocabulary of the instrument. Then
turn *Turns* up a little and the syllable starts folding back on itself.

**Turn *Voice* from bottom to top, on a low pitch.** At the bottom it is a
whistle. Somewhere in the middle a second harmonic appears, then a third, and by
the top it is a rasping stack of nine or ten. Nothing was filtered — the membrane
is being driven harder and has started slamming shut against itself, which is
what a crow's voice physically is.

**Change *Species*.** Ten of them, and each one moves pitch, sweep, length,
harmonic richness, roughness, syllable rate and contour together — because in the
recordings they move together. Every entry is the median of the recordings of
that bird. *Pitch* at its default is the median of the whole library, so
selecting Crow at the default lands at 812 Hz, which is a crow.

**Play a chord, then a melody.** Each note is its own bird with its own flock.
For melodies, load **Melody Bird**: one syllable a note, almost no variation, and
*Pitch* anchored to the loudest moment of the syllable so that what you hear is
the key you pressed.

# How it works

## The syrinx, and the two gestures

Birds do not have vocal cords. They have a syrinx, at the bottom of the trachea,
whose membranes — labia — are pushed apart by air from the air sacs and pulled
back by their own elasticity. The physics is a single equation for how far one
labium has moved from where it rests:

```
x'' + (C·x² − B)·x' + ε·x = 0
```

- **ε** is the elasticity of the tissue, so it sets the frequency: `f = √ε / 2π`.
  The bird controls it with a muscle, and that is why pitch is a *gesture*.
- **B** is the net energy going in — what the airflow adds through the pressure
  between the labia, less what the tissue loses. When B is negative the labium is
  damped and silent; when it crosses zero it starts oscillating. That crossing is
  a *bifurcation*, and it is where a syllable begins and ends.
- **C** stops the labia passing through each other.

So a syllable is two curves over about a tenth of a second: a push of pressure
and a pull of tension. {{PLUGIN}} draws exactly two, and *Skew* says where the
pressure peaks — 0.37 of the way through, because across the library a syllable
rises in 24 ms and falls in 41.

## Why the shape is a phase

This is the part worth understanding, because it explains a control that looks
arbitrary.

Both gestures are curves of the same length. The pitch you hear during the
syllable is whatever the tension gesture is doing *while the pressure gesture is
loud*. Slide one against the other and that changes:

| The two gestures | What the pitch does | The shape |
|---|---|---|
| in phase | rises and falls with the level | an arch |
| a quarter turn apart | sweeps through the loud part | a down-sweep |
| half a turn | dips where the level peaks | a dip |
| three quarters | sweeps the other way | an up-sweep |

That is *Contour*, over a whole turn. It is not a menu of shapes with
crossfades — it is one number, and the shapes are what it looks like at four
places along it.

*Turns* is the other half: how much of a gesture cycle one syllable spans. A
quarter turn is a plain sweep, which is what the library's median syllable is. A
half turn arches or dips once. More than one and the pitch folds back several
times, which is a warble.

*Sweep* stays the whole pitch excursion whatever *Turns* is set to, measured over
the part of the syllable you can actually hear. The two controls do not fight.

## What the references confirm, and what they do not

The claim above is testable, and it is the one place the model could have been
wrong. If a syllable is one turn of two coupled gestures, the correlation between
its envelope and its pitch contour has to be positive for arches, negative for
dips, and near zero for sweeps.

Measured over 3600 syllables, the ordering holds across all six shapes it names:

| shape | arch | down | up | flat | wobble | dip |
|---|---|---|---|---|---|---|
| correlation | **+0.27** | +0.16 | +0.15 | +0.11 | +0.08 | **−0.10** |

29 % of arches correlate above +0.5, against 4 % of dips; 18 % of dips
anticorrelate below −0.5, against 3 % of arches.

It is weaker than two clean sinusoids would give, and the dips especially so.
Two reasons, and only one is the model's fault: the library carries a general
positive bias, because a bird's pitch and level do broadly rise together — which
is the same model with the gestures broadly in phase — and a real syllable is not
one clean cycle of anything. So the phase control is justified and the claim that
it is *sufficient* is not, which is why *Turns* and *Variation* exist. Neither is
in the papers.

## Timbre is the drive

Written another way, the equation has a single shape parameter: `μ = B/√ε`, the
ratio of pressure to tension. That is *Voice*, and it is the only timbre control
in the plugin that is not a filter.

Measured on the plugin's own output with the same estimator that counted the
references' harmonics:

| μ | 0.15 | 0.58 | 1.43 | 3.52 |
|---|---|---|---|---|
| harmonics above −24 dB | 1 | 2 | 4 | 9 |

Across the library, 59 % of syllables have one harmonic and 17 % have six or
more, so both ends of that are real birds. And the three families it splits into
are not something anyone chose:

| Family | Share of the library | Fundamental | Roughness |
|---|---|---|---|
| whistle, ≤ 1 harmonic | 59 % | median 3329 Hz | −30 dB |
| stack, 2–5 harmonics | 25 % | median 1325 Hz | −26 dB |
| rich, ≥ 6 harmonics | 17 % | median 399 Hz | −22 dB |

Harmonic richness, roughness and pitch move together, and downwards — the rich
voices are the low ones. Which is what the model says, since μ falls as ε rises:
a bird singing high cannot sustain a relaxation oscillation, and neither can this
plugin.

## The airflow is one-sided, and that is where the even harmonics are

The equation above is symmetric: flip the displacement and it is unchanged. A
symmetric oscillator has only odd harmonics — energy at f, 3f, 5f and nothing
between — and no amount of drive will make it sound like a crow. This is not a
limitation of the plugin; it is a property of the equation, and the paper it
comes from says so.

The missing physics is that the sound is not the labium moving. It is the **air
that gets past it**, and air only gets past while the labia are apart. For part
of every cycle they are shut and the flow is *zero*. That one-sidedness is where
every even harmonic comes from, and it is the same step that makes a human
glottal pulse rich rather than sinusoidal.

*Voice* therefore does two things at once, as it does in the bird: it drives the
oscillation harder and it closes the labia further, so a whistle barely touches
and a crow is shut for most of every cycle.

*Radiate* is the last part of that. A small source radiates the *rate of change*
of the flow rather than the flow, which tilts the harmonic series up by 6 dB an
octave — and is a large part of why a bird sounds small.

## The tract: a tube and a beak

Above the syrinx is the trachea, closed at one end and open at the beak, so it
resonates at `c/4L` like any such tube. *Tract Length* is L, in centimetres.

The default is 4.9 cm, and it is measured rather than chosen: across the
library's harmonic syllables, the loudest harmonic is **not the first in 80 % of
cases**, and where it sits has a median of 1749 Hz. A tube resonating at 1749 Hz
is 4.9 cm long, which is a plausible mid-sized songbird.

*Beak* is how far it is open, and it does two things. An open beak shortens the
effective tube and damps it, so the resonance rises and broadens. It also makes
the resonance **follow the pitch**: songbirds track the frequency they are
producing with their gape, and a resonance that stays put while the fundamental
sweeps past it hands the loudest harmonic from one to the next in the middle of a
syllable, which sounds like a fault.

*Formant* is how much of that colouring is applied; at zero the syrinx is heard
raw. *Breath* is turbulent air past the labia — and it is also what *starts* the
oscillation, because the noise is injected into the oscillator rather than added
to its output. That is why a syllable's onset is never twice the same.

## Rasp is a real mechanism

A corvid's rasp is not noise and it is not more harmonics. It is the oscillator
being pushed past its harmonic regime into period doubling and chaos, and the
documented route to that in birdsong is the trachea pushing back on the labia.
*Rasp* feeds a scaled tract output into the oscillator, which is that coupling.

The roughest recordings in the library measure 9 dB flatter in the spectrum than
the cleanest, and no amount of harmonics alone reaches it.

## A phrase, and why its gap matters

Above the syllable, the structure is measured. 34 of the 58 references have a
**bimodal** gap distribution: short gaps inside a phrase, long gaps between
them, sitting a factor of six apart — 52 ms against 0.31 s.

That factor is why *Syllable Rate* and *Phrase Gap* are separate controls. One
rate would give a stream; two give a phrase you can hear as a unit.

*Legato* is how much syllables run into each other. It defaults high, because
**70 % of the library's syllable pairs have no silence between them at all**. A
phrase of separated notes is the exception, not the rule.

*Motif* steps the pitch from one syllable to the next, which is what turns a
repeated syllable into a figure. *Variation* is how much each syllable differs
from the last, and it is the difference between a bird and a sequencer.

## A trill is not a modulation

Only **10 of the library's 4268 syllables** carry a periodic wobble of their own
pitch. What the ear calls a trill in these recordings is syllables arriving too
fast to separate — up to 22.8 a second — with no silence between them.

So there is no trill oscillator. A trill is *Syllable Rate* high and *Legato*
high, which is what a trill measurably is. **Nightingale Trill** is exactly that
and nothing else.

Amplitude *pulsing* within a syllable is a separate and real thing: a quarter of
the library's syllables have it, at a median 13 Hz and a depth of 0.82. That is
*Pulse Rate* and *Pulse Depth*.

## The flock is individuals, not a rate

*Birds* sets how many there are, and each one keeps its own pitch offset,
position, distance, syllable length, sweep, contour and voice for as long as the
note lasts. A flock has individuals in it rather than one bird moving about.

They call as a **Poisson process** — the waiting time between phrases is
exponential rather than a clock with jitter on it — at *Flock Rate*, in syllables
a minute, which is how the library was measured (87 to 711, median 273).
*Restless* drifts that density slowly, because no dawn chorus is uniform.

*Answer* is the one behaviour that is not a statistic. Real birds reply to each
other, so with this up, one bird's phrase provokes another from somewhere else a
fraction of a second later. It is most of what makes a flock sound like a
conversation instead of a random process.

## Drumming is not a voice

A woodpecker drumming is *sonation*: a bill against wood, with the syrinx not
involved at all. So it is a separate layer, and it is modelled as a resonance
excited by a contact rather than as an oscillator.

The measurements say two things that look contradictory and are both true. The
strike spectra have a bandwidth of about 935 Hz at a centroid of 1251 — a
resonance that broad has rung out in a fifth of a millisecond. Yet the same
strikes take a measured 8 ms to fall 20 dB. Both hold: the body is broad and the
*contact* is not instantaneous. So *Knock* is the wood's first mode and *Ring* is
how long the contact lasts.

And one measurement contradicts the usual description outright. A woodpecker's
roll is normally said to slow down towards its end. In this library **22 of 35
rolls speed up**: the interval drifts by a median −23 % across a roll, and the
last third runs at 0.79 of the interval of the first. *Accelerate* therefore
defaults to positive.

*Drum Rate* at zero means the woodpecker only answers a note, which is how to put
a single roll exactly where it is wanted. **Woodpecker Drum** is set that way.

## Distance

Birds are point sources, unlike wind or surf, so distance does two things: it
takes the level down by the inverse of the distance, and it takes the top off
through air absorption. *Air* is how much of that top survives — cold dry air
keeps more.

*Depth* is how much the birds differ in distance from each other, and it is most
of what makes a wood sound deep rather than flat. The library's median L/R
correlation is 0.72, noticeably more correlated than wind or surf, which is what
*Width* defaults to reflect: a bird sits somewhere rather than being everywhere.

# The plugin window

{{PLUGIN}} draws its own window with X11 and Cairo on Linux, and Cairo on the
win32 backend on Windows. There is no toolkit dependency, which is what keeps the
plugin one self-contained file. It is embedded in the host's own window through
CLAP's GUI extension and repainted from the host's timer, and it resizes by
zooming one layout, from half size to four times.

The window is the suite's, in {{PLUGIN}}'s own finch-gold theme. Across the
header, behind the wordmark, is a **sonogram** — because that is what the plugin
makes, and a sonogram is what birds are read off. One stroke per syllable,
entering at the right and scrolling off the left, its height and its shape
deterministic in the syllable number: the same syllable always draws the same
stroke. Beside the preset name is the activity meter, showing how many syllables
are sounding and how many have been sung.

Clicking the version label fires one free phrase, with no note behind it. It is
the quickest way to hear what a setting did.

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
| Click the version label | Fire one phrase, with no note |
| `SAVE` | Save the current settings as a user preset |

The line at the bottom of the window shows what the control under the pointer
does — the same one-line explanation that appears in the *What it does* column of
the parameter reference.

# Parameter reference

<div class="paramref" markdown="1">

{{PARAMETER_REFERENCE}}

</div>

# The preset library

{{PRESET_LIBRARY}}

# Using it in a host

## Notes

A note does two things: it fires one phrase at *Shot Level*, and it opens the
gate on the flock at *Flock Level*. Which of those you want is a decision, and
the presets are split roughly half and half between them.

The note is a **pitch**. Middle C leaves *Pitch* alone and every other key
transposes it, so {{PLUGIN}} can be played as an instrument rather than
triggered as a sound effect — **Melody Bird** is set up for exactly that.
*Velocity To Pitch* is worth knowing about: velocity moves the pitch as well as
the level, because a bird calling harder pushes more air past a tighter syrinx.
Both are referenced to full velocity, so *Pitch* means the pitch at velocity 1.

The ADSR gates the **flock** rather than shaping individual syllables — the
syllable envelopes are inside the syllables. A short *Release* thins the flock
out rather than cutting anything off mid-chirp, because syllables already in
flight always finish.

**The phrase a note fires is not gated at all.** It completes however short the
note was, which is what makes a note usable as a one shot: a sixteenth-note
trigger on *Green Woodpecker* still gets all fourteen syllables of the laugh.
The flock around it stops with the envelope.

*Attack* is short by the standards of the rest of the suite, and deliberately: a
bird does not fade in.

There are 16 voices, with 64 syllables, 64 strikes and 64 phrases in shared
pools, so the number of held notes cannot multiply the CPU cost without bound.

## Automation and modulation

Every parameter is automatable, and host parameter modulation
(`CLAP_EVENT_PARAM_MOD`) is supported globally. Per-note modulation and note
expressions are ignored. Parameter changes are sample accurate.

The ones worth automating are *Flock Rate* and *Restless* for a chorus that
builds, and *Pitch* for a bird that moves. Changes to the syllable controls take
effect on the **next** syllable spawned, not the one already sounding — a
syllable is committed when it starts, which is what stops an automation ramp
smearing across a 96 ms chirp.

## Reproducible renders

*Random Seed* at 0 never repeats. Any other value renders the same birds for the
same notes every time — verified byte-identical across two runs at 44.1, 48 and
96 kHz for all of the factory presets.

The flock's layout is deferred to the first scheduling tick rather than being
drawn at note-on, because a note can arrive before the parameter events in its
own block have been applied. That is what makes a fixed seed promise the same
flock even when the host sends the note first.

## CPU cost

Cost is bounded by the pools. Scheduling runs once every 32 samples, which is
0.67 ms at 48 kHz — fine enough for syllables at forty a second — and everything
audible runs per sample.

The expensive control is *Voice*. A hard-driven low voice takes up to eight
integration substeps a sample, because the nonlinear term has to stay inside the
step; a whistle takes one. *Max Voices* is the ceiling on syllables and strikes
sounding at once. *Space Amount* is the room model, and *Birds* costs nothing by
itself — a bird is an identity, not a synthesiser.

## Rendering and tails

A phrase that has started always finishes, so the tail is the release plus the
phrase that was under way when the note went plus whatever the space is still
doing with it. When bouncing, leave a couple of seconds past the end of the note,
and more if *Repeats* or *Phrase Gap* are up.

# What is still being fitted

{{PLUGIN}} {{VERSION}} is a first fit, and the measurements say where it is not
there yet. All of this is in the plugin's `TODO.md` with what is known about it.

**Roughness and harmonic count cannot both be met for three species.** Goose,
Raven and Budgie measure spectrally peaked *and* harmonically rich in the
library, and a relaxation waveform is spectrally flat whether or not any noise
has been added to it. Goose comes out 10 dB rougher than its references, Raven
9 dB, Budgie 6 dB. The likely answer is a second syringeal labium — a bird has
two, controlled independently — which would give the harmonic density without the
flatness.

**Sweep is off by more than 10 % for the three species whose contours turn more
than once**: Woodpecker −33 %, Crow +37 %, Raven −53 %. The six with a plain
sweep are within 10 %.

**A syllable measures 6–33 % longer than *Length*.** The measurement includes
the onset and offset ramps and the oscillator's own tail; the setting is the
length of the gesture. Both are defensible, and the references were measured the
same way — but it means a preset aiming at a particular audible duration sets
*Length* below it.

**Screech does not fit and cannot.** It is the one species with no reference
behind it, and it asks for more harmonics than the band has room for at its own
pitch; the anti-alias clamp refuses and the pitch compensation then overshoots by
38 %. It is kept because a plugin for birds should be able to make a noise no
bird makes.

**The reference library is not evenly sampled.** 25 of the 58 files are ordinary
small birds, while Goose and Raven have one file each — 12 and 6 syllables
respectively. Those two rows of the species table are the weakest numbers in it,
and Raven's harmonic count is in fact the one figure the engine overrides its own
measurement on, for a reason recorded in `tools/analysis/README.md`.

**No preset was fitted band by band against a specific recording.** The fit is
per quantity — pitch, sweep, length, harmonics, roughness — rather than spectral,
so tract errors that a third-octave comparison would catch are currently
invisible.

# Reference and limits

## Where the model comes from

- **Gardner, T., Cecchi, G., Magnasco, M., Laje, R. and Mindlin, G. B.**,
  *Simple motor gestures for birdsongs*, Phys. Rev. Lett. **87**, 208101 (2001) —
  that syllables of diverse acoustic character follow from the phase between two
  gestures. That is *Contour*.
- **Laje, R., Gardner, T. and Mindlin, G. B.**, *Continuous model for vocal
  production in oscine birds*, Phys. Rev. E **65**, 051921 (2002) — the reduced
  two-equation model the voice is built from.
- **Zysman, D., Méndez, J. M., Pando, B., Aliaga, J., Goller, F. and
  Mindlin, G. B.**, *Synthesizing bird song*, Phys. Rev. E **72**, 051926
  (2005) — that the air sac pressure is proportional to the sound envelope and
  the syringeal tension to the pitch, so both gestures can be recovered from a
  recording. Also the observation, quoted in the source, that a richer model is
  needed for species with a wide timbre, which is what the one-sided airflow
  above is.
- **Mindlin, G. B. and Laje, R.**, *The Physics of Birdsong*, Springer (2005).
- **Goller, F. and Suthers, R. A.**, on the syringeal muscles and the
  relationship between muscle activity and frequency.
- **van der Pol, B.** — the relaxation oscillator, and its Liénard form, which is
  what makes the model integrable at audio rates without oversampling.
- **Sabine, W. C.** — the decay time of the space model, as elsewhere in the
  suite.

The papers themselves are not part of the release; they are not ours to
redistribute. `tools/analysis/README.md` in the source tree records every
measurement the library was fitted to, and the ten bugs the measurements found —
each of which had already survived sounding plausible.

## Notes and limits

- Linux and Windows, x86-64. Plugin state is stored little-endian.
- 16 voices, with 64 syllables, 64 drum strikes, 64 phrases and 16 birds per
  voice in shared pools.
- Scheduling runs at a control rate of one update per 32 samples. Everything
  audible runs per sample.
- Host parameter modulation is supported globally. Per-note modulation and note
  expressions are ignored.
- Deliberately built without `-ffast-math`: on x86 GCC that links
  `crtfastmath.o`, which would flip the FTZ and DAZ flags for the entire host
  process.
- Birds only. No insects, no frogs, no wing beats. Several of the references also
  contain wind, traffic and water; those parts were subtracted as a noise floor
  and excluded from the measurements.
- The syllable controls are committed when a syllable starts, so automating them
  changes the next syllable rather than the current one.

## Versioning

Semantic versioning. **MAJOR** is an overhaul: a rewrite of the synthesis model,
or a change that breaks existing presets or saved host state. **MINOR** adds
something — a parameter, a layer, presets. **PATCH** is fixes that add nothing
new.

## License

MIT. The only external dependencies are the CLAP headers, which are MIT
licensed, plus X11 and Cairo for the plugin window.

Everything {{PLUGIN}} produces is computed at run time. It contains no recorded
audio of any kind.

Reference recordings used during development are not part of the release and are
not redistributable.
