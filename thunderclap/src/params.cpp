#include "params.h"

#include "verdalis/param_macros.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace thunderclap {

namespace {

const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
const char *const kModeNames[] = {"One Shot", "Gated", "Storm"};

const ParamDesc kParams[kNumParams] = {
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Final output level of the whole instrument."),

   LOG(kParamDistance, "distance", "Distance", "Strike", 0.5, 0.2, 20.0, "km",
       "How far away the lightning strikes. Near is a crack, far is a rumble. Level is compensated."),
   LOG(kParamHeight, "height", "Height", "Strike", 0.699, 1.0, 10.0, "km",
       "Height of the cloud base the channel comes down from. Taller channels rumble longer."),
   LIN(kParamCloudSpread, "cloud_spread", "Cloud Spread", "Strike", 0.0, 20.0, 4.0, "km",
       "Length of the channel that runs inside the cloud. This is what stretches a distant thunder out."),
   PCT(kParamTortuosity, "tortuosity", "Tortuosity", "Strike", 0.5,
       "How crooked the channel is. A straight channel claps once; a jagged one crackles and rolls."),
   PCT(kParamBranching, "branching", "Branching", "Strike", 0.4,
       "Side branches off the main channel, each a smaller thunder of its own."),
   STEP(kParamStrokes, "strokes", "Strokes", "Strike", 1.0, 8.0, 3.0, "",
        "Return strokes down the same channel. Each repeats the thunder a little quieter."),
   LOG(kParamStrokeGap, "stroke_gap", "Stroke Gap", "Strike", 0.54, 5.0, 500.0, "ms",
       "Time between return strokes. Short gaps thicken the clap; long ones stutter."),
   PCT(kParamVariation, "variation", "Variation", "Strike", 0.5,
       "How far each flash may wander from these settings: distance, height, strokes, direction."),

   PCT(kParamCrack, "crack", "Crack", "Sound", 0.5,
       "Sharpness of the shock fronts. All the high end of a close strike lives here."),
   PCT(kParamWeight, "weight", "Weight", "Sound", 0.5,
       "Length of each shock wave, and so how deep the thunder sits."),
   PCT(kParamSwell, "swell", "Swell", "Sound", 0.3,
       "Shadows the low part of the channel, so the thunder swells in from the sky instead of cracking."),
   PCT(kParamRumble, "rumble", "Rumble", "Sound", 0.5,
       "A rolling noise floor that follows the density of arriving shocks."),
   PCT(kParamRumbleTone, "rumble_tone", "Rumble Tone", "Sound", 0.5,
       "Lowpass corner of the rumble, 60 Hz to 1.5 kHz: a subsonic floor or a mid growl."),
   PCT(kParamAir, "air", "Air Absorption", "Sound", 0.5,
       "Humidity of the air: how much high end is lost per kilometre."),
   PCT(kParamScatter, "scatter", "Scatter", "Sound", 0.5,
       "Random spread of level and length from shock to shock."),
   PCT(kParamFocus, "focus", "Focus", "Sound", 0.7,
       "Directivity of each channel element. Focused, only the parts side-on to you are loud."),

   PCT(kParamWidth, "width", "Width", "Stereo", 0.8,
       "How far the channel's spread across the sky is mapped onto the stereo field."),
   BIPCT(kParamPan, "pan", "Pan", "Stereo", 0.0,
         "Direction of the strike, left to right."),
   PCT(kParamRumbleWidth, "rumble_width", "Rumble Width", "Stereo", 0.9,
       "Stereo spread of the rumble, independent of the shocks."),
   PCT(kParamDrift, "drift", "Drift", "Stereo", 0.3,
       "Slow wander of the rumble across the stereo field while a thunder plays."),

   LIN(kParamEchoLevel, "echo_level", "Echo Level", "Echoes", -60.0, 6.0, -12.0, "dB",
       "Level of the long echoes off hills, buildings and the cloud base."),
   STEP(kParamEchoCount, "echo_count", "Echo Count", "Echoes", 0.0, 8.0, 3.0, "",
        "How many distinct reflectors there are."),
   LOG(kParamEchoSpread, "echo_spread", "Echo Spread", "Echoes", 0.661, 0.1, 6.0, "s",
       "How far away the reflectors are, as the delay of the furthest one."),
   PCT(kParamEchoDamping, "echo_damping", "Echo Damping", "Echoes", 0.5,
       "How much high end each echo loses on the way."),

   PCT(kParamSpaceAmount, "space_amount", "Space Amount", "Space", 0.15,
       "Mix of the room you hear it from: its first reflections and its tail."),
   PCT(kParamSpaceSize, "space_size", "Space Size", "Space", 0.5,
       "Dimension of that room, 3 m to 90 m. Reflections and decay time follow from it."),
   PCT(kParamSpaceDamping, "space_damping", "Space Damping", "Space", 0.5,
       "Absorption of its surfaces: bright stone at the left, soft and absorbent at the right."),

   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Response of the global output filter."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.0, 20.0, 2000.0, "Hz",
       "Rolls off the bottom at 12 dB/oct. Off at the far left."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 20.0, 20000.0, "Hz",
       "Corner frequency of the output filter. Fully open at 20 kHz."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Emphasis at the cutoff frequency."),
   PCT(kParamFilterKeyTrack, "filter_key_track", "Filter Key Track", "Filter", 0.0,
       "How far the played note moves the cutoff. Follows the most recent note."),

   ENUM(kParamMode, "mode", "Mode", "Envelope", 0.0, kModeNames,
        "One Shot plays a flash out. Gated fades it when you let go. Storm keeps flashing while held."),
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.0, 1.0, 15000.0, "ms",
       "Fade-in applied to the shocks as they arrive. Softens the first crack."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.72, 1.0, 20000.0, "ms",
       "Fade-out of what is still to arrive after the note is let go, in Gated and Storm modes."),
   LOG(kParamStormRate, "storm_rate", "Storm Rate", "Envelope", 0.438, 1.0, 60.0, "/min",
       "Average flashes per minute in Storm mode. They arrive at random, never on a grid."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity to Level", "Envelope", 0.5,
       "How much note velocity controls loudness."),
   PCT(kParamVelToDistance, "vel_to_distance", "Velocity to Distance", "Envelope", 0.5,
       "Soft notes strike further away. At full depth a gentle note is eight times as far."),

   STEP(kParamMaxShocks, "max_shocks", "Max Shocks", "System", 128.0, 4096.0, 2048.0, "",
        "Number of channel elements per flash. Trade CPU against detail."),
   STEP(kParamSeed, "seed", "Random Seed", "System", 0.0, 999.0, 0.0, "",
        "Starting point of the random sequence. Zero never repeats a thunder."),

   // Appended after the original 41, so the ids above keep their meaning.
   PCT(kParamCompress, "compress", "Compress", "Dynamics", 0.0,
       "Lifts the rumble and the far claps towards the crack. Threshold and ratio move together; level is made up."),
   LOG(kParamCompAttack, "comp_attack", "Comp Attack", "Dynamics", 0.5, 0.1, 100.0, "ms",
       "How fast the compressor grabs a crack. Slow lets the first snap through."),
   LOG(kParamCompRelease, "comp_release", "Comp Release", "Dynamics", 0.565, 10.0, 3000.0, "ms",
       "How fast it lets go afterwards. Slow is a smooth swell; fast pumps with the claps."),

   PCT(kParamImpact, "impact", "Impact", "Impact", 0.45,
       "How hard the near field lands. Adds the blast the channel throws off and bends the crests "
       "of the shocks the way a wave that loud really does: a slam instead of a spray."),

   PCT(kParamBloom, "bloom", "Bloom", "Impact", 0.45,
       "How long the clap takes to assemble. The strike arrives thin and bright and the bottom "
       "floods in behind it; at zero it all lands at once."),

   PCT(kParamGround, "ground", "Ground", "Space", 0.55,
       "How hard the surface you are standing on is. Every shock arrives twice, direct and off "
       "the ground: that lifts the bottom and hollows the low mid. At zero you are in free air."),
};

#undef LIN
#undef PCT
#undef BIPCT
#undef LOG
#undef STEP
#undef ENUM

} // namespace

const ParamDesc *paramTable() { return kParams; }

const ParamDesc *paramById(uint32_t id) { return paramByIdIn(kParams, kNumParams, id); }

const ParamDesc *paramByKey(const char *key) { return paramByKeyIn(kParams, kNumParams, key); }

} // namespace thunderclap
