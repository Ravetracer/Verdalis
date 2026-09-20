#include "night_engine.h"

#include "bed_generated.h"
#include "chorus_generated.h"
#include "contours_generated.h"

#include "verdalis/dsp/fastmath.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

namespace nightlife {

namespace {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

constexpr float kSpeedOfSoundCmS = 34300.0f;

uint32_t rngStateForSeed(int seed) {
   return 0x9E3779B9u * static_cast<uint32_t>(seed) + 17u;
}

std::atomic<uint32_t> gInstanceCounter{0};

// ------------------------------------------------------------------ callers
//
// The five numbers a median genuinely describes. A call's *shape* is not among
// them and never was: it is in contours_generated.h, as measured curves.
// tools/analysis/callers.py prints this table and the grouping it used.
//
// lengthSec is the median duration of that caller's *archetypes*, not of all
// its segmented calls. The two differ where only some of a caller's calls
// passed the contour quality gate, and using the wrong one stretches a curve to
// a length its own recording never had.
//
//                      pitch     len   harm   rough    rate
constexpr CallerTraits kCallerTable[kNumCallers] = {
   /* Wolf    */ {484.0f, 1.290f, 2.0f, -33.0f, 24.0f},
   /* Owl     */ {497.0f, 0.181f, 1.0f, -34.0f, 67.0f},
   /* Screech */ {1256.0f, 0.456f, 1.0f, -33.0f, 22.0f},
   /* Scops   */ {1272.0f, 0.907f, 1.0f, -32.0f, 56.0f},
   /* Fox     */ {1053.0f, 0.949f, 3.0f, -23.0f, 43.0f},
   /* Loon    */ {1132.0f, 0.268f, 1.0f, -34.0f, 82.0f},
};

// How much of each cycle the valve is shut, for a given harmonic count. The
// relation is ChirpParade's, measured on the same valve: harmonics =
// 1 + 5.5 * closure^0.6, saturating near five. This library asks less of it --
// the noisiest caller here measures three harmonics against a corvid's twelve.
constexpr float kHarmonicsPerClosure = 5.5f;
constexpr float kClosureExponent = 0.6f;

inline float closureForHarmonics(float harmonics) {
   if (harmonics <= 1.0f)
      return 0.0f;
   return clampf(std::pow((harmonics - 1.0f) / kHarmonicsPerClosure, 1.0f / kClosureExponent),
                 0.0f, 0.90f);
}

// ------------------------------------------------------- the contour tables
//
// The archetypes' cosine series, rendered once into tables the audio thread
// reads. Built on first use and forced from prepare(), so the first note does
// not pay for it.
struct ContourTables {
   static constexpr int kN = NightEngine::kContourPoints;
   static constexpr int kH = NightEngine::kHarmPoints;
   float pitch[kNumContours][kN];            // octaves about the loudest moment
   float level[kNumContours][kN];            // linear, peak 1
   float harm[kNumContours][kHarmonics][kH]; // linear, partials sum to unit power

   ContourTables() {
      constexpr float kPi = 3.14159265358979f;
      for (int c = 0; c < kNumContours; ++c) {
         // Only the terms this archetype earned. The count is its own duration
         // at 55 terms a second and the rest of the array is zero, so summing
         // all of them would give the same answer and cost four times as much
         // on a short call.
         const int terms = clampi(kContours[c].pitchTerms, 1, kPitchTerms);
         for (int i = 0; i < kN; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(kN);
            float p = 0.0f;
            for (int k = 0; k < terms; ++k)
               p += kContours[c].pitch[k] * std::cos(kPi * k * t);
            float l = 0.0f;
            for (int k = 0; k < kLevelTerms; ++k)
               l += kContours[c].level[k] * std::cos(kPi * k * t);
            pitch[c][i] = p;
            level[c][i] = std::pow(10.0f, clampf(l, -80.0f, 6.0f) / 20.0f);
         }
         for (int h = 0; h < kHarmonics; ++h) {
            for (int i = 0; i < kH; ++i) {
               const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(kH);
               float a = 0.0f;
               for (int k = 0; k < kHarmTerms; ++k)
                  a += kContours[c].harm[h][k] * std::cos(kPi * k * t);
               harm[c][h][i] = std::pow(10.0f, clampf(a, -60.0f, 6.0f) / 20.0f);
            }
         }
      }
   }
};

const ContourTables &contourTables() {
   static const ContourTables tables;
   return tables;
}

// The archetype an index lands on, within one caller's own set.
inline int archetypeFor(int caller, float where) {
   const int ca = clampi(caller, 0, kNumCallers - 1);
   ContourRange r = kContourRange[ca];
   if (r.count <= 0)
      r = kContourRange[kCallerWolf];
   const int k = static_cast<int>(clampf(where, 0.0f, 0.99999f) * static_cast<float>(r.count));
   return r.first + clampi(k, 0, r.count - 1);
}

// Bends a call's own time axis. The standard bias curve: one divide, and k = 1
// leaves it alone.
inline float warpTime(float t, float k) {
   return t / (t + (1.0f - t) * k + 1.0e-9f);
}

// A soft hinge, for the one-sided valve. max(g, 0) is what aliases; widening
// the corner with frequency is both the cure and what a real valve does, since
// it cannot snap shut arbitrarily fast.
inline float softHinge(float g, float w) {
   return 0.5f * (g + std::sqrt(g * g + w * w));
}

// The frequency the radiation derivative is referenced to, so that `Radiate`
// gives a +6 dB/octave tilt about the library's median pitch rather than a gain
// that changes with the note.
constexpr float kRadiateRefHz = 893.0f;

// The valve's output is bounded, so one calibration constant turns it into a
// level. Measured on the engine's own output: one call at Shot Level 0 dB, full
// velocity and no distance.
constexpr float kCallNorm = 1.20f;

// The same for the other three layers, so that each of them at 0 dB sits where
// a call at 0 dB does. All four were measured by rendering each layer alone
// with the other three at -60 dB, twenty seconds at the default density:
//
//     layer      at 0 dB before      after      what it is judged on
//     pack       peak -9.9 dBFS      (kept)     peak: it is an event layer
//     chorus     peak -17.5          -6         peak, for the same reason
//     insects    rms  -21.3          -14        rms: it is continuous
//     bed        rms   -7.9          -14        rms, and it clipped at 0 dB
//
// An event layer is judged on its peak and a continuous one on its rms,
// deliberately: twenty seconds of a sparse pack is mostly silence, so its rms
// says more about Pack Rate than about how loud a howl is.
constexpr float kCroakNorm = 9.8f;
constexpr float kInsectNorm = 4.4f;
constexpr float kBedNorm = 1.09f;

// The additive path against the valve path, so that `Partials` is a change of
// timbre and not of level, and how far the valve is ducked as they come in.
constexpr float kPartialNorm = 0.79f;
constexpr float kPartialDuck = 1.0f;

// Breath against measured roughness: 13 dB per decade, ChirpParade's
// calibration on the same valve.
constexpr float kRoughDbPerDecade = 13.0f;

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
   l = std::fabs(sin2piFast(0.25f - t));
   r = std::fabs(sin2piFast(t));
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

// Which measured croak a position lands on.
inline int croakTypeFor(float where) {
   const int k = static_cast<int>(clampf(where, 0.0f, 0.99999f) *
                                  static_cast<float>(kNumCroaks));
   return clampi(k, 0, kNumCroaks - 1);
}

} // namespace

const CallerTraits &callerTraits(int caller) {
   return kCallerTable[clampi(caller, 0, kNumCallers - 1)];
}

// ---------------------------------------------------------------- lifecycle

void NightEngine::prepare(double sampleRate, uint32_t /*maxBlockSize*/) {
   mSampleRate = sampleRate;
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   mRng.reseed(static_cast<uint32_t>(now) ^
               (0x9E3779B9u * (gInstanceCounter.fetch_add(1) + 1)));
   mAppliedSeed = 0;
   contourTables();

   // A valley at night is a large space with soft edges. The loop highpass sits
   // lower than ChirpParade's, because a howl has real energy at 140 Hz and the
   // tail of one is most of what distance sounds like.
   mSpace.prepare(static_cast<float>(sampleRate), 55.0f);
   mBedRng.seed(0x51F3A2C7u);
   reset();
   updateFilters();
}

void NightEngine::reset() {
   for (auto &v : mVoices) {
      v.active = false;
      v.env.reset();
      v.restlessLp.reset();
      v.modCounter = 0;
      v.packTimer = 0.0;
      v.animalsReady = false;
      v.pendingShot = false;
   }
   for (auto &c : mCalls) {
      c.active = false;
      c.tract.reset();
      c.top.reset();
      c.air.reset();
      c.osc = 0.0f;
      c.prevFlow = 0.0f;
      c.smoothed = 0.0f;
      c.primed = false;
      c.dcBlock.reset();
   }
   for (auto &c : mCroaks) {
      c.active = false;
      c.body1.reset();
      c.body2.reset();
      c.air.reset();
      c.env = 0.0f;
   }
   for (auto &p : mPhrases)
      p.active = false;
   for (int i = 0; i < 8; ++i) {
      mBedL[i].reset();
      mBedR[i].reset();
   }
   mBedLpL.reset();
   mBedLpR.reset();
   mBedHpL.reset();
   mBedHpR.reset();
   mBedMotionLp.reset();
   mBedMotion = 0.0f;
   mBedEnv = 0.0f;
   mFrogsReady = false;
   mChorusTime = 0.0;

   // A non-zero Seed promises the same night every time, so starting over has
   // to start the sequence over too. Seed 0 deliberately keeps running.
   if (mP.seed != 0)
      mRng.reseed(rngStateForSeed(mP.seed));
   mCallCounter = 0;

   // The three continuous layers carry generator state of their own, and it has
   // to start over with everything else: a bed and an insect field are as much
   // part of "the same night" as the animals are.
   for (auto &ins : mInsects) {
      ins.band.reset();
      ins.trillPhase = 0.0f;
   }
   seedContinuous();

   mSpace.clear();
   mOutFilterL.reset();
   mOutFilterR.reset();
   mOutHpL.reset();
   mOutHpR.reset();
   mSilenceCounter = 0.0f;
}

void NightEngine::setParams(const EngineParams &p) {
   const int wasFrogs = mP.frogs;
   const float wasSpread = mP.chorusSpreadOct;
   const float wasCroak = mP.croak;
   mP = p;
   if (mP.seed != mAppliedSeed) {
      mAppliedSeed = mP.seed;
      if (mP.seed != 0)
         mRng.reseed(rngStateForSeed(mP.seed));
      // And the continuous layers with it. A host applies the seed *after*
      // reset -- the parameter events of a block arrive once the block has
      // started -- so seeding the bed in reset() alone leaves it drawn from
      // whatever the generator held at the time, and the render then depends on
      // its own history. The self-test catches exactly this.
      seedContinuous();
      mFrogsReady = false;
   }
   if (mP.frogs != wasFrogs || mP.chorusSpreadOct != wasSpread || mP.croak != wasCroak)
      mFrogsReady = false;
   updateFilters();
}

void NightEngine::seedContinuous() {
   // From the seed when there is one, so that a seeded night's bed and insect
   // field are the same night too. From the engine's own generator when Seed is
   // zero, which is the promise that zero is always different.
   const uint32_t base =
      mP.seed != 0 ? hash32(rngStateForSeed(mP.seed) ^ 0x5D1C2B3Au) : (mRng.next() | 1u);
   mBedRng.seed(base | 1u);
   for (uint32_t i = 0; i < kNumInsects; ++i)
      mInsects[i].rng.seed(hash32(base + (i + 1u) * 0x9E3779B9u) | 1u);
}

void NightEngine::updateFilters() {
   const float sr = static_cast<float>(mSampleRate);
   const CallerTraits &ca = callerTraits(mP.caller);

   // A caller biases the call controls rather than replacing them, so
   // everything here is a ratio to the library-wide median.
   mCallerPitchMul = ca.pitchHz / kLibraryPitchHz;
   mCallerRateMul = ca.ratePerMin / kLibraryRatePerMin;
   mCallerClosure = closureForHarmonics(ca.harmonics);
   // A caller's measured roughness, turned back into a Breath multiplier. Note
   // what this does *not* claim: spectral flatness cannot tell turbulent noise
   // from a chaotic source, so what is measured is carried as noise, which is
   // the conservative reading. The chaotic route is `Rasp`, and it stays a
   // control the presets set by ear.
   mCallerBreathMul = std::pow(10.0f, (ca.roughDb - kLibraryRoughDb) / kRoughDbPerDecade);

   // The tube above the larynx, closed at one end and open at the mouth,
   // resonating at c/4L. An open mouth shortens the effective tube and damps
   // it, so the resonance rises and its Q falls.
   const float lengthCm = clampf(mP.throatCm, 0.5f, 80.0f);
   const float muzzle = clampf(mP.muzzle, 0.0f, 1.0f);
   mFormantHz = clampf(kSpeedOfSoundCmS / (4.0f * lengthCm) * (1.0f + 0.5f * muzzle), 80.0f,
                       0.45f * sr);
   mFormantQ = clampf(6.0f - 4.0f * muzzle, 0.7f, 12.0f);

   // Distance: air absorption, and the inverse-distance loss that comes with
   // it. An animal is a point source, unlike wind or surf, so the level really
   // does fall off with distance rather than only losing its top.
   const float d = clampf(mP.distance, 0.0f, 1.0f);
   const float airKeep = clampf(mP.air, 0.0f, 1.0f);
   mAirCutoffHz = clampf(20000.0f * std::pow(0.05f, d * (1.4f - 0.7f * airKeep)), 400.0f,
                         20000.0f);
   mDistanceGain = 1.0f / (1.0f + 5.0f * d);

   // The bed's bank. The gains are the solved answer from bed.py; Tilt turns
   // the whole curve about 500 Hz, which is where the measured bed peaks.
   const float tilt = clampf(mP.bedTilt, -1.0f, 1.0f) * 9.0f; // dB per octave
   for (int i = 0; i < kNumBedBands; ++i) {
      const float hz = clampf(kBedBandHz[i], 20.0f, 0.45f * sr);
      // An octave-wide bandpass: Q = 1.414 puts the -3 dB points one octave
      // apart, which is what the bank was solved for.
      mBedL[i].setCutoff(hz, resonanceFor(1.414f), sr);
      mBedR[i].setCutoff(hz, resonanceFor(1.414f), sr);
      const float oct = std::log2(hz / 500.0f);
      mBedGain[i] = kBedBandGain[i] * std::pow(10.0f, tilt * oct / 20.0f);
   }
   // The ends of the curve, which the bank cannot make. Tilt moves them with
   // it, or turning the bed brighter would only raise a shelf under a fixed
   // ceiling.
   const float tiltMul = std::exp2(clampf(mP.bedTilt, -1.0f, 1.0f) * 1.2f);
   mBedLpL.setCutoff(clampf(kBedLpHz * tiltMul, 200.0f, 0.45f * sr), sr);
   mBedLpR.setCutoff(clampf(kBedLpHz * tiltMul, 200.0f, 0.45f * sr), sr);
   mBedHpL.setCutoff(clampf(kBedHpHz * tiltMul, 15.0f, 400.0f), sr);
   mBedHpR.setCutoff(clampf(kBedHpHz * tiltMul, 15.0f, 400.0f), sr);

   mOutHpL.setCutoff(clampf(mP.highpassHz, 10.0f, 4000.0f), sr);
   mOutHpR.setCutoff(clampf(mP.highpassHz, 10.0f, 4000.0f), sr);

   const float reso = clampf(0.05f + 0.90f * mP.filterReso, 0.0f, 0.98f);
   const float cutoff = clampf(mP.filterCutoffHz * noteKeyTrack(), 20.0f, 0.45f * sr);
   mOutFilterL.setCutoff(cutoff, reso, sr);
   mOutFilterR.setCutoff(cutoff, reso, sr);

   mSpace.setSize(mP.spaceSize);
   mSpace.setDamping(mP.spaceDamping);
   // A wood or a valley is an enclosure with no walls: many scatterers and no
   // late build-up, which is the low end of what Space can be told to be.
   mSpace.setEnclosure(0.30f);

   configureInsects();
}

float NightEngine::noteKeyTrack() const {
   if (mP.filterKeyTrack <= 0.0f)
      return 1.0f;
   return std::exp2(mP.filterKeyTrack * (static_cast<float>(mLastKey) - 60.0f) / 12.0f);
}

void NightEngine::noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId,
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
      // phrase of its own. Calls already in flight are never touched: they
      // belong to the pool, not to the voice, and cutting a howl off in the
      // middle is the one thing an animal never does.
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
   v.packTimer = 0.0;

   // The note is a pitch: middle C leaves Pitch alone and every other key
   // transposes it.
   v.pitchMul = std::exp2(static_cast<float>(key - 60) / 12.0f);

   // Nothing is scheduled here, and nothing is drawn from mRng here. A note can
   // arrive before the parameter events in the same block have been applied --
   // the host sends them in its own order -- so a phrase started now would be
   // built from the previous preset's settings.
   v.animalsReady = false;
   v.pendingShot = true;
}

void NightEngine::noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = (noteId >= 0 && v.noteId == noteId) ||
                         (noteId < 0 && v.port == port && v.channel == channel && v.key == key);
      if (match)
         v.env.gateOff();
   }
}

void NightEngine::choke(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
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

void NightEngine::allSoundOff() {
   for (auto &v : mVoices) {
      v.env.kill();
      v.active = false;
   }
   for (auto &p : mPhrases)
      p.active = false;
   for (auto &c : mCalls)
      c.active = false;
   for (auto &c : mCroaks)
      c.active = false;
}

// ------------------------------------------------------------------- pools

Call *NightEngine::allocateCall() {
   uint32_t live = 0;
   Call *free = nullptr;
   Call *oldest = nullptr;
   float furthest = -1.0f;
   for (auto &c : mCalls) {
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
   const uint32_t limit = static_cast<uint32_t>(clampi(mP.maxVoices, 1, kMaxCalls));
   if (live >= limit || !free) {
      // At the ceiling, take the call closest to finishing rather than the
      // quietest: interrupting something mid-sweep is audible, where the end of
      // a call is already fading.
      if (!oldest)
         return nullptr;
      free = oldest;
   }
   free->active = false;
   free->tract.reset();
   free->top.reset();
   free->air.reset();
   free->osc = 0.0f;
   free->prevFlow = 0.0f;
   free->smoothed = 0.0f;
   free->primed = false;
   free->dcBlock.reset();
   free->walk = 0.0f;
   free->phase = 0.0f;
   return free;
}

CroakVoice *NightEngine::allocateCroak() {
   for (auto &c : mCroaks) {
      if (!c.active) {
         c.body1.reset();
         c.body2.reset();
         c.air.reset();
         c.env = 0.0f;
         c.releasing = false;
         return &c;
      }
   }
   return nullptr;
}

bool NightEngine::hasRunningShot(int voiceIndex) const {
   for (const auto &p : mPhrases)
      if (p.active && p.shot && p.voice == voiceIndex)
         return true;
   return false;
}

Phrase *NightEngine::allocatePhrase() {
   for (auto &p : mPhrases)
      if (!p.active)
         return &p;
   return nullptr;
}

// -------------------------------------------------------------------- pack

void NightEngine::configureAnimals(Voice &v) {
   v.animalsReady = true;
   v.animalCount = clampi(mP.animals, 1, static_cast<int>(kMaxAnimals));
   const uint32_t slot = static_cast<uint32_t>(&v - mVoices);
   const uint32_t base = hash32(slot * 2654435761u + static_cast<uint32_t>(v.key) * 40503u +
                                static_cast<uint32_t>(mP.seed) * 2246822519u);

   for (int i = 0; i < v.animalCount; ++i) {
      Animal &a = v.animals[i];
      const uint32_t h = base + static_cast<uint32_t>(i) * 0x9E3779B9u;
      // Pitch: spread over the range, but not evenly. Wolves in a chorus
      // deliberately avoid each other's pitch, which is why a hashed draw with
      // a couple of outliers reads as more animals than an even division does.
      a.pitchOct = 0.5f * mP.pitchSpreadOct * hashBipolar(h + 1u);
      // Position: the first animal in the middle, so that a pack of one is not
      // off to one side.
      a.pan = v.animalCount == 1 ? 0.0f : hashBipolar(h + 2u);
      a.distance = hashBipolar(h + 3u);
      a.voiceOff = hashBipolar(h + 4u);
      a.lengthMul = std::exp2(0.6f * mP.voiceSpread * hashBipolar(h + 5u));
      a.sweepMul = std::exp2(0.8f * mP.voiceSpread * hashBipolar(h + 6u));
      a.contourOff = 0.35f * mP.voiceSpread * hashBipolar(h + 7u);
      a.rateMul = std::exp2(0.7f * mP.voiceSpread * hashBipolar(h + 8u));
      a.levelMul = std::exp2(-0.5f * mP.voiceSpread * hashUnit(h + 9u));
   }
}

Phrase *NightEngine::startPhrase(int voiceIndex, int animal, float level, bool shot) {
   Phrase *p = allocatePhrase();
   if (!p)
      return nullptr;
   const Voice &v = mVoices[voiceIndex];
   const Animal &a = v.animals[clampi(animal, 0, static_cast<int>(kMaxAnimals) - 1)];

   p->active = true;
   p->shot = shot;
   p->voice = voiceIndex;
   p->animal = animal;
   p->index = 0;
   p->timer = 0.0;
   p->level = level;
   p->rng.seed(mRng.next() | 1u);

   p->calls = clampi(mP.calls, 1, 32);
   p->remaining = p->calls;
   p->repeatsLeft = clampi(mP.repeats, 1, 16) - 1;
   const float rate = clampf(mP.callRate * a.rateMul, 0.02f, 40.0f);
   p->interval = 1.0f / rate;
   const float total = clampf(1.0f - mP.rateDrift, 0.15f, 4.0f);
   p->driftMul = std::pow(total, 1.0f / std::max(1.0f, static_cast<float>(p->calls - 1)));
   p->gapAfter = clampf(mP.phraseGapSec, 0.005f, 60.0f);
   p->pitchHz = clampf(mP.pitchHz * mCallerPitchMul * v.pitchMul * std::exp2(a.pitchOct) *
                          std::exp2(mP.velToPitch * (v.velocity - 1.0f)),
                       30.0f, 12000.0f);
   p->motifSemis = mP.motifSemis;
   p->variation = clampf(mP.variation, 0.0f, 1.0f);
   return p;
}

void NightEngine::schedule(Voice &v, float env, double blockSec) {
   const int slot = static_cast<int>(&v - mVoices);
   if (!v.animalsReady)
      configureAnimals(v);

   if (v.pendingShot) {
      v.pendingShot = false;
      if (mP.shotGain > 1.0e-4f)
         startPhrase(slot, 0, mP.shotGain * env, true);
   }

   // The pack. A Poisson process rather than a clock: unlike the frogs, the
   // callers measure as one -- a wolf does not howl on a schedule, and the
   // waiting time between one animal speaking and the next is exponential.
   if (mP.packGain > 1.0e-4f && mP.packRatePerMin > 0.01f) {
      // Restless: a slow drift of the density. No night is uniform.
      const float ctrl = static_cast<float>(mSampleRate) / static_cast<float>(kModInterval);
      v.restlessLp.setCoef(onePoleCoef(2.5f, ctrl));
      const float drift = v.restlessLp.tick(mRng.white());
      const float rateMul = std::exp2(2.0f * clampf(mP.restless, 0.0f, 1.0f) * drift);
      // Pack Rate is calls a minute, as measured, so the rate at which
      // *phrases* start is that divided by the calls in one.
      const float per = static_cast<float>(clampi(mP.calls, 1, 32));
      const float rate =
         clampf(mP.packRatePerMin * mCallerRateMul * rateMul / (60.0f * per), 0.0005f, 30.0f);
      v.packTimer -= blockSec;
      if (v.packTimer <= 0.0) {
         v.packTimer = static_cast<double>(mRng.exponential(rate));
         const int animal = static_cast<int>(mRng.uniform() * static_cast<float>(v.animalCount));
         startPhrase(slot, clampi(animal, 0, v.animalCount - 1), mP.packGain * env, false);
         // Answer: another animal replies from somewhere else, shortly after.
         // This is most of what makes a pack sound like a conversation rather
         // than a random process, and a wolf chorus is exactly it.
         if (v.animalCount > 1 && mRng.uniform() < clampf(mP.answer, 0.0f, 1.0f)) {
            int other = static_cast<int>(mRng.uniform() * static_cast<float>(v.animalCount));
            if (other == animal)
               other = (other + 1) % v.animalCount;
            Phrase *p = startPhrase(slot, other, mP.packGain * env * 0.85f, false);
            if (p)
               p->timer = 0.3 + 1.6 * static_cast<double>(mRng.uniform());
         }
      }
   }
}

// ------------------------------------------------------------------ chorus

void NightEngine::configureFrogs() {
   mFrogsReady = true;
   mFrogCount = clampi(mP.frogs, 1, static_cast<int>(kMaxFrogs));
   const uint32_t base = hash32(0x9E3779B9u * static_cast<uint32_t>(mP.seed) + 0x1D2C3B4Au);
   for (int i = 0; i < mFrogCount; ++i) {
      Frog &f = mFrogs[i];
      const uint32_t h = base + static_cast<uint32_t>(i) * 0x85EBCA6Bu;
      f.pitchMul = std::exp2(0.5f * mP.chorusSpreadOct * hashBipolar(h + 1u));
      f.pan = mFrogCount == 1 ? 0.0f : hashBipolar(h + 2u);
      f.distance = hashUnit(h + 3u);
      // Each frog's own tempo. A pond is not a metronome section.
      f.rateMul = std::exp2(0.55f * hashBipolar(h + 4u));
      f.levelMul = std::exp2(-0.8f * hashUnit(h + 5u));
      // Which measured croak. Croak chooses the centre and the pond spreads
      // around it, so a chorus is several species as a real one is.
      const int centre = croakTypeFor(mP.croak);
      const int spread = 1 + static_cast<int>(1.5f * mP.chorusSpreadOct);
      f.type = clampi(centre + static_cast<int>(std::lround(spread * hashBipolar(h + 6u))), 0,
                      kNumCroaks - 1);
      // Where this frog sits in the pond's shared cycle. Spread over it rather
      // than drawn at random: the frogs of a chorus take turns, and two of them
      // landing on the same instant is what a random draw does and a pond does
      // not. The hash only perturbs the place.
      f.phaseOff = (static_cast<float>(i) + 0.35f * hashBipolar(h + 7u)) /
                   static_cast<float>(mFrogCount);
      // And it starts there rather than at zero. Every frog scheduled for the
      // same instant is what `f.next = 0` gives, and the first thing a note
      // produced was the whole pond croaking at once.
      const double period = 1.0 / static_cast<double>(clampf(mP.croakRatePerMin / 60.0f,
                                                             0.01f, 40.0f));
      f.next = mChorusTime + static_cast<double>(f.phaseOff) * period;
   }
}

void NightEngine::scheduleChorus(float env, double blockSec) {
   if (mP.chorusGain <= 1.0e-4f || mP.croakRatePerMin <= 0.01f)
      return;
   if (!mFrogsReady)
      configureFrogs();

   mChorusTime += blockSec;
   const float reg = clampf(mP.regularity, 0.0f, 1.0f);
   // The pond's shared cycle: one croak per frog per turn of it.
   const double period = 1.0 / static_cast<double>(clampf(mP.croakRatePerMin / 60.0f,
                                                          0.01f, 40.0f));

   for (int i = 0; i < mFrogCount; ++i) {
      Frog &f = mFrogs[i];
      if (mChorusTime < f.next)
         continue;
      const CroakType &ct = kCroaks[clampi(f.type, 0, kNumCroaks - 1)];
      // This frog's own tempo, biased by the croak it makes: a leopard frog
      // calls four times as often as a bullfrog, and that is in the table.
      const float rate = clampf(mP.croakRatePerMin * f.rateMul *
                                   (ct.ratePerMin / kLibraryCroakRate) / 60.0f,
                                0.01f, 40.0f);

      // **The chorus is not a Poisson process, and these lines are why.**
      //
      // Measured Fano factor of the croak arrivals: 0.90 at 50 ms, 0.81 at
      // 250 ms, 0.59 at one second and 0.30 at four, where a Poisson process is
      // exactly 1.0 at every window. Frogs are *more* regular than random --
      // which is the opposite of what CrackleBlaze measured for fire, at 3.90.
      //
      // Giving each frog a clock of its own does not reproduce it, and that is
      // worth knowing rather than discovering twice: the superposition of many
      // independent renewal processes tends to a Poisson process however
      // regular each one is (Palm-Khintchine), and eight frogs on their own
      // clocks rendered 1.15 at four seconds. The regularity is a property of
      // the *pond*, not of the frog. So the chorus keeps one period and each
      // frog has a place in it, which is also what a chorus of real anurans
      // does -- they call in turn, and a pond that has been listened to for a
      // minute is audibly a rhythm section rather than rainfall.
      const double grid = period * (std::floor((mChorusTime - f.phaseOff * period) / period) +
                                    1.0) +
                          f.phaseOff * period;
      const double poisson = mChorusTime + static_cast<double>(mRng.exponential(rate));
      // Even a locked frog is not a metronome; the jitter closes as Regularity
      // rises and is what keeps the top window's Fano factor near the measured
      // 0.90 rather than at zero.
      const double jitter = static_cast<double>(0.25f * (1.0f - 0.7f * reg) *
                                                mRng.white()) * period;
      f.next = reg * grid + (1.0 - reg) * poisson + jitter;
      if (f.next <= mChorusTime)
         f.next = mChorusTime + 0.02 * period;
      if (env > 1.0e-3f)
         spawnCroak(f);
   }
}

void NightEngine::spawnCroak(const Frog &f) {
   CroakVoice *voice = allocateCroak();
   if (!voice)
      return;
   CroakVoice &c = *voice;
   const float sr = static_cast<float>(mSampleRate);
   const CroakType &ct = kCroaks[clampi(f.type, 0, kNumCroaks - 1)];

   // The croak's own measured numbers, biased by the controls. Each control is
   // a ratio to the library median, so turning none of them plays the table.
   const float f1 = clampf(mP.croakHz * (ct.f1Hz / kLibraryCroakHz) * f.pitchMul, 80.0f,
                           0.45f * sr);
   // The second resonance sits at a measured median 0.50 of the first across
   // the library, and this row's own ratio is kept rather than that median.
   const float f2 = clampf(f1 * (ct.f2Hz / std::max(ct.f1Hz, 1.0f)), 60.0f, 0.45f * sr);
   c.body1.setCutoff(f1, resonanceFor(ct.q1), sr);
   c.body2.setCutoff(f2, resonanceFor(0.6f * ct.q1), sr);

   const float rate = clampf(mP.pulseHz * (ct.pulseHz / kLibraryPulseHz), 2.0f, 400.0f);
   c.pulseInc = rate / sr;
   c.pulsePhase = 0.999f; // so the first pulse fires immediately
   c.pulseDepth = clampf(ct.pulseDepth, 0.0f, 1.0f);
   // The excitation decays between pulses by as much as the measured depth
   // says. At 0.97, which is what every row of the table measures, the train is
   // separate clicks and not a tremolo on a tone.
   const float gapSec = 1.0f / rate;
   c.exciteCoef = decayCoef(gapSec * (1.02f - c.pulseDepth) * 3.0f, sr);
   c.excite = 0.0f;

   const float lenMul = mP.croakSec / kLibraryCroakSec;
   const float pulses = clampf(static_cast<float>(mP.pulses) * (ct.pulses / 10.5f) * lenMul,
                               1.0f, 200.0f);
   c.pulsesLeft = static_cast<int>(std::lround(pulses));
   c.attackCoef = onePoleCoef(std::max(0.002f, ct.riseSec * lenMul), sr);
   c.releaseCoef = decayCoef(std::max(0.005f, ct.fallSec * lenMul * 2.0f), sr);
   c.env = 0.0f;
   c.releasing = false;

   const float d = clampf(mP.distance * (0.4f + 0.9f * f.distance), 0.0f, 1.0f);
   c.air.setCutoff(clampf(20000.0f * std::pow(0.05f, d * (1.4f - 0.7f * mP.air)), 400.0f,
                          20000.0f),
                   sr);
   panGains(f.pan * clampf(mP.chorusWidth, 0.0f, 1.0f), c.panL, c.panR);
   c.level = mP.chorusGain * f.levelMul * mBedEnv / (1.0f + 4.0f * d);
   c.rng.seed(mRng.next() | 1u);
   c.active = true;
}

// ----------------------------------------------------------------- insects

void NightEngine::configureInsects() {
   const float sr = static_cast<float>(mSampleRate);
   const float shimmer = clampf(mP.shimmer, 0.0f, 1.0f);
   // The measured Q is 20.7 and `Insect Band` walks either side of it -- but the
   // resonator that renders it is sharper than that number, because the number
   // is a *-6 dB* width and a Q is defined at -3 dB. Root 3 between them.
   //
   // Do not chase the rendered figure past this. The estimator in bed.py
   // smooths over 50 Hz to bridge the comb of a pulsed band, which floors the
   // width it can report at about 210 Hz -- a Q of 13 at 3 kHz -- so a render
   // measures 12 to 13 whatever this is set to, and so would a recording of a
   // single cricket. The references' 20.7 comes from files at 24 kHz, where the
   // same 50 Hz smoother is half as many bins wide. See TODO.md.
   constexpr float kInsectQCal = 1.732f;
   const float q = clampf(kInsectQ * kInsectQCal * std::exp2(2.0f * (0.35f - mP.insectWidth)),
                          1.5f, 120.0f);
   for (uint32_t i = 0; i < kNumInsects; ++i) {
      Insect &ins = mInsects[i];
      const uint32_t h = hash32(0x2545F491u * (i + 1u) +
                                static_cast<uint32_t>(mP.seed) * 374761393u);
      const float det = hashBipolar(h + 1u);
      // A quarter of an octave of detune per unit of Shimmer was the first
      // version and it is four times too much: six voices spread that far
      // measure as one band of Q 5.7 against the references' 20.7, which is a
      // hiss rather than a cricket. 0.15 puts the rendered band back on the
      // measurement with the field still audibly a field.
      ins.band.setCutoff(clampf(mP.insectHz * std::exp2(0.06f * shimmer * det), 200.0f,
                                0.45f * sr),
                         resonanceFor(q), sr);
      ins.trillInc = clampf(mP.trillHz * std::exp2(0.35f * shimmer * hashBipolar(h + 2u)),
                            0.5f, 200.0f) /
                     sr;
      panGains(hashBipolar(h + 3u) * clampf(mP.width, 0.0f, 1.0f), ins.panL, ins.panR);
      ins.level = std::exp2(-1.0f * hashUnit(h + 4u));
      // Nothing is seeded here. This runs on every parameter change, and a
      // generator reseeded -- or merely *advanced* -- by turning a knob is a
      // render that does not repeat, which is what the self-test catches.
   }
}

// ------------------------------------------------------------------ spawning

void NightEngine::spawnCall(const Voice &v, Phrase &ph) {
   Call *call = allocateCall();
   if (!call)
      return;
   Call &c = *call;
   const Animal &a = v.animals[clampi(ph.animal, 0, static_cast<int>(kMaxAnimals) - 1)];
   const float sr = static_cast<float>(mSampleRate);
   const float var = ph.variation;

   // Drawn once per call, from the phrase's own generator, so a phrase is
   // reproducible from its seed however many other animals are sounding.
   const float rp = ph.rng.white();
   const float rl = ph.rng.white();
   const float rc = ph.rng.white();
   const float rd = ph.rng.white();

   // ---------------------------------------------------------- the contour
   const float where = clampf(mP.contour + a.contourOff + 0.45f * var * rc, 0.0f, 1.0f);
   c.archetype = archetypeFor(mP.caller, where);
   const Contour &ct = kContours[c.archetype];

   // Motif: the pitch steps by a fixed interval from one call to the next.
   const float motif = ph.motifSemis * static_cast<float>(ph.index);
   const float pitch = clampf(ph.pitchHz * std::exp2((motif + 3.0f * var * rp) / 12.0f),
                              30.0f, 0.45f * sr);
   // The table holds shape only, about the call's own centre, so placing it is
   // a transposition. Held in octaves: the readout adds the contour to it and
   // takes one exp2 rather than a multiply and an exp2.
   c.pitchOct = std::log2(pitch);

   c.depth = clampf(mP.sweep * a.sweepMul * std::exp2(0.4f * var * rd), 0.0f, 4.0f);
   c.warp = std::exp2(3.0f * (2.0f * clampf(mP.skew, 0.0f, 1.0f) - 1.0f));

   // The archetype plays at its *own* measured duration, scaled by Length. A
   // caller's spread of call lengths is most of what makes it that animal: the
   // wolf archetypes run 311 ms to 2.8 s, and stretching them all to one target
   // is what a summary statistic would do.
   float len = mP.lengthSec * (ct.durationSec / kLibraryLengthSec) * a.lengthMul *
               std::exp2(0.5f * var * rl);

   // A call longer than the nominal interval takes the time it needs: one
   // animal cannot overlap itself, and truncating the curve is what produces
   // clipped, mechanical phrases.
   ph.slot = std::max(ph.interval, clampf(len, 0.01f, 12.0f));

   const float leg = ph.remaining > 1 ? clampf(mP.legato, 0.0f, 1.0f) : 0.0f;
   len = len * (1.0f - leg) + ph.slot * leg;
   len = clampf(len, 0.01f, 12.0f);
   c.phaseInc = 1.0f / (len * sr);

   // Detail: a one-pole on the contour as it is read out.
   const float detail = clampf(mP.detail, 0.0f, 1.0f);
   const float corner = 3.0f * std::pow(400.0f, detail); // 3 Hz .. 1.2 kHz
   c.smoothCoef = detail >= 0.999f ? 1.0f : onePoleCoef(1.0f / corner, sr);
   c.smoothed = 0.0f;
   c.primed = false;

   // ------------------------------------------------------------- the voice
   float closure = mCallerClosure + (clampf(mP.voice, 0.0f, 1.0f) - 0.30f) * 1.3f +
                   0.5f * mP.voiceSpread * a.voiceOff;
   const float ceiling = 0.92f - 2.2f * pitch / sr;
   c.closure = clampf(closure, 0.0f, clampf(ceiling, 0.0f, 0.92f));
   c.closureNow = c.closure;
   c.rasp = clampf(mP.rasp, 0.0f, 1.0f);
   // Scaled by how much of that call's energy the harmonic measurement actually
   // accounted for. An archetype with a second animal in it, or an inharmonic
   // one, has a low share and simply does not respond much -- which degrades
   // honestly instead of asserting a balance it never measured.
   c.partials = clampf(mP.partials, 0.0f, 1.0f) * clampf(ct.harmFit, 0.0f, 1.0f);
   c.prevFlow = 0.0f;
   c.osc = 0.0f;
   c.dcBlock.reset();
   c.dcBlock.setCutoff(clampf(0.35f * pitch, 30.0f, 900.0f), sr);

   c.jitter = clampf(mP.jitter, 0.0f, 1.0f) * 0.04f;
   c.walkCoef = onePoleCoef(0.010f, sr);
   c.walk = 0.0f;

   // Extra vibrato, in cents of pitch. The archetypes already carry their own
   // -- this is on top.
   c.vibDepth = clampf(mP.vibrato, 0.0f, 1.0f) * 0.06f; // up to 0.06 octaves
   c.vibInc = clampf(mP.vibratoHz, 0.1f, 30.0f) / sr;
   c.vibPhase = ph.rng.uniformPositive();

   c.breath = clampf(mP.breath * mCallerBreathMul, 0.0f, 1.0f);
   c.formant = clampf(mP.formant, 0.0f, 1.0f);
   c.radiate = clampf(mP.radiate, 0.0f, 1.0f);

   c.tractHz = clampf(mFormantHz, 60.0f, 0.45f * sr);
   c.tractReso = resonanceFor(mFormantQ);
   c.tractTrack = clampf(mP.muzzle, 0.0f, 1.0f);
   c.tractCounter = 0;
   c.tract.setCutoff(c.tractHz, c.tractReso, sr);
   // The valve's own corner and the tract's skirt both fall at 6 dB/octave, so
   // without this the top of the spectrum is the filter rather than the animal.
   c.top.setCutoff(clampf(pitch * (3.0f + 22.0f * c.closure), 900.0f, 0.45f * sr), sr);

   const float d = clampf(mP.distance + 0.5f * mP.distanceSpread * a.distance, 0.0f, 1.0f);
   c.air.setCutoff(clampf(20000.0f * std::pow(0.05f, d * (1.4f - 0.7f * mP.air)), 400.0f,
                          20000.0f),
                   sr);
   const float distGain = 1.0f / (1.0f + 5.0f * d);

   panGains(a.pan * clampf(mP.width, 0.0f, 1.0f), c.panL, c.panR);

   const float vel = 1.0f - mP.velToLevel * (1.0f - v.velocity);
   c.level = ph.level * a.levelMul * distGain * vel * kCallNorm;
   c.voice = static_cast<int>(&v - mVoices);
   c.rng.seed(ph.rng.next() | 1u);
   c.active = true;
   ++mCallCounter;
}

// ------------------------------------------------------------------ process

void NightEngine::processCalls(float *outL, float *outR, uint32_t numSamples) {
   const float sr = static_cast<float>(mSampleRate);
   const float invSr = 1.0f / sr;
   const ContourTables &tab = contourTables();
   constexpr int kLast = NightEngine::kContourPoints - 2;
   const float kDeriv = sr / (6.2831853f * kRadiateRefHz);

   for (auto &c : mCalls) {
      if (!c.active)
         continue;
      const float *pitchTab = tab.pitch[c.archetype];
      const float *levelTab = tab.level[c.archetype];
      const float (*harmTab)[NightEngine::kHarmPoints] = tab.harm[c.archetype];
      const float nyquist = 0.45f * sr;

      for (uint32_t i = 0; i < numSamples; ++i) {
         // ------------------------------------------------------ the contour
         //
         // The whole call is these few lines: read a measured pitch curve and a
         // measured level curve out over the call's own duration. The howl's
         // vibrato, its break, the fall at the end -- none of them are modelled
         // here, because all of them were measured.
         const float t = warpTime(clampf(c.phase, 0.0f, 1.0f), c.warp);
         const float x = t * static_cast<float>(NightEngine::kContourPoints - 1);
         int k = static_cast<int>(x);
         k = k < 0 ? 0 : (k > kLast ? kLast : k);
         const float frac = x - static_cast<float>(k);

         float oct = pitchTab[k] + frac * (pitchTab[k + 1] - pitchTab[k]);
         if (!c.primed) {
            c.smoothed = oct;
            c.primed = true;
         }
         c.smoothed += c.smoothCoef * (oct - c.smoothed);
         oct = c.smoothed;

         const float lvl = levelTab[k] + frac * (levelTab[k + 1] - levelTab[k]);

         c.walk += c.walkCoef * (c.jitter * c.rng.white() - c.walk);
         float vib = 0.0f;
         if (c.vibDepth > 1.0e-5f) {
            c.vibPhase += c.vibInc;
            if (c.vibPhase >= 1.0f)
               c.vibPhase -= 1.0f;
            vib = c.vibDepth * sin2piFast(c.vibPhase);
         }
         const float f0 = std::exp2(c.pitchOct + oct * c.depth + c.walk + vib);

         // -------------------------------------------------------- the valve
         c.osc += clampf(f0 * invSr, 0.0f, 0.49f);
         if (c.osc >= 1.0f) {
            c.osc -= 1.0f;
            // Rasp: the contact is not the same twice. Irregular closure is
            // what the break at the top of a howl and a vixen's scream both
            // physically are, and it is broadband in a way no amount of extra
            // harmonics is.
            c.closureNow = clampf(c.closure + c.rasp * 0.45f * c.rng.white(), 0.0f, 0.93f);
         }
         const float sine = sin2piFast(c.osc);
         const float gap = sine + (1.0f - 2.0f * c.closureNow);
         const float w = 0.010f + 3.0f * f0 * invSr;
         const float flow = softHinge(gap, w);

         const float dflow = (flow - c.prevFlow) * kDeriv;
         c.prevFlow = flow;

         float y = (1.0f - c.radiate) * flow + c.radiate * dflow;
         // A one-sided flow has a mean, and that mean is modulated at the call
         // rate, which is a thump rather than an animal.
         y = c.dcBlock.tick(y);

         // ------------------------------------------- the tube and the mouth
         const float bp = c.tract.bandpassNormalised(y);
         y += c.formant * 1.2f * bp;

         // ------------------------------------------- the measured partials
         if (c.partials > 1.0e-4f) {
            const float hx = t * static_cast<float>(NightEngine::kHarmPoints - 1);
            int hk = static_cast<int>(hx);
            hk = hk < 0 ? 0
                        : (hk > NightEngine::kHarmPoints - 2 ? NightEngine::kHarmPoints - 2 : hk);
            const float hfrac = hx - static_cast<float>(hk);
            float add = 0.0f;
            for (int h = 0; h < kHarmonics; ++h) {
               if (f0 * static_cast<float>(h + 1) > nyquist)
                  break;
               const float *ht = harmTab[h];
               const float amp = ht[hk] + hfrac * (ht[hk + 1] - ht[hk]);
               add += amp * sin2piFast(c.osc * static_cast<float>(h + 1));
            }
            y = y * (1.0f - kPartialDuck * c.partials) + add * kPartialNorm * c.partials;
         }

         y += c.breath * 0.8f * lvl * c.rng.white();
         y = c.top.tick(y);
         y = c.air.tick(y);

         // The tube's resonance follows the pitch, by as much as the mouth is
         // open. Updated every 16 samples -- a formant does not need a tan() a
         // sample.
         if ((c.tractCounter++ & 15u) == 0u) {
            const float track = c.tractTrack * (std::log2(std::max(f0, 20.0f)) - c.pitchOct);
            c.tract.setCutoff(clampf(c.tractHz * std::exp2(track), 60.0f, 0.45f * sr),
                              c.tractReso, sr);
         }

         const float amp = c.level * lvl;
         outL[i] += y * amp * c.panL;
         outR[i] += y * amp * c.panR;

         c.phase += c.phaseInc;
         if (c.phase >= 1.0f) {
            c.active = false;
            break;
         }
      }
   }
}

void NightEngine::processCroaks(float *outL, float *outR, uint32_t numSamples) {
   for (auto &c : mCroaks) {
      if (!c.active)
         continue;
      for (uint32_t i = 0; i < numSamples; ++i) {
         // The train. Each pulse is an impulse of noise, not a click: a frog's
         // pulse is the sac being driven rather than a spark, and a bare
         // impulse through these resonances is a plucked string.
         c.pulsePhase += c.pulseInc;
         if (c.pulsePhase >= 1.0f) {
            c.pulsePhase -= 1.0f;
            if (c.pulsesLeft > 0) {
               c.excite = 1.0f;
               --c.pulsesLeft;
            } else {
               c.releasing = true;
            }
         }
         c.excite *= c.exciteCoef;
         const float in = c.excite * c.rng.white();
         float x = c.body1.bandpassNormalised(in) + 0.55f * c.body2.bandpassNormalised(in);

         if (c.releasing)
            c.env *= c.releaseCoef;
         else
            c.env += c.attackCoef * (1.0f - c.env);

         x = c.air.tick(x) * c.level * c.env * kCroakNorm;
         outL[i] += x * c.panL;
         outR[i] += x * c.panR;

         if (c.releasing && c.env < 1.0e-4f) {
            c.active = false;
            break;
         }
      }
   }
}

void NightEngine::processBeds(float *outL, float *outR, uint32_t numSamples, float env) {
   const float bed = mP.bedGain * env;
   const float ins = mP.insectGain * env;
   const bool wantBed = bed > 1.0e-5f;
   const bool wantIns = ins > 1.0e-5f;
   if (!wantBed && !wantIns)
      return;

   const float ctrl = static_cast<float>(mSampleRate);
   mBedMotionLp.setCoef(onePoleCoef(3.0f, ctrl / 64.0f));
   const float depth = clampf(mP.bedMotion, 0.0f, 1.0f);
   const float trillDepth = clampf(mP.trillDepth, 0.0f, 1.0f);

   for (uint32_t i = 0; i < numSamples; ++i) {
      if ((i & 63u) == 0u)
         mBedMotion = mBedMotionLp.tick(mBedRng.white());
      const float motion = std::exp2(1.2f * depth * mBedMotion);

      if (wantBed) {
         // One noise source per channel, through the solved bank. Independent
         // noise rather than one source panned: a night is not a point.
         const float nl = mBedRng.white();
         const float nr = mBedRng.white();
         float l = 0.0f, r = 0.0f;
         for (int b = 0; b < kNumBedBands; ++b) {
            if (mBedGain[b] <= 1.0e-6f)
               continue;
            l += mBedGain[b] * mBedL[b].bandpassNormalised(nl);
            r += mBedGain[b] * mBedR[b].bandpassNormalised(nr);
         }
         const float g = bed * motion * kBedNorm;
         outL[i] += mBedHpL.tick(mBedLpL.tick(l)) * g;
         outR[i] += mBedHpR.tick(mBedLpR.tick(r)) * g;
      }

      if (wantIns) {
         float l = 0.0f, r = 0.0f;
         for (auto &insect : mInsects) {
            insect.trillPhase += insect.trillInc;
            if (insect.trillPhase >= 1.0f)
               insect.trillPhase -= 1.0f;
            // The trill is a raised cosine rather than a square: a cricket's
            // pulse has a shape, and a square gate on a resonant band rings.
            const float gate =
               1.0f - trillDepth * 0.5f * (1.0f - sin2piFast(insect.trillPhase + 0.25f));
            const float x = insect.band.bandpassNormalised(insect.rng.white()) *
                            insect.level * gate;
            l += x * insect.panL;
            r += x * insect.panR;
         }
         const float g = ins * motion * kInsectNorm;
         outL[i] += l * g;
         outR[i] += r * g;
      }
   }
}

void NightEngine::processOutputChain(float *outL, float *outR, uint32_t numSamples) {
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

      // Saturate rather than clip. A full pond with a pack over it and every
      // layer up can run past the ceiling, and a night should not crackle when
      // it does.
      outL[i] = softClip(l * mP.gain);
      outR[i] = softClip(r * mP.gain);
   }
}

void NightEngine::process(float *outL, float *outR, uint32_t numSamples) {
   std::fill(outL, outL + numSamples, 0.0f);
   std::fill(outR, outR + numSamples, 0.0f);

   const float sr = static_cast<float>(mSampleRate);
   const double blockSec = static_cast<double>(kModInterval) / mSampleRate;

   // Scheduling runs at the control rate, which is 0.67 ms at 48 kHz.
   uint32_t done = 0;
   float loudest = 0.0f;
   while (done < numSamples) {
      for (auto &v : mVoices) {
         if (!v.active)
            continue;
         float env = 0.0f;
         for (uint32_t k = 0; k < kModInterval; ++k)
            env = v.env.tick();
         loudest = std::max(loudest, env);
         schedule(v, env, blockSec);
         if (v.finished()) {
            // The pack is a bed and stops with the envelope. A phrase a note
            // asked for is not: it finishes, however short the note was, which
            // is what makes a note usable as a one shot.
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

      // The chorus, the insects and the bed follow the loudest note rather than
      // the sum: a second note should bring in more animals, not a second
      // night.
      mBedEnv = loudest;
      scheduleChorus(mBedEnv, blockSec);

      // Phrases: silent themselves, they only decide when something sounds.
      for (auto &p : mPhrases) {
         if (!p.active)
            continue;
         const Voice &v = mVoices[clampi(p.voice, 0, static_cast<int>(kMaxVoices) - 1)];
         p.timer -= blockSec;
         while (p.timer <= 0.0 && p.active) {
            spawnCall(v, p);
            ++p.index;
            --p.remaining;
            p.interval *= p.driftMul;
            if (p.remaining <= 0) {
               if (p.repeatsLeft > 0) {
                  --p.repeatsLeft;
                  p.remaining = p.calls;
                  p.index = 0;
                  p.timer += static_cast<double>(p.gapAfter);
               } else {
                  p.active = false;
               }
            } else {
               p.timer += static_cast<double>(clampf(p.slot, 0.005f, 30.0f));
            }
         }
      }
      done += kModInterval;
   }

   processCalls(outL, outR, numSamples);
   processCroaks(outL, outR, numSamples);
   processBeds(outL, outR, numSamples, mBedEnv);
   processOutputChain(outL, outR, numSamples);

   // Silence tracking, so the host can sleep once the last animal has finished.
   float peak = 0.0f;
   for (uint32_t i = 0; i < numSamples; ++i)
      peak = std::max(peak, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
   if (peak < 1.0e-6f)
      mSilenceCounter += numSamples / sr;
   else
      mSilenceCounter = 0.0f;
}

void NightEngine::triggerShot() {
   // The version label in the header, which is a free call: a phrase with no
   // note behind it. It needs a voice to hang off -- the animals, the pitch and
   // the pan all live there -- so the quietest one is borrowed.
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
      use->animalsReady = false;
      use->pendingShot = false;
      use->packTimer = 1.0e9;
      use->env.setParams(0.001f, 0.05f, 0.0f, 0.2f, static_cast<float>(mSampleRate));
      use->env.gateOn();
      use->env.gateOff();
   }
   if (!use->animalsReady)
      configureAnimals(*use);
   const int slot = static_cast<int>(use - mVoices);
   startPhrase(slot, 0, std::max(mP.shotGain, 0.35f), true);
}

bool NightEngine::isSilent() const {
   if (mSilenceCounter < 0.25f)
      return false;
   for (const auto &v : mVoices)
      if (v.active)
         return false;
   for (const auto &p : mPhrases)
      if (p.active)
         return false;
   for (const auto &c : mCalls)
      if (c.active)
         return false;
   for (const auto &c : mCroaks)
      if (c.active)
         return false;
   return true;
}

float NightEngine::tailSeconds() const {
   // The longest thing still to come after the note goes: the release, plus the
   // phrase that was under way when it went -- calls in flight always finish --
   // plus whatever the space is still doing with it.
   const float space = mP.spaceAmount > 0.001f ? 0.3f + mSpace.decaySeconds() : 0.05f;
   const float phrase = static_cast<float>(clampi(mP.calls, 1, 32)) /
                           clampf(mP.callRate, 0.02f, 40.0f) +
                        mP.lengthSec * 6.0f;
   return mP.releaseSec + phrase + space;
}

uint32_t NightEngine::activeVoiceCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices)
      if (v.active)
         ++n;
   return n;
}

uint32_t NightEngine::activeCallCount() const {
   uint32_t n = 0;
   for (const auto &c : mCalls)
      if (c.active)
         ++n;
   for (const auto &c : mCroaks)
      if (c.active)
         ++n;
   return n;
}

} // namespace nightlife
