#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "filters.h"

namespace rainyday {

// Fixed integer delay line.
class DelayLine {
public:
   void allocate(size_t maxSamples) {
      mBuffer.assign(maxSamples + 4, 0.0f);
      mWrite = 0;
      mLength = maxSamples;
   }

   void clear() { std::fill(mBuffer.begin(), mBuffer.end(), 0.0f); }

   void setDelay(size_t samples) {
      mDelay = samples < 1 ? 1 : (samples > mLength ? mLength : samples);
   }

   inline float read() const {
      size_t r = mWrite + mBuffer.size() - mDelay;
      if (r >= mBuffer.size())
         r -= mBuffer.size();
      return mBuffer[r];
   }

   inline void write(float v) {
      mBuffer[mWrite] = v;
      if (++mWrite >= mBuffer.size())
         mWrite = 0;
   }

private:
   std::vector<float> mBuffer;
   size_t mWrite = 0, mDelay = 1, mLength = 1;
};

// Schroeder allpass, used to diffuse the input before it enters the tank so
// dense rain does not smear into a single flutter.
class Allpass {
public:
   void allocate(size_t maxSamples) { mLine.allocate(maxSamples); }
   void clear() { mLine.clear(); }
   void setDelay(size_t samples) { mLine.setDelay(samples); }
   void setGain(float g) { mGain = g; }

   inline float tick(float in) {
      const float d = mLine.read();
      const float v = in + mGain * d;
      mLine.write(v);
      return d - mGain * v;
   }

private:
   DelayLine mLine;
   float mGain = 0.5f;
};

// 4-line feedback delay network with an orthonormal Hadamard mixing matrix and
// per-line damping. This is the "space" the rain falls in: a cave, a room, or
// the open air. Orthonormal mixing means the feedback gain alone controls
// decay, so it cannot blow up.
class Fdn {
public:
   void prepare(float sampleRate) {
      mSampleRate = sampleRate;
      const size_t maxLine = static_cast<size_t>(sampleRate * 0.13f) + 8;
      for (int i = 0; i < 4; ++i) {
         mLines[i].allocate(maxLine);
         mDamp[i].reset();
      }
      const size_t maxAp = static_cast<size_t>(sampleRate * 0.02f) + 8;
      for (int i = 0; i < 4; ++i) {
         mDiffusers[i].allocate(maxAp);
         mDiffusers[i].setGain(0.62f);
      }
      setSize(0.5f);
      setDamping(0.5f);
   }

   void clear() {
      for (int i = 0; i < 4; ++i) {
         mLines[i].clear();
         mDamp[i].reset();
         mDiffusers[i].clear();
      }
   }

   void setSize(float size) {
      size = clampv(size, 0.0f, 1.0f);
      // Mutually prime-ish base lengths avoid ringing on a single pitch.
      static const float kBaseMs[4] = {23.13f, 31.71f, 41.29f, 53.87f};
      static const float kApMs[4] = {5.31f, 8.73f, 6.47f, 9.91f};
      const float scale = 0.28f + 1.72f * size;
      for (int i = 0; i < 4; ++i) {
         mLines[i].setDelay(static_cast<size_t>(kBaseMs[i] * scale * 0.001f * mSampleRate));
         mDiffusers[i].setDelay(
            static_cast<size_t>(kApMs[i] * (0.5f + 0.5f * scale) * 0.001f * mSampleRate));
      }
      // Bigger spaces ring longer.
      mFeedback = 0.62f + 0.35f * size;
   }

   void setDamping(float damping) {
      damping = clampv(damping, 0.0f, 1.0f);
      // Damping 0 = bright stone, 1 = soft and absorbent.
      const float cutoff = 14000.0f * std::exp2(-5.0f * damping);
      for (int i = 0; i < 4; ++i)
         mDamp[i].setCutoff(cutoff, mSampleRate);
   }

   inline void tick(float inL, float inR, float &outL, float &outR) {
      const float dL = mDiffusers[0].tick(mDiffusers[2].tick(inL));
      const float dR = mDiffusers[1].tick(mDiffusers[3].tick(inR));

      const float a = mLines[0].read();
      const float b = mLines[1].read();
      const float c = mLines[2].read();
      const float d = mLines[3].read();

      // Orthonormal Hadamard mix.
      const float m0 = 0.5f * (a + b + c + d);
      const float m1 = 0.5f * (a - b + c - d);
      const float m2 = 0.5f * (a + b - c - d);
      const float m3 = 0.5f * (a - b - c + d);

      mLines[0].write(dL + mFeedback * mDamp[0].tick(m0));
      mLines[1].write(dR + mFeedback * mDamp[1].tick(m1));
      mLines[2].write(dL + mFeedback * mDamp[2].tick(m2));
      mLines[3].write(dR + mFeedback * mDamp[3].tick(m3));

      outL = 0.5f * (a + c);
      outR = 0.5f * (b + d);
   }

private:
   DelayLine mLines[4];
   Allpass mDiffusers[4];
   OnePoleLp mDamp[4];
   float mSampleRate = 48000.0f;
   float mFeedback = 0.8f;
};

} // namespace rainyday
