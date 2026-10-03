#include "engine_params.h"

#include "verdalis/dsp/fastmath.h"

namespace skyhowl {

EngineParams engineParams(const double *real) {
   EngineParams p;
   p.gain = dbToGain(static_cast<float>(real[kParamGain]));

   // Wind. The flow field: nothing here makes a sound, everything else is
   // driven by it.
   p.windSpeedMs = static_cast<float>(real[kParamWindSpeed]);
   // Turbulence reads as a percentage because that is how meteorology
   // reports it; the engine wants the ratio it actually is.
   p.turbulence = static_cast<float>(real[kParamTurbulence]) * 0.01f;
   p.gustRatePerMin = static_cast<float>(real[kParamGustRate]);
   p.gustDepth = static_cast<float>(real[kParamGustDepth]);
   p.gustLengthSec = static_cast<float>(real[kParamGustLength]) * 0.001f;
   p.gustShape = static_cast<float>(real[kParamGustShape]);
   p.squall = static_cast<float>(real[kParamSquall]);
   p.squallRatePerMin = static_cast<float>(real[kParamSquallRate]);

   // Airflow.
   p.flowGain = dbToGain(static_cast<float>(real[kParamFlowLevel]));
   p.flowToneHz = static_cast<float>(real[kParamFlowTone]);
   p.flowTiltDbOct = static_cast<float>(real[kParamFlowTilt]);
   p.buffetGain = dbToGain(static_cast<float>(real[kParamBuffet]));
   p.buffetToneHz = static_cast<float>(real[kParamBuffetTone]);
   p.hiss = static_cast<float>(real[kParamHiss]);
   p.speedLaw = static_cast<float>(real[kParamSpeedLaw]);

   // Howl.
   p.howlAmount = static_cast<float>(real[kParamHowlAmount]);
   p.obstacle = static_cast<int>(real[kParamObstacle]);
   p.howlSizeMm = static_cast<float>(real[kParamHowlSize]);
   p.howlSpreadOct = static_cast<float>(real[kParamHowlSpread]);
   p.howlVoices = static_cast<int>(real[kParamHowlVoices]);
   p.howlReso = static_cast<float>(real[kParamHowlReso]);
   p.howlTrack = static_cast<float>(real[kParamHowlTrack]);
   p.howlThreshold = static_cast<float>(real[kParamHowlThreshold]);
   p.warble = static_cast<float>(real[kParamWarble]);

   // Rustle.
   p.rustleAmount = static_cast<float>(real[kParamRustleAmount]);
   p.foliage = static_cast<int>(real[kParamFoliage]);
   p.rustleSizeMm = static_cast<float>(real[kParamRustleSize]);
   p.rustleDensityHz = static_cast<float>(real[kParamRustleDensity]);
   p.rustleSpreadOct = static_cast<float>(real[kParamRustleSpread]);
   p.rustleDecaySec = static_cast<float>(real[kParamRustleDecay]) * 0.001f;
   p.rustleThreshold = static_cast<float>(real[kParamRustleThreshold]);
   p.clatter = static_cast<float>(real[kParamClatter]);

   // Place.
   p.terrain = static_cast<int>(real[kParamTerrain]);
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
   p.velToSpeed = static_cast<float>(real[kParamVelToSpeed]);

   p.maxGusts = static_cast<int>(real[kParamMaxGusts]);
   p.seed = static_cast<int>(real[kParamSeed]);
   return p;
}

} // namespace skyhowl
