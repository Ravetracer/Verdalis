#pragma once

// A reverb for long, smooth tails: a sixteen-line feedback delay network.
//
// The topology is Jot's (Pirkle, "Designing Audio Effect Plugins in C++",
// 2nd ed., sec. 17.12; Zoelzer, DAFX 2nd ed., sec. 5.7.2). A lossless prototype
// -- delay lines mixed through an orthogonal matrix, here a 16x16 Hadamard run
// as a fast Walsh-Hadamard transform -- has every loss added per line, sized to
// that line's own length, so every mode in a frequency band decays at the same
// rate and no single one is left ringing. That is the metallic tail a reverb
// with one shared feedback gain has (Pirkle 17.8; DAFX 5.20).
//
// Per line, everything is derived from the decay time asked for:
//
//    g   = 10^(-3 m / (fs T60))            the loss over m samples
//    g_w = 10^(-3 m / (fs T60 r))          the same at the damping frequency,
//                                          where r is how much faster the top
//                                          end dies
//    H(z) = g (1 - b) / (1 - b z^-1)       one pole, exactly g at DC and g_w at
//                                          the damping frequency:
//    b = (A - sqrt(A^2 - (1 - q)^2)) / (1 - q),   q = (g_w / g)^2,
//                                                 A = 1 - q cos w
//
// which holds at any sample rate, unlike a damping specified at Nyquist.
//
// In front of the network are a pre-delay and Dattorro's four input diffusers
// (Pirkle table 17.1: 4.77, 3.60, 12.73 and 9.31 ms at g = 0.75, 0.75, 0.625,
// 0.625), per channel, so the network is fed a dense stream rather than
// discrete events. Every line's read position drifts slowly (Pirkle 20687ff;
// DAFX 5.7.3): with a sixty-second tail the modes are far narrower than their
// spacing and only time variance smears them. The drift is read through the
// Hermite spline, so it adds no damping of its own.
//
// Freeze sets every line's loss to (almost) nothing and closes the input, both
// over a ramp, so a held tail neither clicks in nor drains away.

#include "fx_dsp.h"

namespace verdaliscene {
namespace fx {

struct ReverbSettings {
   float mix = 0.3f;         // 0..1, equal power
   float predelayMs = 20.0f;
   float size = 0.6f;        // 0..1
   float decaySec = 4.0f;
   float diffusion = 0.7f;   // 0..1
   float damping = 0.5f;     // 0..1: how much faster the top end decays
   float dampHz = 4000.0f;
   float modDepth = 0.3f;    // 0..1
   float modRate = 0.5f;     // Hz
   float lowCutHz = 20.0f;
   float highCutHz = 20000.0f;
   float width = 1.0f;       // 0..1
   bool freeze = false;
};

class Reverb {
public:
   static constexpr int kLines = 16;
   static constexpr float kMaxPredelaySec = 1.0f;
   // The longest line at the largest size, plus the deepest drift.
   static constexpr float kMaxLineSec = 0.26f;

   void allocate(float sampleRate) {
      mSampleRate = sampleRate;
      for (auto &p : mPre)
         p.allocate(static_cast<size_t>(kMaxPredelaySec * sampleRate) + 8);
      for (auto &ch : mDiff)
         for (auto &a : ch)
            a.allocate(static_cast<size_t>(0.04f * sampleRate) + 8);
      for (auto &l : mLines)
         l.allocate(static_cast<size_t>(kMaxLineSec * sampleRate) + 8);
      mSizeSmooth.setTime(0.4f, sampleRate);
      mPreSmooth.setTime(0.15f, sampleRate);
      mFreeze.setTime(0.05f, sampleRate);
      mWet.setTime(0.02f, sampleRate);
      mDry.setTime(0.02f, sampleRate);
      for (int i = 0; i < kLines; ++i) {
         // Rates spread over 0.6x to 1.6x of the one asked for, in a
         // sequence that does not repeat, so no two lines drift together.
         mRateScale[i] = 0.6f + std::fmod(0.6180339f * static_cast<float>(i + 1), 1.0f);
         mLfo[i].setPhase(std::fmod(0.381966f * static_cast<float>(i), 1.0f));
      }
      mFirst = true;
      clear();
   }

   void release() {
      for (auto &p : mPre)
         p.release();
      for (auto &ch : mDiff)
         for (auto &a : ch)
            a.release();
      for (auto &l : mLines)
         l.release();
   }

   void clear() {
      for (auto &p : mPre)
         p.clear();
      for (auto &ch : mDiff)
         for (auto &a : ch)
            a.clear();
      for (int i = 0; i < kLines; ++i) {
         mLines[i].clear();
         mDamp[i] = 0.0f;
         // Where the drift is is playing state too: cleared, the reverb
         // replays the same tail for the same input.
         mLfo[i].setPhase(std::fmod(0.381966f * static_cast<float>(i), 1.0f));
      }
      for (auto &c : mLowCut)
         c.reset();
      for (auto &c : mHighCut)
         c.reset();
   }

   void set(const ReverbSettings &s) {
      mS = s;
      const float sr = mSampleRate;
      // Size: 0.15x to 2x the base lengths, on an exponential scale so the
      // knob's travel is spread evenly over small rooms and huge halls.
      const float scale = 0.15f * std::pow(2.0f / 0.15f, clampv(s.size, 0.0f, 1.0f));
      mSizeSmooth.set(scale);
      mPreSmooth.set(clampv(s.predelayMs, 0.0f, kMaxPredelaySec * 1000.0f) * 0.001f * sr);
      mFreeze.set(s.freeze ? 1.0f : 0.0f);
      const float mix = clampv(s.mix, 0.0f, 1.0f);
      // A long tail carries far more energy than a short one -- its impulse
      // response is longer -- so the wet level comes down partly as the decay
      // goes up (about 7 dB less at 60 s than at 2 s), which keeps the knob
      // musical without hiding what a long decay is.
      const float comp = std::pow(std::min(1.0f, 2.0f / std::max(s.decaySec, 0.1f)), 0.25f);
      mWet.set(std::sin(mix * 0.5f * kPi) * comp * kWetTrim);
      mDry.set(std::cos(mix * 0.5f * kPi));
      for (int c = 0; c < 2; ++c) {
         mLowCut[c].setLowCut(std::max(20.0f, s.lowCutHz), sr);
         mHighCut[c].setHighCut(s.highCutHz, sr);
      }
      for (int i = 0; i < kLines; ++i)
         mLfo[i].setRate(std::max(0.01f, s.modRate) * mRateScale[i], sr);
      mDepthSamples = clampv(s.modDepth, 0.0f, 1.0f) * 0.0012f * sr;
      if (mFirst) {
         mSizeSmooth.snap(scale);
         mPreSmooth.snap(mPreSmooth.target());
         mFreeze.snap(mFreeze.target());
         mWet.snap(mWet.target());
         mDry.snap(mDry.target());
         mFirst = false;
      }
      mCoefsDirty = true;
   }

   // In place, stereo.
   void process(float *left, float *right, uint32_t frames) {
      const float sr = mSampleRate;
      const float diffusion = clampv(mS.diffusion, 0.0f, 1.0f);
      const float g1 = 0.75f * diffusion, g2 = 0.625f * diffusion;
      const float width = clampv(mS.width, 0.0f, 1.0f);
      for (uint32_t n = 0; n < frames; ++n) {
         const float scale = mSizeSmooth.tick();
         const float freeze = mFreeze.tick();
         // The losses follow the line lengths, so they are worked out again
         // while Size or Freeze is still gliding -- every 64 samples is far
         // finer than either can be heard to step.
         if (mCoefsDirty || ((n & 63) == 0 && (!mSizeSmooth.settled() || !mFreeze.settled())))
            updateCoefs(scale, freeze);

         const float xl = left[n], xr = right[n];
         // Pre-delay, then the diffusers. Their lengths follow the size, but
         // only half as far, so a small room still diffuses and a large one
         // does not smear its attacks into a wash before the tail.
         const float pre = std::max(1.0f, mPreSmooth.tick());
         mPre[0].write(xl);
         mPre[1].write(xr);
         float in[2] = {mPre[0].readCubic(pre), mPre[1].readCubic(pre)};
         const float diffScale = std::sqrt(scale);
         for (int c = 0; c < 2; ++c) {
            const float skew = c ? 1.071f : 1.0f;
            float v = in[c];
            for (int k = 0; k < 4; ++k) {
               const float len = std::max(2.0f, kDiffMs[k] * 0.001f * sr * diffScale * skew);
               v = mDiff[c][k].tick(v, len, k < 2 ? g1 : g2);
            }
            in[c] = v * (1.0f - freeze);
         }

         // The network: read every line, damp it, mix it, write it back.
         // Frozen, the drift fades out: see updateCoefs().
         const float depth = mDepthSamples * (1.0f - freeze);
         float s[kLines];
         for (int i = 0; i < kLines; ++i) {
            mLfo[i].advance();
            const float d = std::max(2.0f, mLen[i] + depth * mLfo[i].sine());
            const float raw = mLines[i].readCubic(d - 1.0f);
            mDamp[i] = flush(mGain[i] * (1.0f - mB[i]) * raw + mB[i] * mDamp[i]);
            s[i] = mDamp[i];
         }
         // The two outputs are two different rows of the Hadamard matrix
         // applied to the lines, so left and right are orthogonal sums of
         // the same tail: fully decorrelated, equally loud.
         float wl = 0.0f, wr = 0.0f;
         for (int i = 0; i < kLines; ++i) {
            wl += kRowA[i] * s[i];
            wr += kRowB[i] * s[i];
         }
         hadamard16(s);
         for (int i = 0; i < kLines; ++i)
            mLines[i].write(s[i] + ((i & 1) ? in[1] : in[0]) * kInputGain);

         wl *= 0.25f;
         wr *= 0.25f;
         const float mid = 0.5f * (wl + wr);
         const float side = 0.5f * (wl - wr) * width;
         wl = mHighCut[0].tick(mLowCut[0].tick(mid + side));
         wr = mHighCut[1].tick(mLowCut[1].tick(mid - side));
         const float wet = mWet.tick();
         const float dry = mDry.tick();
         left[n] = dry * xl + wet * wl;
         right[n] = dry * xr + wet * wr;
      }
   }

private:
   // Dattorro's input diffusers, in milliseconds (Pirkle table 17.1).
   static constexpr float kDiffMs[4] = {4.77f, 3.60f, 12.73f, 9.31f};
   // The base line lengths at a size of 1x: spread over 31 to 113 ms (about
   // 1:3.6), on irrational ratios so no two share a period. Total about 1.05 s,
   // well over Schroeder's 0.15 modes/Hz (Pirkle 20299).
   static constexpr float kLineMs[kLines] = {31.0f, 33.9f, 36.7f, 40.3f, 43.1f, 47.3f,
                                             51.1f, 55.7f, 60.7f, 65.9f, 71.3f, 77.9f,
                                             84.7f, 92.3f, 101.9f, 112.9f};
   // Two rows of the 16x16 Sylvester-Hadamard matrix (rows 5 and 10).
   static constexpr float kRowA[kLines] = {1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, -1, -1, 1, -1, 1};
   static constexpr float kRowB[kLines] = {1, 1, -1, -1, 1, 1, -1, -1, -1, -1, 1, 1, -1, -1, 1, 1};
   static constexpr float kInputGain = 0.35355339f; // 1/sqrt(8): eight lines per channel
   static constexpr float kWetTrim = 1.6f;

   // 16-point fast Walsh-Hadamard transform, scaled to be orthonormal.
   static inline void hadamard16(float *x) {
      for (int h = 1; h < kLines; h <<= 1)
         for (int i = 0; i < kLines; i += h << 1)
            for (int j = i; j < i + h; ++j) {
               const float a = x[j], b = x[j + h];
               x[j] = a + b;
               x[j + h] = a - b;
            }
      for (int i = 0; i < kLines; ++i)
         x[i] *= 0.25f;
   }

   void updateCoefs(float scale, float freeze) {
      mCoefsDirty = false;
      const float sr = mSampleRate;
      const float t60 = std::max(0.05f, mS.decaySec);
      const float ratio = 1.0f - 0.95f * clampv(mS.damping, 0.0f, 1.0f);
      const float w = kTwoPi * clampv(mS.dampHz, 100.0f, 0.45f * sr) / sr;
      const float cw = std::cos(w);
      const float maxLen = kMaxLineSec * sr - mDepthSamples - 8.0f;
      for (int i = 0; i < kLines; ++i) {
         const float m = std::max(8.0f, std::min(maxLen, kLineMs[i] * 0.001f * sr * scale));
         // A read between samples is never perfectly flat -- the spline is a
         // gentle filter whose shape depends on where between the samples it
         // reads -- and a loop with no loss repeats that filter forever: a
         // frozen tail would slowly darken, or brighten and grow. So frozen,
         // every line slides to a whole number of samples and its drift fades
         // out, and the loop is exact.
         mLen[i] = m + freeze * (std::round(m) - m);
         const float g = std::pow(10.0f, -3.0f * m / (sr * t60));
         const float gw = std::pow(10.0f, -3.0f * m / (sr * t60 * ratio));
         const float q = (gw / g) * (gw / g);
         float b = 0.0f;
         if (q < 0.999999f) {
            const float a = 1.0f - q * cw;
            b = (a - std::sqrt(std::max(0.0f, a * a - (1.0f - q) * (1.0f - q)))) / (1.0f - q);
         }
         // Freeze: no loss, no damping. Not exactly unity -- float rounding
         // would let a perfectly lossless loop random-walk its energy.
         mGain[i] = g + freeze * (0.999995f - g);
         mB[i] = b * (1.0f - freeze);
      }
   }

   ReverbSettings mS;
   float mSampleRate = 48000.0f;
   bool mFirst = true;
   bool mCoefsDirty = true;

   Delay mPre[2];
   AllpassLine mDiff[2][4];
   Delay mLines[kLines];
   float mLen[kLines] = {};
   float mGain[kLines] = {};
   float mB[kLines] = {};
   float mDamp[kLines] = {};
   float mRateScale[kLines] = {};
   Lfo mLfo[kLines];
   float mDepthSamples = 0.0f;
   Corner mLowCut[2], mHighCut[2];
   Smoother mSizeSmooth, mPreSmooth, mFreeze, mWet, mDry;
};

} // namespace fx
} // namespace verdaliscene
