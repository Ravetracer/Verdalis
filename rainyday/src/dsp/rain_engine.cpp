#include "rain_engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

#include "fastmath.h"

namespace rainyday {

namespace {

// Per-surface bias applied on top of the user's droplet controls. This is what
// makes rain on a tin roof read differently from rain on leaves even with
// identical density and pitch settings.
struct SurfaceProfile {
   float decayMul;   // scales droplet ring time
   float resonance;  // resonator Q character, 0..1
   float clickMul;   // impact weight
   float splashMul;  // wet noise-burst weight
   float chirpOct;   // maximum pitch sweep in octaves
   float brightness; // scales the per-droplet air lowpass and the impact pitch
   float tonalMul;   // sine layer weight
   float onsetMul;   // how slowly the tone swells in after the impact
   float harmonic;   // level of the second bubble mode, relative to the first
};

// Wet surfaces trap an air bubble, so their tone swells in a few milliseconds
// behind the splash; a rigid surface starts ringing the instant it is struck.
//
// The chirp column is very small on purpose. Tracking the instantaneous
// frequency of isolated drops in real recordings, across the 80 ms or so that a
// drop is actually audible, puts the bend between 0.01 and 0.09 octaves. It is
// a couple of per cent, not the octave-wide swoop the textbook description of
// bubble entrainment suggests. Chirp at 100 % now sits at the top of that
// measured range rather than above it: past roughly a tenth of an octave a
// droplet stops sounding like water and starts sounding like a laser.
//
// The harmonic column is the second bubble mode. Measuring the isolated drops
// in the reference recordings puts a partial at 1.8 to 2.15 times the
// fundamental, 15 to 25 dB below it, on essentially every drop that falls into
// water; it is what a bubble pulsating hard enough to be heard radiates at
// twice its breathing frequency. It belongs to the bubble, so the surfaces that
// do not trap one do not get it.
const SurfaceProfile kSurfaces[kNumSurfaces] = {
   /* Water    */ {1.00f, 0.55f, 0.50f, 1.20f, 0.08f, 1.00f, 1.00f, 1.00f, 0.11f},
   /* Puddle   */ {1.60f, 0.75f, 0.35f, 1.40f, 0.12f, 0.85f, 1.15f, 1.30f, 0.11f},
   /* Leaves   */ {0.35f, 0.15f, 1.20f, 0.70f, 0.02f, 0.70f, 0.35f, 0.35f, 0.00f},
   /* Wood     */ {0.60f, 0.45f, 1.10f, 0.50f, 0.03f, 0.90f, 0.80f, 0.25f, 0.00f},
   /* Metal    */ {3.00f, 0.90f, 1.30f, 0.45f, 0.015f, 1.60f, 1.30f, 0.12f, 0.06f},
   /* Glass    */ {1.20f, 0.80f, 1.25f, 0.40f, 0.02f, 1.90f, 1.10f, 0.12f, 0.06f},
   /* Concrete */ {0.30f, 0.20f, 1.15f, 0.60f, 0.015f, 0.80f, 0.40f, 0.25f, 0.00f},
   // Fabric: a taut canopy a foot above your head, which is an umbrella or a
   // tent. It is a drumhead, so the impact is the loudest thing about it and
   // carries more weight than on any other surface, but the membrane is lossy
   // and under tension rather than rigid, so what it rings with dies almost at
   // once and has very little pitch to it. Struck from above and radiating
   // straight down, it is also the one surface heard from a few centimetres
   // away rather than across a street.
   /* Fabric   */ {0.40f, 0.22f, 1.45f, 0.65f, 0.02f, 0.85f, 0.50f, 0.18f, 0.00f},
};

// Time constant of the pitch bend, as a fraction of the droplet's ring time and
// as an absolute window. Spread across the ring rather than crammed into its
// first few milliseconds: the recordings show the frequency drifting gently
// over the drop's whole audible life, and concentrating the same small bend
// into the attack is exactly what makes it read as a swoop instead of as the
// pitch of a bubble settling.
constexpr float kChirpTauFraction = 1.0f;
constexpr float kChirpTauMinSec = 0.005f;
constexpr float kChirpTauMaxSec = 0.120f;

// Rise time of the tonal layer, as a fraction of its own ring time and as an
// absolute window. Capped against the ring time so the rise is always clearly
// shorter than the decay.
constexpr float kTonalRiseFraction = 0.10f;
constexpr float kTonalRiseMinSec = 0.00005f;
constexpr float kTonalRiseMaxSec = 0.008f;
constexpr float kTonalRiseCeiling = 0.45f; // of the ring time

// Where the noise bed's highpass sits relative to its lowpass, making the bed a
// band roughly two and a half octaves wide.
constexpr float kBedBandRatio = 0.18f;

// Corner of a droplet's radiation highpass, as a fraction of its own pitch.
constexpr float kBodyHpRatio = 0.45f;

// The initial impact. Following Liu, Cheng and Tong (2019), it is not noise but
// a damped sine at a frequency drawn afresh for every droplet, uniform over
// this range, with a damping constant of twice that frequency so that only
// about two cycles survive. One drop is therefore a tick with a pitch of its
// own; a thousand of them a second are broadband, which is the point. The range
// is scaled by the surface's brightness, so a tin roof ticks higher than
// leaves.
//
// The ceiling is 0.30x the sample rate rather than something just under
// Nyquist. Damping this hard makes the tick's spectrum about as wide as its own
// centre frequency, so a blip placed near Nyquist folds a real part of itself
// back down. At 0.30x the skirt is 7 dB or more down by the time it reaches
// Nyquist, which for a transient this short is inaudible; at 48 kHz that caps
// the tick at 14.4 kHz, and at 96 kHz nothing is capped at all.
constexpr float kImpactMinHz = 1000.0f;
constexpr float kImpactMaxHz = 16000.0f;
constexpr float kImpactDampPerHz = 2.0f;
constexpr float kImpactMaxRate = 0.30f;

// Second bubble mode: a ratio spread around two, and a level spread in dB, both
// drawn per droplet. It decays twice as fast in dB as the fundamental, which is
// what a second harmonic riding on a decaying oscillation does.
constexpr float kHarmonicRatio = 1.97f;
constexpr float kHarmonicRatioSpreadOct = 0.05f;
constexpr float kHarmonicLevelSpreadDb = 4.0f;
constexpr float kHarmonicSpreadClamp = 2.0f; // in sigmas, so +-8 dB and +-0.1 oct
constexpr float kHarmonicDecayRatio = 2.0f;

// How far the pitch spread is allowed to reach downwards, relative to how far
// it reaches up.
constexpr float kDownwardSpread = 0.45f;

// Base amplitude of a single droplet before concurrency normalisation, spread,
// envelope and distance.
constexpr float kDropletBaseAmp = 0.25f;

// The absolute diameter, in millimetres, that a relative size of one stands
// for. The engine only ever works in relative size, but terminal velocity is a
// function of the real drop, so the two have to be tied together somewhere and
// this is that place. One and a half millimetres is the median-volume diameter
// of moderate rain under Marshall and Palmer's distribution, which is the
// distribution the size draw in spawnDroplet is imitating, so it is the size
// the draw already means. It also sits in the middle of the range the velocity
// relation below is useful over, and it leaves the clamped range of relative
// sizes spanning 0.45 mm to 2.3 mm, which is rain rather than drizzle at one
// end or a thunderstorm at the other.
constexpr float kMedianDropMm = 1.5f;

// Terminal velocity of the median drop, in metres per second, i.e.
// terminalVelocityMs(kMedianDropMm). Kept as a constant because std::exp is
// not usable in a constant expression and the value is needed per droplet.
constexpr float kMedianDropVelMs = 5.4623f;

// Hard ceiling on the droplet birth rate so a pathological density/velocity/
// clumping combination cannot starve the pool.
constexpr float kMaxBirthRate = 12000.0f;

// Internal reference level. Applied uniformly to droplets, bed and space as a
// single product with Output Gain, so it shifts the engine's output without
// touching any internal balance. Chosen so that a loudness-matched preset lands
// in the middle of the Output Gain range: the most distant presets used to need
// the whole +12 dB and had no headroom left for the user.
constexpr float kEngineMakeup = 3.55f; // +11 dB

// Atlas and Ulbrich's fit for the speed a raindrop of diameter D millimetres
// falls at once drag balances gravity: about 2 m/s at half a millimetre rising
// to 9 m/s at five. It goes negative below D = 0.11 mm, where a drop that small
// is really still suspended, so the result is floored at zero.
inline float terminalVelocityMs(float diameterMm) {
   return std::max(0.0f, 9.65f - 10.3f * std::exp(-0.6f * diameterMm));
}

inline float softClip(float x) {
   constexpr float t = 0.8f;
   if (x > t)
      return t + (1.0f - t) * std::tanh((x - t) / (1.0f - t));
   if (x < -t)
      return -t - (1.0f - t) * std::tanh((-x - t) / (1.0f - t));
   return x;
}

inline bool noteMatches(const Voice &v, int16_t port, int16_t channel, int16_t key,
                        int32_t noteId) {
   if (noteId >= 0 && v.noteId >= 0)
      return v.noteId == noteId;
   if (port >= 0 && v.port != port)
      return false;
   if (channel >= 0 && v.channel != channel)
      return false;
   if (key >= 0 && v.key != key)
      return false;
   return true;
}

} // namespace

// Seed 0 is the "always different" setting and has no fixed mapping; every
// other value maps here, and only here, so that reset() and setParams() cannot
// disagree about what a given Seed means.
static uint32_t rngStateForSeed(int seed) {
   return 0x9E3779B9u * static_cast<uint32_t>(seed) + 17u;
}

void RainEngine::prepare(double sampleRate, uint32_t /*maxBlockSize*/) {
   mSampleRate = static_cast<float>(sampleRate);
   mDroplets.assign(kMaxDroplets, Droplet());
   mSpace.prepare(mSampleRate);
   // Unique starting point per instance so stacked copies decorrelate.
   static std::atomic<uint32_t> instanceCounter{0};
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   mRng.reseed(static_cast<uint32_t>(now) ^ (0x9E3779B9u * (instanceCounter.fetch_add(1) + 1)));
   mAppliedSeed = 0;
   reset();
}

void RainEngine::reset() {
   for (auto &v : mVoices) {
      v.active = false;
      v.held = false;
      v.noteId = -1;
      v.env.reset();
      v.bedLpL.reset();
      v.bedLpR.reset();
      v.bedHpL.reset();
      v.bedHpR.reset();
      v.driftState = 0.0f;
      v.clumpState = 0.0f;
      v.modCounter = 0;
      v.dropTimer = 0.0;
   }
   for (auto &d : mDroplets)
      d.active = false;
   // The allocation cursor is playing state, not configuration. Left alone it
   // decides which pool slots the next droplets land in, and the slots are
   // summed in index order, so carrying it over makes a render depend on
   // whatever the engine rendered before it -- at the same Seed.
   mDropletCursor = 0;
   mLastKey = 60;
   mFilterL.reset();
   mFilterR.reset();
   mHighpassL.reset();
   mHighpassR.reset();
   mSpace.clear();
   mSilenceCounter = 0;

   // A non-zero Seed promises the same rain every time, so starting over has to
   // start the sequence over too. Seed 0 deliberately keeps running, which is
   // what makes it the setting that never repeats.
   if (mP.seed != 0)
      mRng.reseed(rngStateForSeed(mP.seed));
}

void RainEngine::setParams(const EngineParams &p) {
   mP = p;

   if (mP.seed != mAppliedSeed) {
      mAppliedSeed = mP.seed;
      // Seed 0 keeps the per-instance random seed chosen in prepare(), so two
      // instances of the plugin never generate identical rain. Any other value
      // is reproducible and renders identically every time.
      if (mP.seed != 0)
         mRng.reseed(rngStateForSeed(mP.seed));
   }

   const uint32_t newLimit =
      static_cast<uint32_t>(clampv(mP.maxDroplets, 32, static_cast<int>(kMaxDroplets)));
   if (newLimit < mDropletLimit) {
      // Slots outside the new pool would never be processed again, so they must
      // not be left marked active.
      for (uint32_t i = newLimit; i < mDropletLimit; ++i)
         mDroplets[i].active = false;
   }
   mDropletLimit = newLimit;
   if (mDropletCursor >= mDropletLimit)
      mDropletCursor = 0;

   // Below about 25 Hz the highpass is doing nothing audible, so it steps aside
   // rather than spending two biquads per sample on every preset that leaves it
   // where it starts.
   mHighpassBypass = mP.highpassHz <= 25.0f;
   if (!mHighpassBypass) {
      const float hp = clampv(mP.highpassHz, 20.0f, 0.45f * mSampleRate);
      mHighpassL.setCutoff(hp, mSampleRate);
      mHighpassR.setCutoff(hp, mSampleRate);
   }

   mSpace.setSize(mP.spaceSize);
   mSpace.setDamping(mP.spaceDamping);

   // Bed spectrum: tone sweeps the lowpass corner, distance and air absorption
   // pull it further down, body adds resonance at the corner.
   mBedCutoff = 200.0f * std::pow(80.0f, mP.bedTone);
   mBedCutoff *= std::exp2(-3.0f * mP.distance * mP.air);
   mBedCutoff = clampv(mBedCutoff, 40.0f, 0.45f * mSampleRate);
   // The far field of real rain is a band, not a lowpass: measured recordings
   // are 30 to 50 dB down at 100 Hz relative to their peak. A lowpass alone
   // passes everything underneath it flat and floods the bottom two octaves,
   // which is the single thing that stops synthetic rain sounding like rain.
   mBedHpCutoff = clampv(mBedCutoff * kBedBandRatio, 60.0f, 3000.0f);
   // Noise through a 2-pole lowpass loses level as the corner drops; compensate
   // so that sweeping Bed Tone changes colour rather than loudness.
   mBedGainComp = clampv(1.0f / std::sqrt(clampv(mBedCutoff / 4000.0f, 0.05f, 8.0f)), 0.35f, 3.0f);

   mDistanceAtten = 1.0f / (1.0f + 2.0f * mP.distance);

   // Expected number of droplets ringing at any one time, rate x mean ring
   // time. Incoherent sources sum as sqrt(N), so scaling each droplet by
   // 1/sqrt(1+N) keeps loudness roughly constant while Density and Drop Decay
   // are swept. Density stays a texture control instead of doubling as a
   // volume control, and presets set their level explicitly via Output Gain.
   {
      const SurfaceProfile &sp = kSurfaces[clampv(mP.surface, 0, kNumSurfaces - 1)];
      const float meanDecay = mP.dropDecaySec * sp.decayMul * (1.0f + 0.6f * mP.decaySpread);
      const float concurrency = clampv(mP.densityHz * meanDecay, 0.0f, 1.0e6f);
      // Never scale *up*: an isolated droplet keeps its natural amplitude.
      mDensityNorm = 1.0f / std::sqrt(std::max(1.0f, concurrency));
   }

   const float widthAngle = mP.bedWidth * 0.785398163f; // width * pi/4
   mBedMixA = std::cos(widthAngle);
   mBedMixB = std::sin(widthAngle);
   // Equal power, so sliding the bed across does not change how loud it is.
   const float bedPanAngle = (clampv(mP.bedPan, -1.0f, 1.0f) + 1.0f) * 0.785398163f;
   mBedPanL = std::cos(bedPanAngle) * 1.41421356f;
   mBedPanR = std::sin(bedPanAngle) * 1.41421356f;

   const float modRate = mSampleRate / static_cast<float>(kModInterval);
   mDriftCoef = clampv(onePoleCoef(1.2f, modRate), 1.0e-5f, 1.0f);
   mDriftNorm = std::sqrt((2.0f - mDriftCoef) / mDriftCoef);
   mClumpCoef = clampv(onePoleCoef(0.25f, modRate), 1.0e-5f, 1.0f);
   mClumpNorm = std::sqrt((2.0f - mClumpCoef) / mClumpCoef);

   updateFilters();

   for (auto &v : mVoices) {
      v.env.setParams(mP.attackSec, mP.decaySec, mP.sustain, mP.releaseSec, mSampleRate);
      if (v.active) {
         v.bedLpL.setCutoff(mBedCutoff, mP.bedBody * 0.9f, mSampleRate);
         v.bedLpR.setCutoff(mBedCutoff, mP.bedBody * 0.9f, mSampleRate);
         v.bedHpL.setCutoff(mBedHpCutoff, mSampleRate);
         v.bedHpR.setCutoff(mBedHpCutoff, mSampleRate);
      }
   }
}

void RainEngine::updateFilters() {
   float cutoff = mP.filterCutoffHz;
   if (mP.filterKeyTrack > 0.0f)
      cutoff *= std::exp2(mP.filterKeyTrack * (mLastKey - 60) / 12.0f);
   mFilterCutoff = clampv(cutoff, 20.0f, 0.49f * mSampleRate);

   // A wide-open lowpass is bypassed outright: no colouring, no CPU.
   mFilterBypass = (mP.filterType == kFilterLowpass && mFilterCutoff >= 0.45f * mSampleRate) ||
                   (mP.filterType == kFilterHighpass && mFilterCutoff <= 21.0f);

   mFilterL.setCutoff(mFilterCutoff, mP.filterReso, mSampleRate);
   mFilterR.setCutoff(mFilterCutoff, mP.filterReso, mSampleRate);

   // Branch-free type selection: weight the three SVF outputs.
   mFilterWLp = mFilterWBp = mFilterWHp = 0.0f;
   switch (mP.filterType) {
   case kFilterLowpass:
      mFilterWLp = 1.0f;
      break;
   case kFilterBandpass:
      mFilterWBp = mFilterL.k(); // normalised constant-peak bandpass
      break;
   case kFilterHighpass:
      mFilterWHp = 1.0f;
      break;
   case kFilterNotch:
   default:
      mFilterWLp = 1.0f;
      mFilterWHp = 1.0f;
      break;
   }
}

void RainEngine::noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId,
                        double velocity) {
   Voice *slot = nullptr;
   for (auto &v : mVoices) {
      if (!v.active) {
         slot = &v;
         break;
      }
   }
   if (!slot) {
      // Steal the quietest voice.
      float best = 1e9f;
      for (auto &v : mVoices) {
         const float l = v.env.level();
         if (l < best) {
            best = l;
            slot = &v;
         }
      }
   }
   if (!slot)
      return;

   slot->active = true;
   slot->held = true;
   slot->port = port;
   slot->channel = channel;
   slot->key = key;
   slot->noteId = noteId;
   slot->velocity = static_cast<float>(clampv(velocity, 0.0, 1.0));
   slot->env.setParams(mP.attackSec, mP.decaySec, mP.sustain, mP.releaseSec, mSampleRate);
   slot->env.gateOn();
   slot->bedLpL.reset();
   slot->bedLpR.reset();
   slot->bedHpL.reset();
   slot->bedHpR.reset();
   slot->bedLpL.setCutoff(mBedCutoff, mP.bedBody * 0.9f, mSampleRate);
   slot->bedLpR.setCutoff(mBedCutoff, mP.bedBody * 0.9f, mSampleRate);
   slot->bedHpL.setCutoff(mBedHpCutoff, mSampleRate);
   slot->bedHpR.setCutoff(mBedHpCutoff, mSampleRate);
   slot->driftState = 0.0f;
   slot->clumpState = 0.0f;
   slot->modCounter = 0;
   slot->dropTimer = 0.0; // first droplet lands immediately

   if (key >= 0)
      mLastKey = key;
   updateFilters();
}

void RainEngine::noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (v.active && v.held && noteMatches(v, port, channel, key, noteId)) {
         v.held = false;
         v.env.gateOff();
      }
   }
}

void RainEngine::choke(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (v.active && noteMatches(v, port, channel, key, noteId)) {
         v.env.kill();
         v.active = false;
         v.held = false;
      }
   }
}

void RainEngine::allSoundOff() {
   for (auto &v : mVoices) {
      v.env.kill();
      v.active = false;
      v.held = false;
   }
   for (auto &d : mDroplets)
      d.active = false;
   mSpace.clear();
}

Droplet *RainEngine::allocateDroplet() {
   Droplet *quietest = nullptr;
   float quietestPeak = 1e9f;
   // Scanning from a rotating cursor means a free slot is normally found within
   // a few steps instead of re-walking the whole pool for every droplet, which
   // matters at thousands of births per second.
   for (uint32_t n = 0; n < mDropletLimit; ++n) {
      uint32_t i = mDropletCursor + n;
      if (i >= mDropletLimit)
         i -= mDropletLimit;
      Droplet &d = mDroplets[i];
      if (!d.active) {
         mDropletCursor = (i + 1 < mDropletLimit) ? i + 1 : 0;
         return &d;
      }
      const float p = d.peak();
      if (p < quietestPeak) {
         quietestPeak = p;
         quietest = &d;
      }
   }
   // The pool is full. Reuse a slot only if its droplet has already decayed to
   // effective silence -- stealing an audible one would click.
   if (quietest && quietestPeak < 1.0e-3f)
      return quietest;
   return nullptr;
}

void RainEngine::spawnDroplet(Voice &v, float envLevel, uint32_t offset) {
   Droplet *dp = allocateDroplet();
   if (!dp)
      return;
   Droplet &d = *dp;
   const SurfaceProfile &sp = kSurfaces[clampv(mP.surface, 0, kNumSurfaces - 1)];

   // --- Size: a Marshall-Palmer-ish skew towards many small drops and few big
   // ones. u^k has mean 1/(k+1), so scaling by (k+1) keeps the mean volume of a
   // drop independent of the spread amount.
   const float k = mP.levelSpread * 2.5f;
   const float u = mRng.uniformPositive();
   // u^k * (k+1) has mean 1 and a natural maximum of k+1, so a ceiling of 2
   // was not catching freak drops -- it was flattening the top of the intended
   // distribution, one droplet in seven at the default Level Spread. The guard
   // now sits above that maximum, where it only ever catches a bad parameter.
   const float volumeRand = clampv(std::pow(u, k) * (k + 1.0f), 0.0f, 4.0f);
   // The draw is a volume, which goes as r^3, so relative radius is its cube
   // root. Pitch, ring time and the impact all key off this.
   const float sizeRel = clampv(std::pow(volumeRand + 1.0e-6f, 1.0f / 3.0f), 0.3f, 3.0f);

   // --- Level: how loud a drop is follows from the energy it arrives with, not
   // from how much water it contains. Only about a tenth of a per cent of the
   // kinetic energy 1/2 m v^2 leaves an impact as sound, and amplitude is the
   // square root of energy, so the law is A ~ sqrt(m) * v: relative size to the
   // power one and a half, times the speed the drop was falling at. The engine
   // used the volume itself, which is mass, and that is the wrong power by a
   // factor of the size again. Over the range of sizes the engine draws it
   // spread the loudest drop against the quietest by 130 to 1 where the energy
   // law gives 45 to 1, so big drops read as isolated plonks over a bed instead
   // of as the top of a rain texture.
   //
   // Velocity is not a constant either, which is why this cannot be folded into
   // the exponent: it is terminalVelocityMs above, and it needs a real diameter
   // where the engine has only a relative size, so kMedianDropMm ties the two
   // together. The clamped sizeRel is used rather than the raw draw, so the
   // drop that is heard is the drop that was synthesised; the old law took its
   // level from the unclamped volume and so rendered the drops that hit the
   // floor of the size clamp at very nearly zero.
   //
   // The old law kept mean loudness independent of Level Spread with its (k+1)
   // factor, whose mean is one. The equivalent here is analytic for the size
   // term: sizeRel^1.5 is (u^k (k+1))^0.5, and the mean of that over u is
   // exactly 2 sqrt(k+1) / (k+2). Dividing by it, and by the velocity of the
   // median drop, leaves a mean of one. The velocity half of that is only exact
   // for a narrow spread, because v is curved; integrating the real draw
   // numerically over the whole range of k puts the actual mean between 1.00
   // and 1.05, so mean droplet loudness drifts by under half a decibel across
   // the whole of Level Spread, against 0 dB before. Level Spread still changes
   // texture and not volume.
   const float vTerm = terminalVelocityMs(kMedianDropMm * sizeRel);
   const float levelNorm = 2.0f * std::sqrt(k + 1.0f) / (k + 2.0f) * kMedianDropVelMs;
   const float levelRand =
      clampv(sizeRel * std::sqrt(sizeRel) * vTerm / levelNorm, 0.0f, 4.0f);

   // --- Pitch: a resonating droplet's frequency is inversely proportional to
   // its radius, so big drops plop low and fine drops tick high. The user's
   // Pitch Spread adds an explicit random octave offset on top.
   // Spread is deliberately lopsided. A drop twice the size is far rarer than
   // one half the size, and pitch goes as 1/radius, so the real distribution
   // has a long tail upwards and a short one downwards. A symmetric spread
   // instead throws as many droplets two octaves down as up, and those are
   // what turn a rain texture into mud.
   float spreadG = clampv(mRng.gaussian() * 0.5f, -2.0f, 2.0f);
   if (spreadG < 0.0f)
      spreadG *= kDownwardSpread;
   const float spreadOct = mP.pitchSpreadOct * spreadG;
   const float noteRatio = std::exp2(mP.noteTracking * (v.key - 60) / 12.0f);
   float freq = mP.dropPitchHz * noteRatio * (1.0f / sizeRel) * std::exp2(spreadOct);
   freq = clampv(freq, 20.0f, 0.45f * mSampleRate);

   // --- Ring time: bigger drops ring longer; the surface scales it further.
   const float decayRand = std::exp2(mP.decaySpread * clampv(mRng.gaussian() * 0.8f, -2.5f, 2.5f));
   const float tonalDecaySec =
      clampv(mP.dropDecaySec * sp.decayMul * decayRand * sizeRel, 0.0005f, 4.0f);
   const float noiseDecaySec = clampv(tonalDecaySec * (0.12f + 0.9f * mP.splash), 0.0003f, 4.0f);

   // --- Impact: one frequency per droplet, damped at twice that frequency.
   // decayCoef() takes the time to -60 dB, and e^(-2 f t) reaches it at
   // ln(1000) / (2 f), so a 1 kHz tick lasts 3.5 ms and a 16 kHz one 0.2 ms.
   const float impactHz =
      clampv((kImpactMinHz + (kImpactMaxHz - kImpactMinHz) * mRng.uniformPositive()) *
                sp.brightness,
             80.0f, kImpactMaxRate * mSampleRate);
   const float clickDecaySec = 6.907755279f / (kImpactDampPerHz * impactHz);

   // --- Distance: nearer drops are louder and brighter. Air absorption sets
   // how quickly the high end is lost with distance.
   //
   // Droplets land over an area, and the number falling at a given radius grows
   // with that radius, so most are far and only a few are close. Drawing the
   // radius as sqrt(u) reproduces that; the mean is within a per cent of the
   // old uniform 0.35..1 spread, so Distance still means what it meant, but the
   // occasional very near droplet -- louder and brighter than the rest -- is
   // what stops the texture reading as a flat, even patter.
   const float dist = mP.distance * std::sqrt(mRng.uniformPositive());
   const float distAtten = 1.0f / (1.0f + 3.0f * dist);
   const float airCutoff =
      clampv(18000.0f * sp.brightness * std::exp2(-6.0f * dist * mP.air), 250.0f,
             0.45f * mSampleRate);

   const float amp = kDropletBaseAmp * mDensityNorm * envLevel * levelRand * distAtten;

   // --- Not every impact traps a bubble. Pumphrey and Elmore's measurements,
   // quoted by Liu et al., have only a band of drop sizes entraining one on
   // every impact; the rest of the rain is splash and tick with no pitch at all.
   // Bubble Chance is that fraction, and at 100 % every droplet rings, which is
   // what the engine did before the control existed.
   const bool hasBubble = mRng.uniformPositive() < mP.bubbleChance;

   // --- Layer weights.
   const float tonal = hasBubble ? mP.tonality * sp.tonalMul : 0.0f;
   const float wet = (1.0f - 0.7f * mP.tonality) * (0.35f + 0.9f * mP.splash) * sp.splashMul;
   const float click = mP.impact * sp.clickMul * 0.5f;

   d.noiseAmp = amp * wet;
   d.clickAmp = amp * click;
   d.noiseDecay = decayCoef(noiseDecaySec, mSampleRate);
   d.clickDecay = decayCoef(clickDecaySec, mSampleRate);

   // --- Tonal layer: e^(-t/decay) - e^(-t/rise), so it swells in behind the
   // impact rather than appearing at full level on the first sample. The pair
   // is normalised by its own peak, which keeps `amp` the layer's real peak.
   float tonalRiseSec =
      clampv(tonalDecaySec * kTonalRiseFraction * sp.onsetMul, kTonalRiseMinSec,
             kTonalRiseMaxSec);
   tonalRiseSec = std::min(tonalRiseSec, tonalDecaySec * kTonalRiseCeiling);
   const float riseRatio = clampv(tonalRiseSec / tonalDecaySec, 1.0e-4f, kTonalRiseCeiling);
   const float peakNorm = std::pow(riseRatio, riseRatio / (1.0f - riseRatio)) -
                          std::pow(riseRatio, 1.0f / (1.0f - riseRatio));
   const float tonalPeak = amp * tonal / (peakNorm > 1.0e-3f ? peakNorm : 1.0f);
   d.tonalAmp = tonalPeak;
   d.tonalRise = tonalPeak;
   d.tonalDecay = decayCoef(tonalDecaySec, mSampleRate);
   d.tonalRiseDecay = decayCoef(tonalRiseSec, mSampleRate);

   d.phase = mRng.uniform();
   d.phaseInc = freq / mSampleRate;

   // --- Second bubble mode. Its own phase and its own faster decay; the ratio
   // and level are redrawn per droplet so no two drops ring quite alike.
   const float harmRatio =
      kHarmonicRatio * std::exp2(kHarmonicRatioSpreadOct *
                                 clampv(mRng.gaussian(), -kHarmonicSpreadClamp,
                                        kHarmonicSpreadClamp));
   const float harmLevel =
      sp.harmonic *
      std::pow(10.0f, kHarmonicLevelSpreadDb *
                         clampv(mRng.gaussian(), -kHarmonicSpreadClamp, kHarmonicSpreadClamp) /
                         20.0f);
   d.harmPhase = mRng.uniform();
   d.harmPhaseInc = clampv(freq * harmRatio / mSampleRate, 1.0e-5f, 0.45f);
   d.harmAmp = tonalPeak * harmLevel;
   d.harmDecay = decayCoef(tonalDecaySec / kHarmonicDecayRatio, mSampleRate);

   d.clickPhase = 0.0f;
   d.clickPhaseInc = impactHz / mSampleRate;

   // --- Chirp: a droplet trapping an air bubble in water rises in pitch as the
   // bubble shrinks. The per-sample frequency multiplier starts high and relaxes
   // towards 1 with its own time constant, taken from the ring time, so the
   // total sweep comes to chirpOct octaves spread across the drop's audible
   // life (see kChirpTauFraction: front-loading the same bend into the attack
   // is what makes it read as a swoop rather than as a bubble settling).
   //
   // Drops do not all bend by the same amount. Measured across the isolated
   // drops in the references the bend runs from about nothing to +0.17 octaves
   // with a median near +0.03, so the setting is the mean of a uniform draw
   // rather than a fixed amount: 2u has mean 1, which leaves Chirp meaning what
   // it meant while no two droplets bend alike.
   const float chirpOct = mP.chirp * sp.chirpOct * 2.0f * mRng.uniformPositive();
   const float chirpTauSec =
      clampv(tonalDecaySec * kChirpTauFraction, kChirpTauMinSec, kChirpTauMaxSec);
   const float chirpSamples = chirpTauSec * mSampleRate;
   if (chirpSamples > 1.0f && std::fabs(chirpOct) > 1.0e-4f) {
      d.chirpRate = std::exp2(chirpOct / chirpSamples);
      d.chirpRelax = std::exp(-1.0f / chirpSamples);
   } else {
      d.chirpRate = 1.0f;
      d.chirpRelax = 0.0f;
   }

   // --- Resonator for the wet layer.
   const float reso = clampv(0.35f + 0.62f * mP.tonality * sp.resonance * 1.4f, 0.0f, 0.985f);
   d.resonator.reset();
   d.resonator.setCutoff(freq, reso, mSampleRate);
   d.air.reset();
   d.air.setCutoff(airCutoff, mSampleRate);
   // Radiation rolloff. Power from a small source falls as f^2 once the source
   // is much smaller than the wavelength, so 12 dB/oct below the droplet's own
   // resonance is the right shape, not an arbitrary tidy-up.
   d.body.reset();
   d.body.setCutoff(clampv(freq * kBodyHpRatio, 30.0f, 0.4f * mSampleRate), mSampleRate);

   // --- Placement in the stereo field.
   const float pan = clampv(mRng.white() * mP.width + mP.dropPan, -1.0f, 1.0f);
   const float panAngle = (pan + 1.0f) * 0.785398163f; // maps -1..1 to 0..pi/2
   d.gainL = std::cos(panAngle);
   d.gainR = std::sin(panAngle);

   // --- Lifetime: 1.6x the slowest decay is about -95 dB, plus the resonator's
   // own ring-down. A short fade at the end keeps the hard stop inaudible.
   const float qRing = (1.0f / clampv(d.resonator.k(), 0.02f, 2.0f)) / (3.14159265f * freq);
   const float ringSec = hasBubble ? tonalDecaySec : 0.0f;
   const float longest = std::max(ringSec, std::max(noiseDecaySec + qRing, clickDecaySec));
   d.lifeMax = static_cast<uint32_t>(clampv(1.6f * longest, 0.001f, 4.0f) * mSampleRate) + 96;
   d.life = 0;
   d.startOffset = offset;
   d.active = true;
}

void RainEngine::processVoiceBed(Voice &v, float *outL, float *outR, uint32_t numSamples) {
   const float levelMul = (1.0f - mP.velToLevel) + mP.velToLevel * v.velocity;
   const float densMul = (1.0f - mP.velToDensity) +
                         mP.velToDensity * (0.02f + 1.98f * v.velocity * v.velocity);
   const float bedAmp = mP.bedGain * mBedGainComp * mDistanceAtten * levelMul;
   const float baseRate = mP.densityHz * densMul;

   for (uint32_t i = 0; i < numSamples; ++i) {
      const float env = v.env.tick();
      if (v.env.isIdle()) {
         v.active = false;
         return;
      }

      // Control-rate random walks: slow intensity drift, faster clumping.
      if (v.modCounter == 0) {
         v.driftState += mDriftCoef * (mRng.gaussian() - v.driftState);
         v.clumpState += mClumpCoef * (mRng.gaussian() - v.clumpState);
      }
      if (++v.modCounter >= kModInterval)
         v.modCounter = 0;

      const float drift = v.driftState * mDriftNorm;
      const float clump = v.clumpState * mClumpNorm;

      // --- Noise bed: two decorrelated white sources, mixed to the requested
      // width, lowpassed and highpassed, scaled by drift.
      const float n1 = mRng.white();
      const float n2 = mRng.white();
      const float bl = mBedMixA * n1 + mBedMixB * n2;
      const float br = mBedMixA * n1 - mBedMixB * n2;
      const float driftGain = std::exp2(mP.bedDrift * 1.2f * drift);
      const float g = env * bedAmp * clampv(driftGain, 0.1f, 4.0f);
      outL[i] += v.bedHpL.tick(v.bedLpL.lowpass(bl)) * g * mBedPanL;
      outR[i] += v.bedHpR.tick(v.bedLpR.lowpass(br)) * g * mBedPanR;

      // --- Droplet scheduling: a Cox process, i.e. a Poisson process whose
      // rate is itself modulated. Exponential waiting times give correct
      // clustering statistics; the log-normal rate modulation is compensated so
      // the mean rate stays where the user put it.
      v.dropTimer -= 1.0f;
      while (v.dropTimer <= 0.0f) {
         spawnDroplet(v, env * levelMul, i);
         const float clumpExp = mP.clumping * 2.0f;
         const float clumpMul = std::exp(clumpExp * clump - 0.5f * clumpExp * clumpExp);
         const float driftMul = std::exp2(mP.bedDrift * 0.8f * drift);
         float rate = baseRate * (0.3f + 0.7f * env) * clampv(clumpMul, 0.02f, 12.0f) *
                      clampv(driftMul, 0.25f, 4.0f);
         rate = clampv(rate, 0.005f, kMaxBirthRate);
         const float wait = mRng.exponential(rate) * mSampleRate;
         v.dropTimer += wait < 1.0f ? 1.0f : wait;
      }
   }
}

void RainEngine::processDroplets(float *outL, float *outR, uint32_t numSamples) {
   for (uint32_t di = 0; di < mDropletLimit; ++di) {
      Droplet &d = mDroplets[di];
      if (!d.active)
         continue;

      uint32_t i = d.startOffset;
      d.startOffset = 0;
      const uint32_t fadeStart = d.lifeMax > 64 ? d.lifeMax - 64 : 0;

      for (; i < numSamples; ++i) {
         const float noise = mRng.white();
         // Bubble and splash are radiated by the droplet itself, so both go
         // through its radiation rolloff. The impact is not: it is the surface
         // being struck, and its pitch has nothing to do with the bubble's, so
         // rolling it off below the bubble would silence the low ticks that
         // land under a fine drop. Air absorption applies to all of it.
         float s = (d.tonalAmp - d.tonalRise) * sin2pi(d.phase);
         s += d.harmAmp * sin2pi(d.harmPhase);
         s += d.resonator.bandpassNormalised(noise * d.noiseAmp);
         s = d.body.tick(s);
         s += d.clickAmp * sin2pi(d.clickPhase);
         s = d.air.tick(s);

         if (d.life >= fadeStart)
            s *= static_cast<float>(d.lifeMax - d.life) * (1.0f / 64.0f);

         outL[i] += s * d.gainL;
         outR[i] += s * d.gainR;

         d.phase += d.phaseInc;
         if (d.phase >= 1.0f)
            d.phase -= 1.0f;
         d.harmPhase += d.harmPhaseInc;
         if (d.harmPhase >= 1.0f)
            d.harmPhase -= 1.0f;
         d.clickPhase += d.clickPhaseInc;
         if (d.clickPhase >= 1.0f)
            d.clickPhase -= 1.0f;

         // The second mode is a mode of the same bubble, so it bends with it.
         d.phaseInc *= d.chirpRate;
         d.harmPhaseInc *= d.chirpRate;
         d.chirpRate = 1.0f + (d.chirpRate - 1.0f) * d.chirpRelax;
         if (d.phaseInc > 0.45f)
            d.phaseInc = 0.45f;
         else if (d.phaseInc < 1.0e-5f)
            d.phaseInc = 1.0e-5f;
         if (d.harmPhaseInc > 0.45f)
            d.harmPhaseInc = 0.45f;

         d.tonalAmp *= d.tonalDecay;
         d.tonalRise *= d.tonalRiseDecay;
         d.harmAmp *= d.harmDecay;
         d.noiseAmp *= d.noiseDecay;
         d.clickAmp *= d.clickDecay;

         if (++d.life >= d.lifeMax) {
            d.active = false;
            break;
         }
      }
   }
}

void RainEngine::processOutputChain(float *outL, float *outR, uint32_t numSamples) {
   const bool space = mP.spaceAmount > 0.001f;
   const float wet = mP.spaceAmount;
   const float gain = mP.gain * kEngineMakeup;

   for (uint32_t i = 0; i < numSamples; ++i) {
      float l = outL[i];
      float r = outR[i];

      if (!mFilterBypass) {
         float lp, bp, hp;
         mFilterL.tick(l, lp, bp, hp);
         l = mFilterWLp * lp + mFilterWBp * bp + mFilterWHp * hp;
         mFilterR.tick(r, lp, bp, hp);
         r = mFilterWLp * lp + mFilterWBp * bp + mFilterWHp * hp;
      }

      if (!mHighpassBypass) {
         l = mHighpassL.tick(l);
         r = mHighpassR.tick(r);
      }

      if (space) {
         float wl, wr;
         mSpace.tick(l, r, wl, wr);
         l += wet * wl;
         r += wet * wr;
      }

      outL[i] = softClip(l * gain);
      outR[i] = softClip(r * gain);
   }
}

void RainEngine::process(float *outL, float *outR, uint32_t numSamples) {
   if (numSamples == 0)
      return;

   for (auto &v : mVoices) {
      if (v.active)
         processVoiceBed(v, outL, outR, numSamples);
   }
   processDroplets(outL, outR, numSamples);
   processOutputChain(outL, outR, numSamples);

   if (activeVoiceCount() == 0 && activeDropletCount() == 0) {
      // Saturate rather than wrap: this counter is only compared against the
      // tail length.
      const uint32_t limit = 0xFFFFFFFFu - numSamples;
      mSilenceCounter = mSilenceCounter > limit ? 0xFFFFFFFFu : mSilenceCounter + numSamples;
   } else {
      mSilenceCounter = 0;
   }
}

uint32_t RainEngine::activeVoiceCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices)
      if (v.active)
         ++n;
   return n;
}

uint32_t RainEngine::activeDropletCount() const {
   uint32_t n = 0;
   for (uint32_t i = 0; i < mDropletLimit; ++i)
      if (mDroplets[i].active)
         ++n;
   return n;
}

float RainEngine::tailSeconds() const {
   // Release plus the reverb ring-down, which is what a host needs to know to
   // keep processing after the last note off.
   const float spaceTail = mP.spaceAmount > 0.001f ? 0.5f + 6.0f * mP.spaceSize : 0.05f;
   // Plus a second of slack for the longest droplet ring-down.
   return mP.releaseSec + spaceTail + 1.0f;
}

bool RainEngine::isSilent() const {
   if (activeVoiceCount() != 0 || activeDropletCount() != 0)
      return false;
   return mSilenceCounter > static_cast<uint32_t>(tailSeconds() * mSampleRate);
}

} // namespace rainyday
