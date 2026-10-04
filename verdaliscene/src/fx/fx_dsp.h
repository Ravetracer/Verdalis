#pragma once

// The building blocks VerdaliScene's effects share: a delay line read with
// cubic interpolation, a parameter smoother, an LFO and a second-order filter.
//
// Interpolation matters more here than anywhere else in the suite. A delay
// whose time is modulated -- a chorus, a flanger, a reverb line drifting to
// break up its modes, a tape delay gliding to a new time -- reads between
// samples all the time. Linear interpolation is a lowpass whose cutoff moves
// with the fractional position (DAFX 2nd ed., eq. 2.64), which a modulated
// read turns into a wandering dullness and, inside a feedback loop, into
// damping nobody asked for. An allpass interpolator is flat but rings when its
// position moves (eq. 2.65). The 4-point, 3rd-order Hermite spline (the cubic
// of eq. 2.66, in the form musicdsp's "Hermite interpolation" entry gives) is
// flat enough and costs four reads.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "verdalis/dsp/fastmath.h"

namespace verdaliscene {
namespace fx {

using verdalis::clampv;

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

// A delay line over a power-of-two buffer. Write once per sample, then read
// any number of taps behind the write head; read(d) is the sample written d
// samples before the latest one.
class Delay {
public:
   void allocate(size_t maxSamples) {
      size_t n = 16;
      while (n < maxSamples + 8)
         n <<= 1;
      mBuffer.assign(n, 0.0f);
      mMask = n - 1;
      mWrite = 0;
   }
   void release() {
      std::vector<float>().swap(mBuffer);
      mMask = 0;
      mWrite = 0;
   }
   bool allocated() const { return !mBuffer.empty(); }
   void clear() { std::fill(mBuffer.begin(), mBuffer.end(), 0.0f); }

   // The longest delay a read may ask for, leaving room for the spline's
   // neighbours on both sides.
   float maxDelay() const { return mBuffer.empty() ? 0.0f : static_cast<float>(mBuffer.size() - 4); }

   inline void write(float v) {
      mWrite = (mWrite + 1) & mMask;
      mBuffer[mWrite] = v;
   }

   inline float read(size_t d) const { return mBuffer[(mWrite - d) & mMask]; }

   // A fractional read, d >= 1. Hermite over the samples at d-1, d, d+1, d+2.
   inline float readCubic(float d) const {
      const size_t i = static_cast<size_t>(d);
      const float t = d - static_cast<float>(i);
      const float xm1 = mBuffer[(mWrite - i + 1) & mMask];
      const float x0 = mBuffer[(mWrite - i) & mMask];
      const float x1 = mBuffer[(mWrite - i - 1) & mMask];
      const float x2 = mBuffer[(mWrite - i - 2) & mMask];
      const float c1 = 0.5f * (x1 - xm1);
      const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
      const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
      return ((c3 * t + c2) * t + c1) * t + x0;
   }

private:
   std::vector<float> mBuffer;
   size_t mMask = 0, mWrite = 0;
};

// A one-pole glide towards a target: what keeps a turned knob from stepping
// the signal. `seconds` is the time constant.
class Smoother {
public:
   void setTime(float seconds, float sampleRate) {
      mCoef = seconds <= 0.0f ? 1.0f : 1.0f - std::exp(-1.0f / (seconds * sampleRate));
   }
   void snap(float v) { mValue = mTarget = v; }
   void set(float target) { mTarget = target; }
   inline float tick() {
      mValue += mCoef * (mTarget - mValue);
      return mValue;
   }
   float value() const { return mValue; }
   float target() const { return mTarget; }
   bool settled() const { return std::fabs(mTarget - mValue) < 1.0e-6f; }

private:
   float mValue = 0.0f, mTarget = 0.0f, mCoef = 1.0f;
};

// A sine LFO kept as a phase in [0, 1).
class Lfo {
public:
   void setRate(float hz, float sampleRate) { mInc = hz / sampleRate; }
   void setPhase(float phase) { mPhase = phase - std::floor(phase); }
   float phase() const { return mPhase; }
   inline void advance() {
      mPhase += mInc;
      if (mPhase >= 1.0f)
         mPhase -= 1.0f;
   }
   // The value at the current phase plus an offset in cycles.
   inline float sine(float offset = 0.0f) const {
      float p = mPhase + offset;
      p -= std::floor(p);
      return verdalis::sin2piFast(p);
   }
   inline float triangle(float offset = 0.0f) const {
      float p = mPhase + offset;
      p -= std::floor(p);
      return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
   }

private:
   float mPhase = 0.0f, mInc = 0.0f;
};

// The Audio EQ Cookbook's second-order lowpass and highpass, Butterworth at
// Q = 1/sqrt(2), in transposed direct form II. Used where a corner has to be a
// real 12 dB/oct one: the wet filters of the reverb and the delay's feedback
// loop, and the widener's crossover.
class Biquad2 {
public:
   void reset() { mZ1 = mZ2 = 0.0f; }
   void setBypass() {
      mB0 = 1.0f;
      mB1 = mB2 = mA1 = mA2 = 0.0f;
   }
   void setLowpass(float hz, float sampleRate, float q = 0.70710678f) { design(hz, sampleRate, q, true); }
   void setHighpass(float hz, float sampleRate, float q = 0.70710678f) {
      design(hz, sampleRate, q, false);
   }
   inline float tick(float x) {
      const float y = mB0 * x + mZ1;
      mZ1 = mB1 * x - mA1 * y + mZ2;
      mZ2 = mB2 * x - mA2 * y;
      return y;
   }

private:
   void design(float hz, float sampleRate, float q, bool low) {
      const float f = clampv(hz, 5.0f, 0.45f * sampleRate);
      const float w = kTwoPi * f / sampleRate;
      const float cw = std::cos(w);
      const float alpha = std::sin(w) / (2.0f * q);
      const float a0 = 1.0f + alpha;
      if (low) {
         mB0 = (1.0f - cw) * 0.5f / a0;
         mB1 = (1.0f - cw) / a0;
      } else {
         mB0 = (1.0f + cw) * 0.5f / a0;
         mB1 = -(1.0f + cw) / a0;
      }
      mB2 = mB0;
      mA1 = -2.0f * cw / a0;
      mA2 = (1.0f - alpha) / a0;
   }

   float mB0 = 1.0f, mB1 = 0.0f, mB2 = 0.0f, mA1 = 0.0f, mA2 = 0.0f;
   float mZ1 = 0.0f, mZ2 = 0.0f;
};

// A corner that is left out at the end of its range: a low cut at its bottom
// and a high cut at its top cost nothing and colour nothing.
class Corner {
public:
   void reset() { mF.reset(); }
   void setLowCut(float hz, float sampleRate) {
      mOn = hz > 20.5f;
      if (mOn)
         mF.setHighpass(hz, sampleRate);
   }
   void setHighCut(float hz, float sampleRate) {
      mOn = hz < 19500.0f && hz < 0.45f * sampleRate;
      if (mOn)
         mF.setLowpass(hz, sampleRate);
   }
   inline float tick(float x) { return mOn ? mF.tick(x) : x; }

private:
   Biquad2 mF;
   bool mOn = false;
};

// A one-pole DC blocker, for loops whose gain can sit at or above one.
class DcBlock {
public:
   void reset() { mX = mY = 0.0f; }
   void setCutoff(float hz, float sampleRate) { mR = 1.0f - kTwoPi * hz / sampleRate; }
   inline float tick(float x) {
      const float y = x - mX + mR * mY;
      mX = x;
      mY = y;
      return y;
   }

private:
   float mX = 0.0f, mY = 0.0f, mR = 0.995f;
};

// A Schroeder allpass on its own delay, y = -g x + x(n-m) + g y(n-m) (DAFX
// eq. 5.19; Pirkle eq. 17.12), with a fractional, modulatable length m >= 2.
class AllpassLine {
public:
   void allocate(size_t maxSamples) { mLine.allocate(maxSamples); }
   void release() { mLine.release(); }
   void clear() { mLine.clear(); }
   inline float tick(float x, float delay, float g) {
      // Read before this sample is written, so m - 1 behind the newest is m
      // behind the one being written.
      const float d = mLine.readCubic(delay - 1.0f);
      const float v = x + g * d;
      mLine.write(v);
      return d - g * v;
   }

private:
   Delay mLine;
};

// The rational tanh approximation (musicdsp "Rational tanh approximation"):
// exact at 0, C2-continuous, and exactly +-1 from |x| = 3 on.
inline float tanhApprox(float x) {
   const float y = clampv(x, -3.0f, 3.0f);
   const float y2 = y * y;
   return y * (27.0f + y2) / (27.0f + 9.0f * y2);
}

// Saturation for a feedback loop that is allowed to exceed unity. Bounded at
// +-1/drive, so the delay line holds at most the input plus that, whatever the
// feedback (Pirkle 14.7; the Echoplex and Space Echo run a loop gain up to 2
// into tape saturation, DAFX sec. 12.5): a delay at 150 % builds into a
// sustained wall rather than a runaway. `drive` >= 1.
inline float saturate(float x, float drive) { return tanhApprox(x * drive) / drive; }

// Flushes what would otherwise decay through the denormal range for minutes.
inline float flush(float x) { return std::fabs(x) < 1.0e-20f ? 0.0f : x; }

} // namespace fx
} // namespace verdaliscene
