#include "engine_params.h"

#include "verdalis/dsp/fastmath.h"

namespace riverflow {

EngineParams engineParams(const double *real) {
   EngineParams p;
   p.gain = dbToGain(static_cast<float>(real[kParamGain]));

   // Flow: the bed. Time parameters read in milliseconds, as elsewhere in
   // the suite, and levels in dB.
   p.waterType = static_cast<int>(real[kParamWaterType]);
   p.waterBlend = static_cast<float>(real[kParamWaterBlend]);
   p.flowGain = dbToGain(static_cast<float>(real[kParamFlowLevel]));
   p.flowTilt = static_cast<float>(real[kParamFlowTilt]);
   p.flowBody = static_cast<float>(real[kParamFlowBody]);
   p.turbulence = static_cast<float>(real[kParamTurbulence]);
   p.surgeRateHz = static_cast<float>(real[kParamSurgeRate]);
   p.flowGrain = static_cast<float>(real[kParamFlowGrain]);

   // Stones.
   p.dabbleRateHz = static_cast<float>(real[kParamDabbleRate]);
   p.dabbleGain = dbToGain(static_cast<float>(real[kParamDabbleLevel]));
   p.dabbleSizeMm = static_cast<float>(real[kParamDabbleSize]);
   p.dabbleSpreadOct = static_cast<float>(real[kParamDabbleSpread]);
   p.dabbleCluster = static_cast<int>(real[kParamDabbleCluster]);
   p.dabbleSpillSec = static_cast<float>(real[kParamDabbleSpill]) * 0.001f;
   p.dabbleDamping = static_cast<float>(real[kParamDabbleDamping]);
   p.dabbleGlug = static_cast<float>(real[kParamDabbleGlug]);

   // Trickle.
   p.trickleRateHz = static_cast<float>(real[kParamTrickleRate]);
   p.trickleGain = dbToGain(static_cast<float>(real[kParamTrickleLevel]));
   p.trickleSizeMm = static_cast<float>(real[kParamTrickleSize]);
   p.trickleSpreadOct = static_cast<float>(real[kParamTrickleSpread]);
   p.trickleDecaySec = static_cast<float>(real[kParamTrickleDecay]) * 0.001f;
   p.trickleImpact = static_cast<float>(real[kParamTrickleImpact]);
   p.stoneToneHz = static_cast<float>(real[kParamStoneTone]);
   p.splash = static_cast<float>(real[kParamSplash]);

   // Plunge.
   p.plungeGain = dbToGain(static_cast<float>(real[kParamPlungeLevel]));
   p.plungeToneHz = static_cast<float>(real[kParamPlungeTone]);
   p.plungeDepth = static_cast<float>(real[kParamPlungeDepth]);
   p.plungeQ = static_cast<float>(real[kParamPlungeQ]);

   // Reach.
   p.bank = static_cast<int>(real[kParamBankType]);
   p.distance = static_cast<float>(real[kParamDistance]);
   p.air = static_cast<float>(real[kParamAir]);
   p.width = static_cast<float>(real[kParamWidth]);
   p.flowWidth = static_cast<float>(real[kParamFlowWidth]);
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
   p.velToFlow = static_cast<float>(real[kParamVelToFlow]);

   p.maxEvents = static_cast<int>(real[kParamMaxEvents]);
   p.seed = static_cast<int>(real[kParamSeed]);
   return p;
}

} // namespace riverflow
