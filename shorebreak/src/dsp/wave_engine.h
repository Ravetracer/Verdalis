#pragma once

// ShoreBreak's synthesis: an ocean beach, built wave by wave.
//
// The model comes from what the reference library actually measures (see
// tools/analysis/README.md). A breaking wave is not one sound but four, and
// they overlap:
//
//   break  The crest collapsing into a cloud of bubbles. Broadband noise
//          through a bandpass centred where the cloud resonates -- measured at
//          400-1600 Hz -- sweeping downwards as the cloud grows and coarsens.
//          It rises over 0.25-1.2 s rather than striking, and falls in
//          0.3-0.8 s. A low-weighted copy underneath is the surf rumble.
//
//   foam   The sheet of small bubbles left behind. Highpassed hard: measured
//          -82 dB at 50 Hz, so it has no low end whatsoever. It outlives the
//          break by up to three times, which is why the quiet part between
//          waves measures *brighter* than the break itself.
//
//   wash   The water running up the shore and draining back through whatever
//          the shore is made of. A mid band with a slow random walk on it; the
//          coarser the shore, the more it rattles.
//
//   bubbles  Individual resonators popping in the foam, each ringing at the
//          pitch its radius gives it.
//
// Under all of it sits the swell bed: water moving without breaking, slowly
// breathing. At distance it is most of what is left, because air absorption has
// taken the top off everything else.
//
// No samples, and no wind or rain: those live in other plugins in the suite.

#include "../params.h"

#include "verdalis/dsp/adsr.h"
#include "verdalis/dsp/fastmath.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/reverb.h"
#include "verdalis/dsp/rng.h"

#include <cstdint>
#include <vector>

namespace shorebreak {

struct EngineParams {
   float gain = 1.0f; // linear

   // surf
   float wavePeriodSec = 3.8f;
   float setVariation = 0.35f;
   float waveSize = 0.55f;
   float sizeVariation = 0.45f;
   float breakAttackSec = 0.4f;
   float breakDecaySec = 0.56f;
   float breakToneHz = 800.0f;
   float breakBody = 0.4f;
   float crestSweep = 0.5f;
   int breakerType = kBreakerPlunging;
   float precursor = 0.3f;
   float bubbleMix = 0.82f;

   // foam
   float foamGain = 0.4f; // linear
   float foamDecaySec = 1.5f;
   float foamToneHz = 2500.0f;
   float foamDelaySec = 0.15f;
   float fizz = 0.5f;
   float foamBubbles = 0.7f;

   // swell
   float swellGain = 0.126f; // linear
   float swellToneHz = 400.0f;
   float swellDepth = 0.45f;
   float swellRatePerMin = 6.0f;
   float swellWidth = 0.9f;

   // bubbles
   float bubbleRateHz = 40.0f;
   float bubblePitchHz = 1200.0f;
   float bubbleSpreadOct = 2.2f;
   float bubbleDamping = 1.0f;

   // wash
   float washGain = 0.2f; // linear
   float washDecaySec = 1.6f;
   float washToneHz = 1200.0f;
   float sand = 0.5f;

   // shore
   int shore = kShoreSand;
   float distance = 0.3f;
   float air = 0.5f;
   float width = 0.85f;
   float spaceAmount = 0.18f;
   float spaceSize = 0.55f;
   float spaceDamping = 0.5f;

   // filter
   int filterType = 0;
   float highpassHz = 20.0f;
   float filterCutoffHz = 20000.0f;
   float filterReso = 0.1f;
   float filterKeyTrack = 0.0f;

   // envelope
   float attackSec = 1.2f;
   float decaySec = 0.6f;
   float sustain = 1.0f;
   float releaseSec = 2.5f;
   float velToLevel = 0.5f;
   float velToSize = 0.3f;

   int maxWaves = 128;
   int seed = 0;
};

// One breaking wave, from the crest to the last of its foam. Pooled: a wave is
// walked every sample while it is alive, so the struct is kept small and free
// of anything that would make it expensive to skip.
struct Wave {
   bool active = false;

   // Decay is set from the wave's own size: bigger breakers decay more slowly,
   // -7 dB/s at 1.6-2.0 m against -4.5 dB/s at 2.4-2.7 m.
   // The break: a bandpass on noise, sweeping down as the cloud grows.
   Svf breakBand;
   OnePoleLp bodyLp;
   float breakLevel = 0.0f;   // peak level of this wave
   float breakEnv = 0.0f;     // current envelope value
   float breakAttackInc = 0.0f;
   float breakDecayCoef = 0.0f;
   bool breakRising = true;
   float toneHz = 800.0f;
   // How far open the break's band is, 0 while the wave is still coming and 1
   // once it has collapsed. A wave approaching is deep -- the mass of water
   // moving, and the cloud mode under it -- and the top of the spectrum only
   // appears when it breaks, which is heard as a filter opening.
   float openness = 0.0f;
   float openDepth = 2.0f;   // how many times lower the approach is
   float openCoef = 0.0f;    // how quickly it closes again afterwards
   float body = 0.0f;

   // The slope above 1.5 kHz. Klusek & Lisimenka measure it steepening to about
   // -10 dB/oct in the first second of breaking and relaxing to -5..-6 dB/oct
   // afterwards, so it is animated rather than fixed: one pole gives -6, two
   // give -12, and the crossfade between them is the slope.
   OnePoleLp slopeLp1;
   OnePoleLp slopeLp2;
   float slopeMix = 0.0f;     // 0 = -6 dB/oct, 1 = -12 dB/oct
   float slopeTarget = 0.5f;
   float slopeRelax = 0.0f;

   // A breaker bubbles before it collapses.
   float preEnv = 0.0f;
   float preLevel = 0.0f;
   float preInc = 0.0f;

   // The foam: highpassed noise with a delay and a long decay of its own.
   // The foam is a band, not a shelf: highpassed because it has no low end at
   // all, and lowpassed above because a sheet of bubbles rolls off with
   // frequency. White noise left unbounded above would climb 3 dB per octave
   // and swamp everything, which is not what the references measure.
   Hp2 foamHp;
   Lp2 foamLp;
   float foamLevel = 0.0f;
   float foamEnv = 0.0f;
   float foamAttackInc = 0.0f;
   float foamDecayCoef = 0.0f;
   bool foamRising = true;
   int foamDelaySamples = 0;

   // The collective mode of the whole bubble cloud. Xue et al. show the lowest
   // mode of a cloud of N bubbles falls as f0 / cbrt(N), so a thousand bubbles
   // ring an order of magnitude below one -- which is where surf rumble comes
   // from. It is not a lowpass of the break, it is a resonance of its own.
   Svf cloudBand;
   float cloudLevel = 0.0f;

   // The foam runs its own, faster and higher cascade: smaller bubbles.
   float foamBubbleTimer = 0.0f;
   float foamPitchScale = 2.0f;

   // The wash: a mid band with a slow walk on it.
   Svf washBand;
   float washLevel = 0.0f;
   float washEnv = 0.0f;
   float washAttackInc = 0.0f;
   float washDecayCoef = 0.0f;
   bool washRising = true;
   float washWalk = 0.0f;
   float washWalkCoef = 0.0f;

   // Where this wave sits in the stereo field, and its own noise source.
   float panL = 0.7071f, panR = 0.7071f;
   RngLite rng;

   // The cascade. A break is mostly the sound of bubbles being formed, so the
   // spawn rate follows the break's own envelope: fastest as the crest
   // collapses, thinning out as the foam that follows it decays.
   float bubbleTimer = 0.0f;
   // Newly formed bubbles are small and ring high; larger ones appear and
   // coalesce as the cloud develops, so the size distribution slides downwards.
   // Klusek & Lisimenka see the same thing as a mean frequency that jumps at the
   // plunging instant and falls afterwards.
   float pitchScale = 1.0f;
   float pitchScaleCoef = 0.0f;

   // Done when nothing is still rising and everything has faded out.
   inline bool finished() const {
      return !breakRising && !foamRising && !washRising && breakEnv < 1.0e-5f &&
             foamEnv < 1.0e-5f && washEnv < 1.0e-5f;
   }
};

// One bubble in the foam: a resonator at the pitch its radius gives it. Four
// bytes of RNG state rather than a full generator, because there can be
// hundreds alive and they are walked every sample.
// One bubble: a damped harmonic oscillator, which is what Xue et al.'s
// equation (2) reduces to once it has been struck. Its impulse response is a
// decaying sinusoid, so that is what is generated -- directly, rather than by
// ringing a filter, which gives exact control of level, pitch and decay for the
// price of one table lookup a sample.
struct Bubble {
   bool active = false;
   float phase = 0.0f;
   float inc = 0.0f;
   float level = 0.0f;
   float decayCoef = 0.0f;
   // The entrainment transient: a few milliseconds of noise as the bubble is
   // pinched off, which is the click under the tone.
   float clickLevel = 0.0f;
   float clickCoef = 0.0f;
   float panL = 0.7071f, panR = 0.7071f;
   RngLite rng;
};

// One held note: its own envelope, its own swell bed, its own wave clock.
struct Voice {
   bool active = false;
   int16_t port = 0, channel = 0, key = 60;
   int32_t noteId = -1;
   float velocity = 1.0f;

   Adsr env;

   // The swell bed: two decorrelated noise bands, slowly breathing.
   Svf swellBandL, swellBandR;
   OnePoleHp swellHpL, swellHpR;
   float swellPhase = 0.0f;
   float swellInc = 0.0f;

   // Samples until the next wave breaks.
   double waveTimer = 0.0;

   inline bool finished() const { return env.isIdle(); }
};

class WaveEngine {
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
   // What the window's activity meter counts.
   uint32_t activeWaveCount() const;
   float tailSeconds() const;

   static constexpr uint32_t kMaxVoices = 16;
   static constexpr uint32_t kMaxWaves = 512;
   static constexpr uint32_t kMaxBubbles = 1024;
   static constexpr uint32_t kModInterval = 64; // control rate for slow walks

private:
   void updateFilters();
   void spawnWave(Voice &v, float envLevel);
   void spawnBubble(const Wave &w, float level, float pitchScale);
   Wave *allocateWave();
   Bubble *allocateBubble();
   void processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples);
   void processWaves(float *outL, float *outR, uint32_t numSamples);
   void processBubbles(float *outL, float *outR, uint32_t numSamples);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);

   EngineParams mP;
   double mSampleRate = 48000.0;

   Voice mVoices[kMaxVoices];
   Wave mWaves[kMaxWaves];
   Bubble mBubbles[kMaxBubbles];

   Rng mRng;
   int mAppliedSeed = 0;

   Space mSpace;
   Svf mOutFilterL, mOutFilterR;
   Hp2 mOutHpL, mOutHpR;
   Lp2 mAirLpL, mAirLpR; // distance: air absorption
   OnePoleLp mDistanceTiltL, mDistanceTiltR;

   float mSilenceCounter = 0.0f;
};

} // namespace shorebreak
