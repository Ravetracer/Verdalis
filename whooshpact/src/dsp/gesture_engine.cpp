#include "gesture_engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>

namespace whooshpact {

namespace {

// Seed 0 is the "always different" setting, so two instances must not start
// from the same state. Every other engine in the suite mixes a per-instance
// counter into its start seed for this; this one is no different.
std::atomic<uint32_t> gInstanceCounter{0};

// The control rate. Everything that costs a transcendental -- the gesture
// curve, the filter coefficients, the oscillator frequencies, the flutter rate
// -- is recomputed once per block of this many samples and the audible ones are
// interpolated across it. At 48 kHz that is 1.5 kHz, which is well above the
// fastest flutter the library measures.
constexpr uint32_t kCtrlBlock = 32;

constexpr float kPi = 3.14159265358979323846f;

// Noise colour normalisation. Each colour is white noise with one filter on it,
// and each filter changes the level; these bring all six back to the RMS of
// white so that turning the Noise knob is a change of colour and not of volume.
// Measured by running 400k samples of each through the same code.
constexpr float kPinkNorm = 0.3366f;
constexpr float kBrownNorm = 2.9000f;
constexpr float kBlueNorm = 0.5591f;
constexpr float kVioletNorm = 0.7068f;
// Green is a band rather than a slope, so its figure is the analytic one: a
// constant-peak-gain bandpass at 500 Hz and Q 1.1 passes (pi/2)*f0/Q of the
// spectrum, which at 48 kHz is 3.0 % of the power.
constexpr float kGreenNorm = 5.8000f;

// Middle C. The pitch parameters are literal at this note and transposed from
// it, so a preset's "Sub Pitch = 49 Hz" is 49 Hz when middle C is played --
// which is where a host's keyboard and every step sequencer start.
//
// The measured median fundamental of the library's pitched families is 41-49 Hz,
// all within a tone of G1. That is where the *parameter defaults* sit; it is not
// where the keyboard is centred, and conflating the two would mean a factory
// preset played on the obvious note came out more than two octaves high.
constexpr int kReferenceKey = 60;

// The Hit layer's cluster. Inharmonic on purpose -- struck metal is, and a
// harmonic series here reads as a bell rather than as debris.
constexpr float kHitModeRatio[kHitModes] = {1.0f, 1.71f, 2.63f};

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// polyBLEP: the correction that turns a naive discontinuity into a band-limited
// one. A braam glides upwards by an octave or more, and a naive saw edge at the
// top of that glide aliases audibly.
inline float polyBlep(float t, float dt) {
   if (dt <= 0.0f)
      return 0.0f;
   if (t < dt) {
      const float x = t / dt;
      return x + x - x * x - 1.0f;
   }
   if (t > 1.0f - dt) {
      const float x = (t - 1.0f) / dt;
      return x * x + x + x + 1.0f;
   }
   return 0.0f;
}

inline float waveValue(int kind, float phase, float dt) {
   switch (kind) {
   case kWaveSine:
      return sin2piFast(phase);
   case kWaveTriangle:
      // Naive is enough here: a triangle's harmonics fall 12 dB/octave, so what
      // folds back is already 40 dB down by the time it would be heard.
      return 4.0f * std::fabs(phase - 0.5f) - 1.0f;
   case kWaveSquare: {
      float v = phase < 0.5f ? 1.0f : -1.0f;
      v += polyBlep(phase, dt);
      float p2 = phase + 0.5f;
      if (p2 >= 1.0f)
         p2 -= 1.0f;
      v -= polyBlep(p2, dt);
      return v;
   }
   default: // saw and supersaw
      return (2.0f * phase - 1.0f) - polyBlep(phase, dt);
   }
}

} // namespace

// ---------------------------------------------------------------- noise

inline float NoiseSource::tick(int kind) {
   const float w = rng.white();
   // Both running filters are advanced whatever colour is selected, so that
   // changing colour never starts from a dead state and clicks.
   p0 = 0.99765f * p0 + w * 0.0990460f;
   p1 = 0.96300f * p1 + w * 0.2965164f;
   p2 = 0.57000f * p2 + w * 1.0526913f;
   const float pink = p0 + p1 + p2 + w * 0.1848f;
   brown = (brown + 0.022f * w) * 0.998f;

   float out;
   switch (kind) {
   case kNoisePink:
      out = pink * kPinkNorm;
      break;
   case kNoiseBrown:
      out = brown * kBrownNorm;
      break;
   case kNoiseBlue:
      // Pink differentiated: -3 dB/octave plus 6 is +3.
      out = (pink - lastPink) * kBlueNorm;
      break;
   case kNoiseViolet:
      // White differentiated: +6 dB/octave.
      out = (w - lastWhite) * kVioletNorm;
      break;
   case kNoiseGreen:
      out = green.bandpassNormalised(w) * kGreenNorm;
      break;
   default:
      out = w;
      break;
   }
   lastPink = pink;
   lastWhite = w;
   return out;
}

// --------------------------------------------------------------- lifecycle

void GestureEngine::prepare(double sampleRate, uint32_t /*maxBlockSize*/) {
   mSampleRate = sampleRate;
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   mRng.reseed(static_cast<uint32_t>(now) ^
               (0x9E3779B9u * (gInstanceCounter.fetch_add(1) + 1)));
   // The sub layer works down to 15 Hz, so the tank's loop highpass has to sit
   // below that or a boom loses its bottom the moment it enters the reverb.
   mSpace.prepare(static_cast<float>(sampleRate), 18.0f);
   reset();
   updateFilters();
}

void GestureEngine::reset() {
   for (auto &v : mVoices) {
      v.active = false;
      v.pendingStart = false;
      v.env.reset();
      v.pos = 1.0;
      v.subAmp = 0.0f;
      v.hitAmp = 0.0f;
      v.subFired = false;
      v.hitFired = false;
      v.airL.reset();
      v.airR.reset();
      v.tiltL.reset();
      v.tiltR.reset();
      v.hitBand.reset();
      v.hitBodyFilter.reset();
      for (auto &m : v.hitMode)
         m.reset();
      v.flutterLp.reset();
   }
   mSpace.clear();
   mHpL.reset();
   mHpR.reset();
   mLpL.reset();
   mLpR.reset();
   mEqLowL.reset();
   mEqLowR.reset();
   mEqMidL.reset();
   mEqMidR.reset();
   mEqHighL.reset();
   mEqHighR.reset();
   mProfileTiltL.reset();
   mProfileTiltR.reset();
   mProfileShelfL.reset();
   mProfileShelfR.reset();
   // A non-zero Seed promises the same sequence of gestures every time. Seed 0
   // is the "always different" setting and deliberately keeps running: falling
   // back to a constant here made every instance, and every restart, produce
   // the same gestures.
   if (mP.seed > 0)
      mRng.reseed(static_cast<uint32_t>(mP.seed));
   mAppliedSeed = mP.seed;
   mSilenceCounter = 0.0f;
}

void GestureEngine::setParams(const EngineParams &p) {
   const int previousSeed = mP.seed;
   mP = p;
   // A non-zero Seed promises the same sequence of gestures every time, so
   // setting it has to restart the sequence rather than merely change it.
   if (p.seed != previousSeed) {
      if (p.seed > 0)
         mRng.reseed(static_cast<uint32_t>(p.seed));
      mAppliedSeed = p.seed;
   }
   updateFilters();
}

void GestureEngine::updateFilters() {
   const float sr = static_cast<float>(mSampleRate);

   mHpL.setCutoff(mP.highpassHz, sr);
   mHpR.setCutoff(mP.highpassHz, sr);
   mLpL.setCutoff(mP.lowpassHz, sr);
   mLpR.setCutoff(mP.lowpassHz, sr);

   mEqLowL.setLowShelf(mP.eqLowFreqHz, mP.eqLowGainDb, sr, 0.7f);
   mEqLowR.setLowShelf(mP.eqLowFreqHz, mP.eqLowGainDb, sr, 0.7f);
   mEqMidL.setPeaking(mP.eqMidFreqHz, mP.eqMidGainDb, 0.9f, sr);
   mEqMidR.setPeaking(mP.eqMidFreqHz, mP.eqMidGainDb, 0.9f, sr);
   mEqHighL.setHighShelf(mP.eqHighFreqHz, mP.eqHighGainDb, sr, 0.7f);
   mEqHighR.setHighShelf(mP.eqHighFreqHz, mP.eqHighGainDb, sr, 0.7f);

   // The measured family profile, blended towards the next family so that the
   // six are a continuum. See params.cpp for where the two numbers come from.
   const int a = clampv(mP.type, 0, static_cast<int>(kNumGestureKinds) - 1);
   const int b = (a + 1) % kNumGestureKinds;
   const float t = clampv(mP.blend, 0.0f, 1.0f);
   const TypeProfile &pa = typeProfile(a);
   const TypeProfile &pb = typeProfile(b);
   // 2.2 octaves' worth of the measured slope, which is what a single shelf can
   // carry without swallowing the layer it is shaping.
   const float tiltDb = lerpf(pa.tiltDbPerOct, pb.tiltDbPerOct, t) * 2.2f;
   const float shelfDb = lerpf(pa.shelfDb, pb.shelfDb, t) * 0.8f;
   mProfileTiltL.setHighShelf(700.0f, tiltDb, sr, 0.5f);
   mProfileTiltR.setHighShelf(700.0f, tiltDb, sr, 0.5f);
   mProfileShelfL.setLowShelf(80.0f, shelfDb, sr, 0.7f);
   mProfileShelfR.setLowShelf(80.0f, shelfDb, sr, 0.7f);

   mSpace.setSize(mP.spaceSize);
   mSpace.setDamping(mP.spaceDamping);
   // These are made sounds rather than field recordings, and the room they are
   // heard in is a production choice: a bigger space is a more enclosed one.
   mSpace.setEnclosure(0.35f + 0.55f * clampv(mP.spaceSize, 0.0f, 1.0f));
}

// ------------------------------------------------------------------- notes

Voice *GestureEngine::allocateVoice() {
   const uint32_t limit =
      static_cast<uint32_t>(clampv(mP.maxVoices, 1, static_cast<int>(kMaxVoices)));
   Voice *oldest = nullptr;
   double furthest = -1.0;
   for (uint32_t i = 0; i < limit; ++i) {
      Voice &v = mVoices[i];
      if (!v.active)
         return &v;
      if (v.pos > furthest) {
         furthest = v.pos;
         oldest = &v;
      }
   }
   return oldest;
}

void GestureEngine::drawVariation(Voice &v) {
   const float amount = clampv(mP.variation, 0.0f, 1.0f);
   if (amount <= 0.0f) {
      v.vPeak = 1.0f;
      v.vCutoff = 1.0f;
      v.vSweep = 0.0f;
      v.vPitch = 1.0f;
      v.vLevel = 1.0f;
      v.vPanOff = 0.0f;
      v.vFlutter = 1.0f;
      v.vHitTone = 1.0f;
      v.vDecay = 1.0f;
      return;
   }
   // Gaussian rather than uniform: the point is that most triggers are near the
   // setting on screen and a few are noticeably not, which is what a library of
   // hand-made variants of one sound actually looks like. Clipped at two sigma
   // so that no single note can land somewhere absurd.
   auto draw = [this]() { return clampv(mRng.gaussian(), -2.0f, 2.0f); };

   // Multiplicative, like every other draw here. An additive offset would be a
   // fixed number of span-fractions whatever Peak is set to, which swamps the
   // hit families -- they sit at a Peak of 0.012 to 0.03, so an offset sized for
   // a transition's 0.33 moved their strike by several times its own value and
   // clamping at zero rectified the draw: half the notes on the beat and half
   // straggling. A ratio keeps the spread proportional to the setting.
   v.vPeak = std::exp2(amount * 0.25f * draw());
   v.vCutoff = std::exp2(amount * 0.55f * draw());
   v.vSweep = amount * 0.35f * draw();
   v.vPitch = std::exp2(amount * (2.5f / 12.0f) * draw());
   v.vLevel = dbToGain(amount * 3.0f * draw());
   v.vPanOff = amount * 0.30f * draw();
   v.vFlutter = std::exp2(amount * 0.45f * draw());
   v.vHitTone = std::exp2(amount * 0.40f * draw());
   v.vDecay = std::exp2(amount * 0.35f * draw());
}

// Everything a trigger draws for itself. Called from process() rather than from
// noteOn, so that a note and a parameter change arriving in the same block are
// seen in the order the host sent them: a Random Seed set alongside the first
// note has to govern that note, not the one after it.
void GestureEngine::beginVoice(Voice &v) {
   const float sr = static_cast<float>(mSampleRate);

   v.pendingStart = false;
   v.keyRatio = semitonesToRatio(static_cast<float>(v.key - kReferenceKey));

   drawVariation(v);

   // The span itself varies, and it is the one variation drawn here rather than
   // in drawVariation(): it sets the clock the rest of the gesture runs on.
   const float spanVar =
      mP.variation > 0.0f
         ? std::exp2(clampv(mP.variation, 0.0f, 1.0f) * 0.30f *
                     clampv(mRng.gaussian(), -2.0f, 2.0f))
         : 1.0f;
   const float span = clampv(mP.spanSec * spanVar, 0.05f, 60.0f);
   v.pos = 0.0;
   v.inc = 1.0 / (static_cast<double>(span) * mSampleRate);

   v.env.setParams(mP.attackSec, mP.decaySec, mP.sustain, mP.releaseSec, sr);
   v.env.reset();
   v.env.gateOn();

   const uint32_t seed = mRng.next() | 1u;
   v.noiseL.prepare(seed, sr);
   v.noiseR.prepare(seed * 2654435761u + 12345u, sr);
   v.hitRng.seed(seed ^ 0xA5A5A5A5u);

   v.airL.reset();
   v.airR.reset();
   v.tiltL.reset();
   v.tiltR.reset();
   v.tiltL.set(1000.0f, mP.airTilt * 6.0f, sr);
   v.tiltR.set(1000.0f, mP.airTilt * 6.0f, sr);

   for (int i = 0; i < kToneOscs; ++i) {
      // A free-running phase per oscillator, so two gestures triggered together
      // do not stack into one louder one.
      v.tonePhase[i] = mRng.uniform();
      v.toneSpread[i] = kToneOscs > 1 ? (2.0f * i / (kToneOscs - 1) - 1.0f) : 0.0f;
   }

   v.subFired = false;
   v.subPhase = 0.0f;
   v.subAmp = 0.0f;
   v.subDrop = 0.0f;
   v.subClickEnv = 0.0f;

   v.hitFired = false;
   v.hitAmp = 0.0f;
   v.hitBand.reset();
   v.hitBodyFilter.reset();
   for (auto &m : v.hitMode)
      m.reset();

   v.flutterPhase = 0.0f;
   v.flutterHold = 1.0f;
   v.flutterLp.reset();

   ++mGestureCounter;
}

void GestureEngine::noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId,
                           double velocity) {
   Voice *v = allocateVoice();
   if (!v)
      return;
   // Only the identity is taken here. See beginVoice().
   v->active = true;
   v->pendingStart = true;
   v->port = port;
   v->channel = channel;
   v->key = key;
   v->noteId = noteId;
   v->velocity = static_cast<float>(clampv(velocity, 0.0, 1.0));
   v->pos = 0.0;
   v->env.reset();
   v->subAmp = 0.0f;
   v->hitAmp = 0.0f;
}

void GestureEngine::noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = noteId >= 0 ? v.noteId == noteId
                                     : (v.port == port && v.channel == channel && v.key == key);
      if (match)
         v.env.gateOff();
   }
}

void GestureEngine::choke(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = noteId >= 0 ? v.noteId == noteId
                                     : (v.port == port && v.channel == channel && v.key == key);
      if (match) {
         v.env.kill();
         v.active = false;
         v.subAmp = 0.0f;
         v.hitAmp = 0.0f;
      }
   }
}

void GestureEngine::allSoundOff() {
   for (auto &v : mVoices) {
      v.env.kill();
      v.active = false;
      v.subAmp = 0.0f;
      v.hitAmp = 0.0f;
   }
}

// ----------------------------------------------------------------- gesture

inline float GestureEngine::gestureLevel(float t, float peak) const {
   if (t <= 0.0f || t >= 1.0f)
      return 0.0f;
   const float p = clampv(peak, 0.0f, 0.95f);
   const float holdEnd = clampv(p + clampv(mP.hold, 0.0f, 1.0f), p, 0.995f);
   if (t < p)
      return p <= 1.0e-5f ? 1.0f : std::pow(t / p, mP.rise);
   if (t < holdEnd)
      return 1.0f;
   const float u = (t - holdEnd) / (1.0f - holdEnd);
   return std::pow(clampv(1.0f - u, 0.0f, 1.0f), mP.fall);
}

inline float GestureEngine::sweepWarp(float t) const {
   const float u = clampv(t, 0.0f, 1.0f);
   return std::pow(u, clampv(mP.airCurve, 0.05f, 8.0f));
}

inline float GestureEngine::flutterValue(Voice &v, float rate, float dtPerSample) {
   v.flutterPhase += rate * dtPerSample;
   if (v.flutterPhase >= 1.0f) {
      v.flutterPhase -= std::floor(v.flutterPhase);
      v.flutterHold = v.noiseL.rng.uniformPositive();
   }
   const float u = v.flutterPhase;

   float s;
   switch (mP.flutterShape) {
   case kFlutterTriangle:
      s = 1.0f - 2.0f * std::fabs(u - 0.5f);
      break;
   case kFlutterSquare:
      s = u < 0.5f ? 1.0f : 0.0f;
      break;
   case kFlutterRamp:
      s = 1.0f - u;
      break;
   case kFlutterRandom:
      s = v.flutterHold;
      break;
   default:
      s = 0.5f + 0.5f * sin2piFast(u);
      break;
   }
   // The smoothing corner is set once per control block, in processVoice: it
   // follows the rate, so that Smooth means the same thing at 2 Hz and at 40.
   return clampv(v.flutterLp.tick(s), 0.0f, 1.0f);
}

// The rate glides exponentially from the start speed to the end speed across
// the gesture, which is the whole point of the pair: one number a tempo-synced
// LFO cannot express without drawing an automation curve.
inline float GestureEngine::flutterRate(const Voice &v, float t) const {
   const float f0 = clampv(mP.flutterStartHz * v.vFlutter, 0.05f, 200.0f);
   const float f1 = clampv(mP.flutterEndHz * v.vFlutter, 0.05f, 200.0f);
   return f0 * std::pow(f1 / f0, clampv(t, 0.0f, 1.0f));
}

// ------------------------------------------------------------------- voice

void GestureEngine::processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples) {
   if (v.pendingStart)
      beginVoice(v);

   const float sr = static_cast<float>(mSampleRate);
   const float dt = 1.0f / sr;
   const float nyquist = 0.45f * sr;

   const float velLevel = lerpf(1.0f, v.velocity, clampv(mP.velToLevel, 0.0f, 1.0f));
   // Hitting something harder opens it up as well as making it louder, which is
   // what the library's brighter accents are: the same gesture struck harder.
   const float velTone = std::exp2(clampv(mP.velToTone, 0.0f, 1.0f) * (v.velocity - 0.7f) * 2.0f);
   const float voiceLevel = velLevel * v.vLevel;

   const float peak = clampv(mP.peak * v.vPeak, 0.0f, 0.95f);
   const float subDecay = clampv(mP.subDecaySec * v.vDecay, 0.005f, 20.0f);
   const float hitDecay = clampv(mP.hitDecaySec * v.vDecay, 0.002f, 10.0f);
   // The parameters read in -20 dB times because that is what the references
   // were measured in; decayCoef() falls 60 dB, so it is given three times as
   // long.
   const float subCoef = decayCoef(subDecay * 3.0f, sr);
   const float hitCoef = decayCoef(hitDecay * 3.0f, sr);
   // The pitch drop and the click are one-pole recursions rather than exp()
   // per sample: an impact is a cheap sound to make and should stay one.
   const float subDropCoef = onePoleCoef(subDecay * 0.5f, sr);
   const float subClickCoef = decayCoef(0.015f, sr);
   const float subDriveGain = 1.0f + 7.0f * clampv(mP.subDrive, 0.0f, 1.0f);
   const float subDriveComp = 1.0f / (1.0f + 3.0f * clampv(mP.subDrive, 0.0f, 1.0f));
   const float subDropRatio = mP.subDropSt * (1.0f / 12.0f);
   const float subClickAmount = clampv(mP.subClick, 0.0f, 1.0f) * 7.0f;

   const float toneBase = mP.tonePitchHz * v.keyRatio * v.vPitch;
   const float subBase = mP.subPitchHz * v.keyRatio * v.vPitch;
   const float detune =
      clampv(mP.toneDetune, 0.0f, 1.0f) * (mP.wave == kWaveSupersaw ? 0.20f : 0.06f);
   const float toneWidth = clampv(mP.toneWidth, 0.0f, 1.0f);
   const float airWidth = clampv(mP.airWidth, 0.0f, 1.0f);
   const float narrow = clampv(mP.airReso, 0.0f, 1.0f);
   const float flutterDepth = clampv(mP.flutterDepth, 0.0f, 1.0f);
   const bool flutterOn = flutterDepth > 0.0f;
   const bool flutterLevel = flutterOn && mP.flutterTarget != kFlutterTargetFilter;
   const bool flutterFilter = flutterOn && mP.flutterTarget != kFlutterTargetLevel;

   uint32_t i = 0;
   while (i < numSamples) {
      const uint32_t chunk = std::min<uint32_t>(kCtrlBlock, numSamples - i);
      const float t0 = static_cast<float>(v.pos);
      const float t1 = static_cast<float>(std::min(1.0, v.pos + v.inc * chunk));

      // The gesture curve, computed at the ends of the block and interpolated
      // across it: two pow() per block rather than two per sample.
      const float g0 = gestureLevel(t0, peak);
      const float g1 = gestureLevel(t1, peak);
      const float gStep = (g1 - g0) / static_cast<float>(chunk);
      float g = g0;

      // The flutter's rate and its smoothing corner, both once per block. The
      // phase still advances per sample, which is what keeps a 40 Hz tremolo
      // smooth; only the things that cost a transcendental are at block rate.
      float rate = 0.0f;
      if (flutterOn) {
         rate = flutterRate(v, t0);
         v.flutterLp.setCutoff(rate * lerpf(18.0f, 1.1f, clampv(mP.flutterSmooth, 0.0f, 1.0f)),
                               sr);
      }

      // Where the sweep has got to, and the resulting cutoff. The filter is
      // retuned once per block: a tan() per sample per channel is not worth the
      // 1.5 kHz of extra modulation bandwidth it would buy.
      const float warp = sweepWarp(t0);
      const float cutoffMod =
         flutterFilter ? std::exp2((v.flutterLp.value() - 0.5f) * flutterDepth * 2.0f) : 1.0f;
      const float cutoff = clampv(mP.airCutoffHz * v.vCutoff * velTone * cutoffMod *
                                     std::exp2((mP.airSweepOct + v.vSweep) * warp),
                                  20.0f, nyquist);
      v.airL.setCutoff(cutoff, narrow, sr);
      v.airR.setCutoff(cutoff, narrow, sr);
      const float bandGain = v.airL.k() * 2.0f;

      // Where the gesture is in the stereo field, equal power.
      const float pan =
         clampv(mP.panStart + (mP.panEnd - mP.panStart) * t0 + v.vPanOff, -1.0f, 1.0f);
      const float panAngle = (pan + 1.0f) * 0.25f * kPi;
      const float panL = std::cos(panAngle);
      const float panR = std::sin(panAngle);

      // Tone oscillator increments and placements.
      const float toneFreq =
         clampv(toneBase * std::exp2(mP.toneGlideSt * (1.0f / 12.0f) * warp), 5.0f, nyquist);
      float toneInc[kToneOscs];
      float tonePanL[kToneOscs];
      float tonePanR[kToneOscs];
      for (int k = 0; k < kToneOscs; ++k) {
         toneInc[k] = clampv(toneFreq * (1.0f + v.toneSpread[k] * detune), 1.0f, nyquist) * dt;
         const float a = (v.toneSpread[k] * toneWidth + 1.0f) * 0.25f * kPi;
         tonePanL[k] = std::cos(a);
         tonePanR[k] = std::sin(a);
      }

      // The two struck layers fire once, at their own point in the span.
      if (!v.subFired && t0 >= peak) {
         v.subFired = true;
         v.subAmp = 1.0f;
         v.subDrop = 0.0f;
         v.subClickEnv = 1.0f;
         v.subPhase = 0.0f;
      }
      if (!v.hitFired && t0 >= clampv(mP.hitTime, 0.0f, 1.0f)) {
         v.hitFired = true;
         v.hitAmp = 1.0f;
         const float tone = clampv(mP.hitToneHz * v.vHitTone * velTone, 30.0f, nyquist);
         for (int k = 0; k < kHitModes; ++k)
            v.hitMode[k].setCutoff(clampv(tone * kHitModeRatio[k], 30.0f, nyquist), 0.93f, sr);
         v.hitBand.setCutoff(tone, 0.25f, sr);
         v.hitBodyFilter.setCutoff(clampv(tone * 0.18f, 25.0f, nyquist), 0.4f, sr);
      }

      const bool airOn = mP.airGain > 0.0f;
      const bool toneOn = mP.toneGain > 0.0f;
      const bool subOn = mP.subGain > 0.0f;
      const bool hitOn = mP.hitGain > 0.0f;

      for (uint32_t s = 0; s < chunk; ++s) {
         const float envLevel = v.env.tick();
         const float fv = flutterOn ? flutterValue(v, rate, dt) : 1.0f;
         const float flutter = flutterLevel ? 1.0f - flutterDepth * (1.0f - fv) : 1.0f;
         const float shape = g * envLevel * voiceLevel;

         // ----------------------------------------------------------- air
         float airOutL = 0.0f;
         float airOutR = 0.0f;
         if (airOn) {
            const float nl = v.noiseL.tick(mP.noise);
            const float nr = v.noiseR.tick(mP.noise);
            // Width as decorrelation: at 0 both channels carry the same noise,
            // at 1 they are independent, which is as wide as noise gets.
            const float rr = nl + (nr - nl) * airWidth;
            float lp, bp, hp;
            v.airL.tick(nl, lp, bp, hp);
            // Resonance narrows the filter as well as sharpening it: at 0 this
            // is a plain lowpass and the whoosh is a wall of air closing down,
            // at 1 it is a bandpass and the whoosh has a pitch to follow.
            airOutL = lerpf(lp, bp * bandGain, narrow);
            v.airR.tick(rr, lp, bp, hp);
            airOutR = lerpf(lp, bp * bandGain, narrow);
            (void)hp;
            const float ag = mP.airGain * shape * flutter;
            airOutL = v.tiltL.tick(airOutL) * ag;
            airOutR = v.tiltR.tick(airOutR) * ag;
         }

         // ---------------------------------------------------------- tone
         float toneL = 0.0f;
         float toneR = 0.0f;
         if (toneOn) {
            for (int k = 0; k < kToneOscs; ++k) {
               v.tonePhase[k] += toneInc[k];
               if (v.tonePhase[k] >= 1.0f)
                  v.tonePhase[k] -= std::floor(v.tonePhase[k]);
               const float w = waveValue(mP.wave, v.tonePhase[k], toneInc[k]);
               toneL += w * tonePanL[k];
               toneR += w * tonePanR[k];
            }
            const float tg = mP.toneGain * shape * flutter * (1.0f / kToneOscs);
            toneL *= tg;
            toneR *= tg;
         }

         // ----------------------------------------------------------- sub
         float sub = 0.0f;
         if (subOn && v.subAmp > 1.0e-6f) {
            // The drop is exponential in time rather than in the gesture, and
            // it has to be: a boom's pitch falls as it decays, not as the whole
            // gesture runs.
            v.subDrop += subDropCoef * (1.0f - v.subDrop);
            v.subClickEnv *= subClickCoef;
            // The click is a very short upward snap at the moment of impact,
            // which is what makes the ear hear something landing rather than a
            // note fading in.
            const float f = clampv(subBase * std::exp2(subDropRatio * v.subDrop) *
                                      (1.0f + subClickAmount * v.subClickEnv),
                                   8.0f, nyquist);
            v.subPhase += f * dt;
            if (v.subPhase >= 1.0f)
               v.subPhase -= std::floor(v.subPhase);
            sub = softClip(sin2piFast(v.subPhase) * subDriveGain) * subDriveComp;
            sub *= mP.subGain * v.subAmp * envLevel * voiceLevel;
            v.subAmp *= subCoef;
         }

         // ----------------------------------------------------------- hit
         float hit = 0.0f;
         if (hitOn && v.hitAmp > 1.0e-6f) {
            const float exc = v.hitRng.white() * v.hitAmp;
            float metal = 0.0f;
            for (int k = 0; k < kHitModes; ++k)
               metal += v.hitMode[k].bandpassNormalised(exc);
            metal *= 1.0f / kHitModes;
            const float debris = v.hitBand.bandpassNormalised(exc);
            const float body = v.hitBodyFilter.lowpass(exc);
            hit = lerpf(metal, debris, clampv(mP.hitNoise, 0.0f, 1.0f)) +
                  body * clampv(mP.hitBody, 0.0f, 1.0f) * 2.0f;
            hit *= mP.hitGain * envLevel * voiceLevel;
            v.hitAmp *= hitCoef;
         }

         const float struck = sub + hit;
         outL[i + s] += (airOutL + toneL + struck) * panL;
         outR[i + s] += (airOutR + toneR + struck) * panR;

         g += gStep;
      }

      v.pos = std::min(1.0, v.pos + v.inc * chunk);
      i += chunk;
   }

   if (v.finished())
      v.active = false;
}

// ------------------------------------------------------------------ output

void GestureEngine::processOutputChain(float *outL, float *outR, uint32_t numSamples) {
   const float drive = 1.0f + 6.0f * clampv(mP.drive, 0.0f, 1.0f);
   const float driveComp = 1.0f / (1.0f + 2.2f * clampv(mP.drive, 0.0f, 1.0f));
   const float wet = clampv(mP.spaceAmount, 0.0f, 1.0f);
   const float spaceWidth = clampv(mP.spaceWidth, 0.0f, 1.0f);

   for (uint32_t i = 0; i < numSamples; ++i) {
      float l = outL[i];
      float r = outR[i];

      l = mHpL.tick(l);
      r = mHpR.tick(r);
      l = mLpL.tick(l);
      r = mLpR.tick(r);

      l = mEqHighL.tick(mEqMidL.tick(mEqLowL.tick(l)));
      r = mEqHighR.tick(mEqMidR.tick(mEqLowR.tick(r)));

      l = mProfileShelfL.tick(mProfileTiltL.tick(l));
      r = mProfileShelfR.tick(mProfileTiltR.tick(r));

      l = softClip(l * drive) * driveComp;
      r = softClip(r * drive) * driveComp;

      if (wet > 0.0f) {
         float wl = 0.0f, wr = 0.0f;
         mSpace.tick(l, r, wl, wr);
         // The tail's own width, collapsed towards the middle rather than
         // towards the dry signal, so narrowing it does not also make it quieter.
         const float mid = 0.5f * (wl + wr);
         wl = mid + (wl - mid) * spaceWidth;
         wr = mid + (wr - mid) * spaceWidth;
         l += wl * wet;
         r += wr * wet;
      }

      outL[i] = softClip(l * mP.gain);
      outR[i] = softClip(r * mP.gain);
   }
}

void GestureEngine::process(float *outL, float *outR, uint32_t numSamples) {
   if (numSamples == 0)
      return;

   const uint32_t limit =
      static_cast<uint32_t>(clampv(mP.maxVoices, 1, static_cast<int>(kMaxVoices)));
   bool anyVoice = false;
   for (uint32_t i = 0; i < kMaxVoices; ++i) {
      Voice &v = mVoices[i];
      if (!v.active)
         continue;
      if (i >= limit) {
         // Max Voices was turned down while this one was sounding. Let it
         // release rather than cutting it off.
         v.env.gateOff();
      }
      processVoice(v, outL, outR, numSamples);
      anyVoice = true;
   }

   processOutputChain(outL, outR, numSamples);

   const float seconds = static_cast<float>(numSamples) / static_cast<float>(mSampleRate);
   if (anyVoice)
      mSilenceCounter = 0.0f;
   else
      mSilenceCounter += seconds;
}

// ------------------------------------------------------------------ status

bool GestureEngine::isSilent() const {
   // The tank keeps ringing after the last voice has gone, so silence is only
   // claimed once its own decay has elapsed on top of that.
   return activeVoiceCount() == 0 &&
          mSilenceCounter > mSpace.decaySeconds() * (mP.spaceAmount > 0.0f ? 1.5f : 0.0f) + 0.05f;
}

uint32_t GestureEngine::activeVoiceCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices)
      if (v.active)
         ++n;
   return n;
}

float GestureEngine::tailSeconds() const {
   const float gesture = mP.spanSec * 1.35f; // the widest a varied span can get
   const float struck = std::max(mP.subDecaySec, mP.hitDecaySec) * 3.0f;
   const float space = mP.spaceAmount > 0.0f ? mSpace.decaySeconds() : 0.0f;
   return gesture + std::max(struck, mP.releaseSec) + space;
}

} // namespace whooshpact
