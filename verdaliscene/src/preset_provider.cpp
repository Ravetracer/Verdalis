// Preset discovery for VerdaliScene. The provider itself is shared by the suite;
// this only says which plugin it is serving. A scene file reads as an ordinary
// preset to it: its name, description and features, and nothing about layers.

#include "factories.h"
#include "params.h"
#include "presets_generated.h"
#include "verdaliscene.h"

#include "verdalis/preset_provider.h"

namespace verdaliscene {

namespace {

const PresetProviderSpec kSpec = {
   {kPluginName, kPresetExtension, sceneParamTable(), kNumSceneParams},
   kProviderId,
   kPluginId,
   kPluginVendor,
   "VerdaliScene nature scene preset",
   kBuiltinPresets,
   kNumBuiltinPresets,
};

} // namespace

// Built on first use rather than at static-initialisation time, so the
// parameter table it points at is certainly alive by then.
const clap_preset_discovery_factory_t *presetDiscoveryFactory() {
   static const clap_preset_discovery_factory_t *f = verdalis::presetDiscoveryFactory(kSpec);
   return f;
}

} // namespace verdaliscene
