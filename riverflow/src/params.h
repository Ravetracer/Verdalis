#pragma once

#include <cstdint>

#include "verdalis/params.h"

namespace riverflow {

// The parameter model -- ParamDesc, ParamKind, the range mapping and the text
// formatting -- is shared by the whole suite. Pulling it in here rather than
// qualifying every use keeps the plugin's own code reading as it always did.
// The directive is scoped to this namespace, so nothing escapes into the
// global one.
using namespace verdalis;

// Parameter identifiers. These are persisted in preset files and plugin state,
// so the numeric values must never change: append new parameters at the end
// and never reorder or reuse an id.
enum ParamId : uint32_t {
   // Flow: the broadband bed. A river is mostly this, and the measured shape
   // of it is what tells a mountain river from a creek.
   kParamWaterType = 0,
   kParamWaterBlend,
   kParamFlowLevel,
   kParamFlowTilt,
   kParamFlowBody,
   kParamTurbulence,
   kParamSurgeRate,
   kParamFlowGrain,

   // Stones: the dabbling. Water folding over a stone entrains a pocket of
   // air, and the pocket rings.
   kParamDabbleRate,
   kParamDabbleLevel,
   kParamDabbleSize,
   kParamDabbleSpread,
   kParamDabbleCluster,
   kParamDabbleSpill,
   kParamDabbleDamping,
   kParamDabbleGlug,

   // Trickle: single drops falling onto stone or into a pool.
   kParamTrickleRate,
   kParamTrickleLevel,
   kParamTrickleSize,
   kParamTrickleSpread,
   kParamTrickleDecay,
   kParamTrickleImpact,
   kParamStoneTone,
   kParamSplash,

   // Plunge: the pool under a fall, and the cloud of bubbles held in it.
   kParamPlungeLevel,
   kParamPlungeTone,
   kParamPlungeDepth,
   kParamPlungeQ,

   // Reach: where the listener stands, and what the water runs between.
   kParamBankType,
   kParamDistance,
   kParamAir,
   kParamWidth,
   kParamFlowWidth,
   kParamSpaceAmount,
   kParamSpaceSize,
   kParamSpaceDamping,

   // Filter.
   kParamFilterType,
   kParamHighpass,
   kParamFilterCutoff,
   kParamFilterReso,
   kParamFilterKeyTrack,

   // Envelope.
   kParamAttack,
   kParamDecay,
   kParamSustain,
   kParamRelease,
   kParamVelToLevel,
   kParamVelToFlow,

   // Output.
   kParamGain,
   kParamMaxEvents,
   kParamSeed,

   kNumParams
};

// The six octave-band shapes the reference library actually falls into, in
// order of how much low end they carry. They are not names invented for the
// window: `tools/analysis/shapes.py` clusters all 77 recordings by their
// octave-band colour and these are the six clusters it finds, each shipped as
// the measured centroid of its members. Selecting one selects that centroid.
//
// A table of eight numbers is a formula, not a sample -- see the suite's note
// on pure synthesis. Nothing here reproduces recorded audio; the numbers say
// what colour to give a noise source the plugin generates itself.
enum WaterKind {
   kWaterDeepRush = 0,   // peak 500 Hz-1 kHz, steepest top: big discharge, close
   kWaterRapids,         // peak 1 kHz, broad and even
   kWaterMountainRiver,  // peak 1-2 kHz, nothing below 250 Hz, sharp top
   kWaterStream,         // the library's commonest shape: flat 1-4 kHz
   kWaterCreek,          // peak 4 kHz, no low end at all
   kWaterTrickle,        // rises all the way to 16 kHz: single drops on stone
   kNumWaterKinds
};

// What the water runs between, which decides how much of an early reflected
// field there is. The lesson is ShoreBreak's: eight discrete early reflections
// outdoors is what makes a reverb sound like a bathroom, and an open river has
// nothing close enough to reflect off. A gorge and a culvert do.
enum BankKind {
   kBankOpen = 0,
   kBankForest,
   kBankGorge,
   kBankRockPool,
   kBankCulvert,
   kBankCavern,
   kNumBankKinds
};

// The plugin's own table, and lookups into it.
const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

} // namespace riverflow
