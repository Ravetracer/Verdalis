#pragma once

#if defined(__SSE2__) || defined(__x86_64__)
#   include <xmmintrin.h>
#   define VERDALIS_HAVE_MXCSR 1
#elif defined(__aarch64__)
#   include <cstdint>
#   define VERDALIS_HAVE_FPCR 1
#endif

namespace verdalis {

// Disables denormal arithmetic for the lifetime of the object and restores the
// previous FPU mode afterwards.
//
// Reverb feedback paths and filter states decay towards zero and spend a long
// time in the denormal range, where x86 arithmetic slows down by an order of
// magnitude. ARM (Apple Silicon) gets the same through FPCR.FZ, which costs as
// little and keeps the two architectures numerically alike. Setting
// flush-to-zero here -- rather than compiling with
// -ffast-math, which links crtfastmath.o and changes the mode for the whole
// host process -- keeps the fix contained to our own process() call.
class ScopedNoDenormals {
public:
   ScopedNoDenormals() {
#ifdef VERDALIS_HAVE_MXCSR
      mSaved = _mm_getcsr();
      // 0x8000 = flush-to-zero, 0x0040 = denormals-are-zero
      _mm_setcsr((mSaved | 0x8000u | 0x0040u));
#elif defined(VERDALIS_HAVE_FPCR)
      uint64_t fpcr;
      __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
      mSaved = fpcr;
      // bit 24 = FZ, flush denormal inputs and results to zero
      fpcr |= (uint64_t{1} << 24);
      __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#endif
   }

   ~ScopedNoDenormals() {
#ifdef VERDALIS_HAVE_MXCSR
      _mm_setcsr(mSaved);
#elif defined(VERDALIS_HAVE_FPCR)
      __asm__ __volatile__("msr fpcr, %0" : : "r"(mSaved));
#endif
   }

   ScopedNoDenormals(const ScopedNoDenormals &) = delete;
   ScopedNoDenormals &operator=(const ScopedNoDenormals &) = delete;

private:
#ifdef VERDALIS_HAVE_MXCSR
   unsigned int mSaved = 0;
#elif defined(VERDALIS_HAVE_FPCR)
   uint64_t mSaved = 0;
#endif
};

} // namespace verdalis
