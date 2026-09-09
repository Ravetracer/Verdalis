#include "params.h"

#include "verdalis/param_macros.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace chirpparade {

namespace {

const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
const char *const kSpeciesNames[] = {"Whistler", "Sparrow",  "Warbler", "Budgie", "Woodpecker",
                                     "Crane",    "Goose",    "Crow",    "Raven",  "Screech"};

// Defaults are the medians measured across the reference library: 4287
// syllables in 58 recordings. See tools/analysis/README.md for every number
// and how it was taken.
const ParamDesc kParams[kNumParams] = {
   // -------------------------------------------------------------- syllable
   LOG(kParamPitch, "pitch", "Pitch", "Syllable", 0.6953, 200.0, 8000.0, "Hz",
       "Where the syllable sits. The fundamental across the library runs 292 Hz to "
       "5.9 kHz with a median of 2580, which is the default: corvids at the bottom, "
       "a robin's whistle at the top. The contour is measured about its own centre, "
       "so this transposes it -- and because the default is the library median, every "
       "species at the default sings in its own register."),
   LIN(kParamSweep, "sweep", "Sweep", "Syllable", 0.0, 300.0, 100.0, "%",
       "Scales how far the contour travels. 100 % is the measured curve exactly; "
       "below that it flattens towards a held note, above it exaggerates. A "
       "multiplier rather than a width, because the contour already carries the real "
       "excursion -- the archetypes travel 0.16 to 1.16 octaves of span and up to 3.2 "
       "octaves of path. It goes past 100 % on purpose: the range is wanted, and a "
       "percentage that stops at 100 cannot reach it."),
   PCT(kParamContour, "contour", "Contour", "Syllable", 0.5,
       "Which syllable. Each species carries up to eight contours measured off real "
       "recordings -- the medoids of its clustered syllables -- and this walks across "
       "them, lowest-sitting first. It is the most important control in the plugin: a "
       "syllable's identity is its frequency contour, and these are measured curves "
       "rather than a shape derived from summary statistics."),
   PCT(kParamDetail, "detail", "Detail", "Syllable", 1.0,
       "How much of the contour's fine motion survives. A real syllable changes "
       "direction 2 to 40 times and slews at up to 440 octaves a second, and that "
       "scribble is most of what makes it a bird rather than a whistle. At 100 % the "
       "measured curve passes through; lower it and the contour smooths towards the "
       "glide that a summary statistic would have given -- which is exactly what the "
       "first version of this plugin could only do, and why it did not work."),
   LOG(kParamLength, "length", "Length", "Syllable", 0.4668, 15.0, 800.0, "ms",
       "How long one syllable lasts. Measured 27 to 481 ms, median 96."),
   PCT(kParamSkew, "skew", "Skew", "Syllable", 0.5,
       "Bends the syllable's own time axis: below a half it crowds the contour "
       "towards the start, above it towards the end. 0.5 plays the measured curve at "
       "its measured pace, and the asymmetry the library shows -- a 24 ms rise against "
       "a 41 ms fall -- is already in it."),
   PCT(kParamJitter, "jitter", "Jitter", "Syllable", 0.12,
       "How much the pitch wanders off its own contour. A syrinx is not a "
       "synthesiser: without this every syllable of a phrase is identical, which no "
       "recording in the library is."),
   LOG(kParamPulseRate, "pulse_rate", "Pulse Rate", "Syllable", 0.3294, 4.0, 150.0, "Hz",
       "Rate of the amplitude pulsing inside a syllable -- the flutter of a trill "
       "held on one breath. Measured 8.5 to 66 Hz, median 13.2."),
   PCT(kParamPulseDepth, "pulse_depth", "Pulse Depth", "Syllable", 0.0,
       "How deep that pulsing is, added on top of whatever the measured level "
       "contour already does. Defaults to nothing: the contours carry the real "
       "amplitude modulation, so this is for pushing further rather than for "
       "supplying what is missing."),

   // ---------------------------------------------------------------- timbre
   ENUM(kParamSpecies, "species", "Species", "Timbre", 1.0, kSpeciesNames,
        "What kind of bird. Each entry biases pitch, sweep, length, harmonic "
        "richness, roughness and syllable rate together, because in the references "
        "they move together. Every one is the median of the recordings of that bird; "
        "only Screech is invented."),
   PCT(kParamVoice, "voice", "Voice", "Timbre", 0.30,
       "How much of each cycle the syrinx is shut. Air passes only while the labia "
       "are apart, so this is not a filter: at the bottom the valve never closes and "
       "a pure sine comes out, which is what 59 % of the library's syllables are, and "
       "closing it makes the airflow a one-sided pulse with the harmonic stack a crow "
       "has -- evens as well as odds, which a symmetric oscillator cannot produce at "
       "all. 16 % of the library has six harmonics or more, so both ends are real."),
   PCT(kParamBreath, "breath", "Breath", "Timbre", 0.05,
       "Turbulent air past the labia. It is also what starts the oscillation: the "
       "noise is injected into the oscillator rather than added to its output, which "
       "is why a syllable's onset is never twice the same. Calibrated against the "
       "library: rendering a whistle at a sweep of this and measuring its spectral "
       "flatness gives 13 dB per decade, and 5 % is where a sparrow lands on the "
       "library's median roughness of -28 dB. The chosen species scales it, from "
       "0.42 for the cleanest to 2.8 for the roughest."),
   LOG(kParamTract, "tract", "Tract Length", "Timbre", 0.5001, 1.0, 24.0, "cm",
       "Length of the trachea above the syrinx, which resonates at c/4L like any "
       "closed tube. In the library's harmonic syllables the loudest harmonic is not "
       "the first in 81 % of cases, and it sits at a median 1741 Hz -- a 4.9 cm "
       "trachea, which is the default."),
   PCT(kParamBeak, "beak", "Beak", "Timbre", 0.5,
       "How far the beak is open. An open beak shortens the effective tube and damps "
       "it, so the resonance rises and broadens."),
   PCT(kParamFormant, "formant", "Formant", "Timbre", 0.55,
       "How strongly the tract colours the source. At zero the syrinx is heard raw."),
   PCT(kParamRasp, "rasp", "Rasp", "Timbre", 0.0,
       "How irregular the closure is from one cycle to the next. A corvid's rasp is "
       "not extra harmonics, it is a contact that is never the same twice, and that "
       "is broadband in a way no harmonic stack is. The roughest recordings in the "
       "library measure 9 dB flatter in the spectrum than the cleanest."),
   PCT(kParamRadiate, "radiate", "Radiate", "Timbre", 0.35,
       "How much of the output is the labial velocity rather than its displacement. "
       "The radiated pressure of a small source follows the rate of change of the "
       "airflow rather than the flow, and this is the balance between the two: the "
       "derivative term tilts the harmonics up by 6 dB an octave, referenced to the "
       "library's median pitch so it stays a tilt and not a gain that changes with "
       "the note. It is what makes a small source sound small."),

   // ---------------------------------------------------------------- phrase
   STEP(kParamSyllables, "syllables", "Syllables", "Phrase", 1.0, 24.0, 3.0, "",
        "Syllables in one phrase. Measured 1 to 22, median 3."),
   LOG(kParamSyllableRate, "syllable_rate", "Syllable Rate", "Phrase", 0.6180, 0.5, 40.0, "/s",
       "How fast they come inside the phrase. Measured 2.0 to 21.6 a second, median "
       "7.5. Past about 15 the ear stops counting them and hears a trill."),
   BIPCT(kParamRateDrift, "rate_drift", "Rate Drift", "Phrase", 0.0,
         "Whether the phrase speeds up or slows down as it goes. Positive "
         "accelerates."),
   PCT(kParamLegato, "legato", "Legato", "Phrase", 0.70,
       "How much syllables run into each other rather than being separated by "
       "silence. 70 % of the library's syllable pairs have no silence between them at "
       "all, which is why this defaults high: a phrase of separated notes is the "
       "exception."),
   LIN(kParamMotif, "motif", "Motif", "Phrase", -12.0, 12.0, 0.0, "st",
       "How far the pitch steps from one syllable to the next. Zero repeats one "
       "syllable; a few semitones makes the phrase a figure that goes somewhere."),
   PCT(kParamVariation, "variation", "Variation", "Phrase", 0.35,
       "How much each syllable of a phrase differs from the last in pitch, length "
       "and contour. This is the difference between a bird and a sequencer."),
   LOG(kParamPhraseGap, "phrase_gap", "Phrase Gap", "Phrase", 0.3968, 0.02, 20.0, "s",
       "Silence after a phrase before the next repeat. Measured 0.11 to 1.63 s, "
       "median 0.31 -- and it is 5.9 times the gap inside a phrase, which is what "
       "makes the phrase audible as a unit."),
   STEP(kParamRepeats, "repeats", "Repeats", "Phrase", 1.0, 8.0, 1.0, "",
        "How many times a phrase is repeated before the bird stops."),

   // ----------------------------------------------------------------- flock
   LIN(kParamShotLevel, "shot_level", "Shot Level", "Flock", -60.0, 6.0, 0.0, "dB",
       "Level of the phrase a note fires. This is the one-shot half of the plugin: "
       "play a note and one bird sings once, where you asked for it. Turn it right "
       "down for a flock with nothing deliberate in it."),
   LIN(kParamFlockLevel, "flock_level", "Flock Level", "Flock", -60.0, 6.0, -12.0, "dB",
       "Level of the birds that sing unprompted while a note is held. This is the "
       "drone half: at -60 dB there are none and every sound is one you played."),
   LOG(kParamFlockRate, "flock_rate", "Flock Rate", "Flock", 0.7685, 2.0, 1200.0, "/min",
       "How often somebody in the flock says something, as a Poisson process rather "
       "than a clock. Measured 87 to 711 syllables a minute across the library, "
       "median 273."),
   STEP(kParamBirds, "birds", "Birds", "Flock", 1.0, 16.0, 5.0, "",
        "How many birds there are. Each keeps its own pitch, position, distance and "
        "voice for as long as the note lasts, so a flock has individuals in it "
        "rather than one bird moving about."),
   LIN(kParamPitchSpread, "pitch_spread", "Pitch Spread", "Flock", 0.0, 3.0, 0.8, "oct",
       "How far apart in pitch the birds are. Zero is a flock of clones."),
   PCT(kParamVoiceSpread, "voice_spread", "Voice Spread", "Flock", 0.3,
       "How far apart their voices are: length, contour, richness and roughness."),
   PCT(kParamAnswer, "answer", "Answer", "Flock", 0.35,
       "How often one bird's phrase provokes a reply from another, shortly after and "
       "from somewhere else. Real birds do this and it is most of what makes a flock "
       "sound like a conversation instead of a random process."),
   PCT(kParamRestless, "restless", "Restless", "Flock", 0.4,
       "How much the flock's rate drifts. At zero the density is constant, which no "
       "dawn chorus is."),
   STEP(kParamMaxVoices, "max_voices", "Max Voices", "Flock", 4.0, 64.0, 24.0, "",
        "Ceiling on syllables and drum strikes sounding at once, so the cost is "
        "bounded."),

   // ------------------------------------------------------------------ drum
   LIN(kParamDrumLevel, "drum_level", "Drum Level", "Drum", -60.0, 6.0, -60.0, "dB",
       "Level of the woodpecker layer. Off by default: 9 of the 58 references hold "
       "drumming and the other 49 do not."),
   LIN(kParamDrumRate, "drum_rate", "Drum Rate", "Drum", 0.0, 120.0, 6.0, "/min",
       "Rolls a minute from the flock. At zero the woodpecker only answers a note, "
       "which is how to place a single roll exactly where it is wanted."),
   STEP(kParamStrikes, "strikes", "Strikes", "Drum", 2.0, 32.0, 10.0, "",
        "Strikes in one roll. Measured 4 to 21, median 10."),
   LOG(kParamStrikeRate, "strike_rate", "Strike Rate", "Drum", 0.6260, 2.0, 50.0, "/s",
       "How fast they come. Measured 8.1 to 20.8 a second, median 15."),
   BIPCT(kParamDrumAccel, "drum_accel", "Accelerate", "Drum", 0.23,
         "Whether the roll speeds up as it goes. A woodpecker's roll is usually "
         "described as slowing down; the library says the opposite, and clearly -- 22 "
         "of 35 rolls accelerate, and the last third of a roll runs at 0.79 of the "
         "interval of the first. Positive is faster."),
   LOG(kParamKnock, "knock", "Knock", "Drum", 0.4771, 150.0, 8000.0, "Hz",
       "The wood's first mode. There is a second above it, so the spectral centroid "
       "of a strike lands about a quarter higher than this: the library measures "
       "centroids of 654 Hz to 5.9 kHz with a median of 1251 over a bandwidth of "
       "935, and 1 kHz here is what produces that."),
   LOG(kParamRing, "ring", "Ring", "Drum", 0.5093, 0.5, 300.0, "ms",
       "How long the contact lasts. The body itself is broad -- 935 Hz of bandwidth "
       "is a Q near 1.3, which has rung out in a fifth of a millisecond -- so what "
       "makes a strike last is the contact, not the resonance. Measured strikes fall "
       "20 dB in 2 to 51 ms with a median of 8, which is 13 ms here."),

   // ----------------------------------------------------------------- place
   PCT(kParamDistance, "distance", "Distance", "Place", 0.35,
       "How far away the birds are. Distance takes the top off a call and drops its "
       "level, and it is the single most effective control here for putting birds "
       "behind a mix rather than in it."),
   PCT(kParamDistanceSpread, "distance_spread", "Depth", "Place", 0.4,
       "How much the birds differ in distance. A wood has near birds and far ones, "
       "and that spread is most of its depth."),
   PCT(kParamAir, "air", "Air", "Place", 0.5,
       "How much of the high end survives the distance. Cold dry air keeps more."),
   PCT(kParamWidth, "width", "Width", "Place", 0.6,
       "Stereo spread of the flock. The library's median L/R correlation is 0.71, "
       "which is noticeably more correlated than wind or surf: birds are point "
       "sources, so they sit somewhere rather than being everywhere."),
   PCT(kParamSpaceAmount, "space_amount", "Space Amount", "Place", 0.25,
       "How much of the surroundings you hear: a wood, a courtyard, a hall."),
   PCT(kParamSpaceSize, "space_size", "Space Size", "Place", 0.55,
       "Size of that space, in metres of it."),
   PCT(kParamSpaceDamping, "space_damping", "Space Damping", "Place", 0.6,
       "How absorbent its surfaces are. A wood full of leaves is very absorbent; a "
       "stone courtyard is not."),

   // ---------------------------------------------------------------- filter
   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Shape of the output filter."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.0, 20.0, 2000.0, "Hz",
       "Permanent highpass. Birds have almost nothing below 250 Hz, so this can go "
       "high without losing any of them."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 30.0, 20000.0, "Hz",
       "Corner of the output filter."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Resonance of the output filter."),
   PCT(kParamFilterKeyTrack, "filter_keytrack", "Filter Key Track", "Filter", 0.0,
       "How far the filter follows the played note."),

   // -------------------------------------------------------------- envelope
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.1400, 0.5, 10000.0, "ms",
       "How long the flock takes to come up when a note starts. Short, unlike the "
       "rest of the suite: a bird does not fade in, and the syllable envelopes are "
       "inside the syllables."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.5283, 5.0, 20000.0, "ms",
       "Fall from the initial level to the sustain."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level held while the note is down."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.5503, 5.0, 30000.0, "ms",
       "How long the flock takes to stop after the note is let go. Syllables already "
       "in flight always finish, so a short release thins the flock rather than "
       "cutting anything off."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity To Level", "Envelope", 0.6,
       "How much velocity sets the level."),
   PCT(kParamVelToPitch, "vel_to_pitch", "Velocity To Pitch", "Envelope", 0.2,
       "How much velocity raises the pitch, which is not the same thing: a bird "
       "calling harder pushes more air past a tighter syrinx, so it goes up as well "
       "as getting louder."),

   // ---------------------------------------------------------------- output
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Output level."),
   STEP(kParamSeed, "seed", "Random Seed", "Output", 0.0, 999.0, 0.0, "",
        "Non-zero gives the same birds every time. Zero is always different."),
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

} // namespace chirpparade
