#include "verdaliscene.h"

#include "params.h"

namespace verdaliscene {

const PresetContext &presetContext() {
   static const PresetContext ctx{kPluginName, kPresetExtension, sceneParamTable(),
                                  kNumSceneParams};
   return ctx;
}

std::string factoryPresetDir() { return verdalis::factoryPresetDir(presetContext()); }

std::string userPresetDir() { return verdalis::userPresetDir(presetContext()); }

} // namespace verdaliscene
