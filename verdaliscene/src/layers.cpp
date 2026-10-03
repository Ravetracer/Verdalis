#include "layers.h"

namespace verdaliscene {

// One per adapter in src/layers/.
const LayerType &rainydayLayer();
const LayerType &thunderclapLayer();
const LayerType &shorebreakLayer();
const LayerType &skyhowlLayer();
const LayerType &chirpparadeLayer();
const LayerType &riverflowLayer();
const LayerType &crackleblazeLayer();
const LayerType &insectswarmLayer();
const LayerType &nightlifeLayer();

const LayerType &layerType(int type) {
   switch (type) {
   case kLayerThunder:
      return thunderclapLayer();
   case kLayerWaves:
      return shorebreakLayer();
   case kLayerWind:
      return skyhowlLayer();
   case kLayerBirds:
      return chirpparadeLayer();
   case kLayerRiver:
      return riverflowLayer();
   case kLayerFire:
      return crackleblazeLayer();
   case kLayerInsects:
      return insectswarmLayer();
   case kLayerNight:
      return nightlifeLayer();
   case kLayerRain:
   default:
      return rainydayLayer();
   }
}

const PinnedParam *pinnedParam(const LayerType &t, uint32_t id) {
   for (int i = 0; i < t.pinnedCount; ++i)
      if (t.pinned[i].id == id)
         return &t.pinned[i];
   return nullptr;
}

} // namespace verdaliscene
