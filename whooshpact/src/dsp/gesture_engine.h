#pragma once

// WhooshPact's synthesis: transitions, impacts and everything between them.
//
// This is the one plugin in the suite that does not model a natural sound
// source. There is no physics here to fit, because the reference library is
// not recordings of the world -- it is 211 finished production sounds, made by
// somebody else out of synthesis and processing. What can be measured about
// them is what they *are*, not what made them, and that is what the engine is
// built from (`tools/analysis/`):
//
//   the shape in time   Every reference is one gesture with a beginning, a
//                       peak and an end, and where the peak sits separates the
//                       six families more sharply than anything else measured:
//                       a boom peaks 2.4 % into its span, an impact at 6.0 %,
//                       a braam at 14.8 %, a transition at 32.8 %. The
//                       amplitude contours of all six are reproduced by five
//                       numbers -- span, peak, hold, rise curvature, fall
//                       curvature -- to within the library's own spread.
//
//   the sweep           The spectral centroid moves across the gesture, and it
//                       moves *late*: a transition's is flat for its first
//                       third and then falls 0.80 octaves. Braams are the only
//                       family that rises, by 0.60. Hence Air Sweep and Air
//                       Curve rather than an envelope on a filter.
//
//   the bottom          97 % of a boom's energy and 98 % of a downshifter's is
//                       below 100 Hz, against 62 % for a transition. The
//                       median fundamental of the pitched families is 41-49 Hz
//                       -- all within a tone of G1. The low layer is not a
//                       garnish here, it is the instrument.
//
//   the flutter         And mostly, there isn't any. The fraction of a
//                       reference's modulation energy sitting at one rate is
//                       0.02-0.05 in five of the six families, which is noise
//                       with no rate in it. Where it does appear it is the
//                       downshifters, and it *glides*: `Downshifter - Stutter
//                       Scream` accelerates from 4.1 Hz to 25.0 Hz across its
//                       span. So flutter is a deliberate effect with a
//                       start-and-end speed, off by default, and not a thing
//                       every preset has.
//
// Four layers, and they divide on how they are driven, which is worth
// understanding before touching any of them:
//
//   air     noise of a chosen colour through a swept resonant filter. Follows
//           the gesture envelope. On its own this is the plain white-noise
//           whoosh, which is the simplest thing the plugin makes.
//   tone    a three-oscillator stack with a pitch glide. Follows the gesture
//           envelope. Braams and the top half of a downshifter.
//   sub     a driven sine with a pitch drop. **Struck** at the gesture's peak
//           and then decaying on its own, because that is what a boom is: one
//           event with a decay, not a curve with a low end.
//   hit     a transient: three inharmonic resonators morphing towards filtered
//           noise. **Struck** at its own point in the span, which is the peak
//           for an impact and the end for a whoosh-hit.
//
// And the reason the plugin exists at all is Variation. A sample plays the
// same transition every time; here every note draws its own span, peak,
// cutoff, pitch, pan, flutter rate and level around the settings on screen.
// Setting it to zero makes the plugin deterministic, which is what a
// transition that has to land on a cut needs.
//
// No samples. The two tables of six numbers that carry the measured family
// profiles are formulas; see the suite's note on what pure synthesis does and
// does not forbid.

#include "../params.h"

#include "verdalis/dsp/adsr.h"
#include "verdalis/dsp/biquad.h"
#include "verdalis/dsp/fastmath.h"
#include "verdalis/dsp/filters.h"
#include "verdalis/dsp/reverb.h"
#include "verdalis/dsp/rng.h"

#include <cstdint>

namespace whooshpact {

// The Tone layer's stack. Three is what the measurement asks of it -- the
// braams' low partial is a cluster rather than a line -- and it is also the
// point past which adding oscillators only costs cycles.
constexpr int kToneOscs = 3;
// The Hit layer's inharmonic cluster. Three again: two read as an interval,
// four costs more than it adds once the noise is mixed over them.
constexpr int kHitModes = 3;

struct EngineParams {
   float gain = 1.0f; // linear
   float drive = 0.12f;

   // gesture
   int type = kGestureTransition;
   float blend = 0.0f;
   float spanSec = 3.7f;
   float peak = 0.33f;
   float hold = 0.10f;
   float rise = 1.8f;
   float fall = 2.5f;
   float variation = 0.35f;

   // air
   int noise = kNoiseWhite;
   float airGain = 0.501f; // linear
   float airCutoffHz = 2400.0f;
   float airSweepOct = -0.8f;
   float airReso = 0.25f;
   float airCurve = 2.0f;
   float airTilt = 0.0f;
   float airWidth = 0.8f;

   // tone
   int wave = kWaveSaw;
   float toneGain = 0.0f; // linear
   float tonePitchHz = 49.0f;
   float toneGlideSt = -12.0f;
   float toneDetune = 0.25f;
   float toneWidth = 0.6f;

   // motion
   float panStart = -0.6f;
   float panEnd = 0.6f;
   float spaceAmount = 0.22f;
   float spaceSize = 0.45f;
   float spaceDamping = 0.5f;
   float spaceWidth = 0.8f;

   // sub
   float subGain = 0.1995f; // linear
   float subPitchHz = 49.0f;
   float subDropSt = -12.0f;
   float subDecaySec = 0.7f;
   float subDrive = 0.2f;
   float subClick = 0.3f;

   // hit
   float hitGain = 0.0f; // linear
   float hitToneHz = 900.0f;
   float hitDecaySec = 0.22f;
   float hitNoise = 0.6f;
   float hitBody = 0.5f;
   float hitTime = 0.33f;

   // flutter
   float flutterDepth = 0.0f;
   float flutterStartHz = 6.0f;
   float flutterEndHz = 14.0f;
   int flutterShape = kFlutterSine;
   int flutterTarget = kFlutterTargetLevel;
   float flutterSmooth = 0.35f;

   // eq
   float highpassHz = 25.0f;
   float lowpassHz = 18000.0f;
   float eqLowFreqHz = 90.0f;
   float eqLowGainDb = 0.0f;
   float eqMidFreqHz = 900.0f;
   float eqMidGainDb = 0.0f;
   float eqHighFreqHz = 5000.0f;
   float eqHighGainDb = 0.0f;

   // envelope
   float attackSec = 0.001f;
   float decaySec = 2.0f;
   float sustain = 1.0f;
   float releaseSec = 0.3f;
   float velToLevel = 0.5f;
   float velToTone = 0.3f;

   int maxVoices = 8;
   int seed = 0;
};

// White noise with one filter on it, which is what the colour names have
// always meant. Kept as a struct rather than a class because a voice owns two
// of them and they are walked every sample.
struct NoiseSource {
   RngLite rng;
   // Paul Kellet's three-pole pink filter, and the running integrator that
   // makes brown. Both are cheap enough to leave running whatever colour is
   // selected, which is what stops a colour change from clicking.
   float p0 = 0.0f, p1 = 0.0f, p2 = 0.0f;
   float brown = 0.0f;
   float lastWhite = 0.0f, lastPink = 0.0f;
   Svf green;

   void prepare(uint32_t seed, float sampleRate) {
      rng.seed(seed);
      p0 = p1 = p2 = brown = lastWhite = lastPink = 0.0f;
      green.reset();
      // Green noise is a broad mid emphasis, not a narrow one: Q below 1 so it
      // is still noise rather than a pitch.
      green.setCutoff(500.0f, 0.55f, sampleRate);
   }

   inline float tick(int kind);
};

// One gesture, from the note that started it to the end of its span.
struct Voice {
   bool active = false;
   // Set by noteOn and cleared by the first process() block that reaches this
   // voice. Everything a trigger draws for itself is drawn there rather than in
   // noteOn, so that it sees the parameter values the host set in the same
   // block -- including Random Seed, which otherwise would not take effect
   // until the note after the one that changed it.
   bool pendingStart = false;
   int16_t port = 0, channel = 0, key = 60;
   int32_t noteId = -1;
   float velocity = 1.0f;

   Adsr env;

   // Position in the gesture, 0..1 across the (possibly varied) span, and how
   // far it advances per sample.
   double pos = 0.0;
   double inc = 0.0;

   // What this particular trigger drew for itself. Every one of these is the
   // parameter on screen multiplied or offset by a gaussian scaled by
   // Variation, so at Variation 0 they are all neutral and the plugin repeats
   // exactly.
   float vPeak = 0.0f;       // added to Peak
   float vCutoff = 1.0f;     // multiplies the Air cutoff
   float vSweep = 0.0f;      // added to Air Sweep, octaves
   float vPitch = 1.0f;      // multiplies the Tone and Sub pitch
   float vLevel = 1.0f;      // multiplies the whole voice
   float vPanOff = 0.0f;     // added to both pan ends
   float vFlutter = 1.0f;    // multiplies both flutter speeds
   float vHitTone = 1.0f;    // multiplies the Hit tone
   float vDecay = 1.0f;      // multiplies the Sub and Hit decays

   // air
   NoiseSource noiseL, noiseR;
   Svf airL, airR;
   Tilt tiltL, tiltR;

   // tone
   float tonePhase[kToneOscs]{};
   float toneSpread[kToneOscs]{}; // -1..1 detune position of each oscillator
   float tonePanL[kToneOscs]{}, tonePanR[kToneOscs]{};

   // sub -- struck at the peak, then on its own decay
   bool subFired = false;
   float subPhase = 0.0f;
   float subAmp = 0.0f;
   // The pitch drop and the impact click, both as one-pole states rather than
   // as a clock: an exp() per sample for a single sine oscillator is not worth
   // paying sixteen times over.
   float subDrop = 0.0f;
   float subClickEnv = 0.0f;

   // hit -- struck at Hit Time, then on its own decay
   bool hitFired = false;
   float hitAmp = 0.0f;
   Svf hitMode[kHitModes];
   Svf hitBand;
   Svf hitBodyFilter;
   RngLite hitRng;

   // flutter
   float flutterPhase = 0.0f;
   float flutterHold = 0.0f; // the Random shape's current step
   OnePoleLp flutterLp;

   // The played note, as a frequency ratio against G1 -- the measured median
   // fundamental of the library's pitched families, and therefore the note at
   // which the plugin plays a preset as it was fitted.
   float keyRatio = 1.0f;

   // Either the release has run out, or the gesture has played past its span
   // with both struck layers decayed away -- in which case the voice produces
   // silence however long the note is still held down.
   inline bool finished() const {
      return env.isIdle() || (pos >= 1.0 && subAmp < 1.0e-5f && hitAmp < 1.0e-5f);
   }
};

class GestureEngine {
public:
   void prepare(double sampleRate, uint32_t maxBlockSize);
   void reset();

   void setParams(const EngineParams &p);

   void noteOn(int16_t port, int16_t channel, int16_t key, int32_t noteId, double velocity);
   void noteOff(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void choke(int16_t port, int16_t channel, int16_t key, int32_t noteId);
   void allSoundOff();

   void process(float *outL, float *outR, uint32_t numSamples);

   bool isSilent() const;
   // What the window's activity meter counts: gestures sounding.
   uint32_t activeVoiceCount() const;
   // Monotonic count of gestures triggered, which is what the header ornament
   // draws one streak for.
   uint32_t gestureCounter() const { return mGestureCounter; }
   float tailSeconds() const;

   // A gesture is a whole sound rather than a note in a chord, so the pool is
   // small on purpose: sixteen simultaneous cinematic impacts is already more
   // than any arrangement asks for, and each one carries two noise sources, a
   // three-oscillator stack and a resonator cluster.
   static constexpr uint32_t kMaxVoices = 16;

private:
   void updateFilters();
   Voice *allocateVoice();
   void beginVoice(Voice &v);
   void drawVariation(Voice &v);
   void processVoice(Voice &v, float *outL, float *outR, uint32_t numSamples);
   void processOutputChain(float *outL, float *outR, uint32_t numSamples);

   // The gesture's amplitude at position t, 0..1. Peak/hold/rise/fall, with
   // the voice's own varied peak.
   inline float gestureLevel(float t, float peak) const;
   // The sweep warp: where in the gesture the movement has got to. t^curve,
   // which is what makes a transition's centroid sit still and then fall.
   inline float sweepWarp(float t) const;
   inline float flutterValue(Voice &v, float rate, float dtPerSample);
   inline float flutterRate(const Voice &v, float t) const;

   EngineParams mP;
   double mSampleRate = 48000.0;

   Voice mVoices[kMaxVoices];

   Rng mRng;
   int mAppliedSeed = 0;
   uint32_t mGestureCounter = 0;

   Space mSpace;

   // The output chain, in order: the two ends, the three EQ bands, then the
   // measured family profile. The profile comes last because it is a statement
   // about the finished sound rather than about any one layer.
   Hp2 mHpL, mHpR;
   Lp2 mLpL, mLpR;
   Biquad mEqLowL, mEqLowR;
   Biquad mEqMidL, mEqMidR;
   Biquad mEqHighL, mEqHighR;
   // The measured family profile, as the two shelves it reduces to: how much
   // darker the family is above the middle, and how far its bottom two octaves
   // stand over that. A high shelf rather than a true slope on purpose -- a
   // real -10 dB/octave would take 60 dB off the top of a boom and leave
   // nothing to mix, where the measurement is a statement about balance.
   Biquad mProfileTiltL, mProfileTiltR;
   Biquad mProfileShelfL, mProfileShelfR;

   float mSilenceCounter = 0.0f;
};

} // namespace whooshpact
