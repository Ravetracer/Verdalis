---
tagline: Synthesised birds
subtitle: CLAP instrument for Linux and Windows
accent: #F2C744
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that generates birds. Every syllable is computed
while the plugin plays — nothing is played back. No two calls are ever alike.

It does not work the way a bird synthesiser usually does, and it does not work
the way the first version of it did either.

**A bird syllable is its frequency contour.** Not a pitch envelope on an
oscillator — the actual shape the pitch traces, which for a real chirp changes
direction between two and forty times in fifty milliseconds and slews at up to
four hundred octaves a second. That scribble is the bird. Smooth it out and you
have a whistle.

So {{PLUGIN}} does not compute those shapes. It **measures** them. 67 frequency
contours were extracted from real recordings, fitted as formulas, and they drive
the oscillator directly. *Contour* chooses which one. There is no audio in the
plugin — a contour is forty numbers describing a curve, which is the same thing
van Hunter Adams gets when he reads a cardinal's trace off a spectrogram and
writes down

```
f(x) = -260 sin(-pi x / 5200) + 1740
```

except with forty terms instead of one, because one term cannot draw a scribble.

Above the contour sits a **valve**. Air passes through a bird's syrinx only
while the two membranes are apart, and *Voice* is the fraction of each cycle
they are shut. At the bottom the valve never closes and what comes out is a pure
sine — which is what 59 % of the library's syllables are. Close it and the
airflow becomes a one-sided pulse with the harmonic stack a crow has, evens as
well as odds. That is not a filter; there is nothing between the two settings
because there is nothing between them in the bird.

Above *that* sits the trachea — a closed tube resonating at `c/4L`, with the
beak both raising the resonance and following the pitch with it the way a
songbird's gape does — and above that, the part that makes birds sound like
birds: syllables into phrases, phrases separated by silence, and a flock of
individuals calling and answering.

Every layer came out of measurement, and the measurement is checkable: the
analysis code that measured the recordings also measures the plugin's own output
and compares the two. `tools/analysis/README.md` in the source tree records all
of it, including the first version's failure and the two bugs found in the
measurement code itself.

> **{{PLUGIN}} {{VERSION}} replaced the syllable model.** Version 0.1.0 modelled
> the syrinx from first principles and was fitted to averages; it measured
> correctly on twenty quantities and sounded nothing like a bird. Presets and
> saved state from 0.1.0 do not load meaningfully. Nothing was released at that
> version. *What is still being fitted* at the end of this manual lists what is
> known not to match yet.

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

**Step *Contour* through, on Single Chirp.** Each position is a different real
syllable, measured off a recording. This is the control that decides what kind
of bird it is, more than anything else in the plugin.

**Then sweep *Detail* on the same note.** At 100 % you hear the measured
contour; at the bottom it smooths into a glide. Nothing else changes. That
difference is the whole reason this version of the plugin exists.

**Turn *Voice* from bottom to top, on a low pitch.** At the bottom it is a pure
sine. Somewhere in the middle a second harmonic appears, then a third, and by
the top it is a rasping stack. Nothing was filtered — the valve is closing, and
air only passes while it is open.

**Change *Species*.** Ten of them, and each carries its own set of measured
contours as well as its own pitch register, length, richness, roughness and
rate — because in the recordings those move together. *Pitch* at its default is
the median of the whole library, so selecting Crow at the default lands at
812 Hz, which is a crow.

**Play a chord, then a melody.** Each note is its own bird with its own flock.
For melodies, load **Melody Bird**: one syllable a note, almost no variation, a
flat contour and *Sweep* pulled down, so what you hear is the key you pressed.

# How it works

## The contour is the bird

Put a recording of a chirp on a spectrogram and the syllable is a line. That
line is what you recognise: not its average pitch, not how wide it sweeps, but
its **shape**. Two birds with the same pitch, the same duration and the same
sweep width sound like different birds if their lines differ.

Measured at a third of a millisecond, a real syllable is far busier than it
looks on a normal spectrogram:

| | measured through a 21 ms window | at 0.33 ms |
|---|---|---|
| peak pitch slew | 2.7 oct/s | **20 to 440 oct/s** |
| direction changes | 0.25 | **2 to 40** |
| octaves travelled | 0.35 | **0.2 to 6.8** — against an end-to-end range of 0.2 to 1.0 |

A syllable travels three to seven times further than its range, because it
doubles back. That is the sound of a bird, and it is why the first version of
{{PLUGIN}} — which drew each syllable as one smooth sinusoidal gesture — did not
work, whatever its physics said.

## Where the contours come from

67 of them, extracted from the reference library:

1. Every well-isolated syllable is found and its dominant partial tracked at
   0.33 ms resolution.
2. The pitch track and the level track are each fitted as a cosine series in
   normalised syllable time. **Forty terms** lands within 30 cents of the real
   curve; eight is out by 82; one — which is what a single gesture is — is out
   by more than that before it starts.
3. The contours are clustered per species, and the **medoid** of each cluster is
   kept: one real measured syllable, never an average. Averaging two contours
   that zig-zag out of phase gives a smooth glide, which is exactly the thing
   being avoided.

1641 syllables passed the quality gate; 67 became archetypes. What ships is
**4288 numbers, 17 KB, and no audio at all.**

## The four contour controls

*Contour* walks across the archetypes of the current species, lowest-sitting
first. It is the most important control in the plugin.

*Detail* is how much of the contour's fine motion survives. At 100 % the
measured curve passes through; turn it down and the scribble smooths towards a
glide. It is worth sweeping once on **Single Chirp** just to hear what the
difference is — the bottom of this knob is where the first version of the plugin
lived.

*Sweep* scales how far the contour travels: 100 % is the measured curve, and it
goes to 300 % because the range is wanted.

*Skew* bends the syllable's own time axis, crowding the contour towards the
start or the end. 0.5 plays it at its measured pace — the asymmetry the library
shows, a 24 ms rise against a 41 ms fall, is already in the curve.

*Pitch* transposes the whole thing. The contour is anchored at the syllable's
**loudest moment**, so what you hear is what the knob says; and because the
default is the library median, every species at the default sings in its own
register.

## The valve, and why a symmetric oscillator cannot be a crow

The tone comes from a phase accumulator at the contour's frequency, through a
one-sided valve. *Voice* is the fraction of each cycle the valve is shut:

| Voice → closure | 0.00 | 0.13 | 0.26 | 0.65 | 0.78 | 0.91 |
|---|---|---|---|---|---|---|
| harmonics | 1 | 2 | 3 | 4 | 5 | 5 |

At the bottom the valve never closes and a **pure sine** comes out, which is
what 59 % of the library's syllables are. Close it and the airflow becomes a
one-sided pulse.

That one-sidedness is not a detail. It is where every **even** harmonic comes
from. A symmetric oscillator — and the syrinx equation in the physics papers is
symmetric — has energy at f, 3f, 5f and nothing between, and no amount of drive
makes it a crow: a 234 Hz fundamental comes out as 234, 656 and 1125 Hz. Air
passes only while the membranes are apart, and rectifying at that point is the
same step that makes a human glottal pulse rich rather than sinusoidal.

*Radiate* is the last part of it. A small source radiates the *rate of change*
of the flow rather than the flow, which tilts the harmonics up by 6 dB an
octave, referenced to a fixed frequency so it stays a tilt and not a gain that
changes with the note.

## The partials: reaching for the recording

The valve gives a plausible harmonic stack. What it cannot give is a stack that
**changes shape** as the syllable goes — and real ones do. Measured over 1777
syllables, the balance between the first partials moves **4.4 dB** across a
single syllable. A fixed valve through a fixed tract is flat by construction.

So each archetype also carries six measured amplitude curves, one per harmonic,
and *Partials* crossfades the valve into them. They are phase-locked to the same
oscillator, because a harmonic source is.

Two things to know about it:

**It is scaled by how much the measurement is worth.** Each archetype records
what share of its own energy fell inside the harmonic comb. A syllable that was
inharmonic, or had a second bird in it, has a low share and barely responds —
which is honest, rather than asserting a balance that was never measured.

**It thins the corvids, and that is the library's fault rather than the
engine's.** The contour quality gate selects for tonality, so the archetypes are
each species' *cleanest* syllables. Turn *Partials* up on a Crow and its timbre
starts evolving the way a real one's does — but its harmonic count falls, because
that particular syllable really did have fewer harmonics than a crow's average.
The corvid presets ship at 35 % for that reason. On the whistlers and warblers,
turn it up.

## Three families, found rather than chosen

Sorting every syllable in the library by harmonic count alone splits it into
three groups that barely overlap:

| Family | Share | Fundamental | Roughness |
|---|---|---|---|
| whistle, ≤ 1 harmonic | 59 % | median 3329 Hz | −30 dB |
| stack, 2–5 harmonics | 25 % | median 1325 Hz | −26 dB |
| rich, ≥ 6 harmonics | 17 % | median 399 Hz | −22 dB |

Harmonic richness, roughness and pitch move together and downwards: the rich
voices are the low ones. So the ten species are not ten presets of the same
thing — they sit in genuinely different places, and choosing one moves pitch,
length, richness, roughness, rate and the whole archetype set at once, because
in the recordings those move together.

*Breath* is turbulent air past the membranes. It is calibrated: 13 dB of
spectral flatness per decade, with 5 % putting a sparrow on the library's median
roughness of −28 dB.

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

## Rasp is irregularity, not more harmonics

A corvid's rasp is a contact that is never the same twice. *Rasp* varies the
valve's closure from one cycle to the next, which is broadband in a way that no
harmonic stack is — the roughest recordings in the library measure 9 dB flatter
in the spectrum than the cleanest, and stacking harmonics does not get there.

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

What the ear calls a trill in these recordings is syllables arriving too fast to
separate — up to 22.8 a second — with no silence between them. So there is no
trill oscillator: a trill is *Syllable Rate* high and *Legato* high.
**Nightingale Trill** is exactly that and nothing else.

The fine motion *inside* a syllable is real and abundant — two to forty
direction changes — but it is not periodic, which is a different thing, and the
measured contours carry it directly.

Amplitude pulsing within a syllable is also real: a quarter of the library's
syllables have it, at a median 13 Hz and a depth of 0.82. That is now in the
measured level contours, which is why *Pulse Depth* defaults to nothing — it is
there to push further, not to supply what is missing.

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

{{PLUGIN}} {{VERSION}} is a first fit of a new model, and the measurements say
where it is not there yet. All of this is in the plugin's `TODO.md`.

**The valve saturates around five harmonics** and the noisiest references
measure twelve. Closing it further aliases. The likely answer is a second
syringeal side — a bird has two, controlled independently, which is how some
species produce two notes at once.

**Roughness cannot be met for the rich species.** Crow comes out 8 dB rougher
than its references and Raven 17 dB. A harmonic stack is spectrally flat whether
or not any noise is present, and the flatness measure cannot tell the two
apart — so this may be a limit of the measurement rather than of the engine.

**Goose has one contour and Raven four.** Only one goose and one raven recording
exist in the library, and few of their syllables passed the quality gate, so
*Contour* does nothing on a Goose. More references for the waterfowl and the
corvids would help more than any further fitting.

**Nothing morphs between contours.** *Contour* steps from one measured curve to
the next. Interpolating them naively would give smooth glides, since two curves
that zig-zag out of phase average to a straight line — the exact failure this
version was built to avoid — so a continuous control needs the curves aligned
first.

**Screech does not fit and cannot.** It is the one species with no reference,
borrowing the library's most extreme contours, and its measured pitch runs 54 %
low because those curves swing so far that a fundamental estimate is
meaningless. It is an effect, not a bird.

**The quality gate still throws away most of the library.** Relaxing it in this
version widened every species' set — Goose went from one contour to three — but
most of the dense multi-bird recordings are still rejected, because there the
tracker follows two birds at once.

**Warbler and Goose come out about half the length their table says.** Both have
archetypes of very mixed duration, and stretching a short curve to a long
*Length* turns its own amplitude modulation into separate notes.

# Reference and limits

## Where the model comes from

- **Adams, V. H.**, *Birdsong synthesis* and the Cornell ECE 4760 birdsong lab —
  the method this plugin uses: read the frequency contour off a spectrogram, fit
  a formula, drive an oscillator with it. `f(x) = -260 sin(-pi x / 5200) + 1740`
  is his cardinal swoop, and *Contour* is that idea automated over 58
  recordings with forty terms instead of one.
- **Gardner, T., Cecchi, G., Magnasco, M., Laje, R. and Mindlin, G. B.**,
  *Simple motor gestures for birdsongs*, Phys. Rev. Lett. **87**, 208101 (2001)
  — that a syllable is two coupled gestures with a phase between them. The
  reference library confirms the ordering that predicts, which is why forty
  cosine terms describe a syllable so well; it is *not* enough to make two
  sinusoids draw one.
- **Laje, R., Gardner, T. and Mindlin, G. B.**, Phys. Rev. E **65**, 051921
  (2002) — the reduced two-equation syrinx model.
- **Zysman, D., Méndez, J. M., Pando, B., Aliaga, J., Goller, F. and
  Mindlin, G. B.**, *Synthesizing bird song*, Phys. Rev. E **72**, 051926 (2005)
  — that air sac pressure tracks the sound envelope and syringeal tension the
  pitch, so both can be recovered from a recording. Also the observation,
  quoted in the source, that a richer model is needed for species with a wide
  timbre — which is the one-sided airflow above.
- **Mindlin, G. B. and Laje, R.**, *The Physics of Birdsong*, Springer (2005).
- **Sabine, W. C.** — the decay time of the space model, as elsewhere in the
  suite.

The papers themselves are not part of the release; they are not ours to
redistribute. `tools/analysis/README.md` in the source tree records every
measurement, the eleven bugs the measurements found in the engine, and the two
they eventually found in themselves.

## Notes and limits

- Linux and Windows, x86-64. Plugin state is stored little-endian.
- 16 voices, with 64 syllables, 64 drum strikes, 64 phrases and 16 birds per
  voice in shared pools.
- 71 measured contours, 9656 coefficients, 38 KB. No recorded audio of any kind.
- 60 s of *Dawn Chorus* — twelve birds, 48 voices — renders in 1.07 s, about 56
  times realtime.
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
- The contour tables are rendered once when the plugin is activated, not per
  note, and shared between instances.

## Versioning

Semantic versioning. **MAJOR** is an overhaul: a rewrite of the synthesis model,
or a change that breaks existing presets or saved host state. **MINOR** adds
something — a parameter, a layer, presets. **PATCH** is fixes that add nothing
new.

While the version is below 1.0 the model itself is still settling, and 0.2.0
replaced 0.1.0's synthesis outright. Presets and saved state are not carried
across a 0.x bump.

## License

MIT. The only external dependencies are the CLAP headers, which are MIT
licensed, plus X11 and Cairo for the plugin window.

Everything {{PLUGIN}} produces is computed at run time. It contains no recorded
audio of any kind — the measured contours are coefficients describing two
curves, in the same sense that `f(x) = -260 sin(-pi x / 5200) + 1740` is.

Reference recordings used during development are not part of the release and are
not redistributable.
