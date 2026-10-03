#include "params.h"

#include "verdalis/param_macros.h"

#include <cstring>

namespace verdaliscene {

namespace {

const char *const kGateNames[] = {"Always", "Transport", "Notes"};
const char *const kFilterNames[] = {"Lowpass", "Bandpass", "Highpass", "Notch"};
const char *const kStereoNames[] = {"Stereo", "Mono"};

// Log defaults are the raw 0..1 position: log(def / lo) / log(hi / lo).
const ParamDesc kScene[kNumSceneParams] = {
   ENUM(kParamGate, "gate", "Gate", "Envelope", 0.0, kGateNames,
        "What opens the scene. Always plays from the moment the plugin runs, Transport "
        "follows the host's play and stop, Notes plays while any note is held. Notes only "
        "open and close the scene: no layer ever plays a single event on a note."),
   LOG(kParamAttack, "attack", "Attack", "Envelope", 0.7094, 1.0, 30000.0, "ms",
       "How long the whole scene takes to fade in when the gate opens."),
   LOG(kParamDecay, "decay", "Decay", "Envelope", 0.6701, 1.0, 30000.0, "ms",
       "Fall from full level to the sustain level after the fade-in."),
   PCT(kParamSustain, "sustain", "Sustain", "Envelope", 1.0,
       "Level the scene holds while the gate is open."),
   LOG(kParamRelease, "release", "Release", "Envelope", 0.7766, 1.0, 30000.0, "ms",
       "How long the scene takes to fade out when the gate closes."),

   ENUM(kParamFilterType, "filter_type", "Filter Type", "Filter", 0.0, kFilterNames,
        "Response of the filter over the whole scene."),
   LOG(kParamHighpass, "highpass", "Highpass", "Filter", 0.0, 20.0, 2000.0, "Hz",
       "Rolls off the bottom of the whole scene at 12 dB/oct. Off at the far left."),
   LOG(kParamFilterCutoff, "filter_cutoff", "Filter Cutoff", "Filter", 1.0, 20.0, 20000.0, "Hz",
       "Corner of the scene filter. A lowpass here is the scene heard through a wall."),
   PCT(kParamFilterReso, "filter_reso", "Filter Resonance", "Filter", 0.1,
       "Emphasis at the cutoff frequency."),

   {kParamWidth, "width", "Width", "Output", 0.0, 2.0, 1.0, ParamKind::Percent, 0, 0, "%",
    nullptr, 0,
    "Stereo width of the whole scene: mono at the left, as recorded at 100 %, wider "
    "beyond."},
   LIN(kParamGain, "gain", "Output Gain", "Output", -60.0, 12.0, 0.0, "dB",
       "Final level of the whole scene, after the envelope and the filter."),
};

// A layer's place in the scene. The keys are the ones a scene preset uses in
// a layer's section.
const ParamDesc kSlot[kNumSlotParams] = {
   STEP(kSlotActive, "layer_active", "Active", "Layer", 0.0, 1.0, 0.0, "",
        "Whether the layer exists. Added and removed in the window, not automated."),
   LIN(kSlotLevel, "layer_level", "Level", "Layer", -60.0, 12.0, 0.0, "dB",
       "The layer's fader in the scene mixer, on top of the layer's own output gain."),
   BIPCT(kSlotPan, "layer_pan", "Pan", "Layer", 0.0,
         "Where the layer sits. A stereo layer is balanced, a mono one is placed."),
   ENUM(kSlotStereo, "layer_stereo", "Stereo", "Layer", 0.0, kStereoNames,
        "Stereo keeps the layer's own image. Mono folds it to one point that Pan then "
        "places, for a source that should come from one direction."),
   LIN(kSlotShotRate, "layer_shot_rate", "Shot Rate", "Layer", 0.0, 30.0, 2.0, "/min",
       "Bird and night layers only. How often, at random, the phrase a played note "
       "would fire sings on its own, at the layer's Shot Level. Nothing fires while "
       "Shot Level is off, and zero stops it."),
};

#undef LIN
#undef PCT
#undef BIPCT
#undef LOG
#undef STEP
#undef ENUM

// The manual's table: the scene's, then the placement ones without Active,
// which a preset expresses by having the section at all.
struct DocTable {
   ParamDesc entries[kNumParams];
   DocTable() {
      uint32_t n = 0;
      for (uint32_t i = 0; i < kNumSceneParams; ++i)
         entries[n++] = kScene[i];
      for (uint32_t i = kSlotLevel; i < kNumSlotParams; ++i)
         entries[n++] = kSlot[i];
   }
};

} // namespace

const ParamDesc *sceneParamTable() { return kScene; }
const ParamDesc *slotParamTable() { return kSlot; }

const ParamDesc *paramTable() {
   static const DocTable t;
   return t.entries;
}

const ParamDesc *sceneParamByKey(const char *key) {
   return paramByKeyIn(kScene, kNumSceneParams, key);
}

const ParamDesc *slotParamByKey(const char *key) {
   return paramByKeyIn(kSlot, kNumSlotParams, key);
}

} // namespace verdaliscene
