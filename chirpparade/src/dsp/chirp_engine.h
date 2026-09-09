#pragma once

// ChirpParade's synthesis: a syrinx, a tube above it, and birds taking turns.
//
// The starting point is that **a bird is an oscillator held just past its
// bifurcation**. Nothing here is a sine with a pitch envelope on it. The voice
// is the Gardner-Laje-Mindlin model of a syringeal labium
// (Phys. Rev. Lett. 87, 208101; Phys. Rev. E 65, 051921; Phys. Rev. E 72,
// 051926), which is two equations:
//
//     x' = y
//     y' = -eps*x - C*x^2*y + B*y
//
// x is how far the labium has moved from where it sits before phonation. eps is
// the restitution of the tissue, so it sets the frequency: f = sqrt(eps)/2pi.
// B is the *net* dissipation -- the energy the airflow puts in through the
// interlabial pressure, less what the tissue loses -- so B > 0 is the Hopf
// bifurcation and phonation begins there. C is the nonlinear loss that stops
// the labia from passing through each other. The radiated pressure is
// a1*x + a2*x'.
//
// Two things fall out of that, and they are why the model is worth the trouble.
//
//   **A syllable is two gestures.** Zysman et al. show that B is proportional
//   to the air sac pressure and eps to the tension of the syringeal muscle, and
//   that both can be recovered from a recording -- the envelope gives one and
//   the pitch gives the other. Gardner et al. show that syllables "of quite
//   diverse acoustic nature" follow from nothing but the *phase* between the
//   two. So this engine has no shape menu: it has a `Contour` knob, which is
//   that phase, and the up-sweeps, down-sweeps, arches and dips a sonogram
//   reader names are four readings of it. The reference library confirms the
//   ordering -- see tools/analysis/gestures.py.
//
//   **Timbre is one number, and it is not a filter.** Written as a van der Pol
//   the equation has a single shape parameter mu = B/sqrt(eps), the ratio of
//   pressure to tension. Small mu and the labia move almost sinusoidally: the
//   bird whistles, one harmonic, which is 59 % of the library. Large mu and the
//   oscillation goes into relaxation, growing the harmonic stack a crow has,
//   which is the other end and 16 % of it. There is no filter in between,
//   because there is none in the bird.
//
// Above the syrinx: the trachea, a closed tube resonating at c/4L, and the
// beak, which shortens and damps it. In the library's harmonic syllables the
// loudest harmonic is not the first in 81 % of cases and sits at a median
// 1741 Hz, which is a 4.9 cm trachea.
//
// Above that: scheduling, which is most of what makes birds sound like birds.
// Syllables into phrases at a rate of their own, phrases separated by a gap
// 5.9 times the gap inside them, and a flock of individuals each with its own
// pitch, position, distance and voice, calling as a Poisson process and
// answering each other.
//
// And one layer that is not a voice at all: woodpecker drumming, which is
// sonation -- a bill against wood. Measured separately and modelled as an
// excited resonator.
//
// No samples. Nothing here is a recording of a bird.

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
   float sweepOct = 0.35f;
   float contour = 0.25f; // 0..1 of a whole turn
   float turns = 0.25f;
   float lengthSec = 0.096f;
   float skew = 0.37f;
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
   float pitchHz;
   float sweepOct;
   float lengthSec;
   float skew;      // rise / (rise + fall)
   float harmonics; // above -24 dB of the loudest
   float roughDb;   // spectral flatness of the syllable band
   float ratePerMin;
   float contour; // the group's most common shape, as a Contour offset
   float turns;   // measured from how often the group's contours turn
};

// The library-wide medians the table above is relative to.
constexpr float kLibraryPitchHz = 2580.0f;
constexpr float kLibrarySweepOct = 0.35f;
constexpr float kLibraryLengthSec = 0.096f;
constexpr float kLibrarySkew = 0.37f;
constexpr float kLibraryRoughDb = -28.0f;
constexpr float kLibraryRatePerMin = 273.0f;
constexpr float kLibraryTurns = 0.25f;

const SpeciesTraits &speciesTraits(int species);

// One sounding syllable: one turn of the two gestures, and the oscillator they
// drive. This is the only thing in the engine that makes a voiced sound.
struct Chirp {
   bool active = false;
   int voice = -1;

   // The gesture clock, 0 to 1 across the syllable.
   float phase = 0.0f;
   float phaseInc = 0.0f;

   // The syringeal oscillator, in normalised time and in Lienard coordinates:
   // u is the labial displacement over its own limit-cycle scale, and w is
   // v + F(u) rather than the velocity itself. See processChirps for why.
   float u = 0.0f;
   float w = 0.0f;
   float mu = 0.3f;   // peak B/sqrt(eps): the relaxation parameter
   float bth = 0.03f; // where B crosses zero, as a share of the gesture peak
   // The gap the labia sit at before they move. When the oscillation is bigger
   // than this they close on each other and the airflow stops, and that
   // one-sidedness is where every even harmonic comes from.
   float gap0 = 2.0f;
   OnePoleHp dcBlock;

   // The tension gesture, which is the pitch. `contourScale` and
   // `contourAnchor` are worked out once at spawn so that two things are true
   // whatever Contour and Turns are set to: the pitch equals Pitch at the
   // moment the syllable is loudest, and the whole excursion is Sweep octaves.
   // Without them a contour that turns twice swept twice as far as its label
   // said, and the pitch of a syllable was never the pitch of its knob.
   float pitchHz = 2580.0f;
   float contourPhase = 0.0f; // in turns
   float contourScale = 0.0f; // octaves per unit of the gesture
   float contourAnchor = 0.0f;
   float turns = 0.5f;
   float skew = 0.37f;

   float level = 1.0f;
   float pulsePhase = 0.0f;
   float pulseInc = 0.0f;
   float pulseDepth = 0.0f;

   // The tube above the syrinx, and the radiation off the end of it. The
   // second-order lowpass is not decoration: a resonant bandpass falls away at
   // only 6 dB/octave, and with the velocity term tilting the source up by 6,
   // the top of the spectrum ended up being set by the filter's own skirt.
   Svf tract;
   Lp2 top;
   float formant = 0.55f;
   float radiate = 0.35f;
   // The tract, held rather than baked into the filter, because the resonance
   // follows the pitch across the syllable: an open beak tracks it.
   float tractHz = 1741.0f;
   float tractReso = 0.5f;
   float tractTrack = 0.5f;
   uint32_t tractCounter = 0;
   float breath = 0.18f;
   float rasp = 0.0f;
   float feedback = 0.0f; // the tract's back-pressure on the labia
   float tractOut = 0.0f; // last tract output, which is what feeds back

   // Pitch jitter as a random walk rather than per-sample noise: a syrinx
   // drifts, it does not dither.
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
   float mSpeciesSweepMul = 1.0f;
   float mSpeciesLengthMul = 1.0f;
   float mSpeciesRateMul = 1.0f;
   float mSpeciesTurnsMul = 1.0f;
   float mSpeciesSkew = 0.37f;
   float mSpeciesContour = 0.0f;
   float mSpeciesMu = 0.3f;
   float mSpeciesBreathMul = 1.0f;
   float mFormantHz = 1741.0f;
   float mFormantQ = 0.55f;
   float mAirCutoffHz = 20000.0f;
   float mDistanceGain = 1.0f;

   float mSilenceCounter = 0.0f;
};

} // namespace chirpparade
