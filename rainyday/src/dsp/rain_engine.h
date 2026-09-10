#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "../params.h"
#include "verdalis/dsp/adsr.h"
#include "verdalis/dsp/pocket.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/reverb.h"
#include "verdalis/dsp/rng.h"

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
   float slosh = 0.5f;
   float levelSpread = 0.6f;
   float chirp = 0.35f;
   float bubbleChance = 1.0f;
   int surface = 0;

   // The trickle layer: drops landing on a hard surface. The same generator
   // RiverFlow uses, so the same settings give the same sound.
   float trickleGain = 0.0316f; // linear, -30 dB
   float trickleRateHz = 16.0f;
   float trickleSizeMm = 1.24f;
   float trickleSpreadOct = 1.2f;
   float trickleDecaySec = 0.013f;
   float trickleImpact = 0.45f;
   float stoneToneHz = 3500.0f;
   float trickleSplash = 0.25f;
   float noteTracking = 0.5f;

   float bedGain = 0.25f; // linear
   float bedTone = 0.5f;
   float bedBody = 0.2f;
   float bedDrift = 0.3f;
   float bedWidth = 0.85f;
   float bedPan = 0.0f;

   float width = 0.85f; // droplet width; the bed has its own
   float dropPan = 0.0f;
   float distance = 0.3f;
   float air = 0.5f;
   float spaceAmount = 0.2f;
   float spaceSize = 0.5f;
   float spaceDamping = 0.5f;

   int filterType = 0;
   float highpassHz = 20.0f;
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

// A single impact. Five synthesis layers share one droplet:
//   * a chirped sine that swells in and decays (the "plink" of the air bubble
//     the impact traps, which is why it arrives just after the splash),
//   * a second, much quieter bubble mode near twice that frequency,
//   * a noise burst through a resonant bandpass (the wet splash),
//   * the initial impact: a two-cycle damped sine at a frequency drawn afresh
//     for every droplet,
//   * the surface's own low mode, where a struck panel or canopy rings.
struct Droplet {
   bool active = false;
   uint32_t startOffset = 0; // sample within the current block where it begins
   uint32_t life = 0, lifeMax = 0;

   float phase = 0.0f, phaseInc = 0.0f;
   // The pitch bend has two parts, because a drop falling into water makes two.
   //
   // The dip is a fast downward bend that relaxes back to 1 within a cycle or
   // two of the attack: the cavity is still opening while the splash is at its
   // loudest.
   float chirpRate = 1.0f, chirpRelax = 0.0f;
   // The rise is the bubble shrinking afterwards. Its per-sample log-frequency
   // step *grows*, so the pitch sits almost still through the loud plateau and
   // runs away through the decaying tail, which is where a real drop puts
   // nearly all of its bend.
   float chirpStep = 0.0f, chirpGrow = 1.0f;
   // Sample at which the rise is finished and the pitch holds.
   uint32_t chirpEnd = 0;
   // The tonal layer is a difference of two exponentials, which gives it a
   // short rise instead of switching on at full level like a beep.
   float tonalAmp = 0.0f, tonalDecay = 0.0f;
   float tonalRise = 0.0f, tonalRiseDecay = 0.0f;
   // Second bubble mode. Near twice the fundamental but never exactly, with its
   // own phase so it keeps drifting against it, and its own faster decay.
   float harmPhase = 0.0f, harmPhaseInc = 0.0f;
   float harmAmp = 0.0f, harmDecay = 0.0f;
   Svf resonator;
   // The splash draws from the droplet's own generator, not the engine's, so a
   // droplet that no longer needs noise can stop asking for it without shifting
   // every random number after it. Seeded from the shared generator at spawn,
   // which keeps a fixed Seed meaning one fixed rain.
   RngLite rng;
   float noiseAmp = 0.0f, noiseDecay = 0.0f;
   // The impact is a damped sine, not noise: one frequency per droplet, drawn
   // uniformly, damped hard enough that only about two cycles survive.
   float clickPhase = 0.0f, clickPhaseInc = 0.0f;
   float clickAmp = 0.0f, clickDecay = 0.0f;
   // The tack: the surface being struck. A short burst of noise through a
   // broad resonance -- Q about 6, which is what the event-triggered spectra of
   // real drops on stone measure -- with a fast part that is the strike and a
   // slower one that is the film of water on it. Like the impact and the body
   // it is the surface sounding rather than the droplet radiating, so it
   // bypasses the droplet's radiation highpass.
   Svf tackBp;
   float tackAmp = 0.0f, tackDecay = 0.0f;
   float tackWet = 0.0f, tackWetDecay = 0.0f;
   // The struck surface's own low mode: a panel, a leaf or a canopy rings in
   // the low mids where no droplet radiates. Like the impact it is the surface
   // sounding, so it bypasses the droplet's radiation highpass.
   float bodyPhase = 0.0f, bodyPhaseInc = 0.0f;
   float bodyAmp = 0.0f, bodyDecay = 0.0f, bodyRiseInv = 0.0f;
   // A panel radiates poorly below its own mode for the same reason a droplet
   // does below its bubble: it is small against the wavelength. Without this
   // the mode's spectral skirts fill the two empty octaves under it.
   Hp2 bodyHp;
   // The slosh: the sheet of water a drop throws across a hard wet surface.
   // Broadband noise well above the droplet's own resonance, swelling in over
   // a few milliseconds rather than striking, and unpitched -- it is water
   // spreading, not anything ringing. Zero on every surface but concrete, and
   // skipped for a single comparison when it is.
   Svf sloshBp;
   float sloshAmp = 0.0f, sloshDecay = 0.0f;
   float sloshRise = 0.0f, sloshRiseDecay = 0.0f;
   // A splat is not one burst. Counted on isolated events in the reference, a
   // drop landing on wet concrete throws six of them in the first 40 ms about
   // 5 ms apart, each around 11 dB under the first, and leaves stragglers out
   // to 300 ms -- secondary droplets thrown up and coming back down. One
   // smooth burst with the right spectrum and the right decay still sounds
   // like hiss, because the cascade is the sound.
   uint32_t sloshNext = 0;   // samples until the next sub-burst
   uint32_t sloshLeft = 0;   // sub-bursts still to come
   float sloshSeed = 0.0f;   // level of the first burst, for re-triggering
   OnePoleLp air;
   // A droplet is a small radiator: it cannot put out much energy far below its
   // own resonance, so everything under it is rolled off at 12 dB/oct.
   Hp2 body;
   float gainL = 0.0f, gainR = 0.0f;

   inline float peak() const {
      return (tonalAmp - tonalRise) + harmAmp + noiseAmp + clickAmp + bodyAmp;
   }
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
   double trickleTimer = 0.0; // and until the next drop on the surface

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
   static constexpr uint32_t kMaxPockets = 1024;
   static constexpr uint32_t kModInterval = 64; // control-rate for random walks

private:
   void updateFilters();
   void spawnDroplet(Voice &v, float envLevel, uint32_t offset);
   void spawnTrickle(float envLevel);
   verdalis::Pocket *allocatePocket();
   Droplet *allocateDroplet();
   void processVoiceBed(Voice &v, float *outL, float *outR, uint32_t numSamples);
   void processDroplets(float *outL, float *outR, uint32_t numSamples);
   void processPockets(float *outL, float *outR, uint32_t numSamples);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);

   float mSampleRate = 48000.0f;
   EngineParams mP;
   Rng mRng;
   // The trickle draws from its own generator, not the shared one. Anything
   // drawn from mRng shifts every number after it, so a trickle sharing it
   // would change which droplets fall and when -- and then a fixed Seed would
   // no longer promise the same rain when only the Trickle controls moved.
   Rng mTrickleRng;
   int mAppliedSeed = -1;

   Voice mVoices[kMaxVoices];
   std::vector<Droplet> mDroplets;
   verdalis::Pocket mPockets[kMaxPockets];
   uint32_t mDropletLimit = 512;
   uint32_t mDropletCursor = 0;

   Svf mFilterL, mFilterR;
   // A permanent 12 dB/oct highpass, separate from the multimode filter above:
   // that one is a tone control every preset already uses as a lowpass, and this
   // project turned out to be largely about rain having no low end.
   Hp2 mHighpassL, mHighpassR;
   bool mHighpassBypass = true;
   Space mSpace;
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
   // Equal-power balance applied to the bed after it is decorrelated.
   float mBedPanL = 1.0f, mBedPanR = 1.0f;

   // Control-rate random-walk coefficients and their unit-variance
   // normalisation factors.
   float mDriftCoef = 0.003f, mDriftNorm = 25.0f;
   float mClumpCoef = 0.01f, mClumpNorm = 14.0f;

   float mDistanceAtten = 1.0f;
   float mDensityNorm = 1.0f;
   uint32_t mSilenceCounter = 0;
};

} // namespace rainyday
