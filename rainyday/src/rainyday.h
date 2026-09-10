#pragma once

#include <string>

#include "verdalis/preset.h"

namespace rainyday {

// The preset format, PresetData and the directory conventions are the suite's;
// this plugin only supplies its own name, extension and parameter table.
using namespace verdalis;

constexpr char kPluginId[] = "de.ravetracer.rainyday";
constexpr char kPluginName[] = "RainyDay";
constexpr char kPluginVendor[] = "Ravetracer";
constexpr char kPluginVersion[] = "1.8.0";

// Nothing else keeps the two version sites in step, and drift here is quiet:
// the GUI drew a hardcoded "1.0.0" for the whole of 1.5.0's development because
// it never read this constant. It does now, and the build fails if the version
// here and the one in CMakeLists.txt disagree.
#ifdef RAINYDAY_CMAKE_VERSION
constexpr bool sameString(const char *a, const char *b) {
   return *a == *b && (*a == '\0' || sameString(a + 1, b + 1));
}
static_assert(sameString(kPluginVersion, RAINYDAY_CMAKE_VERSION),
              "kPluginVersion and the CMake project() version disagree");
#endif
constexpr char kPluginUrl[] = "https://github.com/Ravetracer/Verdalis";
constexpr char kPluginDescription[] =
   "Fully synthetic rain generator. No samples: every droplet is computed.";

constexpr char kPresetExtension[] = "rainyday";
constexpr char kProviderId[] = "de.ravetracer.rainyday.preset-provider";

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

} // namespace rainyday
