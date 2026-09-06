#include "thunder_engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

#include "verdalis/dsp/fastmath.h"

namespace thunderclap {

namespace {

// ------------------------------------------------------------------ the model
//
// Thunder is the sound of a lightning channel: several kilometres of it, and
// every metre a separate source. The channel heats and expands in a few
// microseconds, which launches a shock wave from all of it at once, and the
// listener then hears each piece when its own shock arrives -- the near pieces
// first and loud, the far pieces late, quiet and dull, because the air soaks up
// the high end over distance. A crooked channel has stretches side-on to the
// listener whose shocks arrive together and pile into a clap, and stretches
// end-on whose shocks arrive spread out and quiet. That, and the return strokes
// that re-light the same channel a few dozen milliseconds apart, is the whole
// structure of a thunder, and it is all geometry.
//
// So the engine grows a channel, works out for every element of it when and
// how its shock arrives, and plays them. Few (1969) and Ribner and Roy (1982)
// are the two papers this follows: the channel as a tortuous line of N-wave
// sources, the clap as the caustic of the arrival times, and the N-wave
// lengthening as it travels.

constexpr float kSpeedOfSoundMs = 343.0f;

// The reverb tank's loop highpass. Below Space::kDefaultLoopHighpassHz, which
// suits rain; thunder carries real energy under 35 Hz and must keep it.
constexpr float kSpaceLoopHighpassHz = 16.0f;

// The coherence length of the channel: the scale on which it is straight
// enough to radiate as one source. An element longer than this stands for
// several such sources adding incoherently, so its amplitude goes as the
// square root of its length and the power per metre of channel does not
// depend on how finely the channel was cut up.
constexpr float kCoherenceM = 10.0f;

// No element is ever closer than this. A strike under 100 m is a lightning
// strike on the listener, and the 1/r law has nothing sensible to say there.
constexpr float kMinRangeM = 60.0f;

// Duration of an N-wave that has travelled one kilometre, at Weight 50 %, and
// how it lengthens with further travel. A weak shock's duration grows with a
// small power of the distance; a quarter is the textbook figure, and the
// spectra of the reference recordings sit at 80 to 160 Hz for a close strike
// and 20 to 60 Hz for a distant one, which is what these two numbers give.
constexpr float kNwaveBaseSec = 0.0045f;
constexpr float kNwaveRangeExp = 0.3f;

// The channel inside the cloud radiates longer waves than the stroke below it.
// Kappus and Vernon, and Holmes before them, put intracloud thunder's peak near
// 10 Hz against 50 Hz for a cloud-to-ground stroke: five times the wavelength.
// So an element's N-wave lengthens with its height, from nothing at the ground
// to this factor at the cloud base and above, and its crackle fades by the same
// factor, because that is where the deep late swell of a thunder comes from --
// the part that arrives ten seconds after the crack and shakes the floor.
constexpr float kCloudWeight = 2.0f;
constexpr float kCloudWeightStart = 0.35f; // of the cloud height, where it begins
constexpr float kCloudWeightEnd = 0.95f;   // where it has reached the full factor
// And it radiates harder. Kappus and Vernon found the first clap the loudest
// only half the time, and in the recording City Thunder is set against, the
// swell that arrives ten seconds after the crack is the loudest thing in it.
// This is the one number in the model that is a fit and not a law.
constexpr float kCloudGain = 1.5f;
constexpr float kNwaveMinSec = 0.0015f;
constexpr float kNwaveMaxSec = 0.25f;

// Atmospheric absorption: about 5 dB per kilometre at 1 kHz in damp air,
// growing with frequency to the power 1.3, which is what ISO 9613 comes to
// between 1 and 4 kHz at 20 degrees and 70 % humidity (4.7, 10 and 24 dB/km at
// 1, 2 and 4 kHz). The per-shock filter's three poles sit where that loss
// reaches 6, 12 and 24 dB (see Shock).
constexpr float kAbsorbDbPerKm1k = 5.0f;
constexpr float kAbsorbExp = 1.3f;
constexpr float kAbsorbCornerDb = 6.0f;
constexpr float kAbsorbPoleRatio[3] = {1.0f, 2.0f, 4.0f}; // dB at each pole, over the corner

// The crackle at a shock front: how much of the wave the noise burst covers,
// and how loud it is against the wave, both at Crack 100 %. It goes through
// the same air as the wave, so it is what a close strike has and a distant one
// has lost. Measured against the close recordings, the tearing of a strike a
// few hundred metres off carries as much energy above a kilohertz as the wave
// carries below it, and a clean N-wave, whose spectrum falls at 6 dB/oct, is
// 20 dB short of that.
constexpr float kCrackleMinFrac = 0.2f;
constexpr float kCrackleFracPerCrack = 0.6f;
constexpr float kCrackleLevel = 5.0f;

// How the element budget is spread along the channel. Elements are dealt out
// in proportion to length over distance, so the near channel -- the part that
// is heard as separate shocks -- is cut finely, and the far channel, which is
// heard as a wash whatever its detail, coarsely.
constexpr float kBudgetRangeExp = 1.0f;

// The rumble is a band, not a lowpass: its highpass sits this far below its
// lowpass corner. A close thunder has less under 40 Hz than it has at 100.
constexpr float kRumbleBandRatio = 0.25f;

// Every flash is normalised so that the loudest tenth of a second carries this
// much sum of amp^2 x duration. Distance then changes what a thunder sounds
// like without deciding whether it is audible at all; a mild loss per
// kilometre on top keeps far ones further back.
constexpr float kFlashPower = 0.006f;
constexpr float kNormWindowSec = 0.1f;
constexpr float kDistanceLossPerKm = 0.1f;

// How much quieter a side branch is than the main channel, and how much of the
// element budget the branches share between them.
constexpr float kBranchGain = 0.35f;
constexpr float kCloudBranchGain = 0.6f;
constexpr float kBranchBudget = 0.35f;

// Loop gain of the echo feedback: what is left of the echoes' sum after it has
// gone round once more. Set by the loop and not by Echo Level, so the
// landscape's decay is a property of the landscape and the level only says how
// much of it is heard; at 0.4 the loop loses 8 dB a pass and is gone in five
// or six, which over reflectors a second or two away is a tail of some
// seconds, turning to rumble as each pass takes more top off.
constexpr float kEchoLoopGain = 0.4f;
constexpr float kEchoFeedbackMax = 4.0f;

// The rumble integrates arriving shock energy over this long, and is scaled so
// that at Rumble 50 % it sits at about the same power as the shocks it follows.
constexpr float kRumbleTauSec = 0.2f;
constexpr float kRumbleExcite = 1.0f / (3.0f * kRumbleTauSec);
constexpr float kRumbleGain = 1.6f;
constexpr float kRumbleHighpassMinHz = 20.0f;

// The blast pulse, which Impact fades in. Every element of the channel
// radiates its own N-wave, but the near section of a return stroke also
// expands as one body: a few hundred metres of channel, heated in
// microseconds, pushes on the air once. What that sends out is not an N-wave
// but a blast -- a near-instant jump to peak overpressure, a decay back
// through zero, and a longer, shallower negative phase. Friedlander's
// waveform, p(t) = P (1 - t/T) exp(-t/T), is the standard description of it,
// and its spectrum |P(w)| = P w / (w^2 + 1/T^2) peaks at 1/(2 pi T).
//
// This is the part of a close thunder that lands as a slam rather than a tear.
// It fills the 40 to 150 Hz that the reference recordings put at the top of
// the spectrum through the first 300 ms -- a band the elements' own N-waves,
// each one short and each one arriving at its own time, cannot fill between
// them however many there are.
//
// T at one kilometre, and how it stretches further out: the same weak-shock
// lengthening the N-waves get, so a distant blast is lower and slower as well
// as quieter. 2.6 ms puts the peak at 61 Hz.
constexpr float kBlastTauSec = 0.0026f;
constexpr float kBlastTauExp = 0.3f;
constexpr float kBlastTauMin = 0.0008f;
constexpr float kBlastTauMax = 0.05f;
// Peak overpressure of one pulse at Impact 100 %, against the arrival it is
// taken from.
constexpr float kBlastGain = 12.0f;
// Only arrivals inside this much of the flash's start are candidates: the
// blast belongs to the return stroke, not to whatever the cloud does ten
// seconds later. The window is cut into as many slices as there are pulses,
// and the loudest arrival in each slice gets one, which spreads the energy
// over the onset instead of piling it on a single sample.
constexpr float kBlastWindowSec = 0.35f;
// A slice whose loudest arrival is quieter than this much of the best one in
// the window does not get a pulse; without it the tail slices fire on
// whatever noise floor the channel left there.
constexpr float kBlastSliceFloor = 0.12f;
// How long the negative phase is allowed to run before the pulse is dropped.
constexpr float kBlastTailTaus = 12.0f;
// The blast drives the rumble like any other shock, so the slam is followed by
// the swell rather than standing on its own.
constexpr float kBlastRumbleExcite = 0.5f;

// Internal reference level, applied with Output Gain so a matched preset lands
// in the middle of the gain range.
constexpr float kEngineMakeup = 2.0f; // +6 dB

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

inline float smoothstep(float x) {
   x = clampv(x, 0.0f, 1.0f);
   return x * x * (3.0f - 2.0f * x);
}

// Cheap per-element, per-stroke hash for the small level differences between
// strokes, so a repeated channel does not repeat exactly.
inline float strokeJitter(uint32_t element, uint32_t stroke) {
   uint32_t h = element * 2654435761u ^ (stroke + 1) * 0x9E3779B9u;
   h ^= h >> 15;
   h *= 0x85EBCA6Bu;
   h ^= h >> 13;
   return 0.8f + 0.4f * ((h >> 8) * (1.0f / 16777216.0f));
}

} // namespace

// Seed 0 is the "always different" setting and has no fixed mapping; every
// other value maps here, and only here, so that reset() and setParams() cannot
// disagree about what a given Seed means.
static uint32_t rngStateForSeed(int seed) {
   return 0x9E3779B9u * static_cast<uint32_t>(seed) + 17u;
}

void ThunderEngine::prepare(double sampleRate, uint32_t /*maxBlockSize*/) {
   mSampleRate = static_cast<float>(sampleRate);
   mShocks.assign(kMaxShocks, Shock());
   for (auto &f : mFlashes) {
      f.arrivals.assign(kMaxShocks, Arrival{});
      f.active = false;
   }
   mEchoLine.allocate(static_cast<size_t>(kMaxEchoSec * mSampleRate) + 8);
   // Thunder lives an octave below rain, so the tank's loop highpass has to sit
   // lower than the shared default or it takes the bottom off the tail.
   mSpace.prepare(mSampleRate, kSpaceLoopHighpassHz);
   mCompressor.prepare(mSampleRate);
   mRumbleDecay = std::exp(-1.0f / (kRumbleTauSec * mSampleRate));
   // Unique starting point per instance so stacked copies decorrelate.
   static std::atomic<uint32_t> instanceCounter{0};
   const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
   const uint32_t instance = instanceCounter.fetch_add(1) + 1;
   mRng.reseed(static_cast<uint32_t>(now) ^ (0x9E3779B9u * instance));
   mNoiseRng.reseed(static_cast<uint32_t>(now >> 7) ^ (0x85EBCA6Bu * instance));
   mAppliedSeed = 0;
   reset();
}

void ThunderEngine::reset() {
   for (auto &v : mVoices) {
      v.active = false;
      v.held = false;
      v.noteId = -1;
      v.env.reset();
      v.stormTimer = 0.0;
      v.flashesLit = 0;
   }
   for (auto &f : mFlashes) {
      f.active = false;
      f.count = 0;
   }
   for (auto &s : mShocks)
      s.active = false;
   mShockCursor = 0;
   mLastKey = 60;
   mRumbleEnergy = 0.0f;
   mRumbleAirHz = 300.0f;
   mRumbleLpL.reset();
   mRumbleLpR.reset();
   mRumbleHpL.reset();
   mRumbleHpR.reset();
   mDriftState = 0.0f;
   mModCounter = 0;
   mEchoLine.clear();
   for (auto &lp : mEchoLp)
      lp.reset();
   mFilterL.reset();
   mFilterR.reset();
   mHighpassL.reset();
   mHighpassR.reset();
   mSpace.clear();
   mCompressor.reset();
   mSilenceCounter = 0;

   // A non-zero Seed promises the same thunder every time, so starting over has
   // to start the sequence over too. Seed 0 deliberately keeps running, which
   // is what makes it the setting that never repeats.
   if (mP.seed != 0) {
      mRng.reseed(rngStateForSeed(mP.seed));
      mNoiseRng.reseed(rngStateForSeed(mP.seed) ^ 0x7F4A7C15u);
   }

   // Where the reflectors stand is part of the place, not of any one flash, so
   // it is drawn once here from its own generator: the same for every flash of
   // an instance, and the same for every instance with the same Seed.
   Rng placeRng(mP.seed != 0 ? rngStateForSeed(mP.seed) ^ 0x5bd1e995u : mRng.next());
   for (int k = 0; k < kMaxEchoes; ++k) {
      mEchoFrac[k] = 0.15f + 0.85f * placeRng.uniform();
      mEchoAngle[k] = placeRng.white();
   }
   std::sort(mEchoFrac, mEchoFrac + kMaxEchoes);
   updateEchoes();
}

void ThunderEngine::setParams(const EngineParams &p) {
   const bool seedChanged = p.seed != mAppliedSeed;
   mP = p;

   if (seedChanged) {
      mAppliedSeed = mP.seed;
      // Seed 0 keeps the per-instance random seed chosen in prepare(), so two
      // instances of the plugin never generate identical thunder. Any other
      // value is reproducible and renders identically every time.
      if (mP.seed != 0) {
         mRng.reseed(rngStateForSeed(mP.seed));
         mNoiseRng.reseed(rngStateForSeed(mP.seed) ^ 0x7F4A7C15u);
      }
      Rng placeRng(mP.seed != 0 ? rngStateForSeed(mP.seed) ^ 0x5bd1e995u : mRng.next());
      for (int k = 0; k < kMaxEchoes; ++k) {
         mEchoFrac[k] = 0.15f + 0.85f * placeRng.uniform();
         mEchoAngle[k] = placeRng.white();
      }
      std::sort(mEchoFrac, mEchoFrac + kMaxEchoes);
   }

   // Below about 25 Hz the highpass is doing nothing audible, so it steps aside
   // rather than spending two poles per sample on every preset that leaves it
   // where it starts.
   mHighpassBypass = mP.highpassHz <= 25.0f;
   if (!mHighpassBypass) {
      const float hp = clampv(mP.highpassHz, 20.0f, 0.45f * mSampleRate);
      mHighpassL.setCutoff(hp, mSampleRate);
      mHighpassR.setCutoff(hp, mSampleRate);
   }

   mSpace.setSize(mP.spaceSize);
   mSpace.setDamping(mP.spaceDamping);
   mCompressor.setParams(mP.compress, mP.compAttackSec, mP.compReleaseSec);

   // Rumble: two decorrelated noises mixed to the requested width (a^2 + b^2 = 1,
   // so the channel correlation is cos(width * pi/2) with no level change).
   const float widthAngle = mP.rumbleWidth * 0.785398163f;
   mRumbleMixA = std::cos(widthAngle);
   mRumbleMixB = std::sin(widthAngle);

   const float modRate = mSampleRate / static_cast<float>(kModInterval);
   mDriftCoef = clampv(onePoleCoef(2.5f, modRate), 1.0e-5f, 1.0f);
   mDriftNorm = std::sqrt((2.0f - mDriftCoef) / mDriftCoef);

   updateEchoes();
   updateFilters();

   for (auto &v : mVoices)
      v.env.setParams(mP.attackSec, 0.01f, 1.0f, mP.releaseSec, mSampleRate);
}

void ThunderEngine::updateEchoes() {
   mEchoActive = clampv(mP.echoCount, 0, kMaxEchoes);
   const size_t cap = mEchoLine.capacity();
   // The sum of the tap gains times the feedback has to stay well under one,
   // or the landscape rings forever.
   float gainSum = 0.0f;
   for (int k = 0; k < kMaxEchoes; ++k) {
      const float sec = clampv(mP.echoSpreadSec * mEchoFrac[k], 0.02f, kMaxEchoSec);
      mEchoDelay[k] = clampv(static_cast<size_t>(sec * mSampleRate), size_t(1), cap);
      // A further reflector is a longer path, so quieter and duller: the extra
      // distance costs level as 1/r and high end by absorption.
      mEchoGain[k] = mP.echoGain / (1.0f + 1.2f * sec);
      if (k < mEchoActive)
         gainSum += mEchoGain[k];
      const float cutoff =
         clampv(1500.0f * std::exp2(-4.5f * mP.echoDamping) / (1.0f + 0.6f * sec), 30.0f,
                0.45f * mSampleRate);
      mEchoLp[k].setCutoff(cutoff, mSampleRate);
      const float pan = clampv(mEchoAngle[k] * mP.width, -1.0f, 1.0f);
      const float angle = (pan + 1.0f) * 0.785398163f;
      mEchoPanL[k] = std::cos(angle) * 1.41421356f;
      mEchoPanR[k] = std::sin(angle) * 1.41421356f;
   }
   mEchoFeedback = gainSum > 1.0e-6f ? std::min(kEchoFeedbackMax, kEchoLoopGain / gainSum) : 0.0f;
}

void ThunderEngine::updateFilters() {
   float cutoff = mP.filterCutoffHz;
   if (mP.filterKeyTrack > 0.0f)
      cutoff *= std::exp2(mP.filterKeyTrack * (mLastKey - 60) / 12.0f);
   mFilterCutoff = clampv(cutoff, 20.0f, 0.49f * mSampleRate);

   // A wide-open lowpass is bypassed outright: no colouring, no CPU.
   mFilterBypass = (mP.filterType == kFilterLowpass && mFilterCutoff >= 0.45f * mSampleRate) ||
                   (mP.filterType == kFilterHighpass && mFilterCutoff <= 21.0f);

   mFilterL.setCutoff(mFilterCutoff, mP.filterReso, mSampleRate);
   mFilterR.setCutoff(mFilterCutoff, mP.filterReso, mSampleRate);

   mFilterWLp = mFilterWBp = mFilterWHp = 0.0f;
   switch (mP.filterType) {
   case kFilterLowpass:
      mFilterWLp = 1.0f;
      break;
   case kFilterBandpass:
      mFilterWBp = mFilterL.k();
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

// ------------------------------------------------------------------- notes

void ThunderEngine::noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId,
                           double velocity) {
   Voice *slot = nullptr;
   int index = -1;
   for (uint32_t i = 0; i < kMaxVoices; ++i) {
      if (!mVoices[i].active) {
         slot = &mVoices[i];
         index = static_cast<int>(i);
         break;
      }
   }
   if (!slot) {
      // Steal the quietest voice, and take its flashes with it.
      float best = 1e9f;
      for (uint32_t i = 0; i < kMaxVoices; ++i) {
         const float l = mVoices[i].env.level();
         if (l < best) {
            best = l;
            slot = &mVoices[i];
            index = static_cast<int>(i);
         }
      }
      for (auto &f : mFlashes)
         if (f.active && f.voice == index)
            f.active = false;
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
   slot->mode = clampv(mP.mode, 0, kNumModes - 1);
   slot->flashesLit = 0;
   slot->env.setParams(mP.attackSec, 0.01f, 1.0f, mP.releaseSec, mSampleRate);
   slot->env.gateOn();
   // The first flash comes at once in every mode; Storm then keeps going.
   slot->stormTimer = 0.0;

   if (key >= 0)
      mLastKey = key;
   updateFilters();
}

void ThunderEngine::noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (auto &v : mVoices) {
      if (v.active && v.held && noteMatches(v, port, channel, key, noteId)) {
         v.held = false;
         // A one-shot plays out whatever the note does; the other two fade what
         // has not arrived yet.
         if (v.mode != kModeOneShot)
            v.env.gateOff();
      }
   }
}

void ThunderEngine::choke(int16_t port, int16_t channel, int16_t key, int32_t noteId) {
   for (uint32_t i = 0; i < kMaxVoices; ++i) {
      Voice &v = mVoices[i];
      if (v.active && noteMatches(v, port, channel, key, noteId)) {
         v.env.kill();
         v.active = false;
         v.held = false;
         for (auto &f : mFlashes)
            if (f.active && f.voice == static_cast<int>(i))
               f.active = false;
      }
   }
}

void ThunderEngine::allSoundOff() {
   for (auto &v : mVoices) {
      v.env.kill();
      v.active = false;
      v.held = false;
      v.flashesLit = 0;
   }
   for (auto &f : mFlashes)
      f.active = false;
   for (auto &s : mShocks)
      s.active = false;
   mRumbleEnergy = 0.0f;
   mEchoLine.clear();
   mSpace.clear();
}

// ------------------------------------------------------------------ pools

Flash *ThunderEngine::allocateFlash() {
   for (auto &f : mFlashes)
      if (!f.active)
         return &f;
   // Full: take over the one furthest along, which has the least left to say.
   Flash *oldest = nullptr;
   for (auto &f : mFlashes)
      if (!oldest || f.clock > oldest->clock)
         oldest = &f;
   if (oldest)
      flashEnded(*oldest);
   return oldest;
}

Shock *ThunderEngine::allocateShock() {
   // Scanning from a rotating cursor means a free slot is normally found within
   // a few steps instead of re-walking the whole pool for every shock.
   for (uint32_t n = 0; n < kMaxShocks; ++n) {
      uint32_t i = mShockCursor + n;
      if (i >= kMaxShocks)
         i -= kMaxShocks;
      Shock &s = mShocks[i];
      if (!s.active) {
         mShockCursor = (i + 1 < kMaxShocks) ? i + 1 : 0;
         return &s;
      }
   }
   // The pool is full. Dropping a shock is inaudible in a dense clap; stealing
   // one would click.
   return nullptr;
}

// ------------------------------------------------------------------ channel

void ThunderEngine::addElements(Flash &f, Vec3 a, Vec3 b, int count, float tortuosity,
                                float branchScale, bool branch, float weightScale,
                                float shadowElev, float shadowWidth, float heightM) {
   const Vec3 seg = {b.x - a.x, b.y - a.y, b.z - a.z};
   const float segLen = std::sqrt(seg.x * seg.x + seg.y * seg.y + seg.z * seg.z);
   if (segLen < 1.0f || count < 1)
      return;
   const Vec3 dir = {seg.x / segLen, seg.y / segLen, seg.z / segLen};
   const float elemLen = segLen / static_cast<float>(count);
   const float alpha1 = kAbsorbDbPerKm1k * std::exp2(2.0f * (mP.air - 0.5f));
   const float focusExp = 2.0f * mP.focus;
   const float jitterM = tortuosity * elemLen * 0.6f;
   const float bendSigma = tortuosity * 0.9f;

   for (int j = 0; j < count; ++j) {
      if (f.count >= kMaxShocks)
         return;
      const float t = (j + 0.5f) / static_cast<float>(count);
      // Where this element is, and which way it points. The meso-tortuosity is
      // in the direction more than in the position: a channel is a jagged line
      // whose pieces face every way, and it is the facing that decides how
      // loud a piece is from here.
      const Vec3 mid = {a.x + seg.x * t + jitterM * mRng.gaussian(),
                        a.y + seg.y * t + jitterM * mRng.gaussian(),
                        a.z + seg.z * t + jitterM * mRng.gaussian()};
      Vec3 edir = {dir.x + bendSigma * mRng.gaussian(), dir.y + bendSigma * mRng.gaussian(),
                   dir.z + bendSigma * mRng.gaussian()};
      const float en = std::sqrt(edir.x * edir.x + edir.y * edir.y + edir.z * edir.z);
      if (en > 1.0e-6f) {
         edir.x /= en;
         edir.y /= en;
         edir.z /= en;
      } else {
         edir = dir;
      }

      float r = std::sqrt(mid.x * mid.x + mid.y * mid.y + mid.z * mid.z);
      if (r < kMinRangeM)
         r = kMinRangeM;
      const Vec3 u = {-mid.x / r, -mid.y / r, -mid.z / r};

      // Directivity: a line element radiates broadside and not off its ends.
      // |dir x u| is the sine of the angle between the element and the line to
      // the listener; Focus decides how strongly it counts.
      const Vec3 cr = {edir.y * u.z - edir.z * u.y, edir.z * u.x - edir.x * u.z,
                       edir.x * u.y - edir.y * u.x};
      const float sinTheta = std::sqrt(cr.x * cr.x + cr.y * cr.y + cr.z * cr.z);
      const float wDir = std::pow(std::max(sinTheta, 0.05f), focusExp);

      // The ground shadow: sound from low on a distant channel refracts upward
      // and never reaches the listener, which is why a far thunder swells in
      // from the cloud instead of cracking from the strike point.
      const float elev = std::asin(clampv(mid.z / r, -1.0f, 1.0f));
      const float shadow =
         shadowWidth > 0.0f ? smoothstep((elev - shadowElev + shadowWidth) / (2.0f * shadowWidth))
                            : 1.0f;

      const float scatterAmp = std::exp(0.5f * mP.scatter * mRng.gaussian());
      // Higher up the channel, longer waves, less crackle and more energy: see
      // kCloudWeight and kCloudGain.
      const float cloudness =
         smoothstep((mid.z / heightM - kCloudWeightStart) / (kCloudWeightEnd - kCloudWeightStart));
      const float cloudFactor = 1.0f + (kCloudWeight - 1.0f) * cloudness;
      const float cloudGain = 1.0f + (kCloudGain - 1.0f) * cloudness;

      const float amp = std::sqrt(elemLen / kCoherenceM) * wDir * shadow * (1000.0f / r) *
                        branchScale * scatterAmp * cloudGain;

      const float rKm = r * 0.001f;
      const float scatterLen = std::exp(0.35f * mP.scatter * mRng.gaussian());
      const float lenSec = clampv(kNwaveBaseSec * weightScale * cloudFactor *
                                     std::pow(rKm, kNwaveRangeExp) * scatterLen,
                                  kNwaveMinSec, kNwaveMaxSec);

      // Where the air's loss reaches the corner figure: alpha(f) r = c, with
      // alpha(f) = alpha1 (f / 1 kHz)^1.6. The other two poles follow from it.
      const float airHz = clampv(1000.0f * std::pow(kAbsorbCornerDb / (alpha1 * rKm),
                                                    1.0f / kAbsorbExp),
                                 30.0f, 0.45f * mSampleRate);

      const float azimuth = std::atan2(mid.y, mid.x);
      const float pan = clampv(std::sin(azimuth) * mP.width * 1.2f + mP.pan, -1.0f, 1.0f);

      Arrival &ar = f.arrivals[f.count++];
      ar.time = r / kSpeedOfSoundMs;
      ar.amp = amp;
      ar.lenSec = lenSec;
      ar.airHz = airHz;
      ar.pan = pan;
      ar.branch = branch;
      ar.crackle = 1.0f / cloudFactor;
   }
}

void ThunderEngine::growChannel(Flash &f, float distanceM, float azimuth, float heightM,
                                float cloudM, float tortuosity, float branching,
                                float weightScale, float /*crack*/) {
   f.count = 0;
   const int budget = clampv(mP.maxShocks, 128, static_cast<int>(kMaxShocks));

   // --- The main channel as a polyline: up from the strike point to the cloud
   // base, then along inside the cloud. Steps keep a memory of their direction
   // so the channel wanders rather than zigzags; the fine jaggedness is added
   // per element.
   constexpr int kMaxMacro = 200;
   Vec3 pts[kMaxMacro];
   int n = 0;
   Vec3 p = {distanceM * std::cos(azimuth), distanceM * std::sin(azimuth), 0.0f};
   pts[n++] = p;

   const float macroLen = clampv(heightM / 36.0f, 30.0f, 300.0f);
   const float sigma = 0.12f + 0.75f * tortuosity;
   Vec3 dir = {0.0f, 0.0f, 1.0f};
   while (p.z < heightM && n < 110) {
      dir.x += sigma * mRng.gaussian();
      dir.y += sigma * mRng.gaussian();
      dir.z += 0.5f * sigma * mRng.gaussian() + 0.3f;
      if (dir.z < 0.15f)
         dir.z = 0.15f;
      const float dn = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
      dir.x /= dn;
      dir.y /= dn;
      dir.z /= dn;
      p.x += dir.x * macroLen;
      p.y += dir.y * macroLen;
      p.z += dir.z * macroLen;
      pts[n++] = p;
   }
   const int vertEnd = n - 1;

   if (cloudM > 1.0f) {
      const float az2 = 6.283185307f * mRng.uniform();
      dir = {std::cos(az2), std::sin(az2), 0.0f};
      const float cloudLen = clampv(cloudM / 30.0f, 60.0f, 500.0f);
      float travelled = 0.0f;
      while (travelled < cloudM && n < kMaxMacro) {
         dir.x += 0.5f * sigma * mRng.gaussian();
         dir.y += 0.5f * sigma * mRng.gaussian();
         dir.z += 0.25f * sigma * mRng.gaussian();
         dir.z = clampv(dir.z, -0.3f, 0.35f);
         const float dn = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
         dir.x /= dn;
         dir.y /= dn;
         dir.z /= dn;
         p.x += dir.x * cloudLen;
         p.y += dir.y * cloudLen;
         p.z += dir.z * cloudLen;
         if (p.z < heightM * 0.7f)
            p.z = heightM * 0.7f;
         travelled += cloudLen;
         pts[n++] = p;
      }
   }

   // Each macro segment's claim on the element budget: its length over its
   // distance, so the channel is cut finely where it is heard in detail.
   float weight[kMaxMacro];
   float totalWeight = 0.0f;
   for (int i = 0; i + 1 < n; ++i) {
      const float dx = pts[i + 1].x - pts[i].x, dy = pts[i + 1].y - pts[i].y,
                  dz = pts[i + 1].z - pts[i].z;
      const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
      const float mx = 0.5f * (pts[i].x + pts[i + 1].x), my = 0.5f * (pts[i].y + pts[i + 1].y),
                  mz = 0.5f * (pts[i].z + pts[i + 1].z);
      const float r = std::max(kMinRangeM, std::sqrt(mx * mx + my * my + mz * mz));
      weight[i] = len / std::pow(r, kBudgetRangeExp);
      totalWeight += weight[i];
   }
   if (totalWeight <= 0.0f)
      return;

   // --- Branches. Below the cloud they leave the channel and head outward and
   // down; inside it they run off sideways, the way intracloud lightning
   // spreads, and those are what give a thunder its several claps, one for
   // each stretch that lies side-on to the listener. The number is drawn, so
   // Branching at 40 % means two most of the time and sometimes one or three.
   const int branches = clampv(static_cast<int>(std::floor(branching * 5.0f + mRng.uniform())),
                               0, 6);
   const int cloudStart = vertEnd;
   const float branchShare = branches > 0 ? kBranchBudget : 0.0f;

   // Ground shadow geometry, from Swell and from the distance.
   const float dKm = distanceM * 0.001f;
   const float shadowElev = mP.swell * (0.05f + 0.02f * dKm);
   const float shadowWidth = mP.swell > 0.001f ? 0.5f * shadowElev + 0.01f : 0.0f;

   const float mainBudget = budget * (1.0f - branchShare);
   for (int i = 0; i + 1 < n; ++i) {
      const int count = std::max(1, static_cast<int>(mainBudget * weight[i] / totalWeight + 0.5f));
      addElements(f, pts[i], pts[i + 1], count, tortuosity, 1.0f, false, weightScale,
                  shadowElev, shadowWidth, heightM);
   }

   if (branches > 0) {
      const int perBranch = std::max(4, static_cast<int>(budget * branchShare / branches));
      for (int b = 0; b < branches; ++b) {
         // Every other branch is an in-cloud one when there is a cloud channel
         // to leave from.
         const bool inCloud = cloudM > 1.0f && n > cloudStart + 1 && (b & 1);
         int origin;
         Vec3 bd;
         float length;
         if (inCloud) {
            origin = clampv(cloudStart + static_cast<int>(mRng.uniform() * (n - 1 - cloudStart)),
                            cloudStart, n - 2);
            const float az3 = 6.283185307f * mRng.uniform();
            bd = {std::cos(az3), std::sin(az3), -0.15f + 0.3f * mRng.uniform()};
            length = cloudM * (0.25f + 0.4f * mRng.uniform());
         } else {
            origin = clampv(static_cast<int>(mRng.uniform() * vertEnd * 0.85f), 0,
                            std::max(0, vertEnd - 1));
            const float az3 = 6.283185307f * mRng.uniform();
            bd = {std::cos(az3), std::sin(az3), -0.5f + 0.6f * mRng.uniform()};
            length = heightM * (0.08f + 0.35f * mRng.uniform());
         }
         Vec3 q = pts[origin];
         const float stepRef = inCloud ? clampv(length / 20.0f, 60.0f, 500.0f) : macroLen;
         const int steps = clampv(static_cast<int>(length / stepRef) + 1, 2, 24);
         const float stepLen = length / steps;
         const int perStep = std::max(1, perBranch / steps);
         for (int s = 0; s < steps; ++s) {
            bd.x += 1.3f * sigma * mRng.gaussian();
            bd.y += 1.3f * sigma * mRng.gaussian();
            bd.z += 0.6f * sigma * mRng.gaussian();
            const float bn = std::sqrt(bd.x * bd.x + bd.y * bd.y + bd.z * bd.z);
            bd.x /= bn;
            bd.y /= bn;
            bd.z /= bn;
            Vec3 q2 = {q.x + bd.x * stepLen, q.y + bd.y * stepLen, q.z + bd.z * stepLen};
            if (q2.z < 20.0f)
               q2.z = 20.0f;
            if (inCloud && q2.z < heightM * 0.6f)
               q2.z = heightM * 0.6f;
            addElements(f, q, q2, perStep, tortuosity, inCloud ? kCloudBranchGain : kBranchGain,
                        true, weightScale, shadowElev, shadowWidth, heightM);
            q = q2;
         }
      }
   }
}

// A flash that is over, or has been taken over, is no longer its voice's
// business. A one-shot voice, or a storm voice that has been let go, ends with
// its last flash; the envelope is released so the voice can go idle.
void ThunderEngine::flashEnded(Flash &f) {
   f.active = false;
   if (f.voice < 0 || f.voice >= static_cast<int>(kMaxVoices))
      return;
   Voice &owner = mVoices[f.voice];
   if (owner.flashesLit > 0)
      --owner.flashesLit;
   if (owner.flashesLit == 0 &&
       (owner.mode == kModeOneShot || (owner.mode == kModeStorm && !owner.held)))
      owner.env.gateOff();
}

bool ThunderEngine::lightFlash(Voice &v, int voiceIndex) {
   Flash *fp = allocateFlash();
   if (!fp)
      return false;
   Flash &f = *fp;
   const float var = mP.variation;

   // --- What this particular flash is. Variation scatters everything the user
   // dialled in, because no two thunders are alike and a plugin that played the
   // same one twice would be a sample player with extra steps.
   const float velSpread = std::exp2(mP.velToDistance * 3.0f * (1.0f - v.velocity));
   const float distanceKm =
      clampv(mP.distanceKm * velSpread * std::exp(0.6f * var * mRng.gaussian()), 0.1f, 40.0f);
   const float azimuth = clampv(0.9f * var * mRng.gaussian(), -1.3f, 1.3f);
   const float heightKm = clampv(mP.heightKm * std::exp(0.3f * var * mRng.gaussian()), 0.5f, 15.0f);
   const float cloudKm =
      mP.cloudSpreadKm > 0.01f
         ? clampv(mP.cloudSpreadKm * std::exp(0.5f * var * mRng.gaussian()), 0.0f, 40.0f)
         : 0.0f;
   const float tortuosity = clampv(mP.tortuosity + 0.2f * var * mRng.gaussian(), 0.0f, 1.0f);
   const float branching = clampv(mP.branching + 0.2f * var * mRng.gaussian(), 0.0f, 1.0f);
   const float weightScale =
      std::exp2(2.0f * (mP.weight - 0.5f)) * std::exp(0.25f * var * mRng.gaussian());
   const float crack = clampv(mP.crack + 0.15f * var * mRng.gaussian(), 0.0f, 1.0f);

   growChannel(f, distanceKm * 1000.0f, azimuth, heightKm * 1000.0f, cloudKm * 1000.0f, tortuosity,
               branching, weightScale, crack);
   if (f.count == 0)
      return false;

   // --- Arrival order. From here on the channel is a schedule.
   std::sort(f.arrivals.begin(), f.arrivals.begin() + f.count,
             [](const Arrival &a, const Arrival &b) { return a.time < b.time; });

   // --- Level. The loudest tenth of a second decides the scale, so a strike
   // at 300 m and one at 15 km both arrive at a usable level, and the mild
   // per-kilometre loss then puts the far one further back.
   {
      float acc = 0.0f, peak = 0.0f;
      uint32_t j = 0;
      for (uint32_t i = 0; i < f.count; ++i) {
         const float end = f.arrivals[i].time + kNormWindowSec;
         while (j < f.count && f.arrivals[j].time <= end) {
            acc += f.arrivals[j].amp * f.arrivals[j].amp * f.arrivals[j].lenSec;
            ++j;
         }
         peak = std::max(peak, acc);
         acc -= f.arrivals[i].amp * f.arrivals[i].amp * f.arrivals[i].lenSec;
      }
      const float scale = std::sqrt(kFlashPower / std::max(peak, 1.0e-12f));
      for (uint32_t i = 0; i < f.count; ++i)
         f.arrivals[i].amp *= scale;
   }

   // --- The clock starts at the first shock anyone will hear. The elements
   // before it are the shadowed foot of a distant channel, and a second of
   // silence before a thunder is a note that appears not to have played.
   {
      float maxAmp = 0.0f;
      for (uint32_t i = 0; i < f.count; ++i)
         maxAmp = std::max(maxAmp, f.arrivals[i].amp);
      const float audible = 0.03f * maxAmp;
      float t0 = f.arrivals[0].time;
      for (uint32_t i = 0; i < f.count; ++i) {
         if (f.arrivals[i].amp >= audible) {
            t0 = f.arrivals[i].time;
            break;
         }
      }
      for (uint32_t i = 0; i < f.count; ++i)
         f.arrivals[i].time = std::max(0.0f, f.arrivals[i].time - t0);
   }

   // --- The blast. The loudest arrival of the onset says how hard the near
   // channel pushed, how far away it was and from where, and the pulse is
   // built to match it. Searching only the onset keeps it with the crack: in
   // a flash whose cloud swell is the loudest thing in it, the swell is not
   // what slams.
   {
      const float sliceSec = kBlastWindowSec / Flash::kMaxBlasts;
      int bestIdx[Flash::kMaxBlasts];
      float bestAmp[Flash::kMaxBlasts];
      for (int b = 0; b < Flash::kMaxBlasts; ++b) {
         bestIdx[b] = -1;
         bestAmp[b] = 0.0f;
      }
      for (uint32_t i = 0; i < f.count && f.arrivals[i].time < kBlastWindowSec; ++i) {
         const int b = clampv(static_cast<int>(f.arrivals[i].time / sliceSec), 0,
                              Flash::kMaxBlasts - 1);
         if (f.arrivals[i].amp > bestAmp[b]) {
            bestAmp[b] = f.arrivals[i].amp;
            bestIdx[b] = static_cast<int>(i);
         }
      }
      float peak = 0.0f;
      for (int b = 0; b < Flash::kMaxBlasts; ++b)
         peak = std::max(peak, bestAmp[b]);
      const float floor = peak * kBlastSliceFloor;
      f.blastCount = 0;
      for (int b = 0; b < Flash::kMaxBlasts; ++b) {
         if (bestIdx[b] < 0 || bestAmp[b] < floor)
            continue;
         const Arrival &a = f.arrivals[bestIdx[b]];
         const int n = f.blastCount++;
         f.blastTime[n] = a.time;
         f.blastAmp[n] = a.amp * kBlastGain;
         f.blastAirHz[n] = a.airHz;
         f.blastPan[n] = a.pan;
      }
      f.blastTauSec = clampv(kBlastTauSec * std::pow(std::max(f.distanceKm, 0.05f), kBlastTauExp),
                             kBlastTauMin, kBlastTauMax);
      for (int k = 0; k < Flash::kMaxStrokes; ++k)
         f.blastNext[k] = 0;
   }

   // --- Return strokes: the same channel lit again, each a little later than
   // the gap says and usually quieter. Dart leaders do not branch, so only the
   // first stroke lights the branches (see the branch flag in processControl).
   const int strokes = clampv(mP.strokes + static_cast<int>(std::lround(1.5f * var * mRng.gaussian())),
                              1, Flash::kMaxStrokes);
   f.strokes = strokes;
   float offset = 0.0f;
   for (int k = 0; k < strokes; ++k) {
      if (k > 0)
         offset += mP.strokeGapSec * mSampleRate * std::exp(0.5f * var * mRng.gaussian());
      f.strokeOffset[k] = offset;
      f.strokeGain[k] = k == 0 ? 1.0f : clampv(0.55f * std::exp(0.3f * mRng.gaussian()), 0.2f, 1.0f);
      f.cursor[k] = 0;
   }

   const float levelMul = (1.0f - mP.velToLevel) + mP.velToLevel * v.velocity;
   f.level = levelMul * kEngineMakeup / (1.0f + kDistanceLossPerKm * distanceKm);
   f.distanceKm = distanceKm;
   f.clock = 0.0;
   f.voice = voiceIndex;
   f.active = true;
   ++v.flashesLit;
   ++mFlashCounter;
   return true;
}

void ThunderEngine::spawnShock(const Arrival &a, float gain, float crack, uint32_t offset) {
   const float amp = a.amp * gain;
   if (amp < 1.0e-6f)
      return;
   Shock *sp = allocateShock();
   if (!sp)
      return;
   Shock &s = *sp;

   s.blast = false;
   const float len = std::max(4.0f, a.lenSec * mSampleRate);
   // The shock fronts. Crack at 100 % leaves them a hundredth of the wave long,
   // which at a 10 ms wave is 0.1 ms and puts energy out to several kilohertz;
   // at 0 % they take a third of the wave and the shock is a soft thud.
   const float edgeFrac = 0.012f + 0.3f * (1.0f - crack) * (1.0f - crack);
   const float edge = std::max(1.5f, len * edgeFrac);
   s.lenSamples = static_cast<uint32_t>(len);
   s.invLen = 1.0f / len;
   s.edgeInv = 1.0f / edge;
   s.amp = amp;
   s.crackleSamples =
      static_cast<uint32_t>(len * (kCrackleMinFrac + kCrackleFracPerCrack * crack));
   s.crackleAmp = amp * kCrackleLevel * crack * std::sqrt(crack) * a.crackle;
   // The three absorption poles, each where the air's loss reaches its figure:
   // the loss goes as f^1.6, so the pole for k times the decibels sits at
   // k^(1/1.6) times the frequency.
   for (int k = 0; k < 3; ++k) {
      const float hz = clampv(a.airHz * std::pow(kAbsorbPoleRatio[k], 1.0f / kAbsorbExp), 30.0f,
                              0.45f * mSampleRate);
      s.airCoef[k] = 1.0f - std::exp(-6.283185307f * hz / mSampleRate);
      s.airState[k] = 0.0f;
   }
   // Life: the wave itself, then the lowest pole ringing down.
   const float ring = 6.0f * mSampleRate / (6.283185307f * a.airHz);
   s.lifeMax = s.lenSamples + static_cast<uint32_t>(clampv(ring, 4.0f, 2.0f * mSampleRate)) + 16;
   s.life = 0;
   s.startOffset = offset;

   const float angle = (a.pan + 1.0f) * 0.785398163f;
   s.gainL = std::cos(angle);
   s.gainR = std::sin(angle);
   s.active = true;

   // Feed the rumble: energy, and where the air has left its spectrum.
   const float energy = amp * amp * a.lenSec;
   mRumbleEnergy += energy * kRumbleExcite;
   mRumbleAirHz += (a.airHz - mRumbleAirHz) * 0.02f;
}

void ThunderEngine::spawnBlast(const Flash &f, int index, float gain, float crack,
                               uint32_t offset) {
   const float amp = f.blastAmp[index] * gain * mP.impact;
   if (amp < 1.0e-6f)
      return;
   Shock *sp = allocateShock();
   if (!sp)
      return;
   Shock &s = *sp;

   const float tau = std::max(4.0f, f.blastTauSec * mSampleRate);
   s.blast = true;
   s.blastDecay = 1.0f / tau;
   s.blastEnv = 1.0f;
   s.blastEnvCoef = std::exp(-s.blastDecay);
   s.lenSamples = static_cast<uint32_t>(tau);
   s.invLen = s.blastDecay;
   // The jump at the front. A blast front is a discontinuity; Crack decides
   // how much of one survives here, exactly as it does for the N-wave, so a
   // soft setting gives a thump and a hard one gives a slam with an edge.
   const float edge = std::max(1.5f, tau * (0.01f + 0.25f * (1.0f - crack) * (1.0f - crack)));
   s.edgeInv = 1.0f / edge;
   s.amp = amp;
   // No crackle: the tearing belongs to the individual elements' fronts, and
   // the blast is the one thing in the model that is not one of them.
   s.crackleSamples = 0;
   s.crackleAmp = 0.0f;
   for (int k = 0; k < 3; ++k) {
      const float hz = clampv(f.blastAirHz[index] * std::pow(kAbsorbPoleRatio[k], 1.0f / kAbsorbExp),
                              30.0f, 0.45f * mSampleRate);
      s.airCoef[k] = 1.0f - std::exp(-6.283185307f * hz / mSampleRate);
      s.airState[k] = 0.0f;
   }
   s.lifeMax = static_cast<uint32_t>(kBlastTailTaus * tau) + 16;
   s.life = 0;
   s.startOffset = offset;

   const float angle = (f.blastPan[index] + 1.0f) * 0.785398163f;
   s.gainL = std::cos(angle);
   s.gainR = std::sin(angle);
   s.active = true;

   // And it shakes the rumble like anything else that arrives, so the slam is
   // answered by the swell instead of standing on its own.
   mRumbleEnergy += amp * amp * f.blastTauSec * kRumbleExcite * kBlastRumbleExcite;
}

// ------------------------------------------------------------------ process

void ThunderEngine::processControl(float *outL, float *outR, uint32_t numSamples) {
   // Rumble filter for this block: the tone control, or the air if it has
   // taken more off than the tone control asks for.
   const float toneHz = 60.0f * std::pow(25.0f, mP.rumbleTone);
   mRumbleCutoff = clampv(std::min(toneHz, 1.5f * mRumbleAirHz), 30.0f, 0.45f * mSampleRate);
   mRumbleLpL.setCutoff(mRumbleCutoff, 0.15f, mSampleRate);
   mRumbleLpR.setCutoff(mRumbleCutoff, 0.15f, mSampleRate);
   const float rumbleHp = std::max(kRumbleHighpassMinHz, mRumbleCutoff * kRumbleBandRatio);
   mRumbleHpL.setCutoff(rumbleHp, mSampleRate);
   mRumbleHpR.setCutoff(rumbleHp, mSampleRate);
   // Noise through a 2-pole lowpass loses level as the corner drops; compensate
   // so that the tone control changes colour rather than loudness.
   const float toneComp = clampv(std::sqrt(2000.0f / mRumbleCutoff), 0.5f, 6.0f);
   const float rumbleGain = mP.rumble * kRumbleGain * toneComp;
   const float stormRate = mP.stormRatePerMin / 60.0f;

   for (uint32_t i = 0; i < numSamples; ++i) {
      if (mModCounter == 0)
         mDriftState += mDriftCoef * (mNoiseRng.gaussian() - mDriftState);
      if (++mModCounter >= kModInterval)
         mModCounter = 0;

      // --- Voices: envelopes, and the storm clock.
      for (uint32_t vi = 0; vi < kMaxVoices; ++vi) {
         Voice &v = mVoices[vi];
         if (!v.active)
            continue;
         v.env.tick();
         if (v.env.isIdle()) {
            v.active = false;
            for (auto &f : mFlashes)
               if (f.active && f.voice == static_cast<int>(vi))
                  f.active = false;
            continue;
         }
         if (v.mode == kModeStorm) {
            if (v.held) {
               v.stormTimer -= 1.0;
               if (v.stormTimer <= 0.0) {
                  lightFlash(v, static_cast<int>(vi));
                  const float wait = mRng.exponential(stormRate) * mSampleRate;
                  v.stormTimer += wait < 1.0f ? 1.0f : wait;
               }
            }
         } else if (v.stormTimer <= 0.0) {
            // One Shot and Gated: exactly one flash, at the start. A flash that
            // could not be grown still has to let the voice finish.
            v.stormTimer = 1.0e30;
            if (!lightFlash(v, static_cast<int>(vi)))
               v.env.gateOff();
         }
      }

      // --- Flashes: every arrival whose time has come becomes a shock.
      for (auto &f : mFlashes) {
         if (!f.active)
            continue;
         const Voice &v = mVoices[f.voice];
         const float env = v.env.level();
         for (int k = 0; k < f.strokes; ++k) {
            const float off = f.strokeOffset[k];
            while (f.blastNext[k] < f.blastCount &&
                   f.blastTime[f.blastNext[k]] * mSampleRate + off <= f.clock) {
               spawnBlast(f, f.blastNext[k], f.level * f.strokeGain[k] * env, mP.crack, i);
               ++f.blastNext[k];
            }
            while (f.cursor[k] < f.count) {
               const Arrival &a = f.arrivals[f.cursor[k]];
               if (k > 0 && a.branch) {
                  ++f.cursor[k];
                  continue;
               }
               if (a.time * mSampleRate + off > f.clock)
                  break;
               spawnShock(a, f.level * f.strokeGain[k] * env * strokeJitter(f.cursor[k], k),
                          mP.crack, i);
               ++f.cursor[k];
            }
         }
         f.clock += 1.0;
         if (f.exhausted())
            flashEnded(f);
      }

      // --- Rumble.
      mRumbleEnergy *= mRumbleDecay;
      if (mRumbleEnergy > 1.0e-12f && rumbleGain > 0.0f) {
         const float g = rumbleGain * std::sqrt(mRumbleEnergy);
         const float n1 = mNoiseRng.white();
         const float n2 = mNoiseRng.white();
         const float bl = mRumbleMixA * n1 + mRumbleMixB * n2;
         const float br = mRumbleMixA * n1 - mRumbleMixB * n2;
         const float pan = clampv(mP.drift * 0.8f * mDriftState * mDriftNorm, -1.0f, 1.0f);
         const float angle = (pan + 1.0f) * 0.785398163f;
         outL[i] += mRumbleHpL.tick(mRumbleLpL.lowpass(bl)) * g * std::cos(angle) * 1.41421356f;
         outR[i] += mRumbleHpR.tick(mRumbleLpR.lowpass(br)) * g * std::sin(angle) * 1.41421356f;
      } else if (mRumbleEnergy <= 1.0e-12f) {
         mRumbleEnergy = 0.0f;
      }
   }
}

void ThunderEngine::processShocks(float *outL, float *outR, uint32_t numSamples) {
   for (uint32_t si = 0; si < kMaxShocks; ++si) {
      Shock &s = mShocks[si];
      if (!s.active)
         continue;
      if (s.startOffset >= numSamples) {
         s.startOffset -= numSamples;
         continue;
      }
      uint32_t i = s.startOffset;
      s.startOffset = 0;
      const uint32_t len = s.lenSamples;

      for (; i < numSamples; ++i) {
         float x = 0.0f;
         if (s.blast) {
            // Friedlander: up over the front, then (1 - t/T) exp(-t/T), which
            // crosses zero at T and comes back as the long negative phase.
            const float lifeF = static_cast<float>(s.life);
            const float t = lifeF * s.blastDecay;
            float e = std::min(lifeF * s.edgeInv, 1.0f);
            e = e * e * (3.0f - 2.0f * e);
            x = s.amp * e * (1.0f - t) * s.blastEnv;
            s.blastEnv *= s.blastEnvCoef;
         } else if (s.life < len) {
            // The N-wave: a jump up, a straight fall through zero, a jump back.
            // Both fronts are eased over the Crack-controlled rise time, and
            // the easing is a smoothstep so the front's spectrum falls off
            // rather than ringing at the sample rate.
            const float lifeF = static_cast<float>(s.life);
            float e = std::min(lifeF * s.edgeInv, (static_cast<float>(len) - lifeF) * s.edgeInv);
            if (e > 1.0f)
               e = 1.0f;
            e = e * e * (3.0f - 2.0f * e);
            x = (1.0f - 2.0f * lifeF * s.invLen) * e * s.amp;
            if (s.life < s.crackleSamples)
               x += s.crackleAmp * mNoiseRng.white() *
                    (1.0f - lifeF / static_cast<float>(s.crackleSamples));
         }
         s.airState[0] += s.airCoef[0] * (x - s.airState[0]);
         s.airState[1] += s.airCoef[1] * (s.airState[0] - s.airState[1]);
         s.airState[2] += s.airCoef[2] * (s.airState[1] - s.airState[2]);
         x = s.airState[2];
         outL[i] += x * s.gainL;
         outR[i] += x * s.gainR;
         if (++s.life >= s.lifeMax) {
            s.active = false;
            break;
         }
      }
   }
}

void ThunderEngine::processOutputChain(float *outL, float *outR, uint32_t numSamples) {
   const bool space = mP.spaceAmount > 0.001f;
   const float wet = mP.spaceAmount;
   const float gain = mP.gain;
   const bool echoes = mEchoActive > 0 && mP.echoGain > 1.0e-4f;

   for (uint32_t i = 0; i < numSamples; ++i) {
      float l = outL[i];
      float r = outR[i];

      // Echoes off the landscape: a mono sum of the direct sound, back later,
      // quieter and duller, from wherever the reflector stands. A little of
      // what comes back goes round again, because a hill throws the echo of an
      // echo too, and that is what lets a thunder trail off instead of stop.
      {
         const float mono = 0.5f * (l + r);
         float back = 0.0f;
         if (echoes) {
            for (int k = 0; k < mEchoActive; ++k) {
               const float e = mEchoLp[k].tick(mEchoLine.read(mEchoDelay[k])) * mEchoGain[k];
               l += e * mEchoPanL[k];
               r += e * mEchoPanR[k];
               back += e;
            }
         }
         mEchoLine.write(mono + mEchoFeedback * back);
      }

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

      l *= gain;
      r *= gain;
      mCompressor.tick(l, r);
      outL[i] = softClip(l);
      outR[i] = softClip(r);
   }
}

void ThunderEngine::process(float *outL, float *outR, uint32_t numSamples) {
   if (numSamples == 0)
      return;

   processControl(outL, outR, numSamples);
   processShocks(outL, outR, numSamples);
   processOutputChain(outL, outR, numSamples);

   if (activeVoiceCount() == 0 && activeFlashCount() == 0 && activeShockCount() == 0) {
      // Saturate rather than wrap: this counter is only compared against the
      // tail length.
      const uint32_t limit = 0xFFFFFFFFu - numSamples;
      mSilenceCounter = mSilenceCounter > limit ? 0xFFFFFFFFu : mSilenceCounter + numSamples;
   } else {
      mSilenceCounter = 0;
   }
}

uint32_t ThunderEngine::activeVoiceCount() const {
   uint32_t n = 0;
   for (const auto &v : mVoices)
      if (v.active)
         ++n;
   return n;
}

uint32_t ThunderEngine::activeFlashCount() const {
   uint32_t n = 0;
   for (const auto &f : mFlashes)
      if (f.active)
         ++n;
   return n;
}

uint32_t ThunderEngine::activeShockCount() const {
   uint32_t n = 0;
   for (const auto &s : mShocks)
      if (s.active)
         ++n;
   return n;
}

float ThunderEngine::tailSeconds() const {
   // The echoes and the room ring on after the last shock, and the rumble's
   // integrator takes a moment to empty.
   const float spaceTail = mP.spaceAmount > 0.001f ? 0.3f + mSpace.decaySeconds() : 0.05f;
   const float echoTail = mEchoActive > 0 ? mP.echoSpreadSec : 0.0f;
   return mP.releaseSec + spaceTail + echoTail + 1.0f;
}

bool ThunderEngine::isSilent() const {
   if (activeVoiceCount() != 0 || activeFlashCount() != 0 || activeShockCount() != 0)
      return false;
   return mSilenceCounter > static_cast<uint32_t>(tailSeconds() * mSampleRate);
}

} // namespace thunderclap
