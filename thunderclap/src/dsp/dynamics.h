#pragma once

#include "verdalis/dsp/fastmath.h"

namespace thunderclap {

// A stereo-linked feed-forward compressor with a soft knee and automatic
// make-up gain. One control, Compress, moves the threshold down and the ratio
// up together, and the make-up is chosen so that a full-scale peak still comes
// out at full scale: what compression does here is lift everything under the
// crack -- the rumble, the far claps, the echo tail -- towards it, which is how
// a thunder heard through a microphone that was already clipping on the crack
// sounds: dense, and the earth keeps shaking after the sky has stopped.
class Compressor {
public:
   void prepare(float sampleRate) {
      mSampleRate = sampleRate;
      reset();
   }

   void reset() { mEnvelope = 0.0f; }

   // amount 0..1; attack and release in seconds.
   void setParams(float amount, float attackSec, float releaseSec) {
      mAmount = clampv(amount, 0.0f, 1.0f);
      mBypass = mAmount < 0.001f;
      // Threshold from 0 dB down to -24 dB, ratio from 1:1 up to 4:1. Gentle
      // on purpose: a hard ratio flattens the decay of the tail, and a thunder
      // that never quite ends is worse than one that was never compressed.
      mThresholdDb = -24.0f * mAmount;
      mRatio = 1.0f + 3.0f * mAmount;
      mKneeDb = 6.0f;
      mAttackCoef = onePoleCoef(std::max(attackSec, 0.00005f), mSampleRate);
      mReleaseCoef = onePoleCoef(std::max(releaseSec, 0.001f), mSampleRate);
      // Make-up so that 0 dBFS in is 0 dBFS out.
      mMakeupDb = -gainReductionDb(0.0f);
   }

   inline void tick(float &l, float &r) {
      if (mBypass)
         return;
      const float peak = std::max(std::fabs(l), std::fabs(r));
      // Peak follower: fast up, slow down, in the linear domain.
      const float coef = peak > mEnvelope ? mAttackCoef : mReleaseCoef;
      mEnvelope += coef * (peak - mEnvelope);
      const float levelDb = mEnvelope > 1.0e-6f ? 20.0f * std::log10(mEnvelope) : -120.0f;
      const float gainDb = gainReductionDb(levelDb) + mMakeupDb;
      const float g = std::pow(10.0f, gainDb * 0.05f);
      l *= g;
      r *= g;
   }

   bool bypassed() const { return mBypass; }

private:
   // Negative or zero: how many dB come off a signal at this level.
   inline float gainReductionDb(float levelDb) const {
      const float over = levelDb - mThresholdDb;
      const float half = 0.5f * mKneeDb;
      float reduced;
      if (over <= -half)
         reduced = 0.0f;
      else if (over >= half)
         reduced = over * (1.0f / mRatio - 1.0f);
      else {
         // Quadratic knee between the two straight parts.
         const float x = over + half;
         reduced = (1.0f / mRatio - 1.0f) * x * x / (2.0f * mKneeDb);
      }
      return reduced;
   }

   float mSampleRate = 48000.0f;
   float mAmount = 0.0f;
   bool mBypass = true;
   float mThresholdDb = 0.0f, mRatio = 1.0f, mKneeDb = 6.0f, mMakeupDb = 0.0f;
   float mAttackCoef = 0.1f, mReleaseCoef = 0.001f;
   float mEnvelope = 0.0f;
};

} // namespace thunderclap
