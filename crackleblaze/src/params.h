#pragma once

#include <cstdint>

#include "verdalis/params.h"

namespace crackleblaze {

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
   // Blaze: the combustion roar. Quieter than a river's bed relative to what
   // sits on it -- the library's median crest factor is 31.7 dB against a
   // river's 19.6 -- but it is what tells a hearth from a bonfire.
   kParamFireType = 0,
   kParamFireBlend,
   kParamRoarLevel,
   kParamRoarTilt,
   kParamRoarBody,
   kParamFlare,
   kParamFlareRate,
   kParamDraught,

   // Crackle: the ticks. Pyrolysis gas breaking out of heated wood, which is
   // a click and not a ring -- measured spectral flatness 0.67.
   kParamCrackleRate,
   kParamCrackleLevel,
   kParamCrackleDecay,
   kParamCrackleTone,
   kParamCrackleSpread,
   kParamBurst,
   kParamSnap,
   kParamCrackleBody,

   // Sizzle: the same event held open. Steam leaving wet wood takes 19 ms to
   // fall 10 dB where a dry tick takes 2.5.
   kParamSap,
   kParamSizzleLevel,
   kParamSizzleDecay,
   kParamSizzleTone,
   kParamSteam,

   // Settle: a log giving way. Rare -- a median of six a minute -- and the
   // only thing in the plugin with no top end at all.
   kParamSettleRate,
   kParamSettleLevel,
   kParamSettleTone,
   kParamSettleDecay,

   // Hearth: what the fire is burning in, and where the listener is.
   kParamHearthType,
   kParamDistance,
   kParamAir,
   kParamWidth,
   kParamRoarWidth,
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
   kParamVelToFire,

   // Output.
   kParamGain,
   kParamMaxEvents,
   kParamSeed,

   kNumParams
};

// The five octave-band shapes the reference library actually falls into, in
// order of how bright they are. They are not names invented for the window:
// `tools/analysis/shapes.py` clusters the seventeen usable recordings by the
// octave-band colour of their *bed* -- with the crackles gated out first, which
// is not optional at a crest factor of 31.7 dB -- and these are the five
// clusters it finds, each shipped as the measured centroid of its members.
// Selecting one selects that centroid.
//
// A table of eight numbers is a formula, not a sample -- see the suite's note
// on pure synthesis. Nothing here reproduces recorded audio; the numbers say
// what colour to give a noise source the plugin generates itself.
enum FireKind {
   kFireDeepBlaze = 0,  // most low end, dullest top: a big fire at a distance
   kFireLogFire,        // peak at 125 Hz, scooped middle, top back up
   kFireCampFire,       // broad and even, brightest above 4 kHz
   kFireOpenFlame,      // nothing at 125 Hz, flat and bright: close, unenclosed
   kFireStoveDraught,   // 22 dB down at 1 kHz and peaking at 16: a draught
   kNumFireKinds
};

// What the fire is burning in, which decides how much of an early reflected
// field there is. The lesson is ShoreBreak's and RiverFlow's alike: eight
// discrete early reflections outdoors is what makes a reverb sound like a
// bathroom, and a fire in a field has nothing close enough to reflect off. A
// fireplace and a stove do, and the two references recorded inside one measure
// as the most reverberant in the library.
enum HearthKind {
   kHearthOpen = 0,
   kHearthFireRing,
   kHearthHearth,
   kHearthFireplace,
   kHearthStove,
   kHearthCavern,
   kNumHearthKinds
};

// The plugin's own table, and lookups into it.
const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

} // namespace crackleblaze
