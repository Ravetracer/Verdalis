#include "engine_params.h"

#include "verdalis/dsp/fastmath.h"

namespace shorebreak {

EngineParams engineParams(const double *real) {
   EngineParams p;
   p.gain = dbToGain(static_cast<float>(real[kParamGain]));

   // Surf. Time parameters read in milliseconds, as elsewhere in the suite,
   // so they are scaled to the seconds the engine works in.
   p.wavePeriodSec = static_cast<float>(real[kParamWavePeriod]) * 0.001f;
   p.setVariation = static_cast<float>(real[kParamSetVariation]);
   p.waveSize = static_cast<float>(real[kParamWaveSize]);
   p.sizeVariation = static_cast<float>(real[kParamSizeVariation]);
   p.breakAttackSec = static_cast<float>(real[kParamBreakAttack]) * 0.001f;
   p.breakDecaySec = static_cast<float>(real[kParamBreakDecay]) * 0.001f;
   p.breakToneHz = static_cast<float>(real[kParamBreakTone]);
   p.breakBody = static_cast<float>(real[kParamBreakBody]);
   p.crestSweep = static_cast<float>(real[kParamCrestSweep]);
   p.breakerType = static_cast<int>(real[kParamBreakerType]);
   p.precursor = static_cast<float>(real[kParamPrecursor]);
   p.bubbleMix = static_cast<float>(real[kParamBubbleMix]);

   // Foam.
   p.foamGain = dbToGain(static_cast<float>(real[kParamFoamLevel]));
   p.foamDecaySec = static_cast<float>(real[kParamFoamDecay]) * 0.001f;
   p.foamToneHz = static_cast<float>(real[kParamFoamTone]);
   p.foamDelaySec = static_cast<float>(real[kParamFoamDelay]) * 0.001f;
   p.fizz = static_cast<float>(real[kParamFizz]);
   p.foamBubbles = static_cast<float>(real[kParamFoamBubbles]);

   // Swell.
   p.swellGain = dbToGain(static_cast<float>(real[kParamSwellLevel]));
   p.swellToneHz = static_cast<float>(real[kParamSwellTone]);
   p.swellDepth = static_cast<float>(real[kParamSwellDepth]);
   p.swellRatePerMin = static_cast<float>(real[kParamSwellRate]);
   p.swellWidth = static_cast<float>(real[kParamSwellWidth]);

   // Bubbles.
   p.bubbleRateHz = static_cast<float>(real[kParamBubbleRate]);
   p.bubbleRadiusMm = static_cast<float>(real[kParamBubblePitch]);
   p.bubbleSpreadOct = static_cast<float>(real[kParamBubbleSpread]);
   p.bubbleDamping = static_cast<float>(real[kParamBubbleDecay]);

   // Wash.
   p.washGain = dbToGain(static_cast<float>(real[kParamWashLevel]));
   p.washDecaySec = static_cast<float>(real[kParamWashDecay]) * 0.001f;
   p.washToneHz = static_cast<float>(real[kParamWashTone]);
   p.sand = static_cast<float>(real[kParamSand]);

   // Shore.
   p.shore = static_cast<int>(real[kParamShoreType]);
   p.distance = static_cast<float>(real[kParamDistance]);
   p.air = static_cast<float>(real[kParamAir]);
   p.width = static_cast<float>(real[kParamWidth]);
   p.spaceAmount = static_cast<float>(real[kParamSpaceAmount]);
   p.spaceSize = static_cast<float>(real[kParamSpaceSize]);
   p.spaceDamping = static_cast<float>(real[kParamSpaceDamping]);

   // Filter.
   p.filterType = static_cast<int>(real[kParamFilterType]);
   p.highpassHz = static_cast<float>(real[kParamHighpass]);
   p.filterCutoffHz = static_cast<float>(real[kParamFilterCutoff]);
   p.filterReso = static_cast<float>(real[kParamFilterReso]);
   p.filterKeyTrack = static_cast<float>(real[kParamFilterKeyTrack]);

   // Envelope.
   p.attackSec = static_cast<float>(real[kParamAttack]) * 0.001f;
   p.decaySec = static_cast<float>(real[kParamDecay]) * 0.001f;
   p.sustain = static_cast<float>(real[kParamSustain]);
   p.releaseSec = static_cast<float>(real[kParamRelease]) * 0.001f;
   p.velToLevel = static_cast<float>(real[kParamVelToLevel]);
   p.velToSize = static_cast<float>(real[kParamVelToSize]);

   p.maxWaves = static_cast<int>(real[kParamMaxWaves]);
   p.seed = static_cast<int>(real[kParamSeed]);
   return p;
}

} // namespace shorebreak
