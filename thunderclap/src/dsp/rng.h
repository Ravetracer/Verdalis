#pragma once

#include <cmath>
#include <cstdint>

namespace thunderclap {

// xoshiro128+ -- small state, fast, and statistically far better than the
// rand()-style LCGs usually used for noise. Everything in this plugin
// (channel geometry, shock scatter, storm timing, the rumble noise) comes out of here, so it
// has to be both cheap and free of audible periodicity.
class Rng {
public:
   explicit Rng(uint32_t seed = 0x1234567u) { reseed(seed); }

   void reseed(uint32_t seed) {
      // SplitMix32 to spread a small seed over the full state.
      uint32_t z = seed + 0x9E3779B9u;
      for (int i = 0; i < 4; ++i) {
         z += 0x9E3779B9u;
         uint32_t x = z;
         x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
         x = (x ^ (x >> 13)) * 0xC2B2AE35u;
         mState[i] = x ^ (x >> 16);
      }
      if (!(mState[0] | mState[1] | mState[2] | mState[3]))
         mState[0] = 0xDEADBEEFu;
      for (int i = 0; i < 16; ++i)
         next();
      mHasSpare = false;
   }

   inline uint32_t next() {
      const uint32_t result = mState[0] + mState[3];
      const uint32_t t = mState[1] << 9;
      mState[2] ^= mState[0];
      mState[3] ^= mState[1];
      mState[1] ^= mState[2];
      mState[0] ^= mState[3];
      mState[2] ^= t;
      mState[3] = (mState[3] << 11) | (mState[3] >> 21);
      return result;
   }

   // Uniform in [0, 1).
   inline float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }

   // Uniform in [-1, 1) -- the white noise primitive.
   inline float white() { return uniform() * 2.0f - 1.0f; }

   // Uniform in (0, 1], safe to feed to log().
   inline float uniformPositive() {
      return ((next() >> 8) + 1) * (1.0f / 16777216.0f);
   }

   // Normal(0, 1) via Marsaglia polar method, two values per pair of calls.
   inline float gaussian() {
      if (mHasSpare) {
         mHasSpare = false;
         return mSpare;
      }
      float u, v, s;
      do {
         u = white();
         v = white();
         s = u * u + v * v;
      } while (s >= 1.0f || s == 0.0f);
      s = std::sqrt(-2.0f * std::log(s) / s);
      mSpare = v * s;
      mHasSpare = true;
      return u * s;
   }

   // Exponentially distributed waiting time with the given rate (events per
   // unit time). This is what makes a storm a proper Poisson process rather
   // than a metronome with jitter.
   inline float exponential(float rate) {
      if (rate <= 0.0f)
         return 1e30f;
      return -std::log(uniformPositive()) / rate;
   }

private:
   uint32_t mState[4]{};
   float mSpare = 0.0f;
   bool mHasSpare = false;
};

} // namespace thunderclap
