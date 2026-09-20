#pragma once

#include <cstdint>

#include "verdalis/params.h"

namespace insectswarm {

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
   // Swarm: many flying individuals at once. The layer the plugin is named
   // for, and the one the species table is about.
   kParamSpecies = 0,
   kParamRateShift,
   kParamCount,
   kParamSwarmLevel,
   kParamSpread,
   kParamWander,
   kParamWanderRate,
   kParamRasp,

   // Wing: what one individual's buzz is made of. The species table fits a
   // shelf and a resonance to each species' measured harmonic stack; these
   // move that fit rather than replacing it.
   kParamTilt,
   kParamFormant,
   kParamResonance,
   kParamStroke,
   kParamBite,
   kParamFlutter,

   // Flyby: one individual passing the listener. Its audible cue is the level
   // rise, not the Doppler shift -- see Speed.
   kParamFlybyRate,
   kParamFlybyLevel,
   kParamFlybyRise,
   kParamFlybyPass,
   kParamFlybySpeed,
   kParamFlybySweep,

   // Stridulate: the other mechanism entirely. A tymbal or a file-and-scraper
   // is a train of clicks ringing a resonant body, not a beating wing.
   kParamStridLevel,
   kParamCarrier,
   kParamCarrierQ,
   kParamPulseRate,
   kParamEchemeRate,
   kParamDuty,
   kParamChorus,
   kParamStridSpread,

   // Air: where the listener is standing.
   kParamDistance,
   kParamAir,
   kParamWidth,
   kParamSpaceAmount,
   kParamSpaceSize,
   kParamSpaceDamping,

   // Bed: the field the insects are in.
   kParamBedLevel,
   kParamBedTone,
   kParamBedTilt,

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
   kParamVelToSwarm,

   // Output.
   kParamGain,
   kParamMaxVoices,
   kParamSeed,

   // Swarm, appended. An individual's distance to the listener is not fixed:
   // it flies towards the microphone and away from it again, which is a slow
   // level drift on top of everything else. Appended rather than placed with
   // the rest of the Swarm block because the ids are persisted.
   kParamRoam,
   kParamRoamRate,

   // Appended after the fact, so its id is last -- the ids are persisted in
   // presets and state and are never reordered. It belongs to Stridulate and
   // the window puts it there. Not `Stroke`: the WING panel already has one,
   // and a wing's half-stroke asymmetry is a different thing entirely.
   kParamScrape,

   kNumParams
};

// The species are the rows of the generated table in dsp/species_generated.h,
// in the same order: rising wingbeat rate, which is also roughly falling body
// size. The enum is here so the parameter table and the window can name them
// without pulling the DSP header in, and a static_assert in the engine keeps
// the two in step.
enum SpeciesKind {
   kSpeciesHornet = 0,
   kSpeciesBumblebee,
   kSpeciesWasp,
   kSpeciesHousefly,
   kSpeciesHoneybee,
   kSpeciesMosquito,
   kSpeciesDragonfly,
   kNumSpeciesKinds
};

// There is deliberately no Stridulator enum. The library measures a cicada and
// a cricket as the same mechanism with different numbers -- a train of clicks
// ringing a resonant body, at 5549 Hz and Q 13 against 4518 Hz and Q 26 -- and
// nothing else separates them, so a chip that picked between two models would
// be claiming a difference the recordings do not contain. Both measurements
// are shipped as factory presets instead, and the generated table in
// dsp/species_generated.h carries them for the manual to print.

// The plugin's own table, and lookups into it.
const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

} // namespace insectswarm
