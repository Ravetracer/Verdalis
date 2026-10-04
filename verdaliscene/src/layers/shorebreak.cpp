// The Waves layer: ShoreBreak's engine, parameters, presets and panels, from
// ../shorebreak/. Compiled against that plugin's own include directory, so
// every name below that is not VerdaliScene's is ShoreBreak's. All nine
// adapters are written from one template; see layers.h for what each field
// means.

#include "../layers.h"

#include "dsp/wave_engine.h"
#include "engine_params.h"
#include "params.h"
#include "presets_generated.h"
#include "shorebreak.h"

#ifdef VERDALISCENE_WITH_GUI
#include "gui/gui.h"
#include "verdalis/gui/window.h"
#endif

namespace verdaliscene {

// A layer keeps the plugin's ParamId as the low part of its own id, below
// the placement parameters at 200 (see params.h).
static_assert(shorebreak::kNumParams <= 200, "shorebreak has outgrown its slot in the id layout");

namespace {

class Layer final : public LayerEngine {
public:
   void prepare(double sampleRate, uint32_t maxFrames) override {
      mEngine.prepare(sampleRate, maxFrames);
   }
   void reset() override { mEngine.reset(); }
   void setParams(const double *real, const EnvelopeCurves &curves) override {
      shorebreak::EngineParams p = shorebreak::engineParams(real);
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
   uint32_t voiceCount() const override { return mEngine.activeWaveCount(); }
   uint32_t eventCount() const override { return 0; }

private:
   shorebreak::WaveEngine mEngine;
};

LayerEngine *create() { return new Layer(); }

verdalis::PresetLibrarySpec library() {
   return {shorebreak::presetContext(), shorebreak::kBuiltinPresets, shorebreak::kNumBuiltinPresets};
}

} // namespace

const LayerType &shorebreakLayer() {
   static const LayerType t = {
      kLayerWaves,
      "ShoreBreak",
      "Waves",
      {0.310, 0.816, 0.729},
      &shorebreak::paramTable,
      shorebreak::kNumParams,
      &library,
      "Rolling Tide",
      -2.0,
      nullptr,
      0,
      nullptr,
      0,
      kNoLayerParam,
      kNoLayerParam,
      shorebreak::kParamMaxWaves,
      0,
      &create,
#ifdef VERDALISCENE_WITH_GUI
      &shorebreak::windowSpec,
      &shorebreak::createOrnament,
#else
      nullptr,
      nullptr,
#endif
   };
   return t;
}

} // namespace verdaliscene
