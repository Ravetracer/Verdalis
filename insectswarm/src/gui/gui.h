#pragma once

// The window is the suite's; see shared/include/verdalis/gui/gui.h for the
// interface and window.h for what a plugin describes about itself. This only
// brings those names into the plugin's namespace and declares the entry point.

#include "verdalis/gui/gui.h"

namespace insectswarm {

using verdalis::Gui;
using verdalis::GuiDelegate;
using verdalis::GuiPreset;

// Returns nullptr if no X display could be opened.
Gui *createGui(GuiDelegate &delegate);

} // namespace insectswarm
