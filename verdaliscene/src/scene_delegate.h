#pragma once

// What the scene window needs from the plugin. The suite's GuiDelegate is
// written for a plugin with one preset library and one set of meters; a scene
// has a library of its own and one per layer type, a set of meters per layer,
// and layers that come and go. The window adapts this to a GuiDelegate for
// the shared widgets (see gui/gui.cpp); the plugin implements only this.
//
// Every call is made from the main thread.

#include <cstdint>
#include <string>
#include <vector>

#include "verdalis/gui/gui.h"

namespace verdaliscene {

using verdalis::GuiPreset;

// The preset calls take a target: the scene's own library, or a layer slot,
// whose library is its plugin's.
constexpr int kSceneTarget = -1;

class SceneDelegate {
public:
   virtual ~SceneDelegate() = default;

   // ---------------------------------------------------------- parameters
   // By id into fullTable().
   virtual double paramValue(uint32_t id) const = 0;
   virtual void beginEdit(uint32_t id) = 0;
   virtual void setParam(uint32_t id, double value) = 0;
   virtual void endEdit(uint32_t id) = 0;

   // -------------------------------------------------------------- layers
   virtual bool layerActive(int slot) const = 0;
   // Adds a layer of `type` in the first free slot of that kind, at its start
   // preset. Returns the slot, or -1 when all of that kind are in use.
   virtual int addLayer(int type) = 0;
   virtual void removeLayer(int slot) = 0;

   // Mute and solo are not parameters and are never saved: a preset that
   // remembered a mute would be a layer that comes back silent.
   virtual bool layerMuted(int slot) const = 0;
   virtual bool layerSoloed(int slot) const = 0;
   virtual void setLayerMuted(int slot, bool on) = 0;
   virtual void setLayerSoloed(int slot, bool on) = 0;

   // -------------------------------------------------------------- meters
   // Whether the scene's gate is open: a key held, the transport running, or
   // Always.
   virtual bool sceneGateOpen() const = 0;
   virtual void outputPeaks(float &left, float &right) const = 0;
   virtual void layerPeaks(int slot, float &left, float &right) const = 0;
   virtual uint32_t layerVoices(int slot) const = 0;
   virtual uint32_t layerVoiceLimit(int slot) const = 0;
   virtual uint32_t layerEvents(int slot) const = 0;

   // ------------------------------------------------------------- presets
   virtual const std::vector<GuiPreset> &presets(int target) = 0;
   virtual int currentPreset(int target) = 0;
   virtual bool presetEdited(int target) const = 0;
   virtual void loadPreset(int target, int index) = 0;
   virtual std::string suggestedPresetName(int target) = 0;
   virtual bool savePreset(int target, const std::string &folder, const std::string &name,
                           const std::string &description, std::string &error) = 0;
   virtual std::vector<std::string> presetPacks(int target) = 0;
   virtual std::string packPathFor(int target, const std::string &folder) = 0;
   virtual bool exportPack(int target, const std::string &folder, const std::string &path,
                           std::string &error) = 0;
   virtual bool importPack(int target, const std::string &path, std::string &folder,
                           std::string &error) = 0;

   // Collections and editing, on the library `target` addresses. See GuiDelegate.
   virtual std::vector<std::string> presetCollections(int target) = 0;
   virtual bool createCollection(int target, const std::string &name, std::string &folder,
                                 std::string &error) = 0;
   virtual bool renameCollection(int target, const std::string &folder, const std::string &name,
                                 std::string &renamed, std::string &error) = 0;
   virtual bool deleteCollection(int target, const std::string &folder, std::string &error) = 0;
   virtual bool movePreset(int target, int index, const std::string &folder,
                           std::string &error) = 0;
   virtual bool editPreset(int target, int index, const std::string &name,
                           const std::string &description, std::string &error) = 0;
   virtual bool deletePreset(int target, int index, std::string &error) = 0;
};

} // namespace verdaliscene
