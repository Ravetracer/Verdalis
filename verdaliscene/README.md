# VerdaliScene

A whole place in one plugin. VerdaliScene layers the nine nature instruments of
the Verdalis suite — rain, thunder, waves, wind, birds, rivers, fire, insects and
the night — and mixes them into one scene: a stream through a summer wood with
birds in the canopy, a shore in a thunderstorm, a fire burning down under the
stars. Like everything in the suite it is synthesised, not sampled, so no two
playbacks are the same.

## A layer is the plugin itself

VerdaliScene contains no copy of any other plugin. A rain layer is RainyDay's own
engine, driven by RainyDay's own parameter table, offering RainyDay's own preset
library and showing RainyDay's own panels. The nine plugins are compiled into
VerdaliScene from their own folders in this repository, so a change to one of
them is a change to every layer of that kind, with nothing here to keep in step.

That is what makes the layers **compatible with the plugins**:

- every layer has every parameter its plugin has, under the same names, with the
  same ranges and the same meaning;
- any of the plugin's presets loads into a layer — the factory set and your own,
  from the same folder the plugin itself saves to;
- a layer saves back as a preset of its plugin, which then shows up in the plugin
  too.

Up to four layers of each kind can run at once — three rains on three surfaces,
two winds, one storm. A layer costs what the plugin costs; how many to run is
yours to decide.

## Only a drone

A scene plays by itself. No note ever fires a single event in a layer: each layer
is played by one held note for as long as it exists — middle C, which puts every
note-tracking control at its neutral point, at velocity 0.9, which is the velocity
every preset in the suite was fitted and rendered at, so a layer sounds like its
preset does in its plugin's own demos. The few things a plugin only does for a
played note are handled for a scene instead:

- **Thunder flashes at random.** A thunder layer is ThunderClap held in its Storm
  mode, which flashes as a Poisson process at *Storm Rate* — a realistic two or
  three flashes a minute in the factory scenes, anywhere from one to sixty if you
  want. Each flash is placed at its own distance, height and bearing, and the
  first one comes at a random moment soon after the layer starts rather than on
  its first sample.
- **Birds and night animals sing by themselves.** Their flock or pack is the
  drone half of the plugin and needs nothing. The phrase a note would have
  fired — a nightingale's song, a robin's whistle — fires at random instead, at
  the layer's *Shot Rate*, so a preset built around one deliberate bird still has
  that bird in it.

## The scene

The **mixer** has a strip per layer: a fader with the layer's level meter, pan, a
stereo or mono switch — mono folds a layer to one point that pan then places,
for a sound that should come from one direction — and mute and solo, which are
for listening and are never saved.

Over the whole scene sit an **envelope** with three ways to open it — always,
with the host's transport, or while any note is held — a **filter** and highpass,
a width control and the output gain. Notes only ever open and close the scene.

Sixteen factory **scenes** come with it, from *Forest River* and *Stormy Shore* to
*Frog Pond*, *Winter Cabin* and *Dawn Chorus*.

## The window

The suite's window, with a tab for the scene and a tab for every layer. The scene
page is the mixer and the scene's own controls. A layer's page is its plugin's own
panel layout in the plugin's own colour, with the layer's preset and its place in
the scene in a bar along the top. *+* adds a layer, *REMOVE* takes one away, and
it fades out on its own release.

## Scene presets

A scene file is the suite's text preset format with sections in it, one per
layer, and each section is written in its plugin's own preset keys — a section
copied out of a scene is a working preset of that plugin. A section can also start
from one of the plugin's presets by name:

```
layer = RiverFlow
layer_from = Forest Stream
layer_level = -4
flow_level = -9
```

which is how the factory scenes are written. A scene saved from the window writes
every value out instead.

## Build and install

```sh
./install.sh                # configure, build, self-test, install to ~/.clap
./install.sh --vst3         # also build and install the VST3
```

Needs a C++17 compiler, CMake, Ninja, X11 and Cairo, and the CLAP headers beside
the suite in `../CLAP/clap/include` — and the nine plugin folders beside this one,
which it is built from.

## Offline tools

```sh
verdaliscene-render --list                         # walk the preset discovery factory
verdaliscene-render --preset forest_river --out scene.wav --seconds 30
verdaliscene-render --preset some.verdaliscene --layers   # each layer's level, solo
verdaliscene-render --all --outdir /tmp/scenes     # every factory scene
verdaliscene-render --selftest                     # what install.sh runs
```

`tools/analysis/make_scenes.py` writes the factory scenes from one table, placing
each layer by the level `--layers` measured for it.

## Licence

MIT, with the suite. See `../LICENSE`.
