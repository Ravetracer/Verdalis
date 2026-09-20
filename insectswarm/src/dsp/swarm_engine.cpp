#include "swarm_engine.h"

#include "verdalis/dsp/fastmath.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

namespace insectswarm {

namespace {

std::atomic<uint32_t> gInstanceCounter{0};

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Q to the Svf's 0..1 resonance. Svf::setCutoff maps resonance onto k = 1/Q
// over 2..0.02, so this is that mapping read backwards.
inline float resonanceFor(float q) {
   const float k = 1.0f / clampf(q, 0.5f, 60.0f);
   return clampf((2.0f - k) / 1.98f, 0.0f, 1.0f);
}

// ------------------------------------------------------------- the excitation
//
// One half-stroke's pressure pulse: (1 - u^2)^2 over its own half-width, zero
// outside it. Chosen over a raised cosine because its first derivative
// vanishes at both ends -- so narrowing it brightens the buzz rather than
// adding a click -- and because its first two moments are closed forms, which
// is what makes the DC removal and the level normalisation below exact instead
// of measured at runtime.
//
//   mean over one cycle   = hw * 16/315 * 20  = hw * 16/15
//   mean square           = hw * 256/315
// RngLite::white() is uniform on [-1, 1], so its standard deviation is
// 1/sqrt(3). The random-walk normalisations below are derived for a
// unit-variance source, so they carry this factor and the knobs mean what they
// say: Wander is cents and Roam is decibels, both as standard deviations.
constexpr float kWhiteNorm = 1.7320508f;

constexpr float kPulseMeanK = 16.0f / 15.0f;
constexpr float kPulseMsK = 256.0f / 315.0f;

inline float strokePulse(float d, float invHalfWidth) {
   const float u = d * invHalfWidth;
   if (u >= 1.0f)
      return 0.0f;
   const float w = 1.0f - u * u;
   return w * w;
}

// The excitation's variance for a given half-width and stroke asymmetry, with
// its DC already removed. Used to set the turbulence level so that Rasp is a
// harmonic-to-noise ratio in dB rather than a taste.
inline float excitationVariance(float hw, float strokeAmp) {
   const float m = hw * kPulseMeanK;
   const float var = hw * kPulseMsK - m * m;
   return std::max(var, 1.0e-9f) * (1.0f + strokeAmp * strokeAmp);
}

// An equal-power pan from a signed position in -1..1.
inline void panFor(float pos, float &l, float &r) {
   const float t = clampf(pos, -1.0f, 1.0f) * 0.25f + 0.25f; // 0..0.5 of a turn
   l = std::cos(3.14159265f * t);
   r = std::sin(3.14159265f * t);
}

} // namespace

void SwarmEngine::prepare(double sampleRate, uint32_t /*maxBlockSize*/) {
   mSampleRate = sampleRate;
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   mRng.reseed(static_cast<uint32_t>(now) ^
               (0x9E3779B9u * (gInstanceCounter.fetch_add(1) + 1)));
   mAppliedSeed = 0;

   // Nothing below 80 Hz is generated -- the lowest wingbeat in the library is
   // a hornet at 85.7 Hz -- so the tank's own highpass can sit at 40 and take
   // nothing off the bottom of anything real.
   mSpace.prepare(static_cast<float>(sampleRate), 40.0f);
   reset();
   updateFilters();
}

void SwarmEngine::reset() {
   const float sr = static_cast<float>(mSampleRate);
   for (uint32_t vi = 0; vi < kMaxVoices; ++vi) {
      Voice &v = mVoices[vi];
      v.active = false;
      v.env.reset();
      v.numFlyers = 0;
      v.numStrid = 0;
      v.flybyTimer = 0.0;
      v.modCounter = 0;
      for (auto &f : v.flyers)
         f.active = false;
      for (auto &f : v.passes)
         f.active = false;
      for (auto &s : v.strid)
         s.active = false;
      v.bedL.reset();
      v.bedR.reset();
      v.bedTiltL.reset();
      v.bedTiltR.reset();
      v.rngBed.seed(0x51ED0001u + vi * 2654435761u);
      v.rng.seed(0x51ED1001u + vi * 40503u);
   }
   mSpace.clear();
   mOutFilterL.reset();
   mOutFilterR.reset();
   mOutHpL.reset();
   mOutHpR.reset();
   mAirLpL.reset();
   mAirLpR.reset();
   mDistanceTiltL.reset();
   mDistanceTiltR.reset();
   mOutHpL.setCutoff(80.0f, sr);
   mOutHpR.setCutoff(80.0f, sr);
   mSilenceCounter = 0.0f;
   mFlybyCounter = 0;
}

void SwarmEngine::setParams(const EngineParams &p) {
   mP = p;
   if (mP.seed != mAppliedSeed) {
      mAppliedSeed = mP.seed;
      if (mP.seed > 0)
         mRng.reseed(static_cast<uint32_t>(mP.seed) * 2654435761u + 12345u);
   }
   updateFilters();
}

// Everything that depends on a parameter but not on the sample being computed.
// The per-sample path below reads the results as plain numbers, so no tan() or
// pow() happens inside the audio loop.
void SwarmEngine::updateFilters() {
   const float sr = static_cast<float>(mSampleRate);
   const SpeciesRow &sp = kSpecies[clampi(mP.species, 0, kNumSpecies - 1)];

   // ------------------------------------------------------- the species row
   mRateHz = clampf(sp.rateHz * semitonesToRatio(mP.rateShiftSemis), 20.0f, 0.2f * sr);

   // The fitted shelf. tiltDb is the fit; Tilt moves it +-18 dB on top, which
   // is roughly the whole width of the fitted range across the seven species.
   const float tiltDb = sp.tiltDb + 18.0f * clampf(mP.tilt, -1.0f, 1.0f);
   mShelfLow = std::pow(10.0f, tiltDb * 0.025f);
   mShelfHigh = std::pow(10.0f, -tiltDb * 0.025f);

   mFormantHz =
      clampf(sp.formantHz * semitonesToRatio(mP.formantSemis), 40.0f, 0.45f * sr);
   mFormantRes = clampf(sp.formantRes + 0.4f * clampf(mP.resonance, -1.0f, 1.0f), 0.0f, 0.97f);
   mFormantMix = clampf(sp.formantMix, 0.0f, 1.0f);

   // ------------------------------------------------------- the excitation
   //
   // The pulse is clamped to at least 2.5 samples wide whatever Bite asks for.
   // Narrower than that and what it adds is aliasing rather than brightness:
   // a mosquito at 464 Hz with the knob at the top would otherwise be asking
   // for a two-sample edge.
   const float wide = 0.45f * std::pow(0.02f, clampf(mP.bite, 0.0f, 1.0f));
   const float floorHw = 1.25f * mRateHz / sr;
   mHalfWidth = clampf(std::max(wide, floorHw), 0.004f, 0.45f);
   mPulseMean = mHalfWidth * kPulseMeanK;
   mNoiseCutHz = clampf(0.36f * mRateHz / mHalfWidth, 80.0f, 0.45f * sr);

   // Rasp is a harmonic-to-noise ratio in dB, and it has to be one *after* the
   // shaper rather than before it. The resonator is narrow, so it keeps the
   // harmonic that sits under it and only a slice of the turbulence around it:
   // setting the two in the excitation left the rendered ratio between 1.7 dB
   // under the measurement and 8.2 dB over it, species by species. So the two
   // paths are measured separately below and the turbulence is solved for.
   const float hnrTarget = sp.hnrDb - 12.0f * (clampf(mP.rasp, 0.0f, 1.0f) - 0.5f);


   // ------------------------------------------------------- the output chain
   mOutHpL.setCutoff(mP.highpassHz, sr);
   mOutHpR.setCutoff(mP.highpassHz, sr);
   mOutFilterActive = mP.filterCutoffHz < 19000.0f || mP.filterType != 0;
   const float cut = clampf(mP.filterCutoffHz, 20.0f, 0.45f * sr);
   mOutFilterL.setCutoff(cut, mP.filterReso, sr);
   mOutFilterR.setCutoff(cut, mP.filterReso, sr);

   const float dist = clampf(mP.distance, 0.0f, 1.0f);
   mAirActive = dist > 0.001f;
   if (mAirActive) {
      // Dry air takes the top off sooner than humid air does.
      const float top = 20000.0f * std::pow(0.08f, dist * (0.35f + 0.65f * mP.air));
      mAirLpL.setCutoff(clampf(top, 700.0f, 20000.0f), sr);
      mAirLpR.setCutoff(clampf(top, 700.0f, 20000.0f), sr);
   }
   mDistanceTiltL.setCutoff(clampf(4000.0f - 3200.0f * dist, 400.0f, 4000.0f), sr);
   mDistanceTiltR.setCutoff(clampf(4000.0f - 3200.0f * dist, 400.0f, 4000.0f), sr);
   mDistanceLevel = 1.0f - 0.45f * dist;

   mSpace.setSize(mP.spaceSize);
   mSpace.setDamping(mP.spaceDamping);
   // A field has almost nothing close enough to reflect off. The same lesson
   // as ShoreBreak's and CrackleBlaze's: discrete early reflections outdoors
   // are what make a reverb sound like a bathroom.
   mSpace.setEnclosure(0.25f);

   // The bed's band. Set here rather than per note so that moving Bed Tone is
   // audible on a note that is already down.
   const float bedFc = clampf(mP.bedToneHz, 60.0f, 0.45f * sr);
   for (auto &v : mVoices) {
      v.bedL.setCutoff(bedFc, 0.15f, sr);
      v.bedR.setCutoff(bedFc, 0.15f, sr);
      v.bedTiltL.setCutoff(clampf(bedFc * 0.5f, 40.0f, 0.4f * sr), sr);
      v.bedTiltR.setCutoff(clampf(bedFc * 0.5f, 40.0f, 0.4f * sr), sr);
   }

   // Whether anything an individual captured at spawn time has moved. Held
   // notes re-derive from these only when one of them has; comparing eight
   // floats in the parameter path is cheaper than redesigning 64 resonators in
   // the audio one.
   const float sig[] = {mRateHz,     mShelfLow,  mShelfHigh,      mFormantHz,
                        mFormantRes, mHalfWidth, hnrTarget,       mP.stroke,
                        mP.flutter,  mP.width,   mP.wanderRateHz, mP.spreadCents,
                        static_cast<float>(mP.count),            mP.roamRateHz};
   static_assert(sizeof(sig) / sizeof(sig[0]) == 14, "keep mLastSig in step");
   bool moved = false;
   for (int i = 0; i < 14; ++i) {
      if (mLastSig[i] != sig[i]) {
         mLastSig[i] = sig[i];
         moved = true;
      }
   }
   // The stridulation layer's own normalisation, on its own signature: it
   // shares nothing with the wing but the output chain.
   const float ssig[] = {mP.carrierHz, mP.carrierQ, mP.pulseRateHz,
                         mP.duty + 1000.0f * mP.echemeRateHz + 1.0e6f * mP.stridSpread +
                            1.0e9f * static_cast<float>(mP.chorus)};
   bool stridMoved = false;
   for (int i = 0; i < 4; ++i) {
      if (mLastStridSig[i] != ssig[i]) {
         mLastStridSig[i] = ssig[i];
         stridMoved = true;
      }
   }
   if (stridMoved || mLastStridSig[0] != mP.carrierHz) {
      mStridNorm = 1.0f / std::max(calibrateStrid(), 1.0e-6f);
      ++mStridEpoch;
   }

   if (moved) {
      ++mEpoch;
      float rmsPulse = 1.0f, rmsNoise = 1.0f;
      calibrate(rmsPulse, rmsNoise);
      // Solve the turbulence amplitude that puts the two paths `hnrTarget` dB
      // apart at the output, and then normalise on what the two together come
      // out at.
      mNoiseAmp = rmsPulse * std::pow(10.0f, -hnrTarget * 0.05f) / std::max(rmsNoise, 1.0e-6f);
      const float total = std::sqrt(rmsPulse * rmsPulse + mNoiseAmp * rmsNoise * mNoiseAmp * rmsNoise);
      mVoiceNorm = 1.0f / std::max(total, 1.0e-5f);
   }

   // Individuals are uncorrelated, so their powers add. Their amplitudes are
   // drawn uniformly over 0.35..1 to give the swarm depth, whose mean square is
   // 0.4908, so normalising by sqrt(N * 0.4908) leaves Count a density control
   // and not a level one.
   const int n = clampi(mP.count, 1, Voice::kMaxFlyers);
   mSwarmNorm = mVoiceNorm / std::sqrt(static_cast<float>(n) * 0.4908f);
}

// One individual, run open-loop for long enough to settle and then measured --
// twice, because the two halves of its excitation have to be weighed against
// each other and the shaper does not treat them alike.
//
// The alternative was to integrate the shaper's power response against the
// pulse train's line spectrum analytically, which is a page of algebra that
// would have to be redone the moment either the excitation or the shaper
// changed. This runs the real code instead: the same pulse, the same shelf,
// the same resonator, the same flutter. It costs about a quarter of a million
// operations and only runs when a parameter that reaches an individual has
// actually moved.
void SwarmEngine::calibrate(float &rmsPulse, float &rmsNoiseUnit) const {
   const float sr = static_cast<float>(mSampleRate);
   constexpr int kWarm = 1024; // let the resonator reach its steady state
   constexpr int kMeasure = 4096;

   const float hw = clampf(std::max(mHalfWidth, 1.25f * mRateHz / sr), 0.004f, 0.45f);
   const float mean = hw * kPulseMeanK;
   const float strokeAmp = clampf(mP.stroke, 0.0f, 1.0f);
   const float flutter = clampf(mP.flutter, 0.0f, 1.0f);
   const float inc = mRateHz / sr;
   const float invHw = 1.0f / hw;

   // Two independent copies of the shaper: one fed the stroke pulses, one fed
   // unit-amplitude turbulence.
   OnePoleLp shelfP, shelfN;
   Svf formantP, formantN;
   OnePoleHp dcP, dcN;
   shelfP.setCutoff(300.0f, sr);
   shelfN.setCutoff(300.0f, sr);
   formantP.setCutoff(mFormantHz, mFormantRes, sr);
   formantN.setCutoff(mFormantHz, mFormantRes, sr);
   dcP.setCutoff(30.0f, sr);
   dcN.setCutoff(30.0f, sr);

   Lp2 noiseLp;
   noiseLp.setCutoff(mNoiseCutHz, sr);

   RngLite rng;
   rng.seed(0xC0FFEEu);
   float phase = 0.0f;
   double accP = 0.0, accN = 0.0;
   for (int i = 0; i < kWarm + kMeasure; ++i) {
      phase += inc;
      if (phase >= 1.0f)
         phase -= 1.0f;
      const float dA = phase < 0.5f ? phase : 1.0f - phase;
      const float dB = std::fabs(phase - 0.5f);
      const float ex = (strokePulse(dA, invHw) - mean) + strokeAmp * (strokePulse(dB, invHw) - mean);
      const float nz = noiseLp.tick(rng.white());
      const float am = 1.0f - flutter * 0.5f * (1.0f + sin2piFast(phase));

      const float lpP = shelfP.tick(ex);
      float yp = lpP * mShelfLow + (ex - lpP) * mShelfHigh;
      yp = yp * (1.0f - mFormantMix) + formantP.bandpassNormalised(yp) * mFormantMix;
      yp = dcP.tick(yp) * am;

      const float lpN = shelfN.tick(nz);
      float yn = lpN * mShelfLow + (nz - lpN) * mShelfHigh;
      yn = yn * (1.0f - mFormantMix) + formantN.bandpassNormalised(yn) * mFormantMix;
      yn = dcN.tick(yn) * am;

      if (i >= kWarm) {
         accP += static_cast<double>(yp) * yp;
         accN += static_cast<double>(yn) * yn;
      }
   }
   rmsPulse = static_cast<float>(std::sqrt(accP / kMeasure));
   rmsNoiseUnit = static_cast<float>(std::sqrt(accN / kMeasure));
}

// One caller, run with its gate held open, and measured. Same reasoning as
// calibrate(): the closed form exists but would have to be rederived every time
// the excitation or the resonator changed, and this runs the code that actually
// makes the sound.
// The level correction that goes with `Scrape`: a noise burst lasting a
// fraction of the pulse period carries far more energy into the resonator than
// a single sample does, and Stridulate Level must keep meaning what it meant.
// It is mirrored in calibrateStrid() below -- if that ever disagrees with the
// audio path, the layer's level drifts silently.
constexpr float kStrokeNorm = 0.20f;

float SwarmEngine::calibrateStrid() const {
   const float sr = static_cast<float>(mSampleRate);
   constexpr int kWarm = 2048;
   constexpr int kMeasure = 8192;
   Svf body;
   body.setCutoff(clampf(mP.carrierHz, 200.0f, 0.45f * sr),
                  resonanceFor(clampf(mP.carrierQ, 1.0f, 200.0f)), sr);
   RngLite rng;
   rng.seed(0x5C1DAAu);
   const float inc = clampf(mP.pulseRateHz, 1.0f, 2000.0f) / sr;
   float phase = 0.0f;
   float peak = 0.0f;
   double acc = 0.0;
   for (int i = 0; i < kWarm + kMeasure; ++i) {
      phase += inc;
      float drive = 0.0f;
      bool struck = false;
      if (phase >= 1.0f) {
         phase -= 1.0f;
         struck = true;
      }
      // A unit stroke, where the audio path draws its loudness uniformly over
      // 0.5..1. So the normalisation is referenced to the loudest stroke the
      // layer can produce and a typical one lands under it, which is the
      // headroom a train with a high crest factor needs. The shape has to be
      // the audio path's shape: a burst of noise over the same fraction of the
      // period, or this measures a different layer than the one that plays.
      if (struck || phase < clampf(mP.scrape, 0.0f, 1.0f))
         drive = kStrokeNorm * rng.white();
      float lp, bp, hp;
      body.tick(drive, lp, bp, hp);
      if (i >= kWarm) {
         peak = std::max(peak, std::fabs(bp));
         acc += static_cast<double>(bp) * bp;
      }
   }
   const float rms = static_cast<float>(std::sqrt(acc / kMeasure));
   // The RMS, like every other layer, with the peak only as a floor.
   //
   // Two other schemes were tried and both were wrong in the same way -- they
   // made Stridulate Level mean something different from Swarm Level. Levelling
   // on the peak put a full cricket chorus 20 dB under a swarm at the same
   // setting, with no headroom left on the knob to recover it; capping the crest
   // factor made a dense cicada quieter the more headroom it was given, which is
   // backwards.
   //
   // So it is the RMS, and the crest factor is simply a thing to know about the
   // layer: a cricket is 36 clicks a second each ringing for 1.8 ms, so 6 per
   // cent of it is sounding and it peaks some 18 dB over its own average. At a
   // level a swarm would use that is comfortable; pushed to the top of the knob
   // it will reach the output stage's soft clipper, which is true of any sparse
   // percussive layer and is what the knob's top is for.
   //
   // The peak floor only catches the degenerate settings -- a click rate so low,
   // or a Q so high, that almost nothing is sounding -- where a pure RMS
   // normalisation would ask for an unbounded gain.
   return std::max(rms, peak * 0.02f);
}

// Brings a note's swarm to the size the parameters now ask for, sharing the
// budget with whatever is already flying on another note.
void SwarmEngine::refreshSwarmSize(Voice &v, float fire) {
   const int want = clampi(static_cast<int>(std::lround(
                              clampi(mP.count, 1, Voice::kMaxFlyers) * (1.0f + 0.6f * fire))),
                           1, Voice::kMaxFlyers);
   int used = 0;
   for (const auto &o : mVoices)
      if (&o != &v && o.active)
         used += o.numFlyers;
   const int n = clampi(std::min(want, individualBudget() - used), 1, Voice::kMaxFlyers);
   for (int i = v.numFlyers; i < n; ++i) {
      Flyer &f = v.flyers[i];
      f.rng.seed(v.tag + static_cast<uint32_t>(i) * 2246822519u);
      configureFlyer(f, v, fire, false);
   }
   for (int i = n; i < Voice::kMaxFlyers; ++i)
      v.flyers[i].active = false;
   v.numFlyers = n;
}

// Re-derives the whole chorus from the current parameters, keeping each
// caller's own place in it and its own phase.
// How much of `Wander` reaches a caller's chirp clock. A wingbeat's wander is a
// pitch and is quoted in cents; a chirp clock's is a tempo. The factor is swept
// against the references' fractional chirp-peak width -- see refreshStrid().
constexpr float kStridClockFactor = 8.0f;

void SwarmEngine::refreshStrid(Voice &v) const {
   const float sr = static_cast<float>(mSampleRate);
   const float modSr = sr / static_cast<float>(kModInterval);
   const float carrier = clampf(mP.carrierHz * semitonesToRatio(static_cast<float>(v.key - 60)),
                                200.0f, 0.45f * sr);
   const float res = resonanceFor(clampf(mP.carrierQ, 1.0f, 200.0f));
   const float sc = clampf(mP.stridSpread, 0.0f, 1.0f);
   v.numStrid = clampi(mP.chorus, 1, Voice::kMaxStrid);
   for (int i = 0; i < Voice::kMaxStrid; ++i) {
      Stridulator &s = v.strid[i];
      if (i >= v.numStrid) {
         s.active = false;
         continue;
      }
      // The carrier scatter is small, and the library says how small. A chorus
      // recording measures the *composite* peak, and the fourteen cicada
      // references put its Q at 13.2 and the nine cricket ones at 25.8 -- so
      // whatever the callers are doing, they are not spread wider than about a
      // thirteenth of their carrier or that peak would have measured broader
      // than it does. The cricket is the binding case: at Q 25.8 its resonance
      // is only 175 Hz wide, so a chorus spread +-10 per cent measured Q 12.2
      // and +-1.5 per cent still only reached 12.3. At +-1.5 per cent here --
      // the top of the knob -- the resonator stays what sets the width.
      //
      // Which is also the biology. A species' carrier is its anatomy and every
      // member of it shares one; its clock is not, and no two of them keep
      // time. So the rhythms scatter several times as far -- and 0.2.0 said
      // exactly that in this comment while scattering them by a tenth, which is
      // the defect this version fixes.
      //
      // **How far, measured.** A per-file median cannot answer it: every
      // recording gives one number, and the question is how much the callers
      // *inside* one chorus differ. So it is read off the width of the chirp
      // peak in each recording's own modulation spectrum -- a chorus of callers
      // that all keep the same time has a sharp peak, a real field has a hump:
      //
      //   fractional width of the chirp peak, -6 dB
      //     nine cricket references     0.07 .. 0.86, median 0.31
      //     InsectSwarm 0.2.0 rendered  0.10
      //
      // 0.10 is the figure a *single caller* produces, which is what twelve
      // callers within +-5 % of one rate amount to. They then drift in and out
      // of phase together and the chorus throbs at its own chirp rate and drops
      // into near-silence between: the render's quietest tenth of frames sat at
      // 0.12 of its median against the references' 0.37.
      //
      // The scatter below is set where a rendered chorus measures the library's
      // 0.31. The carrier stays where it was, for the reason above it.
      s.carrierHz = clampf(carrier * (1.0f + 0.03f * sc * s.dCarrier), 200.0f, 0.45f * sr);
      s.body.setCutoff(s.carrierHz, res, sr);
      s.pulseInc = clampf(mP.pulseRateHz * (1.0f + 0.30f * sc * s.dPulse), 1.0f, 2000.0f) / sr;
      s.echemeBase = clampf(mP.echemeRateHz * (1.0f + 0.45f * sc * s.dEcheme), 0.05f, 80.0f) / sr;
      s.echemeInc = s.echemeBase * s.clockMul;
      // And the clock drifts. **This is the change that mattered most.**
      //
      // Spreading the callers' rates is not enough on its own, because twelve
      // *exactly* periodic chirp trains are twelve razor-sharp lines in the
      // modulation spectrum -- a picket fence, not a hump -- and widening the
      // spacing between the pickets leaves them pickets. Measured on the
      // references, a chorus is a hump because a single caller is already one:
      //
      //   fractional width of the chirp peak, -6 dB, one caller in the open
      //     lonecricketseptember2013      0.86
      //     night-ambience                0.61
      //     city-night-crickets           0.31
      //     InsectSwarm 0.2.0, any Scatter  0.06 .. 0.10
      //
      // A lone cricket's own chirp clock wanders that far. So each caller here
      // gets a filtered random walk on its rate, at the corner Wander Rate
      // already sets for the wingbeat layer.
      s.clockLp.setCutoff(clampf(mP.wanderRateHz, 0.05f, 40.0f), modSr);
      // Duty scatters too. The nine cricket references measure 0.09 to 0.65 of
      // the cycle sounding, and a chorus in which every caller holds its chirp
      // for exactly as long as its neighbours is the same mistake one line up.
      s.duty = clampf(mP.duty * (1.0f + 0.35f * sc * s.dDuty), 0.02f, 0.98f);
      s.active = true;
   }
   v.stridEpoch = mStridEpoch;
}

// Re-derives everything an individual took from the parameters at spawn time,
// leaving its phase, its filter state and its own place in the swarm alone.
void SwarmEngine::refreshFlyer(Flyer &f, const Voice &v, float fire) {
   const float sr = static_cast<float>(mSampleRate);
   const float key = static_cast<float>(v.key - 60);
   // Spread is quoted as the interquartile range the library measures, so it is
   // divided by 1.349 to become the standard deviation of the draw that
   // produces it.
   const float cents = f.dRate * clampf(mP.spreadCents, 0.0f, 400.0f) / 1.349f;
   f.rateHz = clampf(mRateHz * semitonesToRatio(key + cents * 0.01f) * (1.0f + 0.25f * fire),
                     20.0f, 0.2f * sr);
   if (!f.pass) {
      // A swarm member hangs in the field at a fixed place and a fixed
      // distance. The library's stereo references put a single insect's
      // side-to-mid ratio at a median of -7.8 dB, so even one is not a point.
      panFor(f.dPan * clampf(mP.width, 0.0f, 1.0f), f.panL, f.panR);
      // Uniform in level, so a swarm has depth rather than being one plane of
      // insects. Its mean square is 0.4908, which is what mSwarmNorm divides by.
      f.amp = 0.35f + 0.65f * f.dAmp;
   }
   f.halfWidth = clampf(std::max(mHalfWidth, 1.25f * f.rateHz / sr), 0.004f, 0.45f);
   f.pulseMean = f.halfWidth * kPulseMeanK;
   f.strokeAmp = clampf(mP.stroke, 0.0f, 1.0f);
   f.noiseAmp = mNoiseAmp;
   f.noiseLp.setCutoff(clampf(0.36f * f.rateHz / f.halfWidth, 80.0f, 0.45f * sr), sr);
   f.flutter = clampf(mP.flutter, 0.0f, 1.0f);
   f.shelfLow = mShelfLow;
   f.shelfHigh = mShelfHigh;
   f.formantMix = mFormantMix;
   f.formant.setCutoff(mFormantHz, mFormantRes, sr);
   // Both random walks are ticked once every kModInterval samples, so their
   // corners are set against that rate and not the audio one. Setting them
   // against `sr` puts the corner a factor of kModInterval too low, which is
   // what this used to do: Wander Rate read 3 Hz and delivered 0.05.
   const float modSr = sr / static_cast<float>(kModInterval);
   f.wanderLp.setCutoff(clampf(mP.wanderRateHz, 0.05f, 40.0f), modSr);
   f.roamLp.setCutoff(clampf(mP.roamRateHz, 0.02f, 40.0f), modSr);
}

int SwarmEngine::individualBudget() const {
   return clampi(mP.maxIndividuals, 8, 256);
}

// Sets up one individual: its own rate, its own place in the field and its own
// copy of the species' shaper.
void SwarmEngine::configureFlyer(Flyer &f, const Voice &v, float fire, bool pass) {
   const float sr = static_cast<float>(mSampleRate);

   // Its own place in the swarm. Drawn once, scaled by the parameters every
   // time refreshFlyer() runs.
   f.dRate = clampf(f.rng.white() + f.rng.white(), -2.0f, 2.0f) * 0.5f;
   f.dPan = f.rng.white();
   f.dAmp = f.rng.uniformPositive();
   f.rateMul = 1.0f;
   f.phase = f.rng.uniformPositive();

   f.shelf.reset();
   f.shelf.setCutoff(300.0f, sr);
   f.formant.reset();
   f.dcBlock.reset();
   f.dcBlock.setCutoff(30.0f, sr);
   f.noiseLp.reset();
   f.wanderLp.reset();
   f.roamLp.reset();
   f.roamGain = 1.0f;

   f.pass = pass;
   f.airL.reset();
   f.airR.reset();

   if (!pass) {
      f.t = 0.0f;
      f.tEnd = 0.0f;
   } else {
      // Geometry, not an envelope. `Pass Time` is the width of the level bump
      // 6 dB down from its peak, which for a 1/r law happens at r = 2*d0, so
      // v*t = sqrt(3)*d0 and the full -6 dB width is 2*sqrt(3)*d0/v.
      f.speed = clampf(mP.flybySpeed, 0.05f, 40.0f);
      f.d0 = clampf(f.speed * clampf(mP.flybyPassSec, 0.05f, 12.0f) / 3.4641f, 0.02f, 200.0f);
      // The trajectory is abandoned once the level has fallen by `Rise`.
      const float rise = std::pow(10.0f, clampf(mP.flybyRiseDb, 0.0f, 48.0f) * 0.1f);
      f.tEnd = f.d0 / f.speed * std::sqrt(std::max(rise - 1.0f, 0.01f));
      f.t = -f.tEnd;
      f.sweep = clampf(mP.flybySweep, 0.0f, 1.0f);
      f.amp = 1.0f;
      panFor(0.0f, f.panL, f.panR);
   }
   refreshFlyer(f, v, fire);
   f.active = true;
}

void SwarmEngine::spawnPass(Voice &v, float fire) {
   for (auto &f : v.passes) {
      if (f.active)
         continue;
      f.rng.seed(mRng.next() | 1u);
      configureFlyer(f, v, fire, true);
      ++mFlybyCounter;
      return;
   }
   // A pass dropped from a full slot list is one nobody would have separated
   // from the three already in flight.
}

void SwarmEngine::noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId,
                         double velocity) {
   Voice *free = nullptr;
   for (auto &v : mVoices) {
      if (!v.active) {
         free = &v;
         break;
      }
   }
   if (!free) {
      // Steal the quietest. A swarm is a bed, so the least audible one is the
      // one nobody will miss.
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
   v.velocity = clampf(static_cast<float>(velocity), 0.0f, 1.0f);

   const float sr = static_cast<float>(mSampleRate);
   v.env.setParams(mP.attackSec, mP.decaySec, mP.sustain, mP.releaseSec, sr);
   v.env.gateOn();

   // Deliberately not drawn from mRng. A note can arrive before the Seed
   // parameter has been applied -- events are handled in the order the host
   // sends them -- so anything drawn here would depend on when that happened,
   // and a fixed Seed would stop promising the same swarm. The voice's own slot
   // and key give all the variation this needs, and give it deterministically.
   const uint32_t slot = static_cast<uint32_t>(&v - mVoices);
   const uint32_t tag = slot * 2654435761u + static_cast<uint32_t>(key) * 40503u + 1u;

   // Full velocity is the measured insect and anything softer is a calmer one,
   // which is the same convention as Velocity To Level and the only one that
   // leaves the shipped defaults equal to the measurement: most hosts send
   // velocity 1, and at any other convention an untouched note would land a
   // semitone off the species row.
   // Velocity buys individuals as well as level: a calmer swarm is fewer of
   // them flying more slowly, not the same ones turned down.
   const float fire = clampf(mP.velToSwarm, 0.0f, 1.0f) * (v.velocity - 1.0f);
   v.tag = tag;
   v.numFlyers = 0;
   refreshSwarmSize(v, fire);
   for (auto &f : v.passes)
      f.active = false;

   // ------------------------------------------------------------ the chorus
   //
   // Only each caller's own place in the chorus is drawn here. Everything that
   // comes from a parameter is derived in refreshStrid(), which runs at the
   // control rate whenever one of them has moved -- including the first block
   // after this note, so a host that sends its parameters after the note-on
   // still gets them.
   for (int i = 0; i < Voice::kMaxStrid; ++i) {
      Stridulator &s2 = v.strid[i];
      s2.rng.seed(tag ^ (0x9E3779B9u * (static_cast<uint32_t>(i) + 3u)));
      s2.dCarrier = s2.rng.white();
      s2.dPulse = s2.rng.white();
      s2.dEcheme = s2.rng.white();
      s2.dDuty = s2.rng.white();
      s2.pulsePhase = s2.rng.uniformPositive();
      s2.echemePhase = s2.rng.uniformPositive();
      s2.amp = 0.5f + 0.5f * s2.rng.uniformPositive();
      panFor(s2.rng.white() * clampf(mP.width, 0.0f, 1.0f), s2.panL, s2.panR);
      s2.body.reset();
   }
   refreshStrid(v);

   v.bedL.reset();
   v.bedR.reset();
   v.bedTiltL.reset();
   v.bedTiltR.reset();
   v.rngBed.seed(tag ^ 0xB5297A4Du);
   v.rng.seed(tag ^ 0x68E31DA4u);

   // The insects are already flying when you arrive: the first pass does not
   // wait a whole interval.
   v.flybyTimer = static_cast<double>(sr) / std::max(0.01f, mP.flybyRateHz) *
                  (0.05f + 0.6f * v.rng.uniformPositive());
   v.modCounter = 0;
   v.epoch = mEpoch;
}

void SwarmEngine::noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = (noteId >= 0 && v.noteId == noteId) ||
                         (noteId < 0 && v.port == port && v.channel == channel && v.key == key);
      if (match)
         v.env.gateOff();
   }
}

void SwarmEngine::choke(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
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

void SwarmEngine::allSoundOff() {
   for (auto &v : mVoices) {
      v.env.kill();
      v.active = false;
      for (auto &f : v.flyers)
         f.active = false;
      for (auto &f : v.passes)
         f.active = false;
      for (auto &s : v.strid)
         s.active = false;
   }
}

void SwarmEngine::processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples) {
   const float sr = static_cast<float>(mSampleRate);
   const float velLevel = 1.0f - clampf(mP.velToLevel, 0.0f, 1.0f) * (1.0f - v.velocity);
   const float fire = clampf(mP.velToSwarm, 0.0f, 1.0f) * (v.velocity - 1.0f);

   const float swarmGain = mP.swarmGain * mSwarmNorm * velLevel;
   const float passGain = mP.flybyGain * mVoiceNorm * velLevel;
   const float stridGain =
      mP.stridGain * velLevel / std::sqrt(static_cast<float>(std::max(mP.chorus, 1)));
   const float scrape = clampf(mP.scrape, 0.0f, 1.0f);
   const float bedGain = mP.bedGain * velLevel;

   // The wander is a filtered random walk at the control rate; at a corner of
   // 20 Hz at the very top there is nothing above it to miss. Normalised so
   // that Wander is a spread in cents rather than an amplitude: a one-pole fed
   // white noise has variance a/(2-a) of its input's.
   const float wa = 1.0f - std::exp(-6.283185307f * clampf(mP.wanderRateHz, 0.05f, 40.0f) *
                                    static_cast<float>(kModInterval) / sr);
   const float wanderNorm = std::sqrt((2.0f - wa) / std::max(wa, 1.0e-6f)) * kWhiteNorm;
   const float wanderOct = clampf(mP.wanderCents, 0.0f, 200.0f) * (1.0f / 1200.0f) * wanderNorm;

   // Roam: the same walk again, but in decibels. An individual is not a fixed
   // distance from the listener, and at close range the inverse square law
   // turns a few centimetres into several decibels -- which is most of what
   // separates one fly from a held tone. Per individual, so a crowd's drifts
   // cancel on their own and nothing has to scale this by Count.
   //
   // exp2() of a zero-mean walk has a mean power above one, so the bias is
   // taken out on the power and Roam makes the layer restless rather than
   // louder: the rendered RMS holds to within a few tenths of a decibel from
   // 0 to 12 dB of it, and the peaks stay under the roof.
   const float ro = 1.0f - std::exp(-6.283185307f * clampf(mP.roamRateHz, 0.02f, 40.0f) *
                                    static_cast<float>(kModInterval) / sr);
   const float roamNorm = std::sqrt((2.0f - ro) / std::max(ro, 1.0e-6f)) * kWhiteNorm;
   // In octaves: the drift's standard deviation, before the normalisation that
   // gets the filter's own output back to unit variance. The bias is taken on
   // this and not on roamK -- the two differ by roamNorm, which is a factor of
   // forty at the default rate, and squaring it lands the whole layer on the
   // clamp.
   const float roamOct = clampf(mP.roamDb, 0.0f, 12.0f) * (1.0f / 6.0205999f);
   const float roamK = roamOct * roamNorm;
   const float roamBias = 0.69314718f * roamOct * roamOct;
   // A Gaussian walk has no nearest point and an insect does, so the excursion
   // is bounded at 1.6 standard deviations either way. Without it the tail of
   // the distribution decides the peak: an untruncated 8 dB of Roam put Single
   // Bee's 9.7 dB of headroom on the floor and clipped.
   const float roamLimit = 1.6f * roamOct;

   const float flybyPeriod = sr / std::max(0.005f, mP.flybyRateHz);
   const float bedTiltMix = clampf(mP.bedTilt * 0.5f + 0.5f, 0.0f, 1.0f);
   const float qNorm = mStridNorm;

   for (uint32_t i = 0; i < numSamples; ++i) {
      const float env = v.env.tick();
      if (v.env.isIdle()) {
         v.active = false;
         break;
      }

      // ------------------------------------------------- the control rate
      if ((v.modCounter++ % kModInterval) == 0) {
         if (v.stridEpoch != mStridEpoch)
            refreshStrid(v);
         if (v.epoch != mEpoch) {
            v.epoch = mEpoch;
            refreshSwarmSize(v, fire);
            for (int k = 0; k < v.numFlyers; ++k)
               refreshFlyer(v.flyers[k], v, fire);
            for (auto &f : v.passes)
               if (f.active)
                  refreshFlyer(f, v, fire);
         }
         for (int k = 0; k < v.numFlyers; ++k) {
            Flyer &f = v.flyers[k];
            f.rateMul = std::exp2(f.wanderLp.tick(f.rng.white()) * wanderOct);
            const float re = clampf(f.roamLp.tick(f.rng.white()) * roamK, -roamLimit, roamLimit);
            f.roamGain = std::exp2(re - roamBias);
         }
         // The callers' clocks. `Wander` is in cents because a wingbeat is a
         // pitch; a chirp clock is not, so the same control drives it through
         // the factor below -- calibrated, not chosen, so that the default 18
         // cents renders the fractional chirp-peak width the references
         // measure. At 18 cents this is a drift of about 12 per cent.
         if (stridGain > 1.0e-6f) {
            const float clockDepth = clampf(mP.wanderCents, 0.0f, 200.0f) *
                                     (1.0f / 1200.0f) * wanderNorm * kStridClockFactor;
            for (int k = 0; k < v.numStrid; ++k) {
               Stridulator &s = v.strid[k];
               s.clockMul = std::exp2(s.clockLp.tick(s.rng.white()) * clockDepth);
               s.echemeInc = s.echemeBase * s.clockMul;
            }
         }
         for (auto &f : v.passes) {
            if (!f.active)
               continue;
            f.rateMul = std::exp2(f.wanderLp.tick(f.rng.white()) * wanderOct);
            // Air absorption over the distance travelled, updated here because
            // it is the only tan() in the flyby path.
            const float x = f.speed * f.t;
            const float r = std::sqrt(f.d0 * f.d0 + x * x);
            const float cut = clampf(20000.0f * std::exp(-(r - f.d0) * 0.05f), 900.0f, 20000.0f);
            f.airL.setCutoff(cut, sr);
            f.airR.setCutoff(cut, sr);
         }

         v.flybyTimer -= static_cast<double>(kModInterval);
         if (v.flybyTimer <= 0.0 && mP.flybyGain > 1.0e-5f) {
            spawnPass(v, fire);
            // Poisson: the interval is exponential, so passes clump the way
            // real ones do rather than arriving on a grid.
            v.flybyTimer = static_cast<double>(flybyPeriod) *
                           std::max(0.05f, -std::log(std::max(v.rng.uniformPositive(), 1.0e-6f)));
         }
      }

      float l = 0.0f, r = 0.0f;

      // ------------------------------------------------------------ swarm
      for (int k = 0; k < v.numFlyers; ++k) {
         Flyer &f = v.flyers[k];
         const float inc = f.rateHz * f.rateMul / sr;
         f.phase += inc;
         if (f.phase >= 1.0f)
            f.phase -= 1.0f;

         const float p = f.phase;
         const float invHw = 1.0f / f.halfWidth;
         const float d0 = p < 0.5f ? p : 1.0f - p;      // distance to the downstroke
         const float d1 = std::fabs(p - 0.5f);          // to the upstroke
         float ex = strokePulse(d0, invHw) - f.pulseMean;
         ex += f.strokeAmp * (strokePulse(d1, invHw) - f.pulseMean);
         ex += f.noiseAmp * f.noiseLp.tick(f.rng.white());

         // The species' fitted shaper: one-pole shelf, then the bandpass
         // crossfaded in.
         const float lp = f.shelf.tick(ex);
         float y = lp * f.shelfLow + (ex - lp) * f.shelfHigh;
         y = y * (1.0f - f.formantMix) + f.formant.bandpassNormalised(y) * f.formantMix;
         y = f.dcBlock.tick(y);

         // Flutter: the individual turning relative to the listener, which is
         // amplitude modulation at its own wingbeat rate.
         const float am = 1.0f - f.flutter * 0.5f * (1.0f + sin2piFast(p));
         const float g = y * f.amp * f.roamGain * am;
         l += g * f.panL;
         r += g * f.panR;
      }
      l *= swarmGain;
      r *= swarmGain;

      // ------------------------------------------------------------ flyby
      for (auto &f : v.passes) {
         if (!f.active)
            continue;
         // Geometry first: everything the pass does follows from r(t).
         const float x = f.speed * f.t;
         const float rr = std::sqrt(f.d0 * f.d0 + x * x);
         const float level = f.d0 / rr;
         const float sinTheta = x / rr;
         // Doppler. Physically right, and at insect speeds smaller than the
         // wingbeat's own wander -- see the note in the header.
         const float vr = f.speed * f.speed * f.t / rr;
         const float dop = kSpeedOfSound / std::max(kSpeedOfSound + vr, 1.0f);

         f.phase += f.rateHz * f.rateMul * dop / sr;
         if (f.phase >= 1.0f)
            f.phase -= 1.0f;

         const float p = f.phase;
         const float invHw = 1.0f / f.halfWidth;
         const float dA = p < 0.5f ? p : 1.0f - p;
         const float dB = std::fabs(p - 0.5f);
         float ex = strokePulse(dA, invHw) - f.pulseMean;
         ex += f.strokeAmp * (strokePulse(dB, invHw) - f.pulseMean);
         ex += f.noiseAmp * f.noiseLp.tick(f.rng.white());

         const float lp = f.shelf.tick(ex);
         float y = lp * f.shelfLow + (ex - lp) * f.shelfHigh;
         y = y * (1.0f - f.formantMix) + f.formant.bandpassNormalised(y) * f.formantMix;
         y = f.dcBlock.tick(y);
         const float am = 1.0f - f.flutter * 0.5f * (1.0f + sin2piFast(p));

         float pl, pr;
         panFor(sinTheta * f.sweep, pl, pr);
         const float g = y * level * am * passGain;
         l += f.airL.tick(g * pl);
         r += f.airR.tick(g * pr);

         f.t += 1.0f / sr;
         if (f.t >= f.tEnd)
            f.active = false;
      }

      // ------------------------------------------------------- stridulate
      if (stridGain > 1.0e-6f) {
         for (int k = 0; k < v.numStrid; ++k) {
            Stridulator &s = v.strid[k];
            s.echemePhase += s.echemeInc;
            if (s.echemePhase >= 1.0f)
               s.echemePhase -= 1.0f;
            // The echeme gate, with a short raised-cosine at each end so a
            // chirp starts and stops rather than being switched.
            float gate = 0.0f;
            if (s.echemePhase < s.duty) {
               const float u = s.echemePhase / s.duty;
               const float ramp = 0.12f;
               gate = 1.0f;
               if (u < ramp)
                  gate = 0.5f - 0.5f * sin2piFast(0.25f + 0.5f * (u / ramp));
               else if (u > 1.0f - ramp)
                  gate = 0.5f - 0.5f * sin2piFast(0.25f + 0.5f * ((1.0f - u) / ramp));
            }

            s.pulsePhase += s.pulseInc;
            float drive = 0.0f;
            bool struck = false;
            if (s.pulsePhase >= 1.0f) {
               s.pulsePhase -= 1.0f;
               struck = true;
               // The stroke's own loudness, over a 6 dB spread. Drawn uniformly
               // over the whole range instead, a third of the strokes land far
               // enough under the loudest to be inaudible, and what is left is a
               // sparser and more irregular train than the one that was asked
               // for -- a cicada set to 268 a second produced 127 audible ones.
               s.strokeAmp = (0.5f + 0.5f * s.rng.uniformPositive()) * qNorm;
            }
            // **A stroke, not a click.** A scraper dragged across a file
            // excites the harp for as long as the wing is moving -- tens to
            // hundreds of teeth in one stroke -- and a tymbal buckles rib by
            // rib. 0.2.0 modelled each as a single sample into the resonator,
            // and its own calibration comment recorded what that costs: "6 per
            // cent of it is sounding". The composite of a dozen callers hid it
            // in every spectral statistic, and left the chorus with holes in it
            // that no reference has -- the quietest tenth of its frames sat at
            // 0.09 of the median where nine references sit at 0.13 to 0.68.
            //
            // So the stroke drives the resonator with noise for a fraction of
            // the pulse period. The fraction is the one number here that the
            // library does not bound tightly -- within-chirp duty measures 0.08
            // to 0.69 across the references -- so it is set where the rendered
            // chorus lands on their median rather than at either end.
            // `struck ||` is not a detail: at Scrape 0 the condition below is
            // never true and the layer falls silent, where the bottom of that
            // knob has to be the single click 0.2.0 always fired. One sample
            // minimum, always.
            if (struck || s.pulsePhase < scrape)
               drive = s.strokeAmp * kStrokeNorm * s.rng.white();
            if (gate <= 0.0f && !s.body.ringing(1.0e-6f))
               continue;
            // The raw bandpass, not Svf::bandpassNormalised. That one
            // multiplies by k = 1/Q to hold a *sine's* peak gain constant,
            // which for an impulse divides the ring down by the very Q that is
            // supposed to make it ring: a cricket at Q 26 came out at -83 dBFS.
            float slp, sbp, shp;
            s.body.tick(drive * gate, slp, sbp, shp);
            const float y = sbp * s.amp;
            l += y * s.panL * stridGain;
            r += y * s.panR * stridGain;
         }
      }

      // ------------------------------------------------------------- bed
      if (bedGain > 1.0e-6f) {
         const float nl = v.rngBed.white();
         const float nr = v.rngBed.white();
         float bl = v.bedL.bandpassNormalised(nl);
         float br = v.bedR.bandpassNormalised(nr);
         bl = v.bedTiltL.tick(bl) * bedTiltMix + bl * (1.0f - bedTiltMix);
         br = v.bedTiltR.tick(br) * bedTiltMix + br * (1.0f - bedTiltMix);
         l += bl * bedGain;
         r += br * bedGain;
      }

      outL[i] += l * env;
      outR[i] += r * env;
   }
}

void SwarmEngine::processOutputChain(float *outL, float *outR, uint32_t numSamples) {
   const float wet = clampf(mP.spaceAmount, 0.0f, 1.0f);
   for (uint32_t i = 0; i < numSamples; ++i) {
      float l = outL[i] * mDistanceLevel, r = outR[i] * mDistanceLevel;

      // Distance: the top goes first, then the whole thing tilts down.
      if (mAirActive) {
         l = mAirLpL.tick(l);
         r = mAirLpR.tick(r);
      }
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

      switch (mOutFilterActive ? mP.filterType : -1) {
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
      case 0:
         l = mOutFilterL.lowpass(l);
         r = mOutFilterR.lowpass(r);
         break;
      default:
         break; // fully open: not in circuit at all
      }

      l = mOutHpL.tick(l);
      r = mOutHpR.tick(r);

      outL[i] = softClip(l * mP.gain);
      outR[i] = softClip(r * mP.gain);
   }
}

void SwarmEngine::process(float *outL, float *outR, uint32_t numSamples) {
   std::fill(outL, outL + numSamples, 0.0f);
   std::fill(outR, outR + numSamples, 0.0f);

   const float sr = static_cast<float>(mSampleRate);

   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      processVoice(v, outL, outR, numSamples);
   }

   processOutputChain(outL, outR, numSamples);

   // Silence tracking, so the host can sleep once the field has gone quiet.
   float peak = 0.0f;
   for (uint32_t i = 0; i < numSamples; ++i)
      peak = std::max(peak, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
   if (peak < 1.0e-6f)
      mSilenceCounter += numSamples / sr;
   else
      mSilenceCounter = 0.0f;
}

bool SwarmEngine::isSilent() const {
   if (mSilenceCounter < 0.25f)
      return false;
   for (const auto &v : mVoices)
      if (v.active)
         return false;
   return true;
}

uint32_t SwarmEngine::activeVoiceCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices)
      if (v.active)
         ++n;
   return n;
}

uint32_t SwarmEngine::activeEventCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices) {
      if (!v.active)
         continue;
      n += static_cast<uint32_t>(v.numFlyers);
      for (const auto &f : v.passes)
         if (f.active)
            ++n;
      if (mP.stridGain > 1.0e-5f)
         n += static_cast<uint32_t>(v.numStrid);
   }
   return n;
}

float SwarmEngine::tailSeconds() const {
   // The longest thing still to come after the note goes: the release, plus
   // whatever the last pass and the space are still doing. A pass runs to
   // tEnd, which is the trajectory's own half-length.
   const float pass = mP.flybyGain > 1.0e-5f ? clampf(mP.flybyPassSec, 0.05f, 12.0f) * 2.0f : 0.0f;
   const float space = mP.spaceAmount > 0.001f ? 0.3f + mSpace.decaySeconds() : 0.05f;
   return mP.releaseSec + pass + space;
}

} // namespace insectswarm
