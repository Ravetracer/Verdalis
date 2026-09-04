#pragma once

#include <cstdint>

namespace thunderclap {

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

   kNumParams
};

enum class ParamKind {
   Linear,  // host value is the real value
   Percent, // host value 0..1, displayed as 0..100 %
   Log,     // host value 0..1, mapped exponentially onto [dispMin, dispMax]
   Stepped, // integer host value
   Enum     // integer host value with names
};

enum FilterKind { kFilterLowpass = 0, kFilterBandpass, kFilterHighpass, kFilterNotch };

// How a note turns into thunder.
enum TriggerMode {
   kModeOneShot = 0, // one flash per note, plays out whatever the note does
   kModeGated,       // one flash per note, letting go fades what is still to come
   kModeStorm,       // flashes keep coming at Storm Rate while the note is held
   kNumModes
};

struct ParamDesc {
   uint32_t id;
   const char *key;    // stable machine key used in preset files
   const char *name;   // human readable name shown by the host
   const char *module; // host-side grouping, e.g. "Strike"
   double min;
   double max;
   double def;
   ParamKind kind;
   double dispMin; // only meaningful for Log
   double dispMax;
   const char *unit;
   const char *const *enumNames;
   uint32_t enumCount;
   const char *tip; // one-line explanation, shown in the plugin's help line
};

const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

// Convert a raw host-facing parameter value into the real-world quantity used
// by the DSP (km, ms, dB, ratio, ...).
double paramToReal(const ParamDesc &desc, double raw);

// Inverse of paramToReal, used when parsing preset files written in
// real-world units.
double realToParam(const ParamDesc &desc, double real);

// Formats `raw` for display. Returns false if the buffer was too small.
bool paramValueToText(const ParamDesc &desc, double raw, char *out, uint32_t outSize);
bool paramTextToValue(const ParamDesc &desc, const char *text, double *outRaw);

} // namespace thunderclap
