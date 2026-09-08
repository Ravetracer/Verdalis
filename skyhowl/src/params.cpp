#include "params.h"

#include "verdalis/param_macros.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace skyhowl {

namespace {

const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
const char *const kObstacleNames[] = {"Open",  "Grass", "Reeds", "Twigs", "Branches",
                                      "Wires", "Rocks", "Gap",   "Cave"};
const char *const kFoliageNames[] = {"None",  "Broadleaf", "Dry Leaves",    "Conifer",
                                     "Grass", "Reeds",     "Bare Branches", "Bushes"};
const char *const kTerrainNames[] = {"Plain",  "Meadow", "Forest", "Mountain",
                                     "Desert", "Coast",  "Street", "Tundra"};

// Defaults are the medians measured across the reference library; see
// tools/analysis/README.md for the numbers and how they were taken.
const ParamDesc kParams[kNumParams] = {
   // ------------------------------------------------------------------ wind
   LOG(kParamWindSpeed, "wind_speed", "Wind Speed", "Wind", 0.6327, 0.5, 40.0, "m/s",
       "The mean wind speed, and the master control of the whole plugin: it sets the "
       "level, the brightness, the pitch of the howl and how fast the foliage "
       "rustles. 5 m/s is a breeze, 12 a strong wind, 25 a storm."),
   LIN(kParamTurbulence, "turbulence", "Turbulence", "Wind", 0.0, 50.0, 10.0, "%",
       "Turbulence intensity: the standard deviation of the wind speed over its mean, "
       "which is what a met station reports. Measured 4-27 % across the library, "
       "median 10 %."),
   LOG(kParamGustRate, "gust_rate", "Gust Rate", "Wind", 0.5018, 0.2, 60.0, "/min",
       "How often a coherent gust arrives. This is the turbulence length scale seen "
       "from the other end -- a gust every L/U seconds. Measured 0.6-33 a minute, "
       "median 3.5."),
   PCT(kParamGustDepth, "gust_depth", "Gust Depth", "Wind", 0.5,
       "How much faster a gust is than the mean. Sets the gust factor U_max/U_mean, "
       "measured 1.04-1.54 across the library."),
   LOG(kParamGustLength, "gust_length", "Gust Length", "Wind", 0.5625, 300.0, 30000.0, "ms",
       "How long a gust takes to pass. Short ones slap, long ones swell."),
   PCT(kParamGustShape, "gust_shape", "Gust Shape", "Wind", 0.5,
       "Whether a gust arrives faster than it leaves. At 50 % it is symmetric, which "
       "is what the references measure: the median rise/fall ratio is 1.01, so unlike "
       "a wave a gust is not a transient."),
   PCT(kParamSquall, "squall", "Squall", "Wind", 0.30,
       "A much slower drift on top of the gusting: sets of gusts rather than gusts. "
       "Not a detail -- a median 43 % of the envelope variance in the library sits "
       "below 0.1 Hz."),
   LOG(kParamSquallRate, "squall_rate", "Squall Rate", "Wind", 0.4376, 0.2, 12.0, "/min",
       "How often those sets come round. Minutes rather than seconds."),

   // --------------------------------------------------------------- airflow
   LIN(kParamFlowLevel, "flow_level", "Flow Level", "Airflow", -60.0, 6.0, -18.0, "dB",
       "Level of the broadband bed: the sound of turbulent air going past whatever is "
       "there. Most of what wind is."),
   LOG(kParamFlowTone, "flow_tone", "Flow Tone", "Airflow", 0.5049, 60.0, 4000.0, "Hz",
       "Where the bed's slope begins. Below this it is flat, above it falls at Flow "
       "Tilt."),
   LIN(kParamFlowTilt, "flow_tilt", "Flow Tilt", "Airflow", -24.0, 0.0, -8.7, "dB/oct",
       "The slope above Flow Tone. Kolmogorov's inertial subrange gives a velocity "
       "spectrum going as f^-5/3, which is -5 dB/octave; the library measures "
       "-22 to -2 with a median of -8.7, because radiation from a rigid surface "
       "steepens it further."),
   LIN(kParamBuffet, "buffet", "Buffet", "Airflow", -80.0, 6.0, -16.0, "dB",
       "The low-frequency pressure buffeting under everything: the thump of moving air "
       "rather than its hiss. Measured -58 to 0 dB relative to the whole, median -15.5."),
   LOG(kParamBuffetTone, "buffet_tone", "Buffet Tone", "Airflow", 0.4626, 20.0, 300.0, "Hz",
       "Where the buffeting sits. Low enough to be felt more than heard."),
   PCT(kParamHiss, "hiss", "Hiss", "Airflow", 0.3,
       "The top end: fine-scale turbulence, above where the tilt has taken everything "
       "else away. Part of the bed rather than a layer of its own, so it follows Flow "
       "Level and disappears with it."),
   PCT(kParamSpeedLaw, "speed_law", "Speed Law", "Airflow", 1.0,
       "How hard the level follows the wind speed. At 100 % it is the physical law: "
       "aerodynamic sound power from flow over a rigid surface goes as U^6 "
       "(Curle), so amplitude goes as U^3 and doubling the wind is +18 dB. Lower "
       "it to keep a gusty wind inside a mix."),

   // ------------------------------------------------------------------ howl
   PCT(kParamHowlAmount, "howl_amount", "Howl Amount", "Howl", 0.40,
       "How much of the sound is aeolian tone rather than broadband flow. Zero is "
       "legitimate: only 17 of the library's 63 recordings hold a tone steadily enough "
       "to count as one."),
   ENUM(kParamObstacle, "obstacle", "Obstacle", "Howl", 3.0, kObstacleNames,
        "What the wind is shedding off. Bluff bodies -- grass, twigs, wires -- track "
        "the wind speed; a gap or a cave is a cavity whose pitch its own geometry "
        "fixes, so it barely moves."),
   LOG(kParamHowlSize, "howl_size", "Howl Size", "Howl", 0.4002, 0.3, 60.0, "mm",
       "The obstacle's diameter, which is what its pitch actually is: Strouhal gives "
       "f = 0.2 U / d, so a 2.5 mm twig in an 8 m/s wind sheds at 640 Hz -- which is the "
       "median tone frequency across the library's tonal recordings. They imply "
       "diameters of 1.5 to 11.8 mm."),
   LIN(kParamHowlSpread, "howl_spread", "Howl Spread", "Howl", 0.0, 4.0, 0.4, "oct",
       "Spread of obstacle sizes either side of that. A single wire is one diameter; a "
       "hedge is every diameter at once."),
   STEP(kParamHowlVoices, "howl_voices", "Howl Voices", "Howl", 1.0, 12.0, 3.0, "",
        "How many obstacles are shedding. One is a whistle, a dozen is a chorus."),
   PCT(kParamHowlReso, "howl_reso", "Howl Resonance", "Howl", 0.68,
       "How narrow each tone is, as a Q from 0.8 to 12. Measured 1.1-16.5 with a median "
       "of 5.1: a howl is a narrow band of noise rather than a whistle, and taking this "
       "to the top of its range stops sounding like wind at all."),
   PCT(kParamHowlTrack, "howl_track", "Howl Track", "Howl", 0.8,
       "How much the pitch follows the wind speed. This is the one prediction of the "
       "model the references can falsify, and they confirm it: the tone's pitch rises "
       "with the level in 12 of the 17 tonal recordings, reaching 0.87 in the ones "
       "actually named howling. It swoops a median 0.72 of an octave as it does so."),
   PCT(kParamHowlThreshold, "howl_threshold", "Howl Onset", "Howl", 0.3,
       "The share of the mean wind speed below which nothing sheds. Above it the tone "
       "comes up steeply, which is why a howl arrives with the gust rather than fading in."),
   PCT(kParamWarble, "warble", "Warble", "Howl", 0.25,
       "How much each tone wanders around its own pitch. Vortex shedding is not a "
       "metronome: the local velocity drifts and the tone drifts with it. This is most "
       "of what makes a howl eerie rather than electronic."),

   // ---------------------------------------------------------------- rustle
   PCT(kParamRustleAmount, "rustle_amount", "Rustle Amount", "Rustle", 0.3,
       "How much foliage there is to hear. The bonus layer: wind in the open is one "
       "sound, wind in a tree is another."),
   ENUM(kParamFoliage, "foliage", "Foliage", "Rustle", 1.0, kFoliageNames,
        "What is rustling. Sets the pitch, the decay and how separately audible the "
        "individual leaves are."),
   LOG(kParamRustleSize, "rustle_size", "Leaf Size", "Rustle", 0.5139, 5.0, 300.0, "mm",
       "The size of one leaf, which sets what it radiates: roughly c/2L, so a 40 mm "
       "leaf clicks around 4.2 kHz. The library's rustle onsets measure a median "
       "centroid of 4.2 kHz over a 2.8-6.6 kHz range."),
   LOG(kParamRustleDensity, "rustle_density", "Rustle Density", "Rustle", 0.4216, 2.0, 800.0, "/s",
       "Leaves heard per second at full wind. Keep it near the references' 15-40: any "
       "faster and the clicks overlap into a wash instead of being separately "
       "audible, which is the difference between a rustle and a hiss."),
   LIN(kParamRustleSpread, "rustle_spread", "Rustle Spread", "Rustle", 0.0, 3.0, 1.0, "oct",
       "Spread of leaf sizes. No tree has one leaf size, and a mix of sizes is what "
       "stops a rustle sounding tuned."),
   LOG(kParamRustleDecay, "rustle_decay", "Rustle Decay", "Rustle", 0.4147, 2.0, 400.0, "ms",
       "How long one leaf rings. Short and dead for a soft summer leaf, longer and "
       "harder for a dry one."),
   PCT(kParamRustleThreshold, "rustle_threshold", "Rustle Onset", "Rustle", 0.25,
       "The share of the mean wind speed at which leaves start moving at all. At the "
       "default and an 8 m/s wind that is 2 m/s, which is about where real foliage "
       "starts."),
   PCT(kParamClatter, "clatter", "Clatter", "Rustle", 0.4,
       "How impulsive each leaf is: a soft brush at zero, a dry clatter at one. "
       "Measured as the variation of the onset flux, 0.20 for a merged hiss against "
       "0.80 for dry leaves."),

   // ----------------------------------------------------------------- place
   ENUM(kParamTerrain, "terrain", "Terrain", "Place", 0.0, kTerrainNames,
        "What the wind is crossing. Roughness is what makes turbulence in the first "
        "place, so this biases the gusting as well as the spectrum and the space."),
   PCT(kParamDistance, "distance", "Distance", "Place", 0.2,
       "How far away the wind is. Distance takes the top off it: the library's most "
       "distant recording is 117 dB down at 16 kHz and peaks at 63 Hz."),
   PCT(kParamAir, "air", "Air", "Place", 0.5,
       "How much of the high end survives that distance. Cold dry air keeps more of it."),
   PCT(kParamWidth, "width", "Width", "Place", 0.75,
       "Stereo spread. Measured L/R correlation across the library is 0.49 in the "
       "median, so wind is wide but not decorrelated."),
   PCT(kParamSpaceAmount, "space_amount", "Space Amount", "Place", 0.2,
       "How much of the surroundings you hear: a street, a valley, a cave mouth."),
   PCT(kParamSpaceSize, "space_size", "Space Size", "Place", 0.6,
       "Size of that space, in metres of it."),
   PCT(kParamSpaceDamping, "space_damping", "Space Damping", "Place", 0.5,
       "How absorbent its surfaces are: bare rock rings, a forest does not."),

   // ---------------------------------------------------------------- filter
   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Shape of the output filter."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.0, 20.0, 2000.0, "Hz",
       "Permanent highpass, for keeping the buffeting out of a small speaker."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 30.0, 20000.0, "Hz",
       "Corner of the output filter."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Resonance of the output filter."),
   PCT(kParamFilterKeyTrack, "filter_keytrack", "Filter Key Track", "Filter", 0.0,
       "How far the filter follows the played note."),

   // -------------------------------------------------------------- envelope
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.79, 1.0, 20000.0, "ms",
       "How long the wind takes to get up when a note starts."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.6119, 5.0, 20000.0, "ms",
       "Fall from the initial level to the sustain."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level held while the note is down."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.7684, 5.0, 30000.0, "ms",
       "How long it takes to drop after the note is let go. Wind does not stop at once."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity To Level", "Envelope", 0.5,
       "How much velocity sets the level."),
   PCT(kParamVelToSpeed, "vel_to_speed", "Velocity To Speed", "Envelope", 0.4,
       "How much velocity sets the wind speed itself, which is not the same thing: a "
       "faster wind is brighter and higher as well as louder."),

   // ---------------------------------------------------------------- output
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Output level."),
   STEP(kParamMaxGusts, "max_gusts", "Max Gusts", "Output", 4.0, 256.0, 64.0, "",
        "Ceiling on gusts and rustling leaves alive at once, so the cost is bounded."),
   STEP(kParamSeed, "seed", "Random Seed", "Output", 0.0, 999.0, 0.0, "",
        "Non-zero gives the same wind every time. Zero is always different."),
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

} // namespace skyhowl
