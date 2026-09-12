#pragma once

#include <string>

#include "verdalis/preset.h"

namespace thunderclap {

// The preset format, PresetData and the directory conventions are the suite's;
// this plugin only supplies its own name, extension and parameter table.
using namespace verdalis;

constexpr char kPluginId[] = "de.ravetracer.thunderclap";
constexpr char kPluginName[] = "ThunderClap";
constexpr char kPluginVendor[] = "Ravetracer";
constexpr char kPluginVersion[] = "1.4.0";
constexpr char kPluginUrl[] = "https://github.com/Ravetracer/Verdalis";
constexpr char kPluginDescription[] =
   "Fully synthetic thunder. No samples: every flash grows its own lightning channel and "
   "every shock wave is computed.";

constexpr char kPresetExtension[] = "thunderclap";
constexpr char kProviderId[] = "de.ravetracer.thunderclap.preset-provider";

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

} // namespace thunderclap
