#include "chirp_engine.h"

#include "verdalis/dsp/fastmath.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

namespace chirpparade {

namespace {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

constexpr float kSpeedOfSoundCmS = 34300.0f;

uint32_t rngStateForSeed(int seed) {
   return 0x9E3779B9u * static_cast<uint32_t>(seed) + 17u;
}

std::atomic<uint32_t> gInstanceCounter{0};

// ------------------------------------------------------------------ species
//
// Measured, not chosen. tools/analysis/species.py groups the reference library
// by what each recording is named and prints exactly this table; these rows are
// its output. Only Screech has no reference behind it -- it is every range at
// once, kept because a plugin for birds should be able to make a noise no bird
// makes -- though not without limit: its first draft asked for eight harmonics
// at 5.2 kHz, which is a fortieth harmonic above Nyquist, and the engine's own
// anti-alias clamp quietly refused. Six at 1.8 kHz is what the band allows, and
// that is what it now asks for.
//
// Raven's harmonic count is the one deviation, and it is deliberate. The
// measurement says 1, over the six syllables of the single raven recording, and
// the estimator reported one harmonic in one frame and nine in the next for
// that same file. A raven is audibly rougher than a crow, so the value here is
// 6 rather than the measurement, and the reason is recorded rather than
// quietly applied.
//
//                         pitch  sweep   len   skew  harm  rough   rate  contour turns
constexpr SpeciesTraits kSpecies[kNumSpecies] = {
   /* Whistler   */ {4748.0f, 0.21f, 0.128f, 0.35f, 1.0f, -34.0f, 288.0f, 0.00f, 0.25f},
   /* Sparrow    */ {3147.0f, 0.34f, 0.096f, 0.40f, 1.0f, -29.0f, 276.0f, 0.00f, 0.25f},
   /* Warbler    */ {1128.0f, 0.37f, 0.087f, 0.43f, 2.0f, -27.0f, 369.0f, 0.00f, 0.25f},
   /* Budgie     */ {1351.0f, 0.47f, 0.099f, 0.26f, 3.0f, -28.0f, 293.0f, 0.00f, 0.25f},
   /* Woodpecker */ {3312.0f, 0.31f, 0.096f, 0.35f, 2.0f, -25.0f, 301.0f, -0.25f, 0.50f},
   /* Crane      */ {982.0f, 0.36f, 0.133f, 0.33f, 4.0f, -26.0f, 140.0f, 0.00f, 0.75f},
   /* Goose      */ {566.0f, 0.51f, 0.206f, 0.60f, 4.0f, -32.0f, 239.0f, 0.50f, 0.50f},
   /* Crow       */ {806.0f, 0.45f, 0.144f, 0.41f, 5.0f, -21.0f, 132.0f, 0.00f, 1.50f},
   /* Raven      */ {1171.0f, 0.38f, 0.267f, 0.42f, 6.0f, -30.0f, 88.0f, -0.25f, 0.50f},
   /* Screech    */ {1800.0f, 1.60f, 0.260f, 0.30f, 6.0f, -16.0f, 150.0f, 0.50f, 2.50f},
};

// How many harmonics a given relaxation parameter produces, and its inverse.
//
// The van der Pol form of the model has one shape parameter, mu = B/sqrt(eps).
// Near zero the labia move sinusoidally and the bird has one harmonic; as mu
// grows the oscillation goes into relaxation, the labia start closing on each
// other, and the stack fills in. Measured on the engine's own output with the
// same estimator that counted the references' harmonics:
//
//     mu          0.15   0.58   1.43   3.52
//     harmonics   1      2      4      9
//
// which is harmonics = 1 + 1.70 mu^1.22 over the whole range. Inverting it is
// what turns a species' *measured* harmonic count into a drive setting, so the
// table stays a measurement and this stays the only place a fit lives.
// tools/analysis/fit.py --voice reprints the sweep above.
constexpr float kHarmonicsScale = 1.70f;
constexpr float kHarmonicsExponent = 1.22f;

// Even a pure whistle needs a mu well above zero. At the bifurcation itself the
// oscillation neither grows nor decays, so a syllable with mu near nothing
// takes hundreds of milliseconds to start and as long again to stop -- the
// first version of this had a whistle with a 400 ms tail, where the references
// measure 41 ms. This floor is what a sinusoidal limit cycle that starts and
// stops inside a 96 ms syllable actually needs.
constexpr float kMuFloor = 0.15f;

// The limit cycle of the normalised oscillator has an amplitude of about 2
// whatever mu is, so one calibration constant turns that into a level: a single
// syllable at Shot Level 0 dB, full velocity and no distance peaks a little
// under -3 dBFS, which leaves a flock of them room before the output stage has
// to saturate.
constexpr float kSyllableNorm = 0.35f;

// The same for a drum strike: a burst of unit amplitude through two broad
// resonators comes out well below where a syllable does, so a roll at Drum
// Level 0 dB sits where a phrase at Shot Level 0 dB does.
constexpr float kStrikeNorm = 3.5f;

// Breath against measured roughness. Rendering a whistle at a sweep of Breath
// and measuring its spectral flatness with the same estimator that measured the
// references gives a straight line in log breath:
//
//     Breath   0     2 %   5 %   10 %  18 %  30 %  50 %
//     rough  -37.4 -36.5 -33.0 -28.1 -23.6 -19.5 -15.3  dB
//
// which is 17.7 dB per decade above about 3 %. That is what lets a species'
// measured roughness be turned back into a Breath setting instead of guessed,
// and it is why the default Breath is 10 %: the library's median roughness is
// -28 dB.
constexpr float kRoughDbPerDecade = 17.7f;

// The relaxation pitch drop, and the compensation for it.
//
// A van der Pol oscillator driven towards relaxation does not only change
// timbre: its period lengthens. That is real physics and not an artefact --
// measured on the engine's own output, with the pitch exact at low drive, the
// frequency falls as
//
//     mu     0.15   0.58   0.69   1.38   3.52
//     f/f0   1.000  0.986  0.975  0.926  0.686
//
// which fits 1 / (1 + 0.043 mu^2) across the whole range (the textbook
// small-mu result is mu^2/16, and this is the same shape with the coefficient
// the engine's own normalisation gives).
//
// It is compensated rather than left in, because `Pitch` has to mean the pitch
// that comes out: an instrument played from a keyboard cannot go a fifth flat
// when a timbre control is turned up. What is *not* compensated is the timbre
// itself, which is the point of the control.
constexpr float kRelaxationPull = 0.043f;

// Where the syllable stops being audible, as a value of the pressure gesture.
// The output amplitude goes as sqrt(B), so -12 dB of amplitude is B/Bmax = 1/16
// and, with the bifurcation at bth, a raw gesture of about 0.16.
constexpr float kAudibleGesture = 0.16f;

inline float muForHarmonics(float harmonics) {
   if (harmonics <= 1.0f)
      return kMuFloor;
   return clampf(std::pow((harmonics - 1.0f) / kHarmonicsScale, 1.0f / kHarmonicsExponent),
                 kMuFloor, 6.0f);
}

// ------------------------------------------------------------------ gestures

// The pressure gesture: how hard the air sac is squeezed across one syllable.
//
// An asymmetric raised cosine, peaking at `skew`. That asymmetry is measured
// rather than stylistic: across the library a syllable rises in 24 ms and falls
// in 41, so its peak sits at 0.37 of its length and not at the middle.
inline float pressureGesture(float s, float skew) {
   if (s <= 0.0f || s >= 1.0f)
      return 0.0f;
   skew = clampf(skew, 0.02f, 0.98f);
   // cos(pi*t) written as sin(2*pi*(0.25 - t/2)), so the shared sine table does
   // the work and there is no call to cosf per sample per syllable.
   const float t = s < skew ? s / skew : (s - skew) / (1.0f - skew);
   const float c = sin2piFast(0.25f - 0.5f * t);
   return s < skew ? 0.5f * (1.0f - c) : 0.5f * (1.0f + c);
}

// Svf::setCutoff takes resonance as 0..1, which it maps to k = 1/Q over
// 2..0.02. Passing a Q straight in silently clamps to maximum resonance, so
// convert properly instead.
inline float resonanceFor(float q) {
   const float k = 1.0f / std::max(0.5f, q);
   return clampf((2.0f - k) / 1.98f, 0.0f, 1.0f);
}

// Equal-power pan from a position in -1..1.
inline void panGains(float pan, float &l, float &r) {
   const float t = 0.25f * (clampf(pan, -1.0f, 1.0f) + 1.0f); // 0..0.5 turns
   l = sin2piFast(0.25f - t);                                 // cos(2*pi*t)
   r = sin2piFast(t);
   l = std::fabs(l);
   r = std::fabs(r);
}

// A small deterministic hash, for anything that has to be the same every time
// without drawing from the engine's generator.
inline uint32_t hash32(uint32_t x) {
   x ^= x >> 16;
   x *= 0x7FEB352Du;
   x ^= x >> 15;
   x *= 0x846CA68Bu;
   x ^= x >> 16;
   return x;
}

inline float hashUnit(uint32_t x) { return (hash32(x) >> 8) * (1.0f / 16777216.0f); }
inline float hashBipolar(uint32_t x) { return hashUnit(x) * 2.0f - 1.0f; }

} // namespace

const SpeciesTraits &speciesTraits(int species) {
   return kSpecies[clampi(species, 0, kNumSpecies - 1)];
}

// ---------------------------------------------------------------- lifecycle

void ChirpEngine::prepare(double sampleRate, uint32_t /*maxBlockSize*/) {
   mSampleRate = sampleRate;
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   mRng.reseed(static_cast<uint32_t>(now) ^
               (0x9E3779B9u * (gInstanceCounter.fetch_add(1) + 1)));
   mAppliedSeed = 0;

   // Birds have almost nothing below 250 Hz -- the lowest fundamental in the
   // library is 292 -- so the tank's loop highpass can sit far higher than
   // wind or thunder want, which keeps the tail out of the sub range where a
   // reverb accumulates rumble.
   mSpace.prepare(static_cast<float>(sampleRate), 90.0f);
   reset();
   updateFilters();
}

void ChirpEngine::reset() {
   for (auto &v : mVoices) {
      v.active = false;
      v.env.reset();
      v.restlessLp.reset();
      v.modCounter = 0;
      v.flockTimer = 0.0;
      v.drumTimer = 0.0;
      v.birdsReady = false;
      v.pendingShot = false;
   }
   for (auto &c : mChirps) {
      c.active = false;
      c.tract.reset();
      c.top.reset();
      c.air.reset();
      c.u = c.w = 0.0f;
      c.tractOut = 0.0f;
      c.dcBlock.reset();
   }
   for (auto &s : mStrikes) {
      s.active = false;
      s.body.reset();
      s.body2.reset();
      s.top.reset();
      s.air.reset();
   }
   for (auto &p : mPhrases)
      p.active = false;

   // A non-zero Seed promises the same birds every time, so starting over has
   // to start the sequence over too. Seed 0 deliberately keeps running.
   if (mP.seed != 0)
      mRng.reseed(rngStateForSeed(mP.seed));
   mSyllableCounter = 0;

   mSpace.clear();
   mOutFilterL.reset();
   mOutFilterR.reset();
   mOutHpL.reset();
   mOutHpR.reset();
   mSilenceCounter = 0.0f;
}

void ChirpEngine::setParams(const EngineParams &p) {
   mP = p;
   if (mP.seed != mAppliedSeed) {
      mAppliedSeed = mP.seed;
      if (mP.seed != 0)
         mRng.reseed(rngStateForSeed(mP.seed));
   }
   updateFilters();
}

void ChirpEngine::updateFilters() {
   const float sr = static_cast<float>(mSampleRate);
   const SpeciesTraits &sp = speciesTraits(mP.species);

   // A species biases the syllable controls rather than replacing them, so
   // everything here is a ratio to the library-wide median.
   mSpeciesPitchMul = sp.pitchHz / kLibraryPitchHz;
   mSpeciesSweepMul = sp.sweepOct / kLibrarySweepOct;
   mSpeciesLengthMul = sp.lengthSec / kLibraryLengthSec;
   mSpeciesRateMul = sp.ratePerMin / kLibraryRatePerMin;
   mSpeciesTurnsMul = sp.turns / kLibraryTurns;
   mSpeciesSkew = sp.skew;
   mSpeciesContour = sp.contour;
   mSpeciesMu = muForHarmonics(sp.harmonics);
   // A species' measured roughness, turned back into a Breath multiplier. Note
   // what this does *not* claim: spectral flatness cannot tell turbulent noise
   // apart from a chaotic source, so what is measured here is carried as noise,
   // which is the conservative reading. The chaotic route is `Rasp`, and it
   // stays a control the presets set by ear rather than something a species
   // applies on the strength of a statistic that cannot see it.
   mSpeciesBreathMul = std::pow(10.0f, (sp.roughDb - kLibraryRoughDb) / kRoughDbPerDecade);

   // The trachea, a tube closed at the syrinx and open at the beak, resonating
   // at c/4L. An open beak shortens the effective tube and damps it, so the
   // resonance rises by up to a fifth and its Q falls.
   const float lengthCm = clampf(mP.tractCm, 0.5f, 40.0f);
   const float beak = clampf(mP.beak, 0.0f, 1.0f);
   mFormantHz = clampf(kSpeedOfSoundCmS / (4.0f * lengthCm) * (1.0f + 0.5f * beak), 100.0f,
                       0.45f * sr);
   mFormantQ = clampf(6.0f - 4.0f * beak, 0.7f, 12.0f);

   // Distance: air absorption, and the inverse-distance loss that comes with
   // it. A bird is a point source, unlike wind or surf, so the level really
   // does fall off with distance rather than only losing its top.
   const float d = clampf(mP.distance, 0.0f, 1.0f);
   const float airKeep = clampf(mP.air, 0.0f, 1.0f);
   mAirCutoffHz = clampf(20000.0f * std::pow(0.05f, d * (1.4f - 0.7f * airKeep)), 400.0f,
                         20000.0f);
   mDistanceGain = 1.0f / (1.0f + 5.0f * d);

   mOutHpL.setCutoff(clampf(mP.highpassHz, 10.0f, 4000.0f), sr);
   mOutHpR.setCutoff(clampf(mP.highpassHz, 10.0f, 4000.0f), sr);

   const float reso = clampf(0.05f + 0.90f * mP.filterReso, 0.0f, 0.98f);
   const float cutoff = clampf(mP.filterCutoffHz * noteKeyTrack(), 20.0f, 0.45f * sr);
   mOutFilterL.setCutoff(cutoff, reso, sr);
   mOutFilterR.setCutoff(cutoff, reso, sr);

   mSpace.setSize(mP.spaceSize);
   mSpace.setDamping(mP.spaceDamping);
   // A wood is an enclosure with no walls: many nearby scatterers and no late
   // build-up, which is the low end of what Space can be told to be.
   mSpace.setEnclosure(0.35f);
}

float ChirpEngine::noteKeyTrack() const {
   if (mP.filterKeyTrack <= 0.0f)
      return 1.0f;
   return std::exp2(mP.filterKeyTrack * (static_cast<float>(mLastKey) - 60.0f) / 12.0f);
}

void ChirpEngine::noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId,
                         double velocity) {
   Voice *free = nullptr;
   for (auto &v : mVoices) {
      if (!v.active) {
         free = &v;
         break;
      }
   }
   if (!free) {
      // Steal the quietest voice, but prefer one that is not still finishing a
      // phrase of its own: a voice held only by a completing shot has an
      // envelope level of zero and would otherwise be the first taken, which
      // would move its birds' pitches in the middle of their own phrase.
      // Syllables already in flight are never touched: they belong to the pool,
      // not to the voice, and cutting one off mid-sweep is the one thing a bird
      // never does.
      float lowest = 1.0e9f;
      for (auto &v : mVoices) {
         if (hasRunningShot(static_cast<int>(&v - mVoices)))
            continue;
         if (v.env.level() < lowest) {
            lowest = v.env.level();
            free = &v;
         }
      }
      if (!free) {
         for (auto &v : mVoices) {
            if (v.env.level() < lowest) {
               lowest = v.env.level();
               free = &v;
            }
         }
      }
   }
   if (!free)
      return;

   Voice &v = *free;
   v.active = true;
   v.port = port;
   v.channel = channel;
   v.key = key;
   v.velocity = clampf(static_cast<float>(velocity), 0.0f, 1.0f);
   v.noteId = noteId;
   mLastKey = key;

   const float sr = static_cast<float>(mSampleRate);
   v.env.setParams(mP.attackSec, mP.decaySec, mP.sustain, mP.releaseSec, sr);
   v.env.gateOn();
   v.restlessLp.reset();
   v.modCounter = 0;
   v.flockTimer = 0.0;
   v.drumTimer = 0.0;

   // The note is a pitch: middle C leaves Pitch alone and every other key
   // transposes it. Velocity moves it as well as the level, because a bird
   // calling harder pushes more air past a tighter syrinx -- and, like Velocity
   // To Level, it is referenced to full velocity, so Pitch means the pitch at
   // velocity 1 rather than the pitch at some value in the middle.
   v.pitchMul = std::exp2(static_cast<float>(key - 60) / 12.0f);

   // Nothing is scheduled here, and nothing is drawn from mRng here. A note can
   // arrive before the parameter events in the same block have been applied --
   // the host sends them in its own order -- so a phrase started now would be
   // built from the previous preset's settings, and the flock would be laid out
   // with the previous Pitch Spread. Both are deferred to the first scheduling
   // tick, by which time the block's parameters are in.
   v.birdsReady = false;
   v.pendingShot = true;
}

void ChirpEngine::noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = (noteId >= 0 && v.noteId == noteId) ||
                         (noteId < 0 && v.port == port && v.channel == channel && v.key == key);
      if (match)
         v.env.gateOff();
   }
}

void ChirpEngine::choke(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = (noteId >= 0 && v.noteId == noteId) ||
                         (noteId < 0 && v.port == port && v.channel == channel && v.key == key);
      if (match) {
         v.env.kill();
         v.active = false;
         for (auto &p : mPhrases)
            if (p.voice == static_cast<int>(&v - mVoices))
               p.active = false;
      }
   }
}

void ChirpEngine::allSoundOff() {
   for (auto &v : mVoices) {
      v.env.kill();
      v.active = false;
   }
   for (auto &p : mPhrases)
      p.active = false;
   for (auto &c : mChirps)
      c.active = false;
   for (auto &s : mStrikes)
      s.active = false;
}

// ------------------------------------------------------------------- pools

Chirp *ChirpEngine::allocateChirp() {
   uint32_t live = 0;
   Chirp *free = nullptr;
   Chirp *oldest = nullptr;
   float furthest = -1.0f;
   for (auto &c : mChirps) {
      if (!c.active) {
         if (!free)
            free = &c;
         continue;
      }
      ++live;
      if (c.phase > furthest) {
         furthest = c.phase;
         oldest = &c;
      }
   }
   const uint32_t limit = static_cast<uint32_t>(clampi(mP.maxVoices, 1, kMaxChirps));
   if (live >= limit || !free) {
      // At the ceiling, take the syllable closest to finishing. Stealing the
      // quietest instead was tried and is worse: it interrupts something in
      // the middle of its sweep, which is audible, where the end of a syllable
      // is already fading.
      if (!oldest)
         return nullptr;
      free = oldest;
   }
   free->active = false;
   free->tract.reset();
   free->top.reset();
   free->air.reset();
   free->u = 0.0f;
   free->w = 0.0f;
   free->tractOut = 0.0f;
   free->dcBlock.reset();
   free->walk = 0.0f;
   free->phase = 0.0f;
   return free;
}

Strike *ChirpEngine::allocateStrike() {
   for (auto &s : mStrikes) {
      if (!s.active) {
         s.body.reset();
         s.body2.reset();
         s.top.reset();
         s.air.reset();
         return &s;
      }
   }
   return nullptr;
}

bool ChirpEngine::hasRunningShot(int voiceIndex) const {
   for (const auto &p : mPhrases)
      if (p.active && p.shot && p.voice == voiceIndex)
         return true;
   return false;
}

Phrase *ChirpEngine::allocatePhrase() {
   for (auto &p : mPhrases)
      if (!p.active)
         return &p;
   return nullptr;
}

// ------------------------------------------------------------------- flock

void ChirpEngine::configureBirds(Voice &v) {
   v.birdsReady = true;
   v.birdCount = clampi(mP.birds, 1, static_cast<int>(kMaxBirds));
   const uint32_t slot = static_cast<uint32_t>(&v - mVoices);
   const uint32_t base = hash32(slot * 2654435761u + static_cast<uint32_t>(v.key) * 40503u +
                                static_cast<uint32_t>(mP.seed) * 2246822519u);

   for (int i = 0; i < v.birdCount; ++i) {
      Bird &b = v.birds[i];
      const uint32_t h = base + static_cast<uint32_t>(i) * 0x9E3779B9u;
      // Pitch: spread over the range, but not evenly -- a flock has a couple of
      // outliers and a cluster, which is what a hashed draw gives and an even
      // division does not.
      b.pitchOct = 0.5f * mP.pitchSpreadOct * hashBipolar(h + 1u);
      // Position: spread across the width, with the first bird in the middle so
      // that a flock of one is not off to one side.
      b.pan = v.birdCount == 1 ? 0.0f : hashBipolar(h + 2u);
      b.distance = hashBipolar(h + 3u);
      b.voiceOff = hashBipolar(h + 4u);
      b.lengthMul = std::exp2(0.6f * mP.voiceSpread * hashBipolar(h + 5u));
      b.sweepMul = std::exp2(0.8f * mP.voiceSpread * hashBipolar(h + 6u));
      b.contourOff = 0.35f * mP.voiceSpread * hashBipolar(h + 7u);
      b.rateMul = std::exp2(0.7f * mP.voiceSpread * hashBipolar(h + 8u));
      // Nearer birds are louder, and that is already in the distance gain; this
      // is the rest of it -- birds are not equally loud.
      b.levelMul = std::exp2(-0.5f * mP.voiceSpread * hashUnit(h + 9u));
   }
}

Phrase *ChirpEngine::startPhrase(int voiceIndex, int bird, float level, bool drum,
                                 bool shot) {
   Phrase *p = allocatePhrase();
   if (!p)
      return nullptr;
   const Voice &v = mVoices[voiceIndex];
   const Bird &b = v.birds[clampi(bird, 0, static_cast<int>(kMaxBirds) - 1)];

   p->active = true;
   p->drum = drum;
   p->shot = shot;
   p->voice = voiceIndex;
   p->bird = bird;
   p->index = 0;
   p->timer = 0.0;
   p->level = level;
   p->rng.seed(mRng.next() | 1u);

   if (drum) {
      p->syllables = clampi(mP.strikes, 2, 64);
      p->remaining = p->syllables;
      p->repeatsLeft = 0;
      p->interval = 1.0f / clampf(mP.strikeRate, 0.5f, 60.0f);
      // Accelerate: measured. 22 of the library's 35 rolls speed up, and the
      // last third of a roll runs at 0.79 of the interval of the first, so a
      // roll is not a metronome that fades.
      const float total = clampf(1.0f - mP.drumAccel, 0.15f, 4.0f);
      p->driftMul = std::pow(total, 1.0f / std::max(1.0f, static_cast<float>(p->syllables - 1)));
      p->gapAfter = 0.0f;
      p->pitchHz = clampf(mP.knockHz, 80.0f, 12000.0f);
      p->motifSemis = 0.0f;
      p->variation = clampf(mP.variation, 0.0f, 1.0f);
      return p;
   }

   p->syllables = clampi(mP.syllables, 1, 64);
   p->remaining = p->syllables;
   p->repeatsLeft = clampi(mP.repeats, 1, 16) - 1;
   const float rate = clampf(mP.syllableRate * b.rateMul, 0.2f, 60.0f);
   p->interval = 1.0f / rate;
   const float total = clampf(1.0f - mP.rateDrift, 0.15f, 4.0f);
   p->driftMul = std::pow(total, 1.0f / std::max(1.0f, static_cast<float>(p->syllables - 1)));
   p->gapAfter = clampf(mP.phraseGapSec, 0.005f, 60.0f);
   p->pitchHz = clampf(mP.pitchHz * mSpeciesPitchMul * v.pitchMul * std::exp2(b.pitchOct) *
                          std::exp2(mP.velToPitch * (v.velocity - 1.0f)),
                       40.0f, 18000.0f);
   p->motifSemis = mP.motifSemis;
   p->variation = clampf(mP.variation, 0.0f, 1.0f);
   return p;
}

void ChirpEngine::schedule(Voice &v, float env, double blockSec) {
   const int slot = static_cast<int>(&v - mVoices);
   if (!v.birdsReady)
      configureBirds(v);

   if (v.pendingShot) {
      v.pendingShot = false;
      if (mP.shotGain > 1.0e-4f)
         startPhrase(slot, 0, mP.shotGain * env, false, true);
      // The woodpecker answers a note as well as drumming by itself, which is
      // how a single roll gets placed exactly where it is wanted: turn Drum
      // Rate to zero and every note is one roll.
      if (mP.drumGain > 1.0e-4f) {
         Phrase *p = startPhrase(slot, v.birdCount > 1 ? 1 : 0, mP.drumGain * env, true, true);
         if (p)
            p->timer = 0.02;
      }
   }

   // The flock. A Poisson process rather than a clock: the waiting time between
   // calls is exponential, which is what makes a soundscape sound unplanned.
   if (mP.flockGain > 1.0e-4f && mP.flockRatePerMin > 0.01f) {
      // Restless: a slow drift of the density. No dawn chorus is uniform.
      const float ctrl = static_cast<float>(mSampleRate) / static_cast<float>(kModInterval);
      v.restlessLp.setCoef(onePoleCoef(2.5f, ctrl));
      const float drift = v.restlessLp.tick(mRng.white());
      const float rateMul = std::exp2(2.0f * clampf(mP.restless, 0.0f, 1.0f) * drift);
      // Flock Rate is syllables a minute, as measured, so the rate at which
      // *phrases* start is that divided by the syllables in one.
      const float syl = static_cast<float>(clampi(mP.syllables, 1, 64));
      const float rate =
         clampf(mP.flockRatePerMin * mSpeciesRateMul * rateMul / (60.0f * syl), 0.001f, 60.0f);
      v.flockTimer -= blockSec;
      if (v.flockTimer <= 0.0) {
         v.flockTimer = static_cast<double>(mRng.exponential(rate));
         const int bird = static_cast<int>(mRng.uniform() * static_cast<float>(v.birdCount));
         startPhrase(slot, clampi(bird, 0, v.birdCount - 1), mP.flockGain * env, false, false);
         // Answer: another bird replies from somewhere else, shortly after.
         // Real birds do this, and it is most of what makes a flock sound like
         // a conversation rather than a random process.
         if (v.birdCount > 1 && mRng.uniform() < clampf(mP.answer, 0.0f, 1.0f)) {
            int other = static_cast<int>(mRng.uniform() * static_cast<float>(v.birdCount));
            if (other == bird)
               other = (other + 1) % v.birdCount;
            Phrase *p = startPhrase(slot, other, mP.flockGain * env * 0.85f, false, false);
            if (p)
               p->timer = 0.15 + 0.45 * static_cast<double>(mRng.uniform());
         }
      }
   }

   if (mP.drumGain > 1.0e-4f && mP.drumRatePerMin > 0.01f) {
      v.drumTimer -= blockSec;
      if (v.drumTimer <= 0.0) {
         const float rate = clampf(mP.drumRatePerMin / 60.0f, 0.001f, 10.0f);
         v.drumTimer = static_cast<double>(mRng.exponential(rate));
         const int bird = static_cast<int>(mRng.uniform() * static_cast<float>(v.birdCount));
         startPhrase(slot, clampi(bird, 0, v.birdCount - 1), mP.drumGain * env, true, false);
      }
   }
}

// ------------------------------------------------------------------ spawning

void ChirpEngine::spawnChirp(const Voice &v, Phrase &ph) {
   Chirp *chirp = allocateChirp();
   if (!chirp)
      return;
   Chirp &c = *chirp;
   const Bird &b = v.birds[clampi(ph.bird, 0, static_cast<int>(kMaxBirds) - 1)];
   const float sr = static_cast<float>(mSampleRate);
   const float var = ph.variation;

   // Drawn once per syllable, from the phrase's own generator, so that a
   // phrase is reproducible from its seed however many other birds are
   // sounding at the same time.
   const float rp = ph.rng.white();
   const float rl = ph.rng.white();
   const float rc = ph.rng.white();
   const float rs = ph.rng.white();
   const float rv = ph.rng.white();

   // Motif: the pitch steps by a fixed interval from one syllable to the next,
   // which is what turns a repeated syllable into a figure.
   const float motif = ph.motifSemis * static_cast<float>(ph.index);
   c.pitchHz = clampf(ph.pitchHz * std::exp2((motif + 4.0f * var * rp) / 12.0f), 40.0f,
                      0.45f * sr);

   float len = mP.lengthSec * mSpeciesLengthMul * b.lengthMul *
               std::exp2(0.7f * var * rl);
   // Legato: the syllable is stretched towards filling its own slot. 70 % of
   // the library's syllable pairs have no silence between them at all, so this
   // defaults high and a phrase of separated notes is the exception. It only
   // applies where there is something to run into: stretching the last syllable
   // of a phrase towards an interval that is never used made a one-syllable
   // call 50 % longer than its Length said.
   const float leg = ph.remaining > 1 ? clampf(mP.legato, 0.0f, 1.0f) : 0.0f;
   len = len * (1.0f - leg) + ph.interval * leg;
   len = clampf(len, 0.004f, 8.0f);
   c.phaseInc = 1.0f / (len * sr);

   const float sweep =
      clampf(mP.sweepOct * mSpeciesSweepMul * b.sweepMul * std::exp2(0.6f * var * rs), 0.0f, 6.0f);
   c.contourPhase = mP.contour + mSpeciesContour + b.contourOff + 0.2f * var * rc;
   c.turns = clampf(mP.turns * mSpeciesTurnsMul, 0.05f, 12.0f);
   c.skew = clampf(mP.skew * (mSpeciesSkew / kLibrarySkew), 0.05f, 0.95f);

   // The gesture is a sinusoid over the syllable, and its raw excursion depends
   // on Turns as well as Sweep -- a quarter turn covers part of a cycle, two
   // turns cover all of it twice. So it is measured here, over the syllable, and
   // scaled to the Sweep asked for, and offset so that the pitch at the peak of
   // the pressure gesture is the Pitch asked for.
   //
   // Measured over the *audible* part of the syllable, not all of it. A
   // syllable's onset and offset ramps carry pitch that nobody hears, and
   // including them made the excursion that a measurement of the output reports
   // about 0.7 of the Sweep asked for. The window is where the pressure gesture
   // keeps the output within 12 dB of its peak, which is the same window the
   // analysis measures a reference syllable's pitch range over.
   //
   // 33 samples is exact enough for both: the gesture has at most a dozen turns
   // in it.
   float lo = 1.0f, hi = -1.0f;
   for (int i = 0; i <= 32; ++i) {
      const float t = static_cast<float>(i) / 32.0f;
      if (pressureGesture(t, c.skew) < kAudibleGesture)
         continue;
      const float g = sin2piFast(c.turns * t + c.contourPhase);
      lo = std::min(lo, g);
      hi = std::max(hi, g);
   }
   if (hi < lo) { // a skew so extreme that no sample landed inside the window
      lo = -1.0f;
      hi = 1.0f;
   }
   c.contourAnchor = sin2piFast(c.turns * c.skew + c.contourPhase);
   c.contourScale = sweep / std::max(hi - lo, 1.0e-3f);
   // Where B crosses zero. Not near zero: B is the *net* dissipation, so below
   // the bifurcation it is the tissue's own passive loss, which is a real
   // fraction of the peak drive and not an epsilon. It is also what sets the
   // syllable's release -- at 0.03 the tail ran ten times longer than the
   // measured 41 ms.
   c.bth = 0.10f;

   // The relaxation parameter: the species' measured richness, moved by Voice
   // and spread across the flock.
   float mu = mSpeciesMu * std::exp2(6.5f * (clampf(mP.voice, 0.0f, 1.0f) - 0.30f)) *
              std::exp2(2.0f * mP.voiceSpread * b.voiceOff);
   mu = std::max(mu, kMuFloor);
   // A syrinx cannot sustain a relaxation oscillation at any frequency: the
   // higher it sings the closer the labia are to sinusoidal. Which is also
   // exactly the clamp that keeps the harmonics inside the band -- a whistle at
   // 6 kHz with a crow's mu would alias, and a real bird does not do it either.
   const float dtauNom = 6.2831853f * c.pitchHz / sr;
   c.mu = clampf(mu, kMuFloor, 0.55f / std::max(dtauNom, 1.0e-3f));

   // Ask the oscillator for a higher frequency than is wanted, by exactly what
   // the relaxation regime will take away again.
   c.pitchHz = clampf(c.pitchHz * (1.0f + kRelaxationPull * c.mu * c.mu), 40.0f, 0.45f * sr);

   // How far apart the labia sit before they move, against an oscillation whose
   // own amplitude is 2. Driving the syrinx harder both raises B/sqrt(eps) and
   // adducts the labia further, so the two move together and this follows Voice
   // rather than taking a control of its own: a whistle barely closes, and a
   // crow is shut for most of every cycle.
   c.gap0 = 2.0f * (1.0f - 0.85f * clampf((c.mu - kMuFloor) * 0.5f, 0.0f, 1.0f));
   c.dcBlock.reset();
   c.dcBlock.setCutoff(150.0f, sr);

   c.jitter = clampf(mP.jitter, 0.0f, 1.0f) * 0.06f;
   // A syrinx drifts rather than dithers, so the jitter is a random walk with a
   // corner a few tens of hertz up, not per-sample noise.
   c.walkCoef = onePoleCoef(0.006f, sr);
   c.walk = 0.0f;

   c.pulseDepth = clampf(mP.pulseDepth, 0.0f, 1.0f);
   c.pulseInc = clampf(mP.pulseRateHz, 0.1f, 400.0f) / sr;
   c.pulsePhase = 0.25f * ph.rng.uniformPositive();

   c.breath = clampf(mP.breath * mSpeciesBreathMul, 0.0f, 1.0f);
   c.formant = clampf(mP.formant, 0.0f, 1.0f);
   c.radiate = clampf(mP.radiate, 0.0f, 1.0f);
   const float rasp = clampf(mP.rasp, 0.0f, 1.0f);
   c.rasp = rasp;
   // The trachea's back-pressure on the labia. Source-tract coupling is the
   // documented route to period doubling and chaos in birdsong, and it is what
   // a corvid's rasp actually is: no amount of harmonics alone reaches the 9 dB
   // of extra spectral flatness the roughest references measure.
   c.feedback = rasp * rasp * 1.6f * (0.4f + 0.6f * (1.0f + rv) * 0.5f);

   c.tractHz = clampf(mFormantHz, 60.0f, 0.45f * sr);
   c.tractReso = resonanceFor(mFormantQ);
   c.tractTrack = clampf(mP.beak, 0.0f, 1.0f);
   c.tractCounter = 0;
   c.tract.setCutoff(c.tractHz, c.tractReso, sr);
   // A resonant bandpass falls away at only 6 dB/octave and the velocity term
   // tilts the source up by another 6, so without this the top of the spectrum
   // was the filter's own skirt rather than the bird. Placed relative to the
   // fundamental and to how rich the voice is, so a whistle is closed down just
   // above its own pitch and a crow keeps its stack.
   c.top.setCutoff(clampf(c.pitchHz * (2.5f + 9.0f * c.mu), 800.0f, 0.45f * sr), sr);

   const float d = clampf(mP.distance + 0.5f * mP.distanceSpread * b.distance, 0.0f, 1.0f);
   c.air.setCutoff(clampf(20000.0f * std::pow(0.05f, d * (1.4f - 0.7f * mP.air)), 400.0f,
                          20000.0f),
                   sr);
   const float distGain = 1.0f / (1.0f + 5.0f * d);

   panGains(b.pan * clampf(mP.width, 0.0f, 1.0f), c.panL, c.panR);

   const float vel = 1.0f - mP.velToLevel * (1.0f - v.velocity);
   c.level = ph.level * b.levelMul * distGain * vel * kSyllableNorm;
   c.voice = static_cast<int>(&v - mVoices);
   c.rng.seed(ph.rng.next() | 1u);
   c.active = true;
   ++mSyllableCounter;
}

void ChirpEngine::spawnStrike(const Voice &v, Phrase &ph) {
   Strike *strike = allocateStrike();
   if (!strike)
      return;
   Strike &s = *strike;
   const Bird &b = v.birds[clampi(ph.bird, 0, static_cast<int>(kMaxBirds) - 1)];
   const float sr = static_cast<float>(mSampleRate);

   // Wood has more than one mode, and a struck branch is not a filter sweep:
   // two bands an octave and a bit apart, the second weaker, is what the
   // measured strike spectra look like -- a centroid at 1251 Hz over a
   // bandwidth of about 935.
   const float knock = clampf(ph.pitchHz * std::exp2(0.25f * ph.variation * ph.rng.white()),
                              80.0f, 0.4f * sr);
   s.body.setCutoff(knock, resonanceFor(3.2f), sr);
   s.body2.setCutoff(clampf(knock * 2.3f, 80.0f, 0.45f * sr), resonanceFor(2.2f), sr);
   s.top.reset();
   s.top.setCutoff(clampf(knock * 2.2f, 200.0f, 0.45f * sr), sr);

   // Ring is what the excitation lasts, not what the resonator does with it,
   // and that distinction was worth measuring. The strike spectra have a
   // bandwidth of about 935 Hz at a centroid of 1251, which is a Q near 1.3 --
   // a resonator that broad has rung out in a fifth of a millisecond. Yet the
   // same strikes take a measured 8 ms to fall 20 dB. Both are true: the body
   // is broad and the contact is not instantaneous. So the resonators stay
   // broad and Ring is the length of the burst through them. Setting it on the
   // resonators instead gave a 1 ms knock however Ring was turned.
   const float ring = clampf(mP.ringSec, 0.0002f, 1.0f);
   s.exciteCoef = decayCoef(ring * 3.0f, sr); // to -60 dB, from a -20 dB figure
   s.excite = 1.0f;

   s.air.setCutoff(clampf(20000.0f * std::pow(0.05f, clampf(mP.distance, 0.0f, 1.0f) *
                                                        (1.4f - 0.7f * mP.air)),
                          400.0f, 20000.0f),
                   sr);
   const float d = clampf(mP.distance + 0.5f * mP.distanceSpread * b.distance, 0.0f, 1.0f);
   panGains(b.pan * clampf(mP.width, 0.0f, 1.0f), s.panL, s.panR);
   const float vel = 1.0f - mP.velToLevel * (1.0f - v.velocity);
   s.level = ph.level * vel / (1.0f + 5.0f * d);
   s.voice = static_cast<int>(&v - mVoices);
   s.rng.seed(ph.rng.next() | 1u);
   s.active = true;
}

// ------------------------------------------------------------------ process

void ChirpEngine::processChirps(float *outL, float *outR, uint32_t numSamples) {
   const float sr = static_cast<float>(mSampleRate);
   const float invSr = 1.0f / sr;

   for (auto &c : mChirps) {
      if (!c.active)
         continue;
      for (uint32_t i = 0; i < numSamples; ++i) {
         const float s = c.phase;

         // ------------------------------------------------- the two gestures
         //
         // Everything about the syllable is these two lines. The pressure
         // gesture is the envelope and the switch: B crosses zero at `bth`, and
         // that crossing is the Hopf bifurcation, so the syllable begins and
         // ends by itself rather than being gated. The tension gesture is the
         // pitch, and the phase between the two is the syllable's shape.
         const float p = pressureGesture(s, c.skew);
         const float drive = (p - c.bth) / (1.0f - c.bth);

         c.walk += c.walkCoef * (c.jitter * c.rng.white() - c.walk);
         const float g = sin2piFast(c.turns * s + c.contourPhase);
         const float f0 = c.pitchHz * std::exp2(c.contourScale * (g - c.contourAnchor) + c.walk);

         // ---------------------------------------------- the discretisation
         //
         // The oscillator has to run at exactly the frequency the tension
         // gesture asks for, and getting that right took two goes.
         //
         // Written as it stands in the paper, the nonlinear term is a *scaling*
         // of the velocity. Discretised that way it is a shear rather than a
         // rotation, and a shear moves the frequency as well as the amplitude.
         // Nor does the shift average out over a cycle: around the limit cycle
         // the mean of (1 - u^2/h) is -1, not zero. A 4.7 kHz whistle came out
         // four and a half per cent flat, and a hard-driven crow was out by
         // eighty.
         //
         // The fix is to change coordinates, not the equation. In Lienard form,
         // with
         //
         //     F(u) = mu (u^3/3h - u),      w = v + F(u)
         //
         // the same system reads
         //
         //     du/dtau = w - F(u),      dw/dtau = -u
         //
         // and the nonlinearity has become an *additive* term. An additive term
         // forces the oscillator; it does not shear it, so the rotation keeps
         // its own frequency, which the magic-circle step size
         // d = 2 sin(pi f / sr) makes exactly 2 pi f / sr for any f under
         // Nyquist. The velocity, where it is needed for the radiated
         // pressure, is w - F(u).
         const float wturn = clampf(f0 * invSr, 1.0e-5f, 0.49f);

         // ------------------------------------------- the labial oscillation
         //
         //   x' = y ;  y' = -eps x - C x^2 y + B y
         //
         // in normalised time, with the amplitude scaled by its own limit cycle
         // so that u is the displacement over sqrt(B/C) and the limit amplitude
         // is 2 sqrt(B/Bmax). The envelope therefore comes out of the equation
         // rather than being multiplied on afterwards, and a syllable starts and
         // stops at the bifurcation the way a bird's does.
         const float muInst = c.mu * drive;
         const float h = drive > 1.0e-3f ? drive : 1.0e-3f;
         const float inv3h = 1.0f / (3.0f * h);

         // The additive term is integrated explicitly, so its slope has to stay
         // inside the step: |dF/du| reaches 3*muInst at the limit cycle. One
         // substep is enough for most of the range and a hard-driven low voice
         // takes a handful, which is where the cost belongs -- the whistles that
         // need the smallest steps are the ones with the least nonlinearity.
         const float dNom = 6.2831853f * wturn;
         int nsub = 1 + static_cast<int>(12.0f * dNom * c.mu);
         if (nsub > 8)
            nsub = 8;
         const float dsub = 2.0f * sin2piFast(0.5f * wturn / static_cast<float>(nsub));

         float velocity = 0.0f;
         for (int k = 0; k < nsub; ++k) {
            const float uu = c.u;
            // Below the bifurcation the labia are simply damped, and the cubic
            // term is irrelevant at the amplitude they have left -- as well as
            // being a division by a vanishing h.
            const float F = drive > 0.0f ? muInst * (uu * uu * uu * inv3h - uu) : -muInst * uu;
            velocity = c.w - clampf(F, -16.0f, 16.0f);
            c.u += dsub * velocity;
            c.w -= dsub * c.u;
         }

         const float noise = c.rng.white();
         // Turbulent air past the labia. It is injected into the oscillator
         // rather than added to its output because that is where it is: it is
         // also what starts the oscillation, which is why the onset of a
         // syllable is never twice the same. The floor is deliberate -- the
         // labia are never perfectly still, and with none of this a syllable
         // with Breath at zero would never leave equilibrium at all.
         c.w += dNom * (0.004f + 0.30f * c.breath) * (0.05f + p) * noise;
         // The trachea pushing back on the labia.
         c.w += dNom * c.feedback * c.tractOut;

         // The nonlinear loss is strongly stabilising, but a parameter jump
         // mid-syllable can still put the state somewhere silly, and this is a
         // pool slot that will be reused.
         c.u = clampf(c.u, -8.0f, 8.0f);
         c.w = clampf(c.w, -16.0f, 16.0f);

         // The tract resonance follows the pitch, by as much as the beak is
         // open. Songbirds track the frequency they are producing with their
         // beak gape, and an open beak shortens the effective tube, so a
         // resonance that stays put while the fundamental sweeps past it is
         // wrong twice: it is not what a bird does, and it hands the loudest
         // partial from one harmonic to the next in the middle of a syllable.
         // Updated every 16 samples -- a formant does not need a tan() a
         // sample, and a syllable is thousands of them.
         if ((c.tractCounter++ & 15u) == 0u) {
            const float track = c.tractTrack * std::log2(std::max(f0, 20.0f) / c.pitchHz);
            c.tract.setCutoff(clampf(c.tractHz * std::exp2(track), 60.0f, 0.45f * sr),
                              c.tractReso, sr);
         }

         // ---------------------------------------------------- the airflow
         //
         // The source is not the labial displacement. It is the air that gets
         // past, and air only gets past while the labia are apart -- so the
         // flow is the *one-sided* part of the gap, and it stops dead for the
         // fraction of every cycle in which they are closed.
         //
         // That is not a detail. The equation above is odd-symmetric: u -> -u,
         // v -> -v leaves it unchanged, because the nonlinear loss goes as u^2.
         // An odd-symmetric oscillator has only odd harmonics, and taking its
         // displacement as the output gives a spectrum at f, 3f, 5f with
         // nothing between -- which is not a bird, and is why the first version
         // of this could not make a crow however hard it was driven. Zysman et
         // al. say as much in as many words: "more realistic models for this
         // force lead to signals with different harmonic contents", and note
         // that a richer model is needed for the species with a wide timbre.
         // Rectifying at the point of closure is that model, and it is the same
         // step that makes a glottal pulse rich rather than sinusoidal.
         //
         // The radiated pressure of a small source follows the rate of change
         // of the flow rather than the flow, so `Radiate` mixes the two.
         const float gap = c.gap0 + c.u;
         const float open = gap > 0.0f ? 1.0f : 0.0f;
         float x = (1.0f - c.radiate) * (open * gap) + c.radiate * (open * velocity);
         // A one-sided flow has a mean, and that mean is modulated at the
         // syllable rate, which is a thump rather than a bird. It does not
         // radiate in the first place, so it goes here.
         x = c.dcBlock.tick(x);
         x += c.breath * 0.9f * p * noise;
         const float bp = c.tract.bandpassNormalised(x);
         c.tractOut = bp;
         // A resonance adds to the source, it does not replace it. Crossfading
         // into the bandpass was tried first and is wrong twice over: it loses
         // 7 dB whenever the fundamental is nowhere near the formant, and it
         // takes the source away instead of colouring it.
         x += c.formant * 1.2f * bp;
         x = c.top.tick(x);
         x = c.air.tick(x);

         c.pulsePhase += c.pulseInc;
         if (c.pulsePhase >= 1.0f)
            c.pulsePhase -= 1.0f;
         const float pulse =
            1.0f - c.pulseDepth * 0.5f * (1.0f - sin2piFast(c.pulsePhase + 0.25f));

         const float amp = c.level * pulse;
         outL[i] += x * amp * c.panL;
         outR[i] += x * amp * c.panR;

         c.phase += c.phaseInc;
         if (c.phase >= 1.0f) {
            // The gesture is over, but the labia are still moving: a syllable
            // has a tail of its own and cutting it off clicks. Freed when the
            // oscillator has actually stopped, or after one more syllable's
            // worth of time, whichever comes first.
            if ((std::fabs(c.u) + std::fabs(c.w)) < 2.0e-4f || c.phase > 1.25f) {
               c.active = false;
               break;
            }
         }
      }
   }
}

void ChirpEngine::processStrikes(float *outL, float *outR, uint32_t numSamples) {
   for (auto &s : mStrikes) {
      if (!s.active)
         continue;
      for (uint32_t i = 0; i < numSamples; ++i) {
         s.excite *= s.exciteCoef;
         const float in = s.excite * s.rng.white();
         float x = s.body.bandpassNormalised(in) + 0.45f * s.body2.bandpassNormalised(in);
         x = s.air.tick(s.top.tick(x)) * s.level * kStrikeNorm;
         outL[i] += x * s.panL;
         outR[i] += x * s.panR;
         if (s.excite < 1.0e-5f && !s.body.ringing(1.0e-6f)) {
            s.active = false;
            break;
         }
      }
   }
}

void ChirpEngine::processOutputChain(float *outL, float *outR, uint32_t numSamples) {
   const float wet = clampf(mP.spaceAmount, 0.0f, 1.0f);
   for (uint32_t i = 0; i < numSamples; ++i) {
      float l = outL[i], r = outR[i];

      if (wet > 0.001f) {
         float wl = 0.0f, wr = 0.0f;
         mSpace.tick(l, r, wl, wr);
         l = l * (1.0f - wet * 0.5f) + wl * wet;
         r = r * (1.0f - wet * 0.5f) + wr * wet;
      }

      switch (mP.filterType) {
      case 1:
         l = mOutFilterL.bandpassNormalised(l);
         r = mOutFilterR.bandpassNormalised(r);
         break;
      case 2: {
         float lp, bp, hp;
         mOutFilterL.tick(l, lp, bp, hp);
         l = hp;
         mOutFilterR.tick(r, lp, bp, hp);
         r = hp;
         break;
      }
      case 3:
         l = mOutFilterL.notch(l);
         r = mOutFilterR.notch(r);
         break;
      default:
         l = mOutFilterL.lowpass(l);
         r = mOutFilterR.lowpass(r);
         break;
      }

      l = mOutHpL.tick(l);
      r = mOutHpR.tick(r);

      // Saturate rather than clip. A dawn chorus with sixteen birds and every
      // layer up can run past the ceiling, and birds should not crackle when
      // it does.
      outL[i] = softClip(l * mP.gain);
      outR[i] = softClip(r * mP.gain);
   }
}

void ChirpEngine::process(float *outL, float *outR, uint32_t numSamples) {
   std::fill(outL, outL + numSamples, 0.0f);
   std::fill(outR, outR + numSamples, 0.0f);

   const float sr = static_cast<float>(mSampleRate);
   const double blockSec = static_cast<double>(kModInterval) / mSampleRate;

   // Scheduling runs at the control rate, which is 0.67 ms at 48 kHz: fine
   // enough for syllables at forty a second and strikes at fifty, and 32 times
   // cheaper than doing it per sample.
   uint32_t done = 0;
   while (done < numSamples) {
      for (auto &v : mVoices) {
         if (!v.active)
            continue;
         float env = 0.0f;
         for (uint32_t k = 0; k < kModInterval; ++k)
            env = v.env.tick();
         schedule(v, env, blockSec);
         if (v.finished()) {
            // The flock is a bed and stops with the envelope. A phrase a note
            // asked for is not: it finishes, however short the note was, which
            // is what makes a note usable as a one shot -- a 200 ms note on a
            // fourteen-syllable laugh used to get ten of them. The voice is
            // held until it does, so that its birds keep their pitches and it
            // is not stolen out from under its own phrase.
            const int slot = static_cast<int>(&v - mVoices);
            bool shotRunning = false;
            for (auto &p : mPhrases) {
               if (!p.active || p.voice != slot)
                  continue;
               if (p.shot)
                  shotRunning = true;
               else
                  p.active = false;
            }
            if (!shotRunning)
               v.active = false;
         }
      }

      // Phrases: silent themselves, they only decide when something sounds.
      for (auto &p : mPhrases) {
         if (!p.active)
            continue;
         const Voice &v = mVoices[clampi(p.voice, 0, static_cast<int>(kMaxVoices) - 1)];
         p.timer -= blockSec;
         while (p.timer <= 0.0 && p.active) {
            if (p.drum)
               spawnStrike(v, p);
            else
               spawnChirp(v, p);
            ++p.index;
            --p.remaining;
            p.interval *= p.driftMul;
            if (p.remaining <= 0) {
               if (p.repeatsLeft > 0) {
                  --p.repeatsLeft;
                  p.remaining = p.syllables;
                  p.index = 0;
                  p.timer += static_cast<double>(p.gapAfter);
               } else {
                  p.active = false;
               }
            } else {
               p.timer += static_cast<double>(clampf(p.interval, 0.002f, 30.0f));
            }
         }
      }
      done += kModInterval;
   }

   processChirps(outL, outR, numSamples);
   processStrikes(outL, outR, numSamples);
   processOutputChain(outL, outR, numSamples);

   // Silence tracking, so the host can sleep once the last bird has finished.
   float peak = 0.0f;
   for (uint32_t i = 0; i < numSamples; ++i)
      peak = std::max(peak, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
   if (peak < 1.0e-6f)
      mSilenceCounter += numSamples / sr;
   else
      mSilenceCounter = 0.0f;
}

void ChirpEngine::triggerShot() {
   // The version label in the header, which is a free bird: a phrase with no
   // note behind it. It needs a voice to hang off -- the birds, the pitch and
   // the pan all live there -- so the quietest one is borrowed and released
   // again as soon as its syllables have finished.
   Voice *use = nullptr;
   for (auto &v : mVoices) {
      if (v.active) {
         use = &v;
         break;
      }
   }
   if (!use) {
      use = &mVoices[0];
      use->active = true;
      use->key = 60;
      use->noteId = -1;
      use->velocity = 0.85f;
      use->pitchMul = 1.0f;
      use->birdsReady = false;
      use->pendingShot = false;
      use->flockTimer = 1.0e9;
      use->drumTimer = 1.0e9;
      use->env.setParams(0.001f, 0.05f, 0.0f, 0.2f, static_cast<float>(mSampleRate));
      use->env.gateOn();
      use->env.gateOff();
   }
   if (!use->birdsReady)
      configureBirds(*use);
   const int slot = static_cast<int>(use - mVoices);
   startPhrase(slot, 0, std::max(mP.shotGain, 0.35f), false, true);
}

bool ChirpEngine::isSilent() const {
   if (mSilenceCounter < 0.25f)
      return false;
   for (const auto &v : mVoices)
      if (v.active)
         return false;
   for (const auto &p : mPhrases)
      if (p.active)
         return false;
   for (const auto &c : mChirps)
      if (c.active)
         return false;
   for (const auto &s : mStrikes)
      if (s.active)
         return false;
   return true;
}

float ChirpEngine::tailSeconds() const {
   // The longest thing still to come after the note goes: the release, plus the
   // phrase that was under way when it went -- syllables in flight always
   // finish -- plus whatever the space is still doing with it.
   const float space = mP.spaceAmount > 0.001f ? 0.3f + mSpace.decaySeconds() : 0.05f;
   const float phrase = static_cast<float>(clampi(mP.syllables, 1, 64)) /
                        clampf(mP.syllableRate, 0.2f, 60.0f) +
                        mP.lengthSec;
   return mP.releaseSec + phrase + space;
}

uint32_t ChirpEngine::activeVoiceCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices)
      if (v.active)
         ++n;
   return n;
}

uint32_t ChirpEngine::activeChirpCount() const {
   uint32_t n = 0;
   for (const auto &c : mChirps)
      if (c.active)
         ++n;
   for (const auto &s : mStrikes)
      if (s.active)
         ++n;
   return n;
}

} // namespace chirpparade
