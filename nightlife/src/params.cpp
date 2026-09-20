#include "params.h"

#include "verdalis/param_macros.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace nightlife {

namespace {

const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
const char *const kCallerNames[] = {"Wolf", "Owl", "Screech", "Scops", "Fox", "Loon"};

// Defaults are the medians measured across the reference library: 45 field
// recordings, 225 segmented calls from the six callers and 627 croaks from the
// thirteen frog recordings. See tools/analysis/README.md for every number and
// how it was taken.
const ParamDesc kParams[kNumParams] = {
   // ------------------------------------------------------------------ call
   LOG(kParamPitch, "pitch", "Pitch", "Call", 0.5864, 60.0, 6000.0, "Hz",
       "Where the call sits. The fundamental across the library runs 138 Hz to "
       "3.1 kHz with a median of 893, which is the default: a wolf and a big owl at "
       "the bottom, a vixen's scream at the top. The contour is measured about its "
       "own centre, so this transposes it -- and because the default is the library "
       "median, every caller at the default calls in its own register."),
   PCT(kParamContour, "contour", "Contour", "Call", 0.5,
       "Which call. Each caller carries eight contours measured off real recordings "
       "-- the medoids of its clustered calls -- and this walks across them, "
       "lowest-sitting first. It is the most important control in the plugin: a "
       "call's identity is its frequency contour, and these are measured curves "
       "rather than a shape derived from summary statistics. The wolf's vibrato is "
       "in them, at the 2 to 6 Hz the recordings actually carry."),
   PCT(kParamDetail, "detail", "Detail", "Call", 1.0,
       "How much of the contour's fine motion survives. At 100 % the measured curve "
       "passes through; lower it and the contour smooths towards a glide -- which is "
       "what a howl synthesised from a description rather than a measurement sounds "
       "like, and the reason this pipeline exists."),
   LIN(kParamSweep, "sweep", "Sweep", "Call", 0.0, 300.0, 100.0, "%",
       "Scales how far the contour travels. 100 % is the measured curve exactly; "
       "below that it flattens towards a held note, above it exaggerates. It goes "
       "past 100 % on purpose: a wolf holding one note is a real howl and so is one "
       "that swings a fifth, and the archetypes span 0.04 to 1.4 octaves."),
   LOG(kParamLength, "length", "Length", "Call", 0.5000, 40.0, 6000.0, "ms",
       "How long one call lasts. The archetypes run 75 ms to 3.1 s with a median of "
       "490, and each plays at its own measured duration scaled by this -- so a "
       "caller's spread of call lengths survives instead of every curve being "
       "stretched to one target."),
   PCT(kParamSkew, "skew", "Skew", "Call", 0.5,
       "Bends the call's own time axis: below a half it crowds the contour towards "
       "the start, above it towards the end. 0.5 plays the measured curve at its "
       "measured pace, and the asymmetry the library shows -- a 137 ms rise against a "
       "278 ms fall -- is already in it."),
   PCT(kParamJitter, "jitter", "Jitter", "Call", 0.10,
       "How much the pitch wanders off its own contour. A larynx is not a "
       "synthesiser: without this every call of a phrase is identical, which no "
       "recording in the library is."),
   PCT(kParamVibrato, "vibrato", "Vibrato", "Call", 0.0,
       "Extra vibrato, on top of whatever the measured contour already does. It "
       "defaults to nothing for a reason: six of the eight wolf archetypes carry a "
       "measured vibrato of their own, between 2.1 and 6.4 Hz, so this is for "
       "pushing further rather than for supplying what is missing."),
   LOG(kParamVibratoRate, "vibrato_rate", "Vibrato Rate", "Call", 0.5196, 0.5, 20.0, "Hz",
       "Rate of that extra vibrato. The default is the median measured across every "
       "archetype that has one, 3.4 Hz."),

   // ----------------------------------------------------------------- voice
   ENUM(kParamCaller, "caller", "Caller", "Voice", 0.0, kCallerNames,
        "Which animal. Each entry chooses that caller's own set of measured contours "
        "and biases pitch, length, harmonic richness, roughness and call rate "
        "together, because in the references they move together. Every figure is the "
        "median of the recordings of that animal; nothing here is invented."),
   PCT(kParamVoice, "voice", "Voice", "Voice", 0.30,
       "How much of each cycle the larynx is shut. Air passes only while the folds "
       "are apart, so this is not a filter: at the bottom the valve never closes and "
       "a pure tone comes out, which is what a hoot nearly is, and closing it makes "
       "the airflow a one-sided pulse with the harmonic stack a scream has -- evens "
       "as well as odds, which a symmetric oscillator cannot produce at all."),
   PCT(kParamBreath, "breath", "Breath", "Voice", 0.06,
       "Turbulent air past the folds. It is also what starts the oscillation: the "
       "noise is injected into the oscillator rather than added to its output, which "
       "is why a call's onset is never twice the same. The chosen caller scales it, "
       "from the roughness its own recordings measure -- a fox is 10 dB rougher than "
       "everything else in the library."),
   LIN(kParamThroat, "throat", "Throat", "Voice", 1.0, 60.0, 19.0, "cm",
       "Length of the tube above the larynx, which resonates at c/4L. 19 cm puts the "
       "resonance at 450 Hz, which is where the library's hoots and howls sit; a fox "
       "is a much shorter tube and a wolf a longer one."),
   PCT(kParamMuzzle, "muzzle", "Muzzle", "Voice", 0.5,
       "How far the mouth is open. It raises the tube's resonance, damps it, and "
       "makes it follow the pitch -- which is what an animal that opens its jaw as it "
       "rises actually does, and a wolf does exactly that."),
   PCT(kParamFormant, "formant", "Formant", "Voice", 0.55,
       "How much of that resonance reaches the output. At zero the voice is the bare "
       "valve; up, and the tube colours it."),
   PCT(kParamRasp, "rasp", "Rasp", "Voice", 0.0,
       "Irregular closure of the valve, cycle by cycle. A fox's scream and the break "
       "at the top of a howl are both this rather than more harmonics."),
   PCT(kParamRadiate, "radiate", "Radiate", "Voice", 0.35,
       "How much of the output is the *rate of change* of the flow rather than the "
       "flow. A small source radiates the derivative, which tilts the harmonics up by "
       "6 dB an octave."),
   PCT(kParamPartials, "partials", "Partials", "Voice", 0.6,
       "How much of the timbre is the archetype's own *measured* balance between its "
       "first six partials rather than the synthetic valve. This is the control that "
       "reaches for the recording itself: a fox opens and closes its harmonic stack "
       "within one scream, and a fixed valve through a fixed tract cannot do that at "
       "all. Scaled by how much of that call's energy the measurement actually "
       "accounts for, so an archetype with a second animal in it does not pretend to "
       "know."),

   // ---------------------------------------------------------------- phrase
   STEP(kParamCalls, "calls", "Calls", "Phrase", 1.0, 32.0, 3.0, "",
        "How many calls one phrase holds. Measured at a median of 3, and the spread "
        "is the whole point: a tawny owl's phrase is two notes, a wolf's is one held "
        "note, a fox's six and a scops owl's fifty-seven of the same pip."),
   LOG(kParamCallRate, "call_rate", "Call Rate", "Phrase", 0.4966, 0.05, 20.0, "Hz",
       "How fast the calls of one phrase follow each other, onset to onset. "
       "Measured across the library at 0.98 Hz -- a call about every second -- and "
       "from 0.32 Hz for a screech owl's spaced whinnies to 2.85 for an owl's paired "
       "hoots. A call longer than the interval pushes the next one out rather than "
       "being cut short: one animal cannot overlap itself."),
   PCT(kParamRateDrift, "rate_drift", "Rate Drift", "Phrase", 0.0,
       "How much a phrase speeds up as it goes. A screech owl's whinny accelerates; "
       "a hoot does not."),
   PCT(kParamLegato, "legato", "Legato", "Phrase", 0.32,
       "How far each call is stretched towards filling its own slot, so that a "
       "phrase runs together rather than leaving silence between its notes. The "
       "default is measured: 32 % of consecutive calls in the library touch, against "
       "ChirpParade's 70 % -- a night is not a dawn chorus, and a Legato inherited "
       "from that plugin stretches a 181 ms hoot to fill an 833 ms slot."),
   LIN(kParamMotif, "motif", "Motif", "Phrase", -12.0, 12.0, 0.0, "st",
       "How far the pitch steps from one call of a phrase to the next. A barred owl "
       "drops on its last note; most things do not move at all."),
   PCT(kParamVariation, "variation", "Variation", "Phrase", 0.30,
       "How much each call differs from the last in contour, pitch and length. At "
       "zero a phrase is one call repeated, which nothing in the library is."),
   LOG(kParamPhraseGap, "phrase_gap", "Phrase Gap", "Phrase", 0.6070, 0.05, 60.0, "s",
       "Silence between one phrase and its repeat. Measured at 3.7 s across the "
       "library; a tawny owl leaves 6.3 and a scops owl keeps its own up for "
       "minutes."),
   STEP(kParamRepeats, "repeats", "Repeats", "Phrase", 1.0, 16.0, 1.0, "",
        "How many times a phrase repeats before the animal stops."),

   // ------------------------------------------------------------------ pack
   LIN(kParamShotLevel, "shot_level", "Shot Level", "Pack", -60.0, 6.0, 0.0, "dB",
       "Level of the phrase a note fires. At -60 dB a note brings up the night "
       "without anything calling on it."),
   LIN(kParamPackLevel, "pack_level", "Pack Level", "Pack", -60.0, 6.0, -6.0, "dB",
       "Level of the animals that call unprompted while a note is held. This is the "
       "drone half: at -60 dB every call is one you played."),
   LOG(kParamPackRate, "pack_rate", "Pack Rate", "Pack", 0.5317, 1.0, 600.0, "/min",
       "How often somebody calls, as a Poisson process rather than a clock. Measured "
       "22 to 82 calls a minute across the six callers, median 30."),
   STEP(kParamAnimals, "animals", "Animals", "Pack", 1.0, 16.0, 3.0, "",
        "How many individuals there are. Each has its own pitch, position, distance "
        "and voice, held for as long as the note is -- a pack is individuals, not one "
        "animal moving about."),
   LIN(kParamPitchSpread, "pitch_spread", "Pitch Spread", "Pack", 0.0, 2.0, 0.5, "oct",
       "How far the pack's pitches spread. Wolves deliberately avoid each other's "
       "pitch when they howl together, which is why a chorus of them sounds like more "
       "animals than it is."),
   PCT(kParamVoiceSpread, "voice_spread", "Voice Spread", "Pack", 0.35,
       "How much the individuals differ in timbre, length, contour and rate."),
   PCT(kParamAnswer, "answer", "Answer", "Pack", 0.45,
       "How often one animal answers another, shortly after. This is most of what "
       "makes a pack sound like a conversation rather than a random process, and it "
       "is what a wolf chorus actually is."),
   PCT(kParamRestless, "restless", "Restless", "Pack", 0.4,
       "A slow drift of how active the pack is. No night is uniform."),
   STEP(kParamMaxVoices, "max_voices", "Max Voices", "Pack", 4.0, 64.0, 24.0, "",
        "Ceiling on the calls sounding at once, which is the plugin's CPU cost."),

   // ---------------------------------------------------------------- chorus
   LIN(kParamChorusLevel, "chorus_level", "Chorus Level", "Chorus", -60.0, 6.0, -8.0, "dB",
       "Level of the frog chorus. It is a layer of its own because a croak is not a "
       "call: it is a pulse train through a body resonance, with no pitch contour in "
       "it at all."),
   PCT(kParamCroak, "croak", "Croak", "Chorus", 0.5,
       "Which frog. Eight measured croaks, ordered by pulse rate: from a 10 Hz "
       "knocking at the bottom to a 69 Hz rattle at the top. Each is the median of "
       "one recording -- its pulse rate, its pulse count, both of its resonances and "
       "its envelope."),
   LOG(kParamCroakPitch, "croak_pitch", "Croak Pitch", "Chorus", 0.6436, 300.0, 6000.0, "Hz",
       "Where the croak's first resonance sits. Measured 1.5 to 3.3 kHz, median 2063. "
       "The second one moves with it: it sits at a measured 0.50 of the first across "
       "the library, and a croak with only one resonance is a beep."),
   LOG(kParamPulseRate, "pulse_rate", "Pulse Rate", "Chorus", 0.4881, 4.0, 200.0, "Hz",
       "The rate of the pulse train. This is the species: it is the first thing a "
       "field guide lists, and it is what a listener hears as the difference between "
       "a knock, a creak and a rattle. Measured 10 to 69 Hz, median 27."),
   STEP(kParamPulses, "pulses", "Pulses", "Chorus", 1.0, 64.0, 10.0, "",
        "How many pulses one croak holds. Measured 2 to 25."),
   LOG(kParamCroakLength, "croak_length", "Croak Length", "Chorus", 0.6054, 20.0, 2000.0, "ms",
       "How long one croak lasts. Measured 43 to 608 ms, median 325. It works by "
       "scaling how many pulses the croak holds rather than by slowing the train "
       "down: a long croak and a slow one are different animals, and the pulse rate "
       "is the species."),
   STEP(kParamFrogs, "frogs", "Frogs", "Chorus", 1.0, 48.0, 12.0, "",
        "How many frogs there are. Each has its own pitch, position, distance and "
        "rhythm, and calls on a period of its own."),
   LOG(kParamCroakRate, "croak_rate", "Croak Rate", "Chorus", 0.7193, 2.0, 600.0, "/min",
       "How often one frog croaks. Measured 48 to 480 croaks a minute, median 121."),
   PCT(kParamRegularity, "regularity", "Regularity", "Chorus", 0.95,
       "How far the pond keeps one rhythm. At zero every frog is independent and the "
       "chorus is a Poisson process, which is what the rest of the suite spawns "
       "events with -- and it is measurably wrong here: the Fano factor of the croak "
       "arrivals is 0.90 at 50 ms and 0.30 at four seconds, where a Poisson process "
       "is exactly 1.0 at every window. A frog chorus is *more* regular than random, "
       "not less. The default is not a taste judgement either: rendered and measured "
       "with the same estimator it gives 0.90 / 0.79 / 0.56 / 0.39 against the "
       "library's 0.90 / 0.81 / 0.59 / 0.30, the closest of any setting."),
   LIN(kParamChorusSpread, "chorus_spread", "Chorus Spread", "Chorus", 0.0, 2.0, 0.7, "oct",
       "How far the frogs' pitches spread. A pond is several species at once."),
   PCT(kParamChorusWidth, "chorus_width", "Chorus Width", "Chorus", 0.85,
       "How far the chorus spreads across the stereo field."),

   // --------------------------------------------------------------- insects
   LIN(kParamInsectLevel, "insect_level", "Insect Level", "Insects", -60.0, 6.0, -14.0, "dB",
       "Level of the insect bed: crickets and katydids, which are neither callers nor "
       "croaks. There is no pitch contour and no body resonance in one -- just a "
       "narrow band of noise with a trill on it."),
   LOG(kParamInsectPitch, "insect_pitch", "Insect Pitch", "Insects", 0.5436, 800.0, 9000.0,
       "Hz",
       "Where that band sits. Measured at 2.9 to 3.2 kHz in the three references that "
       "carry an insect bed clear enough to measure."),
   PCT(kParamInsectWidth, "insect_width", "Insect Band", "Insects", 0.35,
       "How wide the band is. The measured Q is 21, which is narrow -- a cricket is "
       "very nearly a tone, and widening it turns the bed into a hiss."),
   LOG(kParamTrillRate, "trill_rate", "Trill Rate", "Insects", 0.7822, 2.0, 120.0, "Hz",
       "The pulse rate on the band. Two references carry a prominent one and they "
       "disagree by an octave, 33 and 49 Hz, which is probably a katydid and a "
       "cricket rather than an error."),
   PCT(kParamTrillDepth, "trill_depth", "Trill Depth", "Insects", 0.7,
       "How deep that pulsing cuts. All the way down is a chirping insect; not at all "
       "is a steady ring, which is what a distant chorus of them becomes."),
   PCT(kParamShimmer, "shimmer", "Shimmer", "Insects", 0.5,
       "How much the individuals differ in pitch and trill rate. It is what turns one "
       "insect into a field of them."),

   // ------------------------------------------------------------------- bed
   LIN(kParamBedLevel, "bed_level", "Bed Level", "Bed", -60.0, 6.0, -18.0, "dB",
       "Level of the night itself: the wind, the distant water, the chorus too far "
       "away to be individual animals. Its shape is measured -- the third-octave curve "
       "of the quietest third of the frames of all 45 references, solved onto an "
       "eight-band filterbank."),
   BIPCT(kParamBedTilt, "bed_tilt", "Bed Tilt", "Bed", 0.0,
         "Tilts that measured curve. Negative is a heavier, further night; positive "
         "brings the top back, which is what a still night close to the trees is."),
   PCT(kParamBedMotion, "bed_motion", "Bed Motion", "Bed", 0.3,
       "A slow drift of the bed's level, so it breathes rather than sitting still."),

   // ----------------------------------------------------------------- place
   PCT(kParamDistance, "distance", "Distance", "Place", 0.45,
       "How far away the animals are. Distance takes the top off a call and drops its "
       "level, and it is the single most effective control here: a wolf at the far "
       "end of a valley is most of what the word 'howl' means."),
   PCT(kParamDistanceSpread, "distance_spread", "Depth", "Place", 0.45,
       "How much the animals differ in distance. A night has near animals and far "
       "ones, and that spread is most of its depth."),
   PCT(kParamAir, "air", "Air", "Place", 0.5,
       "How much of the high end survives the distance. Cold dry air keeps more."),
   PCT(kParamWidth, "width", "Width", "Place", 0.7,
       "Stereo spread of the pack. They are point sources, so they sit somewhere "
       "rather than being everywhere."),
   PCT(kParamSpaceAmount, "space_amount", "Space Amount", "Place", 0.35,
       "How much of the surroundings you hear: a wood, a valley, a marsh."),
   PCT(kParamSpaceSize, "space_size", "Space Size", "Place", 0.7,
       "Size of that space, in metres of it. A howl carries because the valley "
       "answers it."),
   PCT(kParamSpaceDamping, "space_damping", "Space Damping", "Place", 0.55,
       "How absorbent its surfaces are. A wood full of leaves is very absorbent; a "
       "rock face across a lake is not."),

   // ---------------------------------------------------------------- filter
   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Shape of the output filter."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.0, 20.0, 2000.0, "Hz",
       "Permanent highpass. The lowest fundamental in the library is a 138 Hz howl, "
       "so this has room to move before anything is lost."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 30.0, 20000.0, "Hz",
       "Corner of the output filter."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Resonance of the output filter."),
   PCT(kParamFilterKeyTrack, "filter_keytrack", "Filter Key Track", "Filter", 0.0,
       "How far the filter follows the played note."),

   // -------------------------------------------------------------- envelope
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.3000, 0.5, 10000.0, "ms",
       "How long the night takes to come up when a note starts. Longer than "
       "ChirpParade's by default: a chorus and a bed fade in, where a single call "
       "does not."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.5283, 5.0, 20000.0, "ms",
       "Fall from the initial level to the sustain."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level held while the note is down."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.6500, 5.0, 30000.0, "ms",
       "How long the night takes to stop after the note is let go. Calls already in "
       "flight always finish, so a short release thins the pack rather than cutting "
       "a howl in half."),
   PCT(kParamVelToLevel, "vel_to_level", "Velocity To Level", "Envelope", 0.6,
       "How much velocity sets the level."),
   PCT(kParamVelToPitch, "vel_to_pitch", "Velocity To Pitch", "Envelope", 0.15,
       "How much velocity raises the pitch. An animal calling harder pushes more air "
       "past a tighter larynx, so it does both."),

   // ---------------------------------------------------------------- output
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Output level."),
   STEP(kParamSeed, "seed", "Random Seed", "Output", 0.0, 999.0, 0.0, "",
        "Non-zero gives the same night every time. Zero is always different."),
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

} // namespace nightlife
