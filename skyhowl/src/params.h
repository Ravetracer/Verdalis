#pragma once

#include <cstdint>

#include "verdalis/params.h"

namespace skyhowl {

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
   // Wind: the flow itself. Everything else is driven by it.
   kParamWindSpeed = 0,
   kParamTurbulence,
   kParamGustRate,
   kParamGustDepth,
   kParamGustLength,
   kParamGustShape,
   kParamSquall,
   kParamSquallRate,

   // Airflow: the broadband bed the flow makes as it goes past things.
   kParamFlowLevel,
   kParamFlowTone,
   kParamFlowTilt,
   kParamBuffet,
   kParamBuffetTone,
   kParamHiss,
   kParamSpeedLaw,

   // Howl: the aeolian tones. Vortices shed off an obstacle at a frequency
   // proportional to the wind speed, which is why howling swoops.
   kParamHowlAmount,
   kParamObstacle,
   kParamHowlSize,
   kParamHowlSpread,
   kParamHowlVoices,
   kParamHowlReso,
   kParamHowlTrack,
   kParamHowlThreshold,
   kParamWarble,

   // Rustle: foliage. Not wind at all, but what wind is usually heard through.
   kParamRustleAmount,
   kParamFoliage,
   kParamRustleSize,
   kParamRustleDensity,
   kParamRustleSpread,
   kParamRustleDecay,
   kParamRustleThreshold,
   kParamClatter,

   // Place: where the listener stands, and what the ground is like.
   kParamTerrain,
   kParamDistance,
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
   kParamVelToSpeed,

   // Output.
   kParamGain,
   kParamMaxGusts,
   kParamSeed,

   kNumParams
};

// What the wind is blowing past. Air on its own is silent: every sound in this
// plugin is the flow meeting something, and this is what it meets.
//
// The two families behave differently, and the difference is physics rather
// than voicing. A bluff body -- a wire, a twig, a blade of grass -- sheds
// vortices at the Strouhal frequency f = St U / d, so its pitch is
// proportional to the wind speed and rises with every gust. A cavity -- a gap
// under a door, a chimney, a cave mouth -- resonates at a frequency its own
// geometry fixes, and the flow only excites it; its pitch barely moves. The
// obstacle therefore sets how much of Howl Track actually applies.
enum ObstacleKind {
   kObstacleOpen = 0,  // nothing to shed off: almost no tone at all
   kObstacleGrass,     // sub-millimetre blades, many of them, high and hissy
   kObstacleReeds,     // thicker stems, a clear band around a kilohertz
   kObstacleTwigs,     // a few millimetres: the ordinary howl of a bare tree
   kObstacleBranches,  // centimetres, so an octave or two lower
   kObstacleWires,     // one diameter repeated: the narrowest, purest tone
   kObstacleRocks,     // broad, irregular, barely tonal
   kObstacleGap,       // a slot: a cavity mode, and it does not track the wind
   kObstacleCave,      // a large cavity, lower and much longer ringing
   kNumObstacles
};

// What the wind is blowing through. A bonus layer rather than the point of the
// plugin, but wind in the open is rarely what anybody actually records.
enum FoliageKind {
   kFoliageNone = 0,
   kFoliageBroadleaf,   // summer leaves: soft, mid, merging into a wash
   kFoliageDryLeaves,   // autumn: dry, bright and separately audible
   kFoliageConifer,     // needles: the highest and the most continuous, a sough
   kFoliageGrass,       // fine, fast, very high
   kFoliageReeds,       // hollow stems knocking against each other
   kFoliageBareBranches, // no leaves at all: sparse woody knocks
   kFoliageBushes,      // dense and close, between broadleaf and grass
   kNumFoliages
};

// The ground the wind is crossing. Terrain roughness is what sets turbulence
// intensity in the first place -- a boundary layer over a forest is far more
// turbulent than one over water -- so this biases the gusting as well as the
// spectrum.
enum TerrainKind {
   kTerrainPlain = 0,
   kTerrainMeadow,
   kTerrainForest,
   kTerrainMountain,
   kTerrainDesert,
   kTerrainCoast,
   kTerrainStreet,
   kTerrainTundra,
   kNumTerrains
};

// The plugin's own table, and lookups into it.
const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

} // namespace skyhowl
