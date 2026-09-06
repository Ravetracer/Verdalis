#include "params.h"

#include "verdalis/param_macros.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace shorebreak {

namespace {

const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
const char *const kShoreNames[] = {"Sand", "Shingle", "Pebbles", "Rock", "Reef", "Harbour"};
const char *const kBreakerNames[] = {"Spilling", "Plunging", "Collapsing", "Surging"};

// Defaults are the medians measured across the reference library; see
// tools/analysis/README.md for the numbers and how they were taken.
const ParamDesc kParams[kNumParams] = {
   // ------------------------------------------------------------------ surf
   LOG(kParamWavePeriod, "wave_period", "Wave Period", "Surf", 0.33, 1000.0, 12000.0, "ms",
       "Seconds between breaking waves. Measured 2.6-4.2 s on open coast."),
   PCT(kParamSetVariation, "set_variation", "Set Variation", "Surf", 0.35,
       "How unevenly the waves arrive. At 0 they are metronomic; high values group them into sets."),
   PCT(kParamWaveSize, "wave_size", "Wave Size", "Surf", 0.55,
       "How big each break is: its level, its body and how far its tone reaches down."),
   PCT(kParamSizeVariation, "size_variation", "Size Variation", "Surf", 0.45,
       "Spread of size from wave to wave. Real sets are never even."),
   LOG(kParamBreakAttack, "break_attack", "Break Attack", "Surf", 0.35, 30.0, 2500.0, "ms",
       "How long the crest takes to build. A wave rises, it does not strike."),
   LOG(kParamBreakDecay, "break_decay", "Break Decay", "Surf", 0.4, 100.0, 4000.0, "ms",
       "How long the break itself lasts once the crest has collapsed."),
   LOG(kParamBreakTone, "break_tone", "Break Tone", "Surf", 0.5, 150.0, 2500.0, "Hz",
       "Centre of the bubble cloud a breaking wave makes. Measured 400-1600 Hz."),
   PCT(kParamBreakBody, "break_body", "Break Body", "Surf", 0.4,
       "Low weight under the break: the surf rumble you feel more than hear."),
   PCT(kParamCrestSweep, "crest_sweep", "Crest Open", "Surf", 0.2,
       "How much darker a wave is coming in than it is breaking. An approaching wave is "
       "the deep sound of moving water; the top of the spectrum arrives with the "
       "collapse, like a filter opening -- though never all the way."),
   ENUM(kParamBreakerType, "breaker_type", "Breaker", "Surf", 1.0, kBreakerNames,
        "How the crest collapses. Sets the slope above 1.5 kHz: -10 dB/oct for a "
        "plunger, -8.3 for a spiller."),
   PCT(kParamPrecursor, "precursor", "Precursor", "Surf", 0.3,
       "The crest of an incipient breaker already bubbles before it collapses. This is "
       "how much of that you hear."),
   PCT(kParamBubbleMix, "bubble_mix", "Bubble Mix", "Surf", 0.15,
       "How much of the break is separately audible bubbles rather than moving water. "
       "Bubbles are a detail: a break is mostly water, with them heard under it and "
       "just after it. Zero is legitimate -- distant surf has none."),

   // ------------------------------------------------------------------ foam
   LIN(kParamFoamLevel, "foam_level", "Foam Level", "Foam", -60.0, 6.0, -8.0, "dB",
       "Level of the foam a break leaves. It outlives the crest by up to three times."),
   LOG(kParamFoamDecay, "foam_decay", "Foam Decay", "Foam", 0.49, 200.0, 12000.0, "ms",
       "How long the foam hisses on. This is what fills the gap between waves."),
   LOG(kParamFoamTone, "foam_tone", "Foam Tone", "Foam", 0.5, 400.0, 8000.0, "Hz",
       "Where the foam starts. Foam has no low end at all: measured -82 dB at 50 Hz."),
   LOG(kParamFoamDelay, "foam_delay", "Foam Delay", "Foam", 0.79, 1.0, 2500.0, "ms",
       "How long after the break the foam arrives. Zero on a shore break, later off a bar."),
   PCT(kParamFizz, "fizz", "Fizz", "Foam", 0.3,
       "Fineness of the foam. Foam bubbles are smaller than the ones a break makes, so "
       "they ring higher: this is how much higher."),
   PCT(kParamFoamBubbles, "foam_bubbles", "Foam Bubbles", "Foam", 0.3,
       "How much of the foam is separately audible bubbles rather than a bed of hiss. "
       "This is where they belong: the fizzle after a break measures 29 onsets a second "
       "at 2.2 kHz against the break's own 9."),

   // ----------------------------------------------------------------- swell
   LIN(kParamSwellLevel, "swell_level", "Swell Level", "Swell", -80.0, 6.0, -18.0, "dB",
       "The continuous bed under everything: water moving without breaking."),
   LOG(kParamSwellTone, "swell_tone", "Swell Tone", "Swell", 0.45, 80.0, 3000.0, "Hz",
       "Centre of the swell bed. Low and dark for distant surf, higher up close."),
   PCT(kParamSwellDepth, "swell_depth", "Swell Depth", "Swell", 0.45,
       "How much the bed breathes with the swell. Measured 0.10 (steady roar) to 1.86."),
   LOG(kParamSwellRate, "swell_rate", "Swell Rate", "Swell", 0.4, 1.0, 30.0, "/min",
       "How often the bed rises and falls, independently of the breaking waves."),
   PCT(kParamSwellWidth, "swell_width", "Swell Width", "Swell", 0.9,
       "Stereo spread of the bed. The open sea is wide and barely correlated."),

   // --------------------------------------------------------------- bubbles
   LOG(kParamBubbleRate, "bubble_rate", "Bubble Rate", "Bubbles", 0.5, 2.0, 600.0, "/s",
       "Bubbles formed per second at the peak of a break. Keep it near the\n       references' 15-30: any faster and they overlap into noise instead of\n       being separately audible, which is the difference between a cascade and\n       a whoosh."),
   LOG(kParamBubblePitch, "bubble_pitch", "Bubble Pitch", "Bubbles", 0.45, 200.0, 5000.0, "Hz",
       "Centre pitch of a bubble, which is really its radius: small bubbles ring high."),
   LIN(kParamBubbleSpread, "bubble_spread", "Bubble Spread", "Bubbles", 0.0, 5.0, 2.2, "oct",
       "Spread of bubble sizes. A foam sheet holds every size at once."),
   LIN(kParamBubbleDecay, "bubble_damping", "Bubble Damping", "Bubbles", 0.25, 4.0, 1.0, "x",
       "Multiplies the damping a bubble actually has. At 1 it is physical: radiative\n       plus thermal loss after Xue et al., which gives Q 20-46 and ring times of\n       2-124 ms across the size range. Below 1 rings longer, above 1 is deader."),

   // ------------------------------------------------------------------ wash
   LIN(kParamWashLevel, "wash_level", "Wash Level", "Wash", -80.0, 6.0, -14.0, "dB",
       "The sheet of water running up the shore and back down it."),
   LOG(kParamWashDecay, "wash_decay", "Wash Decay", "Wash", 0.45, 200.0, 8000.0, "ms",
       "How long the wash takes to run out. Long on a flat beach, short against rock."),
   LOG(kParamWashTone, "wash_tone", "Wash Tone", "Wash", 0.5, 200.0, 6000.0, "Hz",
       "Brightness of the wash, which is really how coarse the shore is."),
   PCT(kParamSand, "sand", "Sand", "Wash", 0.5,
       "Grain of the shore under the wash: a smooth sheet, or water draining through shingle."),

   // ----------------------------------------------------------------- shore
   ENUM(kParamShoreType, "shore_type", "Shore", "Shore", 0.0, kShoreNames,
        "What the water runs over. Biases the foam, the wash and the whole brightness."),
   PCT(kParamDistance, "distance", "Distance", "Shore", 0.3,
       "How far away the surf is. Distance takes the top off it, as air absorption does."),
   PCT(kParamAir, "air", "Air", "Shore", 0.5,
       "How much of the high end survives the distance. Damp sea air eats it faster."),
   PCT(kParamWidth, "width", "Width", "Shore", 0.85,
       "Stereo spread of the breaks. Measured L/R correlation on open coast is 0.26-0.50."),
   PCT(kParamSpaceAmount, "space_amount", "Space Amount", "Shore", 0.18,
       "How much of the bay, the cove or the harbour wall you hear."),
   PCT(kParamSpaceSize, "space_size", "Space Size", "Shore", 0.55,
       "Size of that space, in metres of it."),
   PCT(kParamSpaceDamping, "space_damping", "Space Damping", "Shore", 0.5,
       "How absorbent its surfaces are: bare rock rings, a sandy cove does not."),

   // ---------------------------------------------------------------- filter
   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Shape of the output filter."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.0, 20.0, 2000.0, "Hz",
       "Permanent highpass, for keeping the surf rumble out of a small speaker."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 30.0, 20000.0, "Hz",
       "Corner of the output filter."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Resonance of the output filter."),
   PCT(kParamFilterKeyTrack, "filter_keytrack", "Filter Key Track", "Filter", 0.0,
       "How far the filter follows the played note."),

   // -------------------------------------------------------------- envelope
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.738, 1.0, 20000.0, "ms",
       "Fade-in of the whole scene when a note starts."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.4, 5.0, 20000.0, "ms",
       "Fall from the initial level to the sustain."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level held while the note is down."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.775, 5.0, 30000.0, "ms",
       "Fade-out after the note is let go. The sea does not stop at once."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity To Level", "Envelope", 0.5,
       "How much velocity sets the level."),
   PCT(kParamVelToSize, "vel_to_size", "Velocity To Size", "Envelope", 0.3,
       "How much velocity sets the size of the waves."),

   // ---------------------------------------------------------------- output
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Output level."),
   STEP(kParamMaxWaves, "max_waves", "Max Waves", "Output", 8.0, 512.0, 128.0, "",
        "Ceiling on breaks, foam patches and bubbles alive at once, so the cost is bounded."),
   STEP(kParamSeed, "seed", "Random Seed", "Output", 0.0, 999.0, 0.0, "",
        "Non-zero gives the same sea every time. Zero is always different."),
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

} // namespace shorebreak
