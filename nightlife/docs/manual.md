---
tagline: A synthesised night
subtitle: CLAP instrument for Linux and Windows
accent: #9FB4E8
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that generates a night. A wolf howling across a
valley, an owl in a wood, a fox screaming in a field, a loon over a lake, a pond
full of frogs, a field of crickets, and the quiet that all of it sits on. Every
sound is computed while the plugin plays — nothing is played back, and no two
takes are alike.

A night is not one texture, so {{PLUGIN}} is not one mechanism. It has four
layers and they are built from three different kinds of measurement, because the
things they model are different kinds of thing.

**A call is its frequency contour.** Not a pitch envelope on an oscillator — the
actual shape the pitch traces. The plugin does not compute those shapes, it
*measures* them: 48 contours were extracted from real recordings of wolves,
owls, foxes and a loon, fitted as formulas, and they drive the oscillator
directly. *Contour* chooses which one. A wolf's vibrato is not a control here;
six of the eight wolf contours carry one at 2 to 6 Hz because the wolves that
were recorded did.

**A croak is not a contour.** A frog is a pulse train through a body resonance —
10 to 69 pulses a second, and the rate is the species. Eight recordings were
measured for their rate, pulse count, both resonances and envelope, and *Croak*
walks across them.

**A chorus of frogs is more regular than random.** Every other event in the
Verdalis suite is spawned as a Poisson process. Measured, a frog chorus is not
one: its arrivals are *under*-dispersed, strongly so over a few seconds. So the
pond here keeps one rhythm and each frog takes a place in it, which is what a
chorus of real frogs does.

**The insects and the night itself are noise, measured.** A cricket band is
narrow, high and steady; the night behind everything is the third-octave curve
of the quietest parts of 45 field recordings, solved onto a filterbank.

> **{{PLUGIN}} {{VERSION}} is the first release.** Nothing before it was
> published, and no preset or saved state from an earlier build exists.

## What is in this manual

*Installing* is the boring part. *A first sound* is five minutes. *How it works*
is what the four layers actually do and why they are built the way they are.
*The parameter reference* lists every control with its range and default, and is
generated from the plugin's own table, so it cannot drift from the build. *The
preset library* is the same for the factory presets.

# Installing

{{PLUGIN}} is one self-contained `.clap` file plus its `presets` folder, and a
`.vst3` bundle beside it. There is nothing to register and no runtime to
install.

## Linux

Copy the plugin folder into your CLAP directory:

```
~/.clap/{{PLUGIN}}/{{PLUGIN}}.clap
~/.clap/{{PLUGIN}}/presets/
```

and, for the VST3, `~/.vst3/{{PLUGIN}}.vst3`. A system-wide install goes in
`/usr/lib/clap` and `/usr/lib/vst3`.

## Windows

```
C:\Program Files\Common Files\CLAP\{{PLUGIN}}\
C:\Program Files\Common Files\VST3\{{PLUGIN}}.vst3
```

Copy the whole `{{PLUGIN}}` folder so that `{{PLUGIN}}.clap` and `presets\` sit
inside it, and rescan.

## Hosts

Any CLAP host: Bitwig Studio, Reaper, Qtractor, or the reference `clap-host`.
{{PLUGIN}} appears as an instrument under the vendor **Ravetracer**. In a VST3
host it appears the same way; the plugin's own preset browser lists the factory
presets there too, though the host's own browser will not.

## Your own presets

*SAVE* in the plugin window writes to your user preset folder:

| Linux | `$XDG_CONFIG_HOME/{{PLUGIN}}/presets`, or `~/.config/{{PLUGIN}}/presets` |
| Windows | `%APPDATA%\{{PLUGIN}}\presets` |

They are plain text — parameter names and values — and can be edited, copied
between machines and kept in version control.

## Building from source

```sh
./install.sh            # configure, build, self-test, install to ~/.clap
./install.sh --vst3     # and the VST3
```

# A first sound

1. Load {{PLUGIN}} on an instrument track.
2. Choose **Wolf Valley** in the preset bar.
3. Hold a long note. Middle C is the pitch the preset was written at; every
   other key transposes the whole night.

The note does two things. It fires one phrase immediately — that is *Shot Level*
— and it brings up everything that happens unprompted while it is held: the
pack, the chorus, the insects and the bed. Let go and the layers fade over
*Release*, but a call already under way always finishes. A howl is never cut in
half.

## Four things to try first

**Turn *Distance* up.** It is the most effective control in the plugin. A wolf
at the far end of a valley is most of what the word "howl" means, and distance
is what makes one — the level falls, and the air takes the top off the call.

**Open the MIXER.** Four layers is one more than anything else in the suite has,
and balancing them is most of what building a night from scratch is. Each fader
has a mute and a solo; solo the chorus, get it right, bring the rest back.

**Turn *Answer* up on Pack Answering.** One animal calls and another replies from
somewhere else a moment later. That behaviour, and not the voice, is what makes
a group of wolves sound like a group.

**Turn *Regularity* down on Pond Chorus.** At zero the pond becomes a Poisson
process — the way every other plugin in the suite spawns events — and it is
immediately audible as *wrong*: the croaks clump. That difference is a
measurement, not a preference.

# How it works

## The contour is the animal

A wolf howl is one pitch held for a second or more, with a slow vibrato on it
and a fall at the end. Everything that makes it a wolf rather than a synthesiser
is in how that line actually moves, and the line is not a curve anybody would
guess: it wanders, it breaks, it is not symmetric, and its vibrato is not a
sine.

So {{PLUGIN}} measures it. `tools/analysis/contours.py` pulls the instantaneous
frequency and level of every well-isolated call in the reference library out of
the recording, fits each as a cosine series in normalised call time, clusters
them per caller and keeps the medoids — a medoid rather than an average, because
averaging two contours that move out of phase gives a smooth glide, and a smooth
glide is exactly what a synthesised howl sounds wrong as.

**No audio is stored.** A contour is a formula: the same thing as

```
f(x) = -260 sin(-pi x / 5200) + 1740
```

read off a spectrogram by hand, with one term per 18 ms of call instead of one
term in total, because a real call turns direction far more often than once.

Eight archetypes per caller, 48 in all, 43 kB.

## The term count is per second, not per call

This is the one place {{PLUGIN}} had to depart from ChirpParade, which does the
same thing for birds. Its fit budget is a fixed 96 terms per syllable, which is
right when everything is between 12 and 900 ms long. Here the calls run from a
75 ms pip to a 3.1 s whinny, and a fixed budget is then a hidden duration
filter: at 96 terms the wolf reproduces 97 % of its measured motion and the
scops owl 253 % of its — the series ringing between the points it was fitted at.

55 terms per second of call holds all six callers between 77 % and 115 %. Each
archetype carries its own count.

## The four contour controls

*Contour* chooses which measured call, walking across that caller's eight from
the lowest-sitting to the highest. *Sweep* scales how far it travels — 100 % is
the measured curve exactly. *Detail* is how much of its fine motion survives; at
the top the measured curve passes through, and turning it down smooths the
contour towards the glide a description of a howl would have given. *Skew* bends
the call's own time axis.

*Length* scales the archetype's **own** measured duration rather than replacing
it, so a caller's spread of call lengths survives. The wolf archetypes run 311 ms
to 2.8 s; stretching them all to one target is what a summary statistic would do.

## The valve

Air passes through a larynx only while the folds are apart. *Voice* is the
fraction of each cycle they are shut. At the bottom the valve never closes and
what comes out is very nearly a pure tone — which is what a hoot is, measured at
one harmonic and −34 dB of roughness. Close it and the airflow becomes a
one-sided pulse with the harmonic stack a scream has, evens as well as odds.
That is not a filter, and there is nothing between the two settings because
there is nothing between them in the animal: a symmetric oscillator has no even
harmonics at all, whatever it is driven with.

*Rasp* is irregularity of that closure, cycle by cycle. The break at the top of
a howl and a vixen's scream are both this rather than more harmonics — the fox
group measures −23 dB of roughness against every other caller's −32 to −34, and
no amount of extra harmonics is the same thing as noise.

## The partials: reaching for the recording

*Partials* crossfades the synthetic valve into the archetype's **own measured
balance** between its first six harmonics, across the call. It is the one
control that reaches for the recording itself: a fox opens and closes its
harmonic stack within a single scream, and a fixed valve through a fixed tract
cannot do that at all.

It is scaled by how much of that call's energy the harmonic measurement actually
accounted for, so an archetype with a second animal in it does not pretend to
know.

## The tract

The tube above the larynx, closed at one end and open at the mouth, resonating
at `c/4L`. *Throat* is its length — 19 cm puts the resonance at 450 Hz, where the
library's hoots and howls sit. *Muzzle* opens the mouth: it raises the resonance,
damps it, and makes it follow the pitch, which is what an animal that opens its
jaw as it rises actually does. A wolf does exactly that.

## A phrase, and a pack

*Calls* is how many calls a phrase holds and *Call Rate* how fast they follow
each other, onset to onset. The library's median is three calls about a second
apart, and the spread is the whole point: a tawny owl's phrase is two notes, a
wolf's is one held note, a barred owl's eight, a scops owl's fifty-seven of the
same pip.

*Legato* stretches each call towards filling its slot. It defaults to 32 %
because that is the measured share of consecutive calls in the library that
touch. ChirpParade measures 70 % for birds and defaults accordingly; a night is
not a dawn chorus.

**The pack is individuals, not a rate.** *Animals* draws that many, each with its
own pitch, position, distance, voice, tempo and level, held for as long as the
note is. *Pitch Spread* is wide by default because wolves in a chorus avoid each
other's pitch — which is why a pack sounds like more animals than it holds.
*Answer* is how often one replies to another shortly after, and it is most of
what makes a pack sound like a conversation rather than a random process.

## The chorus is a pulse train

A croak is not a pitch contour and is not modelled as one. The frog drives its
vocal sac in bursts, and what arrives is a train of pulses, each ringing the sac
and the mouth. *Pulse Rate* is the species — it is the first thing a field guide
lists, and it is what a listener hears as the difference between a knock, a
creak and a rattle. Measured across the library: 10 to 69 Hz.

*Croak Pitch* sets the first resonance. The second one moves with it, at the
ratio that row of the table measured — about half the first, across the whole
library. A croak with only one resonance is a beep.

## Why the pond keeps time

The Verdalis suite spawns events as a Poisson process everywhere else. Measured,
a frog chorus is not one. The statistic is the Fano factor — the variance of the
arrival count in a window over its mean, which for a Poisson process is exactly
1.0 at every window:

| window | 50 ms | 250 ms | 1 s | 4 s |
|---|---|---|---|---|
| a measured chorus | 0.90 | 0.81 | 0.59 | 0.30 |
| a Poisson process | 1.00 | 1.00 | 1.00 | 1.00 |

A frog chorus is *more* regular than random. And giving each frog a clock of its
own does not reproduce it: enough independent clocks add up to a Poisson process
however regular each one is. The regularity belongs to the **pond**. So the
chorus keeps one period and each frog takes a place in it, which is also what
real frogs do — they call in turn.

*Regularity* is the control between the two. At the top the pond has a rhythm;
at the bottom it is the Poisson process the rest of the suite uses, and the
difference is audible immediately.

## The insects

A cricket is a narrow band of noise with a trill on it. There is no pitch
contour and no body resonance in one, and none is modelled: *Insect Pitch* is
the carrier, *Insect Band* its width, *Trill Rate* and *Trill Depth* the pulsing,
*Shimmer* how much the individuals differ.

This is the weakest measurement in the plugin and it is worth saying so plainly.
The reference library has no recording of insects on their own — what is measured
is the cricket band *behind* a scops owl and behind two frog choruses, isolated
by being narrow, high and steady where everything else in those files is none of
the three. Two recordings carry a prominent trill rate and they disagree by an
octave, 33 and 49 Hz, which is plausibly a katydid and a cricket.

## The bed

The night behind everything: wind in leaves, distant water, a chorus too far
away to be individual animals. Its shape is measured — the third-octave curve of
the quietest third of the frames of all 45 references — and solved onto an
eight-band octave filterbank.

*Bed Tilt* turns that curve about 500 Hz, where it peaks. *Bed Motion* drifts its
level slowly, so it breathes rather than sitting still.

## Distance

Distance does two things at once and both are physical: the level falls with
distance, and the air absorbs the top of the sound. *Depth* is how much the
animals differ in it, which is most of what makes a night sound deep rather than
wide. *Air* is how much of the high end survives — cold dry air keeps more.

# The plugin window

{{PLUGIN}} draws its own window with X11 and Cairo on Linux, and Cairo on the
Win32 backend on Windows. There is no toolkit.

Drag a knob to edit it, double-click to reset it to the default, shift-drag for
fine control, and click a value to type one. An enum chip steps through its
choices when clicked on its left or right half.

The preset bar holds the browser, *SAVE*, and *MIXER*. The mixer is an overlay
with one strip per layer: its level as a fader, a mute and a solo. **Mute and
solo are not parameters** — a preset is a set of parameter values, and a mute
that reached the parameters would be saved as a layer that comes back silent. A
`SOLO ON` or `MUTE ON` chip stays in the preset bar for as long as a hold is
active, because a forced-down layer that looks like a saved one is the trap.

The header draws a night sky over a treeline, and every call sends a ring out
from a point on the horizon. You hear a wolf long before you could see one, and
what reaches you is a wavefront.

# The parameter reference

{{PARAMETER_REFERENCE}}

# The preset library

{{PRESET_LIBRARY}}
