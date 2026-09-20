#pragma once

// NightLife's synthesis: four layers, each measured against a different thing.
//
// A night is not one texture. It is a bed with a chorus on it, insects in the
// bed, and every so often something large enough to hear from a mile away. The
// four layers here are those, and each is built from the measurement that
// actually describes it -- which is different in every case:
//
//   the callers   A wolf, an owl, a fox, a loon. **A call is its frequency
//                 contour**, which is ChirpParade's finding, learned there the
//                 hard way: a physical model of the animal fitted to aggregate
//                 statistics measures correctly on twenty quantities and sounds
//                 like nothing alive. So tools/analysis/contours.py pulls the
//                 pitch and level contour of every well-isolated call in the
//                 library out of the WAV, fits each as a cosine series in
//                 normalised call time, clusters them per caller and keeps the
//                 medoids. Eight archetypes per caller, 48 in all.
//
//                 One thing is different from ChirpParade and it matters: the
//                 term count is **per second of call, not per call**. A fixed
//                 budget over a library whose calls span 75 ms to 3.1 s is a
//                 hidden duration filter -- at 96 terms a howl is right and
//                 every short caller rings at twice the motion it was asked to
//                 draw. 55 terms a second holds all six callers within 20 % of
//                 the path their tracked contours travel.
//
//                 The wolf's vibrato needs no oscillator. Six of the eight wolf
//                 archetypes carry one at a measured 2.1 to 6.4 Hz, and it is
//                 in the series where the wolf that was recorded put it.
//
//   the chorus    Frogs. **A croak is not a contour**: it is a pulse train
//                 through a body resonance, 10 to 69 pulses a second, and the
//                 rate is the species. tools/analysis/frogs.py measures the
//                 rate, the pulse count, the depth, both resonances and the
//                 envelope of eight close recordings, and each becomes one row
//                 of kCroaks.
//
//                 How they are *spaced* is measured too, and it is the finding
//                 that most surprised this plugin. CrackleBlaze established
//                 that a plain Poisson process is not how fire crackles arrive
//                 -- its Fano factor is 3.90 at one second against a Poisson
//                 process's 1.0. A frog chorus measures 0.90 at 50 ms and 0.30
//                 at four seconds: it is *more* regular than random, not less.
//                 So each frog here runs a clock with bounded jitter, and
//                 `Regularity` is the lever between that and the Poisson
//                 process the rest of the suite spawns with.
//
//   the insects   A narrow band of noise with a trill on it. No contour, no
//                 resonance, nothing physical modelled at all, because that is
//                 genuinely what a cricket band measures as: 2.9 to 3.2 kHz at
//                 a Q of 21, pulsed at 33 or 49 Hz. It is the weakest
//                 measurement in the plugin and it is honest about why -- the
//                 library has no recording of insects on their own, so what is
//                 measured is the bed *behind* a scops owl and a frog chorus.
//
//   the bed       The night itself. The third-octave curve of the quietest
//                 third of the frames of all 45 references, solved onto an
//                 eight-band octave filterbank. The suite has done this four
//                 times -- ShoreBreak, SkyHowl, RiverFlow, CrackleBlaze -- and
//                 those four solve the bank at run time because their controls
//                 reshape the target. Nothing here does, so the solve is done
//                 in bed.py and what ships is the answer.
//
// Above the callers, the structure ChirpParade built and NightLife is the
// second user of: calls into phrases, phrases into a pack of individuals, each
// with its own pitch, position, distance and voice, calling as a Poisson
// process and answering each other. A wolf chorus *is* that behaviour -- it is
// not decoration on top of the voice, it is what makes the layer sound like
// animals.

#include "../params.h"

#include "verdalis/dsp/adsr.h"
#include "verdalis/dsp/fastmath.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/reverb.h"
#include "verdalis/dsp/rng.h"

#include <cstdint>

namespace nightlife {

struct EngineParams {
   float gain = 1.0f; // linear

   // call
   float pitchHz = 893.0f;
   float contour = 0.5f; // which archetype, across the caller's set
   float detail = 1.0f;  // how much of the contour's fine motion survives
   float sweep = 1.0f;   // scales the contour's pitch excursion
   float lengthSec = 0.490f;
   float skew = 0.5f; // time warp; 0.5 is none
   float jitter = 0.10f;
   float vibrato = 0.0f;
   float vibratoHz = 3.4f;

   // voice
   int caller = kCallerWolf;
   float voice = 0.30f;
   float breath = 0.06f;
   float throatCm = 19.0f;
   float muzzle = 0.5f;
   float formant = 0.55f;
   float rasp = 0.0f;
   float radiate = 0.35f;
   float partials = 0.6f;

   // phrase
   int calls = 3;
   float callRate = 0.98f;
   float rateDrift = 0.0f;
   float legato = 0.32f;
   float motifSemis = 0.0f;
   float variation = 0.30f;
   float phraseGapSec = 3.7f;
   int repeats = 1;

   // pack
   float shotGain = 1.0f;  // linear
   float packGain = 0.5f;  // linear
   float packRatePerMin = 30.0f;
   int animals = 3;
   float pitchSpreadOct = 0.5f;
   float voiceSpread = 0.35f;
   float answer = 0.45f;
   float restless = 0.4f;
   int maxVoices = 24;

   // chorus
   float chorusGain = 0.4f; // linear
   float croak = 0.5f;      // which measured croak
   float croakHz = 2063.0f;
   float pulseHz = 27.0f;
   int pulses = 10;
   float croakSec = 0.325f;
   int frogs = 12;
   float croakRatePerMin = 121.0f;
   float regularity = 0.95f;
   float chorusSpreadOct = 0.7f;
   float chorusWidth = 0.85f;

   // insects
   float insectGain = 0.2f; // linear
   float insectHz = 2982.0f;
   float insectWidth = 0.35f;
   float trillHz = 49.2f;
   float trillDepth = 0.7f;
   float shimmer = 0.5f;

   // bed
   float bedGain = 0.126f; // linear
   float bedTilt = 0.0f;   // -1..1
   float bedMotion = 0.3f;

   // place
   float distance = 0.45f;
   float distanceSpread = 0.45f;
   float air = 0.5f;
   float width = 0.7f;
   float spaceAmount = 0.35f;
   float spaceSize = 0.7f;
   float spaceDamping = 0.55f;

   // filter
   int filterType = 0;
   float highpassHz = 20.0f;
   float filterCutoffHz = 20000.0f;
   float filterReso = 0.1f;
   float filterKeyTrack = 0.0f;

   // envelope
   float attackSec = 0.05f;
   float decaySec = 0.4f;
   float sustain = 1.0f;
   float releaseSec = 1.5f;
   float velToLevel = 0.6f;
   float velToPitch = 0.15f;

   int seed = 0;
};

// What a caller is, as measured. Every field is the median over the recordings
// of that animal in the reference library; see tools/analysis/callers.py, which
// prints this table and the grouping it used, and contours.py, which prints
// lengthSec.
//
// The engine applies these as ratios to the library-wide medians rather than as
// absolute values, so choosing a caller moves the knobs' meaning without taking
// them away.
struct CallerTraits {
   float pitchHz;   // median fundamental of that animal's recordings
   float lengthSec; // median duration of that caller's archetypes
   float harmonics; // above -24 dB of the loudest
   float roughDb;   // spectral flatness of the call band
   float ratePerMin;
};

// The library-wide medians the table above is relative to: the six callers'
// 225 segmented calls pooled, and the 48 archetypes' own median duration.
constexpr float kLibraryPitchHz = 893.0f;
constexpr float kLibraryLengthSec = 0.490f;
constexpr float kLibraryRoughDb = -32.0f;
constexpr float kLibraryRatePerMin = 30.0f;

const CallerTraits &callerTraits(int caller);

// One sounding call: one measured contour, read out over its own duration.
struct Call {
   bool active = false;
   int voice = -1;

   // The call clock, 0 to 1 across it.
   float phase = 0.0f;
   float phaseInc = 0.0f;

   // Which contour, and how it is read. The tables themselves are shared and
   // resolved once at prepare(); a call only holds where to look and how.
   int archetype = 0;
   float depth = 1.0f;      // Sweep: scales the contour's pitch excursion
   float warp = 0.5f;       // Skew: bends the call's own time axis
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
   // Extra vibrato, on top of what the archetype already carries.
   float vibPhase = 0.0f;
   float vibInc = 0.0f;
   float vibDepth = 0.0f;

   // The tube above the valve, and the radiation off the end of it.
   Svf tract;
   Lp2 top;
   float formant = 0.55f;
   float radiate = 0.35f;
   float breath = 0.18f;
   float tractHz = 450.0f;
   float tractReso = 0.5f;
   float tractTrack = 0.5f;
   uint32_t tractCounter = 0;

   // Pitch jitter as a random walk rather than per-sample noise: an animal
   // drifts, it does not dither.
   float jitter = 0.0f;
   float walk = 0.0f;
   float walkCoef = 0.0f;

   float panL = 0.7071f, panR = 0.7071f;
   Lp2 air;
   RngLite rng;

   inline bool finished() const { return phase >= 1.0f; }
};

// One sounding croak: a pulse train through two resonances. No oscillator and
// no contour -- the pulses are the sound, and the resonances are the frog.
struct CroakVoice {
   bool active = false;
   Svf body1, body2;
   // The train.
   float pulsePhase = 0.0f;
   float pulseInc = 0.0f;
   int pulsesLeft = 0;
   float pulseDepth = 0.97f;
   float excite = 0.0f;
   float exciteCoef = 0.0f;
   // The croak's own envelope, over the train.
   float env = 0.0f;
   float attackCoef = 0.0f;
   float releaseCoef = 0.0f;
   bool releasing = false;
   float level = 0.0f;
   float panL = 0.7071f, panR = 0.7071f;
   Lp2 air;
   RngLite rng;
};

// One frog: an identity and a clock. The clock is the point -- a frog calls on
// a period of its own, which is why the measured Fano factor of a chorus is
// below 1 and a Poisson process cannot reproduce it.
struct Frog {
   float pitchMul = 1.0f;
   float pan = 0.0f;
   float distance = 0.5f;
   float rateMul = 1.0f;
   float levelMul = 1.0f;
   int type = 0; // which row of kCroaks
   // Where in the chorus's shared cycle this frog calls, 0..1. The pond keeps
   // one rhythm and the frogs take their places in it; see scheduleChorus().
   float phaseOff = 0.0f;
   double next = 0.0; // when it calls next, on the chorus clock
};

// One insect voice: a bandpass on noise, pulsed. Several of them detuned is
// what a field of crickets is.
struct Insect {
   Svf band;
   float trillPhase = 0.0f;
   float trillInc = 0.0f;
   float panL = 0.7071f, panR = 0.7071f;
   float level = 1.0f;
   RngLite rng;
};

// One animal's identity, held for as long as the note is. A pack has
// individuals in it rather than one animal moving about, so everything that
// distinguishes one from another is drawn once, here, and not per call.
struct Animal {
   float pitchOct = 0.0f;
   float pan = 0.0f;
   float distance = 0.45f;
   float voiceOff = 0.0f; // biases the closure and the roughness
   float lengthMul = 1.0f;
   float sweepMul = 1.0f;
   float contourOff = 0.0f;
   float rateMul = 1.0f;
   float levelMul = 1.0f;
};

// A scheduled sequence of calls. Silent by itself -- a phrase is a plan, not a
// sound, which is what keeps the pool cheap enough to be generous with.
struct Phrase {
   bool active = false;
   // Whether a note asked for this phrase. A shot always finishes, however
   // short the note was -- that is what makes a note usable as a one shot. The
   // pack is gated by the envelope instead, because it is a bed.
   bool shot = false;
   int voice = -1;
   int animal = 0;

   int index = 0;     // which call of the phrase
   int remaining = 0; // calls left in this repeat
   int calls = 1;
   int repeatsLeft = 0;

   double timer = 0.0; // seconds until the next call
   float interval = 0.8f;
   float driftMul = 1.0f;
   float gapAfter = 4.0f;
   // The slot the last call actually took. A measured archetype longer than
   // `interval` cannot be squeezed into it -- one animal cannot overlap itself
   // -- so it pushes the next call out instead of being truncated.
   float slot = 0.8f;

   float pitchHz = 893.0f;
   float motifSemis = 0.0f;
   float variation = 0.0f;
   float level = 1.0f;

   RngLite rng;
};

// One held note: its own pack.
struct Voice {
   bool active = false;
   int16_t port = 0, channel = 0, key = 60;
   int32_t noteId = -1;
   float velocity = 1.0f;
   float pitchMul = 1.0f; // the note, as a ratio on Pitch

   Adsr env;

   Animal animals[16];
   int animalCount = 3;

   double packTimer = 0.0;
   OnePoleLp restlessLp;
   uint32_t modCounter = 0;

   // A note arrives before the parameter events in its own block have been
   // applied, so nothing is laid out or scheduled in noteOn: both are deferred
   // to the first scheduling tick, which these two flags carry.
   bool animalsReady = false;
   bool pendingShot = false;

   inline bool finished() const { return env.isIdle(); }
};

class NightEngine {
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
   // What the window's activity meter counts: everything sounding.
   uint32_t activeCallCount() const;
   // Monotonic count of calls made, for the header ornament.
   uint32_t callCounter() const { return mCallCounter; }
   float tailSeconds() const;

   static constexpr uint32_t kMaxVoices = 16;
   static constexpr uint32_t kMaxCalls = 48;
   static constexpr uint32_t kMaxCroaks = 64;
   static constexpr uint32_t kMaxPhrases = 64;
   static constexpr uint32_t kMaxAnimals = 16;
   static constexpr uint32_t kMaxFrogs = 48;
   static constexpr uint32_t kNumInsects = 6;
   static constexpr uint32_t kModInterval = 32; // control rate for the scheduling

   // How finely the archetype tables are rendered. The highest term of a
   // 128-term series makes 64 full cycles across a call, so a table has to
   // carry a good many points per cycle for linear interpolation not to lose
   // exactly the fine motion the whole thing is for. 1024 gives sixteen points
   // per cycle of the fastest term.
   static constexpr int kContourPoints = 1024;

   // The partial balance moves slowly, so its tables need far fewer points than
   // the pitch contour, whose whole purpose is the fast motion.
   static constexpr int kHarmPoints = 128;

private:
   void updateFilters();
   // Seeds the generators of the three continuous layers. A function of Seed
   // alone when it is non-zero, so that a seeded night's bed and insect field
   // are the same night too.
   void seedContinuous();
   void configureAnimals(Voice &v);
   void configureFrogs();
   void configureInsects();
   void schedule(Voice &v, float env, double blockSec);
   void scheduleChorus(float env, double blockSec);
   Phrase *startPhrase(int voiceIndex, int animal, float level, bool shot);
   bool hasRunningShot(int voiceIndex) const;
   void spawnCall(const Voice &v, Phrase &ph);
   void spawnCroak(const Frog &f);
   Call *allocateCall();
   CroakVoice *allocateCroak();
   Phrase *allocatePhrase();
   void processCalls(float *outL, float *outR, uint32_t numSamples);
   void processCroaks(float *outL, float *outR, uint32_t numSamples);
   void processBeds(float *outL, float *outR, uint32_t numSamples, float env);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);
   float noteKeyTrack() const;

   EngineParams mP;
   double mSampleRate = 48000.0;

   Voice mVoices[kMaxVoices];
   Call mCalls[kMaxCalls];
   CroakVoice mCroaks[kMaxCroaks];
   Phrase mPhrases[kMaxPhrases];
   Frog mFrogs[kMaxFrogs];
   Insect mInsects[kNumInsects];

   // The bed: one noise source per channel through the measured octave bank,
   // and the two filters that make the ends the bank cannot. An octave-wide
   // bandpass falls at 6 dB per octave and the measured night falls 20 dB
   // between 2.5 and 5 kHz, so without these the bed renders 15 dB too bright
   // at the top however the bank is solved -- measured, at 9.6 dB rms against
   // the curve it was fitted to.
   Svf mBedL[8], mBedR[8];
   Lp2 mBedLpL, mBedLpR;
   Hp2 mBedHpL, mBedHpR;
   float mBedGain[8]{};
   OnePoleLp mBedMotionLp;
   float mBedMotion = 0.0f;
   RngLite mBedRng;

   Rng mRng;
   int mAppliedSeed = 0;
   uint32_t mCallCounter = 0;
   int mLastKey = 60;
   int mFrogCount = 0;
   bool mFrogsReady = false;
   // The chorus clock. Seconds since the engine was reset, advanced at the
   // control rate: the frogs are scheduled against it rather than against
   // timers of their own, because a pond keeps time.
   double mChorusTime = 0.0;

   Space mSpace;
   Svf mOutFilterL, mOutFilterR;
   Hp2 mOutHpL, mOutHpR;

   // Derived once per parameter change rather than per sample.
   float mCallerPitchMul = 1.0f;
   float mCallerRateMul = 1.0f;
   float mCallerClosure = 0.0f;
   float mCallerBreathMul = 1.0f;
   float mFormantHz = 450.0f;
   float mFormantQ = 3.5f;
   float mAirCutoffHz = 20000.0f;
   float mDistanceGain = 1.0f;

   // The envelope the beds follow: the loudest note's, so a second note does
   // not make the night twice as loud.
   float mBedEnv = 0.0f;
   float mSilenceCounter = 0.0f;
};

} // namespace nightlife
