#pragma once

#include <string>

#include "verdalis/preset.h"

namespace nightlife {

// The preset format, PresetData and the directory conventions are the suite's;
// this plugin only supplies its own name, extension and parameter table.
using namespace verdalis;

constexpr char kPluginId[] = "de.ravetracer.nightlife";
constexpr char kPluginName[] = "NightLife";
constexpr char kPluginVendor[] = "Ravetracer";
constexpr char kPluginVersion[] = "0.2.0";

// Nothing else keeps the two version sites in step, and drift here is quiet.
// The build fails if the version here and the one in CMakeLists.txt disagree.
#ifdef NIGHTLIFE_CMAKE_VERSION
constexpr bool sameString(const char *a, const char *b) {
   return *a == *b && (*a == '\0' || sameString(a + 1, b + 1));
}
static_assert(sameString(kPluginVersion, NIGHTLIFE_CMAKE_VERSION),
              "kPluginVersion and the CMake project() version disagree");
#endif
constexpr char kPluginUrl[] = "https://github.com/Ravetracer/Verdalis";
constexpr char kPluginDescription[] =
   "A synthesised night. No samples: a frog chorus built from measured pulse trains, "
   "an insect bed, and wolves, owls and foxes whose calls are measured pitch contours "
   "of real animals driving a voice -- as one call on a note, or a whole night of its "
   "own.";

constexpr char kPresetExtension[] = "nightlife";
constexpr char kProviderId[] = "de.ravetracer.nightlife.preset-provider";

// The plugin's own preset context, and the preset calls bound to it. Thin
// wrappers over the shared implementations so that call sites read unchanged.
const PresetContext &presetContext();

std::string factoryPresetDir();
std::string userPresetDir();
bool parsePreset(const char *text, size_t length, PresetData &out, std::string &error);
bool parsePresetFile(const std::string &path, PresetData &out, std::string &error);
std::string formatPreset(const PresetData &preset);
std::string userPresetPath(const std::string &name);
using verdalis::writePresetFile;

} // namespace nightlife
