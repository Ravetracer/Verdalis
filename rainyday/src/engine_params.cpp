#include "engine_params.h"

#include "verdalis/dsp/fastmath.h"

namespace rainyday {

EngineParams engineParams(const double *real) {
   EngineParams p;
   p.gain = dbToGain(static_cast<float>(real[kParamGain]));
   p.densityHz = static_cast<float>(real[kParamDensity]);
   p.clumping = static_cast<float>(real[kParamClumping]);
   p.dropPitchHz = static_cast<float>(real[kParamDropPitch]);
   p.pitchSpreadOct = static_cast<float>(real[kParamPitchSpread]);
   p.dropDecaySec = static_cast<float>(real[kParamDropDecay]) * 0.001f;
   p.decaySpread = static_cast<float>(real[kParamDecaySpread]);
   p.tonality = static_cast<float>(real[kParamTonality]);
   p.impact = static_cast<float>(real[kParamImpact]);
   p.splash = static_cast<float>(real[kParamSplash]);
   p.slosh = static_cast<float>(real[kParamSlosh]);
   p.levelSpread = static_cast<float>(real[kParamLevelSpread]);
   p.chirp = static_cast<float>(real[kParamChirp]);
   p.bubbleChance = static_cast<float>(real[kParamBubble]);
   p.surface = static_cast<int>(real[kParamSurface]);
   p.trickleGain = dbToGain(static_cast<float>(real[kParamTrickleLevel]));
   p.trickleRateHz = static_cast<float>(real[kParamTrickleRate]);
   p.trickleSizeMm = static_cast<float>(real[kParamTrickleSize]);
   p.trickleSpreadOct = static_cast<float>(real[kParamTrickleSpread]);
   p.trickleDecaySec = static_cast<float>(real[kParamTrickleDecay]) * 0.001f;
   p.trickleImpact = static_cast<float>(real[kParamTrickleImpact]);
   p.stoneToneHz = static_cast<float>(real[kParamStoneTone]);
   p.trickleSplash = static_cast<float>(real[kParamTrickleSplash]);
   p.noteTracking = static_cast<float>(real[kParamNoteTracking]);

   p.bedGain = dbToGain(static_cast<float>(real[kParamBedLevel]));
   p.bedTone = static_cast<float>(real[kParamBedTone]);
   p.bedBody = static_cast<float>(real[kParamBedBody]);
   p.bedDrift = static_cast<float>(real[kParamBedDrift]);
   p.bedWidth = static_cast<float>(real[kParamBedWidth]);
   p.bedPan = static_cast<float>(real[kParamBedPan]);

   p.width = static_cast<float>(real[kParamWidth]);
   p.dropPan = static_cast<float>(real[kParamDropPan]);
   p.distance = static_cast<float>(real[kParamDistance]);
   p.air = static_cast<float>(real[kParamAir]);
   p.spaceAmount = static_cast<float>(real[kParamSpaceAmount]);
   p.spaceSize = static_cast<float>(real[kParamSpaceSize]);
   p.spaceDamping = static_cast<float>(real[kParamSpaceDamping]);

   p.filterType = static_cast<int>(real[kParamFilterType]);
   p.highpassHz = static_cast<float>(real[kParamHighpass]);
   p.filterCutoffHz = static_cast<float>(real[kParamFilterCutoff]);
   p.filterReso = static_cast<float>(real[kParamFilterReso]);
   p.filterKeyTrack = static_cast<float>(real[kParamFilterKeyTrack]);

   p.attackSec = static_cast<float>(real[kParamAttack]) * 0.001f;
   p.decaySec = static_cast<float>(real[kParamDecay]) * 0.001f;
   p.sustain = static_cast<float>(real[kParamSustain]);
   p.releaseSec = static_cast<float>(real[kParamRelease]) * 0.001f;
   p.velToLevel = static_cast<float>(real[kParamVelToLevel]);
   p.velToDensity = static_cast<float>(real[kParamVelToDensity]);

   p.maxDroplets = static_cast<int>(real[kParamMaxDroplets]);
   p.seed = static_cast<int>(real[kParamSeed]);
   return p;
}

} // namespace rainyday
