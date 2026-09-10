#pragma once

// RiverFlow's synthesis: running water, from a river to a single drop.
//
// The model is what the reference library measures, and the library says
// something blunt about rivers: most of one is Gaussian noise with a shape on
// it. `tools/analysis/grain.py` compares the 4 ms envelope statistics of every
// recording against a white-noise control of the same length, and the
// smoothest third of the library lands *on* the control row -- coefficient of
// variation 0.30 against the control's 0.30 at 200-800 Hz, 0.13 against 0.12 at
// 2-6 kHz. Those recordings are noise. Not "like noise": indistinguishable
// from it by these statistics.
//
// So the engine is a bed plus a population of events, and the whole span the
// plugin covers is the ratio between them:
//
//   flow     The bed. One noise source per channel through an eight-band
//            filterbank at octave centres, driven at the measured octave-band
//            gains of whichever of the six clusters the library falls into is
//            selected. Each band's level wanders independently -- measured:
//            the correlation between one band's 100 ms envelope and the next
//            one's is 0.07 to 0.25, so the bed is not one gain being moved --
//            and the wander is deepest at the bottom, 2.4x as deep at 125-250
//            Hz as at 1-2 kHz. That is why the low end of a river seems to
//            surge while its hiss sits still.
//
//   dabble   Water folding over a stone traps a pocket of air, and the pocket
//            rings at the Minnaert pitch for its radius. Measured 1.4-9.3 a
//            second, 2.3-5.8 mm, and -- the part that matters -- each is a
//            short *cluster* rather than one pocket: the event-triggered
//            spectrum gives a Q of 0.7-5, which is a ring of about a
//            millisecond, while the event-triggered envelope takes 7-34 ms to
//            fall 10 dB. One bubble cannot do both.
//
//   trickle  A single drop striking stone or standing water: a broadband
//            impact off the surface it hit, then the tiny pocket it entrains.
//            Measured 5-25 a second at 0.42-1.72 mm, ringing 2-8 kHz.
//
//   plunge   The pool under a fall. A cloud of bubbles has collective modes far
//            below any bubble in it, and three of them at Xue et al.'s measured
//            1.00 / 1.53 / 1.90 ratios read as a body of water where one reads
//            as a tuned pipe.
//
// Nothing below 60 Hz is generated on purpose. Four references carry up to a
// quarter of their energy there, and in every one of them its envelope is
// uncorrelated with the water above it: it is wind and handling noise on the
// microphone, and synthesising it would be fitting the recordist's afternoon.
//
// No samples. The bed shapes are tables of eight numbers each; see the suite's
// note on what that does and does not permit.

#include "../params.h"

#include "verdalis/dsp/adsr.h"
#include "verdalis/dsp/bubble.h"
#include "verdalis/dsp/pocket.h"
#include "verdalis/dsp/fastmath.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/reverb.h"
#include "verdalis/dsp/rng.h"

#include <cstdint>

namespace riverflow {

// The bed's filterbank: one bandpass per octave from 125 Hz to 16 kHz, which
// is the resolution the shapes were measured at. The centres never change, so
// the filters are configured once and only their gains move.
constexpr int kNumBedBands = 8;

struct EngineParams {
   float gain = 1.0f; // linear

   // flow
   int waterType = kWaterStream;
   float waterBlend = 0.0f;
   float flowGain = 0.501f; // linear
   float flowTilt = 0.0f;   // -1..1, +/-6 dB/oct about 1 kHz
   float flowBody = 0.5f;
   float turbulence = 0.22f;
   float surgeRateHz = 1.3f;
   float flowGrain = 0.0f;

   // stones
   float dabbleRateHz = 5.5f;
   float dabbleGain = 0.251f; // linear
   float dabbleSizeMm = 3.3f;
   float dabbleSpreadOct = 0.9f;
   int dabbleCluster = 4;
   float dabbleSpillSec = 0.025f;
   float dabbleDamping = 1.0f;
   float dabbleGlug = 0.35f;

   // trickle
   float trickleRateHz = 16.0f;
   float trickleGain = 0.158f; // linear
   float trickleSizeMm = 1.24f;
   float trickleSpreadOct = 1.2f;
   float trickleDecaySec = 0.013f;
   float trickleImpact = 0.45f;
   float stoneToneHz = 3500.0f;
   float splash = 0.25f;

   // plunge
   float plungeGain = 0.063f; // linear
   float plungeToneHz = 180.0f;
   float plungeDepth = 0.4f;
   float plungeQ = 0.5f;

   // reach
   int bank = kBankForest;
   float distance = 0.3f;
   float air = 0.5f;
   float width = 0.7f;
   float flowWidth = 0.4f;
   float spaceAmount = 0.22f;
   float spaceSize = 0.5f;
   float spaceDamping = 0.5f;

   // filter
   int filterType = 0;
   float highpassHz = 60.0f;
   float filterCutoffHz = 20000.0f;
   float filterReso = 0.1f;
   float filterKeyTrack = 0.0f;

   // envelope
   float attackSec = 0.8f;
   float decaySec = 0.5f;
   float sustain = 1.0f;
   float releaseSec = 1.5f;
   float velToLevel = 0.4f;
   float velToFlow = 0.3f;

   int maxEvents = 512;
   int seed = 0;
};

// A pocket of air, and the drop that traps one, are the suite's: see
// verdalis/dsp/pocket.h. RiverFlow's Trickle layer *is* that code, and RainyDay
// uses the same one so that the two agree.
using verdalis::Pocket;
using verdalis::TrickleSpec;

// One held note: its own river. The bed is per-voice rather than global so
// that a second note is a second stretch of water rather than the same one
// twice as loud, and so the envelope and velocity reach it at all.
struct Voice {
   bool active = false;
   int16_t port = 0, channel = 0, key = 60;
   int32_t noteId = -1;
   float velocity = 1.0f;

   Adsr env;

   // The bed: one noise source per channel through the octave filterbank.
   Svf bandL[kNumBedBands], bandR[kNumBedBands];
   // The steep ends of a shape. A bank of octave-wide bandpasses cannot make a
   // skirt steeper than about 6 dB/octave, and three of the six measured
   // shapes have one -- Creek falls 10 dB from 250 Hz to 125, Deep Rush 10 dB
   // from 8 to 16 kHz. These two make those, and the bank is then solved for
   // what is left. Corners come from the shape, so a shape without a steep end
   // gets them parked out of the way instead.
   Hp2 hpL, hpR;
   Lp2 lpL, lpR;
   RngLite rngCommon, rngL, rngR;

   // The surge: one random walk per band per channel, because the measured
   // correlation between neighbouring bands' envelopes is 0.07-0.25. Run at
   // the control rate; at a 1.3 Hz corner there is nothing above it to miss.
   OnePoleLp surgeL[kNumBedBands], surgeR[kNumBedBands];
   float surgeGainL[kNumBedBands]{};
   float surgeGainR[kNumBedBands]{};
   RngLite rngSurge;
   uint32_t modCounter = 0;

   // The bed's own graininess. One sample-and-hold per band per channel, with
   // its own period, because the measured excess over a Gaussian control grows
   // steeply with frequency and because switching all eight together would put
   // a broadband click into the output every few milliseconds.
   float grainHoldL[kNumBedBands]{};
   float grainHoldR[kNumBedBands]{};
   // Ramped to the next value rather than stepped onto it. A sample-and-hold
   // multiplying a band's amplitude steps it, and a step is a discontinuity --
   // eight bands in two channels stepping 250 times a second is four thousand
   // discontinuities a second, which is a continuous fizz. It hides from a
   // line-spectrum search, because the step sizes are random and the bands are
   // out of phase, and from a peak-to-median ratio, because it lifts the median
   // as much as the peaks.
   float grainIncL[kNumBedBands]{};
   float grainIncR[kNumBedBands]{};
   int grainCountL[kNumBedBands]{};
   int grainCountR[kNumBedBands]{};

   // The plunge pool: three collective modes of the bubble cloud held in it.
   Svf plunge[3];
   OnePoleLp plungeSurge;
   RngLite rngPlunge;

   // Samples until the next dabble and the next drop.
   double dabbleTimer = 0.0;
   double trickleTimer = 0.0;

   inline bool finished() const { return env.isIdle(); }
};

class RiverEngine {
public:
   void prepare(double sampleRate, uint32_t maxBlockSize);
   void reset();

   void setParams(const EngineParams &p);

   void noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId, double velocity);
   void noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void choke(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void allSoundOff();

   void process(float *outL, float *outR, uint32_t numSamples);

   bool isSilent() const;
   uint32_t activeVoiceCount() const;
   // What the window's activity meter counts: pockets of air sounding.
   uint32_t activePocketCount() const;
   // Monotonic count of dabbles, which is what the header ornament draws.
   uint32_t dabbleCounter() const { return mDabbleCounter; }
   float tailSeconds() const;

   // A river is a bed, and one stretch of it costs 16 bandpasses a sample, so
   // the voice pool is deliberately smaller than the suite's usual sixteen.
   static constexpr uint32_t kMaxVoices = 8;
   static constexpr uint32_t kMaxPockets = 2048;
   static constexpr uint32_t kModInterval = 64; // control rate for the surge

private:
   void updateFilters();
   // Events are pooled across voices and drawn from the engine's own generator,
   // so neither spawn needs the voice that asked for it.
   void spawnDabble(float envLevel, float flow);
   void spawnTrickle(float envLevel, float flow);
   Pocket *allocatePocket();
   void processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples);
   void processPockets(float *outL, float *outR, uint32_t numSamples);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);

   EngineParams mP;
   double mSampleRate = 48000.0;

   Voice mVoices[kMaxVoices];
   Pocket mPockets[kMaxPockets];

   Rng mRng;
   int mAppliedSeed = 0;
   uint32_t mDabbleCounter = 0;

   Space mSpace;
   Svf mOutFilterL, mOutFilterR;
   Hp2 mOutHpL, mOutHpR;
   Lp2 mAirLpL, mAirLpR; // distance: air absorption
   bool mAirActive = false;
   bool mOutFilterActive = true;
   OnePoleLp mDistanceTiltL, mDistanceTiltR;

   // The bed's band gains, derived from the selected measured shape. The
   // centres are constant, so this is all that changes when a parameter moves.
   float mBedGain[kNumBedBands]{};
   float mBedNorm = 1.0f;

   // What the bank actually does, as against what its gains say. Octave-wide
   // bandpasses an octave apart overlap, so driving band j at unit gain puts
   // energy into band b's centre as well -- and the two edge bands, having a
   // neighbour on one side only, come out 3 to 9 dB below the curve they were
   // set to. mBankResp[b][j] is the power band j contributes at band b's
   // centre, filled in once at prepare() from the SVF's own analytic response;
   // updateFilters() then solves for the gains that make the *sum* match the
   // measured shape instead of merely setting each band to it.
   float mBankResp[kNumBedBands][kNumBedBands]{};
   float mBedHpHz = 40.0f;
   float mBedLpHz = 22000.0f;

   // The grid the band integrals are taken on, and the bank's response on it.
   // Held rather than recomputed because the alternative -- correcting the
   // target for the skirt filters at the band centre instead of integrating
   // them inside the band -- does not work: it asks the bank for exactly as
   // much gain as the filter takes away, so the two cancel and the steep skirt
   // never appears. The filters have to be part of what the solver is told the
   // bank can do.
   //
   // Everything here is fixed at prepare(): the grid, the trapezoid weights,
   // the bank's response, and tan(pi f / sr) at each point so that the skirt
   // filters' response is pure arithmetic in the parameter path.
   static constexpr int kGridSteps = 33;
   float mGridWeight[kNumBedBands][kGridSteps]{};
   float mGridWarp[kNumBedBands][kGridSteps]{};
   float mBpGrid[kNumBedBands][kNumBedBands][kGridSteps]{};

   // Distance turns a stretch of water into a valley of it: more events, each
   // smaller, with their edges smeared by the air they crossed.
   float mDistanceRate = 1.0f;
   float mDistanceLevel = 1.0f;
   float mDistanceSmear = 1.0f;

   float mSilenceCounter = 0.0f;
};

} // namespace riverflow
