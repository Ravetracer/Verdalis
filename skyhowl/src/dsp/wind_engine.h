#pragma once

// SkyHowl's synthesis: moving air, and the things it moves past.
//
// The starting point is that **wind is silent**. Air in motion radiates
// essentially nothing on its own; every sound a listener calls "wind" is the
// flow meeting an obstacle. So the engine is built in two halves: a flow field,
// which makes no sound at all, and a set of sources it drives.
//
//   the flow    A wind speed U(t), per channel. Its mean is Wind Speed; on top
//               of that sit three things measured separately in the reference
//               library: turbulence (a von Karman-shaped noise process whose
//               intensity is sigma_u / U, measured 4-27 %), discrete gusts
//               (0.6-33 a minute, gust factor 1.04-1.54), and a squall drift
//               slower than 0.1 Hz, which carries a median 43 % of the whole
//               envelope variance and is therefore not a detail.
//
//   airflow     The broadband bed: turbulent noise shaped by a four-pole tilt
//               whose slope is a parameter. Kolmogorov's inertial subrange puts
//               the velocity spectrum at f^-5/3, which is -5 dB/octave; the
//               library measures -22 to -2 with a median of -8.7, radiation
//               from a rigid surface being steeper than the flow itself. Its
//               amplitude follows U^3, because aerodynamic sound power from
//               flow over a rigid surface goes as U^6 (Curle 1955).
//
//   buffet      The low end: the pressure fluctuation of the moving air rather
//               than the noise it radiates. It follows the dynamic pressure,
//               U^2, so it grows more slowly than the bed.
//
//   howl        The aeolian tones. A bluff body in a flow sheds vortices at the
//               Strouhal frequency f = St U / d with St ~ 0.2, so a 2 mm twig
//               in an 8 m/s wind sheds at 800 Hz -- and the pitch is
//               proportional to the wind speed, which is why howling swoops
//               with the gust. This is the one prediction the references can
//               falsify, and they confirm it: in 29 of the 31 recordings with
//               an audible tone, the tone's pitch rises with the level.
//               A cavity -- a gap, a cave -- is the exception by construction:
//               its pitch is fixed by its own geometry, so the obstacle scales
//               how much tracking applies.
//
//   rustle      Foliage. Not wind, but what wind is usually heard through: a
//               Poisson stream of leaf clicks, each a short band-limited ring
//               at roughly c/2L for a leaf of size L. The references' rustle
//               onsets sit at a median 4.2 kHz and 15-40 resolvable clicks a
//               second, which is the rate at which they stop merging.
//
// No samples, and no rain and no surf: those live in other plugins in the suite.

#include "../params.h"

#include "verdalis/dsp/adsr.h"
#include "verdalis/dsp/fastmath.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/reverb.h"
#include "verdalis/dsp/rng.h"

#include <cstdint>

namespace skyhowl {

struct EngineParams {
   float gain = 1.0f; // linear

   // wind
   float windSpeedMs = 8.0f;
   float turbulence = 0.10f; // sigma_u / U, not a percentage
   float gustRatePerMin = 3.5f;
   float gustDepth = 0.5f;
   float gustLengthSec = 4.0f;
   float gustShape = 0.5f;
   float squall = 0.43f;
   float squallRatePerMin = 1.2f;

   // airflow
   float flowGain = 0.5f; // linear
   float flowToneHz = 500.0f;
   float flowTiltDbOct = -8.7f;
   float buffetGain = 0.158f; // linear
   float buffetToneHz = 70.0f;
   float hiss = 0.3f;
   float speedLaw = 1.0f;

   // howl
   float howlAmount = 0.35f;
   int obstacle = kObstacleTwigs;
   float howlSizeMm = 2.1f;
   float howlSpreadOct = 1.2f;
   int howlVoices = 5;
   float howlReso = 0.36f;
   float howlTrack = 0.8f;
   float howlThreshold = 0.3f;
   float warble = 0.25f;

   // rustle
   float rustleAmount = 0.3f;
   int foliage = kFoliageBroadleaf;
   float rustleSizeMm = 41.0f;
   float rustleDensityHz = 25.0f;
   float rustleSpreadOct = 1.0f;
   float rustleDecaySec = 0.018f;
   float rustleThreshold = 0.25f;
   float clatter = 0.4f;

   // place
   int terrain = kTerrainPlain;
   float distance = 0.2f;
   float air = 0.5f;
   float width = 0.75f;
   float spaceAmount = 0.2f;
   float spaceSize = 0.6f;
   float spaceDamping = 0.5f;

   // filter
   int filterType = 0;
   float highpassHz = 20.0f;
   float filterCutoffHz = 20000.0f;
   float filterReso = 0.1f;
   float filterKeyTrack = 0.0f;

   // envelope
   float attackSec = 2.5f;
   float decaySec = 0.8f;
   float sustain = 1.0f;
   float releaseSec = 4.0f;
   float velToLevel = 0.5f;
   float velToSpeed = 0.4f;

   int maxGusts = 64;
   int seed = 0;
};

// One gust: a coherent lump of faster-moving air crossing the listener.
//
// Deliberately silent. A gust owns no filter and no noise source, because a
// gust makes no sound of its own -- it is a fluctuation of the flow, and what
// is heard is the bed and the howl responding to it. That is what keeps the
// pool cheap enough to be generous with, and it is also the physics: there is
// nothing else a moving parcel of air can do.
struct Gust {
   bool active = false;
   int voice = -1; // which note spawned it

   float env = 0.0f;
   float strength = 0.0f; // fractional excess over the mean speed
   float risePhase = 0.0f;
   float riseInc = 0.0f; // per control block
   float decayCoef = 0.0f;
   bool rising = true;

   // Where it crosses. A gust arriving from one side is heard on that side
   // first, which is most of why real wind moves. Stored as a position rather
   // than as a pair of weights so that changing Width moves the gusts already
   // in flight.
   float pan = 0.0f; // -1 left, +1 right

   inline bool finished() const { return !rising && env < 1.0e-4f; }
};

// One leaf, heard once. A short band-limited ring at the frequency the leaf's
// own size gives it, plus a fraction of a broadband transient for the dry
// clatter of an autumn leaf.
struct Leaf {
   bool active = false;
   Svf band;
   // A state-variable bandpass falls away at only 6 dB/octave either side, so
   // a leaf ringing at 4 kHz still put measurable energy at 16, and the top of
   // the spectrum ended up set by the skirts of the resonators rather than by
   // the airflow bed. Two more poles above the band take the skirt to
   // 18 dB/octave, which is the 40 dB over two and a half octaves the
   // full-bandwidth references actually measure.
   Lp2 top;
   float level = 0.0f;
   float decayCoef = 0.0f;
   // The moment of contact, before anything has had time to ring. It excites
   // the leaf's own band rather than bypassing it: a leaf is not a broadband
   // radiator, and adding the transient to the output instead of to the input
   // put white noise straight into the top of the spectrum.
   float clickLevel = 0.0f;
   float clickCoef = 0.0f;
   float panL = 0.7071f, panR = 0.7071f;
   RngLite rng;
};

// A relaxation oscillation: turbulent flow forced through a constricted,
// compliant aperture that flaps as it passes.
//
// It is the same family of problem as the aeolian howl above -- flow meeting a
// boundary that resonates -- but two octaves lower, far more heavily amplitude
// modulated, and driven by a falling pressure rather than a steady wind. Each
// closure of the aperture is an impulse that rings the two bands, so the rate
// of closure is the pitch; the turbulence that gets through between closures is
// the hiss over the top.
//
// Fitted against 54 reference recordings: 0.13-1.69 s long (median 0.30),
// fundamental 28-300 Hz (median 113), spectral centroid 367-3267 Hz (median
// 999), a flutter rate of 4-148 Hz (median 8) and an envelope variation of
// 0.48-1.93 (median 0.92). Every trigger draws a fresh set from those ranges,
// so no two are alike -- which is the same rule the rest of the plugin follows.
struct Vent {
   bool active = false;
   Svf band;    // the aperture itself: low and resonant
   Svf formant; // what the cavity behind it does to the result
   float phase = 0.0f;      // aperture cycle, 0..1; one impulse per turn
   float inc = 0.0f;
   float glide = 1.0f;      // per-sample change in inc: the pressure falling
   float level = 0.0f;
   float decayCoef = 0.0f;
   float flutterPhase = 0.0f;
   float flutterInc = 0.0f;
   float flutterDepth = 0.0f;
   float hiss = 0.0f;
   float formantMix = 0.0f;
   float panL = 0.7071f, panR = 0.7071f;
   RngLite rng;
};

// One aeolian source: an obstacle of a particular diameter, shedding.
struct HowlVoice {
   Svf band;
   // Same reasoning as Leaf::top: a high-Q band's own skirt was the brightest
   // thing in several presets fitted to dark references.
   Lp2 top;
   // Where this obstacle sits in the bank, fixed at note-on: an offset in
   // octaves from Howl Size, and a position from -1 to +1. Both are held
   // rather than redrawn, so a bank of obstacles keeps its identity while the
   // wind moves over it.
   float sizeOffset = 0.0f;
   float position = 0.0f;
   float walk = 0.0f; // the warble: a slow drift of the shedding frequency
   float walkCoef = 0.0f;
   float gain = 0.0f; // ramped between control blocks
   float gainStep = 0.0f;
   float panL = 0.7071f, panR = 0.7071f;
   RngLite rng;
};

// One held note: its own wind.
struct Voice {
   bool active = false;
   int16_t port = 0, channel = 0, key = 60;
   int32_t noteId = -1;
   float velocity = 1.0f;

   Adsr env;

   // ------------------------------------------------------------- the flow
   //
   // Turbulence as three one-pole-filtered noise sources an octave and a half
   // apart, weighted 1 : 0.56 : 0.315. Those weights are 4^(-5/6) and
   // 16^(-5/6): summing them gives a spectrum falling at f^-5/3 in power
   // across the band they span, which is the inertial subrange the flow
   // actually has. One filter would give -6 dB/octave, which is too steep, and
   // white noise would give none at all.
   OnePoleLp turb[3];
   float squallPhase = 0.0f;
   float squallInc = 0.0f;
   OnePoleLp squallWalk;

   float speedL = 8.0f, speedR = 8.0f; // the current wind speed, per channel
   // The speed the howl actually follows. An obstacle does not see the
   // instantaneous free-stream velocity: it sees the flow averaged over the
   // eddies that envelop it, which is slower and much smoother. Without this
   // the shedding frequency chases every ripple of the turbulence and the tone
   // smears over two octaves, where the references measure a median swoop of
   // 0.72 -- a gust's worth, not a turbulence spectrum's worth.
   OnePoleLp howlSpeed;

   // The bed. Four cascaded one-poles, crossfaded to give any slope from flat
   // to -24 dB/octave.
   OnePoleLp tiltL[4], tiltR[4];
   OnePoleHp bedHpL, bedHpR;
   Lp2 buffetLpL, buffetLpR;
   // The hiss is a band, not a shelf. Turbulence has a small-scale limit as
   // well as a large one -- viscosity dissipates the smallest eddies -- so
   // leaving the top open put a flat white shelf above 3.5 kHz where the
   // full-bandwidth references measure a real rolloff.
   Hp2 hissHpL, hissHpR;
   Lp2 hissLpL, hissLpR;

   HowlVoice howl[12];

   // Gains held across a control block and ramped per sample, so that a gust
   // does not step.
   float bedGainL = 1.0f, bedGainR = 1.0f;
   float bedStepL = 0.0f, bedStepR = 0.0f;
   float buffetGainL = 1.0f, buffetGainR = 1.0f;
   float buffetStepL = 0.0f, buffetStepR = 0.0f;

   uint32_t modCounter = 0;
   double gustTimer = 0.0;
   double leafTimer = 0.0;
   float rustleGate = 0.0f;

   inline bool finished() const { return env.isIdle(); }
};

class WindEngine {
public:
   void prepare(double sampleRate, uint32_t maxBlockSize);
   void reset();

   void setParams(const EngineParams &p);

   void noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId, double velocity);
   void noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void choke(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void allSoundOff();

   void process(float *outL, float *outR, uint32_t numSamples);

   // Fires one relaxation oscillation, independently of any held note. Called
   // from the audio thread once per request the editor has posted.
   void triggerVent();

   bool isSilent() const;
   uint32_t activeVoiceCount() const;
   // What the window's activity meter counts.
   uint32_t activeGustCount() const;
   // Monotonic count of gusts spawned, for the header ornament.
   uint32_t gustCounter() const { return mGustCounter; }
   float tailSeconds() const;

   static constexpr uint32_t kMaxVoices = 16;
   static constexpr uint32_t kMaxGusts = 256;
   static constexpr uint32_t kMaxLeaves = 768;
   static constexpr uint32_t kMaxHowl = 12;
   static constexpr uint32_t kMaxVents = 8;
   static constexpr uint32_t kModInterval = 64; // control rate for the flow field

private:
   void updateFilters();
   void updateFlow(Voice &v, float env, float velSpeed);
   void spawnGust(Voice &v);
   void spawnLeaf(float speed);
   Gust *allocateGust();
   Leaf *allocateLeaf();
   void processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples);
   void processLeaves(float *outL, float *outR, uint32_t numSamples);
   void processVents(float *outL, float *outR, uint32_t numSamples);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);

   EngineParams mP;
   double mSampleRate = 48000.0;

   Voice mVoices[kMaxVoices];
   Gust mGusts[kMaxGusts];
   Leaf mLeaves[kMaxLeaves];
   Vent mVents[kMaxVents];

   Rng mRng;
   int mAppliedSeed = 0;
   uint32_t mGustCounter = 0;

   Space mSpace;
   Svf mOutFilterL, mOutFilterR;
   Hp2 mOutHpL, mOutHpR;
   Lp2 mAirLpL, mAirLpR; // distance: air absorption
   OnePoleLp mDistanceTiltL, mDistanceTiltR;

   // Derived once per parameter change rather than per sample.
   float mTurbNorm = 1.0f;   // normalises the three-pole turbulence to unit variance
   float mTurbWeight[3] = {1.0f, 0.56f, 0.315f};
   float mHowlQ = 2.0f;
   float mFlowSlopeMix = 1.45f; // slope in poles: 0 = flat, 4 = -24 dB/octave

   float mSilenceCounter = 0.0f;
};

} // namespace skyhowl
