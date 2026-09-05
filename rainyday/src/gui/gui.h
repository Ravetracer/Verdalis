#pragma once

// The plugin's own window. Deliberately toolkit-free: raw X11 for the window
// and Cairo for the drawing, so RainyDay stays a single .clap file with no
// runtime dependencies a Linux audio machine does not already have.
//
// The GUI never touches the plugin's atomics directly. It reads and writes
// parameters through GuiDelegate, which turns writes into proper CLAP events
// so host automation recording sees them.

#include <cstdint>
#include <string>
#include <vector>

namespace rainyday {

// One entry in the preset browser. Exactly one of loadKey / path is set:
// built-in presets live in the binary, user presets are files on disk.
struct GuiPreset {
   std::string name;
   std::string description;
   std::string loadKey;
   std::string path;
   bool userContent = false;
};

// Everything the window needs from the plugin.
class GuiDelegate {
public:
   virtual ~GuiDelegate() = default;

   virtual double guiParamValue(uint32_t id) const = 0;
   virtual void guiBeginEdit(uint32_t id) = 0;
   virtual void guiSetParam(uint32_t id, double value) = 0;
   virtual void guiEndEdit(uint32_t id) = 0;

   // Decaying output peak per channel, 0..1 linear, published by the audio
   // thread.
   virtual void guiOutputPeaks(float &left, float &right) const = 0;

   // Droplets currently sounding, published by the audio thread.
   virtual uint32_t guiDropletCount() const = 0;
   virtual uint32_t guiDropletLimit() const = 0;

   virtual const std::vector<GuiPreset> &guiPresets() const = 0;
   virtual int guiCurrentPreset() const = 0;
   virtual bool guiPresetEdited() const = 0;
   virtual void guiLoadPreset(int index) = 0;

   // The name to offer when the save field opens.
   virtual std::string guiSuggestedPresetName() const = 0;
   // Writes the current parameter values into the user preset directory and
   // rescans it. Returns false and fills `error` if that did not work.
   virtual bool guiSavePreset(const std::string &name, std::string &error) = 0;
};

class Gui {
public:
   virtual ~Gui() = default;

   // Creates the window, unmapped and unparented.
   virtual bool open() = 0;
   // A native window handle. Not `unsigned long`: that is 32 bits on 64-bit
   // Windows, which would quietly truncate an HWND.
   virtual bool embed(uintptr_t parentWindow) = 0;
   virtual bool setTransientFor(uintptr_t parentWindow) = 0;
   virtual void setTitle(const char *title) = 0;
   virtual void setScale(double scale) = 0;
   virtual void size(uint32_t *width, uint32_t *height) const = 0;
   // The layout's own size at scale 1, which is the aspect ratio a host has to
   // keep when it resizes the window.
   virtual void designSize(uint32_t *width, uint32_t *height) const = 0;
   // Snaps a requested size to one the window can take: the largest scale that
   // fits inside it, within the scale range, at the design aspect ratio.
   virtual void fitSize(uint32_t *width, uint32_t *height) const = 0;
   // Resizes to the requested size by choosing the scale from it. The window
   // ends up at fitSize() of the request.
   virtual bool resize(uint32_t width, uint32_t height) = 0;
   virtual void show() = 0;
   virtual void hide() = 0;

   // Drains pending X11 events and repaints. Called from a host timer.
   virtual void tick() = 0;
};

// Returns nullptr if no X display could be opened.
Gui *createGui(GuiDelegate &delegate);

} // namespace rainyday
