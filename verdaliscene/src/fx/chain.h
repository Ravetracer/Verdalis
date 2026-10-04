#pragma once

// One channel's effects -- a layer's, or the scene's -- run in a fixed order:
//
//    phaser -> chorus -> flanger -> delay -> reverb -> widener -> auto-pan
//
// modulation first, then the echoes, then the space they happen in, then where
// the whole result sits. Echoes into a reverb is the order a producer patches
// by hand; a reverb into a delay would repeat the whole wash.
//
// Threads. An effect's buffers are big -- eight seconds of stereo delay -- so
// nothing is allocated until an effect is first switched on, and then on the
// main thread: allocate() makes the buffers and only then marks the effect
// ready, with a release store; the audio thread does not touch an effect until
// it has seen that with an acquire load. Nothing is freed until the plugin is
// deactivated, when the audio thread is not running.
//
// Switching an effect on or off crossfades over 20 ms. An effect switched off
// stops being run once it has faded out, and is cleared before it runs again,
// so turning a reverb back on does not bring back the tail it had.

#include <atomic>
#include <cstring>
#include <vector>

#include "../params.h"
#include "delay.h"
#include "modulation.h"
#include "reverb.h"
#include "stereo.h"

namespace verdaliscene {
namespace fx {

class Chain {
public:
   Chain() {
      for (auto &r : mReady)
         r.store(false, std::memory_order_relaxed);
   }

   // [main thread, audio not running]
   void prepare(float sampleRate, uint32_t maxFrames) {
      releaseAll();
      mSampleRate = sampleRate;
      mDryL.assign(maxFrames, 0.0f);
      mDryR.assign(maxFrames, 0.0f);
      for (int k = 0; k < kNumFxKinds; ++k) {
         mActive[k].setTime(0.02f, sampleRate);
         mActive[k].snap(0.0f);
         mOn[k] = false;
         mNeedsClear[k] = false;
         mApplied[k] = false;
      }
      mQuietRun = 0;
      mFresh = true;
   }

   // [main thread, audio not running]
   void releaseAll() {
      for (int k = 0; k < kNumFxKinds; ++k)
         mReady[k].store(false, std::memory_order_relaxed);
      mReverb.release();
      mDelay.release();
      mChorus.release();
      mFlanger.release();
      mPhaser.release();
      mWidener.release();
      mAutoPan.release();
   }

   bool ready(int kind) const { return mReady[kind].load(std::memory_order_acquire); }

   // [main thread] Makes an effect's buffers. Only for one that is not ready.
   void allocate(int kind) {
      if (ready(kind))
         return;
      switch (kind) {
      case kFxReverb:
         mReverb.allocate(mSampleRate);
         break;
      case kFxDelay:
         mDelay.allocate(mSampleRate);
         break;
      case kFxChorus:
         mChorus.allocate(mSampleRate);
         break;
      case kFxFlanger:
         mFlanger.allocate(mSampleRate);
         break;
      case kFxPhaser:
         mPhaser.allocate(mSampleRate);
         break;
      case kFxWidener:
         mWidener.allocate(mSampleRate);
         break;
      case kFxAutoPan:
      default:
         mAutoPan.allocate(mSampleRate);
         break;
      }
      mReady[kind].store(true, std::memory_order_release);
   }

   // [audio thread] Every effect parameter of the channel, as real values
   // indexed by FxParamId, and the host's tempo for a synced delay.
   void setParams(const double *real, double tempo) {
      for (int k = 0; k < kNumFxKinds; ++k)
         mOn[k] = real[fxKind(k).first] >= 0.5;

      mReverbS.mix = static_cast<float>(real[kFxReverbMix]);
      mReverbS.predelayMs = static_cast<float>(real[kFxReverbPredelay]);
      mReverbS.size = static_cast<float>(real[kFxReverbSize]);
      mReverbS.decaySec = static_cast<float>(real[kFxReverbDecay] * 0.001);
      mReverbS.diffusion = static_cast<float>(real[kFxReverbDiffusion]);
      mReverbS.damping = static_cast<float>(real[kFxReverbDamping]);
      mReverbS.dampHz = static_cast<float>(real[kFxReverbDampFreq]);
      mReverbS.modDepth = static_cast<float>(real[kFxReverbModDepth]);
      mReverbS.modRate = static_cast<float>(real[kFxReverbModRate]);
      mReverbS.lowCutHz = static_cast<float>(real[kFxReverbLowCut]);
      mReverbS.highCutHz = static_cast<float>(real[kFxReverbHighCut]);
      mReverbS.width = static_cast<float>(real[kFxReverbWidth]);
      mReverbS.freeze = real[kFxReverbFreeze] >= 0.5;

      mDelayS.mode = static_cast<int>(real[kFxDelayMode] + 0.5);
      mDelayS.glide = static_cast<int>(real[kFxDelayGlide] + 0.5);
      const bool sync = real[kFxDelaySync] >= 0.5;
      const double bpm = tempo > 1.0 ? tempo : 120.0;
      mDelayS.timeMs = sync ? static_cast<float>(
                                 fxNoteBeats(static_cast<int>(real[kFxDelayNote] + 0.5)) * 60000.0 / bpm)
                            : static_cast<float>(real[kFxDelayTime]);
      mDelayS.offset = static_cast<float>(real[kFxDelayOffset]);
      mDelayS.feedback = static_cast<float>(real[kFxDelayFeedback]);
      mDelayS.lowCutHz = static_cast<float>(real[kFxDelayLowCut]);
      mDelayS.highCutHz = static_cast<float>(real[kFxDelayHighCut]);
      mDelayS.drive = static_cast<float>(real[kFxDelaySaturation]);
      mDelayS.wow = static_cast<float>(real[kFxDelayWow]);
      mDelayS.wowRate = static_cast<float>(real[kFxDelayWowRate]);
      mDelayS.diffusion = static_cast<float>(real[kFxDelayDiffusion]);
      mDelayS.ducking = static_cast<float>(real[kFxDelayDucking]);
      mDelayS.width = static_cast<float>(real[kFxDelayWidth]);
      mDelayS.mix = static_cast<float>(real[kFxDelayMix]);

      mChorusS.rate = static_cast<float>(real[kFxChorusRate]);
      mChorusS.depth = static_cast<float>(real[kFxChorusDepth]);
      mChorusS.delayMs = static_cast<float>(real[kFxChorusDelay]);
      mChorusS.voices = static_cast<int>(real[kFxChorusVoices] + 0.5);
      mChorusS.width = static_cast<float>(real[kFxChorusWidth]);
      mChorusS.mix = static_cast<float>(real[kFxChorusMix]);

      mFlangerS.rate = static_cast<float>(real[kFxFlangerRate]);
      mFlangerS.depth = static_cast<float>(real[kFxFlangerDepth]);
      mFlangerS.manualMs = static_cast<float>(real[kFxFlangerManual]);
      mFlangerS.feedback = static_cast<float>(real[kFxFlangerFeedback]);
      mFlangerS.width = static_cast<float>(real[kFxFlangerWidth]);
      mFlangerS.mix = static_cast<float>(real[kFxFlangerMix]);

      mPhaserS.rate = static_cast<float>(real[kFxPhaserRate]);
      mPhaserS.depth = static_cast<float>(real[kFxPhaserDepth]);
      mPhaserS.centreHz = static_cast<float>(real[kFxPhaserCentre]);
      mPhaserS.feedback = 0.95f * static_cast<float>(real[kFxPhaserFeedback]);
      mPhaserS.stages = 4 + 2 * static_cast<int>(real[kFxPhaserStages] + 0.5);
      mPhaserS.width = static_cast<float>(real[kFxPhaserWidth]);
      mPhaserS.mix = static_cast<float>(real[kFxPhaserMix]);

      mWidenerS.width = static_cast<float>(real[kFxWidenerWidth]);
      mWidenerS.decorrelation = static_cast<float>(real[kFxWidenerDecorrelation]);
      mWidenerS.bassMonoHz = static_cast<float>(real[kFxWidenerBassMono]);

      mAutoPanS.rate = static_cast<float>(real[kFxAutoPanRate]);
      mAutoPanS.depth = static_cast<float>(real[kFxAutoPanDepth]);
      mAutoPanS.shape = static_cast<int>(real[kFxAutoPanShape] + 0.5);

      for (int k = 0; k < kNumFxKinds; ++k) {
         // A switch fades over 20 ms so it does not click into what is
         // playing. A chain that has played nothing since it was cleared has
         // nothing to click into, and takes its state at once: otherwise the
         // first take after a reset fades its effects in and no other does.
         if (mFresh)
            mActive[k].snap(mOn[k] ? 1.0f : 0.0f);
         else
            mActive[k].set(mOn[k] ? 1.0f : 0.0f);
         mApplied[k] = false;
      }
   }

   // [audio thread] Whether an effect that is switched on has no buffers yet,
   // so the main thread has to be asked for them.
   bool missing() const {
      for (int k = 0; k < kNumFxKinds; ++k)
         if (mOn[k] && !ready(k))
            return true;
      return false;
   }

   bool anyOn() const {
      for (int k = 0; k < kNumFxKinds; ++k)
         if (mOn[k])
            return true;
      return false;
   }

   // [audio thread] Whether anything here is switched on or still fading.
   bool running() const {
      for (int k = 0; k < kNumFxKinds; ++k)
         if (mOn[k] || mActive[k].value() > 0.0f)
            return true;
      return false;
   }

   // [audio thread] Silence going in and silence coming out for a quarter of
   // a second: nothing left of any tail.
   bool quiet() const { return !running() || mQuietRun > static_cast<uint32_t>(0.25f * mSampleRate); }

   // [audio thread] Forgets every tail, as if the effects had just been
   // switched on.
   void clear() {
      for (int k = 0; k < kNumFxKinds; ++k) {
         if (ready(k))
            mNeedsClear[k] = true;
         // Starting over is not the middle of a switch: an effect fading in or
         // out lands where it was going.
         mActive[k].snap(mActive[k].target());
      }
      mQuietRun = 0;
      mFresh = true;
   }

   // [audio thread] In place, stereo, frames <= the prepared block size.
   void process(float *left, float *right, uint32_t frames) {
      mFresh = false;
      if (!running()) {
         mQuietRun = 0;
         return;
      }
      float inPeak = 0.0f;
      for (uint32_t i = 0; i < frames; ++i)
         inPeak = std::max(inPeak, std::max(std::fabs(left[i]), std::fabs(right[i])));

      for (const int k : kRunOrder) {
         if (!ready(k))
            continue;
         Smoother &active = mActive[k];
         if (!mOn[k] && active.value() <= 0.0f)
            continue;
         if (mNeedsClear[k]) {
            clearEffect(k);
            mNeedsClear[k] = false;
            mApplied[k] = false;
         }
         if (!mApplied[k]) {
            applySettings(k);
            mApplied[k] = true;
         }
         std::memcpy(mDryL.data(), left, frames * sizeof(float));
         std::memcpy(mDryR.data(), right, frames * sizeof(float));
         runEffect(k, left, right, frames);
         if (!active.settled() || active.value() < 1.0f) {
            for (uint32_t i = 0; i < frames; ++i) {
               const float a = active.tick();
               left[i] = mDryL[i] + a * (left[i] - mDryL[i]);
               right[i] = mDryR[i] + a * (right[i] - mDryR[i]);
            }
            if (!mOn[k] && active.value() < 1.0e-4f) {
               // Faded out: stop running it, and start it clean next time.
               active.snap(0.0f);
               mNeedsClear[k] = true;
            }
         }
      }

      float outPeak = 0.0f;
      for (uint32_t i = 0; i < frames; ++i)
         outPeak = std::max(outPeak, std::max(std::fabs(left[i]), std::fabs(right[i])));
      if (inPeak < 1.0e-7f && outPeak < 1.0e-6f)
         mQuietRun += frames;
      else
         mQuietRun = 0;
   }

   // The effects themselves, for the self-test.
   Reverb &reverb() { return mReverb; }
   StereoDelay &delay() { return mDelay; }

private:
   // Modulation, then echoes, then the space, then placement.
   static constexpr int kRunOrder[kNumFxKinds] = {kFxPhaser, kFxChorus,  kFxFlanger, kFxDelay,
                                                  kFxReverb, kFxWidener, kFxAutoPan};

   void clearEffect(int k) {
      switch (k) {
      case kFxReverb:
         mReverb.clear();
         break;
      case kFxDelay:
         mDelay.clear();
         break;
      case kFxChorus:
         mChorus.clear();
         break;
      case kFxFlanger:
         mFlanger.clear();
         break;
      case kFxPhaser:
         mPhaser.clear();
         break;
      case kFxWidener:
         mWidener.clear();
         break;
      default:
         mAutoPan.clear();
         break;
      }
   }

   void applySettings(int k) {
      switch (k) {
      case kFxReverb:
         mReverb.set(mReverbS);
         break;
      case kFxDelay:
         mDelay.set(mDelayS);
         break;
      case kFxChorus:
         mChorus.set(mChorusS);
         break;
      case kFxFlanger:
         mFlanger.set(mFlangerS);
         break;
      case kFxPhaser:
         mPhaser.set(mPhaserS);
         break;
      case kFxWidener:
         mWidener.set(mWidenerS);
         break;
      default:
         mAutoPan.set(mAutoPanS);
         break;
      }
   }

   void runEffect(int k, float *l, float *r, uint32_t n) {
      switch (k) {
      case kFxReverb:
         mReverb.process(l, r, n);
         break;
      case kFxDelay:
         mDelay.process(l, r, n);
         break;
      case kFxChorus:
         mChorus.process(l, r, n);
         break;
      case kFxFlanger:
         mFlanger.process(l, r, n);
         break;
      case kFxPhaser:
         mPhaser.process(l, r, n);
         break;
      case kFxWidener:
         mWidener.process(l, r, n);
         break;
      default:
         mAutoPan.process(l, r, n);
         break;
      }
   }

   float mSampleRate = 48000.0f;
   std::atomic<bool> mReady[kNumFxKinds];
   bool mOn[kNumFxKinds] = {};
   bool mNeedsClear[kNumFxKinds] = {};
   bool mApplied[kNumFxKinds] = {};
   Smoother mActive[kNumFxKinds];
   bool mFresh = true; // nothing processed since prepare() or clear()
   uint32_t mQuietRun = 0;
   std::vector<float> mDryL, mDryR;

   Reverb mReverb;
   StereoDelay mDelay;
   Chorus mChorus;
   Flanger mFlanger;
   Phaser mPhaser;
   Widener mWidener;
   AutoPan mAutoPan;
   ReverbSettings mReverbS;
   DelaySettings mDelayS;
   ChorusSettings mChorusS;
   FlangerSettings mFlangerS;
   PhaserSettings mPhaserS;
   WidenerSettings mWidenerS;
   AutoPanSettings mAutoPanS;
};

} // namespace fx
} // namespace verdaliscene
