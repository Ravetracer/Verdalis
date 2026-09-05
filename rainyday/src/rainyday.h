#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rainyday {

constexpr char kPluginId[] = "de.ravetracer.rainyday";
constexpr char kPluginName[] = "RainyDay";
constexpr char kPluginVendor[] = "Ravetracer";
constexpr char kPluginVersion[] = "1.0.0";
constexpr char kPluginUrl[] = "https://github.com/";
constexpr char kPluginDescription[] =
   "Fully synthetic rain generator. No samples: every droplet is computed.";

constexpr char kPresetExtension[] = "rainyday";
constexpr char kProviderId[] = "de.ravetracer.rainyday.preset-provider";

// Absolute path to the "presets" directory that sits next to the loaded
// plugin binary, or an empty string if it cannot be determined.
std::string factoryPresetDir();

// Absolute path to the per-user preset directory ($XDG_CONFIG_HOME/RainyDay/
// presets). Not created by the plugin.
std::string userPresetDir();

// A parsed preset: metadata plus the raw (host-facing) parameter values it
// wants to set. Parameters absent from the file keep their current value.
struct PresetData {
   std::string name;
   std::string author;
   std::string description;
   std::vector<std::string> features;
   std::vector<std::pair<uint32_t, double>> values;
};

// Parses the RainyDay text preset format. Returns false and fills `error` on
// malformed input.
bool parsePreset(const char *text, size_t length, PresetData &out, std::string &error);
bool parsePresetFile(const std::string &path, PresetData &out, std::string &error);

// Writes a preset back out in the format parsePreset() reads: real-world units,
// grouped under the same module headings the factory library uses.
std::string formatPreset(const PresetData &preset);

// Turns a display name into the file it should be saved as, inside the user
// preset directory. Empty if that directory cannot be determined.
std::string userPresetPath(const std::string &name);

// Writes `text` to `path`, creating the directories above it. Returns false and
// fills `error` on failure.
bool writePresetFile(const std::string &path, const std::string &text, std::string &error);

} // namespace rainyday
