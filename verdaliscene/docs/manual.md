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

**A scene is a drone you hold.** Press a key and the whole place fades in; let
go and it fades out. While the key is down nothing else is asked of you: every
layer plays on its own, the thunder flashes when it flashes and the birds sing
when they sing. Which key does not matter, and no key ever plays a single event.

> **{{PLUGIN}} {{VERSION}} has effects.** Every layer, and the scene as a whole,
> has its own reverb, delay, chorus, flanger, phaser, stereo widener and
> auto-pan, saved with the scene. Since 0.2.0 a key starts a scene and letting go
> ends it, every layer fades in and out on its own envelope as well as the
> scene's, the mixer's strips carry each layer's envelope and filter, and every
> envelope's stages can be bent. Projects and scenes saved with an earlier
> version sound as they did: they carry their own *Gate*, and every effect starts
> switched off.

## What is in this manual

*Installing* is the boring part. *A first scene* is five minutes. *How it works*
is what a layer is, how a held key plays a whole scene, and what the scene adds
on top. *The plugin window* is the tabs and the mixer. *The parameter
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

The browser files the library in **collections**, exactly as in every other
Verdalis plugin: `NEW`, `RENAME` and `DELETE` under its list of collections, a
preset dragged onto one moves there, and a right-click on one of your own renames
it, describes it or deletes it. `SAVE` asks which collection a scene goes into,
with a name and a description for whoever loads it next. A whole collection --
only the scenes in it -- exports as one **preset pack** and imports again as a
collection of its own. The same goes for every layer's library.

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
2. Choose **Forest River** in the scene bar at the bottom, and hold any key. The
   scene fades in over a couple of seconds; let go and it fades out again. The
   *GATE* light on the scene's strip is lit while the key is down.
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
- **Shape how it comes and goes.** Drag the release in a strip's envelope graph
  upwards and the layer hangs on after the key before it goes; drag it down and
  it drops away at once. The scene's own graph, on the strip at the right, does
  the same for everything together.
- **Put a sound in a place.** Click the row of letters on the birds' strip to
  open their effects, switch on the **Reverb** and give it a long *Decay*: the
  birds are now singing in a large, open space while everything else stays where
  it was. Load **Canyon Echo** or **Cave Mouth** to hear what the effects were
  built for.
- **Tie it to the song.** Set *Gate* to *Transport* and the scene fades in when
  the host plays and out when it stops, on the *Attack* and *Release* you set. On
  *Always* it plays from the moment the plugin runs, with no key and no
  transport at all.

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
every layer is played by one note of its own, held for as long as the scene's
gate is open — middle C, at velocity 0.9, whatever key opened the gate. Middle C
is the pitch every plugin's note tracking treats as neutral, so *Note Tracking*,
*Filter Key Track* and the like change nothing.

Velocity is less tidy: the plugins do not share a neutral one. Five of them
reference their velocity controls to full velocity, four to half. What they do
share is how their presets were made — every preset in the suite was fitted and
rendered at 0.9 — so 0.9 is the velocity at which a layer sounds the way its
preset does in its own plugin. The *Velocity to ...* controls still act, as they
would on a note played at 0.9.

The note also gives every layer its own envelope back. Opening the gate strikes
every layer's note, so each fades in on its plugin's *Attack*; closing it lets
every note go, so each fades out on its own *Release* — inside the scene's
envelope, which shapes the sum. A slow layer under a quick scene release is cut
short by the scene; a quick layer under a slow one is gone before the scene is.
Adding a layer while the gate is open fades it in, and removing one lets it fade
out before the engine is put away.

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
end. More layers than fit side by side scroll, with the arrows beside *MIXER* or
the wheel over a layer's name.

A strip is a channel strip. Under the layer's name is its **envelope**, drawn as
it will play, with the plugin's *Attack*, *Decay*, *Sustain* and *Release* as
small knobs beneath it; then the plugin's **filter** — its type, *Highpass*,
*Cutoff* and *Resonance*. These are the plugin's own controls, the same ones on
the layer's page: turn one on the strip and the page has moved too. Thunder's
envelope has no decay or sustain, so those two are left blank on its strip. A
knob's value shows in the help line while it is under the pointer.

The strip's fader is the layer's **Level**, on top of its plugin's own *Output
Gain*; the bars beside it are the layer's level after the fader.

**Pan** works two ways. With the strip on *STEREO* it is a balance: the far side
comes down and the layer keeps its own stereo image. On *MONO* the layer is
folded to one point and pan places that point, with the same power wherever it
is — a stream off to the left, an owl in one particular tree.

**Mute and solo are not saved**, in the scene or in the project. They are for
listening while you build a scene. A scene that remembered a mute would bring a
layer back silent and look as if it were broken.

The scene's own strip, on the right, has the same shape: the scene's envelope and
filter, *Output Gain* on the fader, *Width* under it, and the **GATE** light.

## Bending an envelope

Every envelope here — the scene's and each layer's — has three stages that move:
the fade-in, the fall to the sustain level, and the fade-out. Each can be bent
on its own, by **Attack Curve**, **Decay Curve** and **Release Curve**.

At 0 % a stage has its natural shape, the analogue one every Verdalis plugin has
always had: the fade-in rises quickly and eases into full level, and the decay
and the release fall quickly and trail off. Towards −100 % a stage first
straightens into a line and then holds back — a fade-in that swells in late, a
release that hangs on and then goes. Towards +100 % it moves at once and then
settles. The length of a stage never changes, only the path it takes; and a
curve of exactly 0 % is the plugin's own envelope, sample for sample.

The quickest way to bend one is in an envelope graph: drag a stage up or down
and it follows the hand, and double-click it for its natural shape again. The
flat stretch in the middle is the sustain level, and dragging it sets that. The
graph spreads its stages over a logarithmic time scale, so a 2 ms attack and a
20 s release can both be seen and grabbed; within each stage the shape is drawn
true.

A layer's curves are the scene's, like its level and pan: they are saved in the
scene and stay when another preset is loaded into the layer, but a layer saved
as a preset of its plugin does not carry them, because the plugin has no such
control. The scene's three are on the *ENVELOPE* panel under the mixer; a
layer's are in the *SHAPE* graph in its bar and on its strip.

## Effects

Every layer has a chain of effects of its own, and so does the scene as a whole:

> phaser → chorus → flanger → **delay** → **reverb** → widener → auto-pan

Modulation first, then the echoes, then the space they happen in, then where the
result sits — echoes into a reverb, the way a producer would patch them by hand.
Each effect is switched on and off on its own; a switched-off effect costs
nothing, and its settings stay where they were. Switching one fades it in or out
over a few milliseconds, and an effect switched back on starts clean rather than
bringing back the tail it had.

**Where they sit.** A layer's effects come after its placement and before its
fader: a bird folded to *MONO* and panned left goes into its reverb from the left,
and the reverb spreads it into the room the way a real one would. The scene's
effects come after the layers are summed and filtered.

**Tails.** *FX Tails*, on the scene's OUTPUT panel, decides what the scene's
release does to the effects:

- **Ring Out**, the default: the scene's envelope fades what goes *into* every
  chain, the layers' and the scene's. When the key is let go the scene fades on
  its release, and every reverb and delay rings on after it, for as long as its
  own tail lasts. A cave stays a cave after the drips have stopped.
- **Release**: the envelope fades what comes *out* of them, after every effect.
  The tails fade with the rest of the scene and are gone when its release has
  run out; the next key starts in silence, whatever the effects were doing.

Before 0.4.0 it was neither: the layers' tails were cut by the scene's release
and the scene's own rang on.

**Opening them.** On every mixer strip, under the filter, is a row of letters —
**P C F D R W A**, one per effect in chain order, lit when that effect is on.
Click the row, or *FX* in a layer's bar, and the page shows that channel's
effects: the chain along the top as switches, and a panel per effect with its
switch in the title. Switched-off effects are drawn faded and can still be set
up. *BACK* returns to where you came from. Every knob drags, types and resets
exactly as on any other page.

**The effects are the scene's, not the plugin's**, like a layer's level and pan:
they are saved in the scene, they stay when another preset is loaded into the
layer, and a layer saved as a preset of its plugin does not carry them.

### Reverb

A sixteen-line feedback delay network, the design Jot described and both Pirkle
and Zölzer's *DAFX* recommend for smooth, long tails. Every line loses exactly as
much as its length calls for, so every mode decays at the same rate and no single
one rings on metallic; and every line drifts slowly, which keeps even a
two-minute tail from settling into fixed resonances.

- **Decay** is the time to fall by 60 dB, from 0.2 s to two minutes, and holds at
  every *Size*. **Freeze** holds the tail as it is, indefinitely, and lets nothing
  new in — a pad made of whatever was playing.
- **Size** is the space's dimension: the spacing of the echoes, not their length.
  **Pre-Delay** is the gap before the tail, up to a second.
- **Diffusion** is how quickly the first echoes melt into a wash.
- **Damping** and **Damp Freq** make the top end die faster than the rest above a
  frequency — soft surfaces, or air over a long distance.
- **Mod Depth** and **Mod Rate** set the drift. More depth is lusher; at the top
  it starts to chorus.
- **Low Cut** and **High Cut** carve the reverb alone, 12 dB/oct each, without
  touching the dry sound: a cave without its rumble, a hall without its hiss.
- **Width** runs from a mono tail to a fully decorrelated one. **Mix** is equal
  power. A long tail carries far more energy than a short one, so the wet level
  comes down a little as *Decay* goes up.

### Delay

- **Mode**: *Mono* is one line heard in both ears; *Stereo* is a line per side;
  *Ping-Pong* starts the repeats on the left and bounces them across. **Offset**
  makes the right side up to half again longer or shorter, which turns a
  ping-pong into a skipping pattern and a stereo delay into a spread.
- **Time** runs from 1 ms to 8 s. With **Sync** on, **Note** sets it as a note
  value at the host's tempo, triplets and dotted values included.
- **Feedback** goes to 150 %. Up to 100 % the repeats die away; beyond, they build
  into a held, slowly darkening wall, the way a tape echo pushed past
  self-oscillation does. The loop saturates rather than running away, and
  **Saturation** drives it harder.
- **Low Cut** and **High Cut** sit in the loop, so every repeat is thinner or
  darker than the one before.
- **Wow** and **Wow Rate** make the time waver like tape, by a slow swing and a
  slower random drift.
- **Diffusion** smears each repeat a little further into a cloud while keeping
  the time between them.
- **Ducking** pulls the repeats down while the layer is loud and lets them up as
  it falls quiet.
- **Glide** says how a time change arrives: *Tape* slides to it and bends the
  pitch of the repeats on the way; *Fade* crossfades to it with no pitch sweep,
  which suits jumping between note values.
- **Width** and **Mix** as for the reverb.

### Chorus, flanger and phaser

The **chorus** is up to four drifting copies a few to forty milliseconds behind
the original, each running a little faster than the last so the pattern never
repeats; **Width** moves the right side's copies a quarter cycle on. The
**flanger** sweeps a very short delay against the original and feeds it back —
positive for the bright jet, negative for the hollow one; *Mix* at 50 % gives the
deepest notches. The **phaser** sweeps four, six or eight allpass stages around
**Centre**, with feedback for a more vocal sound.

### Widener and auto-pan

The **widener** works on the difference between left and right. **Width** scales
it, from mono to double. **Spread** makes a narrow sound wide by sending different
frequencies to different places; it sums back to the original in mono, so it
cannot make a mix collapse. **Bass Mono** centres everything below a frequency.

The **auto-pan** moves the whole channel across the field — a flock passing, wind
moving down a valley. *Sine* and *Triangle* swing evenly; *Drift* wanders to a new
random place each cycle and never stops.

## The scene's envelope, filter and output

Everything the layers make is summed and then passes through the scene's own
processing, in this order: the **highpass**, the **filter**, the scene's
**effects**, **Width**, and **Output Gain**, ending in the suite's soft clip. The
**envelope** sits where *FX Tails* puts it: in front of every effect on *Ring
Out*, so the tails ring on past it, or after the scene's effects on *Release*,
so they fade with it (see *Effects*).

**Gate** is what starts and ends the scene. *Notes*, the default, holds it open
while any key is held — any key at all; the key only opens and closes the scene
and never plays a single event in a layer. *Transport* follows the host's play
and stop. *Always* opens it when the plugin starts and never closes it, which is
the 0.1.0 behaviour. Opening the gate starts the scene's envelope and every
layer's note; closing it releases all of them. Once the gate is closed and the
scene's release has run out, the layers are stopped rather than run unheard --
all but what their effects still hold on *Ring Out* -- and the next opening
starts them afresh.

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

The `layer_` keys are the layer's place in the scene: level, pan, stereo, the
three envelope curves and, for birds and night, shot rate. The `fx_` keys are
effects: among the scene's own keys for the scene's chain, in a layer's section
for that layer's. An effect is only written when it is on or has been changed,
so a scene without effects reads as it always did. A scene saved from
the window writes every value out in full — every layer's every parameter, the
curves and the scene's own — so it does not change if a factory preset it
started from is ever refitted. Only mute and solo are left out.

# The plugin window

{{PLUGIN}} draws the suite's own window with X11 and Cairo on Linux, and Cairo on
the Win32 backend on Windows. There is no toolkit.

Along the top is a tab for the scene and a tab for each layer, and *+* to add one.
The **scene page** is the mixer and the scene's own panels. A **layer's page** is
its plugin's own panel layout, in the plugin's own colour, under a bar with the
layer's preset — browse, step through, save — its envelope graph (*SHAPE*), *FX*
for its effects, its level, pan, stereo switch, mute, solo and *REMOVE*. The
**effects view** takes the place of a page: one channel's effects, opened from a
strip's row of letters or a layer's *FX*. The open tab can also be closed with its cross; a tab that is
not open never removes anything when clicked.

The bar at the bottom is always the scene's. Drag a knob to edit it, double-click
to reset it, shift-drag for fine control, and click a value to type one.

The header draws a ribbon for every layer of the scene, in the layer's colour and
swaying as hard as it is loud. On a layer's page it draws that plugin's own
ornament instead.

# The parameter reference

What follows are {{PLUGIN}}'s own controls: the scene's, the ones that place a
layer in it and bend its envelope, which every layer has — the *Layer* table's
keys are the ones a scene file uses in a layer's section — and the effects, which
the scene and every layer have one set each of. Everything else a layer has is
its plugin's, exactly as that plugin's own manual describes it. In the host all
of it is one list of 4,484 parameters, named by layer: *Rain 1 Reverb Decay*,
*Scene Delay Time*.

{{PARAMETER_REFERENCE}}

# The preset library

{{PRESET_LIBRARY}}
