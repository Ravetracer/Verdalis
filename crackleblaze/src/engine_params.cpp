#include "engine_params.h"

#include "verdalis/dsp/fastmath.h"

namespace crackleblaze {

EngineParams engineParams(const double *real) {
   EngineParams p;
   p.gain = dbToGain(static_cast<float>(real[kParamGain]));

   // blaze
   p.fireType = static_cast<int>(real[kParamFireType]);
   p.fireBlend = static_cast<float>(real[kParamFireBlend]);
   p.roarGain = dbToGain(static_cast<float>(real[kParamRoarLevel]));
   p.roarTilt = static_cast<float>(real[kParamRoarTilt]);
   p.roarBody = static_cast<float>(real[kParamRoarBody]);
   p.flare = static_cast<float>(real[kParamFlare]);
   p.flareRateHz = static_cast<float>(real[kParamFlareRate]);
   p.draught = static_cast<float>(real[kParamDraught]);

   // crackle
   p.crackleRateHz = static_cast<float>(real[kParamCrackleRate]);
   p.crackleGain = dbToGain(static_cast<float>(real[kParamCrackleLevel]));
   p.crackleDecaySec = static_cast<float>(real[kParamCrackleDecay]) * 0.001f;
   p.crackleToneHz = static_cast<float>(real[kParamCrackleTone]);
   p.crackleSpreadDb = static_cast<float>(real[kParamCrackleSpread]);
   p.burst = static_cast<float>(real[kParamBurst]);
   p.snap = static_cast<float>(real[kParamSnap]);
   p.crackleBody = static_cast<float>(real[kParamCrackleBody]);

   // sizzle
   p.sap = static_cast<float>(real[kParamSap]);
   p.sizzleGain = dbToGain(static_cast<float>(real[kParamSizzleLevel]));
   p.sizzleDecaySec = static_cast<float>(real[kParamSizzleDecay]) * 0.001f;
   p.sizzleToneHz = static_cast<float>(real[kParamSizzleTone]);
   p.steam = static_cast<float>(real[kParamSteam]);

   // settle
   p.settleRateHz = static_cast<float>(real[kParamSettleRate]);
   p.settleGain = dbToGain(static_cast<float>(real[kParamSettleLevel]));
   p.settleToneHz = static_cast<float>(real[kParamSettleTone]);
   p.settleDecaySec = static_cast<float>(real[kParamSettleDecay]) * 0.001f;

   // hearth
   p.hearth = static_cast<int>(real[kParamHearthType]);
   p.distance = static_cast<float>(real[kParamDistance]);
   p.air = static_cast<float>(real[kParamAir]);
   p.width = static_cast<float>(real[kParamWidth]);
   p.roarWidth = static_cast<float>(real[kParamRoarWidth]);
   p.spaceAmount = static_cast<float>(real[kParamSpaceAmount]);
   p.spaceSize = static_cast<float>(real[kParamSpaceSize]);
   p.spaceDamping = static_cast<float>(real[kParamSpaceDamping]);

   // filter
   p.filterType = static_cast<int>(real[kParamFilterType]);
   p.highpassHz = static_cast<float>(real[kParamHighpass]);
   p.filterCutoffHz = static_cast<float>(real[kParamFilterCutoff]);
   p.filterReso = static_cast<float>(real[kParamFilterReso]);
   p.filterKeyTrack = static_cast<float>(real[kParamFilterKeyTrack]);

   // envelope
   p.attackSec = static_cast<float>(real[kParamAttack]) * 0.001f;
   p.decaySec = static_cast<float>(real[kParamDecay]) * 0.001f;
   p.sustain = static_cast<float>(real[kParamSustain]);
   p.releaseSec = static_cast<float>(real[kParamRelease]) * 0.001f;
   p.velToLevel = static_cast<float>(real[kParamVelToLevel]);
   p.velToFire = static_cast<float>(real[kParamVelToFire]);

   p.maxEvents = static_cast<int>(real[kParamMaxEvents]);
   p.seed = static_cast<int>(real[kParamSeed]);
   return p;
}

} // namespace crackleblaze
