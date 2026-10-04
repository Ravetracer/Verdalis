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
//    9280 + channel * 128 + f      effect parameter f (FxParamId) of channel
//                                  `channel`: a layer slot, or kMasterFx for
//                                  the scene's own chain
//
// and slot = type * kInstancesPerType + instance. A layer's parameter keeps
// the plugin's own id in its low byte, so a plugin that appends a parameter
// gains it here at a fresh id without moving any other. Nothing in this
// layout may change once released: host automation and saved projects refer
// to these numbers.
constexpr uint32_t kSlotIdBase = 64;
constexpr uint32_t kSlotStride = 256;
constexpr uint32_t kSlotMixBase = 200;
// The effects came after the layers had been released, so they have a block
// of their own past the layers' rather than a share of each slot's.
constexpr uint32_t kFxIdBase = kSlotIdBase + kNumSlots * kSlotStride;
constexpr uint32_t kFxStride = 128;
constexpr int kMasterFx = kNumSlots;          // the scene's chain
constexpr int kNumFxChannels = kNumSlots + 1; // every layer's, and the scene's
constexpr uint32_t kTableSize = kFxIdBase + kNumFxChannels * kFxStride;

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
   kParamAttackCurve,
   kParamDecayCurve,
   kParamReleaseCurve,
   kParamFxTails,
   kNumSceneParams
};
static_assert(kNumSceneParams <= kSlotIdBase, "the scene's parameters overflow into slot 0");

enum GateMode { kGateAlways = 0, kGateTransport, kGateNotes, kNumGateModes };

// Where the scene's envelope sits against the effects. Ring Out envelopes what
// goes into every chain, so each tail rings on by itself after the scene has
// faded; Release envelopes what comes out of them, so the scene's release fades
// the tails with everything else.
enum TailsMode { kTailsRingOut = 0, kTailsRelease, kNumTailsModes };

// A layer's place in the scene. Append only.
enum SlotParamId : uint32_t {
   kSlotActive = 0,
   kSlotLevel,
   kSlotPan,
   kSlotStereo,
   kSlotShotRate,
   kSlotAttackCurve,
   kSlotDecayCurve,
   kSlotReleaseCurve,
   kNumSlotParams
};

enum StereoMode { kStereoOn = 0, kStereoMono };

// One channel's effects, in the order the chain runs them. Append only, and
// only at the end of the list: the ids are released.
enum FxParamId : uint32_t {
   kFxReverbOn = 0,
   kFxReverbMix,
   kFxReverbPredelay,
   kFxReverbSize,
   kFxReverbDecay,
   kFxReverbDiffusion,
   kFxReverbDamping,
   kFxReverbDampFreq,
   kFxReverbModDepth,
   kFxReverbModRate,
   kFxReverbLowCut,
   kFxReverbHighCut,
   kFxReverbWidth,
   kFxReverbFreeze,

   kFxDelayOn,
   kFxDelayMode,
   kFxDelaySync,
   kFxDelayTime,
   kFxDelayNote,
   kFxDelayOffset,
   kFxDelayFeedback,
   kFxDelayLowCut,
   kFxDelayHighCut,
   kFxDelaySaturation,
   kFxDelayWow,
   kFxDelayWowRate,
   kFxDelayDiffusion,
   kFxDelayDucking,
   kFxDelayGlide,
   kFxDelayWidth,
   kFxDelayMix,

   kFxChorusOn,
   kFxChorusRate,
   kFxChorusDepth,
   kFxChorusDelay,
   kFxChorusVoices,
   kFxChorusWidth,
   kFxChorusMix,

   kFxFlangerOn,
   kFxFlangerRate,
   kFxFlangerDepth,
   kFxFlangerManual,
   kFxFlangerFeedback,
   kFxFlangerWidth,
   kFxFlangerMix,

   kFxPhaserOn,
   kFxPhaserRate,
   kFxPhaserDepth,
   kFxPhaserCentre,
   kFxPhaserFeedback,
   kFxPhaserStages,
   kFxPhaserWidth,
   kFxPhaserMix,

   kFxWidenerOn,
   kFxWidenerWidth,
   kFxWidenerDecorrelation,
   kFxWidenerBassMono,

   kFxAutoPanOn,
   kFxAutoPanRate,
   kFxAutoPanDepth,
   kFxAutoPanShape,

   kNumFxParams
};
static_assert(kNumFxParams <= kFxStride, "a channel's effects overflow into the next channel's");

// The effects, as a list: what the window draws a panel for and the chain runs.
enum FxKind {
   kFxReverb = 0,
   kFxDelay,
   kFxChorus,
   kFxFlanger,
   kFxPhaser,
   kFxWidener,
   kFxAutoPan,
   kNumFxKinds
};

struct FxKindInfo {
   const char *name;   // "Reverb"
   const char *letter; // what a mixer strip shows for it: "R"
   uint32_t first;     // its On parameter; the rest follow it
   uint32_t count;
};
const FxKindInfo &fxKind(int kind);
// The effect a parameter belongs to.
int fxKindOf(uint32_t fxParam);

// How long a synced delay's note value is, in quarter notes.
double fxNoteBeats(int note);
int fxNumNotes();

constexpr int slotIndex(int type, int instance) { return type * kInstancesPerType + instance; }
constexpr int slotType(int slot) { return slot / kInstancesPerType; }
constexpr int slotInstance(int slot) { return slot % kInstancesPerType; }
constexpr uint32_t slotBase(int slot) {
   return kSlotIdBase + static_cast<uint32_t>(slot) * kSlotStride;
}
constexpr uint32_t slotParamId(int slot, uint32_t pluginId) { return slotBase(slot) + pluginId; }
constexpr uint32_t slotMixId(int slot, uint32_t p) { return slotBase(slot) + kSlotMixBase + p; }
constexpr int slotOf(uint32_t id) {
   return id < kSlotIdBase || id >= kFxIdBase ? -1
                                              : static_cast<int>((id - kSlotIdBase) / kSlotStride);
}
constexpr uint32_t fxParamId(int channel, uint32_t p) {
   return kFxIdBase + static_cast<uint32_t>(channel) * kFxStride + p;
}
constexpr int fxChannelOf(uint32_t id) {
   return id < kFxIdBase || id >= kTableSize ? -1 : static_cast<int>((id - kFxIdBase) / kFxStride);
}
constexpr uint32_t fxLocalOf(uint32_t id) { return (id - kFxIdBase) % kFxStride; }
constexpr uint32_t localOf(uint32_t id) { return (id - kSlotIdBase) % kSlotStride; }

// Whether a layer of this kind has placement parameter p at all: Shot Rate
// only where the plugin has a shot to fire, Decay Curve only where its
// envelope has a decay. One answer for the host's table, the scene format and
// the window.
bool slotParamApplies(const LayerType &t, uint32_t p);

// The scene's own parameters, ids 0 .. kNumSceneParams - 1.
const ParamDesc *sceneParamTable();

// A layer's placement parameters, indexed by SlotParamId. Their ids are the
// SlotParamId; slotMixId() places them.
const ParamDesc *slotParamTable();

// One channel's effect parameters, indexed by FxParamId; fxParamId() places
// them. Their keys are the ones a scene file uses, all starting "fx_", which
// no plugin's own keys do.
const ParamDesc *fxParamTable();
const ParamDesc *fxParamByKey(const char *key);

// What the manual documents: the scene's parameters followed by a layer's
// placement ones, as they are keyed in a scene preset. A layer's own
// parameters are the plugin's and are documented in that plugin's manual.
// Named the way every plugin's are, because the manual's generator expects
// that name.
constexpr uint32_t kNumParams = kNumSceneParams + kNumSlotParams - 1 + kNumFxParams;
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
