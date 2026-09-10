#include "rain_engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

#include "verdalis/dsp/bubble.h"
#include "verdalis/dsp/fastmath.h"

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
   float impactFromDrop; // 0 statistical, 1 derived from the drop that made it
   float bodyHz;         // centre of the surface's own low mode
   float bodySpreadOct;  // how far that mode is scattered per droplet, in octaves
   float bodyDecaySec;   // how long the surface rings
   float bodyLevel;      // its weight, relative to the impact click
   float sloshHz;        // centre of the splash sheet's noise band
   float sloshDecaySec;  // how long it washes out
   float sloshLevel;     // its weight, relative to the impact click
   float sloshChance;    // fraction of droplets that slap at all
};

// Wet surfaces trap an air bubble, so their tone swells in a few milliseconds
// behind the splash; a rigid surface starts ringing the instant it is struck.
//
// The chirp column is the total pitch bend, and it splits the surfaces in two.
// A drop that traps an air bubble bends by more than an octave as the bubble
// shrinks -- see the measurement above kChirpAccel -- while a drop landing on
// something rigid excites a fixed mode of that thing and barely bends at all.
// So water and puddles get the full Minnaert runaway and the hard surfaces
// keep the couple of per cent they always had. What makes the large values
// usable is that the bend is back-loaded into the decaying tail; spent on the
// attack instead, anything past a tenth of an octave sounds like a laser.
//
// The harmonic column is the second bubble mode. Measuring the isolated drops
// in the reference recordings puts a partial at 1.8 to 2.15 times the
// fundamental, 15 to 25 dB below it, on essentially every drop that falls into
// water; it is what a bubble pulsating hard enough to be heard radiates at
// twice its breathing frequency. It belongs to the bubble, so the surfaces that
// do not trap one do not get it.
//
// The four body columns are the surface itself sounding. Every recording of
// rain on something -- a roof, leaves, a canopy, a car -- has more in the low
// mids than a cloud of droplets radiating into air can make, and the fitted
// library was short there by about 2 dB on thirteen presets out of sixteen
// however it was pointed, which is an engine's bias and not a preset's. A
// droplet cannot put energy far below its own resonance; the thing it lands on
// can. So each impact also excites one low mode of the surface, at a frequency
// and ring time that are the surface's own, scattered a little per droplet
// because a roof is not one panel. Water has none: there is nothing rigid to
// ring. The umbrella has the most, being a drumhead. Weighted by Impact, since
// it is the strike that sets it going.
//
// The four slosh columns are a sheet of water thrown sideways across a hard,
// wet, non-absorbent surface: broadband hiss an octave or two above anything
// the droplet itself radiates. The existing splash layer cannot produce it,
// because that noise is filtered by the droplet's own resonator and so lands
// wherever the bubble is and never up here.
//
// Concrete's four numbers are measured, over a close recording of rain on wet
// concrete: 69 % of the energy between 2 and 8 kHz, 26 % above 8 kHz, median
// 5.8 kHz, and a cascade of sub-bursts rather than one wash (see the splat
// cascade constants below).
//
// Wood's and glass's are not measured, and it is worth being plain about why.
// The same measurement run over every other reference we hold finds the
// signature nowhere: window 84 % below 2 kHz with a median of 509 Hz, metal
// 68 % below 2 kHz, car 99 %, roof the only one with real weight up there at
// 52 % in the 2-8 kHz band. But the comparison is confounded by microphone
// distance rather than by surface -- `rain_on_concrete`, a distant recording of
// the very surface that does slosh, also measures 64 % below 2 kHz and a
// 1.5 kHz median. Every recording that shows the signature is a close one, and
// the concrete slosh file is the only close one we have. So the recordings
// cannot say whether wet wood and wet glass slap; they can only say that
// nothing rules it out.
//
// What is set below is therefore physical reasoning, scaled off concrete's
// measured column, and a close recording of either surface should replace it:
//   - wood holds a thick, broken film on a rough and slightly absorbent
//     surface, so it is darker than concrete and throws less;
//   - glass holds a thin, smooth, fast-draining film, so what it throws is
//     brighter, shorter and lighter still.
// Metal is left out deliberately. It is smooth like glass, but its click is
// three times any other surface's and its own ring already occupies the band a
// slosh would land in, and the two references that contain wet metal (`metal`,
// `car`) give the idea no support at all.
//
// Note that `rain_on_concrete`, which the concrete presets are fitted to, is
// the duller and more distant recording described above. Adding the slosh
// therefore moves those presets away from that reference while moving them
// towards what concrete sounds like from a few feet away. See TODO section 2.
const SurfaceProfile kSurfaces[kNumSurfaces] = {
   /* Water    */ {1.00f, 0.55f, 0.50f, 1.20f, 1.30f, 1.00f, 1.00f, 1.00f, 0.11f, 0.00f, 0.0f, 0.0f, 0.00f, 0.00f, 0.0f, 0.00f, 0.00f, 0.00f},
   /* Puddle   */ {1.60f, 0.75f, 0.35f, 1.40f, 1.45f, 0.85f, 1.15f, 1.30f, 0.11f, 0.15f, 0.0f, 0.0f, 0.00f, 0.00f, 0.0f, 0.00f, 0.00f, 0.00f},
   /* Leaves   */ {0.35f, 0.15f, 1.20f, 0.70f, 0.02f, 0.70f, 0.35f, 0.35f, 0.00f, 0.50f, 280.0f, 0.6f, 0.020f, 0.35f, 0.0f, 0.00f, 0.00f, 0.00f},
   /* Wood     */ {0.60f, 0.45f, 1.10f, 0.50f, 0.03f, 0.90f, 0.80f, 0.25f, 0.00f, 0.90f, 300.0f, 0.4f, 0.050f, 0.35f, 4200.0f, 0.028f, 4.20f, 0.070f},
   /* Metal    */ {3.00f, 0.90f, 1.30f, 0.45f, 0.015f, 1.60f, 1.30f, 0.12f, 0.06f, 1.00f, 320.0f, 0.5f, 0.150f, 0.35f, 0.0f, 0.00f, 0.00f, 0.00f},
   /* Glass    */ {1.20f, 0.80f, 1.25f, 0.40f, 0.02f, 1.90f, 1.10f, 0.12f, 0.06f, 1.00f, 450.0f, 0.4f, 0.060f, 0.20f, 6500.0f, 0.018f, 3.20f, 0.055f},
   /* Concrete */ {0.30f, 0.20f, 1.15f, 0.60f, 0.015f, 0.80f, 0.40f, 0.25f, 0.00f, 1.00f, 400.0f, 0.4f, 0.015f, 0.08f, 5000.0f, 0.030f, 6.50f, 0.095f},
   // Fabric: a taut canopy a foot above your head, which is an umbrella or a
   // tent. It is a drumhead, so the impact is the loudest thing about it and
   // carries more weight than on any other surface, but the membrane is lossy
   // and under tension rather than rigid, so what it rings with dies almost at
   // once and has very little pitch to it. Struck from above and radiating
   // straight down, it is also the one surface heard from a few centimetres
   // away rather than across a street.
   /* Fabric   */ {0.40f, 0.22f, 1.45f, 0.65f, 0.02f, 0.85f, 0.50f, 0.18f, 0.00f, 0.90f, 240.0f, 0.4f, 0.040f, 0.60f, 0.0f, 0.00f, 0.00f, 0.00f},
};

// --- The pitch bend of a drop falling into water.
//
// Tracking an isolated drop cycle by cycle, from the zero crossings of the
// tone itself, gives a shape in three parts:
//
//   1. one cycle of downward bend at the attack, 876 -> 730 Hz, about a
//      quarter of an octave in a millisecond, while the splash is loudest;
//   2. a plateau: 730 -> 775 Hz over the next 15 ms, the part of the drop
//      that is within 2 dB of peak. Barely a twentieth of an octave;
//   3. a rise that accelerates as the drop decays: 775 Hz at 20 ms, 1 kHz at
//      55 ms, 2.4 kHz at 130 ms, by which point it is 42 dB down. About 1.7
//      octaves in total, and roughly 95 % of it below -3 dB.
//
// This is why the bend was previously fitted at a tenth of an octave and no
// more. Measured over the loud part of a drop that is exactly what it is, and
// an energy-weighted fit sees nothing else. The rise lives almost entirely in
// the tail, so a model that spends its bend on the plateau has to keep the
// bend tiny or it sounds like a laser -- which was the old conclusion here.
//
// The shape below therefore back-loads the rise instead of front-loading it.
// The per-sample step grows geometrically, so the accumulated bend after a
// fraction f of the sweep is (e^(kf) - 1) / (e^k - 1). Fitting k against the
// 20x-stretched reference gives 0.5 and rejects anything steep: the real drop
// has done a sixth of its bend a quarter of the way through and half of it by
// the middle, so a strongly back-loaded curve hides the whole rise in the part
// nobody hears. Slightly above the fitted value, to keep the plateau.
constexpr float kChirpAccel = 0.6f;
// Where the sweep finishes, as a multiple of the ring time. decayCoef takes
// the time to -60 dB, and the reference has reached its full span by about
// -36 dB, so the bend is done at 0.6 ring times and holds afterwards. This is
// the number that decides whether any of the bend is audible: spread over the
// droplet's whole lifetime (1.6 ring times, about -96 dB) the drop is already
// 36 dB down before a fifth of the sweep has happened.
constexpr float kChirpReachRings = 0.6f;
// The splat cascade, measured on isolated events in the concrete reference:
// six bursts in the first 40 ms about 5 ms apart, each roughly 11 dB under the
// first, with the last of them straggling out towards 300 ms.
// Slosh at 100 % reaches well past what the reference shows, deliberately:
// the measurement is one recording of one pavement, and a longer cascade is
// the difference between rain on stone and rain into a fountain. The measured
// six sits near the middle of the range.
constexpr uint32_t kSloshBurstsMin = 2;
constexpr uint32_t kSloshBurstsMax = 22;
constexpr float kSloshGapSec = 0.005f;
constexpr float kSloshBurstSec = 0.008f;
constexpr float kSloshBurstLevel = 0.29f;

// The attack dip: how far down, and how fast it relaxes back.
constexpr float kChirpDipOct = 0.22f;
constexpr float kChirpDipTauSec = 0.0012f;

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
// How far either side of v / 2R a rigid surface's impacts are scattered, in
// octaves. An octave is a modelling choice and not a measured one: sweeping it
// from half an octave to two moved the per-frame flatness of the rigid-surface
// presets by under 0.01, so the measurements have no opinion on it. It is set
// where it is because contact time plausibly varies by about that much with the
// angle a drop arrives at and how far it flattens, and because collapsing it to
// zero does measurably make every drop tick at the same pitch.
constexpr float kImpactDropSpreadOct = 1.0f;

// The far edge of the rain field, and how fast sound crosses it. Distance at
// 100 % puts the furthest droplets here, which is a street's width away rather
// than a horizon: past that the direct sound of an individual drop is gone and
// what is left is the statistical bed, which is modelled separately.
constexpr float kFieldRadiusM = 30.0f;
constexpr float kSpeedOfSoundMs = 343.0f;

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

// Below this a layer is not computed any more: 1e-7 is -140 dBFS. The click is
// gone within a few milliseconds and the splash soon after, so for most of a
// droplet's life only the bubble is still being computed. Measured on Downpour
// this took the engine from 130 % of realtime to 54 %, and on Tin Roof from 67 %
// to 17 %, with the output identical to below -100 dB.
constexpr float kSilent = 1.0e-7f;

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
   sinTable(); // built now, on this thread, rather than on the first audio call
   mDroplets.assign(kMaxDroplets, Droplet());
   mSpace.prepare(mSampleRate);
   // Unique starting point per instance so stacked copies decorrelate.
   static std::atomic<uint32_t> instanceCounter{0};
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   const uint32_t instanceSeed =
      static_cast<uint32_t>(now) ^ (0x9E3779B9u * (instanceCounter.fetch_add(1) + 1));
   mRng.reseed(instanceSeed);
   mTrickleRng.reseed(instanceSeed ^ 0x5BF03635u);
   mAppliedSeed = 0;
   reset();
}

void RainEngine::reset() {
   for (auto &p : mPockets)
      p.active = false;
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
      v.trickleTimer = 0.0;
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
      mTrickleRng.reseed(rngStateForSeed(mP.seed) ^ 0x5BF03635u);
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
         mTrickleRng.reseed(rngStateForSeed(mP.seed) ^ 0x5BF03635u);
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
   slot->trickleTimer = 0.0;

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
   //
   // Where that frequency comes from depends on what was struck, because the
   // two sources this model is built from disagree and both are right about
   // their own case. Liu, Cheng and Tong draw it at random over the whole range
   // for drops on water, and a splash into a liquid really is that unruly.
   // gtnoble/drip derives it for a rigid surface as v / 2R, the drop's speed
   // over its own diameter, which for the sizes the engine draws lands between
   // roughly 3 and 4 kHz and is far narrower.
   //
   // Both are honoured, chosen per surface, and the difference is audible: four
   // octaves of randomly tuned two-cycle blips several hundred times a second
   // is a fair description of breaking ice, which is exactly how Tin Roof went
   // wrong before it was bounded. Blended in the log domain because frequency
   // is heard that way.
   const float impactRandomHz =
      kImpactMinHz + (kImpactMaxHz - kImpactMinHz) * mRng.uniformPositive();
   const float dropMm = kMedianDropMm * sizeRel;
   // Not a single frequency: v / 2R is a characteristic contact time, and the
   // real one varies with the angle the drop arrives at, how far it flattens
   // and what the surface is made of, none of which the formula carries. So it
   // sets the centre of a spread rather than the answer. The width is measured,
   // not chosen: see kImpactDropSpreadOct.
   const float impactDropHz = vTerm / (0.001f * dropMm) *
                              std::exp2(kImpactDropSpreadOct * mRng.white());
   const float impactBlend = clampv(sp.impactFromDrop, 0.0f, 1.0f);
   const float impactHz =
      clampv(std::exp2((1.0f - impactBlend) * std::log2(std::max(20.0f, impactRandomHz)) +
                       impactBlend * std::log2(std::max(20.0f, impactDropHz))) *
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
   // How long the sound took to arrive. A near droplet is loud, bright and
   // early; a far one is quiet, dull and late, and the three travel together
   // rather than the first two being asserted without the third.
   const float travelSamples = dist * kFieldRadiusM / kSpeedOfSoundMs * mSampleRate;
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

   // --- Surface body: one low mode of the thing struck, scattered per droplet
   // and scaled with the drop's size the way the impact is, since a bigger
   // drop shakes the panel harder. A surface with no body leaves the layer at
   // zero and it costs nothing.
   //
   // The level is an energy ratio against the click, not an amplitude ratio.
   // The click is two cycles and gone in a millisecond; the body rings for
   // tens or hundreds of them, so at equal peak amplitude it carries twenty
   // times the energy or more, and the first version of this, scaled by peak,
   // put the umbrella 18 dB heavy below 200 Hz. sqrt(T_click / T_body) makes
   // bodyLevel mean what it says.
   const float bodyDecaySec = sp.bodyLevel > 0.0f ? sp.bodyDecaySec : 0.0f;
   if (bodyDecaySec > 0.0f) {
      const float bodyHz =
         clampv(sp.bodyHz * std::exp2(sp.bodySpreadOct * clampv(mRng.gaussian() * 0.5f, -1.0f, 1.0f)),
                40.0f, 2000.0f);
      d.bodyPhase = 0.0f;
      d.bodyPhaseInc = bodyHz / mSampleRate;
      d.bodyAmp = amp * click * sp.bodyLevel * std::sqrt(clickDecaySec / bodyDecaySec);
      d.bodyDecay = decayCoef(bodyDecaySec, mSampleRate);
      // A mode does not appear at full swing on its first sample; it is driven
      // up over about one cycle. Switching it on hard gives the sine skirts
      // wide enough to fill the two octaves under it, which is exactly where
      // rain has nothing.
      d.bodyRiseInv = bodyHz / mSampleRate;
      d.bodyHp.reset();
      d.bodyHp.setCutoff(clampv(0.6f * bodyHz, 30.0f, 0.4f * mSampleRate), mSampleRate);
   } else {
      d.bodyAmp = 0.0f;
      d.bodyDecay = 0.0f;
      d.bodyPhaseInc = 0.0f;
      d.bodyRiseInv = 0.0f;
   }

   // --- Slosh: the sheet of water thrown sideways across hard wet stone.
   // Weighted like the body layer, as an energy ratio against the click rather
   // than a peak ratio, since it lasts sixty times longer than the click does.
   // It follows Splash rather than Impact: it is water being thrown, and a
   // preset that asks for a dry surface should not get a wet sheet on it.
   // Not every drop slaps: a splat needs a film of water to land in, and a
   // concrete surface in rain is wet in patches. Weighting by size cannot
   // produce this on its own -- the size draw spans barely a factor of 1.4
   // from the median drop to the largest, so scaling by it moves the loud
   // events and the quiet ones together and the texture stays a wash. A
   // per-droplet chance is what separates them: the same energy delivered by
   // one droplet in eight is eight times the peak over the same average, which
   // is the difference between hearing a slap and hearing hiss.
   // Slosh drives how often a drop slaps as well as how long each slap
   // cascades. Length alone made the control read as a brightness knob: more
   // bursts is more energy, and the character barely moved. How many drops
   // slap at all is what decides whether the texture has slapping in it.
   const float sloshAmt = clampv(mP.slosh, 0.0f, 1.0f);
   const bool slaps = mRng.uniformPositive() < sp.sloshChance * 2.0f * sloshAmt;
   if (slaps && sp.sloshLevel > 0.0f && sp.sloshDecaySec > 0.0f) {
      // Fast, not swelling. A first version of this rose over 6 ms and ran for
      // 65, which measured close to the reference in spectrum and nothing like
      // it in envelope: on the 2-8 kHz band the recording spends 17 % of its
      // time within 10 dB of its loudest, and that version spent 80 %. A wash
      // instead of a slap, which is what rain on a canopy sounds like and not
      // what it does on stone.
      const float sloshRiseSec = 0.0004f;
      const float ratio = clampv(sloshRiseSec / sp.sloshDecaySec, 1.0e-4f, 0.45f);
      const float norm = std::pow(ratio, ratio / (1.0f - ratio)) -
                         std::pow(ratio, 1.0f / (1.0f - ratio));
      // Only the fat drops slap. Size already scales every layer through amp,
      // but this one needs more than its share of it: a splat is a volume of
      // water arriving, and the reference is a handful of loud events over a
      // quiet bed rather than every drop contributing equally. Squaring the
      // size on top of amp is what puts the loud ones far enough above the
      // rest to read as separate slaps.
      const float fat = clampv(sizeRel * sizeRel, 0.09f, 9.0f);
      const float peak = amp * fat * mP.splash * sp.sloshLevel *
                         std::sqrt(clickDecaySec / sp.sloshDecaySec) /
                         (norm > 1.0e-3f ? norm : 1.0f);
      d.sloshAmp = peak;
      d.sloshRise = peak;
      d.sloshSeed = peak;
      // Each burst is short -- it has to die before the next one lands or the
      // cascade fills in and becomes the wash again.
      d.sloshDecay = decayCoef(kSloshBurstSec, mSampleRate);
      d.sloshRiseDecay = decayCoef(sloshRiseSec, mSampleRate);
      const uint32_t bursts = kSloshBurstsMin +
         static_cast<uint32_t>((kSloshBurstsMax - kSloshBurstsMin) * sloshAmt);
      d.sloshLeft = bursts + (mRng.next() % 4u);
      d.sloshNext = static_cast<uint32_t>(kSloshGapSec * mSampleRate);
      // Wide and gentle: a sheet of water is not a resonator. Scattered a
      // little per droplet so a downpour is not one hiss played many times.
      const float hz = clampv(sp.sloshHz * std::exp2(0.22f * mRng.white()),
                              1000.0f, 0.42f * mSampleRate);
      d.sloshBp.reset();
      d.sloshBp.setCutoff(hz, 0.40f, mSampleRate);
   } else {
      d.sloshAmp = 0.0f;
      d.sloshRise = 0.0f;
      d.sloshDecay = 0.0f;
      d.sloshRiseDecay = 0.0f;
      d.sloshLeft = 0;
      d.sloshNext = 0;
      d.sloshSeed = 0.0f;
   }

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

   // --- Chirp: how far this droplet's bubble bends. Drops do not all bend by
   // the same amount, so the setting is the mean of a uniform draw rather than
   // a fixed amount: 2u has mean 1, which leaves Chirp meaning what it means
   // while no two droplets bend alike. The draw happens here, with the other
   // per-droplet draws, so that a fixed Seed keeps meaning the same rain; the
   // bend itself is set up further down, once the droplet's lifetime is known.
   const float chirpOct = mP.chirp * sp.chirpOct * 2.0f * mRng.uniformPositive();

   // The attack dip: one cycle of downward bend while the cavity is still
   // opening. It belongs to the bubble, so only the surfaces that trap one get
   // it, and it is over long before the rise below has gone anywhere.
   const float dipSamples = kChirpDipTauSec * mSampleRate;
   if (hasBubble && dipSamples > 1.0f && sp.chirpOct > 0.05f) {
      d.chirpRate = std::exp2(-kChirpDipOct * mP.chirp / dipSamples);
      d.chirpRelax = std::exp(-1.0f / dipSamples);
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
   // The cascade outlives any single burst: five or so gaps, the last two of
   // them stretched, plus the final burst's own decay. A droplet whose life is
   // sized on one burst dies with most of its splat unplayed.
   // The cascade outlives any single burst, and how far it outlives it is now
   // the user's choice, so the droplet's life has to be sized from the number
   // of bursts actually scheduled rather than from a constant.
   const float sloshSec =
      d.sloshAmp > 0.0f
         ? kSloshGapSec * 1.2f * (static_cast<float>(d.sloshLeft) + 2.0f * 8.0f) +
              kSloshBurstSec * 2.0f
         : 0.0f;
   const float longest =
      std::max(std::max(std::max(ringSec, bodyDecaySec), sloshSec),
               std::max(noiseDecaySec + qRing, clickDecaySec));
   d.lifeMax = static_cast<uint32_t>(clampv(1.6f * longest, 0.001f, 4.0f) * mSampleRate) + 96;
   d.life = 0;
   d.rng.seed(mRng.next());

   // --- Chirp, the rise: the bubble shrinking. The sweep is spread over the
   // droplet's whole audible life rather than over a fixed window, because
   // that is what the reference does -- it is still bending when it passes
   // -42 dB -- and because a window longer than the drop would deliver only
   // the flat first part of the curve and none of the runaway.
   //
   // The per-sample step grows geometrically, so the bend is back-loaded into
   // the decaying tail. The geometric sum is pinned to chirpOct, which leaves
   // kChirpAccel free to move where the bend happens without changing how far
   // it goes.
   const float sweep = tonalDecaySec * kChirpReachRings * mSampleRate;
   if (sweep > 32.0f && chirpOct > 1.0e-4f) {
      const float grow = std::exp(kChirpAccel / sweep);
      d.chirpGrow = grow;
      d.chirpStep = chirpOct * 0.693147181f * (grow - 1.0f) / std::expm1(kChirpAccel);
      // Past the sweep the bubble has gone; the pitch holds where it ended
      // rather than running on into the noise floor.
      d.chirpEnd = static_cast<uint32_t>(sweep);
   } else {
      d.chirpGrow = 1.0f;
      d.chirpStep = 0.0f;
      d.chirpEnd = 0;
   }
   d.startOffset = offset + static_cast<uint32_t>(clampv(travelSamples, 0.0f, 0.5f * mSampleRate));
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

      // --- The trickle: its own Poisson clock, exactly as in RiverFlow, so
      // that the two plugins agree. Independent of the droplet rate on purpose:
      // it is a population of drops landing on one nearby hard surface, not the
      // whole field of rain.
      if (mP.trickleGain > 1.0e-5f) {
         const float dd = clampv(mP.distance, 0.0f, 1.0f);
         const float trickleRate =
            clampv(mP.trickleRateHz * (1.0f + 11.0f * dd * dd) * (0.3f + 0.7f * env),
                   0.005f, 8000.0f);
         v.trickleTimer -= 1.0;
         while (v.trickleTimer <= 0.0) {
            spawnTrickle(env * levelMul);
            const float w = mTrickleRng.exponential(trickleRate) * mSampleRate;
            v.trickleTimer += w < 1.0f ? 1.0f : w;
         }
      }
   }
}

verdalis::Pocket *RainEngine::allocatePocket() {
   for (uint32_t i = 0; i < kMaxPockets; ++i)
      if (!mPockets[i].active)
         return &mPockets[i];
   return nullptr; // a dropped pocket from a full pool is inaudible
}

// One drop landing on a hard surface. The generator is the suite's -- the same
// one RiverFlow's Trickle uses, in verdalis/dsp/pocket.h -- so these settings
// give the same sound in both plugins. Only the distance treatment is this
// plugin's, and it matches RiverFlow's too: more events, each smaller, with
// their edges smeared by the air they crossed.
void RainEngine::spawnTrickle(float envLevel) {
   verdalis::Pocket *slot = allocatePocket();
   if (!slot)
      return;

   const float d = clampv(mP.distance, 0.0f, 1.0f);
   const float rateMul = 1.0f + 11.0f * d * d;

   verdalis::TrickleSpec spec;
   spec.sizeMm = mP.trickleSizeMm;
   spec.spreadOct = mP.trickleSpreadOct;
   spec.decaySec = mP.trickleDecaySec;
   spec.impact = mP.trickleImpact;
   spec.stoneToneHz = mP.stoneToneHz;
   spec.splash = mP.trickleSplash;
   spec.width = mP.width;
   spec.levelSigma = 0.4f; // narrow: spray is uniform where trapped pockets are not
   spec.smear = 1.0f + 2.5f * d * d;

   // n events at 1/sqrt(n) each, so distance changes the texture and not the
   // level. kEventGain of 4 is RiverFlow's: an event's peak at 0 dB is four
   // times the bed's RMS at unity, which is where the measured prominences sit.
   const float levelBase = envLevel * mP.trickleGain * 4.0f / std::sqrt(rateMul);
   verdalis::spawnTricklePocket(*slot, spec, mTrickleRng, mSampleRate, levelBase);
}

void RainEngine::processPockets(float *outL, float *outR, uint32_t numSamples) {
   verdalis::processPocketPool(mPockets, kMaxPockets, outL, outR, numSamples);
}

void RainEngine::processDroplets(float *outL, float *outR, uint32_t numSamples) {
   for (uint32_t di = 0; di < mDropletLimit; ++di) {
      Droplet &d = mDroplets[di];
      if (!d.active)
         continue;

      // startOffset is when the droplet is heard, not when it fell, so it can
      // reach past the end of this block: sound takes a third of a second to
      // cross a hundred metres and the field is that sort of size.
      if (d.startOffset >= numSamples) {
         d.startOffset -= numSamples;
         continue;
      }
      uint32_t i = d.startOffset;
      d.startOffset = 0;
      const uint32_t fadeStart = d.lifeMax > 64 ? d.lifeMax - 64 : 0;

      for (; i < numSamples; ++i) {
         // Bubble and splash are radiated by the droplet itself, so both go
         // through its radiation rolloff. The impact and the body are not: they
         // are the surface being struck, and their pitch has nothing to do with
         // the bubble's, so rolling them off below the bubble would silence the
         // low ticks that land under a fine drop. Air absorption applies to all
         // of it. Each layer is skipped once it has decayed below kSilent.
         float s = 0.0f;
         if (d.tonalAmp > kSilent) {
            s = (d.tonalAmp - d.tonalRise) * sin2piFast(d.phase);
            s += d.harmAmp * sin2piFast(d.harmPhase);
         }
         // The noise draw comes from the droplet's own generator, so it can be
         // skipped outright once the splash is done rather than drawn and
         // discarded to keep the shared sequence aligned.
         if (d.noiseAmp > kSilent || d.resonator.ringing(kSilent))
            s += d.resonator.bandpassNormalised(d.rng.white() * d.noiseAmp);
         s = d.body.tick(s);
         if (d.clickAmp > kSilent)
            s += d.clickAmp * sin2piFast(d.clickPhase);
         if (d.bodyAmp > kSilent) {
            const float ramp = static_cast<float>(d.life) * d.bodyRiseInv;
            s += d.bodyHp.tick(d.bodyAmp * (ramp < 1.0f ? ramp : 1.0f) * sin2piFast(d.bodyPhase));
         }
         // The slosh is the surface being wetted, not the droplet radiating, so
         // like the impact and the body it bypasses the radiation highpass.
         if (d.sloshAmp > kSilent)
            s += d.sloshBp.bandpassNormalised((d.sloshAmp - d.sloshRise) * d.rng.white());
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
         d.bodyPhase += d.bodyPhaseInc;
         if (d.bodyPhase >= 1.0f)
            d.bodyPhase -= 1.0f;

         // The second mode is a mode of the same bubble, so it bends with it.
         // The attack dip relaxes away; the tail rise accelerates.
         float bend = d.chirpRate;
         if (d.life < d.chirpEnd) {
            bend *= 1.0f + d.chirpStep;
            d.chirpStep *= d.chirpGrow;
         }
         d.phaseInc *= bend;
         d.harmPhaseInc *= bend;
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
         d.bodyAmp *= d.bodyDecay;
         d.sloshAmp *= d.sloshDecay;
         d.sloshRise *= d.sloshRiseDecay;
         // Re-trigger: the next secondary droplet lands. Spacing and level are
         // scattered per burst, because a splat that ticks at a fixed interval
         // reads as a machine and not as water.
         if (d.sloshLeft > 0u) {
            if (d.sloshNext > 0u) {
               --d.sloshNext;
            } else {
               --d.sloshLeft;
               const float lvl = kSloshBurstLevel * (0.45f + 1.10f * d.rng.uniformPositive());
               d.sloshAmp = d.sloshSeed * lvl;
               d.sloshRise = d.sloshAmp;
               const float gap = kSloshGapSec * (0.5f + 1.4f * d.rng.uniformPositive());
               d.sloshNext = static_cast<uint32_t>(gap * mSampleRate);
               // The last few come back late and alone: the stragglers run out
               // to about 300 ms in the reference.
               if (d.sloshLeft <= 2u)
                  d.sloshNext *= 8u;
            }
         }

         // A droplet used to run to 1.6x its longest decay so the shared
         // generator stayed in step; about a third of that is spent below
         // -60 dB. With its own generator it can stop the moment every layer
         // is under kSilent and nothing is still ringing.
         if (++d.life >= d.lifeMax) {
            d.active = false;
            break;
         }
         // Tested every 64 samples, not every one: on a dense preset almost no
         // droplet ends this way -- they are short and reach lifeMax first --
         // so a per-sample test is pure overhead on exactly the presets that
         // can least afford it. A 64-sample granularity is 1.3 ms.
         if ((d.life & 63u) == 0u && d.tonalAmp <= kSilent && d.harmAmp <= kSilent &&
             d.noiseAmp <= kSilent && d.clickAmp <= kSilent && d.bodyAmp <= kSilent &&
             d.sloshAmp <= kSilent && !d.resonator.ringing(kSilent)) {
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
   processPockets(outL, outR, numSamples);
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
   const float spaceTail = mP.spaceAmount > 0.001f ? 0.3f + mSpace.decaySeconds() : 0.05f;
   // Plus a second of slack for the longest droplet ring-down.
   return mP.releaseSec + spaceTail + 1.0f;
}

bool RainEngine::isSilent() const {
   if (activeVoiceCount() != 0 || activeDropletCount() != 0)
      return false;
   return mSilenceCounter > static_cast<uint32_t>(tailSeconds() * mSampleRate);
}

} // namespace rainyday
