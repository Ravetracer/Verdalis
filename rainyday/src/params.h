#pragma once

#include <cstdint>

namespace rainyday {

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
   kNumParams
};

enum class ParamKind {
   Linear,  // host value is the real value
   Percent, // host value 0..1, displayed as 0..100 %
   Log,     // host value 0..1, mapped exponentially onto [dispMin, dispMax]
   Stepped, // integer host value
   Enum     // integer host value with names
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

enum FilterKind { kFilterLowpass = 0, kFilterBandpass, kFilterHighpass, kFilterNotch };

struct ParamDesc {
   uint32_t id;
   const char *key;    // stable machine key used in preset files
   const char *name;   // human readable name shown by the host
   const char *module; // host-side grouping, e.g. "Rain"
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
// by the DSP (Hz, ms, dB, ratio, ...).
double paramToReal(const ParamDesc &desc, double raw);

// Inverse of paramToReal, used when parsing preset files written in
// real-world units.
double realToParam(const ParamDesc &desc, double real);

// Formats `raw` for display. Returns false if the buffer was too small.
bool paramValueToText(const ParamDesc &desc, double raw, char *out, uint32_t outSize);
bool paramTextToValue(const ParamDesc &desc, const char *text, double *outRaw);

} // namespace rainyday
