// The River layer: RiverFlow's engine, parameters, presets and panels, from
// ../riverflow/. Compiled against that plugin's own include directory, so every
// name below that is not VerdaliScene's is RiverFlow's. All nine adapters are
// written from one template; see layers.h for what each field means.

#include "../layers.h"

#include "dsp/river_engine.h"
#include "engine_params.h"
#include "params.h"
#include "presets_generated.h"
#include "riverflow.h"

#ifdef VERDALISCENE_WITH_GUI
#include "gui/gui.h"
#include "verdalis/gui/window.h"
#endif

namespace verdaliscene {

// A layer keeps the plugin's ParamId as the low part of its own id, below
// the placement parameters at 200 (see params.h).
static_assert(riverflow::kNumParams <= 200, "riverflow has outgrown its slot in the id layout");

namespace {

class Layer final : public LayerEngine {
public:
   void prepare(double sampleRate, uint32_t maxFrames) override {
      mEngine.prepare(sampleRate, maxFrames);
   }
   void reset() override { mEngine.reset(); }
   void setParams(const double *real) override { mEngine.setParams(riverflow::engineParams(real)); }
   void noteOn(double velocity) override { mEngine.noteOn(0, 0, 60, -1, velocity); }
   void noteOff() override { mEngine.noteOff(0, 0, 60, -1); }
   void allSoundOff() override { mEngine.allSoundOff(); }
   void process(float *outL, float *outR, uint32_t frames) override {
      mEngine.process(outL, outR, frames);
   }
   bool isSilent() const override { return mEngine.isSilent(); }
   uint32_t voiceCount() const override { return mEngine.activePocketCount(); }
   uint32_t eventCount() const override { return mEngine.dabbleCounter(); }

private:
   riverflow::RiverEngine mEngine;
};

LayerEngine *create() { return new Layer(); }

verdalis::PresetLibrarySpec library() {
   return {riverflow::presetContext(), riverflow::kBuiltinPresets, riverflow::kNumBuiltinPresets};
}

} // namespace

const LayerType &riverflowLayer() {
   static const LayerType t = {
      kLayerRiver,
      "RiverFlow",
      "River",
      {0.341, 0.780, 0.478},
      &riverflow::paramTable,
      riverflow::kNumParams,
      &library,
      "Forest Stream",
      -6.0,
      nullptr,
      0,
      nullptr,
      0,
      kNoLayerParam,
      kNoLayerParam,
      riverflow::kParamMaxEvents,
      0,
      &create,
#ifdef VERDALISCENE_WITH_GUI
      &riverflow::windowSpec,
      &riverflow::createOrnament,
#else
      nullptr,
      nullptr,
#endif
   };
   return t;
}

} // namespace verdaliscene
