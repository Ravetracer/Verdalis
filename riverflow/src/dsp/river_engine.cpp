#include "river_engine.h"

#include "verdalis/dsp/bubble.h"
#include "verdalis/dsp/fastmath.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

namespace riverflow {

namespace {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Minnaert, the Xue damping terms and the Q-to-resonance conversion are the
// suite's; see verdalis/dsp/bubble.h for what they are and why they are trusted.
inline float resonanceFor(float q) { return resonanceForQ(q); }

// Magnitude of Svf::bandpassNormalised at `f` for a band centred at `fc`, as
// power. The TPT structure evaluates the analog prototype at a warped
// frequency, so the digital response is the analog one at
// Omega = tan(pi f / sr) / tan(pi fc / sr), and the constant-peak-gain
// bandpass prototype is k s / (s^2 + k s + 1).
inline float bandpassPower(float f, float fc, float k, float sr) {
   const float wf = std::tan(3.14159265f * clampf(f / sr, 1.0e-5f, 0.49f));
   const float wc = std::tan(3.14159265f * clampf(fc / sr, 1.0e-5f, 0.49f));
   const float om = wf / wc;
   const float num = k * om;
   const float den2 = (1.0f - om * om) * (1.0f - om * om) + num * num;
   const float mag = num / std::sqrt(std::max(den2, 1.0e-20f));
   return mag * mag;
}

// A log-normal multiplier with unit mean, `sigmaOct` octaves wide.
//
// Event levels were drawn uniformly at first, and the renders came out 4 dB
// short of the references' crest factor with every other statistic at its
// correct value. A uniform draw has no tail: real events do, because an
// event's energy follows the volume of water involved in it and that is
// log-normal. Normalised so that widening the spread does not also turn the
// layer up.
inline float logNormal(Rng &rng, float sigmaOct) {
   // Bounded at two sigma. An unbounded Gaussian in the exponent puts the
   // occasional event twelve times above the median, and those are what drove
   // the output stage into its soft clipper: ten of the twenty factory presets
   // peaked at exactly 1.000 and the four worst -- all of them the bubbly ones
   // -- had a fifth of a per cent of their samples past the knee, which is
   // audible as crackle on a noise bed. A real distribution is bounded anyway:
   // a stone can only trap so large a pocket.
   const float g = clampv(rng.gaussian(), -3.0f, 3.0f);
   const float f = std::exp2(g * sigmaOct);
   const float s = sigmaOct * 0.6931472f;
   return f / std::exp(0.5f * s * s);
}

// An exponential decay coefficient reaching -60 dB in `sec`.
inline float decayCoefFor(float sec, double sampleRate) {
   const float n = std::max(1.0f, static_cast<float>(sec * sampleRate));
   return std::exp(-6.907755f / n); // ln(1000)
}

// ------------------------------------------------------------- the bed shapes
//
// Octave centres, and the six measured shapes at them. `tools/analysis/shapes.py`
// clusters all 77 recordings by octave-band colour -- each normalised to its own
// loudest band, so loudness plays no part -- and these are the centroids of the
// six clusters it finds, in dB relative to each cluster's peak band.
//
// The clustering is deliberately run from 125 Hz up. Below that the references
// carry wind and handling noise rather than water: lowend.py measures the
// 20-60 Hz envelope as uncorrelated with the 1-4 kHz envelope in every
// recording, |r| < 0.15, including the four where it is a fifth of the total
// energy. Fitting those bands would fit the weather, not the river.
constexpr float kBedCentres[kNumBedBands] = {125.0f,  250.0f,  500.0f,  1000.0f,
                                             2000.0f, 4000.0f, 8000.0f, 16000.0f};

constexpr float kBedShapes[kNumWaterKinds][kNumBedBands] = {
   /* Deep Rush      */ {-6.2f, -3.2f, -1.3f, -1.5f, -5.6f, -10.5f, -14.8f, -24.5f},
   /* Rapids         */ {-8.5f, -4.7f, -1.5f, -0.7f, -2.6f, -5.5f, -8.4f, -14.2f},
   /* Mountain River */ {-22.2f, -9.2f, -2.6f, -0.8f, -1.7f, -6.5f, -11.8f, -19.0f},
   /* Stream         */ {-15.2f, -12.1f, -6.5f, -1.5f, -1.2f, -2.2f, -4.6f, -10.3f},
   /* Creek          */ {-31.2f, -21.0f, -7.8f, -2.8f, -2.7f, -1.1f, -2.7f, -17.6f},
   /* Trickle        */ {-14.4f, -15.8f, -12.3f, -9.8f, -7.7f, -6.2f, -5.9f, -0.5f},
};

// How deep each band's own surge is, relative to the 1 kHz band.
// `tools/analysis/bandsurge.py` measures the modulation depth of every band's
// 100 ms envelope across the library: 2.43x at 125-250 Hz falling to 0.84x at
// 4-8 kHz, and the correlation between one band's envelope and the 1-2 kHz
// band's is 0.07-0.25 throughout. So the bed is not one gain being moved --
// the bands wander independently, and the bottom wanders furthest. That single
// measurement is most of the difference between this and gated noise.
constexpr float kSurgeDepth[kNumBedBands] = {2.4f, 2.1f, 1.5f, 1.2f, 1.0f, 0.9f, 0.87f, 0.9f};

// How much of the bed's graininess belongs in each band, relative to 2 kHz.
// grain.py measures each band's 4 ms envelope against a Gaussian control of the
// same spectrum: the library's median coefficient of variation runs
// 0.39 / 0.33 / 0.30 / 0.29 against a control's 0.30 / 0.22 / 0.12 / 0.09, so
// the *excess* over noise is 30% at 200-800 Hz and 220% at 6-14 kHz. Water gets
// progressively less Gaussian towards the top, which is what a vast number of
// sub-millimetre bubbles bursting does: too many and too short to be separately
// audible, so they are a granular envelope on a band rather than oscillators.
constexpr float kGrainWeight[kNumBedBands] = {0.25f, 0.30f, 0.35f, 0.50f,
                                              1.00f, 1.50f, 2.00f, 2.20f};

// An event's peak amplitude at 0 dB, as a multiple of the bed's RMS at unity
// Flow Level. Set so that the measured prominences land inside the parameter's
// range: the references' dabbles stand 5.5-16.7 dB above the bed they sit on
// and their drops 2.2-11.7 dB, and without this an event at -12 dB would be
// 20 dB *below* the bed in RMS and could not move a band's statistics at all.
constexpr float kEventGain = 4.0f;

// How wide each layer's level distribution is, in octaves. They differ by a
// lot, and the difference is measured: the references carry their
// impulsiveness below 800 Hz and are close to smooth above it, so the pockets
// a stone traps need a long tail and the spray does not.
constexpr float kDabbleLevelSigma = 1.0f;
constexpr float kTrickleLevelSigma = 0.4f;

// A cloud of bubbles rings far below any bubble in it. Xue et al.'s Figure 3
// puts the first three modes of a pour at 386, 589 and 732 Hz -- ratios of
// 1.00, 1.53 and 1.90. One resonator is a tuned pipe; three are a body of water.
constexpr float kPlungeRatio[3] = {1.0f, 1.53f, 1.90f};
constexpr float kPlungeGain[3] = {1.0f, 0.55f, 0.34f};

// What the water runs between: how much of it is close enough to reflect, and
// how the banks colour what comes back. The enclosure figure is the hard-won
// one -- eight discrete early reflections outdoors is what makes a reverb read
// as a bathroom, and an open river has nothing near enough to bounce off.
struct BankTraits {
   float enclosure; // how much early reflected field there is at all
   float tilt;      // multiplies the bed's brightness
   float bodyTilt;  // multiplies the bed's low weight
   float stoneTilt; // multiplies what the drops are landing on
};

constexpr BankTraits kBankTraits[kNumBankKinds] = {
   /* Open      */ {0.03f, 1.00f, 1.00f, 1.00f},
   /* Forest    */ {0.10f, 0.92f, 1.05f, 0.90f},
   /* Gorge     */ {0.55f, 1.10f, 1.20f, 1.15f},
   /* Rock Pool */ {0.30f, 1.15f, 0.85f, 1.25f},
   /* Culvert   */ {0.90f, 0.85f, 1.35f, 1.10f},
   /* Cavern    */ {0.75f, 0.95f, 1.15f, 1.20f},
};

// Seed 0 is the "always different" setting and has no fixed mapping; every
// non-zero Seed maps here, so two instances never disagree about what a given
// Seed means.
uint32_t rngStateForSeed(int seed) {
   return 0x9E3779B9u * static_cast<uint32_t>(seed) + 17u;
}

std::atomic<uint32_t> gInstanceCounter{0};

} // namespace

void RiverEngine::prepare(double sampleRate, uint32_t /*maxBlockSize*/) {
   mSampleRate = sampleRate;
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   mRng.reseed(static_cast<uint32_t>(now) ^
               (0x9E3779B9u * (gInstanceCounter.fetch_add(1) + 1)));
   mAppliedSeed = 0;

   // The grid every band integral is taken on, the trapezoid weights, and the
   // bank's response at each point. Integrating rather than sampling at the
   // band centre is not academic: the SVF warps frequency towards Nyquist, so
   // at 48 kHz the 16 kHz band's real energy over 11.3-22.6 kHz is about 6 dB
   // below what its centre value predicts.
   {
      const float sr = static_cast<float>(sampleRate);
      const float bankK = 1.0f / 1.4f; // the Q the bands are set to
      for (int b = 0; b < kNumBedBands; ++b) {
         const float lo = kBedCentres[b] * 0.70710678f;
         const float hi = std::min(kBedCentres[b] * 1.41421356f, 0.499f * sr);
         float f[kGridSteps];
         for (int i = 0; i < kGridSteps; ++i) {
            const float t = static_cast<float>(i) / (kGridSteps - 1);
            f[i] = hi > lo ? lo * std::pow(hi / lo, t) : lo;
            mGridWarp[b][i] = std::tan(3.14159265f * clampf(f[i] / sr, 1.0e-6f, 0.4999f));
         }
         // Trapezoid in linear frequency over a log-spaced grid: the integrand
         // is smooth on a log axis and the measure is linear.
         for (int i = 0; i < kGridSteps; ++i) {
            const float a = i > 0 ? f[i] - f[i - 1] : 0.0f;
            const float c = i + 1 < kGridSteps ? f[i + 1] - f[i] : 0.0f;
            mGridWeight[b][i] = 0.5f * (a + c);
         }
         for (int j = 0; j < kNumBedBands; ++j) {
            const float fj = clampf(kBedCentres[j], 20.0f, 0.45f * sr);
            for (int i = 0; i < kGridSteps; ++i)
               mBpGrid[b][j][i] = bandpassPower(f[i], fj, bankK, sr);
         }
      }
   }

   // The plunge pool is the lowest thing here and the tank must not take its
   // bottom off. Nothing below 40 Hz is generated at all, so 30 Hz is clear.
   mSpace.prepare(static_cast<float>(sampleRate), 30.0f);
   reset();
   updateFilters();
}

void RiverEngine::reset() {
   const float sr = static_cast<float>(mSampleRate);
   for (uint32_t vi = 0; vi < kMaxVoices; ++vi) {
      Voice &v = mVoices[vi];
      v.active = false;
      v.env.reset();
      v.hpL.reset();
      v.hpR.reset();
      v.lpL.reset();
      v.lpR.reset();
      v.hpL.setCutoff(mBedHpHz, sr);
      v.hpR.setCutoff(mBedHpHz, sr);
      v.lpL.setCutoff(mBedLpHz, sr);
      v.lpR.setCutoff(mBedLpHz, sr);
      // The filterbank's centres never move, so they are set once here and
      // only the gains change afterwards. That is what makes an eight-band bed
      // affordable at all: no tan() in the parameter path.
      for (int b = 0; b < kNumBedBands; ++b) {
         const float fc = clampf(kBedCentres[b], 20.0f, 0.45f * sr);
         v.bandL[b].reset();
         v.bandR[b].reset();
         v.bandL[b].setCutoff(fc, resonanceFor(1.4f), sr);
         v.bandR[b].setCutoff(fc, resonanceFor(1.4f), sr);
         v.surgeL[b].reset();
         v.surgeR[b].reset();
         v.surgeGainL[b] = 1.0f;
         v.surgeGainR[b] = 1.0f;
      }
      for (int k = 0; k < 3; ++k)
         v.plunge[k].reset();
      v.plungeSurge.reset();
      for (int b = 0; b < kNumBedBands; ++b) {
         v.grainHoldL[b] = 1.0f;
         v.grainHoldR[b] = 1.0f;
         v.grainIncL[b] = 0.0f;
         v.grainIncR[b] = 0.0f;
         // Staggered on purpose: eight sample-and-holds switching on the same
         // sample is a broadband click at a fixed rate, which is audible as a
         // tone rather than as grain.
         v.grainCountL[b] = 1 + static_cast<int>(sr * 0.004f * (b + 1) / 9.0f);
         v.grainCountR[b] = 1 + static_cast<int>(sr * 0.004f * (b + 5) / 13.0f);
      }
      v.modCounter = 0;
      v.dabbleTimer = 0.0;
      v.trickleTimer = 0.0;
      // Seeded from the slot, deterministically: see the note in noteOn.
      v.rngCommon.seed(0x51ED2701u + vi * 2654435761u);
      v.rngL.seed(0x1B873593u + vi * 2246822519u);
      v.rngR.seed(0x85EBCA6Bu + vi * 3266489917u);
      v.rngSurge.seed(0xC2B2AE35u + vi * 668265263u);
      v.rngPlunge.seed(0x27D4EB2Fu + vi * 374761393u);
   }
   for (auto &p : mPockets)
      p.active = false;

   // A non-zero Seed promises the same river every time, so starting over has
   // to start the sequence over too. Seed 0 deliberately keeps running.
   if (mP.seed != 0)
      mRng.reseed(rngStateForSeed(mP.seed));
   mDabbleCounter = 0;

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

void RiverEngine::setParams(const EngineParams &p) {
   mP = p;
   if (mP.seed != mAppliedSeed) {
      mAppliedSeed = mP.seed;
      if (mP.seed != 0)
         mRng.reseed(rngStateForSeed(mP.seed));
   }
   updateFilters();
}

void RiverEngine::updateFilters() {
   const float sr = static_cast<float>(mSampleRate);
   const BankTraits &bt = kBankTraits[clampi(mP.bank, 0, kNumBankKinds - 1)];

   // ----------------------------------------------------------- the bed shape
   //
   // The chosen measured centroid, optionally crossfaded towards the next one
   // so that six clusters are a continuum, then tilted and weighted. The
   // result is normalised to unit power, so Flow Level means the same thing
   // whichever colour is selected -- otherwise picking Trickle would be a
   // 12 dB level change as well as a colour change.
   const int wt = clampi(mP.waterType, 0, kNumWaterKinds - 1);
   const int nxt = clampi(wt + 1, 0, kNumWaterKinds - 1);
   const float blend = clampf(mP.waterBlend, 0.0f, 1.0f);
   const float tilt = clampf(mP.flowTilt, -1.0f, 1.0f) * 6.0f;
   const float body = (clampf(mP.flowBody, 0.0f, 1.0f) - 0.5f) * 24.0f;

   // The response the bed is asked for, per band, in dB.
   float curve[kNumBedBands];
   for (int b = 0; b < kNumBedBands; ++b) {
      float db = kBedShapes[wt][b] * (1.0f - blend) + kBedShapes[nxt][b] * blend;
      db += tilt * std::log2(kBedCentres[b] / 1000.0f);
      db += 20.0f * std::log10(std::max(0.05f, bt.tilt)) *
            clampf(std::log2(kBedCentres[b] / 500.0f) * 0.5f, -1.0f, 1.0f);
      if (b < 2)
         db += body + 20.0f * std::log10(std::max(0.05f, bt.bodyTilt));
      curve[b] = clampf(db, -80.0f, 12.0f);
   }

   // Where the curve's own skirts are too steep for the bank, and only there.
   //
   // An octave-wide bandpass leaks 6 to 8 dB into the octave beside it, so a
   // band whose target sits 15 dB below its neighbour cannot be reached by
   // gains alone however they are solved -- Creek's top octave and Deep Rush's
   // both ask for that. What reaches it is a two-pole filter placed *between*
   // the outermost two bands, at their shared edge, moved further out the
   // steeper the drop is. Below 8 dB across that octave the bank does it
   // unaided and no filter is fitted: Trickle rises monotonically to 16 kHz
   // and forcing a highpass into it would be the error rather than the fix.
   //
   // The corners are a closed form rather than a search. A search over
   // candidate corners would fit better, but it would run in the parameter
   // path, and the residual it would buy back is a few dB in one octave --
   // see the figures in tools/analysis/README.md.
   {
      const float loEdge = kBedCentres[0] * 1.41421356f;                  // 177 Hz
      const float hiEdge = kBedCentres[kNumBedBands - 1] * 0.70710678f;   // 11.3 kHz
      const float sLo = curve[1] - curve[0];
      const float sHi = curve[kNumBedBands - 2] - curve[kNumBedBands - 1];
      mBedHpHz = 40.0f;
      if (sLo >= 8.0f)
         mBedHpHz = clampf(loEdge * std::exp2((sLo - 8.0f) / 12.0f), 60.0f, 900.0f);
      mBedLpHz = 22000.0f;
      if (sHi >= 8.0f)
         mBedLpHz = clampf(hiEdge * std::exp2(-(sHi - 8.0f) / 12.0f), 2000.0f, 22000.0f);
   }

   // What the bank can actually deliver into each band *with the skirt filters
   // in circuit*. The filters' response is arithmetic on the pre-warped grid,
   // so this costs no transcendentals beyond the two corners.
   {
      const float wHp = std::tan(3.14159265f * clampf(mBedHpHz / sr, 1.0e-6f, 0.4999f));
      const float wLp = std::tan(3.14159265f * clampf(mBedLpHz / sr, 1.0e-6f, 0.4999f));
      for (int b = 0; b < kNumBedBands; ++b) {
         float env[kGridSteps];
         for (int i = 0; i < kGridSteps; ++i) {
            const float rh = mGridWarp[b][i] / wHp;
            const float hp = (rh * rh) / (1.0f + rh * rh);
            const float rl = mGridWarp[b][i] / wLp;
            const float lp = 1.0f / (1.0f + rl * rl);
            env[i] = hp * hp * lp * lp; // two poles each
         }
         for (int j = 0; j < kNumBedBands; ++j) {
            float acc = 0.0f;
            for (int i = 0; i < kGridSteps; ++i)
               acc += mGridWeight[b][i] * mBpGrid[b][j][i] * env[i];
            mBankResp[b][j] = acc;
         }
      }
   }

   float want[kNumBedBands];
   for (int b = 0; b < kNumBedBands; ++b) {
      const float g = dbToGain(curve[b]);
      // mBankResp is an energy and now carries the skirt filters, so the
      // target is a plain energy with nothing to undo.
      want[b] = g * g;
   }

   // Solve mBankResp * x = want for the band powers x, so that the *summed*
   // response is the measured curve rather than each band being set to it.
   // Gauss-Seidel with a non-negativity clamp: the matrix is strongly
   // diagonally dominant -- a band's own response at its centre is 1 and its
   // neighbour's is about 0.4 -- so eight sweeps are far more than enough, and
   // the clamp is what keeps a steep step in the curve from asking for a
   // negative gain.
   float x[kNumBedBands];
   for (int b = 0; b < kNumBedBands; ++b)
      x[b] = want[b];
   for (int iter = 0; iter < 8; ++iter) {
      for (int b = 0; b < kNumBedBands; ++b) {
         float sum = 0.0f;
         for (int j = 0; j < kNumBedBands; ++j)
            if (j != b)
               sum += mBankResp[b][j] * x[j];
         const float v = (want[b] - sum) / std::max(mBankResp[b][b], 1.0e-6f);
         x[b] = v > 0.0f ? v : 0.0f;
      }
   }

   float power = 0.0f;
   for (int b = 0; b < kNumBedBands; ++b) {
      mBedGain[b] = std::sqrt(x[b]);
      // The octave bands tile the spectrum, so the total energy out is what
      // they end up holding between them.
      for (int j = 0; j < kNumBedBands; ++j)
         power += mBankResp[b][j] * x[j];
   }
   // Unit RMS out, so Flow Level means the same thing whichever colour is
   // selected -- otherwise picking Trickle would be a level change as well.
   //
   // mBankResp is an energy per unit input density, and the drive is uniform
   // noise on [-1, 1) -- variance 1/3 spread over 0..sr/2, so a density of
   // 2/(3 sr). Leaving that factor out is a 30 dB error in the bed's level
   // that no amount of staring at the shape would reveal, since the shape is
   // exactly right without it.
   const float inputPsd = 2.0f / (3.0f * sr);
   mBedNorm = 1.0f / std::sqrt(std::max(power * inputPsd, 1.0e-12f));

   // ------------------------------------------------------------- the output
   //
   // Distance does two things at once, the way it does outdoors: it takes the
   // top off (air absorption, and more of it in warm damp air) and it tilts
   // what is left downwards.
   const float d = clampf(mP.distance, 0.0f, 1.0f);
   const float airKeep = clampf(mP.air, 0.0f, 1.0f);
   // No distance, no air to absorb: at zero the filter is bypassed outright
   // rather than parked at 20 kHz, where at 48 kHz it still takes several dB
   // off the top octave -- which is the octave two of the measured shapes peak
   // in.
   mAirActive = d > 0.001f;
   const float airHz = 20000.0f * std::pow(0.06f, d * (1.35f - 0.7f * airKeep));
   mAirLpL.setCutoff(clampf(airHz, 400.0f, 20000.0f), sr);
   mAirLpR.setCutoff(clampf(airHz, 400.0f, 20000.0f), sr);
   const float tiltHz = 20000.0f * std::pow(0.25f, d);
   mDistanceTiltL.setCutoff(clampf(tiltHz, 800.0f, 20000.0f), sr);
   mDistanceTiltR.setCutoff(clampf(tiltHz, 800.0f, 20000.0f), sr);

   mOutHpL.setCutoff(clampf(mP.highpassHz, 10.0f, 4000.0f), sr);
   mOutHpR.setCutoff(clampf(mP.highpassHz, 10.0f, 4000.0f), sr);

   // A lowpass at the top of its range is meant to be open. Left in circuit it
   // is a real 20 kHz two-pole, which at 48 kHz is close enough to Nyquist to
   // colour the top octave, so it comes out of the chain instead.
   mOutFilterActive = !(mP.filterType == 0 && mP.filterCutoffHz >= 19990.0f);
   const float reso = clampf(0.05f + 0.90f * mP.filterReso, 0.0f, 0.98f);
   mOutFilterL.setCutoff(clampf(mP.filterCutoffHz, 20.0f, 0.45f * sr), reso, sr);
   mOutFilterR.setCutoff(clampf(mP.filterCutoffHz, 20.0f, 0.45f * sr), reso, sr);

   // A distant river is not one stretch of water heard quietly: it is a whole
   // valley of it, arriving over a wide arc and smeared by the air it crossed.
   // So distance multiplies the number of events and divides their size, which
   // is what turns countable dabbles into a roar. Energy is held roughly
   // constant: n events at 1/sqrt(n) each.
   mDistanceRate = 1.0f + 11.0f * d * d;
   mDistanceLevel = 1.0f / std::sqrt(mDistanceRate);
   mDistanceSmear = 1.0f + 2.5f * d * d;

   mSpace.setSize(mP.spaceSize);
   mSpace.setDamping(mP.spaceDamping);
   mSpace.setEnclosure(bt.enclosure);
}

void RiverEngine::noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId,
                         double velocity) {
   Voice *free = nullptr;
   for (auto &v : mVoices) {
      if (!v.active) {
         free = &v;
         break;
      }
   }
   if (!free) {
      // Steal the quietest voice; a river is a bed, so the least audible one is
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
   v.velocity = clampf(static_cast<float>(velocity), 0.0f, 1.0f);

   const float sr = static_cast<float>(mSampleRate);
   v.env.setParams(mP.attackSec, mP.decaySec, mP.sustain, mP.releaseSec, sr);
   v.env.gateOn();

   for (int b = 0; b < kNumBedBands; ++b) {
      v.bandL[b].reset();
      v.bandR[b].reset();
      v.surgeL[b].reset();
      v.surgeR[b].reset();
   }
   for (int k = 0; k < 3; ++k)
      v.plunge[k].reset();
   v.hpL.reset();
   v.hpR.reset();
   v.lpL.reset();
   v.lpR.reset();
   v.plungeSurge.reset();

   // Deliberately not drawn from mRng. A note can arrive before the Seed
   // parameter has been applied -- events are handled in the order the host
   // sends them -- so anything drawn here would depend on when that happened,
   // and a fixed Seed would stop promising the same river. The voice's own slot
   // and key give all the variation this needs, and give it deterministically.
   const uint32_t slot = static_cast<uint32_t>(&v - mVoices);
   const float spread = static_cast<float>((slot * 7u + static_cast<uint32_t>(key)) % 16u) / 16.0f;

   // The water is already running when you arrive: the first dabble does not
   // wait a full interval.
   v.dabbleTimer = static_cast<double>(sr) / std::max(0.05f, mP.dabbleRateHz) * (0.05f + 0.6f * spread);
   v.trickleTimer =
      static_cast<double>(sr) / std::max(0.05f, mP.trickleRateHz) * (0.05f + 0.6f * spread);
   v.modCounter = 0;
}

void RiverEngine::noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = (noteId >= 0 && v.noteId == noteId) ||
                         (noteId < 0 && v.port == port && v.channel == channel && v.key == key);
      if (match)
         v.env.gateOff();
   }
}

void RiverEngine::choke(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
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

void RiverEngine::allSoundOff() {
   for (auto &v : mVoices) {
      v.env.kill();
      v.active = false;
   }
   for (auto &p : mPockets)
      p.active = false;
}

Pocket *RiverEngine::allocatePocket() {
   const uint32_t cap =
      std::min<uint32_t>(kMaxPockets, static_cast<uint32_t>(std::max(16, mP.maxEvents)));
   for (uint32_t i = 0; i < cap; ++i) {
      if (!mPockets[i].active)
         return &mPockets[i];
   }
   return nullptr; // a pocket dropped from a full pool is inaudible
}

// One dabble: water folding over a stone. The measured event is a short burst
// of pockets rather than one, and they are all in the same place because it is
// one stone -- so the cluster shares a pan and a size, and only jitters within
// it.
void RiverEngine::spawnDabble(float envLevel, float flow) {
   const float sr = static_cast<float>(mSampleRate);
   const int n = clampi(mP.dabbleCluster, 1, 12);

   const float pan = clampf(mRng.white() * clampf(mP.width, 0.0f, 1.0f), -1.0f, 1.0f);
   const float panL = std::sqrt(0.5f * (1.0f - pan));
   const float panR = std::sqrt(0.5f * (1.0f + pan));

   // The size of the pocket this stone traps, log-normal about the set radius.
   const float sizeOct = mRng.gaussian() * clampf(mP.dabbleSpreadOct, 0.0f, 3.0f) * 0.5f;
   const float baseRadius = clampf(mP.dabbleSizeMm * std::exp2(sizeOct), 0.2f, 40.0f);

   const float spill = std::max(1.0f, mP.dabbleSpillSec * sr * mDistanceSmear);
   // n pockets sharing one event's energy.
   const float share = 1.0f / std::sqrt(static_cast<float>(n));

   for (int k = 0; k < n; ++k) {
      Pocket *slot = allocatePocket();
      if (!slot)
         break;
      Pocket &p = *slot;
      p.active = true;
      p.rng.seed(mRng.next() | 1u);
      p.delaySamples = k == 0 ? 0 : static_cast<int>(mRng.uniform() * spill);

      // Pockets from one stone are near the same size: the cluster's own
      // spread is much narrower than the population's.
      const float r = clampf(baseRadius * std::exp2(mRng.gaussian() * 0.22f), 0.2f, 40.0f);
      const float f = clampf(minnaertHz(r), 60.0f, 0.45f * sr);

      const float delta = clampf(bubbleDelta(f) * clampf(mP.dabbleDamping, 0.1f, 8.0f), 0.002f, 0.6f);
      const float beta = 3.14159265f * f * delta;

      p.phase = 0.0f;
      p.inc = f / sr;
      const float rise = 1.03f + 0.09f * mRng.uniform();
      p.chirp = std::pow(rise, beta / sr);
      p.decayCoef = std::exp(-beta / sr);
      // A wide spread, because the references' impulsiveness lives almost
      // entirely below 800 Hz: the size of pocket a stone traps varies hugely
      // from one fold of water to the next, and the rare large one is what
      // gives a creek its character. Measured, creek-02-loop's 200-800 Hz
      // envelope has an excess kurtosis of 35 against 8 an octave above it.
      p.level = envLevel * mP.dabbleGain * kEventGain * mDistanceLevel * share * flow *
                logNormal(mRng, kDabbleLevelSigma);

      // The water being displaced around it.
      p.bodyBand.reset();
      p.bodyBand.setCutoff(clampf(f * 1.15f, 60.0f, 0.45f * sr), resonanceFor(2.0f), sr);
      p.noiseMix = clampf(mP.dabbleGlug, 0.0f, 1.0f) * (0.7f + 0.6f * mRng.uniform());

      // A dabble is water on water: the pinch-off transient, and no impact off
      // a hard surface. The band it goes through sits just above the pocket.
      p.surfaceBand.reset();
      p.surfaceBand.setCutoff(clampf(f * 1.6f, 80.0f, 0.45f * sr), resonanceFor(2.5f), sr);
      p.clickLevel = p.level * 0.15f;
      p.clickCoef = decayCoefFor(0.004f * mDistanceSmear, mSampleRate);
      p.splashLevel = 0.0f;
      p.splashCoef = 0.0f;

      p.panL = panL;
      p.panR = panR;
   }
   ++mDabbleCounter;
}

// One drop striking stone or standing water: the impact off whatever it hit,
// then the tiny pocket it entrained, then -- where the water is deeper -- a
// short wash. The measured drop decays are bimodal, 8-13 ms against 56-86 ms,
// and Splash is that second population.
void RiverEngine::spawnTrickle(float envLevel, float flow) {
   Pocket *slot = allocatePocket();
   if (!slot)
      return;
   Pocket &p = *slot;
   const float sr = static_cast<float>(mSampleRate);
   const BankTraits &bt = kBankTraits[clampi(mP.bank, 0, kNumBankKinds - 1)];

   p.active = true;
   p.rng.seed(mRng.next() | 1u);
   p.delaySamples = 0;

   const float pan = clampf(mRng.white() * clampf(mP.width, 0.0f, 1.0f), -1.0f, 1.0f);
   p.panL = std::sqrt(0.5f * (1.0f - pan));
   p.panR = std::sqrt(0.5f * (1.0f + pan));

   const float oct = mRng.gaussian() * clampf(mP.trickleSpreadOct, 0.0f, 3.0f) * 0.5f;
   const float r = clampf(mP.trickleSizeMm * std::exp2(oct), 0.05f, 12.0f);
   const float f = clampf(minnaertHz(r), 200.0f, 0.45f * sr);

   // A drop's pocket is asked to last a set time rather than to obey the
   // physics alone: Trickle Decay is measured directly from the references'
   // event-triggered envelopes, and Damping is not offered here because a drop
   // that has just landed is not a free bubble.
   const float decay = clampf(mP.trickleDecaySec, 0.001f, 0.5f) * mDistanceSmear;
   p.decayCoef = decayCoefFor(decay * 0.5f, mSampleRate);
   // The chirp is a total rise over the pocket's life, so it is spread over the
   // life this pocket is actually given -- which for a drop comes from the
   // parameter, not from its physical damping. Getting that wrong ran the
   // phase increment into its Nyquist clamp and held it there for most of the
   // pocket's life, which is a near-Nyquist tone read out of an interpolated
   // sine table: broadband hash, not a drop.
   const float lifeSamples = std::max(4.0f, decay * 0.5f * sr);

   p.phase = 0.0f; // struck at zero; see the note in spawnDabble
   p.inc = f / sr;
   const float rise = 1.04f + 0.12f * mRng.uniform();
   p.chirp = std::pow(rise, 1.0f / lifeSamples);

   const float impact = clampf(mP.trickleImpact, 0.0f, 1.0f);
   // Narrow, and deliberately much narrower than a dabble's. Spray is uniform
   // where trapped pockets are not, and the same references that are impulsive
   // below 800 Hz are smooth above it -- kurtosis 8 to 10 against 35.
   const float lvl = envLevel * mP.trickleGain * kEventGain * mDistanceLevel * flow *
                     logNormal(mRng, kTrickleLevelSigma);
   p.level = lvl * (1.0f - 0.75f * impact);

   p.bodyBand.reset();
   p.bodyBand.setCutoff(clampf(f * 1.1f, 80.0f, 0.45f * sr), resonanceFor(2.4f), sr);
   p.noiseMix = 0.35f + 0.4f * mRng.uniform();

   // What it landed on. This is the "stoney surface": a hard wet rock is high
   // and short, moss and gravel are lower and duller.
   const float stone = clampf(mP.stoneToneHz * bt.stoneTilt *
                                 (0.8f + 0.4f * mRng.uniform()),
                              200.0f, 0.45f * sr);
   p.surfaceBand.reset();
   p.surfaceBand.setCutoff(stone, resonanceFor(1.1f), sr);
   p.clickLevel = lvl * impact * 0.6f;
   p.clickCoef = decayCoefFor(0.0022f * mDistanceSmear, mSampleRate);

   // The wash that follows, through the same surface: the impact is the fast
   // decay through that band and the splash is the slow one.
   p.splashLevel = lvl * clampf(mP.splash, 0.0f, 1.0f) * 0.9f;
   p.splashCoef = decayCoefFor(decay * 3.0f, mSampleRate);
}

// One held note, per sample: its envelope, its bed, its plunge pool and the two
// clocks that decide when the next dabble and the next drop happen. All in one
// loop, because a clock has to see the envelope of the sample it spawns into.
void RiverEngine::processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples) {
   const float sr = static_cast<float>(mSampleRate);

   const float velLevel = 1.0f + mP.velToLevel * (v.velocity - 0.5f) * 1.8f;
   // More water means more of everything: more events, and a bed with more
   // weight in it. One control, because that is what a rising river does.
   const float flow = clampf(1.0f + mP.velToFlow * (v.velocity - 0.5f) * 1.6f, 0.1f, 2.0f);

   // The bed's L/R correlation is 1 - Flow Width, so the library's median
   // measured correlation of 0.60 is Flow Width at 0.40.
   const float wide = clampf(mP.flowWidth, 0.0f, 1.0f);
   const float cCommon = std::sqrt(1.0f - wide);
   const float cIndep = std::sqrt(wide);

   v.hpL.setCutoff(mBedHpHz, sr);
   v.hpR.setCutoff(mBedHpHz, sr);
   v.lpL.setCutoff(mBedLpHz, sr);
   v.lpR.setCutoff(mBedLpHz, sr);

   const float bedLevel = mP.flowGain * mBedNorm * velLevel;
   const float turb = clampf(mP.turbulence, 0.0f, 1.0f) * 0.5f;
   const float grain = clampf(mP.flowGrain, 0.0f, 1.0f);
   float gw[kNumBedBands];
   float grainNorm[kNumBedBands];
   for (int b = 0; b < kNumBedBands; ++b) {
      gw[b] = clampf(grain * kGrainWeight[b], 0.0f, 1.0f);
      // Normalised in *power*, not in mean. The hold is spiky and positive,
      // so a unit-mean multiplier still has E[h^2] = 1 + gw^2 Var(h) -- which
      // is up to 2.1 dB of extra band energy at full Grain in the top bands,
      // where the weighting is highest. Grain is meant to change the texture
      // of a band and not its level, and this is what makes it do that.
      // Var(-0.88 + 2.64 u^2) for u uniform on (0, 1] is 0.6196.
      grainNorm[b] = 1.0f / std::sqrt(1.0f + gw[b] * gw[b] * 0.6196f);
   }
   // Each band takes a few milliseconds to reach its next value, which is the
   // scale the graininess was measured at. Reaching it rather than jumping to
   // it is the whole point: see the note on grainIncL in the header.
   const int grainPeriod = 1 + static_cast<int>(sr * 0.004f);

   // The surge: a one-pole lowpass on white noise per band, whose output is
   // normalised to unit variance so that Turbulence means a depth rather than
   // a filter setting. std of a one-pole LP driven by uniform noise of variance
   // 1/3 is sqrt(coef / (2 - coef) / 3).
   const float surgeCoef = onePoleCoef(1.0f / (6.2831853f * clampf(mP.surgeRateHz, 0.02f, 40.0f)),
                                       sr / static_cast<float>(kModInterval));
   const float surgeNorm = 1.0f / std::sqrt(std::max(1.0e-6f, surgeCoef / (2.0f - surgeCoef) / 3.0f));
   for (int b = 0; b < kNumBedBands; ++b) {
      v.surgeL[b].setCoef(surgeCoef);
      v.surgeR[b].setCoef(surgeCoef);
   }

   // The plunge pool: three collective modes, driven by noise and breathing.
   const float plungeQ = 0.8f + 6.0f * clampf(mP.plungeQ, 0.0f, 1.0f);
   for (int k = 0; k < 3; ++k) {
      const float f = clampf(mP.plungeToneHz * kPlungeRatio[k], 30.0f, 0.45f * sr);
      v.plunge[k].setCutoff(f, resonanceFor(plungeQ), sr);
   }
   v.plungeSurge.setCoef(onePoleCoef(0.6f, sr / static_cast<float>(kModInterval)));
   const float plungeLevel = mP.plungeGain * velLevel * flow;

   // Event intervals. Distance multiplies the rate; velocity-as-flow does too.
   const float dabbleRate = clampf(mP.dabbleRateHz * mDistanceRate * flow, 0.005f, 4000.0f);
   const float trickleRate = clampf(mP.trickleRateHz * mDistanceRate * flow, 0.005f, 8000.0f);

   for (uint32_t i = 0; i < numSamples; ++i) {
      const float env = v.env.tick();

      // -------------------------------------------------- the control rate
      if (v.modCounter == 0) {
         for (int b = 0; b < kNumBedBands; ++b) {
            const float sl = v.surgeL[b].tick(v.rngSurge.white()) * surgeNorm;
            const float sr_ = v.surgeR[b].tick(v.rngSurge.white()) * surgeNorm;
            v.surgeGainL[b] = clampf(1.0f + turb * kSurgeDepth[b] * clampf(sl, -3.0f, 3.0f),
                                     0.02f, 4.0f);
            v.surgeGainR[b] = clampf(1.0f + turb * kSurgeDepth[b] * clampf(sr_, -3.0f, 3.0f),
                                     0.02f, 4.0f);
         }
      }
      if (++v.modCounter >= kModInterval)
         v.modCounter = 0;

      // ---------------------------------------------------------- the bed
      //
      // Each band gets its own noise. That is not an optimisation to undo: it
      // is what makes the bank's summed response predictable, because
      // independent drives mean the bands' energies add while a shared drive
      // makes them add coherently and the cross terms are large between
      // neighbours that overlap by design. Solving the gains against a model
      // that assumes one and rendering the other left the steepest shapes 7 dB
      // out in their outermost octave.
      //
      // It is also the more faithful of the two. bandsurge.py measures the
      // correlation between one band's envelope and its neighbour's at 0.07 to
      // 0.25 across the library: a river's bands really do move independently.

      // The bed's own graininess, band by band. A sample-and-hold on each
      // band's amplitude at the 4 ms scale the measurement was taken at,
      // weighted by how far that band departs from Gaussian in the references.
      // Zero Grain is the measured value for a smooth river -- the smoothest
      // third of the library sits *on* the control row -- and is where most
      // presets sit; it is the grainy ones that need this.
      if (grain > 0.001f) {
         const float invPeriod = 1.0f / static_cast<float>(grainPeriod);
         for (int b = 0; b < kNumBedBands; ++b) {
            if (--v.grainCountL[b] <= 0) {
               v.grainCountL[b] = grainPeriod;
               const float u = v.rngSurge.uniformPositive();
               const float target = (1.0f + gw[b] * (-0.88f + 2.64f * u * u)) * grainNorm[b];
               v.grainIncL[b] = (target - v.grainHoldL[b]) * invPeriod;
            }
            v.grainHoldL[b] += v.grainIncL[b];
            if (--v.grainCountR[b] <= 0) {
               v.grainCountR[b] = grainPeriod;
               const float u = v.rngSurge.uniformPositive();
               const float target = (1.0f + gw[b] * (-0.88f + 2.64f * u * u)) * grainNorm[b];
               v.grainIncR[b] = (target - v.grainHoldR[b]) * invPeriod;
            }
            v.grainHoldR[b] += v.grainIncR[b];
         }
      }

      float bedL = 0.0f, bedR = 0.0f;
      for (int b = 0; b < kNumBedBands; ++b) {
         const float g = mBedGain[b];
         // Common plus independent at sqrt weights: the variance is the same
         // whatever Flow Width is, so it changes the image and not the level,
         // and the resulting L/R correlation is 1 - flowWidth.
         const float common = v.rngCommon.white() * cCommon;
         const float nL = common + v.rngL.white() * cIndep;
         const float nR = common + v.rngR.white() * cIndep;
         bedL += v.bandL[b].bandpassNormalised(nL) * g * v.surgeGainL[b] * v.grainHoldL[b];
         bedR += v.bandR[b].bandpassNormalised(nR) * g * v.surgeGainR[b] * v.grainHoldR[b];
      }

      bedL = v.lpL.tick(v.hpL.tick(bedL)) * bedLevel * env;
      bedR = v.lpR.tick(v.hpR.tick(bedR)) * bedLevel * env;

      // ------------------------------------------------------- the plunge
      float plunge = 0.0f;
      if (plungeLevel > 1.0e-5f) {
         const float breathe =
            1.0f + clampf(mP.plungeDepth, 0.0f, 1.0f) *
                      clampf(v.plungeSurge.tick(v.rngPlunge.white()) * 3.2f, -1.0f, 2.0f);
         const float drive = v.rngPlunge.white() * clampf(breathe, 0.0f, 3.0f);
         for (int k = 0; k < 3; ++k)
            plunge += v.plunge[k].bandpassNormalised(drive) * kPlungeGain[k];
         plunge *= plungeLevel * env * 0.5f;
      }

      outL[i] += bedL + plunge;
      outR[i] += bedR + plunge;

      // ------------------------------------------------------- the events
      //
      // Both clocks are Poisson processes rather than metronomes: the interval
      // is drawn afresh each time from an exponential distribution, so the
      // events cluster and gap the way real ones do.
      if (env > 1.0e-4f) {
         v.dabbleTimer -= 1.0;
         while (v.dabbleTimer <= 0.0) {
            if (mP.dabbleGain > 1.0e-5f)
               spawnDabble(env, flow);
            v.dabbleTimer += std::max(1.0, static_cast<double>(mRng.exponential(dabbleRate)) * sr);
         }
         v.trickleTimer -= 1.0;
         while (v.trickleTimer <= 0.0) {
            if (mP.trickleGain > 1.0e-5f)
               spawnTrickle(env, flow);
            v.trickleTimer += std::max(1.0, static_cast<double>(mRng.exponential(trickleRate)) * sr);
         }
      }
   }

   if (v.finished())
      v.active = false;
}

void RiverEngine::processPockets(float *outL, float *outR, uint32_t numSamples) {
   for (auto &p : mPockets) {
      if (!p.active)
         continue;
      for (uint32_t i = 0; i < numSamples; ++i) {
         if (p.delaySamples > 0) {
            --p.delaySamples;
            continue;
         }
         p.inc *= p.chirp;
         // A ceiling well short of Nyquist, whatever the chirp is asked for.
         // sin2piFast reads an interpolated 4096-entry table, and a phase
         // increment close to 0.5 walks it in steps large enough for the
         // interpolation error to become broadband noise rather than a tone.
         if (p.inc > 0.40f)
            p.inc = 0.40f;
         p.phase += p.inc;
         if (p.phase >= 1.0f)
            p.phase -= 1.0f;

         // The ringing air, and the water being displaced around it.
         float s = sin2piFast(p.phase) * p.level;
         if (p.noiseMix > 1.0e-4f)
            s += p.bodyBand.bandpassNormalised(p.rng.white()) * p.level * p.noiseMix;

         // What it struck: the impact, then the wash, both through one band.
         if (p.clickLevel > 1.0e-6f || p.splashLevel > 1.0e-6f) {
            const float surf = p.surfaceBand.bandpassNormalised(p.rng.white());
            s += surf * (p.clickLevel + p.splashLevel);
            p.clickLevel *= p.clickCoef;
            p.splashLevel *= p.splashCoef;
         }

         outL[i] += s * p.panL;
         outR[i] += s * p.panR;
         p.level *= p.decayCoef;
      }
      if (p.delaySamples <= 0 && p.level < 1.0e-5f && p.clickLevel < 1.0e-6f &&
          p.splashLevel < 1.0e-6f)
         p.active = false;
   }
}

void RiverEngine::processOutputChain(float *outL, float *outR, uint32_t numSamples) {
   const float wet = clampf(mP.spaceAmount, 0.0f, 1.0f);
   for (uint32_t i = 0; i < numSamples; ++i) {
      float l = outL[i], r = outR[i];

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

      // Saturate rather than clip: with every layer at once the sum can run
      // past the ceiling, and a river should not crackle when it does.
      outL[i] = softClip(l * mP.gain);
      outR[i] = softClip(r * mP.gain);
   }
}

void RiverEngine::process(float *outL, float *outR, uint32_t numSamples) {
   std::fill(outL, outL + numSamples, 0.0f);
   std::fill(outR, outR + numSamples, 0.0f);

   const float sr = static_cast<float>(mSampleRate);

   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      processVoice(v, outL, outR, numSamples);
   }

   processPockets(outL, outR, numSamples);
   processOutputChain(outL, outR, numSamples);

   // Silence tracking, so the host can sleep when the water has run out.
   float peak = 0.0f;
   for (uint32_t i = 0; i < numSamples; ++i)
      peak = std::max(peak, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
   if (peak < 1.0e-6f)
      mSilenceCounter += numSamples / sr;
   else
      mSilenceCounter = 0.0f;
}

bool RiverEngine::isSilent() const {
   if (mSilenceCounter < 0.25f)
      return false;
   for (const auto &v : mVoices)
      if (v.active)
         return false;
   for (const auto &p : mPockets)
      if (p.active)
         return false;
   return true;
}

uint32_t RiverEngine::activeVoiceCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices)
      if (v.active)
         ++n;
   return n;
}

uint32_t RiverEngine::activePocketCount() const {
   uint32_t n = 0;
   for (const auto &p : mPockets)
      if (p.active)
         ++n;
   return n;
}

float RiverEngine::tailSeconds() const {
   // The longest thing still to come after the note goes: the release, plus
   // whatever the last drop and the space are still doing.
   const float events = std::max(mP.trickleDecaySec * 4.0f, mP.dabbleSpillSec + 0.15f);
   const float space = mP.spaceAmount > 0.001f ? 0.3f + mSpace.decaySeconds() : 0.05f;
   return mP.releaseSec + events + space;
}

} // namespace riverflow
