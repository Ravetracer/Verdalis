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
      addFx(kMasterFx, "Scene", "scene_");

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
            if (!slotParamApplies(t, p))
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
         addFx(slot, prefix, keyPrefix);
      }
   }

   // A channel's effects: "Rain 1 Reverb Decay", grouped as "Rain 1/Reverb".
   void addFx(int channel, const char *prefix, const char *keyPrefix) {
      const ParamDesc *fx = fxParamTable();
      for (uint32_t p = 0; p < kNumFxParams; ++p) {
         const uint32_t id = fxParamId(channel, p);
         ParamDesc d = fx[p];
         d.id = id;
         d.key = keep(std::string(keyPrefix) + fx[p].key);
         d.name = keep(std::string(prefix) + " " + fx[p].module + " " + fx[p].name);
         d.module = keep(std::string(prefix) + "/" + fx[p].module);
         entries[id] = d;
         hostIds.push_back(id);
      }
   }
};

const FullTable &table() {
   static const FullTable t;
   return t;
}

} // namespace

bool slotParamApplies(const LayerType &t, uint32_t p) {
   switch (p) {
   case kSlotShotRate:
      return t.shotLevelParam != kNoLayerParam;
   case kSlotDecayCurve:
      return layerParamByKey(t, "decay") != kNoLayerParam;
   default:
      return p < kNumSlotParams;
   }
}

const ParamDesc *fullTable() { return table().entries.data(); }

const std::vector<uint32_t> &hostParamIds() { return table().hostIds; }

bool isRealParam(uint32_t id) { return id < kTableSize && table().entries[id].key[0] != 0; }

} // namespace verdaliscene
