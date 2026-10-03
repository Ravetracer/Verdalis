---
tagline: Every Verdalis instrument, one place
subtitle: CLAP instrument for Linux and Windows
accent: #A6D35A
---

[TOC]

# Introduction

{{PLUGIN}} is a CLAP instrument that builds a whole place out of the Verdalis
suite. It layers the nine nature instruments — RainyDay's rain, ThunderClap's
thunder, ShoreBreak's waves, SkyHowl's wind, ChirpParade's birds, RiverFlow's
water, CrackleBlaze's fire, InsectSwarm's insects and NightLife's night — and
mixes them into one scene. Every sound is computed while the plugin plays;
nothing is played back, and no two playbacks are the same.

**A layer is the plugin itself.** {{PLUGIN}} contains no copy of any of them. A
rain layer is RainyDay's own engine, with RainyDay's parameters, RainyDay's preset
library and RainyDay's panels. Whatever a plugin can do, its layer can do, and a
preset you made in the plugin loads straight into a layer.

**A scene is a drone.** Nothing waits for a note. Every layer plays from the
moment it is added until it is removed, the thunder flashes when it flashes and
the birds sing when they sing.

> **{{PLUGIN}} {{VERSION}} is the first release.** Nothing before it was
> published, and no scene or saved state from an earlier build exists.

## What is in this manual

*Installing* is the boring part. *A first scene* is five minutes. *How it works*
is what a layer is, how a drone stands in for a played note, and what the scene
adds on top. *The plugin window* is the tabs and the mixer. *The parameter
reference* lists the scene's own controls and is generated from the plugin's own
table; a layer's controls are its plugin's and are documented in that plugin's
manual. *The preset library* lists the factory scenes.

# Installing

{{PLUGIN}} is one self-contained `.clap` file plus its `presets` folder, and a
`.vst3` bundle beside it. It does not need the other plugins installed: the nine
instruments are built into it. There is nothing to register and no runtime to
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
scenes there too, though the host's own browser will not.

A host lists every layer's parameters, whether the layer is in the scene or not,
because a plugin's parameter list cannot change while it runs. They are named by
layer — *Rain 1 Density*, *Rain 2 Density* — and grouped the same way, so the
ones in use are easy to find for automation.

## Your own scenes

*SAVE* in the bar at the bottom writes the scene to your user preset folder:

| Linux | `$XDG_CONFIG_HOME/{{PLUGIN}}/presets`, or `~/.config/{{PLUGIN}}/presets` |
| Windows | `%APPDATA%\{{PLUGIN}}\presets` |

Saving as `Folder/Name` files it in a folder. The browser lists the library by
folder, and a whole folder can be exported as one **preset pack** and imported
again, exactly as in every other Verdalis plugin.

## Your own layer presets

The *SAVE* in a layer's own bar writes a preset **of that layer's plugin**, into
that plugin's own user folder — a rain layer saves a `.rainyday` preset to
`~/.config/RainyDay/presets`. It then shows up in RainyDay as well, and every
RainyDay preset you have shows up in a rain layer's browser.

## Building from source

```sh
./install.sh            # configure, build, self-test, install to ~/.clap
./install.sh --vst3     # and the VST3
```

{{PLUGIN}} is built from the nine plugin folders beside its own, so the suite has
to be checked out whole.

# A first scene

1. Load {{PLUGIN}} on an instrument track. It is silent: an empty scene.
2. Choose **Forest River** in the scene bar at the bottom. It fades in over a
   couple of seconds — no note needed.
3. Open the **BIRDS 1** tab. These are ChirpParade's controls; try another
   ChirpParade preset in the layer's bar at the top of the page.
4. Back on **SCENE**, pull the river's fader down and the birds come forward.

## Four things to try first

- **Add a layer.** *+* at the end of the tabs, then **Thunder**. The storm comes
  in with the first flash some seconds later, not at once; *Storm Rate* on its
  page sets how often.
- **Place a sound.** On a layer's strip, switch *STEREO* to *MONO* and move its
  pan: the layer becomes one point in the field, the way a single bird or a
  stream off to one side is.
- **Close the scene in.** On the scene page, a lowpass around 1 kHz is the scene
  heard from indoors; *Width* at 50 % pulls it narrow.
- **Tie it to the song.** Set *Gate* to *Transport* and the scene fades in when
  the host plays and out when it stops, on the *Attack* and *Release* you set.

# How it works

## A layer is its plugin

Each of the nine plugins keeps the step that turns its parameters into its
engine's settings in a file of its own, and {{PLUGIN}} calls that same file. So a
parameter in a layer is not an imitation of the plugin's parameter: it is the
plugin's parameter, read by the plugin's code, driving the plugin's engine.

That is why a layer has every control its plugin has, and why its presets move
both ways. Loading a preset into a layer and saving the layer straight back gives
the preset you started with, value for value; the self-test checks that for all
163 factory presets of the nine plugins.

Up to four layers of a kind run side by side — rain on a roof, rain on leaves and
a distant wall of rain at once. Each is a whole instance of its engine with its
own randomness, so two layers of the same preset are two different rains, not one
rain twice as loud.

## The drone note

The plugins are instruments: a held note is what makes them sound. In a scene
every layer is played by one note, held for as long as the layer exists — middle
C, at velocity 0.9. Middle C is the pitch every plugin's note tracking treats as
neutral, so *Note Tracking*, *Filter Key Track* and the like change nothing.

Velocity is less tidy: the plugins do not share a neutral one. Five of them
reference their velocity controls to full velocity, four to half. What they do
share is how their presets were made — every preset in the suite was fitted and
rendered at 0.9 — so 0.9 is the velocity at which a layer sounds the way its
preset does in its own plugin. The *Velocity to ...* controls still act, as they
would on a note played at 0.9.

The note also gives every layer its own envelope back. Adding a layer fades it in
on its plugin's *Attack*; removing one lets it fade out on its *Release* before
the engine is put away.

## Thunder flashes at random

ThunderClap has three modes, and a scene has use for one of them: *Storm*, which
keeps flashing for as long as its note is held. A thunder layer is held in Storm,
and its *Mode* control is left off the page. Flashes arrive as a Poisson process
at *Storm Rate*, never on a grid, and every one is grown afresh at its own
distance, height and bearing around *Distance*.

A real storm overhead flashes several times a minute; one passing at a distance,
heard and not seen, rumbles every half minute or so. The factory scenes use two
or three a minute. The first flash comes at a random moment within the storm's
first average interval — never the instant the layer starts, so a scene does not
open on a thunderclap, and never so late that a new storm seems to do nothing.
What you hear of it can come a good deal later than the flash itself: thunder
travels at the speed of sound, and a strike five kilometres off takes fifteen
seconds to arrive.

## Birds and the night sing by themselves

ChirpParade and NightLife have two halves. The flock — or the pack, or the chorus
— sings unprompted while a note is held, and that half needs nothing from a
scene. The other half is the phrase a note fires: one bird, one call, where you
asked for it.

A scene fires that phrase at random instead, at the layer's **Shot Rate**, and at
the plugin's own *Shot Level*. So a preset built around one deliberate bird —
*Nightingale Song*, *Robin Whistle*, *Blackbird Song* — has its bird in a scene
too, singing now and then. With *Shot Level* off nothing fires, whatever the rate.

## The mixer

One strip per layer, in the order of the tabs, and the scene's own strip at the
end. A strip's fader is the layer's **Level**, on top of its plugin's own
*Output Gain*; the bars beside it are the layer's level after the fader.

**Pan** works two ways. With the strip on *STEREO* it is a balance: the far side
comes down and the layer keeps its own stereo image. On *MONO* the layer is
folded to one point and pan places that point, with the same power wherever it
is — a stream off to the left, an owl in one particular tree.

**Mute and solo are not saved**, in the scene or in the project. They are for
listening while you build a scene. A scene that remembered a mute would bring a
layer back silent and look as if it were broken.

## The scene's envelope, filter and output

Everything the layers make is summed and then passes through the scene's own
processing, in this order: the **highpass**, the **filter**, the **envelope**,
**Width**, and **Output Gain**, ending in the suite's soft clip.

**Gate** is what opens the envelope. *Always* opens it when the plugin starts and
never closes it. *Transport* follows the host's play and stop. *Notes* holds it
open while any note is held — on any key; the note only opens and closes the
scene and never reaches a layer. With the gate closed and the release run out,
the layers are paused rather than run unheard.

## Scene files

A scene is a text file in the suite's preset format with layers in it. Each
`layer =` line opens a section for one layer, named by its plugin, and what
follows is that plugin's own preset keys in its own units — a section copied out
of a scene is a working preset of that plugin. A section may also start from one
of the plugin's presets by name, which is how the factory scenes are written:

```
layer = ThunderClap
layer_from = Rolling Thunder
layer_level = -1
storm_rate = 3
```

The `layer_` keys are the strip's: level, pan, stereo and, for birds and night,
shot rate. A scene saved from the window writes every value out in full, so it
does not change if a factory preset it started from is ever refitted.

# The plugin window

{{PLUGIN}} draws the suite's own window with X11 and Cairo on Linux, and Cairo on
the Win32 backend on Windows. There is no toolkit.

Along the top is a tab for the scene and a tab for each layer, and *+* to add one.
The **scene page** is the mixer and the scene's own panels. A **layer's page** is
its plugin's own panel layout, in the plugin's own colour, under a bar with the
layer's preset — browse, step through, save — its level, pan, stereo switch, mute,
solo and *REMOVE*. The open tab can also be closed with its cross; a tab that is
not open never removes anything when clicked.

The bar at the bottom is always the scene's. Drag a knob to edit it, double-click
to reset it, shift-drag for fine control, and click a value to type one.

The header draws a ribbon for every layer of the scene, in the layer's colour and
swaying as hard as it is loud. On a layer's page it draws that plugin's own
ornament instead.

# The parameter reference

What follows are {{PLUGIN}}'s own controls: the scene's, and the four that place a
layer in it, which every layer has — the *Layer* table's keys are the ones a
scene file uses in a layer's section. Everything else a layer has is its
plugin's, exactly as that plugin's own manual describes it. In the host all of
it is one list of 2,119 parameters, named by layer.

{{PARAMETER_REFERENCE}}

# The preset library

{{PRESET_LIBRARY}}
