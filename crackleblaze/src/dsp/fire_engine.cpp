#include "fire_engine.h"

#include "verdalis/dsp/fastmath.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

namespace crackleblaze {

namespace {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Q to the Svf's 0..1 resonance. Svf::setCutoff maps resonance onto k = 1/Q
// over 2..0.02, so this is that mapping read backwards.
inline float resonanceFor(float q) {
   const float k = 1.0f / clampf(q, 0.5f, 60.0f);
   return clampf((2.0f - k) / 1.98f, 0.0f, 1.0f);
}

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

// A log-normal multiplier with unit mean, `sigmaDb` decibels wide.
//
// The width is a measurement, not a taste: `tools/analysis/bursts.py` fits the
// log of every detected crackle's prominence over its bed and gets a standard
// deviation of 0.30 decades across the library, which is 6.0 dB. Normalised so
// that widening the spread does not also turn the layer up.
inline float logNormalDb(Rng &rng, float sigmaDb) {
   // Bounded at three sigma. An unbounded Gaussian in the exponent puts the
   // occasional crackle 30 dB over the median, and those are what drive the
   // output stage into its soft clipper.
   const float g = clampv(rng.gaussian(), -3.0f, 3.0f);
   const float s = sigmaDb * 0.1151293f; // dB -> natural log units
   return std::exp(g * s - 0.5f * s * s);
}

// ------------------------------------------------------------- the bed shapes
//
// Octave centres, and the five measured shapes at them.
// `tools/analysis/shapes.py` clusters the seventeen usable recordings by the
// octave-band colour of their *bed* -- `bed.py` gates out the loudest quarter
// of every recording's 4 ms frames first, which is what removes the crackles --
// and these are the centroids of the five clusters it finds, in dB relative to
// each cluster's own peak band.
//
// Gating before clustering is not a refinement. The library's median crest
// factor is 31.7 dB, so the ungated spectrum of a fire is substantially the
// spectrum of its crackles, and clustering on it sorts the references by how
// close the microphone was rather than by what kind of fire it was.
//
// The clustering runs from 125 Hz up. Below that the references carry traffic,
// ventilation and handling noise rather than fire: lowend.py measures the
// 20-60 Hz envelope as uncorrelated with the 1-4 kHz envelope in twenty-four of
// the twenty-five recordings, including the eight where it is more than a
// quarter of the total energy.
constexpr float kBedCentres[kNumBedBands] = {125.0f,  250.0f,  500.0f,  1000.0f,
                                             2000.0f, 4000.0f, 8000.0f, 16000.0f};

constexpr float kFireShapes[kNumFireKinds][kNumBedBands] = {
   /* Deep Blaze    */ {0.0f, -1.3f, -4.1f, -7.4f, -10.1f, -10.2f, -10.5f, -15.1f},
   /* Log Fire      */ {0.0f, -5.6f, -10.4f, -12.6f, -13.6f, -12.0f, -8.9f, -13.5f},
   /* Camp Fire     */ {0.0f, -4.2f, -8.1f, -9.0f, -7.9f, -5.3f, -4.5f, -7.9f},
   /* Open Flame    */ {-7.3f, 0.0f, -1.2f, -3.9f, -3.3f, -2.4f, -1.9f, -7.4f},
   /* Stove Draught */ {-7.7f, -13.6f, -19.1f, -21.7f, -21.6f, -17.0f, -10.2f, 0.0f},
};

// The coefficient of variation of each band's 100 ms envelope at the default
// Flare of 0.55, as measured by `bed.py` on a robust (25th-percentile)
// envelope that ignores the crackles riding on it.
//
// The bottom three entries are the measurement outright: 0.39, 0.45, 0.57 at
// 125, 250 and 500 Hz. The upper five are not, and the reason is worth stating
// plainly. The measured values there run to 1.28, but no gate removes the
// crackle and sizzle layers cleanly from the band they live in, and *this
// engine generates those layers separately*. Shipping the measured 1.28 at
// 2 kHz would make the bed wobble by an amount that, in the reference, was the
// crackles -- and then add the crackles on top. The upper bands therefore
// continue the measured trend and flatten, and the render's own per-band CV is
// what gets checked against the library instead. See tools/analysis/README.md.
constexpr float kFlareCv[kNumBedBands] = {0.39f, 0.45f, 0.57f, 0.76f,
                                          0.85f, 0.85f, 0.80f, 0.70f};
constexpr float kFlareCvRef = 0.55f; // the Flare those figures were taken at

// How the flare is split between one walk for the whole fire and one per band.
//
// `bed.py` measures the correlation between neighbouring bands' 100 ms
// envelopes at 0.54 to 0.91 across the library, median 0.85 -- where RiverFlow
// measures 0.07 to 0.25 on the same statistic. A fire surges as one flame; a
// river's bands wander independently. Two components at sqrt(0.85) and
// sqrt(1 - 0.85) sum to unit variance and come out correlated at exactly 0.85.
constexpr float kFlareCommonR = 0.85f;

// Where the draught sits. Air pulled into a fire is a broadband hiss with no
// low end -- it is the top two octaves of the Stove Draught reference and
// nothing else -- so a two-pole highpass here and no further shaping.
constexpr float kDraughtHz = 3000.0f;

// The Q the bed's bandpasses are set to, and it is a fitted trade rather than
// a round number. Octave-wide bands (Q = 1.4) leak so much into their
// neighbours that the solve below cannot reach the contrast two of the five
// measured shapes ask for: Log Fire falls 5.6 dB from 125 to 250 Hz and Stove
// Draught 5.9, and at Q = 1.4 the bank's floor leaves them 3.0 and 4.0 dB
// short however the gains are solved -- the non-negativity clamp bites and the
// band sits on its neighbour's leakage.
//
// Narrowing the bands fixes that and costs ripple between the centres, since
// octave-spaced bands narrower than an octave no longer quite meet. Modelled
// over all five shapes:
//
//   Q      worst octave-band error      ripple between centres (sd / peak-peak)
//   1.4          4.0 dB                        0.15 dB / 0.8 dB
//   1.8          2.4 dB                        0.24 dB / 1.4 dB
//   2.2          1.1 dB                        0.34 dB / 2.0 dB
//   2.8          0.1 dB                        0.50 dB / 2.8 dB
//
// 2.2 is where the error stops mattering before the ripple starts to. A 2 dB
// peak-to-peak ripple on a noise bed is not audible; a shape 4 dB short of the
// one it was measured from is a different fire.
constexpr float kBedQ = 2.2f;

// An event's peak amplitude at 0 dB, as a multiple of the bed's RMS at unity
// Roar Level. Fitted, by rendering the default patch and putting it through
// tools/analysis/crackles.py, so that the crackles stand the measured 13.2 dB
// over the bed they sit on.
constexpr float kEventGain = 2.9f;

// What the fire is burning in: how much of it is close enough to reflect, and
// how the enclosure colours the roar. The enclosure figure is the suite's
// hard-won one -- eight discrete early reflections outdoors is what makes a
// reverb read as a bathroom, and a fire in a field has nothing near enough to
// bounce off. The two references recorded inside a fireplace measure as the
// most reverberant in the library, which is where Fireplace's 0.62 comes from.
struct HearthTraits {
   float enclosure; // how much early reflected field there is at all
   float tilt;      // multiplies the roar's brightness
   float bodyTilt;  // multiplies the roar's low weight
};

constexpr HearthTraits kHearthTraits[kNumHearthKinds] = {
   /* Open         */ {0.03f, 1.00f, 0.90f},
   /* Fire Ring    */ {0.12f, 0.98f, 1.05f},
   /* Stone Hearth */ {0.35f, 1.02f, 1.10f},
   /* Fireplace    */ {0.62f, 0.95f, 1.15f},
   /* Stove        */ {0.80f, 1.08f, 1.20f},
   /* Cavern       */ {0.75f, 0.92f, 1.12f},
};

// Seed 0 is the "always different" setting and has no fixed mapping; every
// non-zero Seed maps here, so two instances never disagree about what a given
// Seed means.
uint32_t rngStateForSeed(int seed) {
   return 0x9E3779B9u * static_cast<uint32_t>(seed) + 17u;
}

std::atomic<uint32_t> gInstanceCounter{0};

} // namespace

void FireEngine::prepare(double sampleRate, uint32_t /*maxBlockSize*/) {
   mSampleRate = sampleRate;
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   mRng.reseed(static_cast<uint32_t>(now) ^
               (0x9E3779B9u * (gInstanceCounter.fetch_add(1) + 1)));
   mAppliedSeed = 0;

   // The grid every band integral is taken on, the trapezoid weights, and the
   // bank's response at each point. Integrating rather than sampling at the
   // band centre is not academic: the SVF warps frequency towards Nyquist, so
   // at 48 kHz the 16 kHz band's real energy over 11.3-22.6 kHz is about 6 dB
   // below what its centre value predicts -- and Stove Draught peaks there.
   {
      const float sr = static_cast<float>(sampleRate);
      const float bankK = 1.0f / kBedQ;
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

   // The settle layer is the lowest thing here and the tank must not take its
   // bottom off. Nothing below 40 Hz is generated at all, so 30 Hz is clear.
   mSpace.prepare(static_cast<float>(sampleRate), 30.0f);
   reset();
   updateFilters();
}

void FireEngine::reset() {
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
         v.bandL[b].setCutoff(fc, resonanceFor(kBedQ), sr);
         v.bandR[b].setCutoff(fc, resonanceFor(kBedQ), sr);
         v.flareBand[b].reset();
         v.flareGainL[b] = 1.0f;
         v.flareGainR[b] = 1.0f;
      }
      v.flareCommon.reset();
      v.flareValue = 0.0f;
      v.draughtL.reset();
      v.draughtR.reset();
      v.draughtL.setCutoff(kDraughtHz, sr);
      v.draughtR.setCutoff(kDraughtHz, sr);
      v.modCounter = 0;
      v.crackleTimer = 0.0;
      v.settleTimer = 0.0;
      // Seeded from the slot, deterministically: see the note in noteOn.
      v.rngCommon.seed(0x51ED2701u + vi * 2654435761u);
      v.rngL.seed(0x1B873593u + vi * 2246822519u);
      v.rngR.seed(0x85EBCA6Bu + vi * 3266489917u);
      v.rngFlare.seed(0xC2B2AE35u + vi * 668265263u);
   }
   for (auto &e : mEvents)
      e.active = false;

   // A non-zero Seed promises the same fire every time, so starting over has
   // to start the sequence over too. Seed 0 deliberately keeps running.
   if (mP.seed != 0)
      mRng.reseed(rngStateForSeed(mP.seed));
   mCrackleCounter = 0;

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

void FireEngine::setParams(const EngineParams &p) {
   mP = p;
   if (mP.seed != mAppliedSeed) {
      mAppliedSeed = mP.seed;
      if (mP.seed != 0)
         mRng.reseed(rngStateForSeed(mP.seed));
   }
   updateFilters();
}

void FireEngine::updateFilters() {
   const float sr = static_cast<float>(mSampleRate);
   const HearthTraits &ht = kHearthTraits[clampi(mP.hearth, 0, kNumHearthKinds - 1)];

   // ----------------------------------------------------------- the bed shape
   //
   // The chosen measured centroid, optionally crossfaded towards the next one
   // so that five clusters are a continuum, then tilted and weighted. The
   // result is normalised to unit power, so Roar Level means the same thing
   // whichever colour is selected -- otherwise picking Stove Draught would be a
   // 12 dB level change as well as a colour change.
   const int ft = clampi(mP.fireType, 0, kNumFireKinds - 1);
   const int nxt = clampi(ft + 1, 0, kNumFireKinds - 1);
   const float blend = clampf(mP.fireBlend, 0.0f, 1.0f);
   const float tilt = clampf(mP.roarTilt, -1.0f, 1.0f) * 6.0f;
   const float body = (clampf(mP.roarBody, 0.0f, 1.0f) - 0.5f) * 24.0f;

   // The response the bed is asked for, per band, in dB.
   float curve[kNumBedBands];
   for (int b = 0; b < kNumBedBands; ++b) {
      float db = kFireShapes[ft][b] * (1.0f - blend) + kFireShapes[nxt][b] * blend;
      db += tilt * std::log2(kBedCentres[b] / 1000.0f);
      db += 20.0f * std::log10(std::max(0.05f, ht.tilt)) *
            clampf(std::log2(kBedCentres[b] / 500.0f) * 0.5f, -1.0f, 1.0f);
      if (b < 2)
         db += body + 20.0f * std::log10(std::max(0.05f, ht.bodyTilt));
      curve[b] = clampf(db, -80.0f, 12.0f);
   }

   // Where the curve's own skirts are too steep for the bank, and only there.
   //
   // An octave-wide bandpass leaks 6 to 8 dB into the octave beside it, so a
   // band whose target sits 15 dB below its neighbour cannot be reached by
   // gains alone however they are solved -- Stove Draught's bottom asks for
   // that. What reaches it is a two-pole filter placed *between* the outermost
   // two bands, at their shared edge, moved further out the steeper the drop
   // is. Below 8 dB across that octave the bank does it unaided and no filter
   // is fitted: Camp Fire is almost flat across its top two octaves and
   // forcing a lowpass into it would be the error rather than the fix.
   {
      const float loEdge = kBedCentres[0] * 1.41421356f;                // 177 Hz
      const float hiEdge = kBedCentres[kNumBedBands - 1] * 0.70710678f; // 11.3 kHz
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
   // Unit RMS out, so Roar Level means the same thing whichever colour is
   // selected. mBankResp is an energy per unit input density, and the drive is
   // uniform noise on [-1, 1) -- variance 1/3 spread over 0..sr/2, so a density
   // of 2/(3 sr). Leaving that factor out is a 30 dB error in the bed's level
   // that no amount of staring at the shape would reveal, since the shape is
   // exactly right without it.
   const float inputPsd = 2.0f / (3.0f * sr);
   mBedNorm = 1.0f / std::sqrt(std::max(power * inputPsd, 1.0e-12f));

   // The draught's own normalisation, for the same reason and by the same
   // method: Draught has to mean a level relative to the roar, not a filter
   // setting. What fraction of white noise a two-pole highpass at kDraughtHz
   // passes depends on the sample rate, so it is integrated here rather than
   // guessed -- and the first version of this scaled the draught by the bed's
   // *internal* gain instead of its output level, which put nine decibels of
   // hiss over everything and buried the crackles the plugin exists for.
   {
      float acc = 0.0f;
      constexpr int kSteps = 256;
      for (int i = 0; i < kSteps; ++i) {
         const float f = (static_cast<float>(i) + 0.5f) * (0.5f * sr / kSteps);
         const float r = f / kDraughtHz;
         const float h = (r * r) / (1.0f + r * r); // two one-pole highpasses
         acc += h * h;
      }
      acc /= static_cast<float>(kSteps);
      // 1/sqrt(acc) normalises the filter; sqrt(3) turns the uniform drive
      // (variance 1/3) into a unit-variance one.
      mDraughtNorm = 1.7320508f / std::sqrt(std::max(acc, 1.0e-6f));
   }

   // ------------------------------------------------------------- the output
   //
   // Distance does two things at once, the way it does outdoors: it takes the
   // top off (air absorption, and more of it in warm damp air) and it tilts
   // what is left downwards.
   const float d = clampf(mP.distance, 0.0f, 1.0f);
   const float airKeep = clampf(mP.air, 0.0f, 1.0f);
   // No distance, no air to absorb: at zero the filter is bypassed outright
   // rather than parked at 20 kHz, where at 48 kHz it still takes several dB
   // off the top octave -- which is the octave Stove Draught peaks in.
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
   // colour the top octave.
   mOutFilterActive = !(mP.filterType == 0 && mP.filterCutoffHz >= 19990.0f);
   const float reso = clampf(0.05f + 0.90f * mP.filterReso, 0.0f, 0.98f);
   mOutFilterL.setCutoff(clampf(mP.filterCutoffHz, 20.0f, 0.45f * sr), reso, sr);
   mOutFilterR.setCutoff(clampf(mP.filterCutoffHz, 20.0f, 0.45f * sr), reso, sr);

   // A fire heard across a field is not one hearth turned down: it is a bonfire,
   // and its crackles arrive from a wider arc, smaller and smeared by the air
   // they crossed. So distance multiplies their number and divides their size,
   // which is what turns countable ticks into a roar. Energy is held roughly
   // constant: n events at 1/sqrt(n) each.
   mDistanceRate = 1.0f + 11.0f * d * d;
   mDistanceLevel = 1.0f / std::sqrt(mDistanceRate);
   mDistanceSmear = 1.0f + 2.5f * d * d;

   mSpace.setSize(mP.spaceSize);
   mSpace.setDamping(mP.spaceDamping);
   mSpace.setEnclosure(ht.enclosure);
}

void FireEngine::noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId,
                        double velocity) {
   Voice *free = nullptr;
   for (auto &v : mVoices) {
      if (!v.active) {
         free = &v;
         break;
      }
   }
   if (!free) {
      // Steal the quietest voice; a fire is a bed, so the least audible one is
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
      v.flareBand[b].reset();
   }
   v.flareCommon.reset();
   v.flareValue = 0.0f;
   v.hpL.reset();
   v.hpR.reset();
   v.lpL.reset();
   v.lpR.reset();
   v.draughtL.reset();
   v.draughtR.reset();

   // Deliberately not drawn from mRng. A note can arrive before the Seed
   // parameter has been applied -- events are handled in the order the host
   // sends them -- so anything drawn here would depend on when that happened,
   // and a fixed Seed would stop promising the same fire. The voice's own slot
   // and key give all the variation this needs, and give it deterministically.
   const uint32_t slot = static_cast<uint32_t>(&v - mVoices);
   const float spread = static_cast<float>((slot * 7u + static_cast<uint32_t>(key)) % 16u) / 16.0f;

   // The fire is already burning when you arrive: the first crackle does not
   // wait a full interval.
   v.crackleTimer =
      static_cast<double>(sr) / std::max(0.05f, mP.crackleRateHz) * (0.05f + 0.6f * spread);
   v.settleTimer =
      static_cast<double>(sr) / std::max(0.005f, mP.settleRateHz) * (0.05f + 0.6f * spread);
   v.modCounter = 0;
}

void FireEngine::noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      const bool match = (noteId >= 0 && v.noteId == noteId) ||
                         (noteId < 0 && v.port == port && v.channel == channel && v.key == key);
      if (match)
         v.env.gateOff();
   }
}

void FireEngine::choke(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
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

void FireEngine::allSoundOff() {
   for (auto &v : mVoices) {
      v.env.kill();
      v.active = false;
   }
   for (auto &e : mEvents)
      e.active = false;
}

FireEvent *FireEngine::allocateEvent() {
   const uint32_t cap =
      std::min<uint32_t>(kMaxEvents, static_cast<uint32_t>(std::max(16, mP.maxEvents)));
   for (uint32_t i = 0; i < cap; ++i) {
      if (!mEvents[i].active)
         return &mEvents[i];
   }
   return nullptr; // an event dropped from a full pool is inaudible
}

// One crackle, or one sizzle -- the same call, because the measurement says
// they are the same event held open for different lengths of time.
//
// The train is the part that is easy to miss. `bursts.py` finds 1.97 times as
// many intervals below 10 ms as a Poisson process allows while finding exactly
// as many below 50 ms, so an arrival is not one impulse: it is two or three,
// inside ten milliseconds, and then nothing. They share a pan and a colour
// because they are one pocket of gas leaving one split in one piece of wood.
void FireEngine::spawnCrackle(float envLevel, float fire, bool sizzle) {
   const float sr = static_cast<float>(mSampleRate);

   // Pulses in this train. `Burst` is a mean rather than a count: the fraction
   // decides how often the extra one happens.
   const float b = clampf(mP.burst, 1.0f, 4.0f);
   int n = static_cast<int>(b);
   if (mRng.uniform() < b - static_cast<float>(n))
      ++n;
   n = clampi(n, 1, 6);

   const float pan = clampf(mRng.white() * clampf(mP.width, 0.0f, 1.0f), -1.0f, 1.0f);
   const float panL = std::sqrt(0.5f * (1.0f - pan));
   const float panR = std::sqrt(0.5f * (1.0f + pan));

   const float toneHz = clampf(sizzle ? mP.sizzleToneHz : mP.crackleToneHz, 200.0f, 0.45f * sr);
   const float decaySec = std::max(0.0002f, sizzle ? mP.sizzleDecaySec : mP.crackleDecaySec);
   const float layerGain = sizzle ? mP.sizzleGain : mP.crackleGain;

   // One event's tone wanders around the layer's setting. Half an octave, which
   // is about what the spread of the references' event-triggered centroids
   // comes to once the recordings' own differences are taken out.
   const float toneJitter = std::exp2(mRng.gaussian() * 0.5f);
   const float fc = clampf(toneHz * toneJitter, 150.0f, 0.45f * sr);

   // n pulses sharing one arrival's energy, so Burst changes the texture and
   // not the layer's level.
   const float share = 1.0f / std::sqrt(static_cast<float>(n));
   const float level = envLevel * layerGain * kEventGain * mDistanceLevel * share * fire *
                       logNormalDb(mRng, clampf(mP.crackleSpreadDb, 0.0f, 18.0f));

   // -60 dB in six times the -10 dB time the references were measured at.
   const float dec = decayCoef(6.0f * decaySec * mDistanceSmear, sr);
   // Snap sets how much of the energy is in the front. At the top the event is
   // an impulse with the decay hung off it; lower, the rise is half the decay
   // and the event reads as a thud.
   const float snap = clampf(mP.snap, 0.0f, 1.0f);
   const float attackSamples = std::max(1.0f, (1.0f - snap) * 0.5f * decaySec * sr);

   // The window inside which the rest of the train falls: the measured excess
   // is entirely below 10 ms.
   const float trainSamples = 0.010f * sr * mDistanceSmear;

   for (int k = 0; k < n; ++k) {
      FireEvent *slot = allocateEvent();
      if (!slot)
         break;
      FireEvent &e = *slot;
      e.active = true;
      e.rng.seed(mRng.next() | 1u);
      e.delaySamples = k == 0 ? 0 : static_cast<int>(mRng.uniform() * trainSamples);

      e.amp = 0.0f;
      e.peak = level * (0.6f + 0.8f * mRng.uniform());
      e.rising = true;
      e.attackInc = e.peak / attackSamples;
      e.decay = dec;

      e.band.reset();
      e.band.setCutoff(fc, resonanceFor(0.8f), sr);
      // The low end of a crackle. The measured event spectrum is still within
      // 8 dB of its peak at 250 Hz, so without this a crackle is all fizz and
      // no wood.
      e.body.reset();
      e.body.setCutoff(clampf(fc * 0.30f, 80.0f, 0.45f * sr), resonanceFor(1.0f), sr);
      e.bodyMix = clampf(mP.crackleBody, 0.0f, 1.0f) * 0.8f;

      if (sizzle && mP.steam > 0.001f) {
         // Steam leaving a split in the wood whistles. A narrow band, placed
         // high and jittered per event, because no two splits are the same
         // size.
         e.steam.reset();
         e.steam.setCutoff(clampf(fc * std::exp2(0.5f + mRng.white()), 500.0f, 0.45f * sr),
                           resonanceFor(9.0f), sr);
         e.steamMix = clampf(mP.steam, 0.0f, 1.0f);
      } else {
         e.steamMix = 0.0f;
      }

      e.panL = panL;
      e.panR = panR;
   }
   ++mCrackleCounter;
}

// One log giving way. No train, no body band and no steam: the measured
// population lives in 80-300 Hz and has no top end at all, which is exactly
// what distinguishes it from a loud crackle leaking downwards.
void FireEngine::spawnSettle(float envLevel, float fire) {
   FireEvent *slot = allocateEvent();
   if (!slot)
      return;
   const float sr = static_cast<float>(mSampleRate);
   FireEvent &e = *slot;

   e.active = true;
   e.rng.seed(mRng.next() | 1u);
   e.delaySamples = 0;

   const float pan = clampf(mRng.white() * clampf(mP.width, 0.0f, 1.0f) * 0.5f, -1.0f, 1.0f);
   e.panL = std::sqrt(0.5f * (1.0f - pan));
   e.panR = std::sqrt(0.5f * (1.0f + pan));

   const float fc = clampf(mP.settleToneHz * std::exp2(mRng.gaussian() * 0.35f), 40.0f,
                           0.45f * sr);
   const float decaySec = std::max(0.001f, mP.settleDecaySec);

   e.amp = 0.0f;
   e.peak = envLevel * mP.settleGain * kEventGain * mDistanceLevel * fire *
            logNormalDb(mRng, 5.0f);
   e.rising = true;
   e.attackInc = e.peak / std::max(1.0f, 0.0008f * sr);
   e.decay = decayCoef(6.0f * decaySec * mDistanceSmear, sr);

   e.band.reset();
   e.band.setCutoff(fc, resonanceFor(2.0f), sr);
   e.bodyMix = 0.0f;
   e.steamMix = 0.0f;
}

// One held note, per sample: its envelope, its roar, its draught and the two
// clocks that decide when the next crackle and the next settle happen. All in
// one loop, because a clock has to see the envelope of the sample it spawns
// into -- and, here, the flare as well.
void FireEngine::processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples) {
   const float sr = static_cast<float>(mSampleRate);

   const float velLevel = 1.0f + mP.velToLevel * (v.velocity - 0.5f) * 1.8f;
   // A bigger fire is more of everything: more events, and a roar with more
   // weight in it. One control, because that is what a fire being fed does.
   const float fire = clampf(1.0f + mP.velToFire * (v.velocity - 0.5f) * 1.6f, 0.1f, 2.0f);

   // The roar's L/R correlation is 1 - Roar Width.
   const float wide = clampf(mP.roarWidth, 0.0f, 1.0f);
   const float cCommon = std::sqrt(1.0f - wide);
   const float cIndep = std::sqrt(wide);

   v.hpL.setCutoff(mBedHpHz, sr);
   v.hpR.setCutoff(mBedHpHz, sr);
   v.lpL.setCutoff(mBedLpHz, sr);
   v.lpR.setCutoff(mBedLpHz, sr);

   const float bedLevel = mP.roarGain * mBedNorm * velLevel;
   // The roar's level as it leaves the plugin, which is what the draught and
   // anything else measured against the roar has to be scaled by.
   const float roarOut = mP.roarGain * velLevel;
   const float flare = clampf(mP.flare, 0.0f, 1.0f) / kFlareCvRef;
   const float draught = clampf(mP.draught, 0.0f, 1.0f);

   // The flare walk: a one-pole lowpass on white noise, normalised to unit
   // variance so that Flare means a depth rather than a filter setting. The std
   // of a one-pole LP driven by uniform noise of variance 1/3 is
   // sqrt(coef / (2 - coef) / 3).
   const float flareCoef = onePoleCoef(1.0f / (6.2831853f * clampf(mP.flareRateHz, 0.02f, 40.0f)),
                                       sr / static_cast<float>(kModInterval));
   const float flareNorm = 1.0f / std::sqrt(std::max(1.0e-6f, flareCoef / (2.0f - flareCoef) / 3.0f));
   v.flareCommon.setCoef(flareCoef);
   for (int b = 0; b < kNumBedBands; ++b)
      v.flareBand[b].setCoef(flareCoef);

   // The measured split: one walk for the whole fire at sqrt(0.85), one per
   // band at sqrt(0.15). Any two bands then correlate at 0.85, which is what
   // the library measures.
   const float wCommon = std::sqrt(kFlareCommonR);
   const float wIndep = std::sqrt(1.0f - kFlareCommonR);

   // Event intervals. Distance multiplies the rate; velocity-as-fire does too.
   const float crackleRate = clampf(mP.crackleRateHz * mDistanceRate * fire, 0.005f, 4000.0f);
   const float settleRate = clampf(mP.settleRateHz * mDistanceRate * fire, 0.001f, 200.0f);
   const float sap = clampf(mP.sap, 0.0f, 1.0f);

   // How hard the flare drives the crackle rate, and it is derived rather than
   // dialled in. For a Poisson process whose rate is itself modulated, the Fano
   // factor of the count in a window T is 1 + lambda * CV_lambda^2 * tau, with
   // tau the rate's correlation time. The library measures Fano = 3.90 at
   // lambda = 29/s and a flare corner of about 0.55 Hz (tau = 0.29 s), which
   // gives CV_lambda = 0.59 -- and a log-normal rate multiplier of width
   // `flare` has a CV of sqrt(exp(flare^2) - 1), which at the default Flare of
   // 0.55 is 0.594. So the same number sets both, and the measured Fano factor
   // comes out of the engine rather than being fitted into it.
   const float rateSigma = clampf(mP.flare, 0.0f, 1.0f);

   for (uint32_t i = 0; i < numSamples; ++i) {
      const float env = v.env.tick();

      // -------------------------------------------------- the control rate
      if (v.modCounter == 0) {
         const float common = clampf(v.flareCommon.tick(v.rngFlare.white()) * flareNorm, -3.0f, 3.0f);
         v.flareValue = common;
         for (int b = 0; b < kNumBedBands; ++b) {
            const float ind = clampf(v.flareBand[b].tick(v.rngFlare.white()) * flareNorm, -3.0f, 3.0f);
            const float w = wCommon * common + wIndep * ind;
            // One walk, two channels: a fire flares as one object, so the
            // flare is not decorrelated across the stereo image the way the
            // noise underneath it is. Only the small independent part differs,
            // and it does so through the bands' own noise rather than here.
            const float g = clampf(1.0f + flare * kFlareCv[b] * w, 0.02f, 4.0f);
            v.flareGainL[b] = g;
            v.flareGainR[b] = g;
         }
      }
      if (++v.modCounter >= kModInterval)
         v.modCounter = 0;

      // ---------------------------------------------------------- the roar
      //
      // Each band gets its own noise. That is not an optimisation to undo: it
      // is what makes the bank's summed response predictable, because
      // independent drives mean the bands' energies add while a shared drive
      // makes them add coherently and the cross terms are large between
      // neighbours that overlap by design.
      float bedL = 0.0f, bedR = 0.0f;
      for (int b = 0; b < kNumBedBands; ++b) {
         const float g = mBedGain[b];
         // Common plus independent at sqrt weights: the variance is the same
         // whatever Roar Width is, so it changes the image and not the level,
         // and the resulting L/R correlation is 1 - roarWidth.
         const float common = v.rngCommon.white() * cCommon;
         const float nL = common + v.rngL.white() * cIndep;
         const float nR = common + v.rngR.white() * cIndep;
         bedL += v.bandL[b].bandpassNormalised(nL) * g * v.flareGainL[b];
         bedR += v.bandR[b].bandpassNormalised(nR) * g * v.flareGainR[b];
      }

      bedL = v.lpL.tick(v.hpL.tick(bedL)) * bedLevel;
      bedR = v.lpR.tick(v.hpR.tick(bedR)) * bedLevel;

      // ------------------------------------------------------- the draught
      //
      // Air being pulled into the fire. Steadier than the roar on purpose --
      // it carries only the common part of the flare, because a draught is one
      // flow and does not surge band by band.
      if (draught > 0.001f) {
         // Scaled by the roar's *output* level, not by bedLevel: the latter
         // carries mBedNorm, which is the factor that turns the raw filterbank
         // sum into unit RMS and is nothing to do with how loud the fire is.
         const float dg = draught * 0.45f * roarOut * mDraughtNorm *
                          clampf(1.0f + 0.4f * v.flareValue, 0.1f, 3.0f);
         bedL += v.draughtL.tick(v.rngL.white()) * dg;
         bedR += v.draughtR.tick(v.rngR.white()) * dg;
      }

      outL[i] += bedL * env;
      outR[i] += bedR * env;

      // ------------------------------------------------------- the events
      //
      // A Poisson process at a rate that is itself wandering: the interval is
      // drawn afresh each time from an exponential, and the rate it is drawn
      // at follows the flare. Between them they reproduce the measured Fano
      // factor of 3.9 without anything here being bursty.
      if (env > 1.0e-4f) {
         const float rateMul =
            std::exp(rateSigma * v.flareValue - 0.5f * rateSigma * rateSigma);
         v.crackleTimer -= 1.0;
         while (v.crackleTimer <= 0.0) {
            const bool sizzle = mRng.uniform() < sap;
            const float g = sizzle ? mP.sizzleGain : mP.crackleGain;
            if (g > 1.0e-5f)
               spawnCrackle(env, fire, sizzle);
            v.crackleTimer += std::max(
               1.0, static_cast<double>(mRng.exponential(crackleRate * rateMul)) * sr);
         }
         v.settleTimer -= 1.0;
         while (v.settleTimer <= 0.0) {
            if (mP.settleGain > 1.0e-5f)
               spawnSettle(env, fire);
            v.settleTimer +=
               std::max(1.0, static_cast<double>(mRng.exponential(settleRate * rateMul)) * sr);
         }
      }
   }

   if (v.finished())
      v.active = false;
}

void FireEngine::processEvents(float *outL, float *outR, uint32_t numSamples) {
   for (auto &e : mEvents) {
      if (!e.active)
         continue;
      for (uint32_t i = 0; i < numSamples; ++i) {
         if (e.delaySamples > 0) {
            --e.delaySamples;
            continue;
         }
         if (e.rising) {
            e.amp += e.attackInc;
            if (e.amp >= e.peak) {
               e.amp = e.peak;
               e.rising = false;
            }
         } else {
            e.amp *= e.decay;
            if (e.amp < 1.0e-7f) {
               e.active = false;
               break;
            }
         }
         const float n = e.rng.white();
         float s = e.band.bandpassNormalised(n) * (1.0f - e.bodyMix);
         if (e.bodyMix > 0.0f)
            s += e.body.bandpassNormalised(n) * e.bodyMix;
         if (e.steamMix > 0.0f)
            s += e.steam.bandpassNormalised(n) * e.steamMix;
         const float o = s * e.amp;
         outL[i] += o * e.panL;
         outR[i] += o * e.panR;
      }
   }
}

void FireEngine::processOutputChain(float *outL, float *outR, uint32_t numSamples) {
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

      // Saturate rather than clip. A fire already crackles; it must not also
      // crackle for the wrong reason.
      outL[i] = softClip(l * mP.gain);
      outR[i] = softClip(r * mP.gain);
   }
}

void FireEngine::process(float *outL, float *outR, uint32_t numSamples) {
   std::fill(outL, outL + numSamples, 0.0f);
   std::fill(outR, outR + numSamples, 0.0f);

   const float sr = static_cast<float>(mSampleRate);

   for (auto &v : mVoices) {
      if (!v.active)
         continue;
      processVoice(v, outL, outR, numSamples);
   }

   processEvents(outL, outR, numSamples);
   processOutputChain(outL, outR, numSamples);

   // Silence tracking, so the host can sleep when the fire has gone out.
   float peak = 0.0f;
   for (uint32_t i = 0; i < numSamples; ++i)
      peak = std::max(peak, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
   if (peak < 1.0e-6f)
      mSilenceCounter += numSamples / sr;
   else
      mSilenceCounter = 0.0f;
}

bool FireEngine::isSilent() const {
   if (mSilenceCounter < 0.25f)
      return false;
   for (const auto &v : mVoices)
      if (v.active)
         return false;
   for (const auto &e : mEvents)
      if (e.active)
         return false;
   return true;
}

uint32_t FireEngine::activeVoiceCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices)
      if (v.active)
         ++n;
   return n;
}

uint32_t FireEngine::activeEventCount() const {
   uint32_t n = 0;
   for (const auto &e : mEvents)
      if (e.active)
         ++n;
   return n;
}

float FireEngine::tailSeconds() const {
   // The longest thing still to come after the note goes: the release, plus
   // whatever the last sizzle and the space are still doing. Six times the
   // -10 dB decay is the -60 dB point.
   const float events =
      6.0f * std::max(mP.sizzleDecaySec, std::max(mP.crackleDecaySec, mP.settleDecaySec)) + 0.05f;
   const float space = mP.spaceAmount > 0.001f ? 0.3f + mSpace.decaySeconds() : 0.05f;
   return mP.releaseSec + events + space;
}

} // namespace crackleblaze
