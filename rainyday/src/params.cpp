#include "params.h"

#include "verdalis/param_macros.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace rainyday {

namespace {

const char *const kSurfaceNames[] = {"Water", "Puddle", "Leaves",  "Wood",
                                     "Metal", "Glass",  "Concrete", "Fabric"};
const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};

const ParamDesc kParams[kNumParams] = {
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Final output level of the whole instrument."),

   LOG(kParamDensity, "density", "Density", "Rain", 0.62, 0.2, 5000.0, "drops/s",
       "Average droplet arrival rate. Loudness is compensated, so this is texture, not volume."),
   PCT(kParamClumping, "clumping", "Clumping", "Rain", 0.25,
       "How much the arrival rate fluctuates -- surges and lulls instead of an even patter."),
   LOG(kParamDropPitch, "drop_pitch", "Drop Pitch", "Rain", 0.5, 40.0, 9000.0, "Hz",
       "Base resonant frequency of a droplet. Big drops land below it, fine ones above."),
   LIN(kParamPitchSpread, "pitch_spread", "Pitch Spread", "Rain", 0.0, 5.0, 1.6, "oct",
       "Random octave spread on top of the pitch that drop size already implies."),
   LOG(kParamDropDecay, "drop_decay", "Drop Decay", "Rain", 0.54, 1.0, 1200.0, "ms",
       "Base ring time of a droplet. The surface scales it further."),
   PCT(kParamDecaySpread, "decay_spread", "Decay Spread", "Rain", 0.5,
       "Randomises ring time from drop to drop."),
   PCT(kParamTonality, "tonality", "Tonality", "Rain", 0.35,
       "Noisy splat at the left, pitched plink at the right."),
   PCT(kParamImpact, "impact", "Impact", "Rain", 0.5,
       "Weight of the broadband click at the moment of impact."),
   PCT(kParamSplash, "splash", "Splash", "Rain", 0.35,
       "Length and weight of the wet noise burst after the impact."),
   PCT(kParamSlosh, "slosh", "Slosh", "Rain", 0.5,
       "How long the splash cascades. A drop landing on a hard wet surface -- "
       "stone, wood or glass -- throws secondary droplets that land a few "
       "milliseconds apart; this is how many, and so how much it sounds like "
       "water rather than a hiss. Surfaces that hold no film of water ignore "
       "it."),
   PCT(kParamLevelSpread, "level_spread", "Level Spread", "Rain", 0.6,
       "Skew of the drop-size distribution, which also spreads pitch and decay."),
   BIPCT(kParamChirp, "chirp", "Chirp", "Rain", 0.35,
         "Per-droplet pitch bend. Positive rises, the way a bubble in water does."),
   ENUM(kParamSurface, "surface", "Surface", "Rain", 0.0, kSurfaceNames,
        "What the rain is falling on. Biases decay, resonance, click and chirp."),
   PCT(kParamNoteTracking, "note_tracking", "Note Tracking", "Rain", 0.5,
       "How far the played MIDI note transposes droplet pitch."),

   LIN(kParamBedLevel, "bed_level", "Bed Level", "Distant", -60.0, 6.0, -12.0, "dB",
       "Level of the far-field wash -- the drops too distant to hear individually."),
   PCT(kParamBedTone, "bed_tone", "Bed Tone", "Distant", 0.5,
       "Lowpass corner of the bed, dark to bright. Level compensated."),
   PCT(kParamBedBody, "bed_body", "Bed Body", "Distant", 0.2,
       "Resonance at the bed's corner frequency."),
   PCT(kParamBedDrift, "bed_drift", "Bed Drift", "Distant", 0.3,
       "Slow intensity drift shared by the bed and the droplet rate."),

   PCT(kParamWidth, "width", "Drop Width", "Close", 0.85,
       "How wide droplets are panned and how decorrelated the bed is."),
   PCT(kParamDistance, "distance", "Distance", "Space", 0.3,
       "Pushes the whole rain field away: quieter, duller, further back."),
   PCT(kParamAir, "air", "Air Absorption", "Space", 0.5,
       "How much high end distance costs."),
   PCT(kParamSpaceAmount, "space_amount", "Space Amount", "Space", 0.2,
       "Mix of the room the rain falls in: its first reflections and its tail."),
   PCT(kParamSpaceSize, "space_size", "Space Size", "Space", 0.5,
       "Dimension of that room, 3 m to 90 m. Reflections and decay time follow from it."),
   PCT(kParamSpaceDamping, "space_damping", "Space Damping", "Space", 0.5,
       "Absorption of its surfaces: bright stone at the left, soft and absorbent at the right."),

   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Response of the global output filter."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 20.0, 20000.0, "Hz",
       "Corner frequency of the output filter. Fully open at 20 kHz."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Emphasis at the cutoff frequency."),
   PCT(kParamFilterKeyTrack, "filter_key_track", "Filter Key Track", "Filter", 0.0,
       "How far the played note moves the cutoff. Follows the most recent note."),

   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.551, 1.0, 15000.0, "ms",
       "Fade-in of the rain after a note starts."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.665, 1.0, 15000.0, "ms",
       "Fall from full level down to the sustain level."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level the rain holds at while the note is held."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.716, 1.0, 20000.0, "ms",
       "Fade-out after the note is let go."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity to Level", "Envelope", 0.5,
       "How much note velocity controls loudness."),
   PCT(kParamVelToDensity, "vel_to_density", "Velocity to Density", "Envelope", 0.3,
       "How much note velocity controls the droplet rate."),

   STEP(kParamMaxDroplets, "max_droplets", "Max Droplets", "System", 32.0, 2048.0, 512.0, "",
        "Ceiling on simultaneously sounding droplets. Trade CPU against detail."),
   STEP(kParamSeed, "seed", "Random Seed", "System", 0.0, 999.0, 0.0, "",
        "Starting point of the random sequence, for a repeatable rain."),

   // Appended after the original 36, so the ids above keep their meaning.
   PCT(kParamBubble, "bubble", "Bubble Chance", "Rain", 1.0,
       "Fraction of droplets that ring at all. The rest are only splash and click."),

   // The near droplets and the far-field bed are two layers of the same rain and
   // used to share one width control between them. Each now has its own.
   BIPCT(kParamDropPan, "drop_pan", "Drop Pan", "Close", 0.0,
         "Slides the close droplets left or right without narrowing them."),
   PCT(kParamBedWidth, "bed_width", "Bed Width", "Distant", 0.85,
       "Stereo spread of the far-field bed, independent of the droplets."),
   BIPCT(kParamBedPan, "bed_pan", "Bed Pan", "Distant", 0.0,
         "Slides the far-field bed left or right without narrowing it."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.0, 20.0, 2000.0, "Hz",
       "Rolls off the bottom at 12 dB/oct. Off at the far left."),

   // ---------------------------------------------------------------- trickle
   //
   // Drops landing on a hard surface, as a layer of its own, and the same
   // generator RiverFlow's Trickle uses -- so these eight controls mean exactly
   // what they mean there and the same settings give the same sound. Every
   // figure below is measured from RiverFlow's reference library of running
   // water; see riverflow/tools/analysis/README.md.
   LIN(kParamTrickleLevel, "trickle_level", "Trickle Level", "Trickle", -60.0, 6.0, -30.0, "dB",
       "Level of the drops-on-a-surface layer. At minimum it is off and the plugin is "
       "droplets and bed alone."),
   LOG(kParamTrickleRate, "trickle_rate", "Trickle Rate", "Trickle", 0.578, 0.5, 200.0, "/s",
       "Drops a second striking the surface. Measured 5-25 a second across the water "
       "references, median 16."),
   LOG(kParamTrickleSize, "trickle_size", "Trickle Size", "Trickle", 0.705, 0.15, 3.0, "mm",
       "Radius of the pocket of air a drop entrains, which is its pitch: Minnaert gives\n"
       "       f0 = 3.26/r kHz. Measured 0.42-1.72 mm, median 1.24 -- whose predicted\n"
       "       2629 Hz matches a measured median of 2625."),
   LIN(kParamTrickleSpread, "trickle_spread", "Trickle Spread", "Trickle", 0.0, 3.0, 1.2, "oct",
       "Spread of drop sizes. Measured pitches span about 1.25 octaves around the median."),
   LOG(kParamTrickleDecay, "trickle_decay", "Trickle Decay", "Trickle", 0.407, 2.0, 200.0, "ms",
       "How long one drop lasts. Measured 8-86 ms to fall 10 dB, median 13; the long end "
       "of that is splashing rather than ticking."),
   PCT(kParamTrickleImpact, "trickle_impact", "Trickle Impact", "Trickle", 0.45,
       "The strike itself against the pocket that follows it. A drop landing on rock is "
       "mostly a broadband tick off the rock; a drop landing in water is mostly the "
       "bubble."),
   LOG(kParamStoneTone, "stone_tone", "Stone", "Trickle", 0.545, 800.0, 12000.0, "Hz",
       "What the drops are landing on: the centre of the surface's own resonance. Wet "
       "rock and glass are high and hard, wood and moss lower and duller."),
   PCT(kParamTrickleSplash, "trickle_splash", "Trickle Splash", "Trickle", 0.25,
       "How much of a drop is a short wash of water rather than a clean tick. The "
       "references' drop decays are bimodal -- 8-13 ms for a tick against 56-86 ms where "
       "the water is deeper -- and this is that second population."),

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

} // namespace rainyday
