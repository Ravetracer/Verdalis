#pragma once

#include <cstdint>
#include <vector>

#include "verdalis/params.h"

#include "layers.h"

namespace verdaliscene {

// The parameter model -- ParamDesc, ParamKind, the range mapping and the text
// formatting -- is shared by the whole suite.
using namespace verdalis;

// ------------------------------------------------------------- id layout
//
// Every parameter id is also its index in the table the host and the window
// see, and that table has holes in it on purpose:
//
//    0 .. 63                       the scene's own parameters (ParamId)
//    64 + slot * 256 + n           layer `slot`'s parameter n, where
//         n < 200                  is the plugin's own ParamId n, and
//         n = 200 + SlotParamId    is the layer's place in the scene
//
// and slot = type * kInstancesPerType + instance. A layer's parameter keeps
// the plugin's own id in its low byte, so a plugin that appends a parameter
// gains it here at a fresh id without moving any other. Nothing in this
// layout may change once released: host automation and saved projects refer
// to these numbers.
constexpr uint32_t kSlotIdBase = 64;
constexpr uint32_t kSlotStride = 256;
constexpr uint32_t kSlotMixBase = 200;
constexpr uint32_t kTableSize = kSlotIdBase + kNumSlots * kSlotStride;

// The scene's own parameters. Append only.
enum ParamId : uint32_t {
   kParamGate = 0,
   kParamAttack,
   kParamDecay,
   kParamSustain,
   kParamRelease,
   kParamFilterType,
   kParamHighpass,
   kParamFilterCutoff,
   kParamFilterReso,
   kParamWidth,
   kParamGain,
   kNumSceneParams
};
static_assert(kNumSceneParams <= kSlotIdBase, "the scene's parameters overflow into slot 0");

enum GateMode { kGateAlways = 0, kGateTransport, kGateNotes, kNumGateModes };

// A layer's place in the scene. Append only.
enum SlotParamId : uint32_t {
   kSlotActive = 0,
   kSlotLevel,
   kSlotPan,
   kSlotStereo,
   kSlotShotRate,
   kNumSlotParams
};

enum StereoMode { kStereoOn = 0, kStereoMono };

constexpr int slotIndex(int type, int instance) { return type * kInstancesPerType + instance; }
constexpr int slotType(int slot) { return slot / kInstancesPerType; }
constexpr int slotInstance(int slot) { return slot % kInstancesPerType; }
constexpr uint32_t slotBase(int slot) {
   return kSlotIdBase + static_cast<uint32_t>(slot) * kSlotStride;
}
constexpr uint32_t slotParamId(int slot, uint32_t pluginId) { return slotBase(slot) + pluginId; }
constexpr uint32_t slotMixId(int slot, uint32_t p) { return slotBase(slot) + kSlotMixBase + p; }
constexpr int slotOf(uint32_t id) {
   return id < kSlotIdBase || id >= kTableSize ? -1
                                               : static_cast<int>((id - kSlotIdBase) / kSlotStride);
}
constexpr uint32_t localOf(uint32_t id) { return (id - kSlotIdBase) % kSlotStride; }

// The scene's own parameters, ids 0 .. kNumSceneParams - 1.
const ParamDesc *sceneParamTable();

// A layer's placement parameters, indexed by SlotParamId. Their ids are the
// SlotParamId; slotMixId() places them.
const ParamDesc *slotParamTable();

// What the manual documents: the scene's parameters followed by a layer's
// placement ones, as they are keyed in a scene preset. A layer's own
// parameters are the plugin's and are documented in that plugin's manual.
// Named the way every plugin's are, because the manual's generator expects
// that name.
constexpr uint32_t kNumParams = kNumSceneParams + kNumSlotParams - 1;
const ParamDesc *paramTable();

// The scene's own parameters by key, for the scene preset format.
const ParamDesc *sceneParamByKey(const char *key);
const ParamDesc *slotParamByKey(const char *key);

// ------------------------------------------------------------ the whole table
//
// Built on first use from every layer type's own table (table.cpp), so it is
// only reachable from code that links the layers.

// kTableSize entries, entry i having id i. A hole has an empty key.
const ParamDesc *fullTable();

// The ids the host is shown, in order: every scene parameter, then every
// layer's, leaving out the holes and the parameters a layer pins.
const std::vector<uint32_t> &hostParamIds();

bool isRealParam(uint32_t id);

} // namespace verdaliscene
