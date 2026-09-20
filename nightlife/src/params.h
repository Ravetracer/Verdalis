#pragma once

#include <cstdint>

#include "verdalis/params.h"

namespace nightlife {

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
   // Call: one call of one animal. The unit the caller layer is built from.
   kParamPitch = 0,
   kParamContour,
   kParamDetail,
   kParamSweep,
   kParamLength,
   kParamSkew,
   kParamJitter,
   kParamVibrato,
   kParamVibratoRate,

   // Voice: the larynx and the tube above it.
   kParamCaller,
   kParamVoice,
   kParamBreath,
   kParamThroat,
   kParamMuzzle,
   kParamFormant,
   kParamRasp,
   kParamRadiate,
   kParamPartials,

   // Phrase: calls into a sequence.
   kParamCalls,
   kParamCallRate,
   kParamRateDrift,
   kParamLegato,
   kParamMotif,
   kParamVariation,
   kParamPhraseGap,
   kParamRepeats,

   // Pack: how many animals there are and what they do unprompted.
   kParamShotLevel,
   kParamPackLevel,
   kParamPackRate,
   kParamAnimals,
   kParamPitchSpread,
   kParamVoiceSpread,
   kParamAnswer,
   kParamRestless,
   kParamMaxVoices,

   // Chorus: the frogs. A croak is a pulse train, not a pitch contour.
   kParamChorusLevel,
   kParamCroak,
   kParamCroakPitch,
   kParamPulseRate,
   kParamPulses,
   kParamCroakLength,
   kParamFrogs,
   kParamCroakRate,
   kParamRegularity,
   kParamChorusSpread,
   kParamChorusWidth,

   // Insects: a narrow band of noise with a trill on it.
   kParamInsectLevel,
   kParamInsectPitch,
   kParamInsectWidth,
   kParamTrillRate,
   kParamTrillDepth,
   kParamShimmer,

   // Bed: the night itself, measured off the quiet parts of the library.
   kParamBedLevel,
   kParamBedTilt,
   kParamBedMotion,

   // Place: where the animals are, and what the air and the trees do to them.
   kParamDistance,
   kParamDistanceSpread,
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
   kParamVelToPitch,

   // Output.
   kParamGain,
   kParamSeed,

   kNumParams
};

// Which animal. Every entry is a measurement rather than an impression: the
// reference library is named by what is in it, so it can be grouped by caller
// and each group measured on its own. tools/analysis/callers.py prints the
// table and the grouping it used, and the archetype contours each group's
// recordings produced are in src/dsp/contours_generated.h.
//
// A caller does not replace the CALL and VOICE controls -- it biases them, and
// it chooses which set of measured contours the Contour knob walks across.
// Pitch, length, harmonic richness, roughness and call rate all move together,
// because in the references they do.
enum CallerKind {
   kCallerWolf = 0, // the howl: 484 Hz, 1.3 s, a slow vibrato and a fall
   kCallerOwl,      // the hoot: 497 Hz, near-sinusoidal, in phrases
   kCallerScreech,  // the whinny: a fast descending trill, which no hoot is
   kCallerScops,    // the pip: one short pure tone, repeated for minutes
   kCallerFox,      // the scream: harsh, three harmonics, an octave up
   kCallerLoon,     // the wail: the one bird here, and the one everyone knows
   kNumCallers
};

// The plugin's own table, and lookups into it.
const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

} // namespace nightlife
