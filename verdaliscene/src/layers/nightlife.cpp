// The Night layer: NightLife's engine, parameters, presets and panels, from
// ../nightlife/. Compiled against that plugin's own include directory, so every
// name below that is not VerdaliScene's is NightLife's. All nine adapters are
// written from one template; see layers.h for what each field means.

#include "../layers.h"

#include "dsp/night_engine.h"
#include "engine_params.h"
#include "params.h"
#include "presets_generated.h"
#include "nightlife.h"

#ifdef VERDALISCENE_WITH_GUI
#include "gui/gui.h"
#include "verdalis/gui/window.h"
#endif

namespace verdaliscene {

// A layer keeps the plugin's ParamId as the low part of its own id, below
// the placement parameters at 200 (see params.h).
static_assert(nightlife::kNumParams <= 200, "nightlife has outgrown its slot in the id layout");

namespace {

class Layer final : public LayerEngine {
public:
   void prepare(double sampleRate, uint32_t maxFrames) override {
      mEngine.prepare(sampleRate, maxFrames);
   }
   void reset() override { mEngine.reset(); }
   void setParams(const double *real) override { mEngine.setParams(nightlife::engineParams(real)); }
   void noteOn(double velocity) override { mEngine.noteOn(0, 0, 60, -1, velocity); }
   void noteOff() override { mEngine.noteOff(0, 0, 60, -1); }
   void allSoundOff() override { mEngine.allSoundOff(); }
   void fireShot() override { mEngine.triggerShot(); }
   void process(float *outL, float *outR, uint32_t frames) override {
      mEngine.process(outL, outR, frames);
   }
   bool isSilent() const override { return mEngine.isSilent(); }
   uint32_t voiceCount() const override { return mEngine.activeCallCount(); }
   uint32_t eventCount() const override { return mEngine.callCounter(); }

private:
   nightlife::NightEngine mEngine;
};

LayerEngine *create() { return new Layer(); }

verdalis::PresetLibrarySpec library() {
   return {nightlife::presetContext(), nightlife::kBuiltinPresets, nightlife::kNumBuiltinPresets};
}

} // namespace

const LayerType &nightlifeLayer() {
   static const LayerType t = {
      kLayerNight,
      "NightLife",
      "Night",
      {0.624, 0.706, 0.910},
      &nightlife::paramTable,
      nightlife::kNumParams,
      &library,
      "Summer Night",
      -5.0,
      nullptr,
      0,
      nullptr,
      0,
      nightlife::kParamShotLevel,
      kNoLayerParam,
      nightlife::kParamMaxVoices,
      0,
      &create,
#ifdef VERDALISCENE_WITH_GUI
      &nightlife::windowSpec,
      &nightlife::createOrnament,
#else
      nullptr,
      nullptr,
#endif
   };
   return t;
}

} // namespace verdaliscene
