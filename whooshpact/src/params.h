#pragma once

#include <cstdint>

#include "verdalis/params.h"

namespace whooshpact {

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
   // Gesture: the shape of the whole sound in time. Every other layer is
   // driven by the position inside it, which is why it comes first.
   kParamType = 0,
   kParamBlend,
   kParamSpan,
   kParamPeak,
   kParamHold,
   kParamRise,
   kParamFall,
   kParamVariation,

   // Air: the noise layer. The whoosh itself, and the only layer a plain
   // white-noise transition needs.
   kParamNoise,
   kParamAirLevel,
   kParamAirCutoff,
   kParamAirSweep,
   kParamAirReso,
   kParamAirCurve,
   kParamAirTilt,
   kParamAirWidth,

   // Tone: the pitched layer. Braams and the upper half of a downshifter.
   kParamWave,
   kParamToneLevel,
   kParamTonePitch,
   kParamToneGlide,
   kParamToneDetune,
   kParamToneWidth,

   // Motion: where the gesture is, and what room it is in.
   kParamPanStart,
   kParamPanEnd,
   kParamSpaceAmount,
   kParamSpaceSize,
   kParamSpaceDamping,
   kParamSpaceWidth,

   // Sub: the low end. A boom is almost nothing else -- the library measures
   // 97 per cent of a boom's energy below 100 Hz.
   kParamSubLevel,
   kParamSubPitch,
   kParamSubDrop,
   kParamSubDecay,
   kParamSubDrive,
   kParamSubClick,

   // Hit: the transient at the peak. What makes an impact an impact.
   kParamHitLevel,
   kParamHitTone,
   kParamHitDecay,
   kParamHitNoise,
   kParamHitBody,
   kParamHitTime,

   // Flutter: the amplitude and cutoff being chopped, at a rate that glides
   // from one speed to another across the gesture.
   kParamFlutterDepth,
   kParamFlutterStart,
   kParamFlutterEnd,
   kParamFlutterShape,
   kParamFlutterTarget,
   kParamFlutterSmooth,

   // EQ: three bands plus the two ends, on the summed output.
   kParamHighpass,
   kParamLowpass,
   kParamEqLowFreq,
   kParamEqLowGain,
   kParamEqMidFreq,
   kParamEqMidGain,
   kParamEqHighFreq,
   kParamEqHighGain,

   // Envelope.
   kParamAttack,
   kParamDecay,
   kParamSustain,
   kParamRelease,
   kParamVelToLevel,
   kParamVelToTone,

   // Output.
   kParamGain,
   kParamDrive,
   kParamMaxVoices,
   kParamSeed,

   kNumParams
};

// The six families the reference library is filed under, and which this plugin
// models. Selecting one selects that family's *measured spectral profile*: the
// tilt of its octave-band curve above 125 Hz and how far its bottom two octaves
// stand over that curve, both taken from `tools/analysis/refs.py` and both
// expressed relative to Transition, which is therefore the neutral setting.
//
// It deliberately does **not** set the gesture's shape in time. That is fully
// parametric -- Span, Peak, Hold, Rise and Fall -- because the measured
// envelope contours in `tools/analysis/shape.py` are reproduced by those five
// numbers to within the spread of the library itself, and a type that also
// moved them would fight the preset that had just set them.
//
// Two tables of six numbers are formulas, not samples; see the suite's note on
// what pure synthesis does and does not forbid.
enum GestureKind {
   kGestureAccent = 0,   // short, bright, mid-heavy: a stab
   kGestureBoom,         // 97 % below 100 Hz, falls 9.1 dB/oct above 125
   kGestureBraam,        // the flattest of the six through the middle: a horn
   kGestureDownshifter,  // the steepest, 10.2 dB/oct: nothing above 2 kHz
   kGestureImpact,       // a boom with a top on it
   kGestureTransition,   // the reference: the broadest curve in the library
   kNumGestureKinds
};

// The colour of the Air layer's noise. White is flat; each of the others is
// white with one filter on it, which is what those names have always meant.
enum NoiseKind {
   kNoiseWhite = 0,
   kNoisePink,   // -3 dB/oct
   kNoiseBrown,  // -6 dB/oct
   kNoiseBlue,   // +3 dB/oct
   kNoiseViolet, // +6 dB/oct
   kNoiseGreen,  // mid-band emphasis around 500 Hz
   kNumNoiseKinds
};

// The Tone layer's waveform. Saw and Square are band-limited with polyBLEP,
// because a braam glides upwards by an octave or more and a naive edge would
// alias audibly on the way.
enum WaveKind {
   kWaveSine = 0,
   kWaveTriangle,
   kWaveSaw,
   kWaveSquare,
   kWaveSupersaw, // the saw stack detuned against itself
   kNumWaveKinds
};

// What the flutter's modulator looks like, and what it acts on.
enum FlutterShapeKind {
   kFlutterSine = 0,
   kFlutterTriangle,
   kFlutterSquare,
   kFlutterRamp,
   kFlutterRandom,
   kNumFlutterShapes
};

enum FlutterTargetKind {
   kFlutterTargetLevel = 0,
   kFlutterTargetFilter,
   kFlutterTargetBoth,
   kNumFlutterTargets
};

// The measured profile of one gesture family. Both numbers are relative to
// Transition; see GestureKind above.
struct TypeProfile {
   float tiltDbPerOct; // slope of the octave curve above 125 Hz
   float shelfDb;      // how far 63 Hz stands over 125 Hz
};

const TypeProfile &typeProfile(int kind);

// The plugin's own table, and lookups into it.
const ParamDesc *paramTable();
const ParamDesc *paramById(uint32_t id);
const ParamDesc *paramByKey(const char *key);

} // namespace whooshpact
