#pragma once

#include <cstdint>

#include "verdalis/params.h"

namespace rainyday {

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
   kParamGain = 0,
   kParamDensity,
   kParamClumping,
   kParamDropPitch,
   kParamPitchSpread,
   kParamDropDecay,
   kParamDecaySpread,
   kParamTonality,
   kParamImpact,
   kParamSplash,
   kParamSlosh,
   kParamLevelSpread,
   kParamChirp,
   kParamSurface,
   kParamNoteTracking,
   kParamBedLevel,
   kParamBedTone,
   kParamBedBody,
   kParamBedDrift,
   kParamWidth,
   kParamDistance,
   kParamAir,
   kParamSpaceAmount,
   kParamSpaceSize,
   kParamSpaceDamping,
   kParamFilterType,
   kParamFilterCutoff,
   kParamFilterReso,
   kParamFilterKeyTrack,
   kParamAttack,
   kParamDecay,
   kParamSustain,
   kParamRelease,
   kParamVelToLevel,
   kParamVelToDensity,
   kParamMaxDroplets,
   kParamSeed,
   kParamBubble,

   // Appended for the Close / Distant split and the dedicated highpass.
   kParamDropPan,
   kParamBedWidth,
   kParamBedPan,
   kParamHighpass,

   // The trickle: drops landing on a hard surface, as its own layer. This is
   // RiverFlow's Trickle, generator and all, so the same settings give the same
   // sound in both plugins. It replaces the two-knob Tack of 1.6.0, which was
   // the same idea with most of its controls missing.
   kParamTrickleLevel,
   kParamTrickleRate,
   kParamTrickleSize,
   kParamTrickleSpread,
   kParamTrickleDecay,
   kParamTrickleImpact,
   kParamStoneTone,
   kParamTrickleSplash,
   kNumParams
};

// Surface materials -- these bias the droplet resonator model.
enum SurfaceKind {
   kSurfaceWater = 0,
   kSurfacePuddle,
   kSurfaceLeaves,
   kSurfaceWood,
   kSurfaceMetal,
   kSurfaceGlass,
   kSurfaceConcrete,
   kSurfaceFabric,
   kNumSurfaces
};

// The plugin's own table, and lookups into it.
const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

} // namespace rainyday
