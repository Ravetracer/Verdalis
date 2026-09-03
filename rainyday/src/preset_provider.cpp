// Preset discovery: tells the host where RainyDay presets live and what is in
// them, so they show up in the host's own browser (Bitwig's Sound Content
// browser, for example) without the plugin needing a GUI of its own.

#include <cstring>
#include <string>

#include <sys/stat.h>

#include <clap/clap.h>

#include "factories.h"
#include "params.h"
#include "presets_generated.h"
#include "rainyday.h"

namespace rainyday {

namespace {

const clap_preset_discovery_provider_descriptor_t kProviderDescriptor = {
   CLAP_VERSION_INIT,
   kProviderId,
   "RainyDay Presets",
   kPluginVendor,
};

const clap_universal_plugin_id_t kUniversalId = {"clap", kPluginId};

struct Provider {
   clap_preset_discovery_provider_t iface{};
   const clap_preset_discovery_indexer_t *indexer = nullptr;
   std::string factoryDir;
   std::string userDir;
   bool useBundled = false;

   static Provider *from(const clap_preset_discovery_provider_t *p) {
      return static_cast<Provider *>(p->provider_data);
   }
};

// Reports one preset's metadata to the indexer.
bool describePreset(const clap_preset_discovery_metadata_receiver_t *rx, const PresetData &preset,
                    const char *loadKey, bool factoryContent) {
   if (!rx->begin_preset(rx, preset.name.c_str(), loadKey ? loadKey : ""))
      return false;

   rx->add_plugin_id(rx, &kUniversalId);
   rx->set_flags(rx, factoryContent ? CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT
                                    : CLAP_PRESET_DISCOVERY_IS_USER_CONTENT);
   if (!preset.author.empty())
      rx->add_creator(rx, preset.author.c_str());
   if (!preset.description.empty())
      rx->set_description(rx, preset.description.c_str());

   // Plugin category first so the host knows these presets produce an
   // instrument, then the preset's own descriptive tags.
   rx->add_feature(rx, CLAP_PLUGIN_FEATURE_INSTRUMENT);
   rx->add_feature(rx, CLAP_PLUGIN_FEATURE_SYNTHESIZER);
   for (const auto &f : preset.features)
      rx->add_feature(rx, f.c_str());

   return true;
}

bool providerInit(const clap_preset_discovery_provider_t *p) {
   Provider *self = Provider::from(p);
   const clap_preset_discovery_indexer_t *ix = self->indexer;
   if (!ix)
      return false;

   const clap_preset_discovery_filetype_t filetype = {
      "RainyDay Preset", "RainyDay rain synthesis preset", kPresetExtension};
   if (!ix->declare_filetype(ix, &filetype))
      return false;

   self->factoryDir = factoryPresetDir();
   self->userDir = userPresetDir();

   if (!self->factoryDir.empty()) {
      const clap_preset_discovery_location_t location = {
         CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT, "RainyDay Factory Presets",
         CLAP_PRESET_DISCOVERY_LOCATION_FILE, self->factoryDir.c_str()};
      ix->declare_location(ix, &location);
   } else {
      // No preset directory next to the binary (single-file install): fall back
      // to the copies compiled into the plugin itself, with the plugin acting
      // as its own preset container.
      self->useBundled = true;
      const clap_preset_discovery_location_t location = {
         CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT, "RainyDay Factory Presets",
         CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr};
      ix->declare_location(ix, &location);
   }

   // The user location is only declared when it already exists, so the plugin
   // never creates directories behind the user's back.
   if (!self->userDir.empty()) {
      struct stat st{};
      if (stat(self->userDir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
         const clap_preset_discovery_location_t location = {
            CLAP_PRESET_DISCOVERY_IS_USER_CONTENT, "RainyDay User Presets",
            CLAP_PRESET_DISCOVERY_LOCATION_FILE, self->userDir.c_str()};
         ix->declare_location(ix, &location);
      }
   }

   return true;
}

void providerDestroy(const clap_preset_discovery_provider_t *p) { delete Provider::from(p); }

bool providerGetMetadata(const clap_preset_discovery_provider_t *p, uint32_t locationKind,
                         const char *location,
                         const clap_preset_discovery_metadata_receiver_t *rx) {
   Provider *self = Provider::from(p);
   if (!rx)
      return false;

   if (locationKind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN) {
      // The plugin is the container: enumerate every built-in preset.
      for (unsigned i = 0; i < kNumBuiltinPresets; ++i) {
         PresetData preset;
         std::string error;
         if (!parsePreset(kBuiltinPresets[i].text, std::strlen(kBuiltinPresets[i].text), preset,
                          error))
            continue;
         if (preset.name.empty())
            preset.name = kBuiltinPresets[i].loadKey;
         if (!describePreset(rx, preset, kBuiltinPresets[i].loadKey, true))
            return true; // receiver asked us to stop
      }
      return true;
   }

   if (locationKind != CLAP_PRESET_DISCOVERY_LOCATION_FILE || !location || !location[0]) {
      rx->on_error(rx, 0, "unsupported preset location");
      return false;
   }

   PresetData preset;
   std::string error;
   if (!parsePresetFile(location, preset, error)) {
      rx->on_error(rx, 0, error.c_str());
      return false;
   }

   const bool isUser =
      !self->userDir.empty() && std::strncmp(location, self->userDir.c_str(), self->userDir.size()) == 0;
   // A single-preset file needs no load key.
   describePreset(rx, preset, nullptr, !isUser);
   return true;
}

const void *providerGetExtension(const clap_preset_discovery_provider_t *, const char *) {
   return nullptr;
}

uint32_t factoryCount(const clap_preset_discovery_factory_t *) { return 1; }

const clap_preset_discovery_provider_descriptor_t *
factoryGetDescriptor(const clap_preset_discovery_factory_t *, uint32_t index) {
   return index == 0 ? &kProviderDescriptor : nullptr;
}

const clap_preset_discovery_provider_t *
factoryCreate(const clap_preset_discovery_factory_t *,
              const clap_preset_discovery_indexer_t *indexer, const char *providerId) {
   if (!indexer || !providerId || std::strcmp(providerId, kProviderId) != 0)
      return nullptr;

   auto *self = new Provider();
   self->indexer = indexer;
   self->iface.desc = &kProviderDescriptor;
   self->iface.provider_data = self;
   self->iface.init = providerInit;
   self->iface.destroy = providerDestroy;
   self->iface.get_metadata = providerGetMetadata;
   self->iface.get_extension = providerGetExtension;
   return &self->iface;
}

} // namespace

const clap_preset_discovery_factory_t gPresetDiscoveryFactory = {factoryCount,
                                                                 factoryGetDescriptor,
                                                                 factoryCreate};

} // namespace rainyday
