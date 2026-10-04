#pragma once

// A stereo delay for long, evolving echoes.
//
// Three routings (Pirkle sec. 14.4): Mono, one line fed the sum and heard in
// both ears; Stereo, a line per side; and Ping-Pong, the sum entering the left
// line only and each line feeding the other, so the repeats start on one side
// and bounce. The right line's time is the left's moved by Offset, as a ratio
// (Pirkle's "Ratio"), so a ping-pong can be 3:4 rather than lock-step.
//
// The loop, per line (Pirkle 14.7.1; DAFX 2.6.4):
//
//    read -> diffusion -> low cut -> high cut -> x feedback -> saturate -> + in -> write
//
// with Butterworth corners, never resonant ones -- a loop filter with gain
// above 0 dB is a loop gain above one (Pirkle 18406). Feedback goes to 150 %;
// the saturator bounds what re-enters the line, so above 100 % the repeats
// build into a held, slowly darkening wall the way a tape echo pushed past
// self-oscillation does (DAFX sec. 12.5), and Saturation drives it harder.
//
// Time changes glide (DAFX sec. 12.5.2, "delay control ballistics"): Tape
// slews the read head, which bends the pitch of what is in the line the way a
// tape echo's speed knob does, faster when shortening than when lengthening;
// Fade starts a second read head at the new time and crossfades to it, which
// changes the time without a pitch sweep -- the better choice for jumping
// between note values. Wow drifts the read head by a slow sine and a slower
// random wander, both scaled with the delay, since a real tape's flutter is
// integrated over the length of tape between the heads (DAFX 29780ff).
//
// Diffusion runs each repeat through four allpasses that are inside the
// measured delay time rather than added to it, so the echoes stay in time
// while each one smears further into a cloud. Ducking lowers the echoes while
// the input is loud and lets them up as it falls (Pirkle 14.7.4).

#include "fx_dsp.h"
#include "verdalis/dsp/rng.h"

namespace verdaliscene {
namespace fx {

enum DelayMode { kDelayMono = 0, kDelayStereo, kDelayPingPong };
enum DelayGlide { kGlideTape = 0, kGlideFade };

struct DelaySettings {
   int mode = kDelayStereo;
   int glide = kGlideTape;
   float timeMs = 375.0f; // the left line's
   float offset = 0.0f;   // -1..1: the right line's time is time * (1 + 0.5 offset)
   float feedback = 0.45f; // 0..1.5
   float lowCutHz = 80.0f;
   float highCutHz = 8000.0f;
   float drive = 0.2f;     // 0..1
   float wow = 0.1f;       // 0..1
   float wowRate = 0.6f;   // Hz
   float diffusion = 0.0f; // 0..1
   float ducking = 0.0f;   // 0..1
   float width = 1.0f;     // 0..1
   float mix = 0.3f;       // 0..1, equal power
};

class StereoDelay {
public:
   static constexpr float kMaxSec = 8.0f;

   void allocate(float sampleRate) {
      mSampleRate = sampleRate;
      const size_t n = static_cast<size_t>((kMaxSec + 0.1f) * sampleRate) + 16;
      for (auto &l : mLine)
         l.allocate(n);
      for (auto &ch : mAp)
         for (auto &a : ch)
            a.allocate(static_cast<size_t>(0.07f * sampleRate) + 8);
      mWet.setTime(0.02f, sampleRate);
      mDry.setTime(0.02f, sampleRate);
      mFb.setTime(0.02f, sampleRate);
      for (auto &d : mDc)
         d.setCutoff(5.0f, sampleRate);
      mFirst = true;
      mRng.reseed(0x5EEDu);
      clear();
   }

   void release() {
      for (auto &l : mLine)
         l.release();
      for (auto &ch : mAp)
         for (auto &a : ch)
            a.release();
   }

   void clear() {
      for (auto &l : mLine)
         l.clear();
      for (auto &ch : mAp)
         for (auto &a : ch)
            a.clear();
      for (int c = 0; c < 2; ++c) {
         mLowCut[c].reset();
         mHighCut[c].reset();
         mDc[c].reset();
      }
      mEnv = 0.0f;
      mDrift = mDriftTarget = 0.0f;
      mDriftPhase = 0.0f;
      mFade = 1.0f;
      mWowLfo.setPhase(0.0f);
      mRng.reseed(0x5EEDu);
   }

   void set(const DelaySettings &s) {
      mS = s;
      const float sr = mSampleRate;
      const float maxSamples = kMaxSec * sr;
      float t[2];
      t[0] = clampv(s.timeMs * 0.001f * sr, 4.0f, maxSamples);
      t[1] = clampv(t[0] * (1.0f + 0.5f * clampv(s.offset, -1.0f, 1.0f)), 4.0f, maxSamples);
      for (int c = 0; c < 2; ++c) {
         mLowCut[c].setLowCut(s.lowCutHz, sr);
         mHighCut[c].setHighCut(s.highCutHz, sr);
         if (mFirst) {
            mTime[c] = mTarget[c] = mOld[c] = t[c];
         } else if (t[c] != mTarget[c]) {
            if (s.glide == kGlideFade) {
               // A new head at the new time; the old one fades out from
               // wherever it was.
               mOld[c] = mTime[c];
               mTime[c] = t[c];
               mFade = 0.0f;
            }
            mTarget[c] = t[c];
         }
      }
      mWowLfo.setRate(std::max(0.01f, s.wowRate), sr);
      const float mix = clampv(s.mix, 0.0f, 1.0f);
      mWet.set(std::sin(mix * 0.5f * kPi));
      mDry.set(std::cos(mix * 0.5f * kPi));
      mFb.set(clampv(s.feedback, 0.0f, 1.5f));
      // Tape: shortening (the tape speeding up) settles in about 1 s on a
      // Space Echo and lengthening in about 2 s (DAFX 12.48/12.49). Halved
      // here: an instrument's knob should not take two seconds to arrive.
      mUp = 1.0f - std::exp(-1.0f / (0.5f * sr));
      mDown = 1.0f - std::exp(-1.0f / (1.0f * sr));
      mFadeInc = 1.0f / (0.04f * sr);
      mDriftInc = std::max(0.01f, s.wowRate) * 0.37f / sr;
      if (mFirst) {
         mWet.snap(mWet.target());
         mDry.snap(mDry.target());
         mFb.snap(mFb.target());
         mFirst = false;
      }
   }

   void process(float *left, float *right, uint32_t frames) {
      const float sr = mSampleRate;
      const float driveGain = 1.0f + 3.0f * clampv(mS.drive, 0.0f, 1.0f);
      const float diffusion = clampv(mS.diffusion, 0.0f, 1.0f);
      const float apGain = 0.62f * diffusion;
      const float width = clampv(mS.width, 0.0f, 1.0f);
      const float wowDepth = clampv(mS.wow, 0.0f, 1.0f);
      const float duck = clampv(mS.ducking, 0.0f, 1.0f);
      const float envAttack = 1.0f - std::exp(-1.0f / (0.01f * sr));
      const float envRelease = 1.0f - std::exp(-1.0f / (0.35f * sr));
      const int mode = mS.mode;

      for (uint32_t n = 0; n < frames; ++n) {
         const float xl = left[n], xr = right[n];

         // The read heads.
         mWowLfo.advance();
         mDriftPhase += mDriftInc;
         if (mDriftPhase >= 1.0f) {
            mDriftPhase -= 1.0f;
            mDriftTarget = mRng.uniform() * 2.0f - 1.0f;
         }
         mDrift += 4.0f * mDriftInc * (mDriftTarget - mDrift);
         const float wowShape = 0.7f * mWowLfo.sine() + 0.3f * mDrift;
         if (mS.glide == kGlideTape) {
            for (int c = 0; c < 2; ++c) {
               const float k = mTarget[c] < mTime[c] ? mUp : mDown;
               float step = k * (mTarget[c] - mTime[c]);
               // Never so fast the head runs backwards over the tape.
               step = clampv(step, -0.5f, 0.5f);
               mTime[c] += step;
            }
         } else if (mFade < 1.0f) {
            mFade = std::min(1.0f, mFade + mFadeInc);
         }

         float y[2];
         for (int c = 0; c < 2; ++c) {
            // Wow scales with the delay, as it does on tape: up to about
            // 0.4 % of the time, and never more than 6 ms.
            const float wow =
               wowDepth * std::min(0.006f * sr, 0.004f * mTime[c]) * (c ? -wowShape : wowShape);
            // Diffusion's allpasses sit inside the delay -- up to a sixth of
            // it, at most 40 ms, growing with Diffusion -- and are taken off
            // the main read. At no diffusion they are exact two-sample delays,
            // so plain repeats pass through them untouched.
            float apLen[4];
            float apTotal = 0.0f;
            const float apSpan = diffusion * std::min(kApFraction * mTime[c], kApMaxSec * sr);
            for (int k = 0; k < 4; ++k) {
               apLen[k] = std::max(2.0f, apSpan * kApShare[k]);
               apTotal += apLen[k];
            }
            const float main = std::max(2.0f, mTime[c] - apTotal + wow);
            float v = mLine[c].readCubic(main - 1.0f);
            if (mS.glide == kGlideFade && mFade < 1.0f) {
               const float oldMain = std::max(2.0f, mOld[c] - apTotal + wow);
               const float o = mLine[c].readCubic(oldMain - 1.0f);
               const float a = 0.5f - 0.5f * std::cos(mFade * kPi);
               v = o + a * (v - o);
            }
            for (int k = 0; k < 4; ++k)
               v = mAp[c][k].tick(v, apLen[k], apGain);
            y[c] = v;
         }

         // The loop.
         const float fb = mFb.tick();
         float f[2];
         for (int c = 0; c < 2; ++c) {
            float v = mHighCut[c].tick(mLowCut[c].tick(y[c]));
            v = mDc[c].tick(v);
            f[c] = flush(saturate(fb * v, driveGain));
         }
         const float in = 0.5f * (xl + xr);
         switch (mode) {
         case kDelayMono:
            mLine[0].write(in + f[0]);
            mLine[1].write(0.0f);
            y[1] = y[0];
            break;
         case kDelayPingPong:
            mLine[0].write(in + f[1]);
            mLine[1].write(f[0]);
            break;
         case kDelayStereo:
         default:
            mLine[0].write(xl + f[0]);
            mLine[1].write(xr + f[1]);
            break;
         }

         // Ducking follows the input's level and pulls the echoes down by up
         // to 24 dB while it is loud.
         const float level = std::max(std::fabs(xl), std::fabs(xr));
         mEnv += (level > mEnv ? envAttack : envRelease) * (level - mEnv);
         const float duckGain = 1.0f / (1.0f + duck * 16.0f * mEnv * 4.0f);

         const float mid = 0.5f * (y[0] + y[1]);
         const float side = 0.5f * (y[0] - y[1]) * width;
         const float wet = mWet.tick() * duckGain;
         const float dry = mDry.tick();
         left[n] = dry * xl + wet * (mid + side);
         right[n] = dry * xr + wet * (mid - side);
      }
   }

private:
   static constexpr float kApFraction = 1.0f / 6.0f;
   static constexpr float kApMaxSec = 0.04f;
   // How the diffusion time is shared out over the four allpasses.
   static constexpr float kApShare[4] = {0.153f, 0.237f, 0.271f, 0.339f};

   DelaySettings mS;
   float mSampleRate = 48000.0f;
   bool mFirst = true;
   Delay mLine[2];
   AllpassLine mAp[2][4];
   Corner mLowCut[2], mHighCut[2];
   DcBlock mDc[2];
   float mTime[2] = {1.0f, 1.0f}, mTarget[2] = {1.0f, 1.0f}, mOld[2] = {1.0f, 1.0f};
   float mUp = 0.0f, mDown = 0.0f;
   float mFade = 1.0f, mFadeInc = 0.0f;
   Lfo mWowLfo;
   verdalis::Rng mRng;
   float mDrift = 0.0f, mDriftTarget = 0.0f, mDriftPhase = 0.0f, mDriftInc = 0.0f;
   float mEnv = 0.0f;
   Smoother mWet, mDry, mFb;
};

} // namespace fx
} // namespace verdaliscene
