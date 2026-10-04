// The Wind layer: SkyHowl's engine, parameters, presets and panels, from
// ../skyhowl/. Compiled against that plugin's own include directory, so every
// name below that is not VerdaliScene's is SkyHowl's. All nine adapters are
// written from one template; see layers.h for what each field means.

#include "../layers.h"

#include "dsp/wind_engine.h"
#include "engine_params.h"
#include "params.h"
#include "presets_generated.h"
#include "skyhowl.h"

#ifdef VERDALISCENE_WITH_GUI
#include "gui/gui.h"
#include "verdalis/gui/window.h"
#endif

namespace verdaliscene {

// A layer keeps the plugin's ParamId as the low part of its own id, below
// the placement parameters at 200 (see params.h).
static_assert(skyhowl::kNumParams <= 200, "skyhowl has outgrown its slot in the id layout");

namespace {

class Layer final : public LayerEngine {
public:
   void prepare(double sampleRate, uint32_t maxFrames) override {
      mEngine.prepare(sampleRate, maxFrames);
   }
   void reset() override { mEngine.reset(); }
   void setParams(const double *real, const EnvelopeCurves &curves) override {
      skyhowl::EngineParams p = skyhowl::engineParams(real);
      p.attackCurve = curves.attack;
      p.decayCurve = curves.decay;
      p.releaseCurve = curves.release;
      mEngine.setParams(p);
   }
   void noteOn(double velocity) override { mEngine.noteOn(0, 0, 60, -1, velocity); }
   void noteOff() override { mEngine.noteOff(0, 0, 60, -1); }
   void allSoundOff() override { mEngine.allSoundOff(); }
   void process(float *outL, float *outR, uint32_t frames) override {
      mEngine.process(outL, outR, frames);
   }
   bool isSilent() const override { return mEngine.isSilent(); }
   uint32_t voiceCount() const override { return mEngine.activeGustCount(); }
   uint32_t eventCount() const override { return mEngine.gustCounter(); }

private:
   skyhowl::WindEngine mEngine;
};

LayerEngine *create() { return new Layer(); }

verdalis::PresetLibrarySpec library() {
   return {skyhowl::presetContext(), skyhowl::kBuiltinPresets, skyhowl::kNumBuiltinPresets};
}

} // namespace

const LayerType &skyhowlLayer() {
   static const LayerType t = {
      kLayerWind,
      "SkyHowl",
      "Wind",
      {0.941, 0.518, 0.361},
      &skyhowl::paramTable,
      skyhowl::kNumParams,
      &library,
      "Meadow Breeze",
      0.0,
      nullptr,
      0,
      nullptr,
      0,
      kNoLayerParam,
      kNoLayerParam,
      skyhowl::kParamMaxGusts,
      0,
      &create,
#ifdef VERDALISCENE_WITH_GUI
      &skyhowl::windowSpec,
      &skyhowl::createOrnament,
#else
      nullptr,
      nullptr,
#endif
   };
   return t;
}

} // namespace verdaliscene
