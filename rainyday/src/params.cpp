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

   // ------------------------------------------------------------------- tack
   PCT(kParamTack, "tack", "Tack", "Rain", 0.45,
       "How much of each impact is a broadband tack off the surface rather than a\n"
       "       tuned tick. This is measured, and the measurement is what the parameter\n"
       "       exists for: the event-triggered spectra of real drops on stone have a Q\n"
       "       of about 6 -- broad enough to have no pitch of their own and to take\n"
       "       their colour from the surface -- and fall 10 dB in about 7 ms. An impact\n"
       "       modelled as a damped sine has whatever Q its damping gives it, which\n"
       "       here measured 18 and rang for 96 ms. That is the difference between a\n"
       "       plink and a drop landing on concrete."),
   LOG(kParamTackTone, "tack_tone", "Tack Tone", "Rain", 0.5, 600.0, 12000.0, "Hz",
       "Where that tack sits: what the surface is made of. Wet rock and glass are high "
       "and hard, wood and leaves lower and duller. The Surface setting moves this, and "
       "this trims it."),
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
