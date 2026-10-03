// The Rain layer: RainyDay's engine, parameters, presets and panels, from
// ../rainyday/. Compiled against that plugin's own include directory, so every
// name below that is not VerdaliScene's is RainyDay's. All nine adapters are
// written from one template; see layers.h for what each field means.

#include "../layers.h"

#include "dsp/rain_engine.h"
#include "engine_params.h"
#include "params.h"
#include "presets_generated.h"
#include "rainyday.h"

#ifdef VERDALISCENE_WITH_GUI
#include "gui/gui.h"
#include "verdalis/gui/window.h"
#endif

namespace verdaliscene {

// A layer keeps the plugin's ParamId as the low part of its own id, below
// the placement parameters at 200 (see params.h).
static_assert(rainyday::kNumParams <= 200, "rainyday has outgrown its slot in the id layout");

namespace {

class Layer final : public LayerEngine {
public:
   void prepare(double sampleRate, uint32_t maxFrames) override {
      mEngine.prepare(sampleRate, maxFrames);
   }
   void reset() override { mEngine.reset(); }
   void setParams(const double *real) override { mEngine.setParams(rainyday::engineParams(real)); }
   void noteOn(double velocity) override { mEngine.noteOn(0, 0, 60, -1, velocity); }
   void noteOff() override { mEngine.noteOff(0, 0, 60, -1); }
   void allSoundOff() override { mEngine.allSoundOff(); }
   void process(float *outL, float *outR, uint32_t frames) override {
      mEngine.process(outL, outR, frames);
   }
   bool isSilent() const override { return mEngine.isSilent(); }
   uint32_t voiceCount() const override { return mEngine.activeDropletCount(); }
   uint32_t eventCount() const override { return 0; }

private:
   rainyday::RainEngine mEngine;
};

LayerEngine *create() { return new Layer(); }

verdalis::PresetLibrarySpec library() {
   return {rainyday::presetContext(), rainyday::kBuiltinPresets, rainyday::kNumBuiltinPresets};
}

} // namespace

const LayerType &rainydayLayer() {
   static const LayerType t = {
      kLayerRain,
      "RainyDay",
      "Rain",
      {0.345, 0.714, 0.910},
      &rainyday::paramTable,
      rainyday::kNumParams,
      &library,
      "Steady Rain",
      -5.0,
      nullptr,
      0,
      nullptr,
      0,
      kNoLayerParam,
      kNoLayerParam,
      rainyday::kParamMaxDroplets,
      0,
      &create,
#ifdef VERDALISCENE_WITH_GUI
      &rainyday::windowSpec,
      &rainyday::createOrnament,
#else
      nullptr,
      nullptr,
#endif
   };
   return t;
}

} // namespace verdaliscene
