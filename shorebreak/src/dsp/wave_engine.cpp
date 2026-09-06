#include "wave_engine.h"

#include "verdalis/dsp/fastmath.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

namespace shorebreak {

namespace {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// A bubble's damping, after Xue et al. (2023) eq. 3-5. Three mechanisms add:
//
//   delta_rad = omega0 * r / c   and Minnaert gives r = 3.26 / f0, so this is
//               2 * pi * 3.26 / c = 0.01368 whatever the size -- radiative loss
//               is the same fraction for every bubble.
//   delta_vis = 4 mu / (rho omega0 r^2), below 4e-4 for anything audible, so
//               it is dropped.
//   delta_th  = 2 (sqrt(psi - 3) - (3g-1)/(3(g-1))) / (psi - 4) with
//               psi = 16 Gth / (9 (g-1)^2 f0), which for audible bubbles is
//               well approximated by 2/sqrt(psi) = 4.743e-4 * sqrt(f0).
//
// Q is 1/delta: about 20 for a small high bubble and 46 for a large low one.
// Predicted ring times run 2-124 ms across 0.5-12 mm, against 5-100 ms measured
// in the references.
inline float bubbleDelta(float f) { return 0.01368f + 4.743e-4f * std::sqrt(f); }

// Svf::setCutoff takes resonance as 0..1, which it maps to k = 1/Q over 2..0.02.
// Passing a Q straight in silently clamps to maximum resonance, which turns a
// noise band into a whistle; convert properly instead.
inline float resonanceFor(float q) {
   const float k = 1.0f / std::max(0.5f, q);
   return clampf((2.0f - k) / 1.98f, 0.0f, 1.0f);
}

// An exponential decay coefficient reaching -60 dB in `sec`.
inline float decayCoefFor(float sec, double sampleRate) {
   const float n = std::max(1.0f, static_cast<float>(sec * sampleRate));
   return std::exp(-6.907755f / n); // ln(1000)
}

// How the shore biases what a wave sounds like. Sand absorbs and hisses;
// shingle and pebbles rattle and ring; rock and reef are bright and hard;
// a harbour wall is close, boxy and low.
struct ShoreTraits {
   float foamTilt;   // multiplies the foam corner
   float washTilt;   // multiplies the wash centre
   float grain;      // how much the wash rattles
   float bodyTilt;   // multiplies the break's low weight
   float bubbleTilt; // multiplies bubble pitch
};

constexpr ShoreTraits kShoreTraits[kNumShores] = {
   /* Sand     */ {0.85f, 0.80f, 0.30f, 1.00f, 0.90f},
   /* Shingle  */ {1.05f, 1.35f, 0.85f, 0.85f, 1.15f},
   /* Pebbles  */ {1.15f, 1.55f, 1.00f, 0.80f, 1.30f},
   /* Rock     */ {1.25f, 1.20f, 0.55f, 1.15f, 1.10f},
   /* Reef     */ {1.35f, 1.45f, 0.70f, 1.05f, 1.25f},
   /* Harbour  */ {0.70f, 0.65f, 0.40f, 1.35f, 0.75f},
};

// How the crest collapses. The slope figures are Means & Heitmeyer's (2002)
// measurements of the noise above 1.5 kHz: a plunger throws its crest forward
// and slams, which is the loudest and the steepest; a spiller foams gently down
// the face and is all foam and little body; a surge barely breaks at all.
struct BreakerTraits {
   float slope;     // 0 = -6 dB/oct, 1 = -12 dB/oct, above 1.5 kHz
   float attack;    // multiplies the break attack
   float level;     // multiplies the break level
   float foam;      // multiplies the foam level
   float body;      // multiplies the low weight
   float bright;    // multiplies the initial upward sweep
};

constexpr BreakerTraits kBreakerTraits[kNumBreakers] = {
   /* Spilling   */ {0.38f, 1.45f, 0.80f, 1.35f, 0.75f, 0.70f},
   /* Plunging   */ {0.67f, 0.65f, 1.00f, 1.00f, 1.15f, 1.30f},
   /* Collapsing */ {0.52f, 0.85f, 0.90f, 1.10f, 1.00f, 1.00f},
   /* Surging    */ {0.30f, 1.80f, 0.60f, 0.45f, 1.30f, 0.45f},
};

// Seed 0 is the "always different" setting and has no fixed mapping; every
// non-zero Seed maps here, so two instances never disagree about what a given
// Seed means.
uint32_t rngStateForSeed(int seed) {
   return 0x9E3779B9u * static_cast<uint32_t>(seed) + 17u;
}

std::atomic<uint32_t> gInstanceCounter{0};

} // namespace

void WaveEngine::prepare(double sampleRate, uint32_t /*maxBlockSize*/) {
   mSampleRate = sampleRate;
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   mRng.reseed(static_cast<uint32_t>(now) ^
               (0x9E3779B9u * (gInstanceCounter.fetch_add(1) + 1)));
   mAppliedSeed = 0;

   // The swell is the lowest thing here and the tank must not take its bottom
   // off; surf rumble runs lower than rain but not as low as thunder.
   mSpace.prepare(static_cast<float>(sampleRate), 24.0f);
   reset();
   updateFilters();
}

void WaveEngine::reset() {
   for (auto &v : mVoices) {
      v.active = false;
      v.env.reset();
      v.swellBandL.reset();
      v.swellBandR.reset();
      v.swellHpL.reset();
      v.swellHpR.reset();
      v.swellPhase = 0.0f;
      v.waveTimer = 0.0;
   }
   for (auto &w : mWaves)
      w.active = false;
   for (auto &b : mBubbles)
      b.active = false;

   // A non-zero Seed promises the same sea every time, so starting over has to
   // start the sequence over too. Seed 0 deliberately keeps running.
   if (mP.seed != 0)
      mRng.reseed(rngStateForSeed(mP.seed));

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

void WaveEngine::setParams(const EngineParams &p) {
   mP = p;
   if (mP.seed != mAppliedSeed) {
      mAppliedSeed = mP.seed;
      if (mP.seed != 0)
         mRng.reseed(rngStateForSeed(mP.seed));
   }
   updateFilters();
}

void WaveEngine::updateFilters() {
   const float sr = static_cast<float>(mSampleRate);

   // Distance does two things at once, the way it does outdoors: it takes the
   // top off (air absorption, and more of it in damp sea air) and it tilts what
   // is left downwards.
   const float d = clampf(mP.distance, 0.0f, 1.0f);
   const float airKeep = clampf(mP.air, 0.0f, 1.0f);
   const float airHz = 20000.0f * std::pow(0.06f, d * (1.35f - 0.7f * airKeep));
   mAirLpL.setCutoff(clampf(airHz, 400.0f, 20000.0f), sr);
   mAirLpR.setCutoff(clampf(airHz, 400.0f, 20000.0f), sr);
   const float tiltHz = 20000.0f * std::pow(0.25f, d);
   mDistanceTiltL.setCutoff(clampf(tiltHz, 800.0f, 20000.0f), sr);
   mDistanceTiltR.setCutoff(clampf(tiltHz, 800.0f, 20000.0f), sr);

   mOutHpL.setCutoff(clampf(mP.highpassHz, 10.0f, 4000.0f), sr);
   mOutHpR.setCutoff(clampf(mP.highpassHz, 10.0f, 4000.0f), sr);

   const float reso = clampf(0.05f + 0.90f * mP.filterReso, 0.0f, 0.98f);
   mOutFilterL.setCutoff(clampf(mP.filterCutoffHz, 20.0f, 0.45f * sr), reso, sr);
   mOutFilterR.setCutoff(clampf(mP.filterCutoffHz, 20.0f, 0.45f * sr), reso, sr);

   mSpace.setSize(mP.spaceSize);
   mSpace.setDamping(mP.spaceDamping);
}

void WaveEngine::noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId,
                        double velocity) {
   Voice *free = nullptr;
   for (auto &v : mVoices) {
      if (!v.active) {
         free = &v;
         break;
      }
   }
   if (!free) {
      // Steal the quietest voice; a sea is a bed, so the least audible one is
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
   v.noteId = noteId;
   v.velocity = static_cast<float>(clampf(static_cast<float>(velocity), 0.0f, 1.0f));

   const float sr = static_cast<float>(mSampleRate);
   v.env.setParams(mP.attackSec, mP.decaySec, mP.sustain, mP.releaseSec, sr);
   v.env.gateOn();

   v.swellBandL.reset();
   v.swellBandR.reset();
   v.swellHpL.reset();
   v.swellHpR.reset();
   v.swellHpL.setCutoff(35.0f, sr);
   v.swellHpR.setCutoff(35.0f, sr);
   // Deliberately not drawn from mRng. A note can arrive before the Seed
   // parameter has been applied -- events are handled in the order the host
   // sends them -- so anything drawn here would depend on when that happened,
   // and a fixed Seed would stop promising the same sea. The voice's own slot
   // and key give all the variation this needs, and give it deterministically.
   const uint32_t slot = static_cast<uint32_t>(&v - mVoices);
   const float spread = static_cast<float>((slot * 7u + static_cast<uint32_t>(key)) % 16u) / 16.0f;
   v.swellPhase = spread;
   v.swellInc = (mP.swellRatePerMin / 60.0f) / sr;

   // The first wave does not wait a full period: a beach is already going when
   // you arrive.
   v.waveTimer = mP.wavePeriodSec * sr * (0.05f + 0.35f * spread);
}

void WaveEngine::noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = (noteId >= 0 && v.noteId == noteId) ||
                         (noteId < 0 && v.port == port && v.channel == channel && v.key == key);
      if (match)
         v.env.gateOff();
   }
}

void WaveEngine::choke(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
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

void WaveEngine::allSoundOff() {
   for (auto &v : mVoices) {
      v.env.kill();
      v.active = false;
   }
   for (auto &w : mWaves)
      w.active = false;
   for (auto &b : mBubbles)
      b.active = false;
}

Wave *WaveEngine::allocateWave() {
   const uint32_t cap = std::min<uint32_t>(kMaxWaves, std::max(1, mP.maxWaves));
   for (uint32_t i = 0; i < cap; ++i) {
      if (!mWaves[i].active)
         return &mWaves[i];
   }
   // The pool is full: take the quietest, which is the one furthest through its
   // own life.
   Wave *victim = &mWaves[0];
   float lowest = 1.0e9f;
   for (uint32_t i = 0; i < cap; ++i) {
      const float e = mWaves[i].breakEnv + mWaves[i].foamEnv + mWaves[i].washEnv;
      if (e < lowest) {
         lowest = e;
         victim = &mWaves[i];
      }
   }
   return victim;
}

Bubble *WaveEngine::allocateBubble() {
   const uint32_t cap = std::min<uint32_t>(kMaxBubbles, std::max(8, mP.maxWaves * 2));
   for (uint32_t i = 0; i < cap; ++i) {
      if (!mBubbles[i].active)
         return &mBubbles[i];
   }
   return nullptr; // bubbles are decoration; dropping one is free
}

void WaveEngine::spawnWave(Voice &v, float envLevel) {
   Wave *slot = allocateWave();
   if (!slot)
      return;
   Wave &w = *slot;
   const float sr = static_cast<float>(mSampleRate);
   const ShoreTraits &st = kShoreTraits[clampi(mP.shore, 0, kNumShores - 1)];

   w.active = true;
   w.rng.seed(mRng.next() | 1u);

   // Size: the wave's own draw, spread around the setting, plus velocity.
   const float velSize = 1.0f + mP.velToSize * (v.velocity - 0.5f) * 1.6f;
   const float spread = 1.0f + mP.sizeVariation * (mRng.uniform() * 2.0f - 1.0f) * 0.85f;
   const float size = clampf(mP.waveSize * spread * velSize, 0.02f, 2.0f);

   const BreakerTraits &bt = kBreakerTraits[clampi(mP.breakerType, 0, kNumBreakers - 1)];

   w.breakLevel = envLevel * size * bt.level;
   w.breakEnv = 0.0f;
   w.breakRising = true;
   // A wave rises rather than strikes: a linear ramp over the attack, which is
   // what the references show (0.25-1.2 s from a third of peak to peak).
   const float atk =
      std::max(0.002f, mP.breakAttackSec * bt.attack * (0.7f + 0.6f * mRng.uniform()));
   w.breakAttackInc = 1.0f / (atk * sr);
   // Bigger breakers decay more slowly: -7 dB/s for a 1.6-2.0 m wave against
   // -4.5 dB/s for 2.4-2.7 m. Scale the setting the same way rather than
   // letting size change only the level.
   const float decayScale = 1.0f + 0.55f * (size - 0.5f);
   w.breakDecayCoef =
      decayCoefFor(std::max(0.02f, mP.breakDecaySec * clampf(decayScale, 0.5f, 2.0f)),
                   mSampleRate);

   // Bigger waves resonate lower: the cloud they make is larger.
   w.toneHz = clampf(mP.breakToneHz * std::pow(0.55f, size - 0.5f), 60.0f, 6000.0f);
   // The crest sweeps down as the cloud grows and coarsens.
   w.toneSweptHz = w.toneHz * (1.0f + 1.8f * mP.crestSweep * bt.bright);
   w.sweepCoef = decayCoefFor(std::max(0.05f, atk + mP.breakDecaySec), mSampleRate);
   w.body = clampf(mP.breakBody * st.bodyTilt * bt.body * (0.6f + 0.8f * size), 0.0f, 1.6f);

   w.breakBand.reset();
   w.bodyLp.reset();
   w.bodyLp.setCutoff(clampf(w.toneHz * 0.35f, 40.0f, 400.0f), sr);

   // The collective mode. Xue et al. show the lowest mode of a cloud of N
   // bubbles falls as f0 / cbrt(N), so a thousand bubbles ring an order of
   // magnitude below one -- which is where surf rumble comes from. It is a
   // resonance of the cloud, not a lowpass of the break, and it lands below
   // 400 Hz exactly where Schindall & Heitmeyer put collective oscillations.
   const float cloudN = 60.0f + 5000.0f * size * size;
   const float cloudHz = clampf(mP.bubblePitchHz / std::cbrt(cloudN), 25.0f, 400.0f);
   w.cloudBand.reset();
   w.cloudBand.setCutoff(cloudHz, resonanceFor(3.5f), sr);
   w.cloudLevel = w.body * 1.4f;

   // The slope above 1.5 kHz: steepest at the moment of collapse, relaxing
   // afterwards towards the breaker type's own figure.
   w.slopeLp1.reset();
   w.slopeLp2.reset();
   w.slopeLp1.setCutoff(1500.0f, sr);
   w.slopeLp2.setCutoff(1500.0f, sr);
   w.slopeTarget = bt.slope;
   w.slopeMix = clampf(bt.slope + 0.33f, 0.0f, 1.0f); // steeper while it collapses
   w.slopeRelax = decayCoefFor(1.0f, mSampleRate);    // relaxes over about a second

   // The precursor: the crest is already bubbling as it stands up. Quiet, and
   // brighter than the break it precedes, because it is all small bubbles.
   w.preLevel = w.breakLevel * mP.precursor * 0.22f;
   w.preEnv = 0.0f;
   w.preInc = 1.0f / std::max(1.0f, atk * 1.6f * sr);

   // Foam: highpassed hard, because measured foam has no low end at all.
   w.foamHp.reset();
   w.foamLp.reset();
   const float foamHz = clampf(mP.foamToneHz * st.foamTilt, 120.0f, 9000.0f);
   w.foamHp.setCutoff(foamHz, sr);
   // Fizz is how fine the sheet is, expressed as how far above its corner the
   // band reaches: two octaves for a coarse seething, six for a fine hiss.
   // A sheet of bubbles spans a couple of octaves, not the whole top end: one
   // and a half for a coarse seething, four for the finest hiss. Wider than
   // that and white noise's own 3 dB/octave rise takes the spectrum over.
   w.foamLp.setCutoff(clampf(foamHz * std::pow(2.0f, 1.5f + 2.5f * mP.fizz), 400.0f,
                             0.45f * sr),
                      sr);
   w.foamLevel = w.breakLevel * mP.foamGain * bt.foam * (0.7f + 0.6f * mRng.uniform());
   w.foamEnv = 0.0f;
   w.foamRising = true;
   // The foam floods in quickly and then outlives the break: a short ramp up,
   // then its own decay. Previously the two ran at once and the envelope
   // settled at 0.89 instead of decaying at all.
   w.foamAttackInc = 1.0f / std::max(1.0f, 0.08f * sr);
   w.foamDecayCoef = decayCoefFor(std::max(0.05f, mP.foamDecaySec), mSampleRate);
   w.foamDelaySamples = static_cast<int>(std::max(0.0f, mP.foamDelaySec) * sr *
                                         (0.6f + 0.8f * mRng.uniform()));

   // Wash: a mid band with a slow walk, coarser shores rattling more.
   w.washBand.reset();
   const float washHz = clampf(mP.washToneHz * st.washTilt, 120.0f, 9000.0f);
   // A broad band, deliberately barely resonant: this is water draining
   // through shingle, not a tuned pipe. The first version passed a Q here and
   // it clamped to maximum resonance, which is what made Receding Sand whistle.
   w.washBand.setCutoff(washHz, clampf(0.10f + 0.30f * mP.sand * st.grain, 0.0f, 0.55f), sr);
   w.washLevel = w.breakLevel * mP.washGain * (0.7f + 0.6f * mRng.uniform());
   w.washEnv = 0.0f;
   w.washRising = true;
   w.washAttackInc = 1.0f / std::max(1.0f, 0.15f * sr);
   w.washDecayCoef = decayCoefFor(std::max(0.05f, mP.washDecaySec), mSampleRate);
   w.washWalk = 0.0f;
   w.washWalkCoef = decayCoefFor(0.05f + 0.35f * (1.0f - mP.sand), mSampleRate);

   // Where it breaks along the shore.
   const float pan = (mRng.uniform() * 2.0f - 1.0f) * clampf(mP.width, 0.0f, 1.0f);
   const float a = 0.25f * 3.14159265f * (pan + 1.0f);
   w.panL = std::cos(a);
   w.panR = std::sin(a);

   w.bubbleTimer = 0.0f;
   w.foamBubbleTimer = 0.0f;
   // Foam is made of finer bubbles than the break that left it: the slowed
   // reference measures the fizzle at 2.2 kHz and 29 onsets a second against
   // the break's 850 Hz and 9. Fizz is how much finer.
   w.foamPitchScale = 1.0f + 2.6f * mP.fizz;
   // The size distribution starts high and slides down over the life of the
   // break: small bubbles are formed first, larger ones coalesce after.
   w.pitchScale = 1.0f + 0.35f * mP.crestSweep;
   w.pitchScaleCoef = decayCoefFor(std::max(0.08f, atk + mP.breakDecaySec), mSampleRate);
}

void WaveEngine::spawnBubble(const Wave &w, float level, float pitchScale) {
   Bubble *slot = allocateBubble();
   if (!slot)
      return;
   Bubble &b = *slot;
   const float sr = static_cast<float>(mSampleRate);
   const ShoreTraits &st = kShoreTraits[clampi(mP.shore, 0, kNumShores - 1)];

   b.active = true;
   b.rng.seed(mRng.next() | 1u);

   // A bubble's pitch is its radius, by Minnaert: f0 ~= 3.26 / r, so 1 kHz is a
   // bubble about 3 mm across. The cloud holds a range of sizes at once, and
   // the references measure the audible ones between 650 and 1930 Hz.
   const float oct = (mRng.uniform() * 2.0f - 1.0f) * mP.bubbleSpreadOct;
   const float f = clampf(mP.bubblePitchHz * st.bubbleTilt * pitchScale * std::pow(2.0f, oct),
                          80.0f, 0.45f * sr);

   // Damping straight from the physics, scaled by the parameter. beta is the
   // amplitude decay rate of the oscillator, pi f delta.
   const float delta = clampf(bubbleDelta(f) * mP.bubbleDamping, 0.002f, 0.5f);
   const float beta = 3.14159265f * f * delta;

   b.phase = mRng.uniform();
   b.inc = f / sr;
   b.level = level * (0.55f + 0.45f * mRng.uniform());
   b.decayCoef = std::exp(-beta / sr);

   // The pinch-off transient. Short and broadband, and it is what makes a
   // bubble read as an event rather than a tone.
   b.clickLevel = b.level * 0.32f;
   b.clickCoef = decayCoefFor(0.0015f, mSampleRate);
   b.panL = w.panL;
   b.panR = w.panR;
}

// One held note, per sample: its envelope, the clock that decides when the next
// wave breaks, and the swell bed underneath. All three in one loop, because the
// wave clock has to see the envelope of the sample it is spawning into.
void WaveEngine::processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples) {
   const float sr = static_cast<float>(mSampleRate);
   const float velLevel = 1.0f + mP.velToLevel * (v.velocity - 0.5f) * 1.8f;
   const float reso = 0.28f;
   v.swellBandL.setCutoff(clampf(mP.swellToneHz, 40.0f, 0.45f * sr), reso, sr);
   v.swellBandR.setCutoff(clampf(mP.swellToneHz * 1.08f, 40.0f, 0.45f * sr), reso, sr);
   const float wide = clampf(mP.swellWidth, 0.0f, 1.0f);

   for (uint32_t i = 0; i < numSamples; ++i) {
      const float env = v.env.tick();
      if (v.env.isIdle()) {
         v.active = false;
         return;
      }

      // The next wave. The clock runs on the envelope, so a released note stops
      // making new waves once it has faded, while the ones already breaking
      // finish in their own time.
      v.waveTimer -= 1.0;
      if (v.waveTimer <= 0.0) {
         if (!v.env.isReleasing() || env > 0.02f)
            spawnWave(v, clampf(env * velLevel, 0.0f, 2.0f));
         // Set Variation groups the waves: a narrow draw is a metronome, a wide
         // one arrives in sets the way a real swell does.
         const float var = clampf(mP.setVariation, 0.0f, 1.0f);
         const float jitter = 1.0f + var * (mRng.uniform() * 2.0f - 1.0f) * 1.3f;
         v.waveTimer = static_cast<double>(mP.wavePeriodSec * sr) *
                       static_cast<double>(std::max(0.15f, jitter));
         if (v.waveTimer < 64.0)
            v.waveTimer = 64.0;
      }

      // The swell bed: water moving without breaking, slowly breathing.
      if (mP.swellGain > 0.0f) {
         v.swellPhase += v.swellInc;
         if (v.swellPhase >= 1.0f)
            v.swellPhase -= 1.0f;
         const float breathe =
            1.0f - mP.swellDepth * 0.5f * (1.0f - sin2piFast(v.swellPhase));
         const float g = mP.swellGain * env * velLevel * breathe;

         float l = v.swellBandL.bandpassNormalised(mRng.white());
         float r = v.swellBandR.bandpassNormalised(mRng.white());
         l = v.swellHpL.tick(l);
         r = v.swellHpR.tick(r);
         // Width by mixing the two towards each other rather than by panning:
         // the open sea is two nearly independent signals, not one placed
         // somewhere.
         const float mono = 0.5f * (l + r);
         outL[i] += g * (mono + wide * (l - mono));
         outR[i] += g * (mono + wide * (r - mono));
      }
   }
}

void WaveEngine::processWaves(float *outL, float *outR, uint32_t numSamples) {
   const uint32_t cap = std::min<uint32_t>(kMaxWaves, std::max(1, mP.maxWaves));
   const float bubbleRate = std::max(0.0f, mP.bubbleRateHz) / static_cast<float>(mSampleRate);

   for (uint32_t wi = 0; wi < cap; ++wi) {
      Wave &w = mWaves[wi];
      if (!w.active)
         continue;

      const float sr = static_cast<float>(mSampleRate);
      for (uint32_t i = 0; i < numSamples; ++i) {
         // ---- the break
         if (w.breakRising) {
            w.breakEnv += w.breakAttackInc;
            if (w.breakEnv >= 1.0f) {
               w.breakEnv = 1.0f;
               w.breakRising = false;
            }
         } else {
            w.breakEnv *= w.breakDecayCoef;
         }

         // The cloud's resonance falls towards its final tone as it grows.
         w.toneSweptHz = w.toneHz + (w.toneSweptHz - w.toneHz) * w.sweepCoef;
         if ((i & 31u) == 0u)
            w.breakBand.setCutoff(clampf(w.toneSweptHz, 40.0f, 0.45f * sr), 0.42f, sr);

         const float n = w.rng.white();
         float band = w.breakBand.bandpassNormalised(n);

         // Slope above 1.5 kHz: one pole is -6 dB/oct, two are -12, and the mix
         // between them is the slope. It starts steep, while the crest is still
         // collapsing, and relaxes towards the breaker type's own figure.
         w.slopeMix = w.slopeTarget + (w.slopeMix - w.slopeTarget) * w.slopeRelax;
         const float p1 = w.slopeLp1.tick(band);
         const float p2 = w.slopeLp2.tick(p1);
         band = p1 + (p2 - p1) * w.slopeMix;

         // Bubble Mix decides what the break is made of. A real break is mostly
         // the sound of bubbles being formed, so the turbulence band is only
         // what is left over.
         const float turb = 1.0f - clampf(mP.bubbleMix, 0.0f, 1.0f);
         float s = band * w.breakEnv * w.breakLevel * turb;
         if (w.body > 0.0f) {
            s += w.bodyLp.tick(n) * w.body * w.breakEnv * w.breakLevel * 0.45f;
            // The cloud oscillating as one thing, driven by the same turbulence.
            s += w.cloudBand.bandpassNormalised(n) * w.cloudLevel * w.breakEnv *
                 w.breakLevel * 0.8f;
         }

         // The cascade itself. The rate follows the break envelope, so bubbles
         // are formed fastest as the crest collapses. This is the break; the
         // filters above only colour what surrounds it.
         w.pitchScale = 1.0f + (w.pitchScale - 1.0f) * w.pitchScaleCoef;
         if (bubbleRate > 0.0f && mP.bubbleMix > 0.001f) {
            const float drive = w.breakEnv * w.breakEnv; // fastest at the peak
            w.bubbleTimer -= bubbleRate * drive;
            while (w.bubbleTimer <= 0.0f) {
               w.bubbleTimer += 1.0f;
               spawnBubble(w, w.breakLevel * mP.bubbleMix * 3.2f, w.pitchScale);
            }
         }

         // The precursor bubbling, which fades as the break it announced takes
         // over. Reuses the foam highpass, being the same small bubbles.
         if (w.preLevel > 0.0f && w.preEnv < 1.0f) {
            w.preEnv += w.preInc;
            const float fade = 1.0f - w.breakEnv;
            s += w.foamHp.tick(w.rng.white()) * w.preEnv * fade * w.preLevel;
         }

         // ---- the foam, once it has arrived
         if (w.foamDelaySamples > 0) {
            --w.foamDelaySamples;
         } else {
            // Floods in, then outlives the break.
            if (w.foamRising) {
               w.foamEnv += w.foamAttackInc;
               if (w.foamEnv >= 1.0f) {
                  w.foamEnv = 1.0f;
                  w.foamRising = false;
               }
            } else {
               w.foamEnv *= w.foamDecayCoef;
            }
            const float f = w.foamHp.tick(w.rng.white());
            s += w.foamLp.tick(f) * w.foamEnv * w.foamLevel *
                 (1.0f - 0.65f * clampf(mP.foamBubbles, 0.0f, 1.0f));

            // The foam's own cascade: finer bubbles, faster than the break's,
            // and this is what carries the gap between waves. The slowed
            // reference resolves 29 onsets a second here against the break's 9.
            if (bubbleRate > 0.0f && w.foamEnv > 0.008f && mP.foamBubbles > 0.001f) {
               w.foamBubbleTimer -= bubbleRate * w.foamEnv * (0.6f + 2.4f * mP.foamBubbles);
               while (w.foamBubbleTimer <= 0.0f) {
                  w.foamBubbleTimer += 1.0f;
                  spawnBubble(w, w.foamLevel * w.foamEnv * mP.foamBubbles * 2.6f,
                              w.foamPitchScale);
               }
            }
         }

         // ---- the wash
         if (!w.breakRising) {
            if (w.washRising) {
               w.washEnv += w.washAttackInc;
               if (w.washEnv >= 1.0f) {
                  w.washEnv = 1.0f;
                  w.washRising = false;
               }
            } else {
               w.washEnv *= w.washDecayCoef;
            }
            w.washWalk += (w.rng.white() - w.washWalk) * (1.0f - w.washWalkCoef);
            const float g = 1.0f + 0.8f * mP.sand * w.washWalk;
            s += w.washBand.bandpassNormalised(w.rng.white()) * w.washEnv * w.washLevel * g;
         }

         outL[i] += s * w.panL;
         outR[i] += s * w.panR;
      }

      if (w.finished())
         w.active = false;
   }
}

void WaveEngine::processBubbles(float *outL, float *outR, uint32_t numSamples) {
   for (auto &b : mBubbles) {
      if (!b.active)
         continue;
      for (uint32_t i = 0; i < numSamples; ++i) {
         b.phase += b.inc;
         if (b.phase >= 1.0f)
            b.phase -= 1.0f;
         float s = sin2piFast(b.phase) * b.level;
         if (b.clickLevel > 1.0e-5f) {
            s += b.rng.white() * b.clickLevel;
            b.clickLevel *= b.clickCoef;
         }
         outL[i] += s * b.panL;
         outR[i] += s * b.panR;
         b.level *= b.decayCoef;
      }
      if (b.level < 1.0e-5f)
         b.active = false;
   }
}

void WaveEngine::processOutputChain(float *outL, float *outR, uint32_t numSamples) {
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
         mOutFilterL.tick(l, lp, bp, hp); l = hp;
         mOutFilterR.tick(r, lp, bp, hp); r = hp;
         break;
      }
      case 3: l = mOutFilterL.notch(l); r = mOutFilterR.notch(r); break;
      default: l = mOutFilterL.lowpass(l); r = mOutFilterR.lowpass(r); break;
      }

      l = mOutHpL.tick(l);
      r = mOutHpR.tick(r);

      // Saturate rather than clip: with every layer at once the sum can run
      // past the ceiling, and a beach should not crackle when it does.
      outL[i] = softClip(l * mP.gain);
      outR[i] = softClip(r * mP.gain);
   }
}

void WaveEngine::process(float *outL, float *outR, uint32_t numSamples) {
   std::fill(outL, outL + numSamples, 0.0f);
   std::fill(outR, outR + numSamples, 0.0f);

   const float sr = static_cast<float>(mSampleRate);

   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      processVoice(v, outL, outR, numSamples);
   }

   processWaves(outL, outR, numSamples);
   processBubbles(outL, outR, numSamples);
   processOutputChain(outL, outR, numSamples);

   // Silence tracking, so the host can sleep when the beach has run out.
   float peak = 0.0f;
   for (uint32_t i = 0; i < numSamples; ++i)
      peak = std::max(peak, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
   if (peak < 1.0e-6f)
      mSilenceCounter += numSamples / sr;
   else
      mSilenceCounter = 0.0f;
}

bool WaveEngine::isSilent() const {
   if (mSilenceCounter < 0.25f)
      return false;
   for (const auto &v : mVoices)
      if (v.active)
         return false;
   for (const auto &w : mWaves)
      if (w.active)
         return false;
   for (const auto &b : mBubbles)
      if (b.active)
         return false;
   return true;
}

uint32_t WaveEngine::activeVoiceCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices)
      if (v.active)
         ++n;
   return n;
}

uint32_t WaveEngine::activeWaveCount() const {
   uint32_t n = 0;
   for (const auto &w : mWaves)
      if (w.active)
         ++n;
   return n;
}

float WaveEngine::tailSeconds() const {
   // The longest thing still to come after the note goes: the release, plus
   // whatever the last wave's foam and the space are still doing.
   const float foam = std::max(mP.foamDecaySec, mP.washDecaySec);
   const float space = mP.spaceAmount > 0.001f ? 0.3f + mSpace.decaySeconds() : 0.05f;
   return mP.releaseSec + foam + space + mP.wavePeriodSec;
}

} // namespace shorebreak
