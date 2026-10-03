#include "engine_params.h"

#include "verdalis/dsp/fastmath.h"

namespace chirpparade {

EngineParams engineParams(const double *real) {
   EngineParams p;
   p.gain = dbToGain(static_cast<float>(real[kParamGain]));

   // Syllable: one note of one bird, which is the unit everything else in
   // the engine is built out of.
   p.pitchHz = static_cast<float>(real[kParamPitch]);
   p.sweep = static_cast<float>(real[kParamSweep]) * 0.01f;
   p.contour = static_cast<float>(real[kParamContour]);
   p.detail = static_cast<float>(real[kParamDetail]);
   p.lengthSec = static_cast<float>(real[kParamLength]) * 0.001f;
   p.skew = static_cast<float>(real[kParamSkew]);
   p.jitter = static_cast<float>(real[kParamJitter]);
   p.pulseRateHz = static_cast<float>(real[kParamPulseRate]);
   p.pulseDepth = static_cast<float>(real[kParamPulseDepth]);

   // Timbre.
   p.species = static_cast<int>(real[kParamSpecies]);
   p.voice = static_cast<float>(real[kParamVoice]);
   p.breath = static_cast<float>(real[kParamBreath]);
   p.tractCm = static_cast<float>(real[kParamTract]);
   p.beak = static_cast<float>(real[kParamBeak]);
   p.formant = static_cast<float>(real[kParamFormant]);
   p.rasp = static_cast<float>(real[kParamRasp]);
   p.radiate = static_cast<float>(real[kParamRadiate]);
   p.partials = static_cast<float>(real[kParamPartials]);

   // Phrase.
   p.syllables = static_cast<int>(real[kParamSyllables]);
   p.syllableRate = static_cast<float>(real[kParamSyllableRate]);
   p.rateDrift = static_cast<float>(real[kParamRateDrift]);
   p.legato = static_cast<float>(real[kParamLegato]);
   p.motifSemis = static_cast<float>(real[kParamMotif]);
   p.variation = static_cast<float>(real[kParamVariation]);
   p.phraseGapSec = static_cast<float>(real[kParamPhraseGap]);
   p.repeats = static_cast<int>(real[kParamRepeats]);

   // Flock.
   p.shotGain = dbToGain(static_cast<float>(real[kParamShotLevel]));
   p.flockGain = dbToGain(static_cast<float>(real[kParamFlockLevel]));
   p.flockRatePerMin = static_cast<float>(real[kParamFlockRate]);
   p.birds = static_cast<int>(real[kParamBirds]);
   p.pitchSpreadOct = static_cast<float>(real[kParamPitchSpread]);
   p.voiceSpread = static_cast<float>(real[kParamVoiceSpread]);
   p.answer = static_cast<float>(real[kParamAnswer]);
   p.restless = static_cast<float>(real[kParamRestless]);
   p.maxVoices = static_cast<int>(real[kParamMaxVoices]);

   // Drum.
   p.drumGain = dbToGain(static_cast<float>(real[kParamDrumLevel]));
   p.drumRatePerMin = static_cast<float>(real[kParamDrumRate]);
   p.strikes = static_cast<int>(real[kParamStrikes]);
   p.strikeRate = static_cast<float>(real[kParamStrikeRate]);
   p.drumAccel = static_cast<float>(real[kParamDrumAccel]);
   p.knockHz = static_cast<float>(real[kParamKnock]);
   p.ringSec = static_cast<float>(real[kParamRing]) * 0.001f;

   // Place.
   p.distance = static_cast<float>(real[kParamDistance]);
   p.distanceSpread = static_cast<float>(real[kParamDistanceSpread]);
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
   p.velToPitch = static_cast<float>(real[kParamVelToPitch]);

   p.seed = static_cast<int>(real[kParamSeed]);
   return p;
}

} // namespace chirpparade
