// The Thunder layer: ThunderClap's engine, parameters, presets and panels, from
// ../thunderclap/. Compiled against that plugin's own include directory, so
// every name below that is not VerdaliScene's is ThunderClap's. All nine
// adapters are written from one template; see layers.h for what each field
// means.

#include "../layers.h"

#include "dsp/thunder_engine.h"
#include "engine_params.h"
#include "params.h"
#include "presets_generated.h"
#include "thunderclap.h"

#ifdef VERDALISCENE_WITH_GUI
#include "gui/gui.h"
#include "verdalis/gui/window.h"
#endif

namespace verdaliscene {

// A layer keeps the plugin's ParamId as the low part of its own id, below
// the placement parameters at 200 (see params.h).
static_assert(thunderclap::kNumParams <= 200, "thunderclap has outgrown its slot in the id layout");

namespace {

class Layer final : public LayerEngine {
public:
   void prepare(double sampleRate, uint32_t maxFrames) override {
      mEngine.prepare(sampleRate, maxFrames);
   }
   void reset() override { mEngine.reset(); }
   void setParams(const double *real, const EnvelopeCurves &curves) override {
      thunderclap::EngineParams p = thunderclap::engineParams(real);
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
   uint32_t voiceCount() const override { return mEngine.activeShockCount(); }
   uint32_t eventCount() const override { return mEngine.flashCounter(); }

private:
   thunderclap::ThunderEngine mEngine;
};

LayerEngine *create() { return new Layer(); }

verdalis::PresetLibrarySpec library() {
   return {thunderclap::presetContext(), thunderclap::kBuiltinPresets, thunderclap::kNumBuiltinPresets};
}

const SceneDefault kDefaults[] = {
   // Three flashes a minute. Storm Rate is logarithmic over 1..60 /min, so
   // this is log(3) / log(60). The plugin's own Storm presets run at 4 to 15,
   // which is a storm overhead; a scene is more often one passing by.
   {thunderclap::kParamStormRate, 0.26833},
};

const PinnedParam kPinned[] = {
   // Storm keeps flashing for as long as the note is held, and the scene holds
   // one for as long as the layer exists. One Shot and Gated would give one
   // flash and then silence.
   {thunderclap::kParamMode, 2.0},
};

} // namespace

const LayerType &thunderclapLayer() {
   static const LayerType t = {
      kLayerThunder,
      "ThunderClap",
      "Thunder",
      {0.702, 0.588, 0.980},
      &thunderclap::paramTable,
      thunderclap::kNumParams,
      &library,
      "Summer Storm",
      -8.0,
      kDefaults,
      static_cast<int>(sizeof(kDefaults) / sizeof(kDefaults[0])),
      kPinned,
      static_cast<int>(sizeof(kPinned) / sizeof(kPinned[0])),
      kNoLayerParam,
      thunderclap::kParamStormRate,
      kNoLayerParam,
      thunderclap::ThunderEngine::kMaxShocks,
      &create,
#ifdef VERDALISCENE_WITH_GUI
      &thunderclap::windowSpec,
      &thunderclap::createOrnament,
#else
      nullptr,
      nullptr,
#endif
   };
   return t;
}

} // namespace verdaliscene
