#include "params.h"

#include "verdalis/param_macros.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace riverflow {

namespace {

const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
const char *const kWaterNames[] = {"Deep Rush", "Rapids", "Mountain River",
                                   "Stream",    "Creek",  "Trickle"};
const char *const kBankNames[] = {"Open", "Forest", "Gorge", "Rock Pool", "Culvert", "Cavern"};

// Every default below is a measurement, not a preference. The numbers and how
// they were taken are in tools/analysis/README.md; the short version is 77
// field recordings of rivers, creeks and waterfalls, 52 minutes, measured for
// octave-band shape, envelope statistics against a Gaussian control, and the
// rate, pitch and ring time of their discrete events.
const ParamDesc kParams[kNumParams] = {
   // ------------------------------------------------------------------ flow
   ENUM(kParamWaterType, "water_type", "Water", "Flow", 3.0, kWaterNames,
        "Which measured colour the bed has. The six choices are the six clusters the\n"
        "       reference library falls into, each shipped as its cluster's measured\n"
        "       octave-band centroid -- from Deep Rush, which peaks at 500 Hz and falls\n"
        "       25 dB by 16 kHz, to Trickle, which rises all the way to the top."),
   PCT(kParamWaterBlend, "water_blend", "Blend", "Flow", 0.0,
       "Crossfades from the chosen colour towards the next one, so the six measured "
       "shapes are a continuum rather than six settings. At 0 the bed is exactly the "
       "measured centroid."),
   LIN(kParamFlowLevel, "flow_level", "Flow Level", "Flow", -60.0, 6.0, -24.0, "dB",
       "Level of the bed, and it reads as exactly that: the bed is normalised to unit\n"
       "       RMS before this is applied, whichever colour is selected, so -24 dB is a\n"
       "       bed at -24 dBFS and changing colour is not also a level change. The bed\n"
       "       is most of a river -- the smoothest third of the library is statistically\n"
       "       indistinguishable from shaped Gaussian noise, so the bed alone is a\n"
       "       complete and correct model of those recordings."),
   BIPCT(kParamFlowTilt, "flow_tilt", "Tilt", "Flow", 0.0,
         "Tilts the whole measured shape brighter or darker, +/-6 dB/octave about 1 kHz. "
         "Moves a colour off its measured centre without discarding its shape."),
   PCT(kParamFlowBody, "flow_body", "Body", "Flow", 0.5,
       "Weight of the bed's lowest two bands, 125 and 250 Hz. A large river has body "
       "here; a creek measures nothing at all below 500 Hz."),
   PCT(kParamTurbulence, "turbulence", "Turbulence", "Flow", 0.22,
       "How much the bed's level moves, at the 1 kHz band. Measured 5-35% of the mean\n"
       "       across the library, median 11%, and the lower bands move 1.5 to 2.4 times\n"
       "       further -- so a river's low end surges while its hiss sits still. Each\n"
       "       band wanders on its own: the measured correlation between one band's\n"
       "       envelope and its neighbour's is 0.07-0.25, which is why this cannot be\n"
       "       one gain on the whole bed."),
   LOG(kParamSurgeRate, "surge_rate", "Surge Rate", "Flow", 0.557, 0.1, 10.0, "Hz",
       "How fast that movement is. It is a filtered random walk rather than an\n"
       "       oscillator: the envelope spectrum of every reference falls smoothly at\n"
       "       about -1.5 dB per decade over 0.3-10 Hz with no peak that survives from\n"
       "       one recording to the next, so a river has no rhythm. This sets the corner\n"
       "       of the noise that drives it."),
   PCT(kParamFlowGrain, "flow_grain", "Grain", "Flow", 0.85,
       "How granular the bed itself is, as against the countable events above it.\n"
       "       Frequency-weighted from the measurements: the library's 4 ms envelope\n"
       "       departs from a Gaussian control by 30% at 200-800 Hz and by 220% at\n"
       "       6-14 kHz, so this does far more to the top of the bed than to the bottom.\n"
       "       That is a vast number of sub-millimetre bubbles bursting -- too many and\n"
       "       too short to count, so a granular envelope on a band rather than\n"
       "       oscillators. Zero is right for the smooth third of the library; the\n"
       "       default reproduces the library's median."),

   // ---------------------------------------------------------------- stones
   LOG(kParamDabbleRate, "dabble_rate", "Dabble Rate", "Stones", 0.626, 0.2, 40.0, "/s",
       "Dabbles per second: water folding over a stone and trapping a pocket of air. "
       "Measured 1.4-9.3 a second across the grainy references, median 5.5."),
   LIN(kParamDabbleLevel, "dabble_level", "Dabble Level", "Stones", -60.0, 6.0, -28.0, "dB",
       "Level of the dabbling. Measured 5.5-16.7 dB above the bed it sits on."),
   LOG(kParamDabbleSize, "dabble_size", "Dabble Size", "Stones", 0.480, 1.0, 12.0, "mm",
       "Radius of a typical trapped pocket, which is its pitch: Minnaert gives\n"
       "       f0 = 3.26/r kHz, so 3.3 mm rings at 988 Hz. The library's measured\n"
       "       dabbles are 2.3-5.8 mm, median 3.3 -- and their measured median pitch is\n"
       "       984 Hz, which is Minnaert to within a per cent."),
   LIN(kParamDabbleSpread, "dabble_spread", "Dabble Spread", "Stones", 0.0, 3.0, 0.9, "oct",
       "Spread of sizes either side of that. The measured 10th to 90th percentile of "
       "dabble pitch spans 562-1406 Hz, which is about two-thirds of an octave each way."),
   STEP(kParamDabbleCluster, "dabble_cluster", "Cluster", "Stones", 1.0, 12.0, 4.0, "",
        "How many pockets one dabble makes. This is measured rather than chosen: the\n"
        "       event-triggered spectrum gives a Q of 0.7-5, so a single pocket would\n"
        "       ring for about a millisecond, while the event-triggered envelope takes\n"
        "       7-34 ms to fall 10 dB. One bubble cannot do both, so a dabble is a short\n"
        "       burst of several -- which is also what water folding over a stone does."),
   LOG(kParamDabbleSpill, "dabble_spill", "Spill", "Stones", 0.617, 2.0, 120.0, "ms",
       "How long a cluster is spread over. Measured 7-34 ms to fall 10 dB, median 25."),
   LIN(kParamDabbleDamping, "dabble_damping", "Damping", "Stones", 0.25, 4.0, 1.0, "x",
       "Multiplies the damping a pocket physically has. At 1 it is the radiative plus "
       "thermal loss after Xue et al.; below 1 rings longer, above 1 is deader."),
   PCT(kParamDabbleGlug, "dabble_glug", "Glug", "Stones", 0.35,
       "How much of a dabble is the water being displaced rather than the air ringing. "
       "Without it a cascade of pockets is a music box; with it, it is water."),

   // --------------------------------------------------------------- trickle
   LOG(kParamTrickleRate, "trickle_rate", "Trickle Rate", "Trickle", 0.578, 0.5, 200.0, "/s",
       "Drops a second striking stone or standing water. Measured 5-25 a second, "
       "median 16 -- the tickling of a small fall is at the top of that."),
   LIN(kParamTrickleLevel, "trickle_level", "Trickle Level", "Trickle", -60.0, 6.0, -24.0, "dB",
       "Level of the trickle. Measured 2.2-11.7 dB above the bed: quieter and closer to "
       "the water than the dabbling is."),
   LOG(kParamTrickleSize, "trickle_size", "Trickle Size", "Trickle", 0.705, 0.15, 3.0, "mm",
       "Radius of the tiny pocket a drop entrains, again as its pitch. Measured\n"
       "       0.42-1.72 mm, median 1.24, whose Minnaert pitch is 2629 Hz against a\n"
       "       measured median of 2625."),
   LIN(kParamTrickleSpread, "trickle_spread", "Trickle Spread", "Trickle", 0.0, 3.0, 1.2, "oct",
       "Spread of drop sizes. Measured pitch spans 2.1-5.0 kHz around the median, which "
       "is about 1.25 octaves."),
   LOG(kParamTrickleDecay, "trickle_decay", "Trickle Decay", "Trickle", 0.407, 2.0, 200.0, "ms",
       "How long one drop lasts. Measured 8-86 ms to fall 10 dB, median 13; the long "
       "end of that is splashing rather than ticking."),
   PCT(kParamTrickleImpact, "trickle_impact", "Impact", "Trickle", 0.45,
       "The strike itself against the pocket that follows it. A drop landing on rock is "
       "a broadband tick; a drop landing in water is mostly the bubble."),
   LOG(kParamStoneTone, "stone_tone", "Stone", "Trickle", 0.545, 800.0, 12000.0, "Hz",
       "What the drops are landing on: the centre of the impact's band. Wet rock is "
       "high and hard, gravel and moss are lower and duller."),
   PCT(kParamSplash, "splash", "Splash", "Trickle", 0.25,
       "How much of a drop is a short wash of water rather than a clean tick. The "
       "references' drop decays are bimodal -- 8-13 ms for ticks against 56-86 ms where "
       "the water is deeper -- and this is that second population."),

   // ---------------------------------------------------------------- plunge
   LIN(kParamPlungeLevel, "plunge_level", "Plunge Level", "Plunge", -80.0, 6.0, -42.0, "dB",
       "The pool under a fall. A cloud of bubbles has collective modes far below any "
       "single bubble in it, and that is what the weight under a waterfall is."),
   LOG(kParamPlungeTone, "plunge_tone", "Plunge Tone", "Plunge", 0.555, 40.0, 600.0, "Hz",
       "The cloud's lowest collective mode. Three are generated, at the 1.00 / 1.53 / "
       "1.90 ratios Xue et al. measure for a pour: one resonator is a tuned pipe where "
       "three are a body of water."),
   PCT(kParamPlungeDepth, "plunge_depth", "Plunge Depth", "Plunge", 0.4,
       "How much the pool surges. A plunge pool is never still."),
   PCT(kParamPlungeQ, "plunge_q", "Plunge Q", "Plunge", 0.5,
       "How resonant the pool is. A rock bowl rings; a gravel bed does not."),

   // ----------------------------------------------------------------- reach
   ENUM(kParamBankType, "bank_type", "Banks", "Reach", 1.0, kBankNames,
        "What the water runs between, which is how much early reflected field there "
        "is. An open river has nothing close enough to reflect off; a gorge and a "
        "culvert have a great deal."),
   PCT(kParamDistance, "distance", "Distance", "Reach", 0.3,
       "How far away the water is. Distance takes the top off it, as air absorption "
       "does, and thins the events out."),
   PCT(kParamAir, "air", "Air", "Reach", 0.5,
       "How much of the high end survives the distance. Cold air over water eats less "
       "of it than a warm afternoon does."),
   PCT(kParamWidth, "width", "Width", "Reach", 0.55,
       "Stereo spread of the events. Measured L/R correlation across the library is "
       "0.26-0.99, median 0.60."),
   PCT(kParamFlowWidth, "flow_width", "Flow Width", "Reach", 0.3,
       "Stereo spread of the bed, separately from the events. The bed's L/R correlation "
       "is 1 minus this, so the default of 0.40 is the library's measured median "
       "correlation of 0.60. A river heard from its bank is wide; one heard down a "
       "culvert is not."),
   PCT(kParamSpaceAmount, "space_amount", "Space Amount", "Reach", 0.22,
       "How much of the valley, the gorge or the culvert you hear."),
   PCT(kParamSpaceSize, "space_size", "Space Size", "Reach", 0.5,
       "Size of that space, in metres of it."),
   PCT(kParamSpaceDamping, "space_damping", "Space Damping", "Reach", 0.5,
       "How absorbent its surfaces are: bare rock rings, a wooded bank does not."),

   // ---------------------------------------------------------------- filter
   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Shape of the output filter."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.2385, 20.0, 2000.0, "Hz",
       "Permanent highpass. The default of 60 Hz is measured, not cautious: four\n"
       "       references carry up to a quarter of their energy below 60 Hz, and in\n"
       "       every one of them that energy's envelope is uncorrelated with the water\n"
       "       above it (|r| < 0.15). It is wind and handling noise on the microphone,\n"
       "       not river, so the engine does not synthesise it."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 30.0, 20000.0, "Hz",
       "Corner of the output filter."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Resonance of the output filter."),
   PCT(kParamFilterKeyTrack, "filter_keytrack", "Filter Key Track", "Filter", 0.0,
       "How far the filter follows the played note."),

   // -------------------------------------------------------------- envelope
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.675, 1.0, 20000.0, "ms",
       "Fade-in of the whole scene when a note starts."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.555, 5.0, 20000.0, "ms",
       "Fall from the initial level to the sustain."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level held while the note is down."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.656, 5.0, 30000.0, "ms",
       "Fade-out after the note is let go."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity To Level", "Envelope", 0.4,
       "How much velocity sets the level."),
   PCT(kParamVelToFlow, "vel_to_flow", "Velocity To Flow", "Envelope", 0.3,
       "How much velocity sets how much water there is: the event rates and the bed's "
       "brightness together, which is what more water actually does."),

   // ---------------------------------------------------------------- output
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Output level."),
   STEP(kParamMaxEvents, "max_events", "Max Events", "Output", 16.0, 2048.0, 512.0, "",
        "Ceiling on pockets and drops alive at once, so the cost is bounded."),
   STEP(kParamSeed, "seed", "Random Seed", "Output", 0.0, 999.0, 0.0, "",
        "Non-zero gives the same river every time. Zero is always different."),
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

} // namespace riverflow
