#pragma once

#include <cmath>

#include "fastmath.h"

namespace verdalis {

// Analogue-style ADSR: linear-ish attack via a one-pole overshoot target,
// exponential decay and release. Drives a noise bed level, and the amplitude
// stamped onto each voice as it is spawned.
//
// Each of the three moving stages can be bent. A curve of 0 is the shape above,
// computed exactly as it always was, so every preset that does not ask for a
// curve renders bit-identically. Any other curve plays the same stage over the
// same length of time, from the same start to the same end, along a bent path
// instead: towards +1 the stage moves at once and then settles, towards -1 it
// holds back and moves late, and somewhere in between it is a straight line.
//
// Both are one family. A one-pole approach towards a target beyond the stage's
// end is, in the stage's own progress y over its own normalised time p,
//
//    y(p) = (1 - e^(-k p)) / (1 - e^(-k)),    k = ln((target - start) / (target - end))
//
// and k is the only thing a curve changes. k > 0 is the analogue shape, k = 0 a
// straight line, k < 0 its mirror image. The curve moves the stage's midpoint
// y(1/2) = 1 / (1 + e^(-k/2)) from where the analogue shape has it towards the
// top or the bottom of its range, which reads the same for every stage whatever
// its natural k happens to be.
class Adsr {
public:
   enum class Stage { Idle, Attack, Decay, Sustain, Release };

   // The analogue shape's constants: how far past its end each stage aims, how
   // its time constant relates to the stage time it is given, and how close to
   // its end a stage that only approaches it counts as there.
   static constexpr float kAttackTarget = 1.2f;
   static constexpr float kReleaseTarget = -0.02f;
   static constexpr float kSettle = 1.0e-4f;
   static constexpr float kAttackTau = 0.4f;
   static constexpr float kDecayTau = 0.35f;
   static constexpr float kReleaseTau = 0.35f;
   // The steepest bend a curve of +-1 reaches.
   static constexpr float kMaxBend = 12.0f;

   void reset() {
      mLevel = 0.0f;
      mStage = Stage::Idle;
      mPlanned = false;
   }

   void setParams(float attackSec, float decaySec, float sustain, float releaseSec,
                  float sampleRate, float attackCurve = 0.0f, float decayCurve = 0.0f,
                  float releaseCurve = 0.0f) {
      mAttackCoef = onePoleCoef(attackSec * kAttackTau, sampleRate);
      mDecayCoef = onePoleCoef(decaySec * kDecayTau, sampleRate);
      mReleaseCoef = onePoleCoef(releaseSec * kReleaseTau, sampleRate);
      mSustain = clampv(sustain, 0.0f, 1.0f);

      // A bent stage is planned from where it starts, so it is planned again
      // from where it has got to only when something it was planned from has
      // changed -- not on every parameter update an engine passes along.
      const float tau[3] = {attackSec * kAttackTau * sampleRate,
                            decaySec * kDecayTau * sampleRate,
                            releaseSec * kReleaseTau * sampleRate};
      const float curve[3] = {clampv(attackCurve, -1.0f, 1.0f), clampv(decayCurve, -1.0f, 1.0f),
                              clampv(releaseCurve, -1.0f, 1.0f)};
      const int now = mStage == Stage::Attack    ? 0
                      : mStage == Stage::Decay   ? 1
                      : mStage == Stage::Release ? 2
                                                 : -1;
      if (now >= 0 && (tau[now] != mTau[now] || curve[now] != mCurve[now]))
         mPlanned = false;
      if (now == 1 && mSustain != mPlannedSustain)
         mPlanned = false;
      for (int i = 0; i < 3; ++i) {
         mTau[i] = tau[i];
         mCurve[i] = curve[i];
      }
   }

   void gateOn() {
      mStage = Stage::Attack;
      mPlanned = false;
   }

   void gateOff() {
      if (mStage != Stage::Idle) {
         mStage = Stage::Release;
         mPlanned = false;
      }
   }

   void kill() { reset(); }

   inline float tick() {
      switch (mStage) {
      case Stage::Idle:
         return 0.0f;
      case Stage::Attack:
         if (mCurve[0] != 0.0f) {
            if (bentStep(kAttackTarget, 1.0f, 0)) {
               mLevel = 1.0f;
               mStage = Stage::Decay;
               mPlanned = false;
            }
            break;
         }
         // Aim past 1.0 so the attack ends in finite time with a near-linear shape.
         mLevel += mAttackCoef * (kAttackTarget - mLevel);
         if (mLevel >= 1.0f) {
            mLevel = 1.0f;
            mStage = Stage::Decay;
         }
         break;
      case Stage::Decay:
         if (mCurve[1] != 0.0f) {
            if (bentStep(mSustain, mSustain + kSettle, 1))
               mStage = Stage::Sustain;
            break;
         }
         mLevel += mDecayCoef * (mSustain - mLevel);
         if (mLevel - mSustain < kSettle)
            mStage = Stage::Sustain;
         break;
      case Stage::Sustain:
         mLevel = mSustain;
         if (mSustain <= 0.0f)
            mStage = Stage::Idle;
         break;
      case Stage::Release:
         if (mCurve[2] != 0.0f) {
            if (bentStep(kReleaseTarget, kSettle, 2)) {
               mLevel = 0.0f;
               mStage = Stage::Idle;
            }
            break;
         }
         mLevel += mReleaseCoef * (kReleaseTarget - mLevel);
         if (mLevel <= kSettle) {
            mLevel = 0.0f;
            mStage = Stage::Idle;
         }
         break;
      }
      return mLevel;
   }

   bool isIdle() const { return mStage == Stage::Idle; }
   bool isReleasing() const { return mStage == Stage::Release; }
   float level() const { return mLevel; }

   // ------------------------------------------------------------ the shape
   //
   // The same arithmetic the envelope plays, for anything that wants to draw
   // it.

   // A stage from `start` towards `target`, ending at `end`: its natural k, and
   // its length in units of its time constant (which is also k). False when it
   // has nowhere to go.
   static bool naturalBend(float start, float target, float end, float &k0) {
      const float span = target - end;
      if (span == 0.0f)
         return false;
      const float ratio = (target - start) / span;
      if (!(ratio > 1.0f))
         return false;
      k0 = std::log(ratio);
      return true;
   }

   // The k a curve gives a stage whose natural k is k0.
   static float bend(float k0, float curve) {
      if (curve == 0.0f)
         return k0;
      const float m0 = midpoint(k0);
      const float top = midpoint(kMaxBend);
      const float bottom = 1.0f - top;
      float m = curve > 0.0f ? m0 + curve * (top - m0) : m0 + curve * (m0 - bottom);
      m = clampv(m, bottom, top);
      return 2.0f * std::log(m / (1.0f - m));
   }

   // The stage's progress, 0 to 1, at its normalised time p, 0 to 1.
   static float shape(float p, float k) {
      if (std::fabs(k) < 1.0e-3f)
         return p;
      return (1.0f - std::exp(-k * p)) / (1.0f - std::exp(-k));
   }

private:
   static float midpoint(float k) { return 1.0f / (1.0f + std::exp(-0.5f * k)); }

   // One sample of a bent stage, planning it first if it has not been. True
   // when the stage has reached its end.
   bool bentStep(float target, float end, int stage) {
      if (!mPlanned) {
         float k0 = 0.0f;
         const float length = mTau[stage];
         mPlannedSustain = mSustain;
         if (!naturalBend(mLevel, target, end, k0) || !(length * k0 >= 1.0f))
            return true; // already there, or no time to get there in
         mFrom = mLevel;
         mTo = end;
         mK = bend(k0, mCurve[stage]);
         mStep = 1.0f / (length * k0);
         mPos = 0.0f;
         mPlanned = true;
      }
      mPos += mStep;
      if (mPos >= 1.0f) {
         mPlanned = false;
         mLevel = mTo;
         return true;
      }
      mLevel = mFrom + (mTo - mFrom) * shape(mPos, mK);
      return false;
   }

   float mLevel = 0.0f;
   float mSustain = 1.0f;
   float mAttackCoef = 0.01f, mDecayCoef = 0.01f, mReleaseCoef = 0.01f;
   Stage mStage = Stage::Idle;

   // The bent stages: their time constants in samples and their curves, and
   // the stage under way.
   float mTau[3] = {0.0f, 0.0f, 0.0f};
   float mCurve[3] = {0.0f, 0.0f, 0.0f};
   bool mPlanned = false;
   float mPlannedSustain = -1.0f;
   float mFrom = 0.0f, mTo = 0.0f, mK = 0.0f, mPos = 0.0f, mStep = 0.0f;
};

} // namespace verdalis
