#pragma once

#include <cstdint>
#include <vector>

#include "../params.h"
#include "verdalis/dsp/adsr.h"
#include "dynamics.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/reverb.h"
#include "verdalis/dsp/rng.h"

namespace thunderclap {

// Everything the engine needs for one control block, already converted from
// raw parameter values into real units.
struct EngineParams {
   float gain = 1.0f; // linear

   float distanceKm = 2.0f;
   float heightKm = 5.0f;
   float cloudSpreadKm = 4.0f;
   float tortuosity = 0.5f;
   float branching = 0.4f;
   int strokes = 3;
   float strokeGapSec = 0.06f;
   float variation = 0.5f;

   float crack = 0.5f;
   float weight = 0.5f;
   float swell = 0.3f;
   float rumble = 0.5f;
   float rumbleTone = 0.5f;
   float air = 0.5f;
   float scatter = 0.5f;
   float focus = 0.7f;
   float impact = 0.45f;

   float width = 0.8f;
   float pan = 0.0f;
   float rumbleWidth = 0.9f;
   float drift = 0.3f;

   float echoGain = 0.25f; // linear
   int echoCount = 3;
   float echoSpreadSec = 1.5f;
   float echoDamping = 0.5f;

   float spaceAmount = 0.15f;
   float spaceSize = 0.5f;
   float spaceDamping = 0.5f;

   int filterType = 0;
   float highpassHz = 20.0f;
   float filterCutoffHz = 20000.0f;
   float filterReso = 0.1f;
   float filterKeyTrack = 0.0f;

   int mode = kModeOneShot;
   float attackSec = 0.001f;
   float releaseSec = 1.2f;
   float stormRatePerMin = 6.0f;
   float velToLevel = 0.5f;
   float velToDistance = 0.5f;

   int maxShocks = 2048;
   int seed = 0;

   float compress = 0.0f;
   float compAttackSec = 0.003f;
   float compReleaseSec = 0.25f;
};

// One radiating element of the lightning channel, as the listener will hear
// it: when its shock wave arrives, how loud, how long, how dull, and from where.
struct Arrival {
   float time;    // seconds after the flash's first audible arrival
   float amp;     // pressure amplitude, after normalisation
   float lenSec;  // duration of the N-wave
   float airHz;   // where the air has taken 6 dB off; see Shock::air
   float pan;     // -1..1
   float crackle; // how much of the front's noise burst this element keeps, 0..1
   bool branch;   // a side branch: lit by the first stroke only
};

// A shock wave in flight through the output. An N-wave -- a pressure jump,
// a linear fall through zero to the mirror value, and a jump back -- with the
// two fronts smoothed by the Crack setting, then the air's lowpass.
//
// The air is three one-poles at staggered corners: where the absorption has
// cost 6, 12 and 24 dB. Absorption grows as f^1.6 in decibels, which is gentler
// than one pole near the corner and far steeper two octaves up, and no fixed
// slope fits both a strike at 300 m, which keeps its 4 kHz, and one at 10 km,
// which has lost 50 dB there. Three poles placed on the real curve do.
struct Shock {
   bool active = false;
   // A blast runs the Friedlander shape below instead of the N-wave: the same
   // envelope of air absorption and panning, a different pressure history.
   bool blast = false;
   float blastDecay = 0.0f;      // 1 / time constant, in samples
   float blastEnv = 1.0f;        // exp(-t/T), stepped recursively
   float blastEnvCoef = 1.0f;    // exp(-1/T) per sample
   uint32_t startOffset = 0; // sample within the current block where it begins
   uint32_t life = 0, lifeMax = 0;
   uint32_t lenSamples = 1;
   float invLen = 1.0f;
   float edgeInv = 1.0f; // 1 / rise samples of each front
   float amp = 0.0f;
   // The tearing at the front: the fine roughness of the channel, which
   // radiates a spray of tiny shocks the smooth N-wave cannot carry. Held for
   // crackleStep samples at a time, because a wrinkle of the channel radiates
   // a step and not a hiss -- see kRoughnessM.
   uint32_t crackleSamples = 0;
   float crackleAmp = 0.0f;
   uint32_t crackleStep = 1;  // samples one wrinkle takes to pass
   uint32_t cracklePhase = 0; // countdown to the next one
   float crackleHold = 0.0f;  // the wrinkle being heard
   float airCoef[3] = {1.0f, 1.0f, 1.0f};
   float airState[3] = {0.0f, 0.0f, 0.0f};
   float gainL = 0.0f, gainR = 0.0f;
};

// One flash: a channel's worth of arrivals, sorted by time, replayed once per
// return stroke. The geometry is grown afresh for every flash.
struct Flash {
   static constexpr int kMaxStrokes = 8;

   bool active = false;
   int voice = -1;
   std::vector<Arrival> arrivals;
   uint32_t count = 0;
   double clock = 0.0; // samples since the flash began
   int strokes = 1;
   float strokeOffset[kMaxStrokes] = {}; // samples
   float strokeGain[kMaxStrokes] = {};
   uint32_t cursor[kMaxStrokes] = {};
   float level = 1.0f; // velocity and distance loss, applied to every shock
   float distanceKm = 1.0f;

   // The blast the near channel throws off, replayed by every return stroke.
   // Not one pulse but a short cluster of them: the near channel is several
   // coherent sections, each at its own range, and they push in turn over the
   // first few hundred milliseconds. One pulse alone is a click -- it buys a
   // tall peak and almost no energy in the band that is supposed to hit.
   static constexpr int kMaxBlasts = 8;
   int blastCount = 0;
   float blastTime[kMaxBlasts] = {}; // seconds, same clock as Arrival::time
   float blastAmp[kMaxBlasts] = {};
   float blastAirHz[kMaxBlasts] = {};
   float blastPan[kMaxBlasts] = {};
   float blastTauSec = 0.003f;
   uint8_t blastNext[kMaxStrokes] = {}; // next pulse this stroke owes

   bool exhausted() const {
      for (int k = 0; k < strokes; ++k)
         if (cursor[k] < count)
            return false;
      return true;
   }
};

// One held MIDI note. A voice owns an envelope and, in Storm mode, the clock
// that decides when the next flash comes. Flashes and shocks live in shared
// pools so the cost stays bounded however many notes are held.
struct Voice {
   bool active = false;
   int32_t noteId = -1;
   int16_t port = -1, channel = -1, key = -1;
   float velocity = 1.0f;
   bool held = false;
   int mode = kModeOneShot;

   Adsr env;
   double stormTimer = 0.0; // samples until the next flash, Storm mode only
   uint32_t flashesLit = 0; // flashes this voice has started that are not yet done
};

class ThunderEngine {
public:
   void prepare(double sampleRate, uint32_t maxBlockSize);
   void reset();

   void setParams(const EngineParams &p);

   void noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId, double velocity);
   void noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void choke(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void allSoundOff();

   // Adds the thunder into the (already zeroed) output buffers.
   void process(float *outL, float *outR, uint32_t numSamples);

   bool isSilent() const;
   uint32_t activeVoiceCount() const;
   uint32_t activeFlashCount() const;
   uint32_t activeShockCount() const;
   // Counts every flash ever lit; the window watches it to draw a bolt.
   uint32_t flashCounter() const { return mFlashCounter; }
   float tailSeconds() const;

   static constexpr uint32_t kMaxVoices = 8;
   static constexpr uint32_t kMaxFlashes = 12;
   static constexpr uint32_t kMaxShocks = 4096; // pool, and the ceiling on elements per flash
   static constexpr uint32_t kModInterval = 64; // control rate for the random walks

private:
   struct Vec3 {
      float x, y, z;
   };

   void updateFilters();
   void updateEchoes();
   void drawEchoPlaces();
   Flash *allocateFlash();
   Shock *allocateShock();
   bool lightFlash(Voice &v, int voiceIndex);
   void flashEnded(Flash &f);
   void growChannel(Flash &f, float distanceM, float azimuth, float heightM, float cloudM,
                    float tortuosity, float branching, float weightScale, float crack);
   void addElements(Flash &f, Vec3 a, Vec3 b, int count, float tortuosity, float branchScale,
                    bool branch, float weightScale, float shadowElev, float shadowWidth,
                    float heightM);
   void spawnShock(const Arrival &a, float gain, float crack, uint32_t offset);
   void spawnBlast(const Flash &f, int index, float gain, float crack, uint32_t offset);
   void processControl(float *outL, float *outR, uint32_t numSamples);
   void processShocks(float *outL, float *outR, uint32_t numSamples);
   void steepenShocks(float *busL, float *busR, uint32_t numSamples);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);

   float mSampleRate = 48000.0f;
   EngineParams mP;
   // The shock bus. The shocks are summed here rather than straight into the
   // output, because the near field's nonlinearity acts on their sum and not
   // on the rumble underneath it; see steepenShocks().
   std::vector<float> mBusL, mBusR;
   // Two generators. mRng draws everything that is an event -- channel
   // geometry, scatter, storm timing -- and mNoiseRng runs continuously for the
   // rumble noise and the drift. Kept apart so that a fixed Seed gives the same
   // thunder for the same notes however long the plugin idled before them.
   Rng mRng;
   Rng mNoiseRng;
   int mAppliedSeed = -1;

   Voice mVoices[kMaxVoices];
   Flash mFlashes[kMaxFlashes];
   std::vector<Shock> mShocks;
   uint32_t mShockCursor = 0;
   uint32_t mFlashCounter = 0;

   // The rumble: filtered noise whose level follows the energy of the shocks
   // arriving, so it sits under the crackle and fills the gaps between elements.
   float mRumbleEnergy = 0.0f;
   float mRumbleDecay = 0.999f;
   float mRumbleAirHz = 300.0f;   // running mean of the arriving shocks' air corners
   float mRumbleCutoff = 200.0f;
   Svf mRumbleLpL, mRumbleLpR;
   Hp2 mRumbleHpL, mRumbleHpR;
   float mRumbleMixA = 1.0f, mRumbleMixB = 0.0f;
   float mDriftState = 0.0f;
   float mDriftCoef = 0.003f, mDriftNorm = 25.0f;
   uint32_t mModCounter = 0;

   // Echoes: taps on a mono sum of the direct sound, each its own distance,
   // dullness and direction.
   static constexpr int kMaxEchoes = 8;
   static constexpr float kMaxEchoSec = 6.5f;
   DelayLine mEchoLine;
   size_t mEchoDelay[kMaxEchoes] = {};
   float mEchoGain[kMaxEchoes] = {};
   float mEchoPanL[kMaxEchoes] = {}, mEchoPanR[kMaxEchoes] = {};
   OnePoleLp mEchoLp[kMaxEchoes];
   float mEchoFrac[kMaxEchoes] = {};  // where each reflector sits, 0..1 of Echo Spread
   float mEchoAngle[kMaxEchoes] = {}; // and in which direction
   float mEchoFeedback = 0.0f;
   int mEchoActive = 0;

   Svf mFilterL, mFilterR;
   Hp2 mHighpassL, mHighpassR;
   bool mHighpassBypass = true;
   Space mSpace;
   Compressor mCompressor;
   int mLastKey = 60;

   float mFilterCutoff = 20000.0f;
   float mFilterWLp = 1.0f, mFilterWBp = 0.0f, mFilterWHp = 0.0f;
   bool mFilterBypass = true;

   uint32_t mSilenceCounter = 0;
};

} // namespace thunderclap
