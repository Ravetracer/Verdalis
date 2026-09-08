---
tagline: Synthesised wind
subtitle: CLAP instrument for Linux and Windows
accent: #F0845C
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that generates wind. Every gust, every aeolian
tone and every rustling leaf is computed from noise, filters and resonators
while the plugin plays -- nothing is played back. No two winds are ever alike.

The model starts from an awkward fact. **Wind is silent.** Air in motion
radiates essentially nothing on its own; everything a listener calls wind is the
flow meeting something — a hillside, a wire, a twig, a leaf, the edge of a gap.
So {{PLUGIN}} is built in two halves: a flow field, which makes no sound at all,
and the sources that flow drives.

The flow field is a wind speed with three things happening to it at once:
turbulence, discrete gusts, and a squall drift slower than a tenth of a hertz.
What it drives is a broadband airflow bed, a low-frequency buffet, a bank of
aeolian tones whose pitch is proportional to the wind speed — which is what
howling actually is — and, as a bonus layer, foliage.

Every layer came out of measurement. The model and the factory presets were
fitted against 63 field recordings by measuring the recording and the plugin's
own output with the same analysis code and moving values until the two agreed.
Where a number in this manual comes from a measurement or a paper, it says so.

> **{{PLUGIN}} {{VERSION}} is an early version.** The engine, the parameter set
> and the window are complete and the self-test passes, but the preset library is
> a first fit. *What is still being fitted* at the end of this manual lists what
> is known not to match yet, and it is worth reading before concluding that
> something is broken.

## What is in this manual

*Installing* and *A first sound* are enough to get wind out of the plugin.
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
2. Draw a long note — sixteen bars or more. The wind blows for as long as the
   note is held, and the release lets it drop.
3. Open the preset browser by clicking the preset name in the window header and
   try **Open Plain**, then **Howling Wind**, then **Rustling Leaves**.

Wind needs length, more than anything else in the suite. A gust takes seconds to
arrive and pass, and a squall runs over minutes; a two-second note is a fragment
of one gust and tells you almost nothing.

## Three things to try first

**Turn one knob: *Wind Speed*.** It is the master control of the whole plugin.
Level, brightness, the pitch of the howl and how fast the foliage rustles all
follow it, because in the model they all follow the same U. Sweep it from 2 to
25 m/s and the sound goes from a breath to a gale without touching anything
else.

**Listen to the howl swoop.** Load **Howling Wind** and hold a long note. Each
time a gust arrives, the tone rises in pitch as well as in level. That is not an
effect: vortices shed off an obstacle at `f = St·U/d`, so a faster wind sheds
faster. *Howl Track* is how much of that is applied, and the references confirm
it — see *Howling is aeolian*.

**Change the obstacle, then the terrain.** *Obstacle* is what the wind is
shedding off — grass, reeds, twigs, branches, wires, rock, a gap, a cave — and it
sets the pitch, the sharpness and, crucially, whether the tone tracks the wind at
all. *Terrain* is what the wind is crossing, and it multiplies the turbulence by
real numbers: choosing *Forest* makes the wind two and a half times as gusty as
*Plain*, which is the ratio the logarithmic wind profile gives for those two
roughness lengths.

# How it works

## The flow, and what it drives

| Layer | What it is |
|---|---|
| **The flow** | A wind speed U(t), per channel, and completely silent. Its mean is *Wind Speed*; on top of that sit turbulence (a von Kármán-shaped noise process whose intensity is σ/U, measured 4–27 % across the library), discrete gusts (0.6–33 a minute, gust factor 1.04–1.54) and a squall drift below 0.1 Hz, which carries a median 43 % of the whole envelope variance and is therefore not a detail. |
| **Airflow** | The broadband bed: turbulent noise through a four-pole tilt whose slope is a parameter. Its amplitude follows U³, because aerodynamic sound power from flow over a rigid surface goes as U⁶. |
| **Buffet** | The low end: the pressure fluctuation of the moving air rather than the noise it radiates. It follows the dynamic pressure, U², so it grows more slowly than the bed. |
| **Howl** | The aeolian tones. A bank of resonant bands whose centre frequencies are `St·U/d` for obstacles of diameter d, so their pitch rises and falls with every gust. |
| **Rustle** | Foliage. A Poisson stream of leaf clicks, each a short band-limited ring at roughly `c/2L` for a leaf of size L. |

## Turbulence is a spectrum, not a wobble

The wind speed's own fluctuations have a shape. In the inertial subrange
Kolmogorov gives a velocity spectrum falling as `f^-5/3`, and that is what the
engine generates: three one-pole-filtered noise sources an octave and a half
apart, weighted 1 : 0.56 : 0.315. Those weights are `4^-5/6` and `16^-5/6`, so
summing them gives exactly that slope across the band they span. One filter would
give −6 dB/octave, which is too steep to be turbulence, and white noise would
give no shape at all.

The lowest of the three corners is tied to *Gust Rate*, because both are the same
physical quantity seen from different ends: a turbulence length scale L crossing
at speed U produces something every L/U seconds.

*Turbulence* is the intensity of that process, and it reads as the percentage
meteorology reports — σ/U, measured 4 to 27 % across the library with a median of
10 %.

## A gust is a fluctuation of the flow, not a sound

Gusts own no filter and no noise source in {{PLUGIN}}, because a gust makes no
sound of its own. Each one is an envelope with a strength and a position, and
what is heard is the bed and the howl responding to it. A gust arriving from the
left raises the left channel's wind speed first, which is most of why real wind
moves across a listener.

The strength distribution is heavy-tailed on purpose: most gusts are
unremarkable and a few are much stronger, which is what a gust factor is a
summary of. And the envelope is symmetric — the references' median rise/fall
ratio is 1.01, so unlike a breaking wave a gust is not a transient. *Gust Shape*
is there for when you want one anyway.

## The level law: why a gust is such a large event

Aerodynamic sound from flow over a rigid surface radiates as a dipole, and
Curle's 1955 result gives its power as U⁶. Amplitude therefore goes as U³, and a
doubling of wind speed is **+18 dB**. That is *Speed Law* at 100 %, and it is
worth knowing about before wondering why a gust is so much louder than the wind
between gusts. Backing it off is not cheating — it is the one control that makes
a physically correct wind fit inside a mix.

## Howling is aeolian

A bluff body in a flow — a wire, a twig, a blade of grass, a branch — sheds
vortices alternately from each side at the Strouhal frequency

```
f = St · U / d          St ≈ 0.2 for a cylinder
```

so a 2.5 mm twig in an 8 m/s wind sheds at 640 Hz, which is the median tone
frequency across the library's tonal recordings. *Howl Size* is therefore a real
diameter, not a pitch knob: the tonal references imply obstacles from 1.5 to
11.8 mm.

The interesting part is that **both the pitch and the level follow the same U**,
so an aeolian tone has to rise in pitch as it gets louder. That is a prediction a
recording can falsify, and the library confirms it: the tone's pitch rises with
the level in 12 of the 17 recordings that hold a steady tone at all, and in the
four whose file names actually say *howling* the correlation runs from 0.52 to
0.87. It swoops a median 0.72 of an octave doing it. *Howl Track* is how much of
that is applied.

Three details matter to how it sounds:

- **A howl is a band of noise, not a whistle.** The measured Q runs 1.1 to 16.5
  with a median of 5.1, so each obstacle is a resonant band on its own noise
  rather than an oscillator. Taking *Howl Resonance* to the top of its range
  stops sounding like wind at all.
- **The tone comes up steeply.** Shedding needs the flow: below *Howl Onset*
  nothing happens, and above it the tone rises as the cube of the excess,
  following the same U⁶ power law as the bed. That is why a howl arrives with the
  gust rather than fading in.
- **It wanders.** Vortex shedding is not a metronome — the local velocity drifts
  and the tone drifts with it. *Warble* is how much, and it is most of what makes
  a howl eerie rather than electronic.

## A cavity does not track the wind

The *Gap* and *Cave* obstacles behave differently from the rest, and the reason
is physics rather than voicing. A cavity resonates at a frequency its own
geometry fixes; the flow only excites it, so its pitch barely moves however hard
it blows. Those two obstacles therefore scale *Howl Track* down almost to
nothing, which is what turns a howl into a drone.

The library cannot settle this on its own — its one cave recording tracks at
−0.01 while everything else is positive, and across the tonal set there is no
correlation between Q and tracking at all. The distinction is kept because it is
sound physics, and it is flagged here because it was not measured.

## An obstacle sees a smoother wind than a microphone

The shedding frequency follows a *smoothed* wind speed rather than the
instantaneous one, because an obstacle does not see the free stream: it sees the
flow averaged over the eddies that envelop it. Without that smoothing the tone
chases every ripple of the turbulence and smears over two octaves, where the
references measure a median swoop of 0.72 — a gust's worth, not a turbulence
spectrum's worth.

## Terrain roughness is arithmetic

The logarithmic wind profile gives a turbulence intensity of about `1/ln(z/z₀)`
at height z over ground of roughness length z₀, and z₀ is tabulated. At a
listening height of 10 m, normalised to short grass:

| Terrain | Roughness | Gustiness |
|---|---|---|
| Coast | open water | 0.54× |
| Desert | sand | 0.56× |
| Tundra | snow | 0.77× |
| Plain | short grass | 1.00× |
| Meadow | long grass | 1.26× |
| Mountain | broken country | 1.94× |
| Forest | forest | 2.52× |
| Street | city | 3.06× |

*Terrain* also tilts the spectrum, scales the buffet and sets how enclosed the
space is — 0.02 for open ground, where there is nothing close enough to reflect,
against 0.85 for a street with walls on both sides.

## The rustle: leaves, not hiss

Leaves do not hiss, they click. Each one is a separate collision, so the rustle
is a Poisson stream of short band-limited rings at roughly `c/2L` — a 40 mm leaf
around 4.2 kHz, which is the median onset centroid across the library's foliage
recordings.

The rate is what matters most, and there is a ceiling on it: the references have
**15 to 40 resolvable onsets a second**, which is the rate at which they stop
merging. Push *Rustle Density* past that and the clicks overlap into a wash
rather than being separately audible, which is the difference between a rustle
and a hiss. *Foliage* moves it deliberately: conifer needles and grass are set
to merge, because that is what a sough is, while dry leaves are sparse and
clatter.

*Clatter* is how impulsive each leaf is. It was measured as the variation of the
onset flux, and the two ends of the library are unambiguous: 0.20 for a merged
conifer-like hiss against 0.80 for dry leaves.

Leaves also have a threshold. *Rustle Onset* is the share of the mean wind speed
at which they start moving at all, and at the default and an 8 m/s wind that is
2 m/s, which is about where real foliage begins.

## Distance

*Distance* applies air absorption plus a downward tilt, with *Air* deciding how
much of the top survives. The library's most distant recording is 117 dB down at
16 kHz and peaks at 63 Hz, which is the far end this has to be able to reach.

# The plugin window

{{PLUGIN}} draws its own window with X11 and Cairo on Linux, and Cairo on the
win32 backend on Windows. There is no toolkit dependency, which is what keeps the
plugin one self-contained file. It is embedded in the host's own window through
CLAP's GUI extension and repainted from the host's timer, and it resizes by
zooming one layout, from half size to four times.

The window is the suite's, in {{PLUGIN}}'s own dust-coral theme — the suite's
other plugins are all cold, so wind, which everyone expects to be grey, is the
one that gets to be warm. Streaklines run across the header behind the wordmark
the way a wind tunnel shows a flow it cannot otherwise photograph: they flow
faster the more gusts are in the air, and every new gust puts a shove into the
whole field. Beside the preset name is a gust meter, showing how many are in
flight and how many have passed.

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

{{PRESET_LIBRARY}}

# Using it in a host

## Notes

A note starts the wind and holding it keeps it blowing. The ADSR shapes the whole
scene rather than individual gusts, and *Release* is what lets it drop.
*Velocity To Speed* is worth knowing about: velocity can set the wind speed
itself, which is not the same as setting the level — a faster wind is brighter and
higher-pitched as well as louder.

{{PLUGIN}} needs long notes more than anything else in the suite. A gust takes
seconds to arrive and pass and a squall runs over minutes, so almost all of the
character is inaudible in a short note.

There are 16 voices, with 256 gusts and 768 leaves in shared pools, so the number
of held notes cannot multiply the CPU cost without bound.

## Automation and modulation

Every parameter is automatable, and host parameter modulation
(`CLAP_EVENT_PARAM_MOD`) is supported globally. Per-note modulation and note
expressions are ignored. Parameter changes are sample accurate, so an automation
ramp over a bar — a storm getting up over *Wind Speed* and *Turbulence* — is a
smooth move rather than a staircase at block boundaries.

*Wind Speed* is the one to automate. Because level, brightness, howl pitch and
rustle rate all follow it, a single ramp does what would otherwise take five.

## Reproducible renders

*Random Seed* at 0 never repeats. Any other value renders the same wind for the
same notes every time. The initial squall phase, the first gust timer and each
obstacle's size and position are derived from the voice's slot and key rather
than from the shared generator, so a fixed seed promises the same wind even when
note events arrive before the seed has been applied.

## CPU cost

Cost is bounded by the pools, and the flow field itself is nearly free: it
updates once every 64 samples, and its gains are ramped between updates so a
gust does not step. The most expensive controls are *Howl Voices*, which decides
how many resonators are running, *Rustle Density*, which decides how many leaves
are alive at once, and *Space Amount*, which is the room model. Gusts are the
cheapest thing in the plugin because they are silent: an envelope and a position,
with no filter of their own.

## Rendering and tails

Wind has a long tail — the release, the gust that was passing when the note went,
and whatever the space is still doing with it. When bouncing, leave several
seconds past the end of the note.

# What is still being fitted

{{PLUGIN}} {{VERSION}} is a first fit, and the measurements say where it is not
there yet. All of this is in the plugin's `TODO.md` with what is known about it.

**The presets swoop less than the references.** Across the tonal presets the
median swoop is 0.28 of an octave against the references' 0.72. The pitch follows
a smoothed wind speed, and that smoothing — a fixed 0.65 s time constant — is
probably too much of it.

**The model does not reproduce one thing the library measures.** Its broadband
centroid does not reliably rise with level (median correlation −0.10), and the
model says it should, because the bed follows U³ and the buffet only U². The
recordings that darken as they get louder are the ones with the most energy below
50 Hz, which suggests that what is being measured there is microphone
pseudo-sound and the drag of moving branches — neither of which is radiated wind.
*Buffet* is what a preset has instead: raising it is what makes a gust rumble
rather than hiss.

**Twenty of the twenty-three presets have at least one band more than 6 dB from
their reference**, and all but two are within 10 dB. The two that are not are
*Far Away*, whose reference has a 26 dB notch between two adjacent third-octaves
that the model does not produce, and *Between Houses*, which is 14 dB short at
4 kHz.

**Much of the reference library is band-limited by its codec**, more severely
than a listener would guess — one recording holds nothing above 3 kHz and
several stop between 4 and 7. Bands above each reference's own bandwidth are not
compared, because fitting the synthesis to a band the reference does not contain
would be fitting it to an encoder. Before this was noticed, five presets appeared
to be 20–50 dB too bright and three had had output filters put on them to
compensate.

# Reference and limits

## Where the model comes from

- **Strouhal, V.**, *Über eine besondere Art der Tonerregung*, 1878 — vortex
  shedding at `f = St·U/d`, which is what *Howl Size* and *Howl Track* are built
  on and the reason a howl swoops.
- **Curle, N.**, *The influence of solid boundaries upon aerodynamic sound*,
  1955 — flow over a rigid surface radiates as a dipole with power going as U⁶.
  That is *Speed Law*, and the +18 dB per doubling of wind speed.
- **Kolmogorov, A. N.**, 1941 — the inertial subrange, `f^-5/3`, which is the
  shape of the turbulence process and the reference point for *Flow Tilt*.
- **von Kármán, T.**, on the spectrum of atmospheric turbulence.
- **Wieringa, J.**, *Updating the Davenport roughness classification*, 1992 —
  the roughness lengths behind the *Terrain* table, via `I ≈ 1/ln(z/z₀)`.
- **Sabine, W. C.** — the decay time of the space model, as elsewhere in the
  suite.

The papers themselves are not part of the release; they are not ours to
redistribute. `tools/analysis/README.md` in the source tree records every
measurement the library was fitted to, including the five bugs the measurements
found.

## Notes and limits

- Linux and Windows, x86-64. Plugin state is stored little-endian.
- 16 voices, with 256 gusts, 768 leaves and up to 12 aeolian resonators per
  voice in shared pools.
- The flow field runs at a control rate of one update per 64 samples, with its
  gains ramped between updates. Everything audible runs per sample.
- Host parameter modulation is supported globally. Per-note modulation and note
  expressions are ignored.
- Deliberately built without `-ffast-math`: on x86 GCC that links
  `crtfastmath.o`, which would flip the FTZ and DAZ flags for the entire host
  process.
- Wind only. Several of the references also contain surf, rain, birds or
  crickets; those were used for their flow statistics and excluded from the
  spectral fit.

## Versioning

Semantic versioning. **MAJOR** is an overhaul: a rewrite of the synthesis model,
or a change that breaks existing presets or saved host state. **MINOR** adds
something — a parameter, a layer, presets. **PATCH** is fixes that add nothing
new.

## License

MIT. The only external dependencies are the CLAP headers, which are MIT
licensed, plus X11 and Cairo for the plugin window.

Everything the wind engine produces is computed at run time. {{PLUGIN}} also
embeds a small amount of recorded audio by Jagadamba, obtained from
freesound.org (sound ids 253799-253826) and used under the Creative Commons
Attribution 4.0 International licence; it was decoded, loudness matched and
embedded. See the LICENSE file in the plugin folder for the full notice.

Reference recordings used during development are not part of the release and are
not redistributable.
