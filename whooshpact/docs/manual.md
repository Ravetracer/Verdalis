---
tagline: Synthesised transitions and impacts
subtitle: CLAP instrument for Linux and Windows
accent: #FF3C97
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that makes the sounds a track is glued together
with: whooshes, risers, braams, booms, impacts, accents and downshifters. It
contains no samples. Every gesture is built from noise, oscillators, filters and
envelopes while the plugin plays.

And no two are alike, which is the point of it.

## Why not just use samples

Because a sample plays the same transition every time. Four whooshes in a row
out of one file is one sound repeated four times, and the usual way round it is
to keep twenty variants of the same whoosh on disk and remember which one went
where.

Here every note draws its own span, peak position, filter cutoff, sweep, pitch,
level, pan, flutter rate and transient tone from a distribution around the
settings on screen. *Variation* decides how far — and setting it to zero makes
the plugin bit-exactly reproducible, which is what a transition that has to land
on a picture cut needs.

## The one number the design rests on

The reference library is 211 production sounds filed in six families. *Peak* —
where the loudest moment sits, as a fraction of the whole gesture — separates
them more sharply than anything else that was measured:

| family | peak sits at | span | <100 Hz | centroid sweep |
|---|---|---|---|---|
| booms | **2.4 %** | 5.50 s | 0.97 | −0.12 oct |
| downshifters | **2.4 %** | 4.57 s | 0.98 | −1.07 oct |
| accents | **4.3 %** | 3.55 s | 0.69 | −0.69 oct |
| impacts | **6.0 %** | 4.34 s | 0.92 | −0.63 oct |
| braams | **14.8 %** | 5.63 s | 0.74 | **+0.60 oct** |
| transitions | **32.8 %** | 3.71 s | 0.62 | −0.67 oct |

A hit peaks in the first few per cent of itself. A whoosh peaks a third of the
way in. That is the difference between the two, and it is one knob.

So {{PLUGIN}} has no modes. The gesture is five numbers — *Span*, *Peak*,
*Hold*, *Rise*, *Fall* — and those five reproduce every measured envelope
contour in the library to within the library's own spread. *Type* selects a
family's measured *spectral* profile and nothing else.

## What these references are, and why it matters

Every other plugin in the Verdalis suite is fitted against recordings of the
world, and the physics is there to be found. These are not recordings of
anything: they are finished production sounds, made by somebody else out of
synthesis and processing. There is no underlying object to model, and pretending
otherwise would mean inventing a mechanism the material does not contain.

So the measurement here describes the *target* rather than the mechanism. That
is a real departure from the rest of the suite and it is worth knowing before
reading the rest of this manual. The suite's other rule is unchanged: nothing is
sampled, and every layer is computed while the plugin plays.

## What is in this manual

Installing it, getting a first sound, how the model works and why it is built
this way, the window, then a generated reference for every parameter and every
factory preset. The last two chapters are honest about what does not fit yet and
where the model comes from.

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

Then rescan plugins in your host. X11 and Cairo are the only external
dependencies of the plugin window, and any Linux machine that can run a DAW has
both.

The VST3 goes to `~/.vst3/{{PLUGIN}}.vst3`, as a folder, not a file.

## Windows

CLAP hosts scan the common CLAP folder. Copy the plugin folder into:

```
C:\Program Files\Common Files\CLAP\{{PLUGIN}}\
```

so that `{{PLUGIN}}.clap` and `presets\` sit inside it, and rescan. There are no
DLLs to install beside it — the window and its Cairo are linked in. The VST3
bundle goes into `C:\Program Files\Common Files\VST3\`.

> **Keep the plugin and its presets together.** The plugin finds its factory
> presets by looking for a `presets` directory next to its own binary. Moving
> the `.clap` file on its own leaves it with no factory presets in the *host's*
> browser. Its own browser still has them: the library is embedded in the
> binary.

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
`.whooshpactpack` — so a library can be handed to somebody else, or moved between
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
`~/.clap/{{PLUGIN}}/`. `--vst3` builds and installs a VST3 as well.
`{{PLUGIN}}_PREFIX=/some/where ./install.sh` overrides the destination, and
`-DCLAP_INCLUDE_DIR=/path/to/clap/include` points the build at a CLAP checkout
it cannot find by itself.

# A first sound

1. Load {{PLUGIN}} on an instrument track.
2. Play middle C. One note is one gesture: it plays out over its own *Span*
   whether the note is held or not.
3. Open the preset browser by clicking the preset name in the header and try
   **Simple Whoosh**, then **Deep Impact**, then **Big Horn**.

Those three are the span of the instrument. The first is noise through one
filter; the second is a struck low end with a transient on it; the third is a
detuned oscillator stack that swells and opens.

## Notes are triggers, not gates

This is the one thing about {{PLUGIN}} that is not like a normal synthesiser.

The plugin cannot know how long a note will be when it starts, so the note does
not decide the length of the sound — *Span* does. A note-on starts a gesture and
the gesture plays out.

Letting go early still works: the ADSR runs on top of the gesture, so a short
*Release* cuts a gesture short when the note ends. With *Sustain* at 1 and a
long *Release*, which is what most of the factory presets have, the note length
does not matter at all.

## Three things to try first

**Move *Peak*.** Load *Simple Whoosh* and pull *Peak* down towards zero. The
whoosh turns into a hit, because that is the only structural difference between
the two. Everything else on the panel stays where it is.

**Turn *Variation* up and play the same note five times.** At 0.35, which is the
default, each one is noticeably its own sound and unmistakably the same preset.
At 1.0 they are a family rather than a sound. At 0 they are identical, every
time, on every machine.

**Turn *Flutter* up on a downshifter.** Load *Analog Fall* and move *Start
Speed* and *End Speed* apart. The chopping glides exponentially from one to the
other across the gesture — 3 Hz at the top to 14 Hz at the bottom in that
preset — which is the thing that is genuinely awkward to do with an LFO.

# How it works

## The gesture

Everything is driven by one clock that runs from 0 to 1 across *Span*. Five
numbers shape it:

- ***Peak*** — where the loudest moment sits.
- ***Hold*** — how long it stays there.
- ***Rise*** — the curvature of the approach, as an exponent. 1 is a straight
  ramp; above 1 holds back and then arrives.
- ***Fall*** — the curvature of the decay, on the same scale.

The measured transition contour, on a normalised time axis:

```
t      0.00  0.07  0.13  0.20  0.27  0.33  0.40  0.47  0.53  0.60
env   0.012 0.065 0.162 0.348 0.519 0.560 0.528 0.388 0.299 0.188
```

That is `(t/0.33)^1.8` on the way up and about `(1−u)^2.5` on the way down. The
defaults are exactly those numbers, and the same five fit all six families.

A gesture whose *Span* is longer than what you can hear of it is normal. A boom
with *Peak* at 0.02 and *Fall* at 2.2 is below −45 dB long before its span ends;
the span is the clock, not the audible length.

## Type is a spectrum, not a mode

*Type* carries two measured numbers per family and nothing else: the slope of
the family's octave-band curve above 125 Hz, and how far 63 Hz stands over
125 Hz. Both are relative to Transition, which is the broadest curve in the
library and therefore the neutral setting.

| family | slope above 125 Hz | 63 Hz over 125 Hz |
|---|---|---|
| accents | −3.73 dB/oct | +7.4 dB |
| booms | −9.13 | +12.5 |
| braams | −5.23 | +4.0 |
| downshifters | −10.18 | +10.6 |
| impacts | −4.43 | +7.0 |
| transitions | −4.08 | +2.3 |

*Blend* crossfades to the next family, so the six are a continuum rather than
six settings.

It deliberately does **not** set the shape in time. If it did, choosing a type
would fight the preset that had just set *Peak* and *Hold*, and the five gesture
numbers are exactly the ones a user wants to reach for.

One caveat on how the numbers are used: a true −10 dB/octave would take 60 dB
off the top of a boom and leave nothing to mix with. The plugin applies 2.2
octaves' worth of the measured slope through a single high shelf. The
measurement is a statement about balance, and that is how it is spent.

## Four layers, and two ways of being driven

**Air** and **Tone** follow the gesture envelope. **Sub** and **Hit** are
*struck* at a point in the span and then decay on their own.

That division is not a convenience. A boom is one event with a decay, not a
curve with a low end under it, and the measured per-band decays say so: an
impact's 63 Hz band takes 0.78 s to fall 20 dB where its 8 kHz band takes 0.21.
Those are two different envelopes on two different layers, and trying to make
one gesture curve carry both is what makes a synthesised impact sound like a
snare.

### Air

Noise of a chosen colour through one filter, swept across the gesture.

Six colours, each white noise with one filter on it, which is what those names
have always meant: White is flat, Pink falls 3 dB/octave, Brown 6, Blue and
Violet rise by the same, and Green is a broad mid emphasis around 500 Hz. Each
is normalised to the RMS of white, so the *Noise* knob changes colour and not
level.

*Resonance* narrows the filter as well as sharpening it. At 0 it is a plain
lowpass and the whoosh is a wall of air closing down; at 1 it is a resonant
bandpass and the whoosh has a pitch to follow. The two go together because that
is how the ear reads it: a narrow moving band is a pitch, a broad one is wind.

*Width* is decorrelation. At 1 the two channels are independent noise, which is
as wide as a noise source gets; at 0 they are the same signal. The library's
transitions measure a median L/R correlation of 0.60, and the default renders at
0.61.

### The sweep happens late

A transition's spectral centroid is flat for its first third and then falls 0.80
octaves:

```
t       0.00  0.07  0.13  0.20  0.27  0.33  0.40  0.47  0.53  0.60
oct     0.00  0.04  0.14  0.02  0.04 −0.05 −0.28 −0.53 −0.61 −0.56
```

An envelope that moves the filter evenly across the gesture sounds like none of
these. *Curve* is where that comes from: at 1 the sweep is spread evenly, above
1 the cutoff sits still and then moves late.

*Sweep* is in octaves of **cutoff**, and a centroid does not move as far as the
cutoff carrying it — what is left below the corner does not move at all. The
default of −1.6 octaves of cutoff renders as −0.56 octaves of centroid, against
the library's −0.67.

Braams are the one family that rises, by +0.60 octaves, and every braam preset
therefore has a positive *Sweep* and a positive *Glide*.

### Tone

Three oscillators, detuned against each other, with a pitch glide across the
gesture on the same *Curve* warp the filter uses.

Saw and Square are band-limited with polyBLEP. That is not fastidiousness: a
braam climbs an octave or more, and a naive saw edge at the top of that climb
folds back audibly. Supersaw is the same stack with the detune spread three
times wider.

The measured median fundamental of the library's pitched families is 41–49 Hz —
booms at 49, braams at 47, downshifters at 41, all within a tone of G1. That is
where *Pitch* and *Sub Pitch* default. The played note transposes from middle C,
so a preset plays as it was fitted when middle C is played.

### Sub

A driven sine with a pitch drop, struck at the gesture's peak.

*Drop* is in semitones and happens across the layer's own decay rather than
across the gesture: a boom's pitch falls as it dies away, not as the whole
gesture runs. *Drive* is saturation on the low layer alone — a pure sine at
45 Hz disappears on a small speaker and the harmonics this adds are what carry
it there. *Click* is a very short upward pitch snap at the moment of impact,
and it is what makes the ear hear something landing rather than a note fading
in.

97 % of a boom's energy and 98 % of a downshifter's is below 100 Hz. This layer
is not a garnish.

### Hit

Three inharmonic resonators at 1, 1.71 and 2.63 times *Tone*, morphing towards
plain filtered noise as *Noise* is raised, plus a low *Body* under them. Struck
at *Time*, which is a fraction of the span.

*Time* is where a whoosh-hit comes from: the noise layer peaks two thirds of the
way in and the transient is timed to arrive exactly there. The library files
fifteen of those under *Whoosh Hit*, and the timing is the whole trick.

Inharmonic ratios on purpose. Struck metal is inharmonic, and a harmonic series
here reads as a bell rather than as debris.

## Flutter, and what the measurement actually said

The plugin has *Flutter* because chopping a transition up is a staple and doing
it with an LFO means drawing an automation curve. But the measurement was worth
running before building it, and it produced a mostly negative result that
changed the design.

Taking each reference's amplitude envelope with its slow shape divided out, and
asking what fraction of the modulation energy sits at any single rate:

| family | rate, first third | rate, last third | energy at one rate |
|---|---|---|---|
| accents | 7.7 Hz | 7.0 Hz | 0.031 |
| booms | 9.1 Hz | 8.4 Hz | 0.022 |
| braams | 6.5 Hz | 5.6 Hz | 0.031 |
| downshifters | 8.1 Hz | **16.4 Hz** | 0.045 |
| impacts | 6.8 Hz | 7.7 Hz | 0.038 |
| transitions | 6.8 Hz | 7.4 Hz | 0.033 |

A metronome would give 1.0 in the last column. **Five of the six families are
not fluttering at all**, and the rates shown are simply where the noise happened
to peak.

Where it does appear it is unmistakable, and it is the downshifters:

| file | first third | last third | ratio |
|---|---|---|---|
| Downshifter - Stutter Scream | 4.1 Hz | 25.0 Hz | **6.08** |
| Transition - Quick 05 | 2.6 Hz | 7.2 Hz | 2.75 |
| Boom - Spacious | 22.6 Hz | 22.5 Hz | 0.99 |
| Downshifter - Groin Kick | 4.8 Hz | 2.4 Hz | 0.49 |

2 to 25 Hz, both accelerating and slowing down, with the extremes at 0.22× and
6.08×. That is exactly a start speed and an end speed with an exponential glide
between them.

So *Flutter* is **off by default**, and every preset that has it is one the
measurement supports. *Start Speed* and *End Speed* are the pair; *Shape*
chooses the modulator; *Target* decides whether it moves the level, the Air
layer's cutoff, or both — and on the cutoff is what most of the fluttering
references are actually doing. *Smooth* rounds the modulator off, with a corner
that follows the rate, so it means the same thing at 2 Hz and at 40.

## Nothing above 25 Hz is thrown away

The rest of the Verdalis suite highpasses at 60 Hz, because in a field recording
everything below that is traffic, ventilation and handling noise on the
microphone rather than the thing being recorded.

Here it is the instrument. The *Highpass* default is 25 Hz and it is there to
keep out DC and inaudible cone travel, nothing else.

## The EQ

A three-band EQ on the summed output: a low shelf, a mid bell and a high shelf,
with a highpass and a lowpass either side of them.

*Low Gain* is the control most of these sounds are actually mixed with. A
cinematic impact is a bass decision, and it is a decision that changes with the
track it is going into, which is why it is on the front panel rather than buried
in the layers.

*Mid Gain* is how a transition is made to sit under a mix instead of in front of
it. Cutting a few decibels around 700–900 Hz is the standard move and it is
what *Brass Wall* does.

## Variation

Every trigger draws its own values around the settings on screen: span, peak,
cutoff, sweep, pitch, level, pan, flutter rate, hit tone and the struck layers'
decays. The draws are gaussian and clipped at two sigma, so most triggers are
near the setting and a few are noticeably not — which is what a folder of
hand-made variants of one sound actually looks like.

At 0 the plugin is deterministic and repeats exactly, on any machine, at any
sample rate and block size. The self-test checks that, and *Random Seed* pins
the whole sequence when it is non-zero.

# The plugin window

{{PLUGIN}} draws its own window with X11 and Cairo on Linux, and Cairo on the
win32 backend on Windows. There is no toolkit dependency, which is what keeps
the plugin one self-contained file. It is embedded in the host's own window
through CLAP's GUI extension and repainted from the host's timer, and it resizes
by zooming one layout, from half size to four times.

The window is the suite's, in {{PLUGIN}}'s own impact-magenta theme. Every
gesture fires a streak that crosses the header and lands in a flash at the far
side. The streaks are deterministic in the gesture number, so a pinned *Random
Seed* gives a repeatable picture as well as a repeatable sound.

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
does — the same one-line explanation that appears in the *What it does* column
of the parameter reference.

## The mixer

Every layer's level lives on the panel that layer belongs to, which is right for
editing one layer and wrong for balancing them against each other. `MIXER` opens
the same controls arranged the other way round: one strip per layer, its level
as a fader, its width as a slim slider under it. They are the plugin's own
parameters, not a second set — moving a fader moves the knob on the panel, and
the host sees the same automation it always would.

Four strips plus the output: `AIR`, `TONE`, `SUB`, `HIT`. Only Air and Tone
carry a width, and none of them carries a pan: where a gesture sits is *Pan
Start* and *Pan End* on the Motion panel, and that is a property of the whole
gesture. A whoosh whose noise crossed the field while its sub stayed put would
not read as one sound moving.

`M` mutes a layer and `S` solos one, which is how a preset gets built from
nothing: solo one layer, get it right, bring the next one back. Several layers
can be soloed at once.

Mute and solo are **not** parameters and are not saved with a preset. They work
by holding a layer's level at the bottom of its range and remembering what it
was, so while either is active the preset bar shows a `SOLO ON` or `MUTE ON`
chip — click it to release every hold at once. `SAVE` releases them first, so a
muted layer can never be written into a preset as a silent one, and so does
loading a preset.

# Parameter reference

Every range, default, unit and explanation below is read from the plugin's own
parameter table when this manual is built, so it cannot drift from what the
window and the host show.

{{PARAMETER_REFERENCE}}

# The preset library

Thirty-one presets: five in each of the six families, plus one deliberately thin
accent. Generated from the preset files themselves.

{{PRESET_LIBRARY}}

# Using it in a host

## Notes

One note is one gesture. Velocity does two things: *Velocity To Level* sets how
loud it is, and *Velocity To Tone* opens the Air layer's filter and hardens the
transient, which is what hitting something harder actually does.

The played note transposes the Tone and Sub layers from middle C. A preset plays
as it was fitted at middle C, an octave down at C2, and so on. The Air layer
does not track the note — noise has no pitch to transpose.

*Max Voices* is how many gestures may overlap. One is monophonic, which is often
what a transition wants; more lets a new hit land while the last one is still
ringing. Turning it down while gestures are sounding releases them rather than
cutting them off.

## Automation and modulation

Every parameter is automatable and modulatable. Note that automating a parameter
*during* a gesture works, but several of the gesture's own numbers — *Span*,
*Peak*, the variation draws — are read once when the note starts, so changing
them mid-gesture affects the next trigger rather than the current one.

## Reproducible renders

Set *Variation* to 0 for an exactly repeatable gesture, or leave *Variation*
where it is and set *Random Seed* to a non-zero number for a repeatable
*sequence* of different ones. Either way the output is bit-identical across
runs, machines, sample rates and block sizes, which is what makes a bounced
transition match the one that was auditioned.

## CPU cost

Bounded by *Max Voices*. Each voice carries two noise generators, a
state-variable filter per channel, a tilt pair, three oscillators, a sine and a
resonator cluster; everything that costs a transcendental is recomputed once per
32 samples and interpolated. Sixteen simultaneous gestures is a small fraction
of one core.

## Rendering and tails

The plugin reports a tail to the host that covers the longest a gesture can run:
its span plus the widest a varied span can get, plus the struck layers' decays
and the reverb. A bounce that stops at the last note-off will still have the
whole tail in it if the host honours `CLAP_EXT_TAIL`, which most do.

## Headroom

The factory presets are trimmed to about −8 dBFS peak at *Random Seed* 7, with
*Variation* at its preset value. The output stage is the suite's soft clipper,
so driving it harder saturates rather than clips — but a gesture is a peaky
thing and the library's crest factors run from 7.7 dB to 19.0, so leave room.

# What is still being fitted

**The ear has not had its turn.** The suite's rule is ear first, then
measurement, and this plugin currently has the second without the first. The
measurements say the shapes and the balances are right; they cannot say whether
a braam sounds like a braam. A/B rendering against the references is the next
piece of work.

**The Hit layer is enveloped, not ringing.** Its three modes are excited by
enveloped noise and their decay is that envelope rather than their own Q, which
is why a long *Hit Decay* reads as noise held open instead of as metal ringing
down. A proper modal resonator with a decay rate per mode would fix it, and
would belong in the suite's shared DSP rather than here.

**There is no debris.** The measured impacts are often not one event at all —
*Impact - Bits & Pieces* and *Impact - Complex Wreck* are scatters of small
events over a second or more. The plugin has no way to make one.

**No tempo sync.** *Span* is in seconds and the flutter rate is in hertz. A
transition is almost always cut to the bar.

**Crest factor.** The downshifter references measure 7.7 dB; the presets render
at 11–13. Those are limited masters and the output stage here is a soft clipper,
so the gap is expected rather than surprising — but it is a gap.

# Reference and limits

## Where the model comes from

211 production sounds in six families — 35 accents, 16 booms, 17 braams, 13
downshifters, 24 impacts and 106 transitions — measured with the scripts in
`tools/analysis/`, which report:

- **the shape in time**: span, peak position, the 10–90 % rise and fall, and the
  median envelope contour of each family on a normalised time axis
- **the sweep**: the spectral centroid at the start, the middle and the end, and
  its contour
- **the spectrum**: octave-band levels from 31.5 Hz to 16 kHz, the fraction
  below 100 Hz, and the per-band decay to −20 dB
- **the pitch**: the strongest partial below 200 Hz in each gesture's loudest
  half second
- **the flutter**: the rate, depth and periodicity of the amplitude modulation
  in the first and last third of each gesture

The reference material is not in this repository and is not ours to
redistribute.

One measurement trap is recorded in `tools/analysis/README.md` because it cost
an hour: a 4 ms RMS envelope has its Nyquist at 125 Hz and will happily track
the *carrier* of a sound that has descended to 30 Hz, which is exactly what a
downshifter does. The first run reported flutter at 34 Hz for files with none.

## What it does not do

- **It is not a sampler and will not reproduce a specific reference.** It is
  fitted to what those sounds *are* statistically, not to any one of them.
- **No tempo sync**, no host transport, no reverse mode.
- **The Air layer does not follow the note.** Noise has no pitch, so only Tone
  and Sub transpose.
- **`Blend` wraps from the last family to the first.** Blending past
  Transition arrives at Accent, which is alphabetical rather than musical.
- **A gesture cannot be retriggered from where it was.** A new note is a new
  gesture; there is no legato.
