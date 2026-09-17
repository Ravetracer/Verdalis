#pragma once

// InsectSwarm's synthesis: flying insects, one at a time or sixty-four at once.
//
// The model is what the reference library measures, and the library's first
// answer was a surprise. `tools/analysis/swarm.py` puts a honeybee's
// harmonic-to-noise ratio at **4.3 dB** and a mosquito's at **12.8**. A bee is
// very nearly as much turbulence as tone. That is why a stack of oscillators
// never sounds like one, and it is the single fact the whole engine is built
// around.
//
//   swarm    Many individuals beating at once. Each is a two-pulse-per-cycle
//            excitation -- the downstroke and the upstroke -- plus turbulent
//            air, through the shelf and resonance fitted to that species'
//            measured harmonic stack.
//
//            The crowd needs no separate mechanism. A single close insect
//            measures 8-13 dB harmonic-to-noise; the library's hive and swarm
//            recordings measure 0-2 dB at the same clarity. Many fundamentals
//            scattered over the measured 43-103 cents *are* noise, and putting
//            enough of them together produces the measurement without anything
//            being added to make it happen.
//
//   flyby    One individual on a straight trajectory past the listener, which
//            is what makes an insect read as moving rather than as getting
//            louder. Geometry, not an envelope: distance r(t) = hypot(d0, vt)
//            sets the level, the air absorption, the pan and the Doppler
//            together, and the two measured numbers -- a 13.3 dB rise over a
//            1.37 s pass -- fix d0 and the trajectory's length.
//
//            The Doppler is in it because it is physically right, not because
//            it is audible. `tools/analysis/flyby.py` is blunt about that: at
//            the measured 3.2 m/s a pass shifts the wingbeat by 30 cents,
//            while the same insects' rates wander by 43 to 103 cents on their
//            own. It is buried, and the recordings that appear to show a large
//            Doppler are showing an insect changing gear.
//
//   stridulate  A different mechanism, so a different layer. A cicada's tymbal
//            buckles and a cricket's scraper crosses a file; both are a train
//            of clicks ringing a resonant body, which is a carrier and a pulse
//            rate rather than a fundamental and a stack. Measured 5549 Hz at
//            Q 13 and 268 clicks a second for a cicada, 4518 Hz at Q 26 and 36
//            clicks a second for a cricket -- twice as sharp and seven times
//            slower, which is the whole distance between a dry rattle and a
//            pure whistling trill.
//
//   bed      The field they are in. Off by default: what it models is the
//            recordist's afternoon rather than the insect.
//
// The species table is generated from the library by tools/analysis/species.py
// and lives in species_generated.h. Each row carries two measurements -- the
// wingbeat rate and the harmonic-to-noise ratio -- and four fitted
// coefficients, fitted to that species' median harmonic stack against the
// discrete-time responses of the very filters below, so what is shipped is a
// shape the engine can actually make. The residuals run 1.4 to 3.7 dB RMS
// across stacks 25 dB deep.
//
// Dragonfly is the row that says what the library would not give. Only 8 per
// cent of its frames are periodic at all, against 56 to 94 for every other
// species: it is a clatter of wings and not a buzz, and it is shipped as the
// broad, barely-pitched thing it measures as rather than forced into a tone
// the recordings do not contain.
//
// No samples. The species table is 7 rows of numbers; see the suite's note on
// what that does and does not permit.

#include "../params.h"
#include "species_generated.h"

#include "verdalis/dsp/adsr.h"
#include "verdalis/dsp/fastmath.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/reverb.h"
#include "verdalis/dsp/rng.h"

#include <cstdint>

namespace insectswarm {

// The window's Species chip and the generated table have to name the same
// things in the same order, and nothing else would notice if they drifted.
static_assert(kNumSpecies == static_cast<int>(kNumSpeciesKinds),
              "SpeciesKind and the generated species table disagree");

// Speed of sound, for the flyby's Doppler. The same 343 m/s the analysis used
// when it turned a measured frequency ratio back into a flight speed.
constexpr float kSpeedOfSound = 343.0f;

struct EngineParams {
   float gain = 1.0f; // linear

   // swarm
   int species = kSpeciesHoneybee;
   float rateShiftSemis = 0.0f;
   int count = 12;
   float swarmGain = 0.1995f; // linear
   float spreadCents = 70.0f;
   float wanderCents = 18.0f;
   float wanderRateHz = 3.0f;
   float roamDb = 4.0f;
   float roamRateHz = 0.4f;
   float rasp = 0.5f;

   // wing
   float tilt = 0.0f;
   float formantSemis = 0.0f;
   float resonance = 0.0f;
   float stroke = 0.0f;
   float bite = 0.45f;
   float flutter = 0.25f;

   // flyby
   float flybyRateHz = 0.2f;
   float flybyGain = 0.1259f; // linear
   float flybyRiseDb = 13.3f;
   float flybyPassSec = 1.37f;
   float flybySpeed = 3.2f;
   float flybySweep = 0.8f;

   // stridulate
   float stridGain = 0.0f; // linear
   float carrierHz = 5549.0f;
   float carrierQ = 13.2f;
   float pulseRateHz = 268.4f;
   float echemeRateHz = 12.54f;
   float duty = 0.48f;
   int chorus = 6;
   float stridSpread = 0.5f;

   // air
   float distance = 0.2f;
   float air = 0.5f;
   float width = 0.75f;
   float spaceAmount = 0.12f;
   float spaceSize = 0.55f;
   float spaceDamping = 0.5f;

   // bed
   float bedGain = 0.0f; // linear
   float bedToneHz = 1200.0f;
   float bedTilt = -0.2f;

   // filter
   int filterType = 0;
   float highpassHz = 80.0f;
   float filterCutoffHz = 20000.0f;
   float filterReso = 0.1f;
   float filterKeyTrack = 0.0f;

   // envelope
   float attackSec = 0.3f;
   float decaySec = 0.4f;
   float sustain = 1.0f;
   float releaseSec = 0.9f;
   float velToLevel = 0.4f;
   float velToSwarm = 0.3f;

   int maxIndividuals = 128;
   int seed = 0;
};

// One flying individual.
//
// The excitation is two pulses a cycle rather than one, because a wing pushes
// air on the downstroke and again on the upstroke. When the two are equal the
// odd harmonics cancel and the buzz sits an octave above the wingbeat rate --
// which is not a bug but the measurement: a bumblebee's second harmonic is
// 11.7 dB *above* its fundamental and a hornet's 10.7 dB above, while a
// mosquito's is 0.3 dB below. Stroke is how unequal the two are.
//
// The pulse is (1 - u^2)^2 over its own half-width, which has a continuous
// first derivative at both ends -- so narrowing it brightens the buzz instead
// of adding a click -- and a mean known in closed form, which is subtracted so
// the excitation carries no DC into the shelf below it.
struct Flyer {
   bool active = false;

   float phase = 0.0f; // wingbeat cycle, 0..1
   // Its own place in the swarm, drawn once at spawn and kept: where it sits in
   // the rate spread, where it sits across the field, and how close it is.
   // These are unit draws and nothing more -- every parameter that scales them
   // is applied in refreshFlyer(), so that moving Spread or Width moves the
   // swarm rather than reshuffling it, and so that a host which sends its
   // parameter values after the first note-on still gets them.
   float dRate = 0.0f, dPan = 0.0f, dAmp = 0.0f;
   float rateHz = 220.0f; // its own rate, before wander
   float rateMul = 1.0f;  // the wander, as a multiplier
   float halfWidth = 0.1f;
   float pulseMean = 0.0f; // the pulse's own DC, subtracted
   float strokeAmp = 0.0f; // the upstroke's amplitude against the downstroke's

   float noiseAmp = 0.35f;
   // The turbulence is shed by the stroke, so it carries the stroke's
   // bandwidth and not the whole spectrum. Without this the noise floor is
   // flat where the pulse train has rolled off, and it props the top of the
   // harmonic stack up by more than 10 dB -- a rendered hornet measured -11.6
   // dB at its sixth harmonic against the library's -22.7. The corner is the
   // pulse's own -3 dB point, which sits at 0.36 * rate / halfWidth whatever
   // the two are.
   Lp2 noiseLp;
   float flutter = 0.0f;

   // The species' fitted shaper: a one-pole shelf about 300 Hz, then an SVF
   // bandpass crossfaded in.
   OnePoleLp shelf;
   float shelfLow = 1.0f, shelfHigh = 1.0f;
   Svf formant;
   float formantMix = 1.0f;
   OnePoleHp dcBlock;

   OnePoleLp wanderLp;
   // Roam: the individual closing on the listener and backing off again, as a
   // level drift. Its own filter and its own draw, so it is uncorrelated with
   // the rate wander and with every other individual's.
   OnePoleLp roamLp;
   float roamGain = 1.0f;
   RngLite rng;

   float amp = 1.0f;
   float panL = 0.70710678f, panR = 0.70710678f;

   // Flyby trajectory. `pass` is false for a swarm member, which simply hangs
   // in the field at a fixed level and pan.
   bool pass = false;
   float t = 0.0f;      // seconds from the closest point of approach
   float tEnd = 1.0f;   // where the trajectory is abandoned
   float d0 = 1.0f;     // closest distance, metres
   float speed = 3.2f;  // metres a second
   float sweep = 0.8f;
   Lp2 airL, airR;      // distance absorption, per side
};

// One calling cicada or cricket.
//
// Deliberately not a Flyer with different numbers. A tymbal is a click train
// ringing a body, so there is no fundamental to put a harmonic stack on: the
// pitch you hear is the resonator's, and the pulse rate is heard as a rattle
// or a trill over it. Modelling it as a wingbeat would have put the carrier at
// harmonic twenty of a 268 Hz fundamental and made the Q meaningless.
struct Stridulator {
   bool active = false;
   float pulsePhase = 0.0f, pulseInc = 0.0f;
   float echemePhase = 0.0f, echemeInc = 0.0f;
   float duty = 0.48f;
   float carrierHz = 5549.0f;
   // Its own place in the chorus, drawn once and kept, so that moving Scatter
   // or Carrier moves every caller together rather than reshuffling them.
   float dCarrier = 0.0f, dPulse = 0.0f, dEcheme = 0.0f;
   Svf body;
   float amp = 1.0f;
   float panL = 0.70710678f, panR = 0.70710678f;
   RngLite rng;
};

// One held note: its own swarm. The individuals belong to the voice rather
// than to a global pool, because unlike a crackle or a droplet they sound for
// as long as the note does and have to follow its envelope the whole way.
struct Voice {
   bool active = false;
   int16_t port = 0, channel = 0, key = 60;
   int32_t noteId = -1;
   float velocity = 1.0f;

   Adsr env;

   static constexpr int kMaxFlyers = 64;
   static constexpr int kMaxPasses = 4;
   static constexpr int kMaxStrid = 32;

   Flyer flyers[kMaxFlyers];
   Flyer passes[kMaxPasses];
   Stridulator strid[kMaxStrid];
   int numFlyers = 0;
   int numStrid = 0;

   // The bed: one noise source per channel, band-shaped and tilted.
   Svf bedL, bedR;
   OnePoleLp bedTiltL, bedTiltR;
   RngLite rngBed;

   double flybyTimer = 0.0;
   uint32_t epoch = 0;
   uint32_t stridEpoch = 0;
   uint32_t tag = 1; // seeds this note's individuals, deterministically
   RngLite rng;
   uint32_t modCounter = 0;

   inline bool finished() const { return env.isIdle(); }
};

class SwarmEngine {
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
   // What the window's activity meter counts: individuals sounding, across
   // every held note -- swarm members, passes and callers together.
   uint32_t activeEventCount() const;
   // Monotonic count of flybys, which is what the header ornament draws.
   uint32_t flybyCounter() const { return mFlybyCounter; }
   float tailSeconds() const;

   // A swarm is expensive per note -- up to 64 individuals, each with a shelf
   // and a resonator -- so the note pool is the smallest in the suite. Max
   // Individuals caps the total across all four.
   static constexpr uint32_t kMaxVoices = 4;
   static constexpr uint32_t kModInterval = 64; // control rate for the wander

private:
   void updateFilters();
   void configureFlyer(Flyer &f, const Voice &v, float fire, bool pass);
   // Re-derives an individual's rate, excitation and shaper from the current
   // parameters, keeping its own place in the swarm and its own phase. Run at
   // the control rate, and only when something it reads has actually moved --
   // it costs two tan() per individual, which is not a per-sample price.
   void refreshFlyer(Flyer &f, const Voice &v, float fire);
   // How many individuals this note should have flying, and getting there from
   // however many it has: newly needed ones are spawned, surplus ones are
   // dropped. Run at the control rate, so Count is a live control rather than
   // something fixed when the note arrived.
   void refreshSwarmSize(Voice &v, float fire);
   // Measures what one individual actually comes out at, by running the two
   // halves of its excitation through throwaway copies of the shaper: the
   // stroke pulses alone, and unit-amplitude turbulence alone. See mVoiceNorm
   // and mNoiseAmp.
   void calibrate(float &rmsPulse, float &rmsNoiseUnit) const;
   // The same idea for the stridulation layer. A click into a resonator rings
   // with an amplitude set by the carrier and an energy set by the Q, so
   // without this both of those knobs are level controls as much as timbre
   // ones -- and the 2:1 in Q between a cicada and a cricket is 3 dB of it.
   float calibrateStrid() const;
   // The stridulators have the same problem the individuals do and the same
   // answer: everything they use is set up when the note arrives, so without
   // this a note held while a knob moves keeps the settings it was born with --
   // and a host that sends its parameter values after the first note-on gets
   // the factory defaults for the whole layer.
   void refreshStrid(Voice &v) const;
   void spawnPass(Voice &v, float fire);
   void processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);
   int individualBudget() const;

   EngineParams mP;
   double mSampleRate = 48000.0;

   Voice mVoices[kMaxVoices];

   Rng mRng;
   int mAppliedSeed = 0;
   uint32_t mFlybyCounter = 0;

   Space mSpace;
   Svf mOutFilterL, mOutFilterR;
   Hp2 mOutHpL, mOutHpR;
   Lp2 mAirLpL, mAirLpR; // distance: air absorption
   bool mAirActive = false;
   bool mOutFilterActive = true;
   OnePoleLp mDistanceTiltL, mDistanceTiltR;

   // The selected species row, with the window's offsets already applied, so
   // the per-sample path reads plain numbers.
   float mRateHz = 221.2f;
   float mShelfLow = 1.0f, mShelfHigh = 1.0f;
   float mFormantHz = 306.8f;
   float mFormantRes = 0.94f;
   float mFormantMix = 1.0f;
   float mNoiseAmp = 0.35f;
   float mNoiseCutHz = 800.0f;
   float mStridNorm = 1.0f;
   float mLastStridSig[4] = {};
   float mHalfWidth = 0.1f;
   float mPulseMean = 0.0f;
   // The reciprocal of one individual's RMS, so that Swarm Level is a level
   // and not a function of Bite, Rasp, Flutter and whichever species is
   // selected. It cannot be worked out in closed form -- the excitation is a
   // pulse train whose spectrum depends on its own width, and the shaper is a
   // resonator that keeps a narrow slice of it, so the throughput swings by
   // nearly 30 dB across the table -- so it is measured instead, once per
   // parameter change, by calibrate().
   float mVoiceNorm = 1.0f;
   float mSwarmNorm = 1.0f;
   // Bumped whenever one of the derived values above changes, so a held note
   // picks a knob up without every block paying for a filter redesign.
   uint32_t mEpoch = 1;
   float mLastSig[14] = {};
   uint32_t mStridEpoch = 1;

   float mDistanceLevel = 1.0f;

   float mSilenceCounter = 0.0f;
};

} // namespace insectswarm
