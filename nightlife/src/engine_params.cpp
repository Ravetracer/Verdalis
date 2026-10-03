#include "engine_params.h"

#include "verdalis/dsp/fastmath.h"

namespace nightlife {

EngineParams engineParams(const double *real) {
   EngineParams p;
   p.gain = dbToGain(static_cast<float>(real[kParamGain]));

   // Call: one call of one animal, which is the unit the caller layer is
   // built out of.
   p.pitchHz = static_cast<float>(real[kParamPitch]);
   p.contour = static_cast<float>(real[kParamContour]);
   p.detail = static_cast<float>(real[kParamDetail]);
   p.sweep = static_cast<float>(real[kParamSweep]) * 0.01f;
   p.lengthSec = static_cast<float>(real[kParamLength]) * 0.001f;
   p.skew = static_cast<float>(real[kParamSkew]);
   p.jitter = static_cast<float>(real[kParamJitter]);
   p.vibrato = static_cast<float>(real[kParamVibrato]);
   p.vibratoHz = static_cast<float>(real[kParamVibratoRate]);

   // voice
   p.caller = static_cast<int>(real[kParamCaller]);
   p.voice = static_cast<float>(real[kParamVoice]);
   p.breath = static_cast<float>(real[kParamBreath]);
   p.throatCm = static_cast<float>(real[kParamThroat]);
   p.muzzle = static_cast<float>(real[kParamMuzzle]);
   p.formant = static_cast<float>(real[kParamFormant]);
   p.rasp = static_cast<float>(real[kParamRasp]);
   p.radiate = static_cast<float>(real[kParamRadiate]);
   p.partials = static_cast<float>(real[kParamPartials]);

   // phrase
   p.calls = static_cast<int>(real[kParamCalls]);
   p.callRate = static_cast<float>(real[kParamCallRate]);
   p.rateDrift = static_cast<float>(real[kParamRateDrift]);
   p.legato = static_cast<float>(real[kParamLegato]);
   p.motifSemis = static_cast<float>(real[kParamMotif]);
   p.variation = static_cast<float>(real[kParamVariation]);
   p.phraseGapSec = static_cast<float>(real[kParamPhraseGap]);
   p.repeats = static_cast<int>(real[kParamRepeats]);

   // pack
   p.shotGain = dbToGain(static_cast<float>(real[kParamShotLevel]));
   p.packGain = dbToGain(static_cast<float>(real[kParamPackLevel]));
   p.packRatePerMin = static_cast<float>(real[kParamPackRate]);
   p.animals = static_cast<int>(real[kParamAnimals]);
   p.pitchSpreadOct = static_cast<float>(real[kParamPitchSpread]);
   p.voiceSpread = static_cast<float>(real[kParamVoiceSpread]);
   p.answer = static_cast<float>(real[kParamAnswer]);
   p.restless = static_cast<float>(real[kParamRestless]);
   p.maxVoices = static_cast<int>(real[kParamMaxVoices]);

   // chorus
   p.chorusGain = dbToGain(static_cast<float>(real[kParamChorusLevel]));
   p.croak = static_cast<float>(real[kParamCroak]);
   p.croakHz = static_cast<float>(real[kParamCroakPitch]);
   p.pulseHz = static_cast<float>(real[kParamPulseRate]);
   p.pulses = static_cast<int>(real[kParamPulses]);
   p.croakSec = static_cast<float>(real[kParamCroakLength]) * 0.001f;
   p.frogs = static_cast<int>(real[kParamFrogs]);
   p.croakRatePerMin = static_cast<float>(real[kParamCroakRate]);
   p.regularity = static_cast<float>(real[kParamRegularity]);
   p.chorusSpreadOct = static_cast<float>(real[kParamChorusSpread]);
   p.chorusWidth = static_cast<float>(real[kParamChorusWidth]);

   // insects
   p.insectGain = dbToGain(static_cast<float>(real[kParamInsectLevel]));
   p.insectHz = static_cast<float>(real[kParamInsectPitch]);
   p.insectWidth = static_cast<float>(real[kParamInsectWidth]);
   p.trillHz = static_cast<float>(real[kParamTrillRate]);
   p.trillDepth = static_cast<float>(real[kParamTrillDepth]);
   p.shimmer = static_cast<float>(real[kParamShimmer]);

   // bed
   p.bedGain = dbToGain(static_cast<float>(real[kParamBedLevel]));
   p.bedTilt = static_cast<float>(real[kParamBedTilt]);
   p.bedMotion = static_cast<float>(real[kParamBedMotion]);

   // place
   p.distance = static_cast<float>(real[kParamDistance]);
   p.distanceSpread = static_cast<float>(real[kParamDistanceSpread]);
   p.air = static_cast<float>(real[kParamAir]);
   p.width = static_cast<float>(real[kParamWidth]);
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
   p.velToPitch = static_cast<float>(real[kParamVelToPitch]);

   p.seed = static_cast<int>(real[kParamSeed]);
   return p;
}

} // namespace nightlife
