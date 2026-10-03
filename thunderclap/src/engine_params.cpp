#include "engine_params.h"

#include "verdalis/dsp/fastmath.h"

namespace thunderclap {

EngineParams engineParams(const double *real) {
   EngineParams p;
   p.gain = dbToGain(static_cast<float>(real[kParamGain]));

   p.distanceKm = static_cast<float>(real[kParamDistance]);
   p.heightKm = static_cast<float>(real[kParamHeight]);
   p.cloudSpreadKm = static_cast<float>(real[kParamCloudSpread]);
   p.tortuosity = static_cast<float>(real[kParamTortuosity]);
   p.branching = static_cast<float>(real[kParamBranching]);
   p.strokes = static_cast<int>(real[kParamStrokes]);
   p.strokeGapSec = static_cast<float>(real[kParamStrokeGap]) * 0.001f;
   p.variation = static_cast<float>(real[kParamVariation]);

   p.crack = static_cast<float>(real[kParamCrack]);
   p.weight = static_cast<float>(real[kParamWeight]);
   p.swell = static_cast<float>(real[kParamSwell]);
   p.rumble = static_cast<float>(real[kParamRumble]);
   p.rumbleTone = static_cast<float>(real[kParamRumbleTone]);
   p.air = static_cast<float>(real[kParamAir]);
   p.scatter = static_cast<float>(real[kParamScatter]);
   p.focus = static_cast<float>(real[kParamFocus]);
   p.impact = static_cast<float>(real[kParamImpact]);
   p.bloom = static_cast<float>(real[kParamBloom]);
   p.ground = static_cast<float>(real[kParamGround]);

   p.width = static_cast<float>(real[kParamWidth]);
   p.pan = static_cast<float>(real[kParamPan]);
   p.rumbleWidth = static_cast<float>(real[kParamRumbleWidth]);
   p.drift = static_cast<float>(real[kParamDrift]);

   p.echoGain = dbToGain(static_cast<float>(real[kParamEchoLevel]));
   p.echoCount = static_cast<int>(real[kParamEchoCount]);
   p.echoSpreadSec = static_cast<float>(real[kParamEchoSpread]);
   p.echoDamping = static_cast<float>(real[kParamEchoDamping]);

   p.spaceAmount = static_cast<float>(real[kParamSpaceAmount]);
   p.spaceSize = static_cast<float>(real[kParamSpaceSize]);
   p.spaceDamping = static_cast<float>(real[kParamSpaceDamping]);

   p.filterType = static_cast<int>(real[kParamFilterType]);
   p.highpassHz = static_cast<float>(real[kParamHighpass]);
   p.filterCutoffHz = static_cast<float>(real[kParamFilterCutoff]);
   p.filterReso = static_cast<float>(real[kParamFilterReso]);
   p.filterKeyTrack = static_cast<float>(real[kParamFilterKeyTrack]);

   p.mode = static_cast<int>(real[kParamMode]);
   p.attackSec = static_cast<float>(real[kParamAttack]) * 0.001f;
   p.releaseSec = static_cast<float>(real[kParamRelease]) * 0.001f;
   p.stormRatePerMin = static_cast<float>(real[kParamStormRate]);
   p.velToLevel = static_cast<float>(real[kParamVelToLevel]);
   p.velToDistance = static_cast<float>(real[kParamVelToDistance]);

   p.maxShocks = static_cast<int>(real[kParamMaxShocks]);
   p.seed = static_cast<int>(real[kParamSeed]);

   p.compress = static_cast<float>(real[kParamCompress]);
   p.compAttackSec = static_cast<float>(real[kParamCompAttack]) * 0.001f;
   p.compReleaseSec = static_cast<float>(real[kParamCompRelease]) * 0.001f;
   return p;
}

} // namespace thunderclap
