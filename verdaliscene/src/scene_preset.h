#pragma once

// The scene preset format, and the layer presets it is built from.
//
// A scene file is the suite's preset format with layers in it:
//
//    # VerdaliScene preset
//    format = 1
//    name = Forest River
//    description = ...
//
//    # Envelope
//    gate = Always
//    attack = 2500
//    ...
//
//    layer = RiverFlow
//    layer_from = Forest Stream
//    layer_level = -4
//    flow_level = -9
//
//    layer = ChirpParade
//    ...
//
// Everything before the first `layer =` is the scene's own parameters. Each
// `layer =` line opens a section for one layer, named by its plugin, and the
// lines in it are that plugin's own preset keys in that plugin's own units --
// a section is a single-plugin preset body, and one copied out of a scene is a
// working preset of that plugin. Besides them a section takes the layer_ keys
// from params.cpp (where the layer sits in the scene), `layer_preset` (the
// name the layer's preset browser shows) and `layer_from`, which loads one of
// the plugin's presets by name before the section's own lines are applied.
//
// layer_from is what keeps the factory scenes short and readable: a scene is
// "Forest Stream, a little quieter", not sixty numbers. A scene the window
// saves writes every value out instead, so it does not change when a factory
// preset is refitted.
//
// The shared parser can read a scene file too -- it ignores what it does not
// know -- which is how the preset browser and host discovery get a scene's
// name and description without knowing anything about layers.

#include <string>
#include <vector>

#include "params.h"

namespace verdaliscene {

struct SceneLayer {
   int type = kLayerRain;
   // What the layer's preset browser shows as loaded.
   std::string presetName;
   // The plugin's parameters, raw, indexed by its own ParamId.
   std::vector<double> values;
   // The placement parameters, raw, indexed by SlotParamId. Active is unused.
   double mix[kNumSlotParams] = {};
};

struct Scene {
   std::string name;
   std::string author;
   std::string description;
   std::vector<std::string> features;
   double values[kNumSceneParams] = {};
   std::vector<SceneLayer> layers;
   // Lines that were understood but could not be honoured: an unknown layer,
   // a layer_from naming no preset. Loading goes on without them; the factory
   // self-test treats any of them as a failure.
   std::vector<std::string> warnings;
};

// The scene's own parameters at their defaults, and no layers.
Scene emptyScene();

// A layer of `type` with the plugin's parameter defaults and the placement
// defaults.
SceneLayer defaultLayer(int type);

// What adding a layer in the window gives: the plugin's start preset, then the
// scene's own defaults for that kind of layer on top.
SceneLayer freshLayer(int type);

bool parseScene(const char *text, size_t length, Scene &out, std::string &error);
std::string formatScene(const Scene &scene);

// One of the plugin's presets, applied over `layer`'s values: every parameter
// the preset names takes its value, everything else goes back to its default
// first. `text` is the preset file.
bool applyLayerPreset(int type, const std::string &text, SceneLayer &layer, std::string &error);

// The text of one of the plugin's presets by name: the factory set first,
// then the user's own library. Empty when there is none of that name.
std::string findLayerPresetText(int type, const std::string &name);

// A layer's values as a preset of its own plugin, ready to write into that
// plugin's user library.
std::string formatLayerPreset(const SceneLayer &layer, const std::string &name);

} // namespace verdaliscene
