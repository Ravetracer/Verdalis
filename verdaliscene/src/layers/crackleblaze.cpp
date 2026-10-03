// The Fire layer: CrackleBlaze's engine, parameters, presets and panels, from
// ../crackleblaze/. Compiled against that plugin's own include directory, so
// every name below that is not VerdaliScene's is CrackleBlaze's. All nine
// adapters are written from one template; see layers.h for what each field
// means.

#include "../layers.h"

#include "dsp/fire_engine.h"
#include "engine_params.h"
#include "params.h"
#include "presets_generated.h"
#include "crackleblaze.h"

#ifdef VERDALISCENE_WITH_GUI
#include "gui/gui.h"
#include "verdalis/gui/window.h"
#endif

namespace verdaliscene {

// A layer keeps the plugin's ParamId as the low part of its own id, below
// the placement parameters at 200 (see params.h).
static_assert(crackleblaze::kNumParams <= 200, "crackleblaze has outgrown its slot in the id layout");

namespace {

class Layer final : public LayerEngine {
public:
   void prepare(double sampleRate, uint32_t maxFrames) override {
      mEngine.prepare(sampleRate, maxFrames);
   }
   void reset() override { mEngine.reset(); }
   void setParams(const double *real) override { mEngine.setParams(crackleblaze::engineParams(real)); }
   void noteOn(double velocity) override { mEngine.noteOn(0, 0, 60, -1, velocity); }
   void noteOff() override { mEngine.noteOff(0, 0, 60, -1); }
   void allSoundOff() override { mEngine.allSoundOff(); }
   void process(float *outL, float *outR, uint32_t frames) override {
      mEngine.process(outL, outR, frames);
   }
   bool isSilent() const override { return mEngine.isSilent(); }
   uint32_t voiceCount() const override { return mEngine.activeEventCount(); }
   uint32_t eventCount() const override { return mEngine.crackleCounter(); }

private:
   crackleblaze::FireEngine mEngine;
};

LayerEngine *create() { return new Layer(); }

verdalis::PresetLibrarySpec library() {
   return {crackleblaze::presetContext(), crackleblaze::kBuiltinPresets, crackleblaze::kNumBuiltinPresets};
}

} // namespace

const LayerType &crackleblazeLayer() {
   static const LayerType t = {
      kLayerFire,
      "CrackleBlaze",
      "Fire",
      {1.000, 0.353, 0.173},
      &crackleblaze::paramTable,
      crackleblaze::kNumParams,
      &library,
      "Camp Fire",
      0.0,
      nullptr,
      0,
      nullptr,
      0,
      kNoLayerParam,
      kNoLayerParam,
      crackleblaze::kParamMaxEvents,
      0,
      &create,
#ifdef VERDALISCENE_WITH_GUI
      &crackleblaze::windowSpec,
      &crackleblaze::createOrnament,
#else
      nullptr,
      nullptr,
#endif
   };
   return t;
}

} // namespace verdaliscene
