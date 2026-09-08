---
tagline: Synthesised thunder
subtitle: CLAP instrument for Linux and Windows
accent: #B396FA
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that generates thunder. It contains no samples:
every flash grows its own lightning channel, and every shock wave you hear is
computed from that channel's geometry while the plugin plays. No two thunders
are ever alike.

Thunder is the sound of a lightning channel — several kilometres of it, and
every metre a separate source. The channel heats to twenty thousand degrees and
expands in a few microseconds, which launches a shock wave from all of it at
once. The listener then hears each piece when its own shock arrives: the near
pieces first and loud, the far pieces late, quiet and dull, because the air soaks
up the high end over distance. A crooked channel has stretches side-on to the
listener whose shocks arrive together and pile into a *clap*, and stretches
end-on whose shocks arrive spread out and quiet, which is the *rumble*.

All of that is geometry, and {{PLUGIN}} computes the geometry. That is why
*Distance* behaves the way distance behaves rather than like a filter and a
fader: move the strike and the sharpness of the crack, the loss of high end, the
smearing of thousands of arrivals into a roll and the sub-bass that outlives
everything all follow by themselves, because they are consequences of the model.

The approach is that of Few (1969) and Ribner and Roy (1982), who synthesised
thunder this way and found it convincing. The engine's constants were then set
against 38 recordings of real thunder, by measuring the recording and the
plugin's own output with the same analysis code and moving values until the two
agreed.

## What is in this manual

*Installing* and *A first sound* are enough to get thunder out of the plugin.
*How it works* explains the model, and is worth reading before spending time in
the parameter list — the controls are the model's own quantities, so most of them
make immediate sense once it does. *Parameter reference* and *The preset library*
are generated from the plugin itself, so they always describe the version on the
cover. *Using it in a host* covers notes, the three trigger modes, reproducible
renders and CPU cost.

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
2. Play a single short note. One note is one flash — the note does not have to
   be held for the thunder to play out.
3. Open the preset browser by clicking the preset name in the window header and
   try **Rolling Thunder**, then **Close Strike**, then **Far Horizon**.

The three of those are the same engine at three distances, and comparing them is
the fastest way to hear what the model does.

## Three things to try first

**Move the strike.** *Distance* is the master control. Under a kilometre you get
a crack with a tearing edge; past ten you get nothing but the bottom two octaves,
swelling in over seconds. Nothing else needs changing for that to work.

**Hold a note in Storm mode.** Set *Mode* to *Storm* and hold a long note.
Flashes keep coming at *Storm Rate*, as a Poisson process rather than on a grid,
and *Variation* scatters each flash's distance, height, direction and stroke
count. A held note is a storm rather than a loop.

**Raise Compress.** Thunder recorded on anything that was already limiting sounds
dense, with the ground still shaking after the sky has stopped. *Compress* lifts
the rumble, the far claps and the echo tail towards the crack, which is that
sound. The ratio is capped at 4:1 on purpose.

# How it works

## The channel

Every flash grows a new channel. From the strike point — *Distance* away, at an
azimuth *Variation* scatters — a random walk climbs to the cloud base at
*Height*, each step remembering its direction so that the channel wanders rather
than zigzags; *Tortuosity* sets how far it wanders.

At the top it turns and runs *Cloud Spread* kilometres inside the cloud. That is
the part of a real channel that is longest, and the reason a distant thunder
lasts twenty seconds rather than two: the listener hears the difference in path
length, and a horizontal channel has a great deal of it. *Branching* adds side
branches, downward from the lower channel and sideways from the cloud part, each
a smaller channel of its own. The in-cloud branches are where the extra claps
come from — Kappus and Vernon counted one to five claps per thunder in a storm
5 km off, and only half the time was the first one the loudest.

The channel is then cut into elements, up to *Max Shocks* of them, dealt out in
proportion to length over distance: the near channel finely, where it is heard as
separate shocks, and the far channel coarsely, where it is heard as a wash. An
element longer than the channel's coherence length stands for several sources
adding incoherently, so its amplitude goes as the square root of its length and
the power per metre of channel does not depend on how finely it was cut.

## What each element sends

For every element the engine works out, once, when and how its shock arrives.

**When.** Its distance over the speed of sound. The clock starts at the first
shock anyone will hear.

**How loud.** `1/r` for spherical spreading, times a directivity: a line element
radiates broadside and not off its ends, so its level goes as the sine of the
angle between the element and the line of sight, raised to a power *Focus* sets.
Focused, only the parts side-on to you are loud, and the claps stand out from the
rumble.

**How long.** The N-wave a shock has become by the time it arrives lengthens with
a small power of the distance travelled, so far shocks are deeper, and *Weight*
scales that. The channel inside the cloud radiates longer waves than the stroke
below it: Kappus and Vernon, and Holmes before them, put intracloud thunder's
peak near 10 Hz against 50 Hz for a cloud-to-ground stroke. An element's wave
therefore lengthens with its height, to twice the length at the cloud base, and
its crackle fades by the same factor. That is the deep swell arriving ten seconds
after the crack. In the recording *City Thunder* is set against, that swell is
the loudest thing in the file, so the cloud also radiates half again as hard —
the one number in the model that is a fit and not a law.

**How dull.** Atmospheric absorption grows as `f^1.3` in decibels, about 5 dB per
kilometre at 1 kHz in the damp air under a storm; ISO 9613 gives 4.7, 10 and
24 dB/km at 1, 2 and 4 kHz at 20 °C and 70 % humidity, and *Air Absorption*
scales that. No fixed filter slope fits both a strike at 300 m, which keeps its
4 kHz, and one at 10 km, which has lost 40 dB there — so each shock gets three
one-poles at staggered corners, placed where the loss reaches 6, 12 and 24 dB,
which sit on the real curve.

**From where.** Its azimuth, mapped onto the stereo field by *Width*. A channel a
kilometre away spans the sky and the stereo field; one fifteen kilometres away is
a point.

**The ground shadow.** In a normal temperature lapse sound bends upward, so the
foot of a distant channel is never heard, and a far thunder swells in from the
cloud instead of cracking from the strike. *Swell* sets how much of the low
channel is shadowed, and the shadow grows with distance on its own.

*Scatter* spreads level and length from element to element, and the whole flash is
normalised so that its loudest tenth of a second carries a fixed energy.
*Distance* therefore changes what a thunder sounds like without deciding whether
it is audible; a mild loss per kilometre on top keeps far ones further back.

## Return strokes

*Strokes* re-light the channel, *Stroke Gap* apart, each usually quieter and a
little late. Dart leaders do not branch, so only the first stroke lights the
branches. Kappus and Vernon note that strokes 30 ms apart would put a peak at
33 Hz in the spectrum if they were exact repeats; they are not exact here — a
per-element, per-stroke jitter sees to that — and they do put the 20 ms
fluctuation into the first second of a close strike that the recordings show.

## The shock wave

Each arrival is played as an **N-wave**: a pressure jump, a straight fall through
zero to the mirror value, and a jump back. Both fronts are eased over a rise time
*Crack* sets, so a soft setting is a thud and a hard one a snap.

A clean N-wave's spectrum falls at 6 dB/oct above its peak, and measured against
the close recordings that is 20 dB short above 2 kHz: the tearing of a strike a
few hundred metres off carries as much energy above a kilohertz as the wave
carries below it. So every front also carries a burst of noise, standing for the
fine roughness of the channel radiating a spray of tiny shocks. It goes through
the same air as the wave, which is why a close strike has it and a distant one has
lost it.

## The rumble

Thousands of N-waves overlapping are a rumble, but with a finite element budget
they are also a crackle, so a noise layer fills in underneath. Its level follows
the energy of the shocks arriving, integrated over a fifth of a second, and its
lowpass follows either *Rumble Tone* or the air, whichever has taken more off. It
is a band and not a lowpass, because a close thunder has less under 40 Hz than at
100. *Rumble Width* spreads it and *Drift* lets it wander across the field while
the thunder plays.

## Echoes and space

*Echoes* are the landscape: up to eight reflectors at distances *Echo Spread*
apart, each duller and quieter the further it stands, and panned where it stands.
A little of what comes back goes round again, because a hill throws the echo of
an echo too, and the loop gain is a property of the landscape rather than of
*Echo Level* — so the tail decays at the same rate however loud the echoes are
mixed. This is what lets a thunder trail off instead of stop.

*Space* is the suite's room model: eight early reflections and an eight-line
feedback delay network built from one physical size, with Sabine's law for the
decay time, and its lowest highpass moved down an octave so it does not take the
bottom off the thunder. It is the room you hear the storm from — a porch, a
stairwell, a hall — not the storm itself.

## Dynamics

*Compress* is a stereo-linked compressor at the very end, before the safety
clipper, with a soft knee and automatic make-up: threshold and ratio move
together, from nothing to −24 dB at 4:1, and a full-scale peak still comes out at
full scale. What it does to a thunder is lift everything under the crack — the
rumble, the far claps, the echo tail — towards it.

*Comp Attack* slow lets the first snap through; *Comp Release* fast pumps with the
claps. The ratio is capped at 4:1 on purpose, because a hard ratio flattens the
decay of the tail, and a thunder that never quite ends is worse than one never
compressed.

## Triggering

*Mode* decides what a note means.

| Mode | What a note does |
|---|---|
| One Shot | One flash per note, played out whatever the note does afterwards |
| Gated | One flash per note; letting go fades what has not yet arrived, over *Release* |
| Storm | Flashes keep coming at *Storm Rate* for as long as the note is held |

Storm mode fires as a Poisson process, never on a grid, and each flash draws its
own distance, height, direction and stroke count from *Variation*. *Attack* fades
the shocks in as they arrive, which softens a first crack. *Velocity to Distance*
puts soft notes further away, up to eight times as far, and *Velocity to Level*
does the obvious.

## Variation

Every flash grows a new channel, and *Variation* scatters the settings it grows
from: distance, height, cloud spread, tortuosity, branching, weight, crack, the
number of strokes, their gaps and the azimuth the strike stands at. At 0 % every
flash is a fresh channel with the same statistics; at 100 % the same preset gives
you a strike overhead and a rumble over the hills on successive keys.

# The plugin window

{{PLUGIN}} draws its own window with X11 and Cairo on Linux, and Cairo on the
win32 backend on Windows. There is no toolkit dependency, which is what keeps the
plugin one self-contained file. It is embedded in the host's own window through
CLAP's GUI extension and repainted from the host's timer, and it resizes by
zooming one layout, from half size to four times.

The header carries the wordmark, the preset name with its browser, the
shock-activity meter — and a lightning bolt, grown afresh for every flash. The
bolt is deterministic in the flash number, so the same flash always draws the same
bolt.

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

One note is one flash, and in One Shot mode the thunder plays out whether the
note is still held or not — so a single sixteenth is enough to fire a twenty-second
rumble. Use Gated when the arrangement needs the thunder to stop with the note,
and Storm when it needs to keep going.

Parameters are read at the moment a flash fires, and they decide that flash.
Changing *Distance* while a thunder is rolling changes the next one, not the one
you are listening to. That is worth knowing when automating: put the move before
the note, not on it.

There are 8 voices, with 12 flashes and 4096 shocks in shared pools, so the
number of held notes cannot multiply the CPU cost without bound.

## Automation and modulation

Every parameter is automatable, and host parameter modulation
(`CLAP_EVENT_PARAM_MOD`) is supported globally. Per-note modulation and note
expressions are ignored. Parameter changes are sample accurate.

## Reproducible renders

*Random Seed* at 0 never repeats. Any other value renders the same thunder for
the same notes every time, whatever the plugin was doing before them — the noise
and the geometry come from separate generators, so idling cannot move the sequence
along. Set a non-zero seed before bouncing anything that has to match a previous
render.

## CPU cost

A flash's channel is grown on the audio thread when it fires, which costs a few
hundred microseconds at 2048 elements. *Max Shocks* is the control for that: it
caps how finely the channel is cut, and halving it halves both the setup cost and
the density of the crackle. After that, *Echoes* and *Space* are the next most
expensive blocks.

## Rendering and tails

Thunder has a long tail — the far claps, the echoes and the room can run for
twenty seconds after the note. When bouncing, leave that much past the end of the
last note, or the roll is cut off mid-decay.

# Reference and limits

## Where the model comes from

- Few, A. A., *Power spectrum of thunder*, J. Geophys. Res. 74, 1969.
- Ribner, H. S. and Roy, D., *Acoustics of thunder: a quasilinear model for
  tortuous lightning*, J. Acoust. Soc. Am. 72, 1982.
- Kappus, M. E. and Vernon, F. L., *Acoustic signature of thunder from seismic
  records*, J. Geophys. Res. 96, 1991.
- Guo et al., *Study and analysis of the thunder source location error based on
  acoustic ray-tracing*, Remote Sensing 16, 2024.
- Rusz et al., *Locating thunder source using a large-aperture micro-barometer
  array*, Front. Earth Sci. 9, 2021.
- ISO 9613-1, atmospheric attenuation, for the absorption figures.

What the recordings themselves settled is measured rather than taken from any of
those: a close strike peaks at 80 to 160 Hz and is 42 dB down at 5 kHz; a distant
one is nothing but the bottom two octaves; the attack is 20 ms, the clap a second
and the rumble ten; and distant thunder takes seconds to swell in.

## Notes and limits

- Linux and Windows, x86-64. Plugin state is stored little-endian.
- 8 voices, 12 flashes and 4096 shocks in shared pools.
- Host parameter modulation is supported globally. Per-note modulation and note
  expressions are ignored.
- Parameters read at the moment a flash fires decide that flash.
- Deliberately built without `-ffast-math`: on x86 GCC that links
  `crtfastmath.o`, which would flip the FTZ and DAZ flags for the entire host
  process.
- No rain and no wind — this is thunder only. RainyDay does the rain.

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
