#pragma once

#include <cstdint>

#include "verdalis/params.h"

namespace thunderclap {

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

   // Strike: the lightning channel itself.
   kParamDistance,
   kParamHeight,
   kParamCloudSpread,
   kParamTortuosity,
   kParamBranching,
   kParamStrokes,
   kParamStrokeGap,
   kParamVariation,

   // Sound: how each shock is heard.
   kParamCrack,
   kParamWeight,
   kParamSwell,
   kParamRumble,
   kParamRumbleTone,
   kParamAir,
   kParamScatter,
   kParamFocus,

   // Stereo.
   kParamWidth,
   kParamPan,
   kParamRumbleWidth,
   kParamDrift,

   // Echoes: hills, buildings, the cloud base.
   kParamEchoLevel,
   kParamEchoCount,
   kParamEchoSpread,
   kParamEchoDamping,

   // Space: the room the listener is in.
   kParamSpaceAmount,
   kParamSpaceSize,
   kParamSpaceDamping,

   // Filter.
   kParamFilterType,
   kParamHighpass,
   kParamFilterCutoff,
   kParamFilterReso,
   kParamFilterKeyTrack,

   // Envelope and triggering.
   kParamMode,
   kParamAttack,
   kParamRelease,
   kParamStormRate,
   kParamVelToLevel,
   kParamVelToDistance,

   // System.
   kParamMaxShocks,
   kParamSeed,

   // Appended: output dynamics.
   kParamCompress,
   kParamCompAttack,
   kParamCompRelease,

   // Appended: the near channel's blast pulse.
   kParamImpact,

   // Appended: how long the clap takes to assemble.
   kParamBloom,

   kNumParams
};

// How a note turns into thunder.
enum TriggerMode {
   kModeOneShot = 0, // one flash per note, plays out whatever the note does
   kModeGated,       // one flash per note, letting go fades what is still to come
   kModeStorm,       // flashes keep coming at Storm Rate while the note is held
   kNumModes
};

// The plugin's own table, and lookups into it.
const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

} // namespace thunderclap
