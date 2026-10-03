// The whole parameter table: the scene's own parameters, then every layer
// slot's copy of its plugin's table and its placement parameters, at the ids
// params.h lays out. Built once, on first use, and never freed -- the host and
// the window both hold pointers into it for as long as the plugin is loaded.

#include "params.h"

#include <cstdio>
#include <deque>
#include <string>

namespace verdaliscene {

namespace {

struct FullTable {
   std::vector<ParamDesc> entries;
   std::vector<uint32_t> hostIds;
   // The generated names and keys. A deque, because ParamDesc keeps pointers
   // into these and a deque never moves what it already holds.
   std::deque<std::string> strings;

   const char *keep(std::string s) {
      strings.push_back(std::move(s));
      return strings.back().c_str();
   }

   FullTable() {
      entries.resize(kTableSize);
      for (uint32_t i = 0; i < kTableSize; ++i)
         entries[i] = ParamDesc{i, "", "", "", 0.0, 0.0, 0.0, ParamKind::Linear, 0.0, 0.0, "",
                                nullptr, 0, ""};

      const ParamDesc *scene = sceneParamTable();
      for (uint32_t i = 0; i < kNumSceneParams; ++i) {
         entries[i] = scene[i];
         hostIds.push_back(i);
      }

      for (int slot = 0; slot < kNumSlots; ++slot) {
         const LayerType &t = layerType(slotType(slot));
         char prefix[32];
         std::snprintf(prefix, sizeof(prefix), "%s %d", t.label, slotInstance(slot) + 1);
         char keyPrefix[32];
         std::snprintf(keyPrefix, sizeof(keyPrefix), "%s%d_", t.label, slotInstance(slot) + 1);
         for (char *c = keyPrefix; *c; ++c)
            if (*c >= 'A' && *c <= 'Z')
               *c = static_cast<char>(*c + 32);

         const ParamDesc *own = t.paramTable();
         for (uint32_t i = 0; i < t.paramCount; ++i) {
            const ParamDesc &src = own[i];
            const uint32_t id = slotParamId(slot, src.id);
            ParamDesc d = src;
            d.id = id;
            d.key = keep(std::string(keyPrefix) + src.key);
            d.name = keep(std::string(prefix) + " " + src.name);
            d.module = keep(std::string(prefix) + "/" + src.module);
            entries[id] = d;
            if (!pinnedParam(t, src.id))
               hostIds.push_back(id);
         }

         const ParamDesc *mix = slotParamTable();
         for (uint32_t p = 0; p < kNumSlotParams; ++p) {
            if (p == kSlotShotRate && t.shotLevelParam == kNoLayerParam)
               continue;
            const uint32_t id = slotMixId(slot, p);
            ParamDesc d = mix[p];
            d.id = id;
            d.key = keep(std::string(keyPrefix) + mix[p].key);
            d.name = keep(std::string(prefix) + " " + mix[p].name);
            d.module = keep(std::string(prefix) + "/Mixer");
            entries[id] = d;
            hostIds.push_back(id);
         }
      }
   }
};

const FullTable &table() {
   static const FullTable t;
   return t;
}

} // namespace

const ParamDesc *fullTable() { return table().entries.data(); }

const std::vector<uint32_t> &hostParamIds() { return table().hostIds; }

bool isRealParam(uint32_t id) { return id < kTableSize && table().entries[id].key[0] != 0; }

} // namespace verdaliscene
