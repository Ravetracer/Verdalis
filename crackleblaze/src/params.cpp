#include "params.h"

#include "verdalis/param_macros.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace crackleblaze {

namespace {

const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
const char *const kFireNames[] = {"Deep Blaze", "Log Fire", "Camp Fire", "Open Flame",
                                  "Stove Draught"};
const char *const kHearthNames[] = {"Open",      "Fire Ring", "Stone Hearth",
                                    "Fireplace", "Stove",     "Cavern"};

// Every default below is a measurement, not a preference. The numbers and how
// they were taken are in tools/analysis/README.md; the short version is 25
// field recordings of fires, of which seventeen proved usable, measured for
// the octave-band colour of the bed with the crackles gated out of it, for the
// rate, amplitude law, decay and spectrum of the crackles, and for the three
// timescales on which those crackles arrive.
const ParamDesc kParams[kNumParams] = {
   // ----------------------------------------------------------------- blaze
   ENUM(kParamFireType, "fire_type", "Fire", "Blaze", 1.0, kFireNames,
        "Which measured colour the roar has. The five choices are the five clusters\n"
        "       the reference library's *beds* fall into, each shipped as its cluster's\n"
        "       measured octave-band centroid -- from Deep Blaze, which peaks at 125 Hz\n"
        "       and falls 15 dB by 8 kHz, to Stove Draught, which is 22 dB down at 1 kHz\n"
        "       and peaks at the very top.\n"
        "\n"
        "       Gating the crackles out before clustering is what makes these five\n"
        "       shapes mean anything. The library's median crest factor is 31.7 dB, so\n"
        "       the ungated spectrum of a fire is largely the spectrum of its crackles,\n"
        "       and clustering on that sorts recordings by how close the microphone was\n"
        "       rather than by what kind of fire it was."),
   PCT(kParamFireBlend, "fire_blend", "Blend", "Blaze", 0.0,
       "Crossfades from the chosen colour towards the next one, so the five measured "
       "shapes are a continuum rather than five settings. At 0 the roar is exactly the "
       "measured centroid."),
   LIN(kParamRoarLevel, "roar_level", "Roar Level", "Blaze", -60.0, 6.0, -31.0, "dB",
       "Level of the combustion roar, and it reads as exactly that: the roar is\n"
       "       normalised to unit RMS before this is applied, whichever colour is\n"
       "       selected, so -26 dB is a roar at -26 dBFS and changing colour is not\n"
       "       also a level change.\n"
       "\n"
       "       A fire is much less bed than a river is. The library's crest factor is\n"
       "       31.7 dB against a river's 19.6 -- so where a river is mostly its bed\n"
       "       with events on top, a fire is a quiet roar with very loud, very short\n"
       "       things happening over it."),
   BIPCT(kParamRoarTilt, "roar_tilt", "Tilt", "Blaze", 0.0,
         "Tilts the whole measured shape brighter or darker, +/-6 dB/octave about 1 kHz. "
         "Moves a colour off its measured centre without discarding its shape."),
   PCT(kParamRoarBody, "roar_body", "Body", "Blaze", 0.5,
       "Weight of the roar's lowest two bands, 125 and 250 Hz. This is where a fire's\n"
       "       size lives: the library's bed measures a median of -4.0 dB at 125 Hz\n"
       "       against -14.9 dB at 2 kHz, so the roar is a low-frequency phenomenon\n"
       "       whatever the crackles on top of it are doing."),
   PCT(kParamFlare, "flare", "Flare", "Blaze", 0.55,
       "How much the fire surges -- and it surges as *one fire*, which is the measured\n"
       "       difference between this plugin's bed and RiverFlow's. The correlation\n"
       "       between one octave band's 100 ms envelope and its neighbour's is 0.54 to\n"
       "       0.91, median 0.85, where a river measures 0.07 to 0.25. So a single\n"
       "       common walk drives every band, with only a small independent part on\n"
       "       each; a river needs eight separate ones.\n"
       "\n"
       "       Fitted to the bed's low bands, where the measured CV is 0.39 at 125 Hz\n"
       "       and 0.45 at 250. The upper-band figures run to 1.28, but those still\n"
       "       carry crackle and sizzle leakage that no gate removes cleanly, so they\n"
       "       are not what this was set from.\n"
       "\n"
       "       It moves the crackle rate as well as the roar's level, because in a real\n"
       "       fire a flare-up is both -- see Crackle Rate."),
   LOG(kParamFlareRate, "flare_rate", "Flare Rate", "Blaze", 0.5207, 0.05, 5.0, "Hz",
       "How fast that surging is. It is a filtered random walk rather than an\n"
       "       oscillator: the envelope spectrum of every reference falls smoothly at\n"
       "       about -5.7 dB per decade over 0.1-10 Hz, and the strongest frequency in\n"
       "       it ranges from 0.12 to 2.54 Hz with no peak that survives from one\n"
       "       recording to the next. A fire has no rhythm. This sets the corner of the\n"
       "       noise that drives it."),
   PCT(kParamDraught, "draught", "Draught", "Blaze", 0.15,
       "Air being pulled into the fire: a high-frequency hiss added over the roar, "
       "steadier than the roar itself. It is what separates a stove with its lid open "
       "from the same fire in a room, and Stove Draught is the shape fitted to that "
       "reference."),

   // --------------------------------------------------------------- crackle
   LOG(kParamCrackleRate, "crackle_rate", "Crackle Rate", "Crackle", 0.7409, 0.5, 120.0, "/s",
       "Crackles a second, averaged. Measured 11.5 to 51.0 across the library, median\n"
       "       29.\n"
       "\n"
       "       Averaged, because the rate does not hold still. The variance of the\n"
       "       count in a one-second window is 3.9 times its mean across the library and\n"
       "       up to 20 times in places, where a Poisson process would give exactly 1.\n"
       "       That whole excess is the rate itself wandering on a seconds-long\n"
       "       timescale -- the same walk that drives Flare -- and not the crackles\n"
       "       clustering: at 50 ms the arrivals are Poisson to within 3%, and the\n"
       "       branching ratio is 0.02, which rules out one crackle provoking the next."),
   LIN(kParamCrackleLevel, "crackle_level", "Crackle Level", "Crackle", -60.0, 6.0, -16.0, "dB",
       "Level of the crackles. Measured as prominence over the bed they sit on: 6.5 to "
       "20.2 dB, median 13.2."),
   LOG(kParamCrackleDecay, "crackle_decay", "Decay", "Crackle", 0.3662, 0.3, 40.0, "ms",
       "How long one crackle takes to fall 10 dB. Measured median 2.5 ms, from 1.0 to "
       "5.5 -- these are clicks, and at the short end of the range a crackle is over "
       "before the ear can hear a pitch in it."),
   LOG(kParamCrackleTone, "crackle_tone", "Tone", "Crackle", 0.6770, 400.0, 12000.0, "Hz",
       "Where the crackle's energy sits. The measured spectrum, with the roar\n"
       "       underneath subtracted, falls about 2 dB per octave from 250 Hz and then\n"
       "       drops away above 8 kHz; the crackles add +5.9 dB to the library's median\n"
       "       spectrum at 4 kHz, +4.0 at 2 kHz and +3.8 at 8 kHz. This moves that whole\n"
       "       shape."),
   LIN(kParamCrackleSpread, "crackle_spread", "Spread", "Crackle", 0.0, 18.0, 6.0, "dB",
       "How unequal the crackles are. Their amplitudes are log-normal with a measured\n"
       "       standard deviation of 0.30 decades, which is 6 dB -- so the loudest\n"
       "       crackle in a few seconds is some 15 dB over the quietest, and a fire with\n"
       "       this at zero sounds mechanical immediately."),
   LIN(kParamBurst, "burst", "Burst", "Crackle", 1.0, 4.0, 2.0, "",
       "Pulses per crackle, averaged. A crackle is not one impulse: below 10 ms the\n"
       "       references carry 1.97 times as many intervals as a Poisson process\n"
       "       allows, and up to 4 times in places, while at 50 ms they carry exactly\n"
       "       as many. Something happens two or three times inside ten milliseconds\n"
       "       and then does not happen again -- a pocket of gas breaking out of the\n"
       "       wood in stages rather than at once."),
   PCT(kParamSnap, "snap", "Snap", "Crackle", 0.6,
       "How sharp the front of a crackle is. At the top it is an impulse with the "
       "decay hung off it; lower, the energy is spread into the first milliseconds and "
       "the crackle reads as a thud rather than a tick."),
   PCT(kParamCrackleBody, "crackle_body", "Body", "Crackle", 0.30,
       "How much low end each crackle carries, below its Tone. The measured crackle "
       "spectrum does not stop at the bottom -- it is still within 8 dB of its peak at "
       "250 Hz -- and without this a crackle is all fizz and no wood."),

   // ---------------------------------------------------------------- sizzle
   PCT(kParamSap, "sap", "Sap", "Sizzle", 0.21,
       "What fraction of the events are sizzles rather than crackles -- wet wood\n"
       "       rather than dry. This is the widest per-recording variable in the whole\n"
       "       library: measured from 0.03 in the big open blaze to 0.57 in\n"
       "       `fire_near_open_close`, median 0.21.\n"
       "\n"
       "       The two populations are the same event held open for different lengths\n"
       "       of time. Their measured spectra agree to within 2 dB per octave; what\n"
       "       differs is that a sizzle takes 19 ms to fall 10 dB where a crackle takes\n"
       "       2.5. Steam has to leave the wood, and that takes time a bursting gas\n"
       "       pocket does not need."),
   LIN(kParamSizzleLevel, "sizzle_level", "Sizzle Level", "Sizzle", -60.0, 6.0, -23.0, "dB",
       "Level of the sizzles."),
   LOG(kParamSizzleDecay, "sizzle_decay", "Decay", "Sizzle", 0.3619, 5.0, 200.0, "ms",
       "How long a sizzle takes to fall 10 dB. Measured median 19 ms, from 11 to 30 -- "
       "seven to eight times a crackle's."),
   LOG(kParamSizzleTone, "sizzle_tone", "Tone", "Sizzle", 0.7116, 400.0, 12000.0, "Hz",
       "Where a sizzle's energy sits. Defaults slightly above Crackle Tone because the "
       "measured sizzle spectrum is a little flatter across the upper middle, but the "
       "two are close on purpose: the measurement says these events differ in duration, "
       "not in colour."),
   PCT(kParamSteam, "steam", "Steam", "Sizzle", 0.25,
       "How much of a sizzle is a narrow jet rather than broadband noise. Steam leaving "
       "a split in the wood whistles; this decides how much of that is in the sound."),

   // ---------------------------------------------------------------- settle
   LOG(kParamSettleRate, "settle_rate", "Settle Rate", "Settle", 0.5, 0.005, 2.0, "/s",
       "How often a log gives way. Rare on purpose: measured a median of six a minute\n"
       "       across the library, and zero in two of the seventeen recordings. Finding\n"
       "       them at all needs an onset detector run on 80-300 Hz with anything that\n"
       "       also jumps in the top thrown away, because a loud crackle leaks into\n"
       "       every band and would otherwise be counted twice."),
   LIN(kParamSettleLevel, "settle_level", "Settle Level", "Settle", -60.0, 6.0, -25.0, "dB",
       "Level of the settles. Measured 13.9 to 25.7 dB over the low bed, median 16.1."),
   LOG(kParamSettleTone, "settle_tone", "Tone", "Settle", 0.5740, 40.0, 400.0, "Hz",
       "Where the thump sits. The population was measured in the 80-300 Hz band and "
       "has no top end at all -- that is what distinguishes it from a loud crackle."),
   LOG(kParamSettleDecay, "settle_decay", "Decay", "Settle", 0.2130, 3.0, 300.0, "ms",
       "How long a settle takes to fall 10 dB. Measured 6 to 10 ms -- a thump, not a "
       "boom. Longer than that and it stops being wood and starts being a drum."),

   // ---------------------------------------------------------------- hearth
   ENUM(kParamHearthType, "hearth_type", "Hearth", "Hearth", 3.0, kHearthNames,
        "What the fire is burning in, which sets how much early reflected field there\n"
        "       is. Open has almost none -- a fire in a field has nothing close enough\n"
        "       to reflect off, and eight discrete early reflections outdoors is exactly\n"
        "       what makes a reverb sound like a bathroom. A fireplace and a stove have\n"
        "       a great deal, and the two references recorded inside one measure as the\n"
        "       most reverberant in the library."),
   PCT(kParamDistance, "distance", "Distance", "Hearth", 0.15,
       "How far away the fire is. Air absorption plus a downward tilt, with the events "
       "multiplied and their edges smeared -- and bypassed outright at zero, because no "
       "distance means no air to absorb."),
   PCT(kParamAir, "air", "Air", "Hearth", 0.5,
       "How much of the top end that distance costs. Humid air absorbs less than dry."),
   PCT(kParamWidth, "width", "Width", "Hearth", 0.7,
       "How widely the crackles, sizzles and settles are spread across the stereo "
       "field. The library measures an L/R correlation from -0.52 to 1.00, median 0.69."),
   PCT(kParamRoarWidth, "roar_width", "Roar Width", "Hearth", 0.45,
       "The same for the roar, separately. A fire's roar is one turbulent source and "
       "decorrelates less than the discrete events around it."),
   PCT(kParamSpaceAmount, "space_amount", "Space", "Hearth", 0.25,
       "How much reverberant field there is beyond the early reflections."),
   PCT(kParamSpaceSize, "space_size", "Size", "Hearth", 0.4, "How big that space is."),
   PCT(kParamSpaceDamping, "space_damping", "Damping", "Hearth", 0.6,
       "How quickly the space eats the top end. Brick and stone eat a great deal."),

   // ---------------------------------------------------------------- filter
   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Shape of the output filter."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.2385, 20.0, 2000.0, "Hz",
       "Permanent highpass. The default of 60 Hz is measured, not cautious: eight of\n"
       "       the twenty-five references carry between 27 and 60 per cent of their\n"
       "       energy below 60 Hz, and in twenty-four of the twenty-five that energy's\n"
       "       envelope is uncorrelated with the fire above it (|r| < 0.2). It is\n"
       "       traffic, ventilation and handling noise on the microphone, not fire, so\n"
       "       the engine does not synthesise it."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 30.0, 20000.0, "Hz",
       "Corner of the output filter."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Resonance of the output filter."),
   PCT(kParamFilterKeyTrack, "filter_keytrack", "Filter Key Track", "Filter", 0.0,
       "How far the filter follows the played note."),

   // -------------------------------------------------------------- envelope
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.675, 1.0, 20000.0, "ms",
       "Fade-in of the whole fire when a note starts."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.555, 5.0, 20000.0, "ms",
       "Fall from the initial level to the sustain."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level held while the note is down."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.656, 5.0, 30000.0, "ms",
       "Fade-out after the note is let go."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity To Level", "Envelope", 0.4,
       "How much velocity sets the level."),
   PCT(kParamVelToFire, "vel_to_fire", "Velocity To Fire", "Envelope", 0.3,
       "How much velocity sets how much fire there is: the event rates and the roar's "
       "brightness together, which is what a bigger fire actually does."),

   // ---------------------------------------------------------------- output
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, -3.0, "dB",
       "Output level. The default leaves 3 dB of headroom, which a fire needs more\n"
       "       than a river does: its amplitudes are log-normal with a measured 6 dB\n"
       "       spread, so the loudest crackle in a minute stands far above the one the\n"
       "       meter usually shows."),
   STEP(kParamMaxEvents, "max_events", "Max Events", "Output", 16.0, 2048.0, 1024.0, "",
        "Ceiling on crackles, sizzles and settles alive at once, so the cost is "
        "bounded. Higher here than in the suite's other plugins because a fire spawns "
        "far more events a second than a river does."),
   STEP(kParamSeed, "seed", "Random Seed", "Output", 0.0, 999.0, 0.0, "",
        "Non-zero gives the same fire every time. Zero is always different."),
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

} // namespace crackleblaze
