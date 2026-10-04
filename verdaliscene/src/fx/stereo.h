#pragma once

// The stereo effects: a widener and an auto-pan.
//
// The widener works in mid/side (musicdsp "Stereo width control"): the side
// signal is scaled by Width, 0 % folding the layer to mono and 200 % doubling
// what difference it has. Two things are added for sources that have little
// difference to scale -- a layer recorded close to mono, a single bird:
//
//  - Decorrelation makes a side signal out of the mid one, through a cascade of
//    allpasses, and adds it to the left and subtracts it from the right. That
//    spreads different frequencies to different places without a delay a
//    listener would hear as the image pulling to one speaker (DAFX sec. 5.3.2),
//    and L + R is still exactly twice the mid: it cannot cancel in mono.
//  - Bass Mono high-passes only the side signal, 24 dB/oct. Everything below
//    the corner is centred, and the mid -- most of the energy -- is never
//    filtered, so nothing has to be split and summed back in phase.
//
// The auto-pan moves the whole layer across the field with a constant-power
// law (DAFX eq. 5.3), as a sine, a triangle or a slow random drift -- a flock
// passing, wind moving from one side of a valley to the other.

#include "fx_dsp.h"
#include "verdalis/dsp/rng.h"

namespace verdaliscene {
namespace fx {

struct WidenerSettings {
   float width = 1.3f;        // 0..2
   float decorrelation = 0.3f; // 0..1
   float bassMonoHz = 20.0f;  // 20 = off
};

class Widener {
public:
   void allocate(float sampleRate) {
      mSampleRate = sampleRate;
      for (auto &a : mAp)
         a.allocate(static_cast<size_t>(0.012f * sampleRate) + 8);
      mWidth.setTime(0.02f, sampleRate);
      mDecor.setTime(0.02f, sampleRate);
      mDecorHp.setHighpass(250.0f, sampleRate);
      mFirst = true;
      clear();
   }
   void release() {
      for (auto &a : mAp)
         a.release();
   }
   void clear() {
      for (auto &a : mAp)
         a.clear();
      mSideHp[0].reset();
      mSideHp[1].reset();
      mDecorHp.reset();
   }
   void set(const WidenerSettings &s) {
      mS = s;
      mWidth.set(clampv(s.width, 0.0f, 2.0f));
      mDecor.set(clampv(s.decorrelation, 0.0f, 1.0f));
      mBassMono = s.bassMonoHz > 20.5f;
      if (mBassMono) {
         mSideHp[0].setHighpass(s.bassMonoHz, mSampleRate);
         mSideHp[1].setHighpass(s.bassMonoHz, mSampleRate);
      }
      if (mFirst) {
         mWidth.snap(mWidth.target());
         mDecor.snap(mDecor.target());
         mFirst = false;
      }
   }
   void process(float *left, float *right, uint32_t frames) {
      const float sr = mSampleRate;
      for (uint32_t n = 0; n < frames; ++n) {
         const float mid = 0.5f * (left[n] + right[n]);
         float side = 0.5f * (left[n] - right[n]) * mWidth.tick();
         // The decorrelated copy of the mid. An allpass delays each frequency
         // by a different amount rather than the whole signal by one, so the
         // copy has no single arrival time to be heard as an echo or to pull
         // the image towards one side; high-passed, so the bass is not smeared.
         float d = mid;
         for (int k = 0; k < 4; ++k)
            d = mAp[k].tick(d, kApMs[k] * 0.001f * sr, 0.6f);
         side += 0.7f * mDecor.tick() * mDecorHp.tick(d);
         if (mBassMono)
            side = mSideHp[1].tick(mSideHp[0].tick(side));
         left[n] = mid + side;
         right[n] = mid - side;
      }
   }

private:
   static constexpr float kApMs[4] = {1.7f, 3.1f, 4.7f, 7.3f};
   WidenerSettings mS;
   float mSampleRate = 48000.0f;
   bool mFirst = true;
   bool mBassMono = false;
   AllpassLine mAp[4];
   Biquad2 mSideHp[2], mDecorHp;
   Smoother mWidth, mDecor;
};

enum PanShape { kPanSine = 0, kPanTriangle, kPanDrift };

struct AutoPanSettings {
   float rate = 0.1f;  // Hz
   float depth = 0.5f; // 0..1
   int shape = kPanDrift;
};

class AutoPan {
public:
   void allocate(float sampleRate) {
      mSampleRate = sampleRate;
      mDepth.setTime(0.05f, sampleRate);
      mRng.reseed(0xA070u);
      mFirst = true;
      clear();
   }
   void release() {}
   void clear() {
      mRng.reseed(0xA070u);
      mFrom = 0.0f;
      mTo = mRng.uniform() * 2.0f - 1.0f;
      mT = 0.0f;
      mLfo.setPhase(0.0f);
   }
   void set(const AutoPanSettings &s) {
      mS = s;
      mLfo.setRate(std::max(0.005f, s.rate), mSampleRate);
      mInc = std::max(0.005f, s.rate) / mSampleRate;
      mDepth.set(clampv(s.depth, 0.0f, 1.0f));
      if (mFirst) {
         mDepth.snap(mDepth.target());
         mFirst = false;
      }
   }
   void process(float *left, float *right, uint32_t frames) {
      for (uint32_t n = 0; n < frames; ++n) {
         mLfo.advance();
         float m;
         switch (mS.shape) {
         case kPanSine:
            m = mLfo.sine();
            break;
         case kPanTriangle:
            m = mLfo.triangle();
            break;
         case kPanDrift:
         default: {
            // A new random place every cycle, reached along a cosine, so the
            // movement never stops and never jerks.
            mT += mInc;
            if (mT >= 1.0f) {
               mT -= 1.0f;
               mFrom = mTo;
               mTo = mRng.uniform() * 2.0f - 1.0f;
            }
            const float a = 0.5f - 0.5f * std::cos(mT * kPi);
            m = mFrom + a * (mTo - mFrom);
            break;
         }
         }
         const float p = clampv(m * mDepth.tick(), -1.0f, 1.0f);
         // Constant power, and unity at the centre: a balance, so the layer's
         // own stereo image goes with it.
         const float theta = (p + 1.0f) * 0.25f * kPi;
         const float gl = 1.41421356f * std::cos(theta);
         const float gr = 1.41421356f * std::sin(theta);
         left[n] *= gl;
         right[n] *= gr;
      }
   }

private:
   AutoPanSettings mS;
   float mSampleRate = 48000.0f;
   bool mFirst = true;
   Lfo mLfo;
   verdalis::Rng mRng;
   float mFrom = 0.0f, mTo = 0.0f, mT = 0.0f, mInc = 0.0f;
   Smoother mDepth;
};

} // namespace fx
} // namespace verdaliscene
