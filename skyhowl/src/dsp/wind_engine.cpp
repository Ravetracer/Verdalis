#include "wind_engine.h"

#include "verdalis/dsp/fastmath.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

namespace skyhowl {

namespace {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Strouhal's relation, which is the whole reason wind has a pitch at all. A
// bluff body in a flow sheds vortices alternately from each side at
//
//    f = St * U / d,    St ~ 0.2 for a cylinder over the whole Reynolds range
//                       that matters outdoors (300 < Re < 2e5),
//
// so with d in millimetres this is f = 200 U / d: a 2 mm twig in an 8 m/s wind
// sheds at 800 Hz, and the library's tonal recordings imply diameters of
// 0.7-13 mm.
constexpr float kStrouhal = 0.2f;
inline float sheddingHz(float speedMs, float diameterMm) {
   return 1000.0f * kStrouhal * speedMs / clampf(diameterMm, 0.05f, 1000.0f);
}

// What a leaf of a given size radiates. A flat plate a wavelength across
// couples to the air, which puts a leaf of dimension L around c / 2L -- so a
// 40 mm leaf clicks at about 4.2 kHz, against a measured median onset centroid
// of 4.2 kHz across the library's foliage recordings.
inline float leafHz(float sizeMm) {
   return 343000.0f * 0.5f / clampf(sizeMm, 1.0f, 2000.0f);
}

// Svf::setCutoff takes resonance as 0..1, which it maps to k = 1/Q over
// 2..0.02. Passing a Q straight in silently clamps to maximum resonance, which
// turns a noise band into a whistle; convert properly instead.
inline float resonanceFor(float q) {
   const float k = 1.0f / std::max(0.5f, q);
   return clampf((2.0f - k) / 1.98f, 0.0f, 1.0f);
}

// A smooth 0..1 ramp, for anything that has to come in without a step.
inline float smoothstep(float x) {
   const float t = clampf(x, 0.0f, 1.0f);
   return t * t * (3.0f - 2.0f * t);
}

// -------------------------------------------------------------------- terrain
//
// Terrain roughness is not decoration: it is where turbulence comes from. The
// logarithmic wind profile gives a turbulence intensity of about
//
//    I = 1 / ln(z / z0)
//
// at height z over ground of roughness length z0, and z0 is tabulated
// (Wieringa 1992): open water 0.0002 m, sand 0.0003, snow 0.005, short grass
// 0.03, long grass 0.1, broken country 0.5, forest 1.0, city 1.5. At a
// listening height of 10 m that gives intensities of 0.09 to 0.53, and the
// factors below are those normalised to short grass -- so choosing Forest
// really does make the wind two and a half times as gusty as choosing Plain,
// by the same arithmetic a wind engineer would use.
struct TerrainTraits {
   float turbulence; // multiplies Turbulence and Gust Depth alike
   float tiltAdd;    // dB/octave added to Flow Tilt
   float buffet;     // multiplies Buffet
   float hiss;       // multiplies Hiss
   // How much is close enough to reflect. Open ground has nothing: the sound
   // goes away over the fields and does not come back, and early reflections
   // that make a reverb read as a room have to be absent or it sounds like a
   // corridor however large the tail is. A street has walls on both sides.
   float enclosure;
};

constexpr TerrainTraits kTerrainTraits[kNumTerrains] = {
   /* Plain    */ {1.00f, 0.0f, 1.00f, 1.00f, 0.02f},
   /* Meadow   */ {1.26f, -1.0f, 0.90f, 1.10f, 0.03f},
   /* Forest   */ {2.52f, -2.5f, 1.35f, 0.85f, 0.18f},
   /* Mountain */ {1.94f, 1.0f, 1.10f, 1.15f, 0.30f},
   /* Desert   */ {0.56f, 2.0f, 0.70f, 1.35f, 0.02f},
   /* Coast    */ {0.54f, 0.0f, 1.20f, 1.05f, 0.05f},
   /* Street   */ {3.06f, -1.5f, 1.15f, 0.95f, 0.85f},
   /* Tundra   */ {0.77f, 1.5f, 0.85f, 1.20f, 0.02f},
};

// ------------------------------------------------------------------- obstacle
//
// Two families, and the difference between them is physics rather than
// voicing. A bluff body sheds at St U / d, so its pitch is proportional to the
// wind speed. A cavity -- a slot under a door, a cave mouth -- resonates at a
// frequency its own geometry fixes and the flow merely excites, so its pitch
// barely moves however hard it blows. `track` is how much of Howl Track
// survives, and it is what separates a howl that swoops from a drone that
// does not.
//
// The library cannot settle this on its own: tracking is positive in 29 of its
// 31 tonal recordings whatever their Q, and the one cave recording (Q 7.5,
// correlation -0.07) is a single case rather than a trend. The distinction is
// kept because it is sound physics, not because it was measured.
struct ObstacleTraits {
   float level;     // multiplies Howl Amount
   float sizeMul;   // multiplies Howl Size
   float qMul;      // multiplies the Q that Howl Resonance asks for
   float spreadMul; // multiplies Howl Spread
   float track;     // how much of Howl Track applies
};

// The size multipliers are deliberately modest, at both ends. An obstacle that
// multiplied Howl Size tenfold would put the whole useful part of its pitch
// range below the parameter's own minimum, so a cave would be untunable
// however the knob was set; four is enough to be two octaves below a twig and
// still leave the range reachable. At the other end, grass was 0.25 -- a blade
// really is that thin -- and the result was a bank of tones in the top octave
// of the spectrum, 25 dB above what any reference holds up there. Half is
// thin enough.
constexpr ObstacleTraits kObstacleTraits[kNumObstacles] = {
   /* Open     */ {0.15f, 1.0f, 0.55f, 2.00f, 1.00f},
   /* Grass    */ {0.55f, 0.50f, 0.80f, 1.40f, 1.00f},
   /* Reeds    */ {0.85f, 1.6f, 1.30f, 0.80f, 1.00f},
   /* Twigs    */ {1.00f, 1.0f, 1.00f, 1.00f, 1.00f},
   /* Branches */ {0.90f, 4.0f, 1.00f, 0.90f, 1.00f},
   /* Wires    */ {1.10f, 1.2f, 2.40f, 0.25f, 1.00f},
   /* Rocks    */ {0.60f, 6.0f, 0.50f, 1.60f, 0.85f},
   /* Gap      */ {1.00f, 2.0f, 2.80f, 0.35f, 0.15f},
   /* Cave     */ {1.15f, 4.0f, 3.20f, 0.30f, 0.08f},
};

// -------------------------------------------------------------------- foliage
//
// What the wind is heard through. The numbers are ratios rather than
// measurements: the library's foliage recordings agree on the band (a median
// onset centroid of 4.2 kHz over 2.8-6.6 kHz) and on the resolvable rate
// (15-40 clicks a second), but they do not name their species, so the spread
// between types is voiced from the two that are unambiguous -- dry leaves,
// which clatter at a flux variation of 0.80, and a merged conifer-like hiss at
// 0.20.
struct FoliageTraits {
   float level;
   float toneMul;    // multiplies the leaf's own frequency
   float decayMul;   // multiplies Rustle Decay
   float clatterAdd; // added to Clatter
   float densityMul; // multiplies Rustle Density
   float onsetMul;   // multiplies Rustle Onset
   float spreadMul;  // multiplies Rustle Spread
};

// The tone multipliers are kept close to unity on purpose. An earlier version
// gave grass 2.8, which is defensible -- a blade is small and rings high --
// but it meant that fitting a preset to a measured onset centroid asked for a
// leaf 170 mm across, and a parameter that has to be set to an implausible
// number to produce the right sound is not a parameter, it is a fudge. The
// size now reads as the size, and the type only tilts it.
constexpr FoliageTraits kFoliageTraits[kNumFoliages] = {
   /* None          */ {0.00f, 1.00f, 1.00f, 0.00f, 1.00f, 1.00f, 1.00f},
   /* Broadleaf     */ {1.00f, 1.00f, 1.00f, 0.00f, 1.00f, 1.00f, 1.00f},
   /* Dry Leaves    */ {1.10f, 1.15f, 1.70f, 0.35f, 1.20f, 0.80f, 1.20f},
   /* Conifer       */ {0.80f, 1.50f, 0.55f, -0.25f, 3.00f, 0.90f, 0.70f},
   /* Grass         */ {0.70f, 1.20f, 0.40f, -0.15f, 4.00f, 0.70f, 0.80f},
   /* Reeds         */ {0.90f, 0.80f, 1.40f, 0.20f, 0.70f, 1.00f, 0.90f},
   /* Bare Branches */ {0.75f, 0.60f, 2.20f, 0.45f, 0.35f, 1.30f, 1.30f},
   /* Bushes        */ {0.95f, 1.20f, 0.80f, -0.05f, 2.00f, 0.85f, 1.10f},
};

// Seed 0 is the "always different" setting and has no fixed mapping; every
// non-zero Seed maps here, so two instances never disagree about what a given
// Seed means.
uint32_t rngStateForSeed(int seed) {
   return 0x9E3779B9u * static_cast<uint32_t>(seed) + 17u;
}

std::atomic<uint32_t> gInstanceCounter{0};

} // namespace

void WindEngine::prepare(double sampleRate, uint32_t /*maxBlockSize*/) {
   mSampleRate = sampleRate;
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   mRng.reseed(static_cast<uint32_t>(now) ^
               (0x9E3779B9u * (gInstanceCounter.fetch_add(1) + 1)));
   mAppliedSeed = 0;

   // The buffet runs lower than surf rumble and about as low as thunder, so
   // the tank's loop highpass has to sit under it rather than over it.
   mSpace.prepare(static_cast<float>(sampleRate), 22.0f);
   reset();
   updateFilters();
}

void WindEngine::reset() {
   for (auto &v : mVoices) {
      v.active = false;
      v.env.reset();
      for (auto &t : v.turb)
         t.reset();
      v.squallWalk.reset();
      v.howlSpeed.reset();
      v.squallPhase = 0.0f;
      for (int i = 0; i < 4; ++i) {
         v.tiltL[i].reset();
         v.tiltR[i].reset();
      }
      v.bedHpL.reset();
      v.bedHpR.reset();
      v.buffetLpL.reset();
      v.buffetLpR.reset();
      v.hissHpL.reset();
      v.hissHpR.reset();
      v.hissLpL.reset();
      v.hissLpR.reset();
      for (auto &h : v.howl) {
         h.band.reset();
         h.top.reset();
         h.walk = 0.0f;
         h.gain = 0.0f;
         h.gainStep = 0.0f;
      }
      v.bedGainL = v.bedGainR = 1.0f;
      v.bedStepL = v.bedStepR = 0.0f;
      v.buffetGainL = v.buffetGainR = 1.0f;
      v.buffetStepL = v.buffetStepR = 0.0f;
      v.modCounter = 0;
      v.gustTimer = 0.0;
      v.leafTimer = 0.0;
      v.rustleGate = 0.0f;
   }
   for (auto &g : mGusts)
      g.active = false;
   for (auto &l : mLeaves)
      l.active = false;
   for (auto &v : mVents)
      v.active = false;

   // A non-zero Seed promises the same wind every time, so starting over has
   // to start the sequence over too. Seed 0 deliberately keeps running.
   if (mP.seed != 0)
      mRng.reseed(rngStateForSeed(mP.seed));
   mGustCounter = 0;

   mSpace.clear();
   mOutFilterL.reset();
   mOutFilterR.reset();
   mOutHpL.reset();
   mOutHpR.reset();
   mAirLpL.reset();
   mAirLpR.reset();
   mDistanceTiltL.reset();
   mDistanceTiltR.reset();
   mSilenceCounter = 0.0f;
}

void WindEngine::setParams(const EngineParams &p) {
   mP = p;
   if (mP.seed != mAppliedSeed) {
      mAppliedSeed = mP.seed;
      if (mP.seed != 0)
         mRng.reseed(rngStateForSeed(mP.seed));
   }
   updateFilters();
}

void WindEngine::updateFilters() {
   const float sr = static_cast<float>(mSampleRate);

   // Distance does two things at once, the way it does outdoors: it takes the
   // top off (air absorption, and more of it in damp air) and it tilts what is
   // left downwards. The library's most distant recording is 117 dB down at
   // 16 kHz and peaks at 63 Hz, which is what this has to be able to reach.
   const float d = clampf(mP.distance, 0.0f, 1.0f);
   const float airKeep = clampf(mP.air, 0.0f, 1.0f);
   const float airHz = 20000.0f * std::pow(0.04f, d * (1.35f - 0.7f * airKeep));
   mAirLpL.setCutoff(clampf(airHz, 250.0f, 20000.0f), sr);
   mAirLpR.setCutoff(clampf(airHz, 250.0f, 20000.0f), sr);
   const float tiltHz = 20000.0f * std::pow(0.2f, d);
   mDistanceTiltL.setCutoff(clampf(tiltHz, 500.0f, 20000.0f), sr);
   mDistanceTiltR.setCutoff(clampf(tiltHz, 500.0f, 20000.0f), sr);

   mOutHpL.setCutoff(clampf(mP.highpassHz, 10.0f, 4000.0f), sr);
   mOutHpR.setCutoff(clampf(mP.highpassHz, 10.0f, 4000.0f), sr);

   const float reso = clampf(0.05f + 0.90f * mP.filterReso, 0.0f, 0.98f);
   mOutFilterL.setCutoff(clampf(mP.filterCutoffHz, 20.0f, 0.45f * sr), reso, sr);
   mOutFilterR.setCutoff(clampf(mP.filterCutoffHz, 20.0f, 0.45f * sr), reso, sr);

   // Howl Resonance reads as 0..1 and means a Q from 0.8 to 12. The library
   // measures 0.9-9.7 with a median of 2.1, so the default sits just above the
   // bottom third: a howl is a narrow band of noise, not a whistle.
   mHowlQ = 0.8f * std::pow(15.0f, clampf(mP.howlReso, 0.0f, 1.0f));

   // The bed's slope, in poles. Each one-pole is 6 dB/octave, and the four are
   // crossfaded, so any slope from flat to -24 dB/octave is available.
   mFlowSlopeMix = clampf(-mP.flowTiltDbOct / 6.0f, 0.0f, 4.0f);

   // The turbulence process, normalised to unit variance. A one-pole fed white
   // noise of unit variance has variance c / (2 - c), so the three weighted
   // outputs sum to a known variance and the reciprocal square root of it is
   // what makes Turbulence mean what it says.
   const float gustHz = clampf(mP.gustRatePerMin / 60.0f, 0.001f, 4.0f);
   float var = 0.0f;
   for (int i = 0; i < 3; ++i) {
      const float corner = gustHz * std::pow(4.0f, static_cast<float>(i));
      const float c = clampf(1.0f - std::exp(-6.283185307f * corner /
                                             (sr / static_cast<float>(kModInterval))),
                             1.0e-5f, 1.0f);
      var += mTurbWeight[i] * mTurbWeight[i] * c / (2.0f - c);
   }
   mTurbNorm = 1.0f / std::sqrt(std::max(1.0e-9f, var));

   mSpace.setSize(mP.spaceSize);
   mSpace.setDamping(mP.spaceDamping);
   mSpace.setEnclosure(kTerrainTraits[clampi(mP.terrain, 0, kNumTerrains - 1)].enclosure);
}

void WindEngine::noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId,
                        double velocity) {
   Voice *free = nullptr;
   for (auto &v : mVoices) {
      if (!v.active) {
         free = &v;
         break;
      }
   }
   if (!free) {
      // Steal the quietest voice; wind is a bed, so the least audible one is
      // the one nobody will miss.
      float lowest = 1.0e9f;
      for (auto &v : mVoices) {
         if (v.env.level() < lowest) {
            lowest = v.env.level();
            free = &v;
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

   const float sr = static_cast<float>(mSampleRate);
   const float ctrl = sr / static_cast<float>(kModInterval);
   v.env.setParams(mP.attackSec, mP.decaySec, mP.sustain, mP.releaseSec, sr);
   v.env.gateOn();

   for (auto &t : v.turb)
      t.reset();
   v.squallWalk.reset();
   // Started at the mean rather than at zero, or the first quarter second of
   // every note would sweep up from nothing.
   v.howlSpeed.reset();
   v.howlSpeed.setCutoff(0.65f, ctrl);
   for (int i = 0; i < 4; ++i) {
      v.tiltL[i].reset();
      v.tiltR[i].reset();
   }
   v.bedHpL.reset();
   v.bedHpR.reset();
   v.bedHpL.setCutoff(24.0f, sr);
   v.bedHpR.setCutoff(24.0f, sr);
   v.buffetLpL.reset();
   v.buffetLpR.reset();
   v.hissHpL.reset();
   v.hissHpR.reset();
   v.hissLpL.reset();
   v.hissLpR.reset();
   v.bedGainL = v.bedGainR = 0.0f;
   v.buffetGainL = v.buffetGainR = 0.0f;
   v.bedStepL = v.bedStepR = 0.0f;
   v.buffetStepL = v.buffetStepR = 0.0f;
   v.modCounter = 0;
   v.rustleGate = 0.0f;
   v.leafTimer = 0.0;

   // Deliberately not drawn from mRng. A note can arrive before the Seed
   // parameter has been applied -- events are handled in the order the host
   // sends them -- so anything drawn here would depend on when that happened,
   // and a fixed Seed would stop promising the same wind. The voice's own slot
   // and key give all the variation this needs, and give it deterministically.
   const uint32_t slot = static_cast<uint32_t>(&v - mVoices);
   const float spread = static_cast<float>((slot * 7u + static_cast<uint32_t>(key)) % 16u) / 16.0f;
   v.squallPhase = spread;
   v.squallInc = (mP.squallRatePerMin / 60.0f) / ctrl;
   v.squallWalk.setCutoff(clampf(0.25f * mP.squallRatePerMin / 60.0f, 0.002f, 1.0f), ctrl);

   // The first gust does not wait a full period: the wind is already blowing
   // when you arrive.
   v.gustTimer = static_cast<double>(60.0f / std::max(0.01f, mP.gustRatePerMin) * sr) *
                 static_cast<double>(0.05f + 0.4f * spread);

   for (uint32_t i = 0; i < kMaxHowl; ++i) {
      HowlVoice &h = v.howl[i];
      h.band.reset();
      h.top.reset();
      h.gain = 0.0f;
      h.gainStep = 0.0f;
      h.walk = 0.0f;
      // Vortex shedding wanders because the local velocity does, on a scale of
      // a second or two. Each obstacle wanders independently, which is what
      // makes a group of them a chorus rather than one detuned tone.
      h.walkCoef = onePoleCoef(0.7f + 0.35f * static_cast<float>(i), ctrl);
      h.rng.seed(0x51ED270Bu * (slot + 1u) + 0x9E3779B9u * (i + 3u));
      // Derived from the slot and the index rather than from mRng, for the same
      // reason noteOn draws nothing: a note can arrive before Seed has been
      // applied, and a fixed Seed has to promise the same wind.
      h.sizeOffset = (static_cast<float>((slot * 31u + i * 97u + 13u) % 64u) / 63.0f) - 0.5f;
      h.position =
         (static_cast<float>((slot * 17u + i * 53u + 7u) % 64u) / 63.0f) * 2.0f - 1.0f;
   }
}

void WindEngine::noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = (noteId >= 0 && v.noteId == noteId) ||
                         (noteId < 0 && v.port == port && v.channel == channel && v.key == key);
      if (match)
         v.env.gateOff();
   }
}

void WindEngine::choke(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = (noteId >= 0 && v.noteId == noteId) ||
                         (noteId < 0 && v.port == port && v.channel == channel && v.key == key);
      if (match) {
         v.env.kill();
         v.active = false;
      }
   }
}

void WindEngine::allSoundOff() {
   for (auto &v : mVoices) {
      v.env.kill();
      v.active = false;
   }
   for (auto &g : mGusts)
      g.active = false;
   for (auto &l : mLeaves)
      l.active = false;
   for (auto &v : mVents)
      v.active = false;
}

Gust *WindEngine::allocateGust() {
   const uint32_t cap = std::min<uint32_t>(kMaxGusts, std::max(1, mP.maxGusts));
   for (uint32_t i = 0; i < cap; ++i) {
      if (!mGusts[i].active)
         return &mGusts[i];
   }
   // The pool is full: take the weakest, which is the one furthest through its
   // own life.
   Gust *victim = &mGusts[0];
   float lowest = 1.0e9f;
   for (uint32_t i = 0; i < cap; ++i) {
      const float e = mGusts[i].env * mGusts[i].strength;
      if (e < lowest) {
         lowest = e;
         victim = &mGusts[i];
      }
   }
   return victim;
}

Leaf *WindEngine::allocateLeaf() {
   const uint32_t cap = std::min<uint32_t>(kMaxLeaves, std::max(8, mP.maxGusts * 6));
   for (uint32_t i = 0; i < cap; ++i) {
      if (!mLeaves[i].active)
         return &mLeaves[i];
   }
   return nullptr; // leaves are decoration; dropping one is free
}

void WindEngine::spawnGust(Voice &v) {
   Gust *slot = allocateGust();
   if (!slot)
      return;
   Gust &g = *slot;
   const float ctrl = static_cast<float>(mSampleRate) / static_cast<float>(kModInterval);
   const TerrainTraits &tt = kTerrainTraits[clampi(mP.terrain, 0, kNumTerrains - 1)];

   g.active = true;
   g.voice = static_cast<int>(&v - mVoices);
   g.env = 0.0f;
   g.rising = true;
   g.risePhase = 0.0f;

   // Gust strength is heavy-tailed on purpose: most gusts are unremarkable and
   // a few are much stronger, which is what a gust factor is a summary of.
   // Squaring a uniform draw is the cheapest distribution with that shape.
   const float u = mRng.uniform();
   g.strength = clampf(mP.gustDepth, 0.0f, 1.0f) * std::sqrt(tt.turbulence) *
                (0.30f + 1.15f * u * u);

   // Gust Shape decides how much of the length is the arrival. At the default
   // it is half and half, which is what the references measure: the median
   // rise/fall ratio of their envelopes is 1.01, so unlike a breaking wave a
   // gust is not a transient.
   const float length = clampf(mP.gustLengthSec, 0.05f, 60.0f) *
                        (0.55f + 0.9f * mRng.uniform());
   const float riseFrac = clampf(0.5f - 0.45f * (2.0f * mP.gustShape - 1.0f), 0.05f, 0.95f);
   g.riseInc = 1.0f / std::max(1.0f, length * riseFrac * ctrl);
   g.decayCoef = decayCoef(std::max(0.01f, length * (1.0f - riseFrac)), ctrl);

   g.pan = mRng.white();
   ++mGustCounter;
}

void WindEngine::spawnLeaf(float speed) {
   Leaf *slot = allocateLeaf();
   if (!slot)
      return;
   Leaf &l = *slot;
   const float sr = static_cast<float>(mSampleRate);
   const FoliageTraits &ft = kFoliageTraits[clampi(mP.foliage, 0, kNumFoliages - 1)];

   l.active = true;
   l.rng.seed(mRng.next() | 1u);
   l.band.reset();

   const float spread = clampf(mP.rustleSpreadOct * ft.spreadMul, 0.0f, 4.0f);
   const float hz = leafHz(mP.rustleSizeMm) * ft.toneMul *
                    std::exp2(spread * 0.5f * l.rng.white());
   const float clat = clampf(mP.clatter + ft.clatterAdd, 0.0f, 1.0f);
   // A dry leaf is not just louder, it is shorter: the clatter is a stiffer
   // leaf, and a stiffer leaf rings for less time.
   const float decaySec =
      clampf(mP.rustleDecaySec * ft.decayMul * (1.0f - 0.55f * clat), 0.0005f, 2.0f) *
      (0.6f + 0.8f * l.rng.uniformPositive());

   l.band.setCutoff(clampf(hz, 60.0f, 0.45f * sr), resonanceFor(1.6f + 3.0f * clat), sr);
   l.top.reset();
   l.top.setCutoff(clampf(hz * 1.5f, 100.0f, 0.45f * sr), sr);
   l.decayCoef = decayCoef(decaySec, sr);

   // Level follows how hard the wind is pushing, not just how often. A leaf in
   // a gust is struck harder.
   const float drive = clampf(speed / std::max(0.1f, mP.windSpeedMs), 0.0f, 3.0f);
   l.level = clampf(mP.rustleAmount, 0.0f, 1.0f) * ft.level * drive *
             (0.35f + 0.9f * l.rng.uniformPositive());

   // The transient: the moment of contact, before anything has had time to
   // ring. It is what a dry leaf is mostly made of.
   l.clickLevel = l.level * clat * 1.8f;
   l.clickCoef = decayCoef(0.0012f + 0.004f * (1.0f - clat), sr);

   const float pan = l.rng.white() * clampf(mP.width, 0.0f, 1.0f);
   l.panL = std::sqrt(clampf(0.5f * (1.0f - pan), 0.0f, 1.0f));
   l.panR = std::sqrt(clampf(0.5f * (1.0f + pan), 0.0f, 1.0f));
}

// The flow field: one update every kModInterval samples. Nothing here makes a
// sound -- it works out how fast the air is moving, and everything audible is
// driven from that.
void WindEngine::updateFlow(Voice &v, float envLevel, float velSpeed) {
   const float sr = static_cast<float>(mSampleRate);
   const float ctrl = sr / static_cast<float>(kModInterval);
   const int slot = static_cast<int>(&v - mVoices);
   const TerrainTraits &tt = kTerrainTraits[clampi(mP.terrain, 0, kNumTerrains - 1)];
   const ObstacleTraits &ot = kObstacleTraits[clampi(mP.obstacle, 0, kNumObstacles - 1)];

   const float mean = clampf(mP.windSpeedMs * velSpeed, 0.05f, 80.0f);

   // Turbulence: three one-poles weighted 4^(-5/6) apart, which sums to a
   // f^-5/3 spectrum across the band they span -- the inertial subrange.
   const float gustHz = clampf(mP.gustRatePerMin / 60.0f, 0.001f, 4.0f);
   float turb = 0.0f;
   for (int i = 0; i < 3; ++i) {
      const float corner = gustHz * std::pow(4.0f, static_cast<float>(i));
      v.turb[i].setCutoff(clampf(corner, 0.001f, 0.45f * ctrl), ctrl);
      turb += mTurbWeight[i] * v.turb[i].tick(mRng.white());
   }
   turb *= mTurbNorm * clampf(mP.turbulence, 0.0f, 1.0f) * tt.turbulence;

   // Squall: a drift far slower than the gusting. Half of it is periodic and
   // half a random walk, because sets of gusts are neither regular nor
   // memoryless.
   v.squallPhase += v.squallInc;
   if (v.squallPhase >= 1.0f)
      v.squallPhase -= std::floor(v.squallPhase);
   const float walk = v.squallWalk.tick(mRng.white());
   const float squall =
      clampf(mP.squall, 0.0f, 1.0f) * (0.35f * sin2piFast(v.squallPhase) + 1.4f * walk);

   // Gusts. Each is a scalar excess with a position, so a gust arriving from
   // the left raises the left channel's wind speed first.
   const uint32_t gustCap = std::min<uint32_t>(kMaxGusts, std::max(1, mP.maxGusts));
   const float width = clampf(mP.width, 0.0f, 1.0f);
   float gl = 0.0f, gr = 0.0f;
   for (uint32_t i = 0; i < gustCap; ++i) {
      Gust &g = mGusts[i];
      if (!g.active || g.voice != slot)
         continue;
      if (g.rising) {
         g.risePhase += g.riseInc;
         if (g.risePhase >= 1.0f) {
            g.risePhase = 1.0f;
            g.rising = false;
         }
         // A raised cosine, so a gust neither starts nor peaks with a corner:
         // 0.5 - 0.5 cos(pi t), written as a sine because that is the table
         // there is. The quarter-turn offset is what makes it run 0 to 1
         // rather than 0.5 to 1 and back.
         g.env = 0.5f - 0.5f * sin2piFast(0.25f + 0.5f * g.risePhase);
      } else {
         g.env *= g.decayCoef;
         if (g.finished()) {
            g.active = false;
            continue;
         }
      }
      const float e = g.env * g.strength;
      gl += e * (1.0f + 0.7f * g.pan * width);
      gr += e * (1.0f - 0.7f * g.pan * width);
   }

   const float base = 1.0f + squall + turb;
   v.speedL = clampf(mean * (base + gl), 0.02f, 200.0f);
   v.speedR = clampf(mean * (base + gr), 0.02f, 200.0f);
   const float speedM = 0.5f * (v.speedL + v.speedR);
   const float howlSpeedMs = v.howlSpeed.tick(speedM);

   // The level law. Aerodynamic sound power from flow over a rigid surface
   // goes as U^6 (Curle), so amplitude goes as U^3 and doubling the wind is
   // +18 dB. Speed Law scales the exponent, because a physically correct wind
   // is very hard to keep inside a mix.
   const float law = 3.0f * clampf(mP.speedLaw, 0.0f, 1.0f);
   const float ratioL = clampf(v.speedL / mean, 0.02f, 8.0f);
   const float ratioR = clampf(v.speedR / mean, 0.02f, 8.0f);
   const float lvl = envLevel * clampf(mP.flowGain, 0.0f, 4.0f);
   const float bedL = clampf(lvl * std::exp2(law * std::log2(ratioL)), 0.0f, 16.0f);
   const float bedR = clampf(lvl * std::exp2(law * std::log2(ratioR)), 0.0f, 16.0f);

   // The buffeting follows the dynamic pressure, U^2, so it grows more slowly
   // than the bed and a gust brightens rather than darkens. The library
   // disagrees -- its broadband centroid does not reliably rise with level
   // (median correlation -0.10) -- but most of what it is measuring there is
   // microphone pseudo-sound and branch drag, neither of which is radiated
   // wind. What is left to the preset is Buffet: raising it is what makes a
   // gust rumble instead of hiss.
   const float blaw = 2.0f * clampf(mP.speedLaw, 0.0f, 1.0f);
   const float blvl = envLevel * clampf(mP.buffetGain, 0.0f, 4.0f) * tt.buffet;
   const float bufL = clampf(blvl * std::exp2(blaw * std::log2(ratioL)), 0.0f, 16.0f);
   const float bufR = clampf(blvl * std::exp2(blaw * std::log2(ratioR)), 0.0f, 16.0f);

   const float inv = 1.0f / static_cast<float>(kModInterval);
   v.bedStepL = (bedL - v.bedGainL) * inv;
   v.bedStepR = (bedR - v.bedGainR) * inv;
   v.buffetStepL = (bufL - v.buffetGainL) * inv;
   v.buffetStepR = (bufR - v.buffetGainR) * inv;

   // The bed's corner. The flow's own scaling would move it in proportion to
   // the wind speed, but the library's centroid does not follow the level that
   // far, so half the exponent is used and the rest is left to Buffet.
   const float toneHz = clampf(mP.flowToneHz * std::sqrt(0.5f * (ratioL + ratioR)), 20.0f,
                               0.45f * sr);

   // The slope, as a staggered cascade rather than a crossfade between taps.
   //
   // Crossfading two taps of the cascade looks like it should interpolate the
   // slope and does not: the sum of two transfer functions is dominated at
   // high frequency by whichever falls more slowly, so mixing a two-pole tap
   // into a three-pole one still asymptotes at -12 dB/octave, only 3.5 dB
   // quieter. Flow Tilt could be set to -24 and the bed would still fall at
   // -12, which is why every dark preset measured far too bright.
   //
   // Instead the whole part of the slope is that many poles at the corner, and
   // the fraction is one more pole whose corner slides down from five octaves
   // above the others -- where it is nearly transparent -- to sitting on top of
   // them. The poles above that are parked near Nyquist and pass everything.
   const int whole = static_cast<int>(mFlowSlopeMix);
   const float frac = mFlowSlopeMix - static_cast<float>(whole);
   const float open = 0.45f * sr;
   for (int i = 0; i < 4; ++i) {
      float corner;
      if (i < whole)
         corner = toneHz;
      else if (i == whole)
         corner = clampf(toneHz * std::exp2(5.0f * (1.0f - frac)), toneHz, open);
      else
         corner = open;
      v.tiltL[i].setCutoff(corner, sr);
      v.tiltR[i].setCutoff(corner, sr);
   }
   v.buffetLpL.setCutoff(clampf(mP.buffetToneHz, 15.0f, 800.0f), sr);
   v.buffetLpR.setCutoff(clampf(mP.buffetToneHz, 15.0f, 800.0f), sr);
   v.hissHpL.setCutoff(clampf(3500.0f, 200.0f, 0.45f * sr), sr);
   v.hissHpR.setCutoff(clampf(3500.0f, 200.0f, 0.45f * sr), sr);
   v.hissLpL.setCutoff(clampf(18000.0f, 400.0f, 0.45f * sr), sr);
   v.hissLpR.setCutoff(clampf(18000.0f, 400.0f, 0.45f * sr), sr);

   // ------------------------------------------------------------------ howl
   const int voices = clampi(mP.howlVoices, 1, static_cast<int>(kMaxHowl));
   const float amount = clampf(mP.howlAmount, 0.0f, 1.0f) * ot.level;
   const float onset = clampf(mP.howlThreshold, 0.0f, 1.0f) * mean;
   const float track = clampf(mP.howlTrack, 0.0f, 1.0f) * ot.track;
   const float q = clampf(mHowlQ * ot.qMul, 0.5f, 40.0f);
   const float spread = clampf(mP.howlSpreadOct * ot.spreadMul, 0.0f, 5.0f);
   const float sizeBase = clampf(mP.howlSizeMm * ot.sizeMul, 0.05f, 800.0f);
   const float warble = clampf(mP.warble, 0.0f, 1.0f);

   // Shedding needs the flow: below the onset speed nothing happens at all,
   // and above it the tone comes up as the cube of the excess, following the
   // same U^6 power law as the bed. That steepness is why a howl arrives with
   // the gust instead of fading in.
   const float excess = smoothstep((howlSpeedMs - onset) / std::max(0.05f, 0.35f * mean));
   const float gate = excess * excess * excess;

   for (int i = 0; i < voices; ++i) {
      HowlVoice &h = v.howl[i];
      const float sizeMm = clampf(sizeBase * std::exp2(spread * h.sizeOffset), 0.05f, 2000.0f);
      h.walk += h.walkCoef * (h.rng.white() - h.walk);
      // Only the part of the speed that Howl Track lets through moves the
      // pitch. At zero the obstacle keeps one frequency however hard it blows,
      // which is what a cavity does.
      const float uEff = mean * (1.0f + track * (howlSpeedMs / mean - 1.0f));
      float hz = sheddingHz(uEff, sizeMm) * (1.0f + 0.4f * warble * h.walk);
      hz = clampf(hz, 20.0f, 0.45f * sr);
      h.band.setCutoff(hz, resonanceFor(q), sr);
      h.top.setCutoff(clampf(hz * 2.0f, 40.0f, 0.45f * sr), sr);

      const float target = amount * gate * envLevel / std::sqrt(static_cast<float>(voices));
      h.gainStep = (target - h.gain) * inv;
      const float p = h.position * width;
      h.panL = std::sqrt(clampf(0.5f * (1.0f - p), 0.0f, 1.0f));
      h.panR = std::sqrt(clampf(0.5f * (1.0f + p), 0.0f, 1.0f));
   }

   // ---------------------------------------------------------------- rustle
   const FoliageTraits &ft = kFoliageTraits[clampi(mP.foliage, 0, kNumFoliages - 1)];
   const float rustleOnset = clampf(mP.rustleThreshold, 0.0f, 1.0f) * ft.onsetMul * mean;
   const float rgate = smoothstep((speedM - rustleOnset) / std::max(0.05f, 0.5f * mean));
   v.rustleGate = rgate * std::sqrt(rgate) * ft.densityMul * envLevel;

   // --------------------------------------------------------------- gusting
   v.gustTimer -= static_cast<double>(kModInterval);
   if (v.gustTimer <= 0.0) {
      if (!v.env.isReleasing() || envLevel > 0.02f)
         spawnGust(v);
      // A proper Poisson process rather than a metronome with jitter: gusts
      // arrive independently of each other, which is what makes a wind sound
      // unpredictable rather than pulsed.
      const float rate = clampf(mP.gustRatePerMin / 60.0f, 0.002f, 20.0f);
      v.gustTimer = static_cast<double>(mRng.exponential(rate) * sr);
      if (v.gustTimer < static_cast<double>(kModInterval))
         v.gustTimer = static_cast<double>(kModInterval);
   }
}

void WindEngine::processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples) {
   const float sr = static_cast<float>(mSampleRate);
   const float velLevel = 1.0f + mP.velToLevel * (v.velocity - 0.5f) * 1.8f;
   const float velSpeed = clampf(1.0f + mP.velToSpeed * (v.velocity - 0.5f) * 1.6f, 0.1f, 3.0f);
   const float wide = clampf(mP.width, 0.0f, 1.0f);
   const float hiss = clampf(mP.hiss, 0.0f, 1.0f);
   const int voices = clampi(mP.howlVoices, 1, static_cast<int>(kMaxHowl));
   const float slopeMix = mFlowSlopeMix;
   const float density = clampf(mP.rustleDensityHz, 0.0f, 4000.0f);
   const bool wantRustle = mP.rustleAmount > 0.0005f && mP.foliage != kFoliageNone;

   for (uint32_t i = 0; i < numSamples; ++i) {
      const float env = v.env.tick();
      if (v.env.isIdle()) {
         v.active = false;
         return;
      }

      if (v.modCounter == 0)
         updateFlow(v, env * velLevel, velSpeed);
      v.modCounter = (v.modCounter + 1) % kModInterval;

      v.bedGainL += v.bedStepL;
      v.bedGainR += v.bedStepR;
      v.buffetGainL += v.buffetStepL;
      v.buffetGainR += v.buffetStepR;

      // The bed: white noise through a four-pole cascade, tapped between the
      // poles so that the slope is continuous rather than a choice of four.
      float nL = mRng.white();
      float nR = mRng.white();
      // The whole cascade, always: the poles that are not wanted sit near
      // Nyquist and pass everything, so the slope is set by their corners
      // rather than by which tap is read.
      float bedL = nL, bedR = nR;
      for (int p = 0; p < 4; ++p) {
         bedL = v.tiltL[p].tick(bedL);
         bedR = v.tiltR[p].tick(bedR);
      }
      // Each pole at the corner takes energy off the whole band, so the level
      // has to be put back or the slope would double as a volume control.
      bedL *= 1.0f + 0.9f * slopeMix;
      bedR *= 1.0f + 0.9f * slopeMix;
      bedL = v.bedHpL.tick(bedL);
      bedR = v.bedHpR.tick(bedR);

      // The top end: fine-scale turbulence, above where the tilt has taken
      // everything else away.
      if (hiss > 0.0f) {
         bedL += hiss * 0.9f * v.hissLpL.tick(v.hissHpL.tick(nL));
         bedR += hiss * 0.9f * v.hissLpR.tick(v.hissHpR.tick(nR));
      }

      float l = bedL * v.bedGainL;
      float r = bedR * v.bedGainR;

      // The buffeting: the pressure of the moving air rather than its hiss.
      if (mP.buffetGain > 0.0f) {
         l += 2.6f * v.buffetLpL.tick(nL) * v.buffetGainL;
         r += 2.6f * v.buffetLpR.tick(nR) * v.buffetGainR;
      }

      // Width by mixing the two towards each other rather than by panning:
      // wind is two nearly independent signals, not one placed somewhere.
      const float mono = 0.5f * (l + r);
      l = mono + wide * (l - mono);
      r = mono + wide * (r - mono);

      // The howl. Each obstacle is a resonant band on its own noise, not an
      // oscillator: a measured Q of about 2 is a narrow band of noise, and an
      // oscillator would be a whistle.
      if (mP.howlAmount > 0.0005f) {
         for (int hv = 0; hv < voices; ++hv) {
            HowlVoice &h = v.howl[hv];
            h.gain += h.gainStep;
            if (h.gain <= 1.0e-6f && h.gainStep <= 0.0f)
               continue;
            const float s =
               h.top.tick(h.band.bandpassNormalised(h.rng.white())) * h.gain * 2.0f;
            // 2.0 rather than a rounder number: measured against the bed, it
            // is what makes Howl Amount at 100 % about as loud as the airflow
            // at Flow Level -12 dB, so the parameter reads as a balance rather
            // than as a boost.
            l += s * h.panL;
            r += s * h.panR;
         }
      }

      outL[i] += l;
      outR[i] += r;

      // Leaves, spawned as a Poisson stream whose rate follows the wind.
      if (wantRustle && v.rustleGate > 0.0f && density > 0.0f) {
         v.leafTimer -= 1.0;
         if (v.leafTimer <= 0.0) {
            spawnLeaf(0.5f * (v.speedL + v.speedR));
            const float rate = density * v.rustleGate;
            v.leafTimer = static_cast<double>(mRng.exponential(rate / sr));
            if (v.leafTimer < 1.0)
               v.leafTimer = 1.0;
         }
      }
   }
}

void WindEngine::processLeaves(float *outL, float *outR, uint32_t numSamples) {
   const uint32_t cap = std::min<uint32_t>(kMaxLeaves, std::max(8, mP.maxGusts * 6));
   for (uint32_t li = 0; li < cap; ++li) {
      Leaf &l = mLeaves[li];
      if (!l.active)
         continue;
      for (uint32_t i = 0; i < numSamples; ++i) {
         const float n = l.rng.white();
         const float s =
            l.top.tick(l.band.bandpassNormalised(n * (l.level + l.clickLevel))) * 2.6f;
         outL[i] += s * l.panL;
         outR[i] += s * l.panR;
         l.level *= l.decayCoef;
         l.clickLevel *= l.clickCoef;
      }
      if (l.level < 1.0e-5f && l.clickLevel < 1.0e-5f && !l.band.ringing(1.0e-5f))
         l.active = false;
   }
}

void WindEngine::triggerVent() {
   Vent *slot = nullptr;
   for (auto &v : mVents) {
      if (!v.active) {
         slot = &v;
         break;
      }
   }
   if (!slot) {
      // The pool is full, which means several are already sounding. Take the
      // quietest rather than dropping the request: a button that sometimes
      // does nothing reads as broken.
      float lowest = 1.0e9f;
      for (auto &v : mVents) {
         if (v.level < lowest) {
            lowest = v.level;
            slot = &v;
         }
      }
   }
   Vent &v = *slot;
   const float sr = static_cast<float>(mSampleRate);

   v.active = true;
   v.rng.seed(mRng.next() | 1u);
   v.band.reset();
   v.formant.reset();
   v.phase = 0.0f;
   v.flutterPhase = 0.0f;

   // Every field is drawn fresh, from the ranges the references measure. A
   // log-uniform draw is the right shape for all of them: the quantities span
   // more than an octave and their medians sit below the midpoint.
   auto logDraw = [&](float lo, float hi) {
      return lo * std::exp2(std::log2(hi / lo) * v.rng.uniformPositive());
   };

   const float f0 = logDraw(45.0f, 280.0f);          // measured 28-300, median 113
   const float seconds = logDraw(0.18f, 1.7f);       // measured 0.13-1.69, median 0.30
   const float formantHz = logDraw(500.0f, 2400.0f); // centroid 367-3267, median 999

   v.inc = clampf(f0 / sr, 1.0e-6f, 0.2f);
   // The pressure behind it falls, and the pitch falls with it -- though not
   // always: a few of the references rise instead.
   const float total = 0.5f + 1.2f * v.rng.uniformPositive();
   v.glide = std::pow(total, 1.0f / std::max(1.0f, seconds * sr));
   v.decayCoef = decayCoef(seconds, sr);
   v.level = 0.5f + 0.5f * v.rng.uniformPositive();

   v.flutterInc = logDraw(4.5f, 60.0f) / sr;         // measured 4-148, median 8
   v.flutterDepth = 0.30f + 0.65f * v.rng.uniformPositive();
   v.hiss = 0.10f + 0.60f * v.rng.uniformPositive();
   v.formantMix = 0.25f + 0.55f * v.rng.uniformPositive();

   v.band.setCutoff(clampf(f0, 20.0f, 0.45f * sr), resonanceFor(4.0f + 12.0f * v.rng.uniformPositive()),
                    sr);
   v.formant.setCutoff(clampf(formantHz, 60.0f, 0.45f * sr),
                       resonanceFor(1.2f + 2.3f * v.rng.uniformPositive()), sr);

   const float pan = 0.35f * v.rng.white();
   v.panL = std::sqrt(clampf(0.5f * (1.0f - pan), 0.0f, 1.0f));
   v.panR = std::sqrt(clampf(0.5f * (1.0f + pan), 0.0f, 1.0f));
}

void WindEngine::processVents(float *outL, float *outR, uint32_t numSamples) {
   bool any = false;
   for (const auto &v : mVents)
      any = any || v.active;
   if (!any)
      return;

   for (auto &v : mVents) {
      if (!v.active)
         continue;
      for (uint32_t i = 0; i < numSamples; ++i) {
         v.inc = clampf(v.inc * v.glide, 1.0e-6f, 0.2f);
         v.phase += v.inc;
         float pulse = 0.0f;
         if (v.phase >= 1.0f) {
            v.phase -= std::floor(v.phase);
            pulse = 1.0f; // the aperture closing: one impulse a turn
         }

         v.flutterPhase += v.flutterInc;
         if (v.flutterPhase >= 1.0f)
            v.flutterPhase -= std::floor(v.flutterPhase);
         const float flutter =
            1.0f - v.flutterDepth * (0.5f - 0.5f * sin2piFast(0.25f + v.flutterPhase));

         const float n = v.rng.white();
         const float exc = (pulse * 5.0f + n * v.hiss) * flutter * v.level;
         // The aperture band carries most of it. Weighted up rather than
         // turning the formant down, because a normalised bandpass has unity
         // peak gain whatever its Q, so the low band and the mid one arrive
         // equal -- and the references put a median 38 % of the energy below
         // 500 Hz, against 14 % when the two were simply summed.
         const float s = 1.4f * v.band.bandpassNormalised(exc) +
                         v.formantMix * v.formant.bandpassNormalised(exc);

         // Added after the output chain rather than through it, so that a patch
         // with a 2 kHz highpass on it does not silence the thing entirely. It
         // still takes Output Gain, and the sum is clipped again, so the result
         // stays bounded whatever it lands on top of.
         outL[i] = softClip(outL[i] + s * v.panL * mP.gain);
         outR[i] = softClip(outR[i] + s * v.panR * mP.gain);

         v.level *= v.decayCoef;
      }
      if (v.level < 1.0e-5f && !v.band.ringing(1.0e-5f) && !v.formant.ringing(1.0e-5f))
         v.active = false;
   }
}

void WindEngine::processOutputChain(float *outL, float *outR, uint32_t numSamples) {
   const float wet = clampf(mP.spaceAmount, 0.0f, 1.0f);
   for (uint32_t i = 0; i < numSamples; ++i) {
      float l = outL[i], r = outR[i];

      // Distance: the top goes first, then the whole thing tilts down.
      l = mAirLpL.tick(l);
      r = mAirLpR.tick(r);
      if (mP.distance > 0.001f) {
         const float t = clampf(mP.distance, 0.0f, 1.0f);
         l = l * (1.0f - t) + mDistanceTiltL.tick(l) * t;
         r = r * (1.0f - t) + mDistanceTiltR.tick(r) * t;
      }

      if (wet > 0.001f) {
         float wl = 0.0f, wr = 0.0f;
         mSpace.tick(l, r, wl, wr);
         l = l * (1.0f - wet * 0.6f) + wl * wet;
         r = r * (1.0f - wet * 0.6f) + wr * wet;
      }

      switch (mP.filterType) {
      case 1: l = mOutFilterL.bandpassNormalised(l); r = mOutFilterR.bandpassNormalised(r); break;
      case 2: {
         float lp, bp, hp;
         mOutFilterL.tick(l, lp, bp, hp);
         l = hp;
         mOutFilterR.tick(r, lp, bp, hp);
         r = hp;
         break;
      }
      case 3: l = mOutFilterL.notch(l); r = mOutFilterR.notch(r); break;
      default: l = mOutFilterL.lowpass(l); r = mOutFilterR.lowpass(r); break;
      }

      l = mOutHpL.tick(l);
      r = mOutHpR.tick(r);

      // Saturate rather than clip: a storm gust with every layer up can run
      // past the ceiling, and wind should not crackle when it does.
      outL[i] = softClip(l * mP.gain);
      outR[i] = softClip(r * mP.gain);
   }
}

void WindEngine::process(float *outL, float *outR, uint32_t numSamples) {
   std::fill(outL, outL + numSamples, 0.0f);
   std::fill(outR, outR + numSamples, 0.0f);

   const float sr = static_cast<float>(mSampleRate);

   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      processVoice(v, outL, outR, numSamples);
   }

   processLeaves(outL, outR, numSamples);
   processOutputChain(outL, outR, numSamples);
   processVents(outL, outR, numSamples);

   // Silence tracking, so the host can sleep once the wind has dropped.
   float peak = 0.0f;
   for (uint32_t i = 0; i < numSamples; ++i)
      peak = std::max(peak, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
   if (peak < 1.0e-6f)
      mSilenceCounter += numSamples / sr;
   else
      mSilenceCounter = 0.0f;
}

bool WindEngine::isSilent() const {
   if (mSilenceCounter < 0.25f)
      return false;
   for (const auto &v : mVoices)
      if (v.active)
         return false;
   for (const auto &l : mLeaves)
      if (l.active)
         return false;
   for (const auto &v : mVents)
      if (v.active)
         return false;
   return true;
}

uint32_t WindEngine::activeVoiceCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices)
      if (v.active)
         ++n;
   return n;
}

uint32_t WindEngine::activeGustCount() const {
   uint32_t n = 0;
   for (const auto &g : mGusts)
      if (g.active)
         ++n;
   return n;
}

float WindEngine::tailSeconds() const {
   // The longest thing still to come after the note goes: the release, plus
   // the gust that was passing when it went, plus whatever the space is still
   // doing with it.
   const float space = mP.spaceAmount > 0.001f ? 0.3f + mSpace.decaySeconds() : 0.05f;
   return mP.releaseSec + mP.gustLengthSec + space + mP.rustleDecaySec;
}

} // namespace skyhowl
