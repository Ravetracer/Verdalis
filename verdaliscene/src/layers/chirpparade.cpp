// The Birds layer: ChirpParade's engine, parameters, presets and panels, from
// ../chirpparade/. Compiled against that plugin's own include directory, so
// every name below that is not VerdaliScene's is ChirpParade's. All nine
// adapters are written from one template; see layers.h for what each field
// means.

#include "../layers.h"

#include "dsp/chirp_engine.h"
#include "engine_params.h"
#include "params.h"
#include "presets_generated.h"
#include "chirpparade.h"

#ifdef VERDALISCENE_WITH_GUI
#include "gui/gui.h"
#include "verdalis/gui/window.h"
#endif

namespace verdaliscene {

// A layer keeps the plugin's ParamId as the low part of its own id, below
// the placement parameters at 200 (see params.h).
static_assert(chirpparade::kNumParams <= 200, "chirpparade has outgrown its slot in the id layout");

namespace {

class Layer final : public LayerEngine {
public:
   void prepare(double sampleRate, uint32_t maxFrames) override {
      mEngine.prepare(sampleRate, maxFrames);
   }
   void reset() override { mEngine.reset(); }
   void setParams(const double *real, const EnvelopeCurves &curves) override {
      chirpparade::EngineParams p = chirpparade::engineParams(real);
      p.attackCurve = curves.attack;
      p.decayCurve = curves.decay;
      p.releaseCurve = curves.release;
      mEngine.setParams(p);
   }
   void noteOn(double velocity) override { mEngine.noteOn(0, 0, 60, -1, velocity); }
   void noteOff() override { mEngine.noteOff(0, 0, 60, -1); }
   void allSoundOff() override { mEngine.allSoundOff(); }
   void fireShot() override { mEngine.triggerShot(); }
   void process(float *outL, float *outR, uint32_t frames) override {
      mEngine.process(outL, outR, frames);
   }
   bool isSilent() const override { return mEngine.isSilent(); }
   uint32_t voiceCount() const override { return mEngine.activeChirpCount(); }
   uint32_t eventCount() const override { return mEngine.syllableCounter(); }

private:
   chirpparade::ChirpEngine mEngine;
};

LayerEngine *create() { return new Layer(); }

verdalis::PresetLibrarySpec library() {
   return {chirpparade::presetContext(), chirpparade::kBuiltinPresets, chirpparade::kNumBuiltinPresets};
}

} // namespace

const LayerType &chirpparadeLayer() {
   static const LayerType t = {
      kLayerBirds,
      "ChirpParade",
      "Birds",
      {0.949, 0.780, 0.267},
      &chirpparade::paramTable,
      chirpparade::kNumParams,
      &library,
      "Forest Morning",
      0.0,
      nullptr,
      0,
      nullptr,
      0,
      chirpparade::kParamShotLevel,
      kNoLayerParam,
      chirpparade::kParamMaxVoices,
      0,
      &create,
#ifdef VERDALISCENE_WITH_GUI
      &chirpparade::windowSpec,
      &chirpparade::createOrnament,
#else
      nullptr,
      nullptr,
#endif
   };
   return t;
}

} // namespace verdaliscene
