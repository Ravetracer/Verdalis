#pragma once

// The modulated-delay effects and the phaser.
//
// Chorus and flanger are one structure with different numbers (DAFX 2nd ed.,
// table 2.9; Pirkle table 15.1): a delay read at a position an LFO moves. A
// chorus reads 1 to 30 ms back and moves around that centre, so the copy is
// heard as a second voice slightly out of tune; a flanger reads 0.1 to 10 ms
// back, moving up from a minimum, so the copy and the original comb against
// each other and the notches sweep.
//
// The chorus has up to four voices per side, their LFOs spread evenly around
// the cycle and their rates detuned by irrational ratios so the pattern never
// repeats audibly (Pirkle 18675ff). Width offsets the right side's LFOs by up
// to a quarter cycle: 90 degrees is the widest that does not start to cancel
// in the room (Pirkle 15.1.1).
//
// The flanger sweeps exponentially, so equal LFO movement is an equal step in
// the notches' pitch, and feeds the moving tap back, positive or negative.
//
// The phaser is Pirkle's six-stage design (sec. 13.6): a cascade of
// first-order allpasses (DAFX eq. 2.13/2.14) whose break frequencies an LFO
// sweeps together, the output summed with the input so the phase shift turns
// into notches, with feedback for depth.

#include "fx_dsp.h"

namespace verdaliscene {
namespace fx {

struct ChorusSettings {
   float rate = 0.4f;    // Hz
   float depth = 0.5f;   // 0..1
   float delayMs = 12.0f;
   int voices = 2;       // 1..4
   float width = 1.0f;   // 0..1
   float mix = 0.5f;     // 0..1
};

class Chorus {
public:
   void allocate(float sampleRate) {
      mSampleRate = sampleRate;
      for (auto &l : mLine)
         l.allocate(static_cast<size_t>(0.1f * sampleRate) + 8);
      mMix.setTime(0.02f, sampleRate);
      mDelay.setTime(0.1f, sampleRate);
      mFirst = true;
      clear();
   }
   void release() {
      for (auto &l : mLine)
         l.release();
   }
   void clear() {
      for (auto &l : mLine)
         l.clear();
      for (auto &lfo : mLfo)
         lfo.setPhase(0.0f);
   }
   void set(const ChorusSettings &s) {
      mS = s;
      for (int v = 0; v < 4; ++v)
         mLfo[v].setRate(std::max(0.01f, s.rate) * kDetune[v], mSampleRate);
      mMix.set(clampv(s.mix, 0.0f, 1.0f));
      mDelay.set(clampv(s.delayMs, 1.0f, 40.0f) * 0.001f * mSampleRate);
      if (mFirst) {
         mMix.snap(mMix.target());
         mDelay.snap(mDelay.target());
         mFirst = false;
      }
   }
   void process(float *left, float *right, uint32_t frames) {
      const int voices = clampv(mS.voices, 1, 4);
      const float depth = clampv(mS.depth, 0.0f, 1.0f);
      const float spread = 0.25f * clampv(mS.width, 0.0f, 1.0f);
      const float norm = 1.0f / std::sqrt(static_cast<float>(voices));
      for (uint32_t n = 0; n < frames; ++n) {
         for (int v = 0; v < voices; ++v)
            mLfo[v].advance();
         const float centre = mDelay.tick();
         // Around the centre by up to 45 %: deep, and never through zero.
         const float sweep = 0.45f * depth * centre;
         const float x[2] = {left[n], right[n]};
         mLine[0].write(x[0]);
         mLine[1].write(x[1]);
         float wet[2] = {0.0f, 0.0f};
         for (int v = 0; v < voices; ++v) {
            // Spread evenly round the cycle, and the right side a little
            // further on.
            const float phase = static_cast<float>(v) / voices;
            for (int c = 0; c < 2; ++c) {
               const float lfo = mLfo[v].sine(phase + (c ? spread : 0.0f));
               wet[c] += mLine[c].readCubic(std::max(1.0f, centre + sweep * lfo));
            }
         }
         const float mix = mMix.tick();
         left[n] = x[0] + mix * (wet[0] * norm - x[0]);
         right[n] = x[1] + mix * (wet[1] * norm - x[1]);
      }
   }

private:
   static constexpr float kDetune[4] = {1.0f, 1.13f, 1.29f, 1.41f};
   ChorusSettings mS;
   float mSampleRate = 48000.0f;
   bool mFirst = true;
   Delay mLine[2];
   Lfo mLfo[4];
   Smoother mMix, mDelay;
};

struct FlangerSettings {
   float rate = 0.15f;   // Hz
   float depth = 0.6f;   // 0..1
   float manualMs = 1.5f; // the shortest delay the sweep reaches
   float feedback = 0.5f; // -1..1
   float width = 0.5f;   // 0..1
   float mix = 0.5f;     // 0..1
};

class Flanger {
public:
   void allocate(float sampleRate) {
      mSampleRate = sampleRate;
      for (auto &l : mLine)
         l.allocate(static_cast<size_t>(0.06f * sampleRate) + 8);
      mMix.setTime(0.02f, sampleRate);
      mManual.setTime(0.05f, sampleRate);
      mFb.setTime(0.02f, sampleRate);
      mFirst = true;
      clear();
   }
   void release() {
      for (auto &l : mLine)
         l.release();
   }
   void clear() {
      for (auto &l : mLine)
         l.clear();
      mLfo.setPhase(0.0f);
   }
   void set(const FlangerSettings &s) {
      mS = s;
      mLfo.setRate(std::max(0.01f, s.rate), mSampleRate);
      mMix.set(clampv(s.mix, 0.0f, 1.0f));
      mManual.set(clampv(s.manualMs, 0.1f, 10.0f) * 0.001f * mSampleRate);
      mFb.set(0.95f * clampv(s.feedback, -1.0f, 1.0f));
      if (mFirst) {
         mMix.snap(mMix.target());
         mManual.snap(mManual.target());
         mFb.snap(mFb.target());
         mFirst = false;
      }
   }
   void process(float *left, float *right, uint32_t frames) {
      const float depth = clampv(mS.depth, 0.0f, 1.0f);
      const float offset = 0.25f * clampv(mS.width, 0.0f, 1.0f);
      const float maxDelay = 0.05f * mSampleRate;
      for (uint32_t n = 0; n < frames; ++n) {
         mLfo.advance();
         const float manual = mManual.tick();
         const float fb = mFb.tick();
         const float x[2] = {left[n], right[n]};
         float y[2];
         for (int c = 0; c < 2; ++c) {
            // Up from the manual delay by as much as three octaves of comb
            // spacing, on a triangle, so the notches move at an even pace.
            const float u = 0.5f + 0.5f * mLfo.triangle(c ? offset : 0.0f);
            const float d = clampv(manual * std::exp2(3.0f * depth * u), 1.0f, maxDelay);
            // Read before this sample is written: the feedback is the tap
            // itself, one comb.
            y[c] = mLine[c].readCubic(d);
            mLine[c].write(flush(x[c] + fb * y[c]));
         }
         const float mix = mMix.tick();
         // Normalised by the comb's peak gain, 1 / (1 - |fb|) (DAFX eq. 2.67),
         // halfway: full normalisation flattens what feedback is for.
         const float norm = 1.0f / (1.0f + 0.5f * std::fabs(fb) / (1.0f - std::fabs(fb)));
         left[n] = x[0] + mix * (y[0] * norm - x[0]);
         right[n] = x[1] + mix * (y[1] * norm - x[1]);
      }
   }

private:
   FlangerSettings mS;
   float mSampleRate = 48000.0f;
   bool mFirst = true;
   Delay mLine[2];
   Lfo mLfo;
   Smoother mMix, mManual, mFb;
};

struct PhaserSettings {
   float rate = 0.2f;      // Hz
   float depth = 0.7f;     // 0..1
   float centreHz = 800.0f;
   float feedback = 0.5f;  // 0..0.95
   int stages = 6;         // 4, 6 or 8
   float width = 0.5f;     // 0..1
   float mix = 0.5f;       // 0..1
};

class Phaser {
public:
   static constexpr int kMaxStages = 8;

   void allocate(float sampleRate) {
      mSampleRate = sampleRate;
      mMix.setTime(0.02f, sampleRate);
      mFirst = true;
      clear();
   }
   void release() {}
   void clear() {
      for (auto &ch : mZ)
         for (float &z : ch)
            z = 0.0f;
      mLast[0] = mLast[1] = 0.0f;
      mLfo.setPhase(0.0f);
   }
   void set(const PhaserSettings &s) {
      mS = s;
      mLfo.setRate(std::max(0.01f, s.rate), mSampleRate);
      mMix.set(clampv(s.mix, 0.0f, 1.0f));
      if (mFirst) {
         mMix.snap(mMix.target());
         mFirst = false;
      }
   }
   void process(float *left, float *right, uint32_t frames) {
      const int stages = mS.stages <= 4 ? 4 : (mS.stages >= 8 ? 8 : 6);
      const float depth = clampv(mS.depth, 0.0f, 1.0f);
      const float fb = clampv(mS.feedback, 0.0f, 0.95f);
      const float offset = 0.25f * clampv(mS.width, 0.0f, 1.0f);
      const float centre = clampv(mS.centreHz, 50.0f, 0.2f * mSampleRate);
      for (uint32_t n = 0; n < frames; ++n) {
         mLfo.advance();
         const float x[2] = {left[n], right[n]};
         const float mix = mMix.tick();
         float out[2];
         for (int c = 0; c < 2; ++c) {
            // The sweep: up to two octaves either side of the centre.
            const float f = centre * std::exp2(2.0f * depth * mLfo.sine(c ? offset : 0.0f));
            const float t = std::tan(kPi * std::min(f, 0.45f * mSampleRate) / mSampleRate);
            const float a = (t - 1.0f) / (t + 1.0f); // DAFX eq. 2.14
            float v = x[c] + fb * mLast[c];
            for (int k = 0; k < stages; ++k) {
               // First-order allpass, y = a x + x1 - a y1, in one state.
               const float y = a * v + mZ[c][k];
               mZ[c][k] = flush(v - a * y);
               v = y;
            }
            mLast[c] = v;
            out[c] = x[c] + mix * (0.5f * (x[c] + v) - x[c]);
         }
         left[n] = out[0];
         right[n] = out[1];
      }
   }

private:
   PhaserSettings mS;
   float mSampleRate = 48000.0f;
   bool mFirst = true;
   float mZ[2][kMaxStages] = {};
   float mLast[2] = {0.0f, 0.0f};
   Lfo mLfo;
   Smoother mMix;
};

} // namespace fx
} // namespace verdaliscene
