#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "../params.h"
#include "adsr.h"
#include "filters.h"
#include "reverb.h"
#include "rng.h"

namespace rainyday {

// Everything the engine needs for one control block, already converted from
// raw parameter values into real units.
struct EngineParams {
   float gain = 1.0f;         // linear
   float densityHz = 500.0f;  // droplets per second at full envelope
   float clumping = 0.25f;
   float dropPitchHz = 600.0f;
   float pitchSpreadOct = 1.6f;
   float dropDecaySec = 0.045f;
   float decaySpread = 0.5f;
   float tonality = 0.35f;
   float impact = 0.5f;
   float splash = 0.35f;
   float levelSpread = 0.6f;
   float chirp = 0.35f;
   int surface = 0;
   float noteTracking = 0.5f;

   float bedGain = 0.25f; // linear
   float bedTone = 0.5f;
   float bedBody = 0.2f;
   float bedDrift = 0.3f;

   float width = 0.85f;
   float distance = 0.3f;
   float air = 0.5f;
   float spaceAmount = 0.2f;
   float spaceSize = 0.5f;
   float spaceDamping = 0.5f;

   int filterType = 0;
   float filterCutoffHz = 20000.0f;
   float filterReso = 0.1f;
   float filterKeyTrack = 0.0f;

   float attackSec = 0.2f;
   float decaySec = 0.6f;
   float sustain = 1.0f;
   float releaseSec = 1.2f;
   float velToLevel = 0.5f;
   float velToDensity = 0.3f;

   int maxDroplets = 512;
   int seed = 0;
};

// A single impact. Three synthesis layers share one droplet:
//   * a chirped sine that swells in and decays (the "plink" of the air bubble
//     the impact traps, which is why it arrives just after the splash),
//   * a noise burst through a resonant bandpass (the wet splash),
//   * a very short broadband click (the mechanical impact).
struct Droplet {
   bool active = false;
   uint32_t startOffset = 0; // sample within the current block where it begins
   uint32_t life = 0, lifeMax = 0;

   float phase = 0.0f, phaseInc = 0.0f;
   // Chirp is a per-sample frequency multiplier that itself relaxes back to 1,
   // so the pitch bend is front-loaded instead of crawling across the ring.
   float chirpRate = 1.0f, chirpRelax = 0.0f;
   // The tonal layer is a difference of two exponentials, which gives it a
   // short rise instead of switching on at full level like a beep.
   float tonalAmp = 0.0f, tonalDecay = 0.0f;
   float tonalRise = 0.0f, tonalRiseDecay = 0.0f;
   Svf resonator;
   float noiseAmp = 0.0f, noiseDecay = 0.0f;
   float clickAmp = 0.0f, clickDecay = 0.0f;
   OnePoleLp air;
   // A droplet is a small radiator: it cannot put out much energy far below its
   // own resonance, so everything under it is rolled off at 12 dB/oct.
   Hp2 body;
   float gainL = 0.0f, gainR = 0.0f;

   inline float peak() const { return (tonalAmp - tonalRise) + noiseAmp + clickAmp; }
};

// One held MIDI note. A voice owns an envelope, its own noise bed and its own
// droplet scheduler; droplets themselves live in a shared pool so that CPU
// cost stays bounded no matter how many notes are held.
struct Voice {
   bool active = false;
   int32_t noteId = -1;
   int16_t port = -1, channel = -1, key = -1;
   float velocity = 1.0f;
   bool held = false;

   Adsr env;
   double dropTimer = 0.0; // samples until the next droplet (double: low
                           // densities mean waits beyond float's integer range)

   Svf bedLpL, bedLpR;
   Hp2 bedHpL, bedHpR;

   // Control-rate random walks driving intensity drift and droplet clumping.
   float driftState = 0.0f, clumpState = 0.0f;
   uint32_t modCounter = 0;
};

class RainEngine {
public:
   void prepare(double sampleRate, uint32_t maxBlockSize);
   void reset();

   void setParams(const EngineParams &p);

   void noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId, double velocity);
   void noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void choke(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void allSoundOff();

   // Adds the rain into the (already zeroed) output buffers.
   void process(float *outL, float *outR, uint32_t numSamples);

   bool isSilent() const;
   uint32_t activeVoiceCount() const;
   uint32_t activeDropletCount() const;
   float tailSeconds() const;

   static constexpr uint32_t kMaxVoices = 16;
   static constexpr uint32_t kMaxDroplets = 2048;
   static constexpr uint32_t kModInterval = 64; // control-rate for random walks

private:
   void updateFilters();
   void spawnDroplet(Voice &v, float envLevel, uint32_t offset);
   Droplet *allocateDroplet();
   void processVoiceBed(Voice &v, float *outL, float *outR, uint32_t numSamples);
   void processDroplets(float *outL, float *outR, uint32_t numSamples);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);

   float mSampleRate = 48000.0f;
   EngineParams mP;
   Rng mRng;
   int mAppliedSeed = -1;

   Voice mVoices[kMaxVoices];
   std::vector<Droplet> mDroplets;
   uint32_t mDropletLimit = 512;
   uint32_t mDropletCursor = 0;

   Svf mFilterL, mFilterR;
   Fdn mSpace;
   int mLastKey = 60;

   // Cached derived values, refreshed by setParams().
   float mBedCutoff = 8000.0f;
   float mBedHpCutoff = 300.0f;
   float mBedGainComp = 1.0f;
   float mFilterCutoff = 20000.0f;
   float mFilterWLp = 1.0f, mFilterWBp = 0.0f, mFilterWHp = 0.0f;
   bool mFilterBypass = true;

   // Stereo decorrelation weights for the noise bed (a^2 + b^2 = 1, so the
   // channel correlation is cos(width * pi/2) with no level change).
   float mBedMixA = 1.0f, mBedMixB = 0.0f;

   // Control-rate random-walk coefficients and their unit-variance
   // normalisation factors.
   float mDriftCoef = 0.003f, mDriftNorm = 25.0f;
   float mClumpCoef = 0.01f, mClumpNorm = 14.0f;

   float mDistanceAtten = 1.0f;
   float mDensityNorm = 1.0f;
   uint32_t mSilenceCounter = 0;
};

} // namespace rainyday
