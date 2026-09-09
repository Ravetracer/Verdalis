#pragma once

#include <cstdint>

#include "verdalis/params.h"

namespace chirpparade {

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
   // Syllable: one note of one bird. The unit everything else is built from.
   kParamPitch = 0,
   kParamSweep,
   kParamContour,
   kParamDetail,
   kParamLength,
   kParamSkew,
   kParamJitter,
   kParamPulseRate,
   kParamPulseDepth,

   // Timbre: the syrinx and the tube above it.
   kParamSpecies,
   kParamVoice,
   kParamBreath,
   kParamTract,
   kParamBeak,
   kParamFormant,
   kParamRasp,
   kParamRadiate,

   // Phrase: syllables into a song.
   kParamSyllables,
   kParamSyllableRate,
   kParamRateDrift,
   kParamLegato,
   kParamMotif,
   kParamVariation,
   kParamPhraseGap,
   kParamRepeats,

   // Flock: how many birds there are and what they do unprompted.
   kParamShotLevel,
   kParamFlockLevel,
   kParamFlockRate,
   kParamBirds,
   kParamPitchSpread,
   kParamVoiceSpread,
   kParamAnswer,
   kParamRestless,
   kParamMaxVoices,

   // Drum: woodpecker drumming, which is not a voice at all.
   kParamDrumLevel,
   kParamDrumRate,
   kParamStrikes,
   kParamStrikeRate,
   kParamDrumAccel,
   kParamKnock,
   kParamRing,

   // Place: where the birds are and what the air and the trees do to them.
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

   // Appended after the fact, so its id is last -- the ids are persisted in
   // presets and state and are never reordered. It belongs to Timbre and the
   // window puts it there.
   kParamPartials,

   kNumParams
};

// What kind of bird. Every entry is a measurement rather than an impression:
// the reference library is named by what is in it, so it can be grouped by
// species and each group measured on its own. tools/analysis/species.py prints
// the table and the grouping it used; the biases in the engine are its output.
//
// A species does not replace the SYLLABLE and TIMBRE controls -- it biases
// them, the way SkyHowl's Obstacle biases its howl. Pitch, sweep, syllable
// length, harmonic richness, roughness and syllable rate all move together,
// because in the references they do.
enum SpeciesKind {
   kSpeciesWhistler = 0, // robin: 4.7 kHz, one harmonic, the purest voice measured
   kSpeciesSparrow,      // the library's ordinary small bird: 3.1 kHz, short, clean
   kSpeciesWarbler,      // nightingale and tui: lower, faster, two harmonics
   kSpeciesBudgie,       // parakeet chatter: 1.4 kHz, three harmonics, wide sweeps
   kSpeciesWoodpecker,   // the green woodpecker's laugh: up-sweeps at ten a second
   kSpeciesCrane,        // a waterbird's bugle: 1 kHz, four harmonics
   kSpeciesGoose,        // honking: 570 Hz, the widest sweeps in the library
   kSpeciesScreech,      // no reference: the top of every range at once, for effect
   kSpeciesPiper,        // oystercatchers: fast, clean, mid-pitched piping
   kNumSpecies
};

// The plugin's own table, and lookups into it.
const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

} // namespace chirpparade
