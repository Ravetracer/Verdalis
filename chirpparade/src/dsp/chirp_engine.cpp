#include "chirp_engine.h"

#include "contours_generated.h"

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
// What is left of the species table after the contours took over its middle.
// Sweep, contour shape, turns and skew were columns here in the first version
// and they were the wrong four numbers: a syllable's shape is not summarised by
// a sweep width and a turn count. It is in contours_generated.h now, measured.
//
// These five are the ones a median genuinely describes. tools/analysis/species.py
// prints them and the grouping it used.
//
// No figure in this table is overridden any more. Crow and Raven were, and they
// are gone: see TODO.md, "The corvids were removed". Everything below is the
// median species.py measured, with lengthSec from contours.py.
//
// lengthSec is the median duration of that species' *archetypes*, not of all
// its measured syllables. The two differ where only some of a species'
// syllables passed the contour quality gate -- Crane's usable contours are its
// short ones -- and using the wrong one stretches a 48 ms curve to 133 ms,
// which turns its internal amplitude modulation into separate notes.
// contours.py prints these when it regenerates the table.
//
//                        pitch    len    harm  rough   rate
constexpr SpeciesTraits kSpecies[kNumSpecies] = {
   /* Whistler   */ {3728.0f, 0.094f, 1.0f, -34.0f, 218.0f},
   /* Sparrow    */ {3491.0f, 0.071f, 1.0f, -32.0f, 303.0f},
   /* Warbler    */ {2812.0f, 0.078f, 1.0f, -30.0f, 321.0f},
   /* Budgie     */ {1353.0f, 0.066f, 3.0f, -27.0f, 293.0f},
   /* Woodpecker */ {2550.0f, 0.129f, 2.0f, -30.0f, 344.0f},
   /* Crane      */ {821.0f, 0.137f, 5.0f, -25.0f, 233.0f},
   /* Goose      */ {528.0f, 0.178f, 6.0f, -25.0f, 182.0f},
   // Screech is invented and its four voice figures stay as configured; only
   // lengthSec follows its archetypes, because those did change.
   /* Screech    */ {1800.0f, 0.208f, 6.0f, -16.0f, 150.0f},
   /* Piper      */ {2559.0f, 0.059f, 2.0f, -33.0f, 527.0f},
};

// How much of each cycle the valve is shut, for a given harmonic count.
//
// At zero the valve never closes and a pure sine comes out, which is what 59 %
// of the library's syllables are. Closing it makes the airflow a one-sided
// pulse, and a one-sided pulse has both even and odd harmonics -- which is the
// one thing the first version got right and the reason it is kept: a symmetric
// oscillator has no even harmonics at all, whatever it is driven with.
//
// Measured on the engine's own output with the same estimator that counted the
// references' harmonics, at 900 Hz with no breath:
//
//     closure     0.00  0.13  0.26  0.39  0.52  0.65  0.78  0.91
//     harmonics      1     2     3     3     3     4     5     5
//
// which fits harmonics = 1 + 5.5 * closure^0.6 and saturates near five. The
// valve alone does not reach the twelve harmonics the noisiest references
// measure -- see the plugin's TODO -- so the species table's harmonic counts
// above six are met approximately and the rest is Breath and Rasp.
// tools/analysis/fit.py --voice reprints the sweep.
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
   static constexpr int kN = ChirpEngine::kContourPoints;
   static constexpr int kH = ChirpEngine::kHarmPoints;
   float pitch[kNumContours][kN];            // octaves about the loudest moment
   float level[kNumContours][kN];            // linear, peak 1
   float harm[kNumContours][kHarmonics][kH]; // linear, partials sum to unit power

   ContourTables() {
      constexpr float kPi = 3.14159265358979f;
      for (int c = 0; c < kNumContours; ++c) {
         for (int i = 0; i < kN; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(kN);
            float p = 0.0f;
            for (int k = 0; k < kPitchTerms; ++k)
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

// The archetype an index lands on, within one species' own set. Species with
// too few usable references to cluster fall back to Sparrow's, which is the
// group with 1282 of them.
inline int archetypeFor(int species, float where) {
   const int sp = clampi(species, 0, kNumSpecies - 1);
   ContourRange r = kContourRange[sp];
   if (r.count <= 0)
      r = kContourRange[kSpeciesSparrow];
   const int k = static_cast<int>(clampf(where, 0.0f, 0.99999f) * static_cast<float>(r.count));
   return r.first + clampi(k, 0, r.count - 1);
}

// Bends a syllable's own time axis. The standard bias curve: one divide, and
// k = 1 leaves it alone.
inline float warpTime(float t, float k) {
   return t / (t + (1.0f - t) * k + 1.0e-9f);
}

// A soft hinge, for the one-sided valve. max(g, 0) is what aliases; widening
// the corner with frequency is both the cure and what a real valve does, since
// it cannot snap shut arbitrarily fast. The width matters: too much of it and
// the pulse rounds back into the sine it came from.
inline float softHinge(float g, float w) {
   return 0.5f * (g + std::sqrt(g * g + w * w));
}

// The frequency the radiation derivative is referenced to, so that `Radiate`
// gives the model's a1*x + a2*x' with a fixed a2 -- a +6 dB/octave tilt about
// the library's median pitch rather than a gain that changes with the note.
constexpr float kRadiateRefHz = 2580.0f;

// The valve's output is bounded, so one calibration constant turns it into a
// level. Measured rather than chosen: a single syllable at Shot Level 0 dB,
// full velocity and no distance came out at -15.1 dBFS with this at 0.35, so
// 1.20 puts it a little under -5 dBFS -- loud enough to use on its own, with
// room left for a flock of them before the output stage has to saturate.
constexpr float kSyllableNorm = 1.20f;

// The same for a drum strike, so a roll at Drum Level 0 dB sits where a phrase
// at Shot Level 0 dB does.
constexpr float kStrikeNorm = 3.5f;

// The additive path against the valve path, so that `Partials` is a change of
// timbre and not of level. Measured: at 1.9 the additive path came out 7.6 dB
// hotter than the valve.
constexpr float kPartialNorm = 0.79f;

// How much the valve is ducked as the partials come in. A full crossfade, after
// trying the alternative: laying the partials *over* a ducked valve was tried
// and is worse on both counts -- it moved the balance barely at all (a sparrow's
// drift went 1.0 to 1.2 dB instead of 1.0 to 3.2) and it still thinned the
// corvids. See the comment at the blend for what the crossfade costs.
constexpr float kPartialDuck = 1.0f;

// Breath against measured roughness. Rendering a whistle at a sweep of Breath
// and measuring its spectral flatness with the same estimator that measured the
// references:
//
//     effective breath   0    4.3 %  12.9 %  25.9 %
//     roughness       -31.5  -27.8   -22.0   -17.7  dB
//
// which is 13 dB per decade above the floor. *Effective* breath: the species
// multiplier below scales the parameter, and the first version of this
// calibration measured the parameter while the multiplier was silently scaling
// it -- on a Whistler, whose multiplier is 0.42 -- so the slope came out at
// 15.7 and the default landed four times too high. tools/analysis/fit.py
// --breath now reports both numbers.
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
      c.osc = 0.0f;
      c.prevFlow = 0.0f;
      c.smoothed = 0.0f;
      c.primed = false;
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
   mSpeciesRateMul = sp.ratePerMin / kLibraryRatePerMin;
   mSpeciesClosure = closureForHarmonics(sp.harmonics);
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
   free->osc = 0.0f;
   free->prevFlow = 0.0f;
   free->smoothed = 0.0f;
   free->primed = false;
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

   // Drawn once per syllable, from the phrase's own generator, so a phrase is
   // reproducible from its seed however many other birds are sounding.
   const float rp = ph.rng.white();
   const float rl = ph.rng.white();
   const float rc = ph.rng.white();
   const float rd = ph.rng.white();

   // ------------------------------------------------------------- the contour
   //
   // Which measured syllable this is. Contour walks across the species' set,
   // and Variation lets a phrase wander off it -- which is what stops a phrase
   // being one shape repeated.
   const float where = clampf(mP.contour + 0.45f * var * rc, 0.0f, 1.0f);
   c.archetype = archetypeFor(mP.species, where);
   const Contour &ct = kContours[c.archetype];

   // Motif: the pitch steps by a fixed interval from one syllable to the next.
   const float motif = ph.motifSemis * static_cast<float>(ph.index);
   const float pitch = clampf(ph.pitchHz * std::exp2((motif + 4.0f * var * rp) / 12.0f),
                              40.0f, 0.45f * sr);
   // The table holds shape only, about the syllable's own centre, so placing it
   // is a transposition. Held in octaves: the readout adds the contour to it and
   // takes one exp2 rather than a multiply and an exp2.
   c.pitchOct = std::log2(pitch);

   c.depth = clampf(mP.sweep * std::exp2(0.5f * var * rd), 0.0f, 4.0f);
   // Skew bends the syllable's time. 0.5 leaves the measured contour alone.
   c.warp = std::exp2(3.0f * (2.0f * clampf(mP.skew, 0.0f, 1.0f) - 1.0f));

   // The archetype plays at its *own* measured duration, scaled by Length.
   //
   // `Contour::durationSec` has been measured and stored for every archetype
   // since the table existed and was never read: the length came from the
   // species median alone, so every curve was stretched to the same target and
   // a species could only vary its syllable length by the +-2.3x that Variation
   // gives. One nightingale recording spans 21 to 1296 ms -- 60x -- and that
   // range is most of what makes it sound like a bird rather than a machine.
   //
   // At the species' median archetype `durationSec / kLibraryLengthSec` equals
   // the species length multiplier this replaces, so a median syllable is timed
   // exactly as before and the change is entirely in the spread around it. It
   // also removes a trap: that multiplier silently rescaled every preset's
   // Length whenever a species' measured median moved, which is what shortened
   // Nightingale Trill by 20 % at 0.5.0 and broke it into separate tones.
   float len = mP.lengthSec * (ct.durationSec / kLibraryLengthSec) * b.lengthMul *
               std::exp2(0.7f * var * rl);

   // A syllable longer than the nominal interval takes the time it needs: one
   // bird cannot overlap itself, and truncating the curve is what produced the
   // clipped, mechanical phrases. Everything at or under the pace keeps the
   // interval, so Syllable Rate still means what it says.
   ph.slot = std::max(ph.interval, clampf(len, 0.004f, 8.0f));

   // Legato: the syllable is stretched towards filling its own slot. 70 % of the
   // library's syllable pairs have no silence between them at all, so this
   // defaults high. It only applies where there is something to run into.
   const float leg = ph.remaining > 1 ? clampf(mP.legato, 0.0f, 1.0f) : 0.0f;
   len = len * (1.0f - leg) + ph.slot * leg;
   len = clampf(len, 0.004f, 8.0f);
   c.phaseInc = 1.0f / (len * sr);

   // Detail: a one-pole on the contour as it is read out. At the top the
   // measured curve passes through; lower down the scribble smooths towards a
   // glide, which is what the first version of this plugin could only do.
   const float detail = clampf(mP.detail, 0.0f, 1.0f);
   const float corner = 8.0f * std::pow(400.0f, detail); // 8 Hz .. 3.2 kHz
   c.smoothCoef = detail >= 0.999f ? 1.0f : onePoleCoef(1.0f / corner, sr);
   c.smoothed = 0.0f;
   c.primed = false;

   // --------------------------------------------------------------- the voice
   //
   // How much of each cycle the valve is shut. The species' measured harmonic
   // count sets it and Voice moves it; a narrow pulse at a high pitch would
   // alias, and a real syrinx cannot snap shut arbitrarily fast either, so the
   // ceiling falls with frequency.
   float closure = mSpeciesClosure + (clampf(mP.voice, 0.0f, 1.0f) - 0.30f) * 1.3f +
                   0.5f * mP.voiceSpread * b.voiceOff;
   const float ceiling = 0.92f - 2.2f * pitch / sr;
   c.closure = clampf(closure, 0.0f, clampf(ceiling, 0.0f, 0.92f));
   c.closureNow = c.closure;
   c.rasp = clampf(mP.rasp, 0.0f, 1.0f);
   // Scaled by how much of that syllable's energy the harmonic measurement
   // actually accounted for. An archetype with a second bird in it, or an
   // inharmonic one, has a low share and simply does not respond much -- which
   // degrades honestly instead of asserting a balance it never measured.
   c.partials = clampf(mP.partials, 0.0f, 1.0f) * clampf(ct.harmFit, 0.0f, 1.0f);
   c.prevFlow = 0.0f;
   c.osc = 0.0f;
   c.dcBlock.reset();
   c.dcBlock.setCutoff(clampf(0.35f * pitch, 60.0f, 900.0f), sr);

   c.jitter = clampf(mP.jitter, 0.0f, 1.0f) * 0.05f;
   c.walkCoef = onePoleCoef(0.006f, sr);
   c.walk = 0.0f;

   c.pulseDepth = clampf(mP.pulseDepth, 0.0f, 1.0f);
   c.pulseInc = clampf(mP.pulseRateHz, 0.1f, 400.0f) / sr;
   c.pulsePhase = 0.25f * ph.rng.uniformPositive();

   c.breath = clampf(mP.breath * mSpeciesBreathMul, 0.0f, 1.0f);
   c.formant = clampf(mP.formant, 0.0f, 1.0f);
   c.radiate = clampf(mP.radiate, 0.0f, 1.0f);

   c.tractHz = clampf(mFormantHz, 60.0f, 0.45f * sr);
   c.tractReso = resonanceFor(mFormantQ);
   c.tractTrack = clampf(mP.beak, 0.0f, 1.0f);
   c.tractCounter = 0;
   c.tract.setCutoff(c.tractHz, c.tractReso, sr);
   // The valve's own corner and the tract's skirt both fall at 6 dB/octave, so
   // without this the top of the spectrum is the filter rather than the bird.
   c.top.setCutoff(clampf(pitch * (3.0f + 22.0f * c.closure), 1200.0f, 0.45f * sr), sr);

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
   const ContourTables &tab = contourTables();
   constexpr int kLast = ChirpEngine::kContourPoints - 2;
   const float kDeriv = sr / (6.2831853f * kRadiateRefHz);

   for (auto &c : mChirps) {
      if (!c.active)
         continue;
      const float *pitchTab = tab.pitch[c.archetype];
      const float *levelTab = tab.level[c.archetype];
      const float (*harmTab)[ChirpEngine::kHarmPoints] = tab.harm[c.archetype];
      const float nyquist = 0.45f * sr;

      for (uint32_t i = 0; i < numSamples; ++i) {
         // ------------------------------------------------------- the contour
         //
         // The whole syllable is these few lines: read a measured pitch curve
         // and a measured level curve out over the syllable's own duration.
         // Everything the first version tried to derive from two sinusoids --
         // the shape, the sweep, the turns, the envelope's asymmetry -- is in
         // the tables, because it was measured off a real bird.
         const float t = warpTime(clampf(c.phase, 0.0f, 1.0f), c.warp);
         const float x = t * static_cast<float>(ChirpEngine::kContourPoints - 1);
         int k = static_cast<int>(x);
         k = k < 0 ? 0 : (k > kLast ? kLast : k);
         const float frac = x - static_cast<float>(k);

         float oct = pitchTab[k] + frac * (pitchTab[k + 1] - pitchTab[k]);
         // Detail smooths the contour as it is read. Primed on the first sample
         // so a syllable does not glide in from wherever the smoother was.
         if (!c.primed) {
            c.smoothed = oct;
            c.primed = true;
         }
         c.smoothed += c.smoothCoef * (oct - c.smoothed);
         oct = c.smoothed;

         const float lvl = levelTab[k] + frac * (levelTab[k + 1] - levelTab[k]);

         c.walk += c.walkCoef * (c.jitter * c.rng.white() - c.walk);
         const float f0 = std::exp2(c.pitchOct + oct * c.depth + c.walk);

         // --------------------------------------------------------- the valve
         //
         // A phase accumulator at the contour's frequency, through a one-sided
         // valve: air passes only while the labia are apart. `closure` is the
         // fraction of the cycle they are shut, so at zero this passes a pure
         // sine -- which is what 59 % of the library's syllables are -- and
         // closing it grows the harmonic stack a corvid has, evens as well as
         // odds. A symmetric oscillator has no even harmonics at all.
         c.osc += clampf(f0 * invSr, 0.0f, 0.49f);
         if (c.osc >= 1.0f) {
            c.osc -= 1.0f;
            // Rasp: the contact is not the same twice. Irregular closure is
            // what a corvid's rasp physically is, and it is broadband in a way
            // that no amount of extra harmonics is.
            c.closureNow = clampf(c.closure + c.rasp * 0.45f * c.rng.white(), 0.0f, 0.93f);
         }
         const float sine = sin2piFast(c.osc);
         const float gap = sine + (1.0f - 2.0f * c.closureNow);
// The corner is what aliases, so it is widened with frequency -- but only
         // just enough. At 0.12 + 7*f/sr it rounded a narrow pulse back into a
         // sine and Voice could not reach past four harmonics at any setting.
         const float w = 0.010f + 3.0f * f0 * invSr;
         const float flow = softHinge(gap, w);

         // The radiated pressure of a small source follows the rate of change
         // of the flow rather than the flow, which tilts the harmonics up by
         // 6 dB an octave. Referenced to a fixed frequency so `Radiate` is a
         // tilt and not a gain that changes with the note.
         const float dflow = (flow - c.prevFlow) * kDeriv;
         c.prevFlow = flow;

         float y = (1.0f - c.radiate) * flow + c.radiate * dflow;
         // A one-sided flow has a mean, and that mean is modulated at the
         // syllable rate, which is a thump rather than a bird.
         y = c.dcBlock.tick(y);

         // ----------------------------------------------- the tube and the beak
         const float bp = c.tract.bandpassNormalised(y);
         y += c.formant * 1.2f * bp;

         // --------------------------------------------- the measured partials
         //
         // The other half of the timbre, and the half the valve cannot reach.
         // Six partials of the same phase accumulator, at the balance this
         // archetype's own recording had at this point in the syllable. The
         // partials are phase-locked to the fundamental because a harmonic
         // source is, and sin2piFast wraps its argument, so the h-th one is
         // free.
         if (c.partials > 1.0e-4f) {
            const float hx = t * static_cast<float>(ChirpEngine::kHarmPoints - 1);
            int hk = static_cast<int>(hx);
            hk = hk < 0 ? 0 : (hk > ChirpEngine::kHarmPoints - 2
                                  ? ChirpEngine::kHarmPoints - 2
                                  : hk);
            const float hfrac = hx - static_cast<float>(hk);
            float add = 0.0f;
            for (int h = 0; h < kHarmonics; ++h) {
               // Stop at Nyquist rather than folding a partial back into the
               // band. A high syllable simply has fewer of them, which is also
               // true of the bird.
               if (f0 * static_cast<float>(h + 1) > nyquist)
                  break;
               const float *ht = harmTab[h];
               const float a = ht[hk] + hfrac * (ht[hk + 1] - ht[hk]);
               add += a * sin2piFast(c.osc * static_cast<float>(h + 1));
            }
            // A crossfade, and it has a known cost. The archetypes that
            // survived the contour quality gate are the *cleanest* syllables of
            // each species -- the gate selects for tonality -- so their measured
            // balance is more fundamental-dominated than a typical bird of that
            // species is. Turning this up therefore makes a corvid's timbre
            // evolve the way a real one's does (its drift goes from 1.7 to
            // 2.7 dB against the references' 4.4) while reducing its harmonic
            // count from 4.3 to 3.0.
            //
            // That is not the engine exaggerating: the archetype's spectrum is
            // what that syllable actually had. It is the library, and the fix is
            // a wider gate rather than a thumb on this scale -- see TODO.
            y = y * (1.0f - kPartialDuck * c.partials) + add * kPartialNorm * c.partials;
         }

         y += c.breath * 0.8f * lvl * c.rng.white();
         y = c.top.tick(y);
         y = c.air.tick(y);

         // The tract resonance follows the pitch, by as much as the beak is
         // open: songbirds track the frequency they are producing with their
         // gape. Updated every 16 samples -- a formant does not need a tan() a
         // sample.
         if ((c.tractCounter++ & 15u) == 0u) {
            const float track = c.tractTrack * (std::log2(std::max(f0, 20.0f)) - c.pitchOct);
            c.tract.setCutoff(clampf(c.tractHz * std::exp2(track), 60.0f, 0.45f * sr),
                              c.tractReso, sr);
         }

         c.pulsePhase += c.pulseInc;
         if (c.pulsePhase >= 1.0f)
            c.pulsePhase -= 1.0f;
         const float pulse =
            1.0f - c.pulseDepth * 0.5f * (1.0f - sin2piFast(c.pulsePhase + 0.25f));

         const float amp = c.level * lvl * pulse;
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
               p.timer += static_cast<double>(clampf(p.drum ? p.interval : p.slot,
                                                     0.002f, 30.0f));
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
