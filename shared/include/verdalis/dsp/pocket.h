#pragma once

// A drop landing on a surface, as a sounding voice.
//
// This is RiverFlow's Trickle layer, lifted out so that RainyDay can use the
// same one. Not a reimplementation of it: the same code, drawing its random
// numbers in the same order, so the two plugins produce the same sound from
// the same settings.
//
// What a drop landing on wet stone actually is, in three parts:
//
//   the pocket    A decaying sinusoid at the Minnaert pitch for the radius of
//                 air the drop entrained, bending upward as the pocket shrinks.
//                 Struck at phase zero, because that is where the impulse
//                 response of a damped oscillator starts and anywhere else is
//                 a click.
//   the water     A band of noise at the pocket's own pitch: the water being
//                 displaced around it. Without this a cascade of pockets is a
//                 music box.
//   the surface   One band, at the surface's own resonance, carrying both the
//                 impact and the wash that follows it -- the impact is the fast
//                 decay through that band and the splash is the slow one. This
//                 is the part that makes it stone rather than a bucket.
//
// Sizes are radii in millimetres, because that is what a pocket's pitch
// physically is; see bubble.h for Minnaert's relation and why it is trusted.
//
// The strike can also be a smack rather than a tick, which is what a raindrop
// arriving at terminal velocity makes of a wet pavement and a drop falling a
// hand's width into a stream does not. Measured on close recordings of rain on
// wet concrete (rainyday/tools/analysis/README.md), a smack is three things in
// the surface's band, and the generator above already had the second and third:
//
//   the click     One impulse: 0.18 ms wide at -6 dB, a cycle and a half of the
//                 surface band and nothing else. This is the part that was
//                 missing, and it is what the ear reads as *hitting* something.
//   the snap      Noise 16 dB under it, gone in about 3 ms: the drop collapsing.
//   the body      Noise falling 60 dB in about 54 ms: the wet part, water
//                 thrown out across the film it landed in.
//
// and then, about once per drop, the same again smaller a few milliseconds
// later, because what it throws up comes back down. RainyDay and RiverFlow both
// ask for it (askForSmack below); a spec that does not gets the tick, exactly as
// this file rendered it before the smack existed.
//
// Nothing the smack adds is drawn from the caller's generator. A drop's own
// scatter, how many times it strikes and when, all come from the generator the
// pocket carries, so the caller's sequence is the one it always was -- which is
// what lets a plugin take the smack and still render the same river, or the
// same rain, at the same Seed, with only the drop changed.

#include <cmath>

#include "bubble.h"
#include "fastmath.h"
#include "filters.h"
#include "rng.h"

namespace verdalis {

// One pocket of air, sounding. Pooled and walked every sample while alive, so
// it carries four bytes of RNG state rather than a full generator.
struct Pocket {
   bool active = false;
   // A cluster of pockets is spread over its spill time, so a pocket can be
   // spawned now and start later. Counted down before anything else happens.
   int delaySamples = 0;

   // The ringing air.
   float phase = 0.0f;
   float inc = 0.0f;
   float level = 0.0f;
   float decayCoef = 0.0f;
   // Expressed as the total rise spread over the pocket's life. Applying it as
   // a per-sample factor compounds it into the megahertz within a millisecond,
   // which is a bug this suite has already had twice.
   float chirp = 1.0f;

   // The water around it being displaced.
   Svf bodyBand;
   float noiseMix = 0.0f;

   // What it struck.
   Svf surfaceBand;
   float clickLevel = 0.0f;
   float clickCoef = 0.0f;
   float splashLevel = 0.0f;
   float splashCoef = 0.0f;

   // The smack. `impulse` is one sample fed into the surface band and then
   // spent; the rest re-arm it for the strikes that follow the first.
   float impulse = 0.0f;
   float strikeGain = 0.0f;  // the click against the snap behind it
   float snapLevel = 0.0f;   // the snap's level at the first strike
   float spatterGap = 0.0f;  // mean samples between secondary strikes
   int spatterMin = 0;       // and the least there can be
   int spatterLeft = 0;
   int spatterNext = 0;

   float panL = 0.7071f, panR = 0.7071f;
   RngLite rng;

   inline bool finished() const {
      return delaySamples <= 0 && level < 1.0e-5f && clickLevel < 1.0e-6f &&
             splashLevel < 1.0e-6f;
   }
};

// Everything a drop needs to know about itself. Levels and rates stay with the
// caller; this is the shape of one event.
struct TrickleSpec {
   float sizeMm = 1.24f;      // radius of the pocket entrained, hence its pitch
   float spreadOct = 1.2f;    // spread of sizes, in octaves
   float decaySec = 0.013f;   // how long one drop lasts
   float impact = 0.45f;      // the strike against the pocket that follows it
   float stoneToneHz = 3500.0f; // the surface's own resonance
   float splash = 0.25f;      // the wash where the water is deeper
   float width = 0.55f;       // stereo spread of the population
   float levelSigma = 0.4f;   // log-normal spread of event levels, in octaves
   float smear = 1.0f;        // distance smearing of the edges, 1 = none

   // The smack. Every one of these at its default is the generator as it was.
   float strike = 0.0f;         // the click at the leading edge; 1 is as measured
   float clickSec = 0.0022f;    // how long the snap behind it takes to fall 60 dB
   float stoneSpreadOct = 0.0f; // scatter of the surface band from drop to drop,
                                // one sigma in octaves; 0 keeps the old +-20 %
   float spatter = 0.0f;        // mean number of secondary strikes per drop
   float spatterGapSec = 0.004f; // mean time between them
};

// The click against the snap, as amplitudes at 48 kHz: an impulse of 4.26 units
// into the surface band for every unit of noise. Fitted, with the snap and the
// body, against the median of the two hundred loudest strikes in each of two
// recordings; the three together land within 1.5 dB of it from 0.3 to 7 ms.
constexpr float kStrikeImpulse = 4.26f;
// Strikes of one drop do not land on top of each other: nothing was measured
// closer than this.
constexpr float kSpatterMinGapSec = 0.0015f;

// The rest of the smack as it was measured, for whoever asks for one.
//
// How long the snap behind the click takes to fall 60 dB, and how far the
// surface band is scattered from one strike to the next: the click's spectral
// centroid sits at 8.0 and 7.5 kHz in the two recordings, one sigma 0.42 and
// 0.47 octaves apart, and a band scattered by 0.6 measures the same through
// the same analysis.
constexpr float kSmackSnapSec = 0.0032f;
constexpr float kSmackSpreadOct = 0.6f;
// The body against the snap, as a TrickleSpec's splash: at 0.25 with the strike
// at full the two stand 2.7 to 1 in amplitude, which is what was measured.
constexpr float kSmackBody = 0.25f;
// Secondary strikes per unit of splash. The density of strikes in the 2 to 4 ms
// after a strike is twice the mean rate in both recordings and back to it by
// 30 ms -- about one more strike per drop -- and 1.2 at a splash of 0.25 is that.
constexpr float kSmackSpatterPerSplash = 4.8f;

// Turns a spec into a smack: the click, the snap, the scatter and the spatter
// that goes with however wet the spec already says the drop is. Levels, sizes
// and the spread of levels are left as the caller set them.
inline void askForSmack(TrickleSpec &spec) {
   spec.strike = 1.0f;
   spec.clickSec = kSmackSnapSec;
   spec.stoneSpreadOct = kSmackSpreadOct;
   spec.spatter = kSmackSpatterPerSplash * clampv(spec.splash, 0.0f, 1.0f);
}

// A normal deviate from a pocket's own small generator, for the few draws the
// smack makes at spawn. Box and Muller's, one value per call.
inline float pocketGaussian(RngLite &rng) {
   const float u = rng.uniformPositive();
   return std::sqrt(-2.0f * std::log(u)) * sin2pi(rng.uniformPositive());
}

// Makes a pocket that has just been spawned louder or quieter as a whole. For a
// caller that decides a drop's level from something only it knows -- where on a
// surface the drop landed -- after the drop exists.
inline void scalePocket(Pocket &p, float gain) {
   p.level *= gain;
   p.clickLevel *= gain;
   p.splashLevel *= gain;
   p.snapLevel *= gain;
}

// A log-normal multiplier with unit mean, `sigmaOct` octaves wide, bounded at
// three sigma. Event energies follow the volume of water involved, which is
// log-normal rather than uniform -- drawing them uniformly costs about 4 dB of
// crest factor. Bounded because a real distribution is, and because an
// unbounded tail drives an output stage into its clipper.
inline float pocketLogNormal(Rng &rng, float sigmaOct) {
   const float g = clampv(rng.gaussian(), -3.0f, 3.0f);
   const float f = std::exp2(g * sigmaOct);
   const float s = sigmaOct * 0.6931472f;
   return f / std::exp(0.5f * s * s);
}

// Start one drop. `levelBase` is the peak amplitude before this event's own
// log-normal scatter is applied.
//
// The order of draws from `rng` is part of the contract: two plugins sharing
// this function render identically from the same seed only while it is
// unchanged, so do not reorder them, and do not add to them -- see the note on
// the smack at the top of this file.
inline void spawnTricklePocket(Pocket &p, const TrickleSpec &spec, Rng &rng, float sampleRate,
                               float levelBase) {
   p.active = true;
   p.rng.seed(rng.next() | 1u);
   p.delaySamples = 0;

   const float pan = clampv(rng.white() * clampv(spec.width, 0.0f, 1.0f), -1.0f, 1.0f);
   p.panL = std::sqrt(0.5f * (1.0f - pan));
   p.panR = std::sqrt(0.5f * (1.0f + pan));

   const float oct = rng.gaussian() * clampv(spec.spreadOct, 0.0f, 3.0f) * 0.5f;
   const float r = clampv(spec.sizeMm * std::exp2(oct), 0.05f, 12.0f);
   const float f = clampv(minnaertHz(r), 200.0f, 0.45f * sampleRate);

   // A drop's pocket is asked to last a set time rather than to obey the
   // physics alone: the decay is measured directly from event-triggered
   // envelopes, and no damping control is offered, because a drop that has just
   // landed is not a free bubble.
   const float decay = clampv(spec.decaySec, 0.001f, 0.5f) * spec.smear;
   p.decayCoef = decayCoef(decay * 0.5f, sampleRate);
   const float lifeSamples = std::max(4.0f, decay * 0.5f * sampleRate);

   p.phase = 0.0f;
   p.inc = f / sampleRate;
   const float rise = 1.04f + 0.12f * rng.uniform();
   p.chirp = std::pow(rise, 1.0f / lifeSamples);

   const float impact = clampv(spec.impact, 0.0f, 1.0f);
   const float lvl = levelBase * pocketLogNormal(rng, spec.levelSigma);
   // A tick leaves a quarter of the pocket under it however hard the strike is
   // turned up. A smack can be all strike: rain on stone traps nothing.
   const bool smack = spec.strike > 0.0f;
   const float pocketKept = smack ? 1.0f : 0.75f;
   p.level = lvl * (1.0f - pocketKept * impact);

   p.bodyBand.reset();
   p.bodyBand.setCutoff(clampv(f * 1.1f, 80.0f, 0.45f * sampleRate), resonanceForQ(2.4f),
                        sampleRate);
   p.noiseMix = 0.35f + 0.4f * rng.uniform();

   // What it landed on. Hard wet rock is high and short; moss and gravel are
   // lower and duller.
   float stone =
      clampv(spec.stoneToneHz * (0.8f + 0.4f * rng.uniform()), 200.0f, 0.45f * sampleRate);
   // That was the last draw from `rng`. Everything the smack adds below is
   // drawn from the pocket's own generator.
   if (spec.stoneSpreadOct > 0.0f)
      stone = clampv(spec.stoneToneHz * std::exp2(spec.stoneSpreadOct *
                                                  clampv(pocketGaussian(p.rng), -2.5f, 2.5f)),
                     200.0f, 0.45f * sampleRate);
   p.surfaceBand.reset();
   p.surfaceBand.setCutoff(stone, resonanceForQ(1.1f), sampleRate);
   p.clickLevel = lvl * impact * 0.6f;
   p.clickCoef = decayCoef(spec.clickSec * spec.smear, sampleRate);

   p.splashLevel = lvl * clampv(spec.splash, 0.0f, 1.0f) * 0.9f;
   p.splashCoef = decayCoef(decay * 3.0f, sampleRate);

   p.impulse = 0.0f;
   p.strikeGain = 0.0f;
   p.snapLevel = 0.0f;
   p.spatterLeft = 0;
   p.spatterNext = 0;
   p.spatterMin = 0;
   if (smack && p.clickLevel > 0.0f) {
      // The band's response to one sample falls as the sample rate rises and
      // its response to noise falls as the root of it, so the click is scaled
      // by the root to keep its place against the snap at any rate.
      p.strikeGain = spec.strike * kStrikeImpulse * std::sqrt(sampleRate / 48000.0f);
      p.snapLevel = p.clickLevel;
      // The band's output is scaled by both envelopes, so the impulse is
      // divided by their sum to come out at the snap's level and not theirs.
      p.impulse = p.strikeGain * p.clickLevel / (p.clickLevel + p.splashLevel);

      // How many more strikes: Poisson, by Knuth's product of uniforms, which is
      // exact and cheap at a mean of one or two.
      const float mean = clampv(spec.spatter, 0.0f, 8.0f);
      if (mean > 0.0f) {
         const float limit = std::exp(-mean);
         float prod = p.rng.uniformPositive();
         int n = 0;
         while (prod > limit && n < 24) {
            prod *= p.rng.uniformPositive();
            ++n;
         }
         p.spatterLeft = n;
         p.spatterGap = std::max(1.0f, spec.spatterGapSec * sampleRate);
         p.spatterMin = static_cast<int>(kSpatterMinGapSec * sampleRate);
         p.spatterNext = p.spatterMin + static_cast<int>(-std::log(p.rng.uniformPositive()) *
                                                         p.spatterGap);
      }
   }
}

// Render a pool of pockets into a stereo block, additively.
inline void processPocketPool(Pocket *pool, size_t count, float *outL, float *outR,
                              uint32_t numSamples) {
   for (size_t k = 0; k < count; ++k) {
      Pocket &p = pool[k];
      if (!p.active)
         continue;
      for (uint32_t i = 0; i < numSamples; ++i) {
         if (p.delaySamples > 0) {
            --p.delaySamples;
            continue;
         }
         float s = 0.0f;
         // A smack with no pocket under it has a level of exactly zero and
         // skips the oscillator. A pocket that has merely decayed never gets
         // there while it is alive, so this changes nothing it renders.
         if (p.level != 0.0f) {
            p.inc *= p.chirp;
            // A ceiling well short of Nyquist, whatever the chirp is asked for.
            // sin2piFast reads an interpolated 4096-entry table, and a phase
            // increment near 0.5 walks it in steps large enough for the
            // interpolation error to become broadband noise rather than a tone.
            if (p.inc > 0.40f)
               p.inc = 0.40f;
            p.phase += p.inc;
            if (p.phase >= 1.0f)
               p.phase -= 1.0f;

            s = sin2piFast(p.phase) * p.level;
            if (p.noiseMix > 1.0e-4f)
               s += p.bodyBand.bandpassNormalised(p.rng.white()) * p.level * p.noiseMix;
         }

         if (p.clickLevel > 1.0e-6f || p.splashLevel > 1.0e-6f) {
            float in = p.rng.white();
            if (p.impulse != 0.0f) {
               in += p.impulse;
               p.impulse = 0.0f;
            }
            const float surf = p.surfaceBand.bandpassNormalised(in);
            s += surf * (p.clickLevel + p.splashLevel);
            p.clickLevel *= p.clickCoef;
            p.splashLevel *= p.splashCoef;

            // The next strike of the same drop: the click and the snap again,
            // smaller, on top of whatever is left of the first.
            if (p.spatterLeft > 0 && --p.spatterNext <= 0) {
               --p.spatterLeft;
               const float u = 0.25f + 0.75f * p.rng.uniformPositive();
               const float snap = p.snapLevel * u;
               p.clickLevel += snap;
               p.impulse = p.strikeGain * snap / (p.clickLevel + p.splashLevel);
               p.spatterNext = p.spatterMin + static_cast<int>(-std::log(p.rng.uniformPositive()) *
                                                               p.spatterGap);
            }
         }

         outL[i] += s * p.panL;
         outR[i] += s * p.panR;
         p.level *= p.decayCoef;
      }
      if (p.finished())
         p.active = false;
   }
}

} // namespace verdalis
