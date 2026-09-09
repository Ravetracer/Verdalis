#pragma once

// ChirpParade's synthesis: measured contours, driving an oscillator.
//
// A bird syllable *is* its frequency contour. That is the whole finding, and it
// took a wrong version of this plugin to reach it.
//
// The first attempt modelled the syrinx from first principles -- the
// Gardner-Laje-Mindlin labium, two gestures, the phase between them as the
// syllable's shape -- and fitted it to the medians of the reference library.
// Every number agreed and it sounded nothing like a bird, because the medians
// had been measured through a 21 ms analysis window:
//
//     through a 21 ms window     peak slew   2.7 oct/s,  0.25 direction changes
//     at 0.33 ms resolution      peak slew    20-440 oct/s,  2-40 changes
//
// A real syllable is a scribble. A single sinusoidal gesture cannot draw one,
// and no amount of correct physics above it helps.
//
// So the contour is measured instead. tools/analysis/contours.py pulls the
// pitch and level contour of every well-isolated syllable in the library out of
// the WAV, fits each as a cosine series in normalised syllable time, clusters
// them per species and keeps the medoids. Forty terms lands within a third of a
// semitone of the real thing; one term -- which is what a single gesture is --
// is out by most of a semitone before it starts.
//
// That is the same procedure van Hunter Adams uses to synthesise a northern
// cardinal, and the same one the Bitwig Grid patch in !dev uses: read the
// contour off the recording and drive an oscillator with it. Adams does it by
// drawing lines on a spectrogram in PowerPoint and fitting one sine term; this
// does it automatically, at scale, with forty.
//
// **No audio is stored.** contours_generated.h holds coefficients -- a pitch
// curve and a level curve per archetype, 4288 floats in total. A contour is a
// formula in exactly the sense Adams' is.
//
// What the engine does per syllable:
//
//   the contour   Two tables read out over the syllable: pitch in octaves about
//                 its own centre, level in dB. `Contour` chooses which
//                 archetype, `Detail` how much of its fine motion survives,
//                 `Sweep` how far it travels, `Skew` how its time is warped.
//
//   the voice     A phase accumulator at the contour's frequency, through a
//                 one-sided valve. `Voice` is the fraction of each cycle the
//                 valve is shut: at zero it passes a pure sine, which is what
//                 59 % of the library's syllables are, and closing it grows the
//                 harmonic stack a corvid has. Air only passes while the valve
//                 is open, and that one-sidedness is where the even harmonics
//                 come from -- a symmetric oscillator has none at all.
//
//   the tract     The trachea as a closed tube at c/4L, with the beak both
//                 raising the resonance and making it follow the pitch, the way
//                 a songbird's gape does. Measured: in the library's harmonic
//                 syllables the loudest harmonic is not the first in 80 % of
//                 cases, and it sits at a median 1749 Hz -- a 4.9 cm trachea.
//
// Above the syllable, unchanged from the first version because it was not what
// was wrong: syllables into phrases at a rate of their own, phrases separated by
// a gap 5.9 times the gap inside them, and a flock of individuals each with its
// own pitch, position, distance and voice, calling as a Poisson process and
// answering each other. And one layer that is not a voice at all: woodpecker
// drumming, which is sonation -- a bill against wood.

#include "../params.h"

#include "verdalis/dsp/adsr.h"
#include "verdalis/dsp/fastmath.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/reverb.h"
#include "verdalis/dsp/rng.h"

#include <cstdint>

namespace chirpparade {

struct EngineParams {
   float gain = 1.0f; // linear

   // syllable
   float pitchHz = 2580.0f;
   float contour = 0.5f;   // which archetype, across the species' set
   float detail = 1.0f;    // how much of the contour's fine motion survives
   float sweep = 1.0f;     // scales the contour's pitch excursion
   float lengthSec = 0.096f;
   float skew = 0.5f;      // time warp; 0.5 is none
   float jitter = 0.12f;
   float pulseRateHz = 13.4f;
   float pulseDepth = 0.2f;

   // timbre
   int species = kSpeciesSparrow;
   float voice = 0.30f;
   float breath = 0.18f;
   float tractCm = 4.9f;
   float beak = 0.5f;
   float formant = 0.55f;
   float rasp = 0.0f;
   float radiate = 0.35f;
   float partials = 0.6f;

   // phrase
   int syllables = 3;
   float syllableRate = 7.5f;
   float rateDrift = 0.0f;
   float legato = 0.70f;
   float motifSemis = 0.0f;
   float variation = 0.35f;
   float phraseGapSec = 0.31f;
   int repeats = 1;

   // flock
   float shotGain = 1.0f;   // linear
   float flockGain = 0.25f; // linear
   float flockRatePerMin = 273.0f;
   int birds = 5;
   float pitchSpreadOct = 0.8f;
   float voiceSpread = 0.3f;
   float answer = 0.35f;
   float restless = 0.4f;
   int maxVoices = 24;

   // drum
   float drumGain = 0.0f; // linear
   float drumRatePerMin = 6.0f;
   int strikes = 10;
   float strikeRate = 15.0f;
   float drumAccel = 0.23f;
   float knockHz = 1250.0f;
   float ringSec = 0.008f;

   // place
   float distance = 0.35f;
   float distanceSpread = 0.4f;
   float air = 0.5f;
   float width = 0.6f;
   float spaceAmount = 0.25f;
   float spaceSize = 0.55f;
   float spaceDamping = 0.6f;

   // filter
   int filterType = 0;
   float highpassHz = 20.0f;
   float filterCutoffHz = 20000.0f;
   float filterReso = 0.1f;
   float filterKeyTrack = 0.0f;

   // envelope
   float attackSec = 0.002f;
   float decaySec = 0.4f;
   float sustain = 1.0f;
   float releaseSec = 0.6f;
   float velToLevel = 0.6f;
   float velToPitch = 0.2f;

   int seed = 0;
};

// What a species is, as measured. Every field except Screech's is the median
// over the recordings of that bird in the reference library; see
// tools/analysis/species.py, which prints this table and the grouping it used.
//
// The engine applies these as ratios to the library-wide medians rather than as
// absolute values, so choosing a species moves the knobs' meaning without
// taking them away.
struct SpeciesTraits {
   float pitchHz;   // median fundamental of that bird's recordings
   // Median duration of this species' archetypes. Documentation and the
   // analysis tools' target only: since 0.5.0 the engine takes each syllable's
   // length from the archetype it actually plays, not from this median.
   float lengthSec;
   float harmonics; // above -24 dB of the loudest
   float roughDb;   // spectral flatness of the syllable band
   float ratePerMin;
};

// Sweep, contour shape and skew are no longer here. They were columns of this
// table in the first version, and they were the wrong three numbers: a
// syllable's shape is not summarised by a sweep width and a turn count. It is
// in contours_generated.h, as measured curves.

// The library-wide medians the table above is relative to.
constexpr float kLibraryPitchHz = 2580.0f;
constexpr float kLibraryLengthSec = 0.096f;
constexpr float kLibraryRoughDb = -28.0f;
constexpr float kLibraryRatePerMin = 273.0f;

const SpeciesTraits &speciesTraits(int species);

// One sounding syllable: one measured contour, read out over its own duration.
struct Chirp {
   bool active = false;
   int voice = -1;

   // The syllable clock, 0 to 1 across it.
   float phase = 0.0f;
   float phaseInc = 0.0f;

   // Which contour, and how it is read. The tables themselves are shared and
   // resolved once at prepare(); a chirp only holds where to look and how.
   int archetype = 0;
   float depth = 1.0f;   // Sweep: scales the contour's pitch excursion
   float warp = 0.5f;    // Skew: bends the syllable's own time axis
   float smoothCoef = 1.0f; // Detail: a one-pole on the contour as it is read
   float pitchOct = 0.0f;   // where the archetype's centre is placed, in octaves
   float smoothed = 0.0f;   // the smoother's state
   bool primed = false;

   // The oscillator, and the valve above it.
   float osc = 0.0f;
   float closure = 0.0f; // Voice: the fraction of the cycle the valve is shut
   float rasp = 0.0f;
   // How much of the timbre is the archetype's measured partial balance rather
   // than the valve, already scaled by how much that measurement is worth.
   float partials = 0.0f;
   float closureNow = 0.0f; // jittered per cycle when Rasp is up
   float prevFlow = 0.0f;   // for the radiation derivative
   OnePoleHp dcBlock;

   float level = 1.0f;
   float pulsePhase = 0.0f;
   float pulseInc = 0.0f;
   float pulseDepth = 0.0f;

   // The tube above the valve, and the radiation off the end of it.
   Svf tract;
   Lp2 top;
   float formant = 0.55f;
   float radiate = 0.35f;
   float breath = 0.18f;
   float tractHz = 1749.0f;
   float tractReso = 0.5f;
   float tractTrack = 0.5f;
   uint32_t tractCounter = 0;

   // Pitch jitter as a random walk rather than per-sample noise: a bird drifts,
   // it does not dither.
   float jitter = 0.0f;
   float walk = 0.0f;
   float walkCoef = 0.0f;

   float panL = 0.7071f, panR = 0.7071f;
   Lp2 air;
   RngLite rng;

   inline bool finished() const { return phase >= 1.0f; }
};

// One woodpecker strike: a bill against wood, so an excited resonator rather
// than an oscillator. Two modes, because a struck branch has more than one.
struct Strike {
   bool active = false;
   int voice = -1;
   Svf body, body2;
   // The resonators' own skirts fall at only 6 dB/octave, so without this the
   // spectral centroid of a strike sat an octave above the Knock it was set to.
   Lp2 top;
   float excite = 0.0f;
   float exciteCoef = 0.0f;
   float level = 0.0f;
   float panL = 0.7071f, panR = 0.7071f;
   Lp2 air;
   RngLite rng;
};

// One bird's identity, held for as long as the note is. A flock has
// individuals in it rather than one bird moving about, so everything that
// distinguishes one bird from another is drawn once, here, and not per
// syllable.
struct Bird {
   float pitchOct = 0.0f;
   float pan = 0.0f;
   float distance = 0.35f;
   float voiceOff = 0.0f;   // biases mu and roughness
   float lengthMul = 1.0f;
   float sweepMul = 1.0f;
   float contourOff = 0.0f;
   float rateMul = 1.0f;
   float levelMul = 1.0f;
};

// A scheduled sequence: syllables, or the strikes of a drum roll. Silent by
// itself -- a phrase is a plan, not a sound, which is what keeps the pool cheap
// enough to be generous with.
struct Phrase {
   bool active = false;
   bool drum = false;
   // Whether a note asked for this phrase. A shot always finishes, however
   // short the note was -- that is what makes a note usable as a one shot. The
   // flock is gated by the envelope instead, because it is a bed.
   bool shot = false;
   int voice = -1;
   int bird = 0;

   int index = 0;     // which syllable of the phrase
   int remaining = 0; // syllables left in this repeat
   int syllables = 1;
   int repeatsLeft = 0;

   double timer = 0.0;  // seconds until the next syllable
   float interval = 0.1f;
   float driftMul = 1.0f;
   float gapAfter = 0.3f;
   // The slot the last syllable actually took. A measured archetype longer than
   // `interval` cannot be squeezed into it -- one bird cannot overlap itself --
   // so it pushes the next syllable out instead of being truncated. Equal to
   // `interval` for everything at or under the nominal pace, which is most of
   // the library, so the common case is timed exactly as before.
   float slot = 0.1f;

   float pitchHz = 2580.0f;
   float motifSemis = 0.0f;
   float variation = 0.0f;
   float level = 1.0f;

   RngLite rng;
};

// One held note: its own flock.
struct Voice {
   bool active = false;
   int16_t port = 0, channel = 0, key = 60;
   int32_t noteId = -1;
   float velocity = 1.0f;
   float pitchMul = 1.0f; // the note, as a ratio on Pitch

   Adsr env;

   Bird birds[16];
   int birdCount = 5;

   double flockTimer = 0.0;
   double drumTimer = 0.0;
   OnePoleLp restlessLp;
   uint32_t modCounter = 0;

   // A note arrives before the parameter events in its own block have been
   // applied, so nothing is laid out or scheduled in noteOn: both are deferred
   // to the first scheduling tick, which these two flags carry.
   bool birdsReady = false;
   bool pendingShot = false;

   inline bool finished() const { return env.isIdle(); }
};

class ChirpEngine {
public:
   void prepare(double sampleRate, uint32_t maxBlockSize);
   void reset();

   void setParams(const EngineParams &p);

   void noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId, double velocity);
   void noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void choke(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void allSoundOff();

   void process(float *outL, float *outR, uint32_t numSamples);

   // Fires one phrase with no note held, from the editor. Called on the audio
   // thread once per request the window has posted.
   void triggerShot();

   bool isSilent() const;
   uint32_t activeVoiceCount() const;
   // What the window's activity meter counts.
   uint32_t activeChirpCount() const;
   // Monotonic count of syllables sung, for the header ornament.
   uint32_t syllableCounter() const { return mSyllableCounter; }
   float tailSeconds() const;

   static constexpr uint32_t kMaxVoices = 16;
   static constexpr uint32_t kMaxChirps = 64;
   static constexpr uint32_t kMaxStrikes = 64;
   static constexpr uint32_t kMaxPhrases = 64;
   static constexpr uint32_t kMaxBirds = 16;
   static constexpr uint32_t kModInterval = 32; // control rate for the scheduling

   // How finely the archetype tables are rendered. The 40th cosine term makes
   // twenty full cycles across a syllable, so a table has to carry a good many
   // points per cycle for linear interpolation not to lose exactly the fine
   // motion the whole thing is for. 512 gives twelve points per cycle of the
   // fastest term.
   static constexpr int kContourPoints = 512;

   // The partial balance moves slowly -- 4.4 dB across a whole syllable -- so its
   // tables need far fewer points than the pitch contour, whose whole purpose is
   // the fast motion. Twelve terms over 128 points is twenty-one points per
   // cycle of the fastest one.
   static constexpr int kHarmPoints = 128;

private:
   void updateFilters();
   void configureBirds(Voice &v);
   void schedule(Voice &v, float env, double blockSec);
   Phrase *startPhrase(int voiceIndex, int bird, float level, bool drum, bool shot);
   bool hasRunningShot(int voiceIndex) const;
   void spawnChirp(const Voice &v, Phrase &ph);
   void spawnStrike(const Voice &v, Phrase &ph);
   Chirp *allocateChirp();
   Strike *allocateStrike();
   Phrase *allocatePhrase();
   void processChirps(float *outL, float *outR, uint32_t numSamples);
   void processStrikes(float *outL, float *outR, uint32_t numSamples);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);
   float noteKeyTrack() const;

   EngineParams mP;
   double mSampleRate = 48000.0;

   Voice mVoices[kMaxVoices];
   Chirp mChirps[kMaxChirps];
   Strike mStrikes[kMaxStrikes];
   Phrase mPhrases[kMaxPhrases];

   Rng mRng;
   int mAppliedSeed = 0;
   uint32_t mSyllableCounter = 0;
   int mLastKey = 60;

   Space mSpace;
   Svf mOutFilterL, mOutFilterR;
   Hp2 mOutHpL, mOutHpR;

   // Derived once per parameter change rather than per sample.
   float mSpeciesPitchMul = 1.0f;
   float mSpeciesRateMul = 1.0f;
   float mSpeciesClosure = 0.0f;
   float mSpeciesBreathMul = 1.0f;
   float mFormantHz = 1741.0f;
   float mFormantQ = 0.55f;
   float mAirCutoffHz = 20000.0f;
   float mDistanceGain = 1.0f;

   float mSilenceCounter = 0.0f;
};

} // namespace chirpparade
