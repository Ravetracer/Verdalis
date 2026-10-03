#pragma once

// The instruments VerdaliScene layers, and what it needs to know about each.
//
// A layer is not a copy of another plugin: it is that plugin's own engine, its
// own parameter table, its own preset library and, in the window, its own
// panels. Each one is reached through an adapter in src/layers/, compiled
// against the plugin's sources in its own folder, so a change to RainyDay is a
// change to every rain layer with nothing here to keep in step.
//
// This header is deliberately free of anything plugin specific. The adapters
// include it by relative path and each plugin's headers through their own
// include directory, which is why VerdaliScene's own params.h never meets a
// plugin's params.h in one translation unit.

#include <cstdint>

#include "verdalis/params.h"
#include "verdalis/preset.h"
#include "verdalis/preset_library.h"

namespace verdalis {
struct WindowSpec;
class HeaderOrnament;
} // namespace verdalis

namespace verdaliscene {

// The nine nature instruments of the suite, in the suite's own order.
// WhooshPact is not one of them on purpose: it makes production sounds, not
// a place.
enum LayerTypeId : int {
   kLayerRain = 0,
   kLayerThunder,
   kLayerWaves,
   kLayerWind,
   kLayerBirds,
   kLayerRiver,
   kLayerFire,
   kLayerInsects,
   kLayerNight,
   kNumLayerTypes
};

// How many layers of one kind a scene can hold. Part of the parameter id
// layout (see params.h), so it can never change without moving every id.
constexpr int kInstancesPerType = 4;
constexpr int kNumSlots = kNumLayerTypes * kInstancesPerType;

constexpr uint32_t kNoLayerParam = 0xFFFFFFFFu;

// One running instance of a layer's engine, driven the way the plugin itself
// drives it: parameters as real-world values, and one note held for as long
// as the layer exists. Created and prepared on the main thread; everything
// else is called from the audio thread.
class LayerEngine {
public:
   virtual ~LayerEngine() = default;

   virtual void prepare(double sampleRate, uint32_t maxFrames) = 0;
   virtual void reset() = 0;
   // `real` is paramToReal() of every one of the plugin's parameters, indexed
   // by the plugin's own ParamId.
   virtual void setParams(const double *real) = 0;

   // The drone note: middle C, so every note-tracking control sits at its
   // neutral point, at the velocity plugin.cpp explains.
   virtual void noteOn(double velocity) = 0;
   virtual void noteOff() = 0;
   virtual void allSoundOff() = 0;

   // Fires the phrase a played note would have fired, on the voice the drone
   // note holds. Only the bird and night layers have one.
   virtual void fireShot() {}

   // Writes into both buffers, which the caller clears first -- exactly as the
   // plugin does.
   virtual void process(float *outL, float *outR, uint32_t frames) = 0;
   virtual bool isSilent() const = 0;

   // What the plugin's own activity meter counts, and its event count.
   virtual uint32_t voiceCount() const = 0;
   virtual uint32_t eventCount() const = 0;
};

// A parameter the scene holds at one value whatever the layer's settings say,
// because the value it is held away from only makes sense for a played note.
// It is not offered to the host or drawn on the layer's page.
struct PinnedParam {
   uint32_t id; // the plugin's own ParamId
   double raw;  // host-facing value
};

// A setting a freshly added layer takes instead of its start preset's.
struct SceneDefault {
   uint32_t id;
   double raw;
};

struct LayerType {
   LayerTypeId type;
   const char *pluginName; // "RainyDay": the preset context, the user folder
   const char *label;      // "Rain": tabs, mixer strips, host parameter names
   double accent[3];       // the plugin's own accent, as its window draws it

   const verdalis::ParamDesc *(*paramTable)();
   uint32_t paramCount;

   // The plugin's preset context and its embedded factory set, so that its
   // presets load into a layer and a layer saves back into its library.
   verdalis::PresetLibrarySpec (*library)();

   // Where a new layer of this kind starts: one of the plugin's factory
   // presets by name, then the scene defaults on top of it.
   const char *startPreset;
   // The fader a new layer of this kind starts at: its start preset brought to
   // about -31 dBFS RMS, so a layer added to a scene arrives in proportion to
   // the others rather than at whatever level its plugin plays alone. Thunder
   // is placed by its flashes' peaks instead, which is what a storm is heard by.
   double startLevelDb;
   const SceneDefault *sceneDefaults;
   int sceneDefaultCount;

   const PinnedParam *pinned;
   int pinnedCount;

   // The bird and night layers: the plugin's Shot Level, which the scene's
   // Shot Rate fires at random. kNoLayerParam elsewhere.
   uint32_t shotLevelParam;

   // Thunder: the rate whose average interval its first event is placed in,
   // so that a storm does not open with a flash at the moment the layer is
   // added. In events a minute, real value. kNoLayerParam elsewhere.
   uint32_t firstEventRateParam;

   // The activity meter's ceiling: a parameter of the plugin's, or a fixed
   // pool when the plugin has none.
   uint32_t voiceLimitParam;
   uint32_t voiceLimitFixed;

   LayerEngine *(*create)();

   // The plugin's own window, without its ornament, and a fresh ornament.
   // Both null in a build with no window.
   verdalis::WindowSpec (*windowSpec)();
   verdalis::HeaderOrnament *(*createOrnament)();
};

const LayerType &layerType(int type);

// The pinned entry for one of the plugin's own ParamIds, or null.
const PinnedParam *pinnedParam(const LayerType &t, uint32_t id);

} // namespace verdaliscene
