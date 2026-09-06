#include "thunderclap.h"

#include "params.h"

namespace thunderclap {

// The preset format itself lives in shared/src/preset.cpp. All this plugin
// contributes is which name, extension and parameter table to use, and the
// wrappers that bind them so the rest of the plugin can call these without
// carrying a context around.
const PresetContext &presetContext() {
   static const PresetContext ctx{kPluginName, kPresetExtension, paramTable(), kNumParams};
   return ctx;
}

std::string factoryPresetDir() { return verdalis::factoryPresetDir(presetContext()); }

std::string userPresetDir() { return verdalis::userPresetDir(presetContext()); }

bool parsePreset(const char *text, size_t length, PresetData &out, std::string &error) {
   return verdalis::parsePreset(presetContext(), text, length, out, error);
}

bool parsePresetFile(const std::string &path, PresetData &out, std::string &error) {
   return verdalis::parsePresetFile(presetContext(), path, out, error);
}

std::string formatPreset(const PresetData &preset) {
   return verdalis::formatPreset(presetContext(), preset);
}

std::string userPresetPath(const std::string &name) {
   return verdalis::userPresetPath(presetContext(), name);
}

} // namespace thunderclap
