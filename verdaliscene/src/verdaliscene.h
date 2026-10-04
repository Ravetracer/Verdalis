#pragma once

#include <string>

#include "verdalis/preset.h"

namespace verdaliscene {

// The preset format, PresetData and the directory conventions are the suite's;
// this plugin only supplies its own name, extension and parameter table -- and
// a scene preset carries its layers in a format of its own on top (see
// scene_preset.h).
using namespace verdalis;

constexpr char kPluginId[] = "de.ravetracer.verdaliscene";
constexpr char kPluginName[] = "VerdaliScene";
constexpr char kPluginVendor[] = "Ravetracer";
constexpr char kPluginVersion[] = "0.4.0";

#ifdef VERDALISCENE_CMAKE_VERSION
constexpr bool sameString(const char *a, const char *b) {
   return *a == *b && (*a == '\0' || sameString(a + 1, b + 1));
}
static_assert(sameString(kPluginVersion, VERDALISCENE_CMAKE_VERSION),
              "kPluginVersion and the CMake project() version disagree");
#endif
constexpr char kPluginUrl[] = "https://github.com/Ravetracer/Verdalis";
constexpr char kPluginDescription[] =
   "Nature scenes from every Verdalis instrument at once: rain, thunder, waves, wind, "
   "birds, rivers, fire, insects and the night, layered and mixed. No samples.";

constexpr char kPresetExtension[] = "verdaliscene";
constexpr char kProviderId[] = "de.ravetracer.verdaliscene.preset-provider";

// The scene's preset context. Its table is the scene's own parameters only;
// the layers in a scene preset are read by scene_preset.cpp, not by the
// shared parser, which only ever sees a scene's name and description.
const PresetContext &presetContext();

std::string factoryPresetDir();
std::string userPresetDir();
using verdalis::writePresetFile;

} // namespace verdaliscene
