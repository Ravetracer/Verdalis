// Preset discovery for ShoreBreak. The provider itself is shared by the suite;
// this only says which plugin it is serving.

#include "factories.h"
#include "params.h"
#include "presets_generated.h"
#include "shorebreak.h"

#include "verdalis/preset_provider.h"

namespace shorebreak {

namespace {

const PresetProviderSpec kSpec = {
   {kPluginName, kPresetExtension, paramTable(), kNumParams},
   kProviderId,
   kPluginId,
   kPluginVendor,
   "ShoreBreak rain synthesis preset",
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

} // namespace shorebreak
