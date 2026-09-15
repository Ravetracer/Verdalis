#include "params.h"

#include "verdalis/param_macros.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace whooshpact {

namespace {

const char *const kTypeNames[] = {"Accent",      "Boom",   "Braam",
                                  "Downshifter", "Impact", "Transition"};
const char *const kNoiseNames[] = {"White", "Pink", "Brown", "Blue", "Violet", "Green"};
const char *const kWaveNames[] = {"Sine", "Triangle", "Saw", "Square", "Supersaw"};
const char *const kFlutterShapeNames[] = {"Sine", "Triangle", "Square", "Ramp", "Random"};
const char *const kFlutterTargetNames[] = {"Level", "Filter", "Both"};

// The measured spectral profile of each family, relative to Transition.
//
// `tools/analysis/refs.py` takes the octave-band curve of all 211 references
// and prints the median per family. The slope above 125 Hz and the height of
// 63 Hz over 125 Hz are read off those curves; subtracting Transition's leaves
// what actually distinguishes the families, which is what is stored:
//
//   family        125 Hz .. 8 kHz slope   63 Hz over 125 Hz
//   Accent               -3.73 dB/oct          +7.4 dB
//   Boom                 -9.13                +12.5
//   Braam                -5.23                 +4.0
//   Downshifter         -10.18                +10.6
//   Impact               -4.43                 +7.0
//   Transition           -4.08                 +2.3
//
// Transition is the reference because it is the broadest curve in the library
// and the one the other five are heard against.
const TypeProfile kProfiles[kNumGestureKinds] = {
   /* Accent      */ {+0.35f, +5.1f},
   /* Boom        */ {-5.05f, +10.2f},
   /* Braam       */ {-1.15f, +1.7f},
   /* Downshifter */ {-6.10f, +8.3f},
   /* Impact      */ {-0.35f, +4.7f},
   /* Transition  */ {0.0f, 0.0f},
};

// Every default below is a measurement, not a preference. The numbers and how
// they were taken are in tools/analysis/README.md; the short version is 211
// reference gestures across six families, measured for how long they last,
// where in them the peak sits, how their spectrum moves from start to end, how
// much of them is below 100 Hz, and at what rate their amplitude is being
// chopped at the beginning and at the end.
//
// The defaults form one complete sound -- the plain noise transition, which is
// the simplest thing the plugin can make and the one everything else is built
// out of. Tone and Hit are off in it, which is why their levels default to the
// bottom of their range.
const ParamDesc kParams[kNumParams] = {
   // --------------------------------------------------------------- gesture
   ENUM(kParamType, "type", "Type", "Gesture", 5.0, kTypeNames,
        "Which family's measured spectral profile the sound is given: the slope of its\n"
        "       octave-band curve above 125 Hz, and how far its bottom two octaves stand\n"
        "       over that curve. Both come from the 211-file reference library and both\n"
        "       are relative to Transition, the broadest curve in it, which is therefore\n"
        "       the neutral setting.\n"
        "\n"
        "       It does not set the shape in time. That is Span, Peak, Hold, Rise and\n"
        "       Fall, and those five reproduce every measured envelope contour in the\n"
        "       library to within the library's own spread -- so a type that moved them\n"
        "       as well would only fight the preset that had just set them."),
   PCT(kParamBlend, "blend", "Blend", "Gesture", 0.0,
       "Crossfades from the chosen profile towards the next one, so the six measured "
       "shapes are a continuum rather than six settings. At 0 the sound is exactly the "
       "measured profile."),
   LOG(kParamSpan, "span", "Span", "Gesture", 0.6815, 0.1, 20.0, "s",
       "How long the whole gesture lasts. The plugin cannot know how long a note will\n"
       "       be held, so this and not the note decides the shape: the rise, the middle\n"
       "       and the fall are all fractions of it.\n"
       "\n"
       "       Measured medians run from 3.6 s for an accent to 5.6 s for a braam, with\n"
       "       the whole library between 1.7 and 12.1 s."),
   PCT(kParamPeak, "peak", "Peak", "Gesture", 0.33,
       "Where in the span the sound is loudest, as a fraction of it. This one number is\n"
       "       what separates the families more sharply than anything else measured: a\n"
       "       boom peaks at 0.024 and a downshifter at 0.024, an impact at 0.060, an\n"
       "       accent at 0.043 -- all of them at the very front -- while a transition\n"
       "       peaks at 0.328 and a braam at 0.148.\n"
       "\n"
       "       So a hit is this near zero and a whoosh is this near a third."),
   PCT(kParamHold, "hold", "Hold", "Gesture", 0.10,
       "How long the sound stays at full level after the peak, as a fraction of the\n"
       "       span. Near zero for anything struck; a braam measures a plateau roughly a\n"
       "       third of its span long, and a downshifter over half."),
   LOG(kParamRise, "rise", "Rise", "Gesture", 0.5696, 0.25, 8.0, "",
       "The curvature of the approach to the peak. 1 is a straight ramp, above 1 holds\n"
       "       back and then arrives, below 1 jumps and then creeps. Fitted to the\n"
       "       measured transition contour, which rises as t^1.8."),
   LOG(kParamFall, "fall", "Fall", "Gesture", 0.6644, 0.25, 8.0, "",
       "The curvature of the fall after the peak, on the same scale. Fitted to the\n"
       "       measured impact contour, which is down to two thirds a tenth of the way\n"
       "       through its fall and to a quarter by a third of it -- about t^3.5."),
   PCT(kParamVariation, "variation", "Variation", "Gesture", 0.35,
       "How differently each trigger comes out. This is the reason the plugin exists:\n"
       "       a sample plays the same transition every time and a track full of them\n"
       "       reads as one sound repeated. Here every note draws its own span, peak,\n"
       "       cutoff, pitch, pan, flutter rate and level around the settings on screen,\n"
       "       and no two are alike.\n"
       "\n"
       "       At 0 the plugin is deterministic and repeats exactly, which is what to\n"
       "       use when a transition has to line up with a cut."),

   // ------------------------------------------------------------------- air
   ENUM(kParamNoise, "noise", "Noise", "Air", 0.0, kNoiseNames,
        "The colour of the noise the whoosh is made of. White is flat, Pink falls\n"
        "       3 dB/octave, Brown 6, Blue and Violet rise by the same, and Green is a\n"
        "       broad mid-band emphasis around 500 Hz. Each is white noise with one\n"
        "       filter on it, which is what those names have always meant."),
   LIN(kParamAirLevel, "air_level", "Air Level", "Air", -60.0, 6.0, -6.0, "dB",
       "Level of the noise layer. On its own with everything else down this is the "
       "plain white-noise transition, which is the simplest thing the plugin makes."),
   LOG(kParamAirCutoff, "air_cutoff", "Cutoff", "Air", 0.6702, 40.0, 18000.0, "Hz",
       "Where the noise is filtered at the start of the gesture. The sweep moves it "
       "from here."),
   LIN(kParamAirSweep, "air_sweep", "Sweep", "Air", -4.0, 4.0, -1.6, "oct",
       "How far the cutoff travels across the gesture, in octaves. Negative closes\n"
       "       down, positive opens up.\n"
       "\n"
       "       The default is fitted rather than copied. The library's transitions fall\n"
       "       0.80 octaves in spectral *centroid* from start to end -- accents 0.69,\n"
       "       impacts 0.63, downshifters 1.07, and braams alone rise, by 0.60. A\n"
       "       centroid does not move as far as the cutoff that carries it, because\n"
       "       what is left below the corner does not move at all: rendering this at\n"
       "       -1.6 octaves measures -0.56 of centroid, which is where it sits."),
   PCT(kParamAirReso, "air_reso", "Resonance", "Air", 0.25,
       "How much the filter rings at its corner -- and, with it, how narrow the filter\n"
       "       is. At 0 this is a plain lowpass and the whoosh is a wall of air closing\n"
       "       down; at 1 it is a resonant bandpass and the whoosh has a pitch to\n"
       "       follow. The two go together because that is how the ear hears it: a\n"
       "       narrow moving band is a pitch, a broad one is wind."),
   LOG(kParamAirCurve, "air_curve", "Curve", "Air", 0.75, 0.25, 4.0, "",
       "When in the gesture the sweep happens. 1 spreads it evenly; above 1 holds the\n"
       "       cutoff still and then moves it late, which is what the measurement asks\n"
       "       for -- a transition's centroid is flat for its first third and then falls\n"
       "       0.8 octaves."),
   BIPCT(kParamAirTilt, "air_tilt", "Tilt", "Air", 0.0,
         "Tilts the noise brighter or darker, +/-6 dB/octave about 1 kHz, on top of the "
         "colour chosen above."),
   PCT(kParamAirWidth, "air_width", "Width", "Air", 0.6,
       "How decorrelated the two channels of the noise are. At 1 they are independent,\n"
       "       which is as wide as a noise source gets; at 0 they are the same signal\n"
       "       and the whoosh is a point in the middle.\n"
       "\n"
       "       The default is measured: the library's transitions have a median L/R\n"
       "       correlation of 0.60, and this renders at 0.61."),

   // ------------------------------------------------------------------ tone
   ENUM(kParamWave, "wave", "Wave", "Tone", 2.0, kWaveNames,
        "The waveform of the pitched layer. Saw and Square are band-limited, because a "
        "braam glides upwards by an octave or more and a naive edge would alias audibly "
        "on the way. Supersaw is the saw stack detuned against itself."),
   LIN(kParamToneLevel, "tone_level", "Tone Level", "Tone", -60.0, 6.0, -60.0, "dB",
       "Level of the pitched layer. Off in the default patch -- a plain noise "
       "transition has no pitch in it -- and the loudest thing in every braam."),
   LOG(kParamTonePitch, "tone_pitch", "Pitch", "Tone", 0.1946, 20.0, 2000.0, "Hz",
       "Where the pitched layer starts. The default is measured: the strongest partial\n"
       "       below 200 Hz in the loudest half second of a braam is 47 Hz, of a boom\n"
       "       49, of a downshifter 41 -- all of them within a tone of G1. The note\n"
       "       played transposes this."),
   LIN(kParamToneGlide, "tone_glide", "Glide", "Tone", -48.0, 48.0, -12.0, "st",
       "How far the pitch travels across the gesture, in semitones. Down is a\n"
       "       downshifter, up is a braam opening out -- the library's braams rise a\n"
       "       measured 0.6 octaves in centroid and their low partial by more than an\n"
       "       octave."),
   PCT(kParamToneDetune, "tone_detune", "Detune", "Tone", 0.25,
       "How far the three oscillators of the stack are pulled apart. What turns one "
       "note into a section."),
   PCT(kParamToneWidth, "tone_width", "Width", "Tone", 0.6,
       "How far the detuned oscillators are spread across the stereo field."),

   // ---------------------------------------------------------------- motion
   BIPCT(kParamPanStart, "pan_start", "Pan Start", "Motion", -0.6,
         "Where the gesture sits at its beginning. With Pan End this is the passby: a\n"
         "       whoosh that crosses the listener rather than sitting in front of them.\n"
         "       The library's transitions measure an L/R correlation of 0.60 against a\n"
         "       boom's 0.89, which is most of what makes one feel like movement."),
   BIPCT(kParamPanEnd, "pan_end", "Pan End", "Motion", 0.6,
         "Where it has arrived by the end. Equal to Pan Start for a sound that does not "
         "travel."),
   PCT(kParamSpaceAmount, "space_amount", "Space", "Motion", 0.22,
       "How much reverberant field there is around the gesture. Cinematic impacts live "
       "or die by this: the tail is most of what makes one sound big."),
   PCT(kParamSpaceSize, "space_size", "Size", "Motion", 0.45, "How big that space is."),
   PCT(kParamSpaceDamping, "space_damping", "Damping", "Motion", 0.5,
       "How quickly the space eats the top end."),
   PCT(kParamSpaceWidth, "space_width", "Reverb Width", "Motion", 0.8,
       "How wide the reverberant field is. Narrow keeps the tail behind the sound; "
       "wide puts it all around."),

   // ------------------------------------------------------------------- sub
   LIN(kParamSubLevel, "sub_level", "Sub Level", "Sub", -60.0, 6.0, -60.0, "dB",
       "Level of the low layer, off by default so that a patch opts into it. A boom is\n"
       "       almost nothing else: the library measures 97 per cent of a boom's energy\n"
       "       and 98 per cent of a downshifter's below 100 Hz, against 62 per cent for\n"
       "       a transition -- but a transition that has to sit under a mix often wants\n"
       "       no low end at all."),
   LOG(kParamSubPitch, "sub_pitch", "Sub Pitch", "Sub", 0.4570, 15.0, 200.0, "Hz",
       "Where the low layer starts. 49 Hz is the measured median fundamental of the "
       "booms, which is G1."),
   LIN(kParamSubDrop, "sub_drop", "Drop", "Sub", -48.0, 12.0, -12.0, "st",
       "How far the low layer falls across its decay, in semitones. A boom that does "
       "not drop sounds like a note; one that drops sounds like something hitting the "
       "ground."),
   LOG(kParamSubDecay, "sub_decay", "Decay", "Sub", 0.5934, 20.0, 8000.0, "ms",
       "How long the low layer takes to fall 20 dB. Measured 0.63 s at 63 Hz for the\n"
       "       booms, 0.78 for the impacts and 1.9 for the downshifters, against 0.35\n"
       "       for the accents -- the low decay is most of what tells those apart."),
   PCT(kParamSubDrive, "sub_drive", "Drive", "Sub", 0.2,
       "Saturation on the low layer alone. A pure sine at 45 Hz disappears on small "
       "speakers; the harmonics this adds are what carry it there."),
   PCT(kParamSubClick, "sub_click", "Click", "Sub", 0.3,
       "A very short upward pitch snap at the start of the low layer. It is what makes "
       "a boom read as an impact rather than as a note fading in."),

   // ------------------------------------------------------------------- hit
   LIN(kParamHitLevel, "hit_level", "Hit Level", "Hit", -60.0, 6.0, -60.0, "dB",
       "Level of the transient. Off in the default patch, and the whole point of "
       "every impact and accent preset."),
   LOG(kParamHitTone, "hit_tone", "Tone", "Hit", 0.5111, 60.0, 12000.0, "Hz",
       "Where the transient's energy sits."),
   LOG(kParamHitDecay, "hit_decay", "Decay", "Hit", 0.5916, 5.0, 3000.0, "ms",
       "How long the transient takes to fall 20 dB. The library's accents measure\n"
       "       0.32-0.50 s across the octaves above 500 Hz and its impacts 0.27-0.53,\n"
       "       which is a short sound with a long room on it rather than a long sound."),
   PCT(kParamHitNoise, "hit_noise", "Noise", "Hit", 0.6,
       "Morphs the transient from a cluster of three inharmonic resonators towards "
       "plain filtered noise. Low is metal, high is debris."),
   PCT(kParamHitBody, "hit_body", "Body", "Hit", 0.5,
       "How much low end the transient carries under its tone. Without it a hit is all "
       "crack and no weight."),
   PCT(kParamHitTime, "hit_time", "Time", "Hit", 0.33,
       "Where in the span the transient lands, as a fraction of it. At the peak for an "
       "impact; at the end of a whoosh for a whoosh-hit, which is what the library "
       "files under Transition - Whoosh Hit."),

   // --------------------------------------------------------------- flutter
   PCT(kParamFlutterDepth, "flutter_depth", "Flutter", "Flutter", 0.0,
       "How deeply the gesture is chopped up. Zero is off, and off is the default --\n"
       "       the library is mostly not fluttering. Measuring it says so plainly: the\n"
       "       fraction of the in-band modulation energy sitting at a single rate is\n"
       "       0.02 to 0.05 in five of the six families, which is noise with no rate at\n"
       "       all.\n"
       "\n"
       "       Where it does appear it is unmistakable, and it is the downshifters that\n"
       "       have it: `Downshifter - Stutter Scream` measures 0.12 and accelerates\n"
       "       from 4.1 to 25.0 Hz across its span. So this is a deliberate effect with\n"
       "       a measured range rather than something every preset gets."),
   LOG(kParamFlutterStart, "flutter_start", "Start Speed", "Flutter", 0.5190, 0.5, 60.0, "Hz",
       "How fast the chopping is at the beginning of the gesture. Measured 2.6 to "
       "25.0 Hz across the references that have any."),
   LOG(kParamFlutterEnd, "flutter_end", "End Speed", "Flutter", 0.6960, 0.5, 60.0, "Hz",
       "How fast it is by the end. The rate glides exponentially between the two, which\n"
       "       is what a single pair of numbers can do that an LFO cannot.\n"
       "\n"
       "       The measured end-to-start ratios run from 0.22 to 6.08 -- both\n"
       "       accelerating and slowing down happen, and the downshifters' median is a\n"
       "       doubling."),
   ENUM(kParamFlutterShape, "flutter_shape", "Shape", "Flutter", 0.0, kFlutterShapeNames,
        "What the chopping looks like. Sine is a tremolo, Square a gate, Ramp a "
        "repeated fall, Random a new level each cycle."),
   ENUM(kParamFlutterTarget, "flutter_target", "Target", "Flutter", 0.0, kFlutterTargetNames,
        "Whether the chopping moves the level, the Air layer's cutoff, or both. On the "
        "cutoff it is the filter fluttering rather than the amplitude, which is what "
        "most of the downshifters in the library are doing."),
   PCT(kParamFlutterSmooth, "flutter_smooth", "Smooth", "Flutter", 0.35,
       "Rounds the modulator off. A hard square at 20 Hz clicks on every edge; this is "
       "how much of that is taken away."),

   // -------------------------------------------------------------------- eq
   LOG(kParamHighpass, "highpass", "Highpass", "EQ", 0.1216, 15.0, 1000.0, "Hz",
       "Permanent highpass, and set low on purpose. The rest of the suite starts at\n"
       "       60 Hz because below that a field recording is traffic and ventilation;\n"
       "       here the sub-60 Hz band is the instrument. 25 Hz keeps the DC and the\n"
       "       inaudible cone travel out and nothing else."),
   LOG(kParamLowpass, "lowpass", "Lowpass", "EQ", 0.9470, 500.0, 22000.0, "Hz",
       "Permanent lowpass, for taking the fizz off the top of a noise layer."),
   LOG(kParamEqLowFreq, "eq_low_freq", "Low Freq", "EQ", 0.4241, 30.0, 400.0, "Hz",
       "Corner of the low shelf."),
   LIN(kParamEqLowGain, "eq_low_gain", "Low Gain", "EQ", -18.0, 18.0, 0.0, "dB",
       "Low shelf gain. This is the one control most of these sounds are actually "
       "mixed with: a cinematic impact is a bass decision."),
   LOG(kParamEqMidFreq, "eq_mid_freq", "Mid Freq", "EQ", 0.4857, 150.0, 6000.0, "Hz",
       "Centre of the mid bell."),
   LIN(kParamEqMidGain, "eq_mid_gain", "Mid Gain", "EQ", -18.0, 18.0, 0.0, "dB",
       "Mid bell gain. Cutting here is how a transition is made to sit under a mix "
       "instead of in front of it."),
   LOG(kParamEqHighFreq, "eq_high_freq", "High Freq", "EQ", 0.5805, 1000.0, 16000.0, "Hz",
       "Corner of the high shelf."),
   LIN(kParamEqHighGain, "eq_high_gain", "High Gain", "EQ", -18.0, 18.0, 0.0, "dB",
       "High shelf gain."),

   // -------------------------------------------------------------- envelope
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.1747, 0.2, 2000.0, "ms",
       "Fade-in applied on top of the gesture. Short by default: the gesture's own "
       "Rise is what shapes the front of the sound, and this is only here to take the "
       "edge off it."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.7224, 5.0, 20000.0, "ms",
       "Fall from the initial level to the sustain."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level held while the note is down. At 1 the gesture plays out exactly as its "
       "own shape says, whatever the note does."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.4936, 5.0, 20000.0, "ms",
       "Fade-out after the note is let go. This is how a gesture is cut short: the "
       "span decides how long the sound wants to be, and letting go early with a short "
       "release ends it anyway."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity To Level", "Envelope", 0.5,
       "How much velocity sets the level."),
   PCT(kParamVelToTone, "vel_to_tone", "Velocity To Tone", "Envelope", 0.3,
       "How much velocity opens the Air layer's filter and hardens the transient -- "
       "which is what hitting something harder actually does."),

   // ---------------------------------------------------------------- output
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, -4.0, "dB",
       "Output level."),
   PCT(kParamDrive, "drive", "Drive", "Output", 0.12,
       "Saturation on the summed output. The library's crest factors run from 7.7 dB "
       "for the downshifters to 19.0 for the accents, and the low end of that is a "
       "sound that has been driven hard."),
   STEP(kParamMaxVoices, "max_voices", "Max Voices", "Output", 1.0, 16.0, 8.0, "",
        "How many gestures may overlap. One is monophonic, which is what a transition "
        "usually wants; more lets a new hit land while the last one is still ringing."),
   STEP(kParamSeed, "seed", "Random Seed", "Output", 0.0, 999.0, 0.0, "",
        "Non-zero gives the same sequence of gestures every time, which is what makes "
        "a rendered transition reproducible. Zero is always different."),
};

#undef LIN
#undef PCT
#undef BIPCT
#undef LOG
#undef STEP
#undef ENUM

} // namespace

const TypeProfile &typeProfile(int kind) {
   if (kind < 0 || kind >= kNumGestureKinds)
      kind = kGestureTransition;
   return kProfiles[kind];
}

const ParamDesc *paramTable() { return kParams; }

const ParamDesc *paramById(uint32_t id) { return paramByIdIn(kParams, kNumParams, id); }

const ParamDesc *paramByKey(const char *key) { return paramByKeyIn(kParams, kNumParams, key); }

} // namespace whooshpact
