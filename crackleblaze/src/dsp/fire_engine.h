#pragma once

// CrackleBlaze's synthesis: fire, from a hearth to a burning roof.
//
// The model is what the reference library measures, and the first thing the
// library says is that a fire is not a river. `tools/analysis/refs.py` puts its
// median crest factor at **31.7 dB** against RiverFlow's 19.6, and its 4 ms
// envelope CV at 0.85 against 0.26. Half a river is its bed. A fire is a quiet
// bed with very loud, very short things happening on top of it, and almost
// every decision below follows from that ratio.
//
//   roar     The bed. One noise source per channel through an eight-band
//            filterbank at octave centres, driven at the measured octave-band
//            gains of whichever of the five clusters the library's *beds* fall
//            into is selected -- gated first, because at a crest factor of 31.7
//            dB the ungated spectrum of a fire is largely the spectrum of its
//            crackles.
//
//            Its flare is one walk, not eight. The measured correlation between
//            one band's 100 ms envelope and its neighbour's is 0.54 to 0.91,
//            median 0.85, where a river measures 0.07 to 0.25 -- so a common
//            component at sqrt(0.85) and an independent one at sqrt(0.15)
//            reproduces the measurement, and a fire surges as one flame.
//
//   crackle  Pyrolysis gas breaking out of heated wood. Measured 11.5-51 a
//            second, standing 6.5-20.2 dB over the bed, log-normal in amplitude
//            with a spread of 6 dB, and falling 10 dB in 2.5 ms.
//
//            It is a click and **not a ring**: the bed-subtracted
//            event-triggered spectrum has a median spectral flatness of 0.67,
//            so there is no resonator here and fitting one would be inventing
//            a physical object the recordings do not contain.
//
//   sizzle   The same event held open. Steam leaving wet wood takes a measured
//            19 ms to fall 10 dB where a dry tick takes 2.5, and the two
//            populations' spectra agree to within 2 dB per octave -- they
//            differ in duration, not in tone. What fraction of the events are
//            sizzles is the widest per-recording variable in the library,
//            0.03 to 0.57, which is why `Sap` is a parameter and not a
//            constant.
//
//   settle   A log giving way: a thump in 80-300 Hz with no top end at all.
//            Rare -- a median of six a minute -- and the only layer here that
//            is not made of the same broadband click as the others.
//
// How the crackles arrive is the finding the whole engine rests on, and it took
// three timescales to see (`tools/analysis/bursts.py`):
//
//   at 50 ms   P(interval < 50 ms) is 1.03x the exponential and the branching
//              ratio is 0.02. The arrivals are Poisson, and one crackle does
//              not make the next one more likely. No Hawkes cascade.
//   below 10 ms  1.97x the exponential, up to 4x. A crackle is a short *train*
//              of one to three pulses inside ten milliseconds -- gas breaking
//              out of the wood in stages rather than at once.
//   at 1 s     the Fano factor of the count is 3.90 and runs to 20.1 against a
//              Poisson process's 1.0. The *rate itself* wanders, on the same
//              seconds-long timescale as the bed's flare.
//
// So the spawner has three levels and no cascade: a slowly wandering rate, a
// Poisson process at that instantaneous rate, and a 1-3 pulse train per
// arrival. The wander is the same walk that drives the bed, which is why Flare
// moves the roar and the crackle rate together -- in a real fire a flare-up is
// both.
//
// Nothing below 60 Hz is generated on purpose. Eight of the twenty-five
// references carry between 27 and 60 per cent of their energy there, and in
// twenty-four of the twenty-five its envelope is uncorrelated with the fire
// above it: it is traffic, ventilation and handling noise, and synthesising it
// would be fitting the recordist's afternoon.
//
// No samples. The bed shapes are tables of eight numbers each; see the suite's
// note on what that does and does not permit.

#include "../params.h"

#include "verdalis/dsp/adsr.h"
#include "verdalis/dsp/fastmath.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/reverb.h"
#include "verdalis/dsp/rng.h"

#include <cstdint>

namespace crackleblaze {

// The bed's filterbank: one bandpass per octave from 125 Hz to 16 kHz, which
// is the resolution the shapes were measured at. The centres never change, so
// the filters are configured once and only their gains move.
constexpr int kNumBedBands = 8;

struct EngineParams {
   float gain = 1.0f; // linear

   // blaze
   int fireType = kFireLogFire;
   float fireBlend = 0.0f;
   float roarGain = 0.0282f; // linear
   float roarTilt = 0.0f;    // -1..1, +/-6 dB/oct about 1 kHz
   float roarBody = 0.5f;
   float flare = 0.55f;
   float flareRateHz = 0.55f;
   float draught = 0.15f;

   // crackle
   float crackleRateHz = 29.0f;
   float crackleGain = 0.158f; // linear
   float crackleDecaySec = 0.0018f;
   float crackleToneHz = 4000.0f;
   float crackleSpreadDb = 6.0f;
   float burst = 2.0f;
   float snap = 0.6f;
   float crackleBody = 0.30f;

   // sizzle
   float sap = 0.21f;
   float sizzleGain = 0.0708f; // linear
   float sizzleDecaySec = 0.019f;
   float sizzleToneHz = 4500.0f;
   float steam = 0.25f;

   // settle
   float settleRateHz = 0.1f;
   float settleGain = 0.0562f; // linear
   float settleToneHz = 150.0f;
   float settleDecaySec = 0.008f;

   // hearth
   int hearth = kHearthFireplace;
   float distance = 0.15f;
   float air = 0.5f;
   float width = 0.7f;
   float roarWidth = 0.45f;
   float spaceAmount = 0.25f;
   float spaceSize = 0.4f;
   float spaceDamping = 0.6f;

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
   float velToFire = 0.3f;

   int maxEvents = 1024;
   int seed = 0;
};

// One crackle, sizzle or settle.
//
// All three are the same object because the measurement says they are: the
// bed-subtracted spectra of the tick and hiss populations agree to within 2 dB
// per octave and both have a spectral flatness around 0.65, so what separates
// them is how long they last and where their energy sits -- not what kind of
// thing they are. A settle is the same generator with its band moved down to
// 80-300 Hz and its body mix at zero.
//
// Deliberately *not* a resonator. RiverFlow's Pocket rings at a Minnaert pitch
// because a bubble is a physical oscillator with a measurable Q; a crackle has
// a measured flatness of 0.67, which is a click. Giving it a resonator would
// be inventing an object the recordings do not contain.
struct FireEvent {
   bool active = false;
   // Ticks down before the event starts. Carries both the 1-10 ms offsets
   // inside a burst train and the smear that distance adds.
   int delaySamples = 0;

   float amp = 0.0f;  // current envelope
   float peak = 0.0f; // what it rises to
   float attackInc = 0.0f;
   bool rising = true;
   float decay = 0.0f; // multiplicative, per sample

   Svf band;  // the event's own colour
   Svf body;  // its low end, an octave and a half down
   Svf steam; // a sizzle's narrow jet; unused by the others
   float bodyMix = 0.0f;
   float steamMix = 0.0f;

   RngLite rng;
   float panL = 0.70710678f, panR = 0.70710678f;
};

// One held note: its own fire. The bed is per-voice rather than global so that
// a second note is a second fire rather than the same one twice as loud, and
// so the envelope and velocity reach it at all.
struct Voice {
   bool active = false;
   int16_t port = 0, channel = 0, key = 60;
   int32_t noteId = -1;
   float velocity = 1.0f;

   Adsr env;

   // The bed: one noise source per channel through the octave filterbank.
   Svf bandL[kNumBedBands], bandR[kNumBedBands];
   // The steep ends of a shape. A bank of octave-wide bandpasses cannot make a
   // skirt steeper than about 6 dB/octave, and two of the five measured shapes
   // have one -- Open Flame falls 7 dB from 250 Hz to 125, Stove Draught rises
   // 21 dB from 1 kHz to 16. These two make those, and the bank is then solved
   // for what is left.
   Hp2 hpL, hpR;
   Lp2 lpL, lpR;
   RngLite rngCommon, rngL, rngR;

   // The flare. One common walk for the whole fire and one small independent
   // walk per band, mixed at sqrt(0.85) and sqrt(0.15) so that the correlation
   // between any two bands comes out at the measured 0.85. Run at the control
   // rate; at a 0.55 Hz corner there is nothing above it to miss.
   OnePoleLp flareCommon;
   OnePoleLp flareBand[kNumBedBands];
   float flareGainL[kNumBedBands]{};
   float flareGainR[kNumBedBands]{};
   // The common walk's current value, which the crackle clock reads as well as
   // the bed does. That sharing is the point: a flare-up is more roar *and*
   // more crackling, and it is what produces the measured Fano factor of 3.9.
   float flareValue = 0.0f;
   RngLite rngFlare;
   uint32_t modCounter = 0;

   // The draught: air pulled into the fire, as a steady hiss over the roar.
   Hp2 draughtL, draughtR;

   // Samples until the next crackle and the next settle. Sizzles share the
   // crackle clock and are split off it by Sap, because the measurement treats
   // them as one population divided by how wet the wood is.
   double crackleTimer = 0.0;
   double settleTimer = 0.0;

   inline bool finished() const { return env.isIdle(); }
};

class FireEngine {
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
   // What the window's activity meter counts: crackles, sizzles and settles
   // sounding.
   uint32_t activeEventCount() const;
   // Monotonic count of crackles, which is what the header ornament draws.
   uint32_t crackleCounter() const { return mCrackleCounter; }
   float tailSeconds() const;

   // A fire is a bed, and one of them costs 16 bandpasses a sample, so the
   // voice pool is deliberately smaller than the suite's usual sixteen.
   static constexpr uint32_t kMaxVoices = 8;
   // Larger than RiverFlow's pool because a fire spawns far more events a
   // second than a river does -- up to 51 crackles a second, each of them a
   // burst of two or three.
   static constexpr uint32_t kMaxEvents = 2048;
   static constexpr uint32_t kModInterval = 64; // control rate for the flare

private:
   void updateFilters();
   // Events are pooled across voices and drawn from the engine's own generator,
   // so neither spawn needs the voice that asked for it.
   void spawnCrackle(float envLevel, float fire, bool sizzle);
   void spawnSettle(float envLevel, float fire);
   FireEvent *allocateEvent();
   void processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples);
   void processEvents(float *outL, float *outR, uint32_t numSamples);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);

   EngineParams mP;
   double mSampleRate = 48000.0;

   Voice mVoices[kMaxVoices];
   FireEvent mEvents[kMaxEvents];

   Rng mRng;
   int mAppliedSeed = 0;
   uint32_t mCrackleCounter = 0;

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
   // Turns the draught's highpassed noise into unit RMS, so Draught is a level
   // against the roar rather than a filter setting. Sample-rate dependent, so
   // it is integrated in updateFilters() rather than being a constant.
   float mDraughtNorm = 1.0f;

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
   // Everything here is fixed at prepare(): the grid, the trapezoid weights,
   // the bank's response, and tan(pi f / sr) at each point so that the skirt
   // filters' response is pure arithmetic in the parameter path.
   static constexpr int kGridSteps = 33;
   float mGridWeight[kNumBedBands][kGridSteps]{};
   float mGridWarp[kNumBedBands][kGridSteps]{};
   float mBpGrid[kNumBedBands][kNumBedBands][kGridSteps]{};

   // Distance turns one fire into a fire heard across a field: fewer of its
   // crackles resolve separately, each is smaller, and their edges are smeared
   // by the air they crossed.
   float mDistanceRate = 1.0f;
   float mDistanceLevel = 1.0f;
   float mDistanceSmear = 1.0f;

   float mSilenceCounter = 0.0f;
};

} // namespace crackleblaze
