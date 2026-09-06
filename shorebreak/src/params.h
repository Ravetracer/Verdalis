#pragma once

#include <cstdint>

#include "verdalis/params.h"

namespace shorebreak {

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
   // Surf: the breaking waves themselves.
   kParamWavePeriod = 0,
   kParamSetVariation,
   kParamWaveSize,
   kParamSizeVariation,
   kParamBreakAttack,
   kParamBreakDecay,
   kParamBreakTone,
   kParamBreakBody,
   kParamCrestSweep,
   kParamBreakerType,
   kParamPrecursor,
   kParamBubbleMix,

   // Foam: what a break leaves behind.
   kParamFoamLevel,
   kParamFoamDecay,
   kParamFoamTone,
   kParamFoamDelay,
   kParamFizz,
   kParamFoamBubbles,

   // Swell: the continuous bed the waves sit on.
   kParamSwellLevel,
   kParamSwellTone,
   kParamSwellDepth,
   kParamSwellRate,
   kParamSwellWidth,

   // Bubbles: individual resonators in the foam.
   kParamBubbleRate,
   kParamBubblePitch,
   kParamBubbleSpread,
   kParamBubbleDecay,

   // Wash: the water running back over sand and shingle.
   kParamWashLevel,
   kParamWashDecay,
   kParamWashTone,
   kParamSand,

   // Shore: where the listener stands.
   kParamShoreType,
   kParamDistance,
   kParamAir,
   kParamWidth,
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
   kParamVelToSize,

   // Output.
   kParamGain,
   kParamMaxWaves,
   kParamSeed,

   kNumParams
};

// How the wave breaks, after Galvin's (1968) classification. The type decides
// how violently the crest collapses, and with it the slope of the noise above
// 1.5 kHz: Means & Heitmeyer (2002) measured -10 dB/octave for plungers and
// -8.3 dB/octave for spillers.
enum BreakerKind {
   kBreakerSpilling = 0, // crest foams down the face; gradual, foam-heavy
   kBreakerPlunging,     // crest throws forward and slams; the loudest, steepest
   kBreakerCollapsing,   // lower face collapses; between plunging and surging
   kBreakerSurging,      // face surges up the shore without a proper break
   kNumBreakers
};

// What the shore is made of. A break's foam, its wash and its brightness all
// depend on what the water is running over.
enum ShoreKind {
   kShoreSand = 0,
   kShoreShingle,
   kShorePebbles,
   kShoreRock,
   kShoreReef,
   kShoreHarbour,
   kNumShores
};

// The plugin's own table, and lookups into it.
const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

} // namespace shorebreak
