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
   float sloshHz;        // centre of the band a smack sounds in
   float sloshDecaySec;  // how long its wet body takes to fall 60 dB
   float sloshLevel;     // its weight against the rest of the droplet
   float sloshChance;    // fraction of droplets that smack at all
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
// The four slosh columns are the smack: a drop arriving at terminal velocity in
// the film of water on something hard and non-absorbent. It is a different
// sound from anything above, an octave or two over what a droplet radiates and
// with no pitch at all, and the splash layer cannot make it because that noise
// goes through the droplet's own resonator and lands wherever the bubble is.
//
// It is not synthesised in the droplet. A droplet that smacks starts a voice in
// the pocket pool -- verdalis/dsp/pocket.h, the generator the Trickle layer
// uses -- so there is one model of a drop hitting wet stone in this engine and
// not two.
//
// Concrete's column is measured, on two close recordings of rain on wet
// concrete (tools/analysis/README.md has the method and every number):
//
//   - The band. The click of a strike has its spectral centroid at 8.0 and
//     7.5 kHz in the two recordings, one sigma 0.42 and 0.47 octaves from
//     strike to strike, and the body after it sits in the same place. A band
//     centred on 6.5 kHz, scattered by 0.6 octaves, measures 7.3 to 8.4 kHz
//     through the same analysis. It does not get duller as strikes get quieter:
//     the centroid is within 300 Hz from the faintest strikes to the loudest.
//   - The shape. One click, 0.18 ms wide at -6 dB; a snap 16 dB under it that
//     is gone in 3 ms; a body that takes 44 ms to fall 60 dB in one recording
//     and 81 ms in the other. 54 ms here.
//   - What it is not. Until 1.11.0 this was a cascade: up to twenty-two bursts
//     5 ms apart, each about 11 dB under the first, straggling out to 300 ms.
//     That came from counting envelope peaks on one recording, and it was
//     wrong in the way that matters. Laid over the median of the two hundred
//     loudest strikes, the cascade holds a plateau 18 dB under its own peak for
//     more than 200 ms where a real strike is 25 dB down inside 10 ms, and it
//     has no click at all -- 6 dB down after 0.3 ms where the recordings are
//     9 dB down and already falling. It read as a rattle. What follows a real
//     strike is one more strike or two, smaller, within about 10 ms.
//
// Wood's and glass's are still not measured, for the reason they never were:
// every recording that shows the smack is a close one, and the close ones are
// all concrete. They are concrete's column moved by physical reasoning, and a
// close recording of either should replace them:
//   - wood holds a thick, broken film on a rough and slightly absorbent
//     surface, so it is darker than concrete and throws less;
//   - glass holds a thin, smooth, fast-draining film, so what it throws is
//     brighter, shorter and lighter still.
// Metal is left out deliberately. It is smooth like glass, but its click is
// three times any other surface's and its own ring already occupies the band a
// smack would land in.
const SurfaceProfile kSurfaces[kNumSurfaces] = {
   /* Water    */ {1.00f, 0.55f, 0.50f, 1.20f, 1.30f, 1.00f, 1.00f, 1.00f, 0.11f, 0.00f, 0.0f, 0.0f, 0.00f, 0.00f, 0.0f, 0.00f, 0.00f, 0.00f},
   /* Puddle   */ {1.60f, 0.75f, 0.35f, 1.40f, 1.45f, 0.85f, 1.15f, 1.30f, 0.11f, 0.15f, 0.0f, 0.0f, 0.00f, 0.00f, 0.0f, 0.00f, 0.00f, 0.00f},
   /* Leaves   */ {0.35f, 0.15f, 1.20f, 0.70f, 0.02f, 0.70f, 0.35f, 0.35f, 0.00f, 0.50f, 280.0f, 0.6f, 0.020f, 0.35f, 0.0f, 0.00f, 0.00f, 0.00f},
   /* Wood     */ {0.60f, 0.45f, 1.10f, 0.50f, 0.03f, 0.90f, 0.80f, 0.25f, 0.00f, 0.90f, 300.0f, 0.4f, 0.050f, 0.35f, 5400.0f, 0.050f, 4.20f, 0.070f},
   /* Metal    */ {3.00f, 0.90f, 1.30f, 0.45f, 0.015f, 1.60f, 1.30f, 0.12f, 0.06f, 1.00f, 320.0f, 0.5f, 0.150f, 0.35f, 0.0f, 0.00f, 0.00f, 0.00f},
   /* Glass    */ {1.20f, 0.80f, 1.25f, 0.40f, 0.02f, 1.90f, 1.10f, 0.12f, 0.06f, 1.00f, 450.0f, 0.4f, 0.060f, 0.20f, 8400.0f, 0.032f, 4.80f, 0.055f},
   /* Concrete */ {0.30f, 0.20f, 1.15f, 0.60f, 0.015f, 0.80f, 0.40f, 0.25f, 0.00f, 1.00f, 400.0f, 0.4f, 0.015f, 0.08f, 6500.0f, 0.054f, 6.50f, 0.095f},
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

// --- The smack, as verdalis/dsp/pocket.h is asked for it. The click, the snap,
// the body and how they stand against each other are that file's, measured on
// rain and shared with RiverFlow; these are the numbers that are this plugin's.
//
// How much a droplet's smack spatters. Slosh at 50 % is the one more strike per
// drop that the recordings have; the control runs from a clean single strike to
// a drop that strikes three or four times.
constexpr float kSmackSpatterMin = 0.3f;
constexpr float kSmackSpatterRange = 2.4f;
// A droplet's smack against the droplet itself. Set so that the layer carries
// the energy the cascade it replaces carried in Concrete Alley: the sound
// changed, the balance against the rest of the rain did not.
constexpr float kSmackGain = 0.25f;

// --- The trickle's field. Drops land all over a surface and the listener is
// at one point above it, so loudness follows from where each one landed: with
// drops uniform over the patch, twice as many fall at twice the range and each
// is half as loud. That is a power law, not a bell curve, and it is what the
// recordings have -- the loudest strike in a hundred is 21 dB over the median
// one, where any spread of drop sizes gives a third of that.
//
// The nearest a drop can land, as a fraction of the patch's radius, with the
// listener right at the surface. Distance moves the listener back, and at 100 %
// every drop is as far away as every other.
//
// This and the spread below are set by the top of the distribution, not the
// middle of it. A nearest of 0.04 with 5 dB of spread fits the recordings'
// percentiles as well as these do, but the loudest strike of a 40 s take then
// stands 40 dB over the layer's own RMS where the recording's stands 35 --
// and a preset fitted with that tail only matched because the output clipper
// was flattening it.
constexpr float kTrickleNearest = 0.06f;
// What is left for the drops themselves once range is accounted for: one sigma
// of 4 dB, in octaves of amplitude.
constexpr float kTrickleLevelSigmaOct = 0.664f;
// Holds the layer where 1.10 had it at the same Trickle Level, so that nothing
// mixed against the old trickle has moved: measured over fifteen presets, the
// new drop came out 0.4 dB under the old one.
constexpr float kTrickleTrim = 1.05f;

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
   if (mP.seed != 0) {
      mRng.reseed(rngStateForSeed(mP.seed));
      mTrickleRng.reseed(rngStateForSeed(mP.seed) ^ 0x5BF03635u);
   }
}

void RainEngine::setParams(const EngineParams &p) {
   mP = p;

   if (mP.seed != mAppliedSeed) {
      mAppliedSeed = mP.seed;
      // Seed 0 keeps the per-instance random seed chosen in prepare(), so two
      // instances of the plugin never generate identical rain. Any other value
      // is reproducible and renders identically every time.
      if (mP.seed != 0) {
         mRng.reseed(rngStateForSeed(mP.seed));
         mTrickleRng.reseed(rngStateForSeed(mP.seed) ^ 0x5BF03635u);
      }
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

   // The trickle's nearest drop. A drop at a fraction u of the patch's area is
   // at range sqrt(n^2 + (1 - n^2) u) of its radius and as loud as one over
   // that; the mean of the square over u is 2 ln(1/n) / (1 - n^2), and dividing
   // by its root keeps the layer's level where it is while the nearness moves.
   {
      const float d = clampv(mP.distance, 0.0f, 1.0f);
      // (`near` is a macro in the Windows headers.)
      const float nearest = clampv(std::pow(kTrickleNearest, 1.0f - d), kTrickleNearest, 1.0f);
      mTrickleNearSq = nearest * nearest;
      mTrickleNearNorm =
         nearest < 0.999f ? std::sqrt((1.0f - mTrickleNearSq) / (-2.0f * std::log(nearest)))
                          : 1.0f;
   }

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
      v.env.setParams(mP.attackSec, mP.decaySec, mP.sustain, mP.releaseSec, mSampleRate,
                      mP.attackCurve, mP.decayCurve, mP.releaseCurve);
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
   slot->env.setParams(mP.attackSec, mP.decaySec, mP.sustain, mP.releaseSec, mSampleRate,
                       mP.attackCurve, mP.decayCurve, mP.releaseCurve);
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

   // --- The smack: this drop landed in the film of water on something hard.
   // Not every drop does. A wet pavement is wet in patches, and weighting by
   // size cannot stand in for that -- the size draw spans barely a factor of
   // 1.4 from the median drop to the largest, so it moves the loud events and
   // the quiet ones together and the texture stays a wash. A chance per droplet
   // is what separates them: the same energy from one droplet in eight is eight
   // times the peak over the same average, which is the difference between
   // hearing a smack and hearing hiss. Slosh drives that chance as well as how
   // much each smack spatters, because how many drops smack at all is what
   // decides whether the texture has smacking in it.
   //
   // It follows Splash rather than Impact: it is water being thrown, and a
   // preset that asks for a dry surface should not get a wet sheet on it.
   //
   // The voice is the pocket pool's -- see the surface table. It is started
   // here and placed further down, once this droplet's pan and its travel time
   // are known.
   const float sloshAmt = clampv(mP.slosh, 0.0f, 1.0f);
   const bool slaps = mRng.uniformPositive() < sp.sloshChance * 2.0f * sloshAmt;
   verdalis::Pocket *smack = nullptr;
   uint32_t smackSeed = 0;
   if (slaps && sp.sloshLevel > 0.0f && sp.sloshDecaySec > 0.0f) {
      // Two draws from the rain's generator, and exactly two: the two the
      // cascade took at this point. The smack needs more than that, so it gets
      // a generator of its own seeded from them -- and a fixed Seed then gives
      // the same rain it gave in 1.10, the same drops at the same moments, with
      // only what a smack sounds like changed. Taking the smack's draws straight
      // from mRng moved every drop after the first smack, and the two versions
      // could not be compared by ear on a preset with any drift in it.
      smackSeed = mRng.next();
      smackSeed ^= static_cast<uint32_t>((mRng.white() + 1.0f) * 8388608.0f) * 0x9E3779B9u;
      smack = allocatePocket();
   }
   if (smack) {
      // Only the fat drops smack. Size already scales every layer through amp,
      // but this one needs more than its share: a smack is a volume of water
      // arriving, and the recordings are a handful of loud events over a quiet
      // bed rather than every drop contributing alike.
      const float fat = clampv(sizeRel * sizeRel, 0.09f, 9.0f);
      // The droplet's air filter, as far as a voice without one can follow it.
      // One pole across a band costs the band its level at the centre and
      // tilts what is left downwards, which moves it by about the root of that.
      const float over = sp.sloshHz / airCutoff;
      const float airLoss = 1.0f / std::sqrt(1.0f + over * over);

      verdalis::TrickleSpec spec;
      spec.impact = 1.0f; // all strike: rain on stone traps nothing
      spec.strike = 1.0f;
      spec.clickSec = verdalis::kSmackSnapSec;
      spec.stoneToneHz = sp.sloshHz * std::sqrt(airLoss);
      spec.stoneSpreadOct = verdalis::kSmackSpreadOct;
      // The generator's splash runs three times its decay.
      spec.decaySec = sp.sloshDecaySec / 3.0f;
      spec.splash = verdalis::kSmackBody;
      spec.spatter = kSmackSpatterMin + kSmackSpatterRange * sloshAmt;
      spec.levelSigma = 0.0f; // its level is this droplet's
      spec.width = 0.0f;      // and so is its place
      Rng smackRng(smackSeed);
      verdalis::spawnTricklePocket(*smack, spec, smackRng, mSampleRate,
                                   amp * fat * mP.splash * sp.sloshLevel * kSmackGain * airLoss);
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
   const float longest =
      std::max(std::max(ringSec, bodyDecaySec), std::max(noiseDecaySec + qRing, clickDecaySec));
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

   // The smack is this droplet's: heard where it is and when it is.
   if (smack) {
      smack->panL = d.gainL;
      smack->panR = d.gainR;
      smack->delaySamples = static_cast<int>(d.startOffset);
   }
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

// One drop landing on a hard surface. The generator is the suite's, in
// verdalis/dsp/pocket.h, and so is the drop: a smack, measured on rain, which
// RiverFlow's Trickle asks for as well. What is this plugin's own is where the
// drops are. Rain falls all over a pavement and the listener stands at one
// point on it, which a stream's spray does not have to answer to.
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
   spec.levelSigma = kTrickleLevelSigmaOct;
   spec.smear = 1.0f + 2.5f * d * d;
   verdalis::askForSmack(spec);

   // n events at 1/sqrt(n) each, so distance changes the texture and not the
   // level. kEventGain of 4 is RiverFlow's: an event's peak at 0 dB is four
   // times the bed's RMS at unity, which is where the measured prominences sit.
   const float levelBase = envLevel * mP.trickleGain * 4.0f / std::sqrt(rateMul) * kTrickleTrim;
   verdalis::spawnTricklePocket(*slot, spec, mTrickleRng, mSampleRate, levelBase);

   // Where on the surface it landed, and so how loud it is: see
   // kTrickleNearest. Drawn from the drop's own generator once the drop exists,
   // so the trickle's generator has made exactly the draws it made before there
   // was a field -- and a fixed Seed puts the same drops at the same moments.
   const float range =
      std::sqrt(mTrickleNearSq + (1.0f - mTrickleNearSq) * slot->rng.uniformPositive());
   verdalis::scalePocket(*slot, mTrickleNearNorm / range);
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
             !d.resonator.ringing(kSilent)) {
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
