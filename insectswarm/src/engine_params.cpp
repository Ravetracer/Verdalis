#include "engine_params.h"

#include "verdalis/dsp/fastmath.h"

namespace insectswarm {

EngineParams engineParams(const double *real) {
   EngineParams p;
   p.gain = dbToGain(static_cast<float>(real[kParamGain]));

   // swarm
   p.species = static_cast<int>(real[kParamSpecies]);
   p.rateShiftSemis = static_cast<float>(real[kParamRateShift]);
   p.count = static_cast<int>(real[kParamCount]);
   p.swarmGain = dbToGain(static_cast<float>(real[kParamSwarmLevel]));
   p.spreadCents = static_cast<float>(real[kParamSpread]);
   p.wanderCents = static_cast<float>(real[kParamWander]);
   p.wanderRateHz = static_cast<float>(real[kParamWanderRate]);
   p.roamDb = static_cast<float>(real[kParamRoam]);
   p.roamRateHz = static_cast<float>(real[kParamRoamRate]);
   p.rasp = static_cast<float>(real[kParamRasp]);

   // wing
   p.tilt = static_cast<float>(real[kParamTilt]);
   p.formantSemis = static_cast<float>(real[kParamFormant]);
   p.resonance = static_cast<float>(real[kParamResonance]);
   p.stroke = static_cast<float>(real[kParamStroke]);
   p.bite = static_cast<float>(real[kParamBite]);
   p.flutter = static_cast<float>(real[kParamFlutter]);

   // flyby
   p.flybyRateHz = static_cast<float>(real[kParamFlybyRate]);
   p.flybyGain = dbToGain(static_cast<float>(real[kParamFlybyLevel]));
   p.flybyRiseDb = static_cast<float>(real[kParamFlybyRise]);
   p.flybyPassSec = static_cast<float>(real[kParamFlybyPass]);
   p.flybySpeed = static_cast<float>(real[kParamFlybySpeed]);
   p.flybySweep = static_cast<float>(real[kParamFlybySweep]);

   // stridulate
   p.stridGain = dbToGain(static_cast<float>(real[kParamStridLevel]));
   p.carrierHz = static_cast<float>(real[kParamCarrier]);
   p.carrierQ = static_cast<float>(real[kParamCarrierQ]);
   p.pulseRateHz = static_cast<float>(real[kParamPulseRate]);
   p.echemeRateHz = static_cast<float>(real[kParamEchemeRate]);
   p.duty = static_cast<float>(real[kParamDuty]);
   p.scrape = static_cast<float>(real[kParamScrape]);
   p.chorus = static_cast<int>(real[kParamChorus]);
   p.stridSpread = static_cast<float>(real[kParamStridSpread]);

   // air
   p.distance = static_cast<float>(real[kParamDistance]);
   p.air = static_cast<float>(real[kParamAir]);
   p.width = static_cast<float>(real[kParamWidth]);
   p.spaceAmount = static_cast<float>(real[kParamSpaceAmount]);
   p.spaceSize = static_cast<float>(real[kParamSpaceSize]);
   p.spaceDamping = static_cast<float>(real[kParamSpaceDamping]);

   // bed
   p.bedGain = dbToGain(static_cast<float>(real[kParamBedLevel]));
   p.bedToneHz = static_cast<float>(real[kParamBedTone]);
   p.bedTilt = static_cast<float>(real[kParamBedTilt]);

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
   p.velToSwarm = static_cast<float>(real[kParamVelToSwarm]);

   p.maxIndividuals = static_cast<int>(real[kParamMaxVoices]);
   p.seed = static_cast<int>(real[kParamSeed]);
   return p;
}

} // namespace insectswarm
